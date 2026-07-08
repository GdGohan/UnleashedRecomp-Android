#include <stdafx.h>
#include "touch_controls.h"
#include "imgui_utils.h"
#include "button_guide.h"
#include "game_window.h"
#include <gpu/video.h>
#include <sdl_listener.h>
#include <atomic>
#include <cmath>

// ---------------------------------------------------------------------------
// Layout constants.
//
// Positions are fractions of the viewport; X of the viewport width, Y of the
// viewport height. Sizes are fractions of the viewport height so the layout
// keeps its proportions across resolutions/aspect ratios.
// ---------------------------------------------------------------------------

// Left analog stick.
static constexpr float STICK_CX      = 0.135f; // of width
static constexpr float STICK_CY      = 0.760f; // of height
static constexpr float STICK_BASE_R  = 0.150f; // of height
static constexpr float STICK_THUMB_R = 0.070f; // of height
static constexpr float STICK_ZONE_R  = 0.210f; // of height (hit radius)

// Right face-button cluster (Xbox diamond: A bottom, B right, X left, Y top).
static constexpr float FACE_CX       = 0.865f; // of width
static constexpr float FACE_CY       = 0.760f; // of height
static constexpr float FACE_BTN_R    = 0.058f; // of height
static constexpr float FACE_SPREAD   = 0.108f; // of height (centre -> button)

// Shoulders (top corners) and triggers (just below them).
static constexpr float SHOULDER_HW   = 0.075f; // half-width, of height
static constexpr float SHOULDER_HH   = 0.036f; // half-height, of height
static constexpr float SHOULDER_LX   = 0.075f; // of width
static constexpr float SHOULDER_RX   = 0.925f; // of width
static constexpr float SHOULDER_Y    = 0.090f; // of height
static constexpr float TRIGGER_Y     = 0.185f; // of height

// Start / Back, centred along the top.
static constexpr float MENU_Y        = 0.070f; // of height
static constexpr float MENU_DX       = 0.055f; // of width (centre -> button)
static constexpr float MENU_HW       = 0.032f; // half-width, of height
static constexpr float MENU_HH       = 0.032f; // half-height, of height

namespace
{
    struct Finger
    {
        SDL_FingerID id;
        float nx; // normalised [0,1] over the window
        float ny;
    };

    std::mutex g_mutex;
    std::vector<Finger> g_fingers;

    std::atomic<bool> g_visible{ true };
    XAMINPUT_GAMEPAD g_state{};

    // The finger currently driving the analog stick (-1 = none). It captures the
    // stick on touch-down inside the zone and keeps it until released, so the
    // thumb keeps following even after the finger leaves the zone. Only touched
    // from Draw() (render thread), so it needs no synchronisation.
    SDL_FingerID g_stickFingerId = (SDL_FingerID)-1;

    bool AnyFingerInCircle(const std::vector<ImVec2>& pts, ImVec2 c, float r)
    {
        const float r2 = r * r;
        for (const auto& p : pts)
        {
            const float dx = p.x - c.x;
            const float dy = p.y - c.y;
            if (dx * dx + dy * dy <= r2)
                return true;
        }
        return false;
    }

    bool AnyFingerInRect(const std::vector<ImVec2>& pts, ImVec2 mn, ImVec2 mx)
    {
        for (const auto& p : pts)
        {
            if (p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y)
                return true;
        }
        return false;
    }

    // Draws a button glyph from the shared controller atlas, preserving aspect.
    void DrawGlyph(ImDrawList* dl, ImVec2 c, float halfW, float halfH, EButtonIcon icon, int alpha)
    {
        auto ic = GetButtonIcon(icon);
        auto* tex = std::get<1>(ic);
        if (!tex)
            return;

        dl->AddImage(tex, { c.x - halfW, c.y - halfH }, { c.x + halfW, c.y + halfH },
            GET_UV_COORDS(std::get<0>(ic)), IM_COL32(255, 255, 255, alpha));
    }

    // Round face button (A/B/X/Y): dark backing disc + coloured glyph on top.
    void DrawFaceButton(ImDrawList* dl, const std::vector<ImVec2>& pts, ImVec2 c, float r,
        EButtonIcon icon, uint16_t bit, XAMINPUT_GAMEPAD& st)
    {
        const bool pressed = AnyFingerInCircle(pts, c, r * 1.3f);
        if (pressed)
            st.wButtons |= bit;

        dl->AddCircleFilled(c, r * 1.25f, IM_COL32(0, 0, 0, pressed ? 120 : 70), 32);
        DrawGlyph(dl, c, r, r, icon, pressed ? 255 : 210);
    }

    // Wide button (shoulders/triggers/start/back): rounded backing + glyph.
    // Returns whether it is currently pressed.
    bool DrawWideButton(ImDrawList* dl, const std::vector<ImVec2>& pts, ImVec2 c,
        float halfW, float halfH, float glyphHalfW, float glyphHalfH, EButtonIcon icon)
    {
        const bool pressed = AnyFingerInRect(pts, { c.x - halfW, c.y - halfH }, { c.x + halfW, c.y + halfH });

        dl->AddRectFilled({ c.x - halfW, c.y - halfH }, { c.x + halfW, c.y + halfH },
            IM_COL32(0, 0, 0, pressed ? 120 : 70), halfH * 0.5f);
        DrawGlyph(dl, c, glyphHalfW, glyphHalfH, icon, pressed ? 255 : 210);

        return pressed;
    }
}

// ---------------------------------------------------------------------------
// SDL touch event handling.
// ---------------------------------------------------------------------------

class SDLEventListenerForTouchControls : public SDLEventListener
{
public:
    bool OnSDLEvent(SDL_Event* event) override
    {
        switch (event->type)
        {
            case SDL_FINGERDOWN:
            {
                {
                    std::lock_guard lock(g_mutex);
                    g_fingers.push_back({ event->tfinger.fingerId, event->tfinger.x, event->tfinger.y });
                }

                // Any touch brings the overlay back.
                TouchControls::SetVisible(true);
                break;
            }

            case SDL_FINGERMOTION:
            {
                std::lock_guard lock(g_mutex);
                for (auto& f : g_fingers)
                {
                    if (f.id == event->tfinger.fingerId)
                    {
                        f.nx = event->tfinger.x;
                        f.ny = event->tfinger.y;
                        break;
                    }
                }
                break;
            }

            case SDL_FINGERUP:
            {
                std::lock_guard lock(g_mutex);
                std::erase_if(g_fingers, [&](const Finger& f) { return f.id == event->tfinger.fingerId; });
                break;
            }
        }

        return false;
    }
}
g_sdlEventListenerForTouchControls;

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

bool TouchControls::IsVisible()
{
    return g_visible.load(std::memory_order_relaxed);
}

void TouchControls::SetVisible(bool visible)
{
    g_visible.store(visible, std::memory_order_relaxed);
}

const XAMINPUT_GAMEPAD& TouchControls::GetGamepadState()
{
    return g_state;
}

void TouchControls::Init()
{
    // Nothing to load: the glyph atlas is owned by ButtonGuide, which is
    // initialised before us. Kept for symmetry / future dedicated assets.
}

void TouchControls::Draw()
{
    if (!g_visible.load(std::memory_order_relaxed))
    {
        g_state = {};
        g_stickFingerId = (SDL_FingerID)-1;
        return;
    }

    const float vw = float(Video::s_viewportWidth);
    const float vh = float(Video::s_viewportHeight);
    if (vw <= 0.0f || vh <= 0.0f)
        return;

    // Map normalised finger coordinates (over the window/swapchain) into the
    // ImGui viewport space that the overlays draw in. The viewport is centred
    // within the swapchain for some aspect-ratio settings.
    int pw = 0, ph = 0;
    GameWindow::GetSizeInPixels(&pw, &ph);
    const float sw = pw > 0 ? float(pw) : vw;
    const float sh = ph > 0 ? float(ph) : vh;
    const float offX = (sw - vw) * 0.5f;
    const float offY = (sh - vh) * 0.5f;

    // Snapshot the active fingers (id + viewport-space position).
    struct FingerPt { SDL_FingerID id; ImVec2 pos; };
    std::vector<FingerPt> fps;
    {
        std::lock_guard lock(g_mutex);
        fps.reserve(g_fingers.size());
        for (const auto& f : g_fingers)
            fps.push_back({ f.id, { f.nx * sw - offX, f.ny * sh - offY } });
    }

    XAMINPUT_GAMEPAD st{};
    auto* dl = ImGui::GetForegroundDrawList();

    // ---- Left analog stick ----
    const ImVec2 stickC(vw * STICK_CX, vh * STICK_CY);
    const float baseR = vh * STICK_BASE_R;
    const float thumbR = vh * STICK_THUMB_R;
    const float zoneR = vh * STICK_ZONE_R;

    // Resolve which finger owns the stick. A finger grabs it on touch-down inside
    // the zone and keeps control until it is lifted - even if it wanders outside
    // the zone, in which case the thumb just clamps to the base radius.
    const ImVec2* stickPos = nullptr;
    if (g_stickFingerId != (SDL_FingerID)-1)
    {
        for (const auto& fp : fps)
            if (fp.id == g_stickFingerId) { stickPos = &fp.pos; break; }

        if (!stickPos)
            g_stickFingerId = (SDL_FingerID)-1; // owning finger was lifted
    }
    if (g_stickFingerId == (SDL_FingerID)-1)
    {
        for (const auto& fp : fps)
        {
            const float dx = fp.pos.x - stickC.x;
            const float dy = fp.pos.y - stickC.y;
            if (dx * dx + dy * dy <= zoneR * zoneR)
            {
                g_stickFingerId = fp.id;
                stickPos = &fp.pos;
                break;
            }
        }
    }

    ImVec2 thumbPos = stickC;
    bool stickActive = false;
    if (stickPos)
    {
        const float dx = stickPos->x - stickC.x;
        const float dy = stickPos->y - stickC.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        const float cl = std::min(len, baseR);
        const float ux = len > 0.0f ? dx / len : 0.0f;
        const float uy = len > 0.0f ? dy / len : 0.0f;

        thumbPos = { stickC.x + ux * cl, stickC.y + uy * cl };

        const float ax = (ux * cl) / baseR;          // -1..1, right positive
        const float ay = (uy * cl) / baseR;          // -1..1, screen-down positive
        st.sThumbLX = int16_t(std::clamp(ax * 32767.0f, -32767.0f, 32767.0f));
        st.sThumbLY = int16_t(std::clamp(-ay * 32767.0f, -32767.0f, 32767.0f)); // up = forward

        stickActive = true;
    }

    // Buttons are hit-tested against every finger except the one driving the stick.
    std::vector<ImVec2> pts;
    pts.reserve(fps.size());
    for (const auto& fp : fps)
        if (fp.id != g_stickFingerId)
            pts.push_back(fp.pos);

    dl->AddCircleFilled(stickC, baseR, IM_COL32(0, 0, 0, stickActive ? 90 : 55), 48);
    dl->AddCircle(stickC, baseR, IM_COL32(255, 255, 255, 130), 48, 3.0f);
    dl->AddCircleFilled(thumbPos, thumbR, IM_COL32(255, 255, 255, stickActive ? 170 : 110), 32);

    // ---- Right face buttons ----
    const ImVec2 faceC(vw * FACE_CX, vh * FACE_CY);
    const float spread = vh * FACE_SPREAD;
    const float faceR = vh * FACE_BTN_R;
    DrawFaceButton(dl, pts, { faceC.x, faceC.y + spread }, faceR, EButtonIcon::A, XAMINPUT_GAMEPAD_A, st);
    DrawFaceButton(dl, pts, { faceC.x + spread, faceC.y }, faceR, EButtonIcon::B, XAMINPUT_GAMEPAD_B, st);
    DrawFaceButton(dl, pts, { faceC.x - spread, faceC.y }, faceR, EButtonIcon::X, XAMINPUT_GAMEPAD_X, st);
    DrawFaceButton(dl, pts, { faceC.x, faceC.y - spread }, faceR, EButtonIcon::Y, XAMINPUT_GAMEPAD_Y, st);

    // ---- Shoulders ----
    const float shHW = vh * SHOULDER_HW;
    const float shHH = vh * SHOULDER_HH;
    if (DrawWideButton(dl, pts, { vw * SHOULDER_LX, vh * SHOULDER_Y }, shHW, shHH, shHW * 0.75f, shHH * 0.85f, EButtonIcon::LB))
        st.wButtons |= XAMINPUT_GAMEPAD_LEFT_SHOULDER;
    if (DrawWideButton(dl, pts, { vw * SHOULDER_RX, vh * SHOULDER_Y }, shHW, shHH, shHW * 0.75f, shHH * 0.85f, EButtonIcon::RB))
        st.wButtons |= XAMINPUT_GAMEPAD_RIGHT_SHOULDER;

    // ---- Triggers ----
    if (DrawWideButton(dl, pts, { vw * SHOULDER_LX, vh * TRIGGER_Y }, shHW, shHH, shHH * 0.95f, shHH * 0.95f, EButtonIcon::LT))
        st.bLeftTrigger = 255;
    if (DrawWideButton(dl, pts, { vw * SHOULDER_RX, vh * TRIGGER_Y }, shHW, shHH, shHH * 0.95f, shHH * 0.95f, EButtonIcon::RT))
        st.bRightTrigger = 255;

    // ---- Start / Back ----
    const float menuHW = vh * MENU_HW;
    const float menuHH = vh * MENU_HH;
    if (DrawWideButton(dl, pts, { vw * 0.5f + vw * MENU_DX, vh * MENU_Y }, menuHW, menuHH, menuHW, menuHH, EButtonIcon::Start))
        st.wButtons |= XAMINPUT_GAMEPAD_START;
    if (DrawWideButton(dl, pts, { vw * 0.5f - vw * MENU_DX, vh * MENU_Y }, menuHW, menuHH, menuHW, menuHH, EButtonIcon::Back))
        st.wButtons |= XAMINPUT_GAMEPAD_BACK;

    g_state = st;
}
