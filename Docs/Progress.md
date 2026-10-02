# Progress

Last updated: 2026-10-03.

## Milestone status

| Milestone | Status |
|---|---|
| M1 Infrastructure & foundation | **Done** (verified 2026-10-03) |
| M2 First-person player & moving-platform tests | **Done** (verified 2026-10-03) |
| M3 Visual test scene | Next: water spike, then the scene and the feedback checkpoint |
| M4 Starting boat & navigable water | Planned |
| M5 Starting harbor & complete loop | Planned |

## M1 – Infrastructure (done)
Implemented:
- Git repository with LFS (`.gitattributes`) and ignore rules; the template baseline is the first commit.
- Template clean-up (Horror/Shooter variants removed); automation enabled (Python remote execution, MCP).
- Blender pipeline (`sb_blender.py`, headless build script) and Interchange importer (`import_assets.py`, manifest).
- Test map `L_DevTest`; packaging script; launch/build scripts.

Verified:
- `SM_Crate_Small` imports at 61.2 × 41.2 × 40.2 cm (expected from the geometry), with correct +X/+Y axis
  markers, both slot materials assigned by name, and one convex collision hull. A line trace hits the UCX
  surface at exactly −20.0 cm.
- The standalone Development build packages successfully (5 min 40 s for the first full cook), launches
  fullscreen at 2560×1440, and loads `L_DevTest`. On that near-empty map: 158 fps, frame 6.3 ms,
  GPU 5.9 ms at the default 60.6 % TSR screen percentage. This is not representative yet.

## M2 – Player & moving platform (done)
Implemented:
- `ASaltPlayerCharacter`: 176 cm capsule, eye at 165 cm, walk 2.5 m/s, sprint 4.2 m/s, low jump. No head
  bob, and the camera never rolls.
- Interaction: focus volumes on a dedicated trace channel, a 5 cm sphere probe out to 2.2 m, and an
  occlusion check. HUD shows a subtle dot that brightens on focus, plus a fading `[E] <action>` prompt.
- Control stations: step in with a 0.45 s blend in station space; look limits relative to the station;
  WASD routed to the station's owner; E to leave (with a fallback exit point if the exit is blocked).
- Moving decks: CharacterMovement based movement on a kinematic deck that moves before the character
  each frame. Rotational base velocity for kinematic decks via `ISaltKinematicBase`; `bStayBasedInAir`
  keeps jumps in the deck's frame.
- Dev tools: `ASaltDevMovingPlatform` and the automated suite `sb.Dev.RunMovementTests` (runner:
  `python Tools/run_pie_test.py "sb.Dev.RunMovementTests <motion> <speed> <turn>"`).

Verified (PIE, about 120 fps). All 10 checks pass in each of three conditions:

| Condition | Standing drift (12 s) | Eye jitter RMS / max | Jump landing offset | Slide after stop |
|---|---|---|---|---|
| Motion 1× (heave 25 cm, pitch 2.5°, roll 4°), 3 m/s, 8°/s | 0.25 cm | 0.009 / 0.25 cm | 1.0 cm | 0.0 cm |
| Motion 2.5×, 5 m/s, 15°/s | 0.27 cm | 0.031 / 0.26 cm | 4.4 cm | 0.25 cm |
| Motion 4× (≈1 m heave, 16° roll), 6 m/s, 25°/s | 1.0 cm | 0.058 / 0.27 cm | 8.7 cm | 1.3 cm |

Standalone Development build (fullscreen 2560×1440, `-ExecCmds` run of the suite at 1×/3 m/s/8°/s): all 10
checks pass, with eye jitter RMS 0.010 cm, jump offset 1.8 cm, about 149 fps.

Further checks: walking speed relative to the deck matches input. Focus on the helm, entering (stand error
0.000 cm), steering, and leaving (exit error < 1 cm, back on the deck) all work.

Finding: before `bStayBasedInAir`, a jump on a turning deck landed 24 cm (moderate) or 66 cm (rough) from
the take-off point. The suite caught it; it is now fixed. Also found: the side lane next to a 200 cm wide
deckhouse was narrower than the player capsule. The rule is now walkways ≥ 70 cm.

## Controls
| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move | W A S D / arrow keys | Left stick |
| Look | Mouse | Right stick |
| Sprint | Left Shift (hold) | Left stick click |
| Jump | Space | A / Cross |
| Interact / leave station | E | X / Square |
| At a station | W/S and A/D are routed to the station (e.g. speed / steer) | Left stick |

Developer console (`~`, non-shipping builds): `sb.Dev.RunMovementTests [motion speed turn]`,
`sb.Dev.Platform <motion> [speed turn]`, `sb.Dev.ResetPlayer`, `sb.MouseSensitivity <x>`,
`sb.Dev.ShowInteractionVolumes 1`.

## Known issues and limitations
- About 0.25 cm vertical camera pops on heaving decks. This comes from CharacterMovement's floor-distance
  correction; it is below the visible threshold in tests so far. A small camera-height smoothing can
  remove it if it ever shows.
- Mouse sensitivity is a first guess (0.07°/count × 1.0) and has not been tested by a human. Adjust with
  `sb.MouseSensitivity`.
- No player body or hands; there are no footstep or interaction sounds yet.
- The key label in the prompt is fixed to "E" and does not reflect the gamepad mapping.
- `L_DevTest` uses grid placeholder materials; `BoardingRamp` is not aligned to the deck yet.

## Next steps
1. **M3 water spike:** compare the engine Water plugin (rendering + CPU wave query) with a custom shared
   Gerstner ocean on accuracy (boat sits on the visible surface), look, and cost. Then decide D6.
2. **M3 visual test scene:** deck section with railing, lantern, dock, rock/coast and water. Day and
   evening lighting; first master materials and palette. Request feedback at this checkpoint.
3. Keep validating the standalone build after each milestone.
