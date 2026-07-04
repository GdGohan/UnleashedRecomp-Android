#include "vulkan_driver_android.h"

#include <os/android/storage_android.h>
#include <os/logger.h>

#include <adrenotools/driver.h>

#include <SDL.h>
#include <SDL_system.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <vector>

// The driver that ships inside the APK assets: source-built Mesa 26.1.4 Turnip with the
// per-draw WAIT_FOR_ME fix compiled in (no TU_DEBUG gate) and Adreno 732 device ids added.
// Covers a725/a732/a750 (a750 additionally requires MSAA off, which is the Android config
// default). Extracted to internal storage on first launch. TU_DEBUG must stay "none": on
// source builds "flushall" enables Mesa's REAL full per-draw flush (huge FPS hit).
static constexpr const char *BUNDLED_DRIVER_NAME = "vulkan.unleashed26_1_wfm_a732.so";
static constexpr const char *BUNDLED_DRIVER_ASSET = "turnip/vulkan.unleashed26_1_wfm_a732.so";
static constexpr const char *DEFAULT_DRIVER_NAME = "vulkan.unleashed26_1_wfm_a732.so";

// AArch64 "MOVZ w9, #imm16" encodings of the mask that tu6_emit_flushes() ORs into the
// per-draw flush bits when TU_DEBUG=flushall is set:
//   0x16FF = TU_CMD_FLAG_ALL_CLEAN | TU_CMD_FLAG_ALL_INVALIDATE (stock Mesa, unusably slow)
//   0x0200 = TU_CMD_FLAG_WAIT_FOR_ME only - the single bit that fixes the per-draw
//            "shimmer" corruption on Adreno 725 at a fraction of the cost.
// A 4-byte pattern can legitimately appear elsewhere (the patched encoding occurs ~8 times
// naturally in known builds as unrelated code), so imported drivers are only patched where
// the match is provably this instruction: inside an executable PT_LOAD segment of the ELF
// and 4-byte aligned in mapped memory. If no such match exists (different Mesa version or
// codegen), or the match count is implausible, the driver is installed unpatched and the
// fix simply won't apply.
static constexpr uint8_t MOVZ_W9_FLUSHALL_MASK[4] = { 0xE9, 0xDF, 0x82, 0x52 };
static constexpr uint8_t MOVZ_W9_WAIT_FOR_ME[4] = { 0x09, 0x40, 0x80, 0x52 };
static constexpr size_t MAX_EXPECTED_FLUSHALL_MATCHES = 8; // known builds have 3 (per-gen inlined copies)

static std::string GetTurnipDir()
{
    const std::filesystem::path &internalDir = os::android::GetInternalFilesDir();
    if (internalDir.empty())
        return {};

    return (internalDir / "turnip/").string();
}

static bool ReadWholeFile(const std::filesystem::path &path, std::vector<uint8_t> &data)
{
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(file);
        return false;
    }

    data.resize(size_t(size));
    bool ok = fread(data.data(), 1, data.size(), file) == data.size();
    fclose(file);
    return ok;
}

// Writes through a temporary file + rename so a mid-write kill can't leave a truncated
// driver behind that would then be loaded on every subsequent launch.
static bool WriteWholeFile(const std::filesystem::path &path, const void *data, size_t size)
{
    std::filesystem::path tempPath = path;
    tempPath += ".tmp";

    FILE *file = fopen(tempPath.c_str(), "wb");
    if (file == nullptr)
        return false;

    bool ok = fwrite(data, 1, size, file) == size;
    fclose(file);

    if (!ok)
    {
        remove(tempPath.c_str());
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, path, ec);
    if (ec)
        return false;

    // Match the permissions a manually pushed driver would have; files created by the
    // app default to 0600, which is unnecessarily tight for something the loader mmaps.
    chmod(path.c_str(), 0755);
    return true;
}

static void WriteTextFileIfMissing(const std::filesystem::path &path, const char *contents)
{
    std::error_code ec;
    if (std::filesystem::exists(path, ec))
        return;

    WriteWholeFile(path, contents, strlen(contents));
}

static void WriteTextFile(const std::filesystem::path &path, const char *contents)
{
    WriteWholeFile(path, contents, strlen(contents));
}

struct ElfExecSegment
{
    size_t fileOffset;
    size_t fileSize;
    uint64_t virtualAddress;
};

// Returns every executable PT_LOAD segment of a 64-bit little-endian ELF, or an empty
// vector when the file isn't one (which also makes the patcher below refuse to touch it).
static std::vector<ElfExecSegment> FindElfExecSegments(const std::vector<uint8_t> &elf)
{
    std::vector<ElfExecSegment> segments;
    if (elf.size() < 0x40 || memcmp(elf.data(), "\177ELF", 4) != 0 || elf[4] != 2 || elf[5] != 1)
        return segments;

    uint64_t programHeaderOffset;
    uint16_t programHeaderEntrySize, programHeaderCount;
    memcpy(&programHeaderOffset, elf.data() + 0x20, sizeof(programHeaderOffset));
    memcpy(&programHeaderEntrySize, elf.data() + 0x36, sizeof(programHeaderEntrySize));
    memcpy(&programHeaderCount, elf.data() + 0x38, sizeof(programHeaderCount));

    for (uint16_t i = 0; i < programHeaderCount; i++)
    {
        size_t base = size_t(programHeaderOffset) + size_t(i) * programHeaderEntrySize;
        if (base + 0x28 > elf.size())
            break;

        uint32_t type, flags;
        uint64_t fileOffset, virtualAddress, fileSize;
        memcpy(&type, elf.data() + base + 0x00, sizeof(type));
        memcpy(&flags, elf.data() + base + 0x04, sizeof(flags));
        memcpy(&fileOffset, elf.data() + base + 0x08, sizeof(fileOffset));
        memcpy(&virtualAddress, elf.data() + base + 0x10, sizeof(virtualAddress));
        memcpy(&fileSize, elf.data() + base + 0x20, sizeof(fileSize));

        if (type == 1 /* PT_LOAD */ && (flags & 1) /* PF_X */ && fileOffset + fileSize <= elf.size())
            segments.push_back({ size_t(fileOffset), size_t(fileSize), virtualAddress });
    }

    return segments;
}

// Replaces MOVZ w9,#0x16ff with MOVZ w9,#0x200 (see constants above) inside executable
// code only. Returns the number of instructions patched (3 on known builds).
static size_t PatchDriverFlushallMask(std::vector<uint8_t> &driver)
{
    std::vector<ElfExecSegment> segments = FindElfExecSegments(driver);
    if (segments.empty())
    {
        LOG_ERROR("Driver patch: no executable ELF segment found, leaving driver unpatched.");
        return 0;
    }

    std::vector<size_t> matches;
    for (size_t i = 0; i + sizeof(MOVZ_W9_FLUSHALL_MASK) <= driver.size(); i++)
    {
        if (memcmp(driver.data() + i, MOVZ_W9_FLUSHALL_MASK, sizeof(MOVZ_W9_FLUSHALL_MASK)) != 0)
            continue;

        for (const ElfExecSegment &segment : segments)
        {
            if (i >= segment.fileOffset &&
                i + sizeof(MOVZ_W9_FLUSHALL_MASK) <= segment.fileOffset + segment.fileSize &&
                (segment.virtualAddress + (i - segment.fileOffset)) % 4 == 0)
            {
                matches.push_back(i);
                break;
            }
        }
    }

    if (matches.size() > MAX_EXPECTED_FLUSHALL_MATCHES)
    {
        LOGF_ERROR("Driver patch: {} matches exceed the sanity cap of {}, leaving driver unpatched.",
            matches.size(), MAX_EXPECTED_FLUSHALL_MATCHES);
        return 0;
    }

    for (size_t offset : matches)
        memcpy(driver.data() + offset, MOVZ_W9_WAIT_FOR_ME, sizeof(MOVZ_W9_WAIT_FOR_ME));

    return matches.size();
}

// Reads a file bundled in the APK assets (SDL routes relative paths to the asset manager).
static bool ReadAssetFile(const char *assetPath, std::vector<uint8_t> &data)
{
    SDL_RWops *rw = SDL_RWFromFile(assetPath, "rb");
    if (rw == nullptr)
        return false;

    Sint64 size = SDL_RWsize(rw);
    if (size <= 0)
    {
        SDL_RWclose(rw);
        return false;
    }

    data.resize(size_t(size));
    bool ok = SDL_RWread(rw, data.data(), 1, data.size()) == size_t(size);
    SDL_RWclose(rw);
    return ok;
}

// Users can drop an arbitrary Turnip build (plain .so, extracted from the release zip)
// into <external>/driver_import/. It gets the WAIT_FOR_ME patch applied when possible,
// is installed to internal storage, selected via driver_name.txt, and the source file is
// moved to driver_import/installed/ so it isn't re-processed every launch.
static void ProcessDriverImportDir(const std::filesystem::path &turnipDir)
{
    const std::filesystem::path &externalDir = os::android::GetExternalFilesDir();
    if (externalDir.empty())
        return;

    std::error_code ec;
    std::filesystem::path importDir = externalDir / "driver_import";
    std::filesystem::create_directories(importDir, ec);

    // Rewritten every launch so testers always see the current instructions.
    WriteTextFile(importDir / "readme.txt",
        "Optional: drop a Mesa Turnip Vulkan driver here as a plain .so file\n"
        "(extract it from the driver zip first, e.g. libvulkan_freedreno.so).\n"
        "On the next launch it will be installed and selected. If it is an older\n"
        "stock build containing the known byte pattern, the Adreno 7xx\n"
        "anti-shimmer patch is applied to it first. Processed files move to the\n"
        "installed/ subfolder.\n"
        "\n"
        "You can also create a tu_debug.txt file in THIS folder to set Turnip's\n"
        "TU_DEBUG options without rebuilding the app; it overrides the internal\n"
        "default (none). The bundled driver has the anti-shimmer fix compiled\n"
        "in and needs no TU_DEBUG options. Diagnostic examples (one per run):\n"
        "  nolrz\n"
        "  sysmem\n"
        "  noubwc\n"
        "NOTE: \"flushall\" is only for legacy binary-patched drivers, where it\n"
        "activates their fix. On the bundled driver (or any stock/source-built\n"
        "one) it enables Mesa's real full per-draw flush - a massive FPS hit,\n"
        "diagnostic use only.\n"
        "Delete tu_debug.txt to return to the default (none).\n"
        "The app already ships with a working driver; this is for experiments.\n"
        "\n"
        "DIAGNOSTICS: the app writes a log to log.txt in the PARENT folder (one\n"
        "level up from this driver_import/ folder). If the game freezes, close it\n"
        "and send that log.txt - it records what each thread was doing when frames\n"
        "stopped. The previous run is kept as log_prev.txt.\n"
        "\n"
        "GFXReconstruct capture (for sending a GPU trace to driver developers):\n"
        "create an empty file named gfxrecon_capture.txt in THIS folder. On the\n"
        "next launch the game records a Vulkan trace to ../gfxr/unleashed_capture.gfxr\n"
        "(the PARENT folder's gfxr/ subfolder). Keep the session SHORT (reach the\n"
        "spot that shows the bug, then close the game) - the file grows the whole\n"
        "time and can get large. Send that .gfxr, then DELETE gfxrecon_capture.txt\n"
        "to return to normal (capturing slows the game down a lot).\n");

    for (const auto &entry : std::filesystem::directory_iterator(importDir, ec))
    {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".so")
            continue;

        std::string fileName = entry.path().filename().string();

        std::vector<uint8_t> driver;
        if (!ReadWholeFile(entry.path(), driver))
        {
            LOGF_ERROR("Driver import: failed to read {}", fileName);
            continue;
        }

        size_t patched = PatchDriverFlushallMask(driver);
        if (patched > 0)
            LOGF("Driver import: applied WAIT_FOR_ME patch to {} ({} instruction(s)).", fileName, patched);
        else
            LOGF("Driver import: flushall mask pattern not found in {}, installing unpatched.", fileName);

        if (!WriteWholeFile(turnipDir / fileName, driver.data(), driver.size()))
        {
            LOGF_ERROR("Driver import: failed to install {} to internal storage.", fileName);
            continue;
        }

        WriteTextFile(turnipDir / "driver_name.txt", fileName.c_str());

        // The fix only fires while TU_DEBUG=flushall gates the (patched) mask in. Don't
        // overwrite an existing tu_debug.txt - it may carry deliberate extra flags.
        if (patched > 0)
            WriteTextFileIfMissing(turnipDir / "tu_debug.txt", "flushall");

        std::filesystem::path installedDir = importDir / "installed";
        std::filesystem::create_directories(installedDir, ec);
        std::filesystem::rename(entry.path(), installedDir / fileName, ec);
        if (ec)
            std::filesystem::remove(entry.path(), ec);

        LOGF("Driver import: installed and selected {}.", fileName);
    }
}

// First-launch provisioning: extract the bundled pre-patched driver to internal storage
// and select it. Existing files are never overwritten, so a manually pushed driver or
// hand-edited driver_name.txt/tu_debug.txt setup keeps working unchanged.
static void InstallBundledDriverIfNeeded(const std::filesystem::path &turnipDir)
{
    std::error_code ec;
    std::filesystem::path driverPath = turnipDir / BUNDLED_DRIVER_NAME;

    // Re-extract not only when missing but also on a size mismatch, so a truncated or
    // stale copy (e.g. from an interrupted first launch or an APK update that ships a
    // newer driver) heals itself instead of failing to dlopen on every launch.
    std::vector<uint8_t> driver;
    if (!ReadAssetFile(BUNDLED_DRIVER_ASSET, driver))
    {
        // APK built without the bundled driver - nothing to provision.
        return;
    }

    if (!std::filesystem::exists(driverPath, ec) || std::filesystem::file_size(driverPath, ec) != driver.size())
    {
        if (!WriteWholeFile(driverPath, driver.data(), driver.size()))
        {
            LOG_ERROR("Failed to extract the bundled Vulkan driver to internal storage.");
            return;
        }

        LOGF("Extracted bundled Vulkan driver to {}.", driverPath.string());
    }

    WriteTextFileIfMissing(turnipDir / "driver_name.txt", BUNDLED_DRIVER_NAME);
    WriteTextFileIfMissing(turnipDir / "tu_debug.txt", "none");
}

static void EnsureVulkanDriverInstalled(const std::string &turnipDirString)
{
    std::filesystem::path turnipDir(turnipDirString);

    std::error_code ec;
    std::filesystem::create_directories(turnipDir, ec);

    ProcessDriverImportDir(turnipDir);
    InstallBundledDriverIfNeeded(turnipDir);
}

static std::string GetCustomDriverName(const std::string &turnipDir)
{
    std::string path = turnipDir + "driver_name.txt";
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
        return DEFAULT_DRIVER_NAME;

    char buffer[256]{};
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);

    while (bytesRead > 0 && (buffer[bytesRead - 1] == '\n' || buffer[bytesRead - 1] == '\r' || buffer[bytesRead - 1] == ' '))
        --bytesRead;

    buffer[bytesRead] = '\0';
    return (bytesRead > 0) ? std::string(buffer) : DEFAULT_DRIVER_NAME;
}

static bool ReadTrimmedTextFile(const std::filesystem::path &path, char *buffer, size_t bufferSize)
{
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;

    size_t bytesRead = fread(buffer, 1, bufferSize - 1, file);
    fclose(file);

    while (bytesRead > 0 && (buffer[bytesRead - 1] == '\n' || buffer[bytesRead - 1] == '\r' || buffer[bytesRead - 1] == ' '))
        --bytesRead;

    buffer[bytesRead] = '\0';
    return bytesRead > 0;
}

// Optional: if a "tu_debug.txt" file is present, its (trimmed) contents are used as the
// TU_DEBUG environment variable value, letting Turnip debug/workaround options (e.g.
// "noubwc", "nolrz") be tried without rebuilding the APK. The copy in the external
// driver_import/ folder (editable over MTP/file managers without root) takes priority over
// the internal one, so testers can flip options themselves. Note that "flushall" is the
// gate that activates the patched WAIT_FOR_ME mask (see the patcher constants above), so
// it should stay in the list when experimenting, e.g. "flushall,nolrz". See
// https://docs.mesa3d.org/drivers/freedreno.html for the full list of TU_DEBUG options.
static void ApplyTuDebugOverride(const std::string &turnipDir)
{
    char buffer[256]{};
    std::filesystem::path source;

    const std::filesystem::path &externalDir = os::android::GetExternalFilesDir();
    std::filesystem::path externalPath = externalDir / "driver_import" / "tu_debug.txt";
    std::filesystem::path internalPath = std::filesystem::path(turnipDir) / "tu_debug.txt";

    if (!externalDir.empty() && ReadTrimmedTextFile(externalPath, buffer, sizeof(buffer)))
        source = externalPath;
    else if (ReadTrimmedTextFile(internalPath, buffer, sizeof(buffer)))
        source = internalPath;
    else
        return;

    setenv("TU_DEBUG", buffer, 1);
    LOGF("Applied TU_DEBUG override from {}: \"{}\"", source.string(), buffer);
}

// Optional: if a "vk_layer_settings.txt" file is pushed alongside the driver, it's pointed to via
// VK_LAYER_SETTINGS_PATH so VK_LAYER_KHRONOS_validation picks up settings from it (e.g. enabling
// Synchronization Validation) without rebuilding the APK. See
// https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/main/docs/syncval_usage.md
static void ApplyLayerSettingsOverride(const std::string &turnipDir)
{
    std::string path = turnipDir + "vk_layer_settings.txt";

    struct stat buf {};
    if (stat(path.c_str(), &buf) != 0)
        return;

    setenv("VK_LAYER_SETTINGS_PATH", path.c_str(), 1);

    // Legacy/redundant path: older Khronos validation layer builds read validation features
    // directly from VK_LAYER_ENABLES instead of (or in addition to) the settings file mechanism.
    // Set both so this works regardless of which one this particular layer build honors.
    setenv("VK_LAYER_ENABLES", "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT", 1);

    LOGF("Applied VK_LAYER_SETTINGS_PATH override: \"{}\"", path);
}

// Optional GFXReconstruct capture. If the marker file "gfxrecon_capture.txt" is present in the
// external driver_import/ folder, arm the bundled capture layer (enabled + configured in
// plume_vulkan.cpp via VK_EXT_layer_settings) by setting env vars it reads at instance creation.
// The trace is written to <external>/gfxr/unleashed_capture.gfxr, which the tester copies off over
// MTP. Off unless the marker is present: capturing adds heavy overhead and writes a large file, so
// this is strictly a diagnostic (e.g. sending a Turnip repro to a Mesa maintainer).
static void ApplyGfxreconstructCapture()
{
    const std::filesystem::path &externalDir = os::android::GetExternalFilesDir();
    if (externalDir.empty())
        return;

    std::error_code ec;
    std::filesystem::path marker = externalDir / "driver_import" / "gfxrecon_capture.txt";
    if (!std::filesystem::exists(marker, ec))
        return;

    std::filesystem::path captureDir = externalDir / "gfxr";
    std::filesystem::create_directories(captureDir, ec);

    std::filesystem::path captureFile = captureDir / "unleashed_capture.gfxr";
    setenv("UNLEASHED_GFXRECON_CAPTURE", "1", 1);
    setenv("UNLEASHED_GFXRECON_CAPTURE_FILE", captureFile.string().c_str(), 1);

    LOGF("GFXReconstruct capture armed (marker present). Trace: {}", captureFile.string());
}

static std::string GetNativeLibraryDir()
{
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr)
        return {};

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getApplicationInfoMethod = env->GetMethodID(activityClass, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    jobject applicationInfo = env->CallObjectMethod(activity, getApplicationInfoMethod);
    jclass applicationInfoClass = env->GetObjectClass(applicationInfo);
    jfieldID nativeLibraryDirField = env->GetFieldID(applicationInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
    jstring nativeLibraryDirString = static_cast<jstring>(env->GetObjectField(applicationInfo, nativeLibraryDirField));

    const char *chars = env->GetStringUTFChars(nativeLibraryDirString, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(nativeLibraryDirString, chars);

    env->DeleteLocalRef(nativeLibraryDirString);
    env->DeleteLocalRef(applicationInfoClass);
    env->DeleteLocalRef(applicationInfo);
    env->DeleteLocalRef(activityClass);

    if (!result.empty() && result.back() != '/')
        result.push_back('/');

    return result;
}

void *AndroidGetCustomVulkanLoader()
{
    // Arm capture first: it is independent of which Vulkan driver we end up loading (it works even
    // on the stock driver path where this function returns nullptr below).
    ApplyGfxreconstructCapture();

    std::string turnipDir = GetTurnipDir();
    if (turnipDir.empty())
    {
        LOG_ERROR("Internal storage path unavailable, cannot set up the custom Vulkan driver.");
        return nullptr;
    }

    EnsureVulkanDriverInstalled(turnipDir);

    std::string driverName = GetCustomDriverName(turnipDir);

    struct stat buf {};
    std::string driverPath = turnipDir + driverName;
    if (stat(driverPath.c_str(), &buf) != 0)
    {
        // No bundled driver in the APK and nothing installed manually - fall back to the
        // stock system driver.
        return nullptr;
    }

    std::string nativeLibraryDir = GetNativeLibraryDir();
    if (nativeLibraryDir.empty())
    {
        LOG_ERROR("Failed to query nativeLibraryDir via JNI, cannot load custom Vulkan driver.");
        return nullptr;
    }

    ApplyTuDebugOverride(turnipDir);
    ApplyLayerSettingsOverride(turnipDir);

    void *libVulkan = adrenotools_open_libvulkan(
        RTLD_NOW | RTLD_LOCAL,
        ADRENOTOOLS_DRIVER_CUSTOM,
        nullptr, // tmpLibDir: unused on API >= 29 (memfd is used instead)
        nativeLibraryDir.c_str(),
        turnipDir.c_str(),
        driverName.c_str(),
        nullptr,
        nullptr);

    if (libVulkan == nullptr)
    {
        LOG_ERROR("adrenotools_open_libvulkan failed, falling back to the default Vulkan driver.");
        return nullptr;
    }

    void *getInstanceProcAddr = dlsym(libVulkan, "vkGetInstanceProcAddr");
    if (getInstanceProcAddr == nullptr)
    {
        LOG_ERROR("Custom Vulkan driver loaded but vkGetInstanceProcAddr symbol is missing.");
        return nullptr;
    }

    LOGF("Successfully loaded custom Vulkan driver ({}) via libadrenotools.", driverName);
    return getInstanceProcAddr;
}
