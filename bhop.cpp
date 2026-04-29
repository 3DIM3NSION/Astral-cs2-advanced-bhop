// Astral CS2 Advanced Bhop -- implementation.
// See bhop.h for public API + design notes.

#include "bhop.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <math.h>

namespace astral_bhop {

namespace {

constexpr int   kJumpKey          = VK_SPACE;
constexpr int   kStrafeL          = 'A';
constexpr int   kStrafeR          = 'D';

// Air-strafe tuning constants. Inherited from CS:S / CS:GO speedrun
// recordings (HSW / kZ runs); CS2's air physics share the same model.
// strafe_power scales these at runtime.
constexpr float kStrafeMinSpeed   = 30.0f;     // u/s lateral before assist engages
constexpr float kMouseBase        = 4.0f;      // baseline px/tick at rest
constexpr float kMouseScalePerUps = 0.040f;    // additional px/tick per u/s
constexpr int   kMouseMin         = 2;
constexpr int   kMouseMax         = 32;

// ----- Win32 input helpers ------------------------------------------------

void TapKey(int vk) {
    INPUT down{}, up{};
    down.type     = INPUT_KEYBOARD;
    down.ki.wVk   = (WORD)vk;
    down.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    down.ki.dwFlags = KEYEVENTF_SCANCODE;
    up = down;
    up.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &down, sizeof(INPUT));
    SendInput(1, &up,   sizeof(INPUT));
}
inline void TapJump() { TapKey(VK_SPACE); }

// Track held / released state explicitly. Avoids repeating SendInput
// every frame while a key is held -- only fires on transitions.
void HoldKey(int vk, bool press, bool& held) {
    if (press == held) return;
    INPUT k{};
    k.type     = INPUT_KEYBOARD;
    k.ki.wVk   = (WORD)vk;
    k.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    k.ki.dwFlags = KEYEVENTF_SCANCODE | (press ? 0u : KEYEVENTF_KEYUP);
    SendInput(1, &k, sizeof(INPUT));
    held = press;
}

void NudgeMouseX(int dx) {
    if (dx == 0) return;
    INPUT mi{};
    mi.type        = INPUT_MOUSE;
    mi.mi.dwFlags  = MOUSEEVENTF_MOVE;
    mi.mi.dx       = dx;
    mi.mi.dy       = 0;
    SendInput(1, &mi, sizeof(INPUT));
}

}  // namespace

void Tick(const Config& cfg, const Inputs& in) {
    // ----- State -----
    static bool g_strafe_l    = false;
    static bool g_strafe_r    = false;
    static bool g_was_ground  = true;
    static bool g_chain_live  = false;
    static bool g_knife_done  = false;
    static int  g_alt         = -1;     // -1 none, 0 left, 1 right

    // ----- Master gates ------------------------------------------------------
    if (!cfg.enabled || !in.game_focused) {
        // Release any synthetic A/D we might be holding so they don't
        // leak into the next focused window.
        if (g_strafe_l) HoldKey(kStrafeL, false, g_strafe_l);
        if (g_strafe_r) HoldKey(kStrafeR, false, g_strafe_r);
        g_chain_live = false;
        g_knife_done = false;
        g_was_ground = true;
        return;
    }

    // ----- Activation --------------------------------------------------------
    // Chain stays alive while ANY of: bhop hotkey, auto-strafe hotkey, real
    // spacebar. Auto-strafe key is folded into the trigger set so the user
    // can put bhop+strafe on a single non-Mouse-4 button (e.g. Mouse 5
    // alone = bhop + strafe combo) without ALSO needing the bhop hotkey held.
    const bool hotkey_held  = cfg.hotkey_vk != 0 &&
                              (GetAsyncKeyState(cfg.hotkey_vk) & 0x8000) != 0;
    const bool as_phys_held = cfg.autostrafe_vk != 0 &&
                              (GetAsyncKeyState(cfg.autostrafe_vk) & 0x8000) != 0;
    const bool space_held   = (GetAsyncKeyState(kJumpKey) & 0x8000) != 0;
    const bool active       = hotkey_held || as_phys_held || space_held;
    const bool grounded     = in.grounded;

    // First-jump synth.
    if ((hotkey_held || as_phys_held) && grounded && !g_chain_live) {
        TapJump();
    }

    // ----- Chain transitions -------------------------------------------------
    if (g_was_ground && !grounded && active) {
        g_chain_live = true;
        if (cfg.auto_knife && !g_knife_done) {
            TapKey(cfg.knife_slot_vk);
            g_knife_done = true;
        }
    }
    if (!active) {
        if (cfg.auto_knife && g_knife_done) {
            TapKey(cfg.lastinv_vk);
        }
        g_chain_live = false;
        g_knife_done = false;
    }

    // Bhop retap on landing.
    if (g_chain_live && active && grounded) {
        TapJump();
    }

    // ----- Auto-strafe -------------------------------------------------------
    bool want_l = false, want_r = false;

    // Auto-strafe gate. vk == 0 = always-on while chain is live.
    const bool as_hotkey_held = cfg.autostrafe_vk == 0 ||
                                (GetAsyncKeyState(cfg.autostrafe_vk) & 0x8000) != 0;

    if (cfg.autostrafe && g_chain_live && as_hotkey_held) {
        // User-held A/D wins -- never fight a deliberate strafe.
        const bool user_a = (GetAsyncKeyState(kStrafeL) & 0x8000) != 0;
        const bool user_d = (GetAsyncKeyState(kStrafeR) & 0x8000) != 0;

        // Direction selection priority:
        //   1. live A/D held by user
        //   2. existing lateral velocity at take-off (match the drift)
        //   3. alternate L/R if user enabled it
        //   4. otherwise no assist (player goes straight)
        if (user_d) g_alt = 1;
        else if (user_a) g_alt = 0;
        else if (g_was_ground && !grounded) {
            // Project velocity onto the side axis to detect drift direction.
            const float yaw = in.eye_angles.y * 0.01745329252f;  // deg -> rad
            const float c   = cosf(yaw), si = sinf(yaw);
            const float side = in.velocity.x * si - in.velocity.y * c;
            if      (side >  kStrafeMinSpeed) g_alt = 1;
            else if (side < -kStrafeMinSpeed) g_alt = 0;
            else if (cfg.alternate_dir)       g_alt = (g_alt == 1) ? 0 : 1;
            else                               g_alt = -1;
        }

        if (!grounded && g_alt >= 0) {
            const bool right = (g_alt == 1);
            if (!user_a && !user_d) {
                want_r =  right;
                want_l = !right;
            }
            // Mouse turn rate scales with current XY speed -- keeps the
            // air-accel curve flat instead of overrotating at peak velocity.
            const float vx       = in.velocity.x;
            const float vy       = in.velocity.y;
            const float speed_xy = sqrtf(vx*vx + vy*vy);
            float power = cfg.strafe_power;
            if (power < 0.3f) power = 0.3f;
            if (power > 3.0f) power = 3.0f;
            int dx = (int)((kMouseBase + speed_xy * kMouseScalePerUps) * power);
            if (dx < kMouseMin) dx = kMouseMin;
            if (dx > kMouseMax) dx = kMouseMax;
            NudgeMouseX(right ? dx : -dx);
        }
    }

    HoldKey(kStrafeL, want_l, g_strafe_l);
    HoldKey(kStrafeR, want_r, g_strafe_r);

    g_was_ground = grounded;
}

}  // namespace astral_bhop
