#include <apu/audio.h>
#include <cpu/guest_thread.h>
#include <kernel/heap.h>
#include <os/logger.h>
#include <user/config.h>

// ============================================================================================
// Audio output, v2 ("clocked producer / trivial consumer").
//
// Invariants distilled from every failure mode seen so far (see project notes):
//
//  1. The guest render callback is the engine's audio CLOCK. It must be invoked at an average
//     of exactly XAUDIO_SAMPLES_HZ/XAUDIO_NUM_SAMPLES (187.5/s) of wall time, never bursting
//     above real time based on queue state ("catch-up" broke level loading twice), and never
//     stopping (a stopped clock deadlocks cutscenes/loads that wait on audio progress).
//  2. The guest must never run on the platform's audio thread, and the clock must never
//     depend on the platform stream's liveness: AAudio/AudioTrack streams were observed to
//     stall permanently (MMAP on SD 8 Gen 3; legacy AudioTrack write futex on the dev phone),
//     which with a device-clocked design froze the whole game.
//  3. Production capped at "one frame per wakeup" loses slots whenever the OS wakes the
//     thread late; the resulting deficit is permanent, so outputs that consume in large
//     bursts (Bluetooth routes, 1024-frame HALs) sit at an empty buffer and crackle - the
//     original tester bug. Production must be per ELAPSED SLOT, not per wakeup.
//
// Design: a producer thread owns a virtual slot clock (CLOCK_MONOTONIC based). On every
// wakeup it invokes the guest once per elapsed slot, submissions land in a small FIFO. The
// SDL device callback (large chunks, no low-latency modes) only memcpy's from the FIFO and
// plays silence when it runs dry - it never blocks and never touches guest state. The clock
// starts CUSHION_FRAMES in the past so a jitter-absorbing cushion builds during the very
// first wakeups (boot, before any streaming) and is never rebuilt mid-game above real time.
// If the FIFO is full the producer skips guest calls (stock backpressure semantics) - unless
// the device looks dead (no consumption for DEVICE_DEAD_NANOS), in which case frames are
// rendered and dropped so the engine clock survives a dead platform stream.
// ============================================================================================
#define APU_PULL_MODEL 1

static PPCFunc* g_clientCallback{};
static uint32_t g_clientCallbackParam{}; // pointer in guest memory
static SDL_AudioDeviceID g_audioDevice{};
static bool g_downMixToStereo;

#if APU_PULL_MODEL

// Device-side chunk. On Android small buffers select aggressive low-latency paths (MMAP)
// that our workload cannot feed reliably and does not need; 4 guest frames per chunk keeps
// Bluetooth and 1024-burst HALs comfortable.
#ifdef __ANDROID__
#define APU_DEVICE_SAMPLES (XAUDIO_NUM_SAMPLES * 4)
#else
#define APU_DEVICE_SAMPLES XAUDIO_NUM_SAMPLES
#endif

static constexpr size_t CUSHION_FRAMES = 12;  // ~64 ms of jitter absorption
static constexpr size_t FIFO_CAPACITY_FRAMES = CUSHION_FRAMES + 10; // cushion + stock MAX_LATENCY
static constexpr int64_t DEVICE_DEAD_NANOS = 1'000'000'000; // 1 s without consumption = stalled stream

// FIFO between the producer thread and the SDL audio callback. Byte-granular ring so odd
// device chunk sizes work. Guarded by SDL_LockAudioDevice (the callback runs with the device
// lock held, so the producer locks it around every ring access).
static std::vector<uint8_t> g_fifo;
static size_t g_fifoHead; // read position
static size_t g_fifoUsed; // bytes stored
static std::atomic<int64_t> g_lastConsumeTime{}; // updated by the callback; watchdog input
static bool g_lastCallSubmitted; // did the last guest invocation submit a frame?

static int64_t MonotonicNanos()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static size_t FrameBytes()
{
    size_t channels = g_downMixToStereo ? 2 : XAUDIO_NUM_CHANNELS;
    return channels * XAUDIO_NUM_SAMPLES * sizeof(float);
}

static void FifoWrite(const uint8_t* data, size_t size)
{
    // Called with the device lock held. Caller guarantees capacity.
    size_t tail = (g_fifoHead + g_fifoUsed) % g_fifo.size();
    size_t first = std::min(size, g_fifo.size() - tail);
    memcpy(g_fifo.data() + tail, data, first);
    memcpy(g_fifo.data(), data + first, size - first);
    g_fifoUsed += size;
}

static void AudioDeviceCallback(void* userdata, Uint8* stream, int len)
{
    g_lastConsumeTime.store(MonotonicNanos(), std::memory_order_relaxed);

    if (g_fifo.empty())
    {
        memset(stream, 0, len);
        return;
    }

    size_t toCopy = std::min(size_t(len), g_fifoUsed);
    size_t first = std::min(toCopy, g_fifo.size() - g_fifoHead);
    memcpy(stream, g_fifo.data() + g_fifoHead, first);
    memcpy(stream + first, g_fifo.data(), toCopy - first);
    g_fifoHead = (g_fifoHead + toCopy) % g_fifo.size();
    g_fifoUsed -= toCopy;

    if (toCopy < size_t(len))
        memset(stream + toCopy, 0, len - toCopy); // ran dry: this slice plays silence
}

static std::unique_ptr<std::thread> g_audioThread;
static volatile bool g_audioThreadShouldExit;

static void AudioThread()
{
    using namespace std::chrono_literals;

    GuestThreadContext ctx(0);

    constexpr auto INTERVAL = std::chrono::nanoseconds(1000000000ns * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ);

    // Start the slot clock in the past so the first wakeups legitimately owe CUSHION_FRAMES
    // extra slots - the cushion builds at boot without any mid-game catch-up mechanism.
    int64_t startTime = MonotonicNanos() - CUSHION_FRAMES * INTERVAL.count();
    uint64_t producedSlots = 0;
    int64_t lastDriftCorrection = 0;

    g_lastConsumeTime.store(MonotonicNanos(), std::memory_order_relaxed);

    while (!g_audioThreadShouldExit)
    {
        const uint64_t elapsedSlots = uint64_t(MonotonicNanos() - startTime) / INTERVAL.count();

        while (producedSlots < elapsedSlots && !g_audioThreadShouldExit)
        {
            const size_t frameBytes = FrameBytes();

            SDL_LockAudioDevice(g_audioDevice);
            bool fifoFull = (g_fifo.size() - g_fifoUsed) < frameBytes;
            SDL_UnlockAudioDevice(g_audioDevice);

            if (fifoFull)
            {
                int64_t sinceConsume = MonotonicNanos() - g_lastConsumeTime.load(std::memory_order_relaxed);
                if (sinceConsume < DEVICE_DEAD_NANOS)
                {
                    // Healthy backpressure: the device is behind; pause the guest clock for
                    // this slot exactly like the stock MAX_LATENCY check did.
                    producedSlots = elapsedSlots;
                    break;
                }
                // Stream looks dead (see invariant 2): keep the engine clock alive, render
                // into the void. The FIFO stays full so audio resumes the moment the device
                // comes back and starts consuming again.
            }

            g_lastCallSubmitted = false;
            ctx.ppcContext.r3.u32 = g_clientCallbackParam;
            g_clientCallback(ctx.ppcContext, g_memory.base);
            producedSlots++;
        }

        // Crystal drift correction: if the device's clock runs slightly faster than ours, the
        // cushion drains by ~1 frame every few minutes and would eventually hit the crackle
        // regime again. When the FIFO is fully dry AND the engine is actively producing audio
        // (never true during level loads - their guest calls submit nothing, so this cannot
        // re-create the catch-up bug), shift the virtual clock back one slot to owe one extra
        // frame. Rate-limited hard to once per second.
        {
            int64_t nowNs = MonotonicNanos();
            SDL_LockAudioDevice(g_audioDevice);
            bool fifoDry = (g_fifoUsed == 0);
            SDL_UnlockAudioDevice(g_audioDevice);

            if (fifoDry && g_lastCallSubmitted && nowNs - lastDriftCorrection > 1'000'000'000)
            {
                startTime -= INTERVAL.count();
                lastDriftCorrection = nowNs;
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto next = now + (INTERVAL - now.time_since_epoch() % INTERVAL);

        std::this_thread::sleep_for(std::chrono::floor<std::chrono::milliseconds>(next - now));

        while (std::chrono::steady_clock::now() < next && !g_audioThreadShouldExit)
            std::this_thread::yield();
    }
}

#endif

static void CreateAudioDevice()
{
    if (g_audioDevice != NULL)
        SDL_CloseAudioDevice(g_audioDevice);

    bool surround = Config::ChannelConfiguration == EChannelConfiguration::Surround;
    int allowedChanges = surround ? SDL_AUDIO_ALLOW_CHANNELS_CHANGE : 0;

    SDL_AudioSpec desired{}, obtained{};
    desired.freq = XAUDIO_SAMPLES_HZ;
    desired.format = AUDIO_F32SYS;
    desired.channels = surround ? XAUDIO_NUM_CHANNELS : 2;
#if APU_PULL_MODEL
    desired.samples = APU_DEVICE_SAMPLES;
    desired.callback = AudioDeviceCallback;
#else
    desired.samples = XAUDIO_NUM_SAMPLES;
#endif
    g_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, allowedChanges);

    if (obtained.channels != 2 && obtained.channels != XAUDIO_NUM_CHANNELS) // This check may fail only when surround sound is enabled.
    {
        SDL_CloseAudioDevice(g_audioDevice);
        g_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    }

    if (!g_audioDevice)
        LOGFN_ERROR("Failed to open audio device: {}", SDL_GetError());
    else
        LOGFN("Audio device opened: freq {}, channels {}, samples {} (requested {})",
            obtained.freq, (int)obtained.channels, (int)obtained.samples, (int)desired.samples);

    g_downMixToStereo = (obtained.channels == 2);

#if APU_PULL_MODEL
    {
        // (Re)size the ring for the obtained channel count before any callback runs.
        SDL_LockAudioDevice(g_audioDevice);
        g_fifo.assign(FIFO_CAPACITY_FRAMES * FrameBytes(), 0);
        g_fifoHead = 0;
        g_fifoUsed = 0;
        SDL_UnlockAudioDevice(g_audioDevice);

        // If the device was reopened after the client had already registered, resume it.
        if (g_clientCallback != nullptr)
            SDL_PauseAudioDevice(g_audioDevice, 0);
    }
#endif
}

void XAudioInitializeSystem()
{
#ifdef _WIN32
    // Force wasapi on Windows.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_NAME, "Unleashed Recompiled");

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        LOGFN_ERROR("Failed to init audio subsystem: {}", SDL_GetError());
        return;
    }

    CreateAudioDevice();
}

#if !APU_PULL_MODEL

static std::unique_ptr<std::thread> g_audioThread;
static volatile bool g_audioThreadShouldExit;

static void AudioThread()
{
    using namespace std::chrono_literals;

    GuestThreadContext ctx(0);

    size_t channels = g_downMixToStereo ? 2 : XAUDIO_NUM_CHANNELS;

    while (!g_audioThreadShouldExit)
    {
        uint32_t queuedAudioSize = SDL_GetQueuedAudioSize(g_audioDevice);
        constexpr size_t MAX_LATENCY = 10;
        const size_t callbackAudioSize = channels * XAUDIO_NUM_SAMPLES * sizeof(float);

        if ((queuedAudioSize / callbackAudioSize) <= MAX_LATENCY)
        {
            ctx.ppcContext.r3.u32 = g_clientCallbackParam;
            g_clientCallback(ctx.ppcContext, g_memory.base);
        }

        auto now = std::chrono::steady_clock::now();
        constexpr auto INTERVAL = 1000000000ns * XAUDIO_NUM_SAMPLES / XAUDIO_SAMPLES_HZ;
        auto next = now + (INTERVAL - now.time_since_epoch() % INTERVAL);

        std::this_thread::sleep_for(std::chrono::floor<std::chrono::milliseconds>(next - now));

        while (std::chrono::steady_clock::now() < next)
            std::this_thread::yield();
    }
}

#endif

static void CreateAudioThread()
{
    SDL_PauseAudioDevice(g_audioDevice, 0);
    g_audioThreadShouldExit = false;
    g_audioThread = std::make_unique<std::thread>(AudioThread);
}

void XAudioRegisterClient(PPCFunc* callback, uint32_t param)
{
    auto* pClientParam = static_cast<uint32_t*>(g_userHeap.Alloc(sizeof(param)));
    ByteSwapInplace(param);
    *pClientParam = param;
    g_clientCallbackParam = g_memory.MapVirtual(pClientParam);
    g_clientCallback = callback;

    CreateAudioThread();
}

void XAudioSubmitFrame(void* samples)
{
    auto floatSamples = reinterpret_cast<be<float>*>(samples);

    if (g_downMixToStereo)
    {
        // 0: left 1.0f, right 0.0f
        // 1: left 0.0f, right 1.0f
        // 2: left 0.75f, right 0.75f
        // 3: left 0.0f, right 0.0f
        // 4: left 1.0f, right 0.0f
        // 5: left 0.0f, right 1.0f

        std::array<float, 2 * XAUDIO_NUM_SAMPLES> audioFrames;

        for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; i++)
        {
            float ch0 = floatSamples[0 * XAUDIO_NUM_SAMPLES + i];
            float ch1 = floatSamples[1 * XAUDIO_NUM_SAMPLES + i];
            float ch2 = floatSamples[2 * XAUDIO_NUM_SAMPLES + i];
            float ch3 = floatSamples[3 * XAUDIO_NUM_SAMPLES + i];
            float ch4 = floatSamples[4 * XAUDIO_NUM_SAMPLES + i];
            float ch5 = floatSamples[5 * XAUDIO_NUM_SAMPLES + i];

            audioFrames[i * 2 + 0] = (ch0 + ch2 * 0.75f + ch4) * Config::MasterVolume;
            audioFrames[i * 2 + 1] = (ch1 + ch2 * 0.75f + ch5) * Config::MasterVolume;
        }

#if APU_PULL_MODEL
        g_lastCallSubmitted = true;
        SDL_LockAudioDevice(g_audioDevice);
        if (g_fifo.size() - g_fifoUsed >= sizeof(audioFrames))
            FifoWrite(reinterpret_cast<const uint8_t*>(audioFrames.data()), sizeof(audioFrames));
        // else: device stalled and the ring is full - drop the frame, the clock matters more.
        SDL_UnlockAudioDevice(g_audioDevice);
#else
        SDL_QueueAudio(g_audioDevice, &audioFrames, sizeof(audioFrames));
#endif
    }
    else
    {
        std::array<float, XAUDIO_NUM_CHANNELS * XAUDIO_NUM_SAMPLES> audioFrames;

        for (size_t i = 0; i < XAUDIO_NUM_SAMPLES; i++)
        {
            for (size_t j = 0; j < XAUDIO_NUM_CHANNELS; j++)
                audioFrames[i * XAUDIO_NUM_CHANNELS + j] = floatSamples[j * XAUDIO_NUM_SAMPLES + i] * Config::MasterVolume;
        }

#if APU_PULL_MODEL
        g_lastCallSubmitted = true;
        SDL_LockAudioDevice(g_audioDevice);
        if (g_fifo.size() - g_fifoUsed >= sizeof(audioFrames))
            FifoWrite(reinterpret_cast<const uint8_t*>(audioFrames.data()), sizeof(audioFrames));
        SDL_UnlockAudioDevice(g_audioDevice);
#else
        SDL_QueueAudio(g_audioDevice, &audioFrames, sizeof(audioFrames));
#endif
    }
}
