// Minimal example wiring for Astral CS2 Advanced Bhop.
//
// Same shape as the basic version, but the Inputs struct also wants
// the local pawn's velocity + eye_angles for the auto-strafe pass.
// Both are 12-byte / 12-byte reads off the local pawn, same shape
// as you'd already wire for an aim / ESP feature.
//
// What you fill in:
//   * ReadBuf(addr, dst, len)  -- bulk-read from CS2 memory
//   * Cs2ClientBase()          -- module base of client.dll
//   * dwLocalPlayerPawn etc    -- offsets from cs2-dumper output
//
// Build: link with bhop.cpp + bhop.h on the include path. C++17.

#include "../bhop.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ===========================================================================
// === USER-SUPPLIED MEMORY ADAPTER ==========================================
// ===========================================================================
// Replace these with calls into your own driver / IOCTL / RPM / etc.

extern bool ReadBuf(uint64_t addr, void* dst, size_t len);
extern bool Cs2ClientBase(uint64_t* out);

// Stub fallbacks. Delete once your driver is in place.
__attribute__((weak)) bool ReadBuf(uint64_t /*addr*/, void* dst, size_t len) {
    if (dst) std::memset(dst, 0, len);
    return false;
}
__attribute__((weak)) bool Cs2ClientBase(uint64_t* out) {
    if (out) *out = 0;
    return false;
}

template <class T>
bool Read(uint64_t addr, T* out) {
    return ReadBuf(addr, out, sizeof(T));
}

// ===========================================================================
// === FOREGROUND CHECK ======================================================
// ===========================================================================

bool IsCs2Foreground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    wchar_t cls[64] = {};
    if (GetClassNameW(fg, cls, 64) <= 0) return false;
    return wcscmp(cls, L"Valve001") == 0;
}

// ===========================================================================
// === CS2 OFFSETS ===========================================================
// ===========================================================================
// Pull these from your cs2-dumper output. Update on every CS2 patch.
// Placeholders below WILL drift -- do not ship them.

constexpr uint64_t kDwLocalPlayerPawn = 0x0;     // dwLocalPlayerPawn
constexpr uint64_t kMfFlags           = 0x0;     // C_BaseEntity::m_fFlags
constexpr uint64_t kVecVelocity       = 0x0;     // C_BaseEntity::m_vecVelocity
constexpr uint64_t kAngEyeAngles      = 0x0;     // C_CSPlayerPawn::m_angEyeAngles
constexpr int      kFlOnGround        = 0x1;

// ===========================================================================
// === FRAME LOOP ============================================================
// ===========================================================================

bool ReadLocalState(astral_bhop::Inputs& out) {
    out.grounded     = true;
    out.velocity     = {0,0,0};
    out.eye_angles   = {0,0,0};

    uint64_t client_base = 0;
    if (!Cs2ClientBase(&client_base) || !client_base) return false;

    // CHandle decoding from dwLocalPlayerPawn -> real pawn pointer is
    // exercise-for-the-reader; left as a stub. Wire your existing
    // entity-list helper here.
    uint64_t local_pawn = 0;
    (void)local_pawn;
    return false;

    /* once you have local_pawn:
    int flags = 0;
    Read<int>(local_pawn + kMfFlags, &flags);
    out.grounded = (flags & kFlOnGround) != 0;
    Read<astral_bhop::Vec3>(local_pawn + kVecVelocity,   &out.velocity);
    Read<astral_bhop::Vec3>(local_pawn + kAngEyeAngles,  &out.eye_angles);
    return true;
    */
}

int main() {
    astral_bhop::Config cfg;
    cfg.enabled        = true;
    cfg.hotkey_vk      = VK_XBUTTON1;     // Mouse 4
    cfg.auto_knife     = true;
    cfg.knife_slot_vk  = '3';
    cfg.lastinv_vk     = 'Q';

    cfg.autostrafe     = true;
    cfg.autostrafe_vk  = VK_XBUTTON1;     // same key = combined behavior
    cfg.alternate_dir  = false;
    cfg.strafe_power   = 1.6f;            // tune for your sens

    std::printf("[advanced-bhop-example] running.\n");
    std::printf("  Hold Mouse 4 in CS2 to bhop + auto-strafe.\n");
    std::printf("  Ctrl-C to exit.\n");

    using namespace std::chrono_literals;
    while (true) {
        astral_bhop::Inputs in;
        in.game_focused = IsCs2Foreground();
        if (in.game_focused) ReadLocalState(in);

        astral_bhop::Tick(cfg, in);

        std::this_thread::sleep_for(8ms);   // ~125 Hz tick
    }
}
