# Stats

Every object's numbers, one table per kind (D59, owner: "create lists for stats for each object like the ships").
The tables live in code; change a number there and the game follows (the boat reads its row when play starts, the
player when it is made). "Measured" values come from the tests and are not set anywhere: they follow from the others.

| Kind | Table in code |
|---|---|
| Boats (and their stores) | `Saltbound/Source/Saltbound/Core/SaltStats.cpp`, `SaltStats::Boats()` |
| Player | `Core/SaltStats.h`, `FSaltPlayerStats` (the defaults are the values) |
| Items | `Core/SaltItems.cpp`, `SaltItems::All()` |
| Tools | `Core/SaltTools.cpp`, `SaltTools::All()` |
| The hub ship's systems (D63) | the `Hub` category of `Hub/SaltHubShip.h`, `SaltHubBerth.h`, `SaltHubHoist.h`, `SaltHubLadder.h`, `SaltHubGate.h`; the places from `Docs/Design/HubShip/HubShip_Data.json` (`build_hub_level.py`) |

## Boats

### Double-ender (the first boat, `DoubleEnder`)

10 m gaff-rigged double-ender, 3.4 m beam, 10.9 t, steered with an oar on the starboard quarter.

| Stat | Value | What it does |
|---|---|---|
| Sail thrust | 10 000 N | Forward force of the full sail in a 7 m/s breeze (was 7 800 N before D59) |
| Hull speed | 3.75 m/s | Past it the resistance climbs steeply |
| Resistance at hull speed | 1 700 N | |
| Coasting drag | 1 500 N s/m | How fast she slows when the sail comes in |
| Set time | 2.5 s | E at the pin rails: the sail fully set or taken in |
| Trim time | 3 s | W / S at the helm: from no sail to full (or back) |
| Sail belly | 70 cm | How deep the wind presses the cloth beyond its cut, in a fresh breeze |
| Rudder | 35 deg at 50 deg/s | The oar's most and how fast it turns |
| Turning at rest | 1 500 N m | Sculling with the oar while stopped |
| Barrel | 10 slots | 3 in the middle, 7 around |
| Crate | 10 slots | 5 x 2 |

Measured (before D59, 7 800 N): 6.7-7.4 kn at every point of sail in a 7 m/s breeze, half a minute to full speed, a
22 m turning circle. With 10 000 N: about 8.4 kn on a reach (computed from the resistance model; not yet measured).

Size of the things on deck (D59, `Art/Blender/Scripts/build_boat.py`): barrel 0.54 m across and 0.86 m high, crate
0.68 x 0.5 x 0.48 m, sea chest 0.8 x 0.52 x 0.42 m.

### Gaff cutter (the second boat, D51)

Not built yet: a 14 m gaff cutter with a top deck and a lower deck (`Docs/Design/Boat2.md`). Its row is added when it is.

## The hub ship (Hollowkeel, D61 blockout, D64 layout)

Every number from `Docs/Design/HubShip/HubShip_Data.json` (generated from `hub_data.js`); D64 measured in `L_HubReview`.

| Stat | Value | Notes |
|---|---|---|
| Size | 160 m on deck (188 m with the bowsprit), 40 m beam, keel at -12 m | 37.8 m wide at the main deck (the hull's tumblehome) |
| Decks (D64) | the Deep -4.5, hold -1.5, dock deck +1.5, lower +4.5, middle +7.5, upper +10.5, gun +13.5, main +16.5 m; sterncastle 19.5 / 22.5 / 25.5, poop 28.5; forecastle 19.5, its deck 22.5 | 3 m floor to floor, 2.7 m under the slabs, 2.4 m under the beams |
| Rooms (D64) | 893 rooms on 14 floors, corridors fore and aft and across every 24 m | `hub_plan.py`; every room has a door |
| Open at the start | 5 602 of 47 720 m2 (11.7 %, D64; D61: 20.7 % of 31 234 m2) | more floors at 3 m |
| Bulwarks | 0.9 m with a cap rail | the 1.4 m player's eyes see over them |
| Water Hall (D64) | water 16 x 40 m (x 30..46), its floor -4; quays 4 m wide at +1.5; 13.2 m clear over the water | galleries over both quays on three decks; the slipway x 46..60 |
| Side doors (D64) | a pair in each side at x 32..40, 4 m leaves, -2.5 to 13.0 m; open in 10 s (the aft leaf 10 degrees ahead); clear way 5.54 m at its narrowest (z 6), 7.1 m at the waterline | the watch opens for a boat within 45 m, shuts 20 s after; `sb.Hub.Doors` |
| Stairs | 17 steps of 17.6 cm on 30 cm treads per storey (D64), 3 m wide in the Well, 2.5 m the dock stair | ramps in the collision |
| Berths (D64) | fitting x 43.9, y 6..16 and a 10 m fleet berth x 43.9, y -16..-6 (the forward quay 2.1 m to starboard); 14 m fleet berth x 32.3, y -7..7 (the aft quay 2.3 m to port) | all bow to port; clear of the leaves' swing |
| Walking from the fitting berth | Slip 3.0 s, Tally House 6.4 s, Keelgate 27.5 s, Chandlery 55 s (D64) | 2.5 m/s; sprinting about 0.6 x |
| Her berth | (81, 415) m, bow to 92 degrees | off the lagoon's channel, held by two bow cables and two stern moorings |
| Calm zones | 20 % within 120 m of her (full by 300 m), 7 % in the water hall (D64; was the basin) | Storm (D63): +1.20 m under the hull, +0.36 m in the basin |
| Lanterns below decks | 60 cd, 2100 K, lit by day, reach 10 m (D63; was 16 m) | exposure down to EV 3 inside, up to EV 7.5 |
| Rig (D62) | masts 54 / 76 / 64 m over the main deck; round tops 14 m (main); shrouds 5 / 8 / 6 a side | ratlines every 0.5 m; lateen yard 34 m on the mizzen |
| Art (D64) | outside about 13 700 pieces, inside every floor one mesh (`SM_Hub_Lvl_*`, about 4 M vertices) plus the designed spaces | about 3 min to build all (`build_hub_*.py`); the blockout collides, one part per floor |
| Her level (D63) | `L_Hub`: her pivot (x 80, the waterline) at the origin, bow to +X | `ASaltHubShip` streams it in at a berth (`HubLevel`) |
| Follow rule (D63) | she moves only while the player is more than 2 500 m from both berths, checked every 2 s | `FollowDistanceM`, `CheckInterval`; a berth's harbour is 1 200 m round it (`HarbourRadiusM`) |
| First sight (D63) | hidden until the first island is found; then she appears only beyond 2 500 m or outside 75 degrees of the view | `bHiddenUntilLandfall`, `ViewHalfAngle`; `sb.Hub.Show 1` |
| Moorings (D63) | fitting berth (the quay 2.1 m to starboard), fleet berths (2.1 m and 2.5 m to port); the quays 1.5 m over the water | the windlass moors to the nearest within 8 m (`MooringRadius`); 11-29 cm off the pose moored |
| Hoist (D63) | stops quay / lower / main deck at 0 / 3 / 15 m (D64); 1.6 m/s, 0.6 m/s2 to start and stop | D63 measured: 4 m in 4.8 s, 12 m in 9.7 s; `HoistSpeed`, `HoistAccel` |
| Counterweight (D63) | 0.8 x 1.2 x 1.5 m of stone, runs the other way, in its railed bay when the cage is at the main deck | its sheave 0.9 m aft and 2.3 m to port of the cage's |
| Gates (D64) | 38 blockers (the Keelgate, G5, blocks nothing): 32 at the locked corridors' mouths (2 m wide, door high), the cargo rope, a hatch, two shroud plates, the forecastle's two ladders | block while their zone (L1-L18) is locked; `sb.Hub.Unlock` |
| Quay ladders (D64) | 4 (the aft quay's edge at y 14 and -15, the forward quay's at -8 and 17), rungs every 30 cm | from the water onto the quay in 2.3 s |
| Salt purse (D63) | a saved number, 0 at the start; nothing spends it yet | `sb.Dev.Salt <n>` |
| Storm sea at her (D63, `HubSeaLow`) | under the hull up to +1.20 m, in the basin up to +0.36 m (60 s) | right at the 1.2 m limit; D61 measured +1.06 m in another minute of the same sea |

## Player

| Stat | Value | What it does |
|---|---|---|
| Walk / sprint | 250 / 420 cm/s | |
| Acceleration / braking | 2 800 / 2 600 cm/s2 | To full speed and to a stop in about a tenth of a second (D59; was 1 500 / 1 800) |
| Ground friction | 10 | (was 8) |
| Step height | 25 cm | Walked up without a jump |
| Jump | 420 cm/s at 1.5 x gravity | A 60 cm jump, quick up and down (D59; was 340 cm/s at 1 x: the same height, floatier) |
| Air control | 0.35 | Steering in the air (was 0.25) |
| Swim / swim fast | 300 / 460 cm/s | |
| Ladder | 190 cm/s | The same up and down, eased in and out (D59) |
| On to a ladder / over its top | 320 cm/s / 0.35 s | (D59) |
| Eyes | 120 cm | Above the feet; body 1.4 m, 26 cm radius |
| Reach | 220 cm | How far E reaches |

## Items

| Item | Stack | Notes |
|---|---|---|
| Driftwood | 20 | |
| Seashell | 30 | |
| Coil of rope | 10 | Two in the satchel at the start |
| Glass float | 10 | |
| Ancient shard | 5 | "Hold it up to the light" (no use yet) |

The satchel has 24 pockets.

## Tools

Compass, spyglass (view narrows to 20 degrees in 0.35 s), lantern (2 100 K, 28 cd), satchel, empty hands. The player owns
the compass, the spyglass and the lantern from the start.
