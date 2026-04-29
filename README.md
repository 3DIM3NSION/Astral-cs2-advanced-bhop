# Astral CS2 Advanced Bhop

Drop-in bunny-hop module for CS2 with pro-style auto-strafe. Synthesizes
spacebar on landing, holds A/D in the air, nudges the mouse just enough
to keep the source-engine air-accel curve flat, swaps to knife mid-chain
for the speed bonus, swaps back when you're done. Single `Tick()` call
per frame.

This is the same code that ships in [Astral Project](https://github.com/3DIM3NSION/Astral-Project-CS),
pulled out into a single header / source pair so anyone can plug it into
their own loader.

> Just want the bhop / auto-knife without auto-strafe? See
> **[Astral CS2 Bhop](https://github.com/3DIM3NSION/Astral-cs2-bhop)** —
> same module without the air-strafe pass. Smaller integration surface
> if all you want is the chain.

---

## What it does

- **Bhop.** Hold the configured key (default Mouse 4). The module taps
  space the moment you land, every landing, no rhythm required.
- **Auto-knife (optional).** Chain start → tap `3` (slot 3, knife).
  Chain end → tap `Q` (lastinv). Knife base move speed is 260 u/s,
  highest of any equip — meaningful accel across a long chain.
- **Auto-strafe.** While airborne mid-chain, holds A or D and nudges
  the mouse on the same axis at a speed-aware rate. Direction picks
  itself: held A/D wins → existing lateral velocity → optional auto-
  alternate → otherwise straight. Independent hotkey so you can run
  bhop without strafe (or vice versa).
- **Foreground gate.** Won't ever fire if CS2 doesn't have focus.
  No leaking A/D / mouse-moves / space taps into chat, your terminal,
  or whatever else stole foreground.

---

## Why auto-strafe

Source-engine air acceleration peaks when the player's view yaw
**leads** the velocity vector by a small offset and the matching
strafe key is held. Turn the mouse smoothly in the same direction
as the strafe and you maintain that offset while gaining speed.

The math is well-understood, the timing isn't — it takes practice
to do consistently by hand. This module automates it. Turn rate
scales with current XY speed (`base + speed * scale`) so you keep
the accel curve flat from start to peak instead of overrotating
once you're moving fast.

The `strafe_power` knob is a flat multiplier on that turn rate.
Tune it for your in-game sensitivity. Too low and you don't
accelerate; too high and you over-rotate and lose speed. Sane band
for ~2.5 sens is around 1.5–2.0.

---

## Why bhop the way we bhop

CS2 doesn't let you bind two different keys to `+jump` like CS:GO did.
You're stuck with one bind on the spacebar. This module sidesteps that
by never touching the bind — it just synthesizes spacebar input at the
exact frame you land, every landing. Your real spacebar still works
alongside it; this is purely additive.

Auto-knife is the cleanest way to claw back the ~10% movement speed
CS2 takes from you when you're holding a rifle. Slot 3 (knife) base
speed is 260 u/s vs 250 for an AK and 220 for an AWP. That extra
accel compounds across a long chain.

---

## How it runs

The module is a **state machine + Win32 SendInput**. No memory hooks,
no driver, no game injection from this module. We synthesize keypresses
+ mouse-moves into the Windows input queue; the OS delivers them to
whatever has focus.

You provide four things per frame:

- `grounded` — does the local pawn's `m_fFlags` have `FL_ONGROUND` (`0x1`)?
- `game_focused` — does CS2 have the foreground window?
- `velocity` — local pawn `m_vecVelocity` (Vec3 of u/s)
- `eye_angles` — local pawn `m_angEyeAngles` (pitch, yaw, roll **in degrees**)

How you read those is up to you. kdmapper'd vulnerable driver, your
own kernel driver, RPM through a handle, hardware DMA, anything that
lets you read CS2 process memory. **We don't ship that part.** Different
projects have different security postures and threat models — pick yours.

For `game_focused`, the stock approach is `GetForegroundWindow()` against
CS2's `Valve001` window class. No driver involved. See `example/main.cpp`.

---

## Integration

```cpp
#include "bhop.h"

astral_bhop::Config cfg;
cfg.enabled        = true;
cfg.hotkey_vk      = VK_XBUTTON1;     // Mouse 4 -- hold to bhop
cfg.auto_knife     = true;
cfg.knife_slot_vk  = '3';
cfg.lastinv_vk     = 'Q';

cfg.autostrafe     = true;
cfg.autostrafe_vk  = VK_XBUTTON1;     // same key = combined behavior
cfg.alternate_dir  = false;
cfg.strafe_power   = 1.6f;            // tune for your sens

// Once per frame:
astral_bhop::Inputs in;
in.grounded     = read_local_pawn_grounded();
in.game_focused = is_cs2_foreground();
in.velocity     = read_local_pawn_velocity();
in.eye_angles   = read_local_pawn_eye_angles();
astral_bhop::Tick(cfg, in);
```

That's the whole API. Two structs, one function. See `example/main.cpp`
for a complete shell with stub memory adapters — replace the stubs with
whatever read primitive your driver exposes.

**The module is single-threaded.** Call `Tick()` from one thread only —
the static state machine inside isn't synchronized.

---

## Multi-key patterns

Three useful binding setups you can hit out-of-the-box with the two
hotkey fields:

| `hotkey_vk` | `autostrafe_vk` | Behavior |
|---|---|---|
| Mouse 4 | Mouse 4 | One button covers both. Hold M4 = bhop + strafe. (default) |
| Mouse 4 | Mouse 5 | M4 alone = bhop only. M5 alone OR M4+M5 = bhop + strafe. |
| Mouse 4 | 0 (unbound) | M4 = bhop. Strafe is always-on whenever a chain is live. |
| 0 (unbound) | 0 (unbound) | No hotkeys -- use the real spacebar. Strafe always-on. |

Either hotkey can start a chain on its own — the auto-strafe key
isn't gate-only, it's also a chain trigger. So binding strafe to
M5 and pressing M5 alone gets you bhop + strafe with one button,
without also having to hold M4.

---

## What it doesn't do

- **Doesn't read memory.** You wire that. We don't even include the
  field offsets; pull those from [a2x/cs2-dumper](https://github.com/a2x/cs2-dumper)
  and update on every CS2 patch.
- **Doesn't bypass anti-cheat.** SendInput is detectable in many ways.
  If you ship this in a context that gets you banned, that's on you.
- **Doesn't handle weapon detection.** Auto-knife is unconditional on
  chain start. If you want "only swap when holding a rifle", layer
  that yourself.
- **Doesn't lobby-check.** It'll happily try to bhop on the main menu
  if you don't gate `enabled` on actually-in-a-match yourself.

---

## Disclaimer

Provided for **educational and research purposes**. Running this on
official CS2 servers (Valve matchmaking, Faceit, ESEA, anything with
a real anti-cheat) violates terms of service and will eventually get
your account banned. We don't run it on competitive matchmaking; you
shouldn't either. Use it offline against bots, on community servers
that explicitly allow it, or as a learning reference for input-synthesis
state machines.

The author and contributors aren't liable for VAC bans, lost rank,
hardware damage, broken accounts, or anything else that comes from
running this. **By using this code you accept that.**

---

## License

MIT. Use it, fork it, ship it commercially, embed it in your own loader.
The only ask: keep the copyright notice. Recognition is the whole point.

If you build something cool with it, drop a link in an issue — always
nice to see where this stuff ends up.

```
Copyright (c) 2026 3DIM3NSION / Astral Project
```
