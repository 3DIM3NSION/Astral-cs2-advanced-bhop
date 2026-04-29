// Astral CS2 Advanced Bhop -- drop-in bhop + pro-style auto-strafe.
//
// Single-header public API. Pair with bhop.cpp.
//
// Tick() is a pure-input state machine: per-frame inputs in, Win32
// input synthesis out. No memory hooks, no driver code, no game
// injection from this module. You wire your own way to read the
// few CS2 fields it needs.
//
// Single-threaded. Call Tick() from one thread.

#pragma once
#include <cstdint>

namespace astral_bhop {

struct Vec3 { float x, y, z; };

struct Config {
    // Master toggle. When false Tick() is a no-op + resets internal state.
    bool enabled = false;

    // Bhop activation hotkey (Win32 VK code).
    //   != 0  -- holding this key starts a chain. Module taps space on
    //            landing for you. Default 0x05 = VK_XBUTTON1 = Mouse 4.
    //   0     -- no hotkey, fall through to "real spacebar held" detection
    //            so users with a single space bind still get the retap.
    int hotkey_vk = 0;

    // Auto-knife. Chain start -> tap knife_slot_vk (default '3' = slot 3).
    // Chain end -> tap lastinv_vk (default 'Q' = lastinv) to swap back.
    // Knife base movement speed is 260 u/s -- highest of any equip.
    bool auto_knife    = false;
    int  knife_slot_vk = '3';
    int  lastinv_vk    = 'Q';

    // ---- Auto-strafe ------------------------------------------------------
    // Pro-style air-strafe: peak source-engine air acceleration happens
    // when view yaw leads the velocity vector by a small offset and the
    // matching strafe key is held. Turn rate scales with current XY speed
    // so we hold a flat air-accel curve from start to peak. Holds A or D
    // ourselves AND nudges the mouse on the same axis at a speed-aware
    // rate.

    bool autostrafe = false;

    // Auto-strafe activation hotkey. Independent from hotkey_vk so the
    // user can run bhop without strafe (or vice versa) by binding them
    // to different keys.
    //   != 0  -- holding this key gates auto-strafe AND, like hotkey_vk
    //            above, can start a chain on its own. Default Mouse 4 so
    //            one button covers both behaviors out of the box.
    //   0     -- always-on whenever a chain is live (started by hotkey_vk
    //            or real spacebar).
    int autostrafe_vk = 0;

    // When true, alternate strafe direction left / right each hop when
    // the user isn't actively holding A or D themselves. False = no
    // assist when neither A/D nor any lateral velocity is present
    // (player goes straight).
    bool alternate_dir = false;

    // Mouse turn-rate multiplier (clamped 0.3..3.0). Higher = harder turn.
    // Tune for your in-game sensitivity. Too low = no acceleration; too
    // high = over-rotate and lose speed instead of gaining it.
    float strafe_power = 1.0f;
};

struct Inputs {
    // True iff the local pawn's m_fFlags has the FL_ONGROUND bit (0x1).
    bool grounded = true;

    // True iff CS2 has the foreground window.
    bool game_focused = false;

    // Local pawn m_vecVelocity. Used by auto-strafe to compute the
    // along-track speed and side-scalar projection that picks strafe
    // direction at take-off.
    Vec3 velocity = {0,0,0};

    // Local pawn m_angEyeAngles. (pitch, yaw, roll) in DEGREES.
    // Auto-strafe needs the yaw to project velocity onto the side
    // axis (which strafe direction matches the player's drift).
    Vec3 eye_angles = {0,0,0};
};

// Per-frame entry point. Call once per main-loop iteration.
void Tick(const Config& cfg, const Inputs& in);

}  // namespace astral_bhop
