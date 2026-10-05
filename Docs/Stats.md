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

## The hub ship (Hollowkeel, D61 blockout)

Every number from `Docs/Design/HubShip/HubShip_Data.json` (generated from `hub_data.js`); measured on the blockout in `L_HubBlockout`.

| Stat | Value | Notes |
|---|---|---|
| Size | 160 m on deck (188 m with the bowsprit), 40 m beam, keel at -12 m | 37.8 m wide at the main deck (the hull's tumblehome) |
| Decks | quay +1.5, lower +5.5, middle +9.5, upper +13.5, main +17.5 m | 4 m floor to floor, 3.7 m clear under the slabs |
| Open at the start | 6 466 of 31 234 m2 (20.7 %) | |
| Bulwarks | 0.9 m with a cap rail | the 1.4 m player's eyes see over them |
| Stern Dock | basin 44 x 20 m, 5 m deep; the arch 21.5-25.5 m clear | quays 4 m wide at +1.5 |
| Stairs | 22 steps of 18 cm on 30 cm treads per storey, 3 m wide | ramps in the collision |
| Fitting berth | x 32..42, y 7.9 (0.4 m off the wall) | the boat moors there (`MooringPierHeight` 150 cm) |
| Walking from the fitting berth | Slip 4.6 s, Tally House 10.2 s, Keelgate 33.4 s, Chandlery 63.1 s | 2.5 m/s; sprinting about 0.6 x |
| Her berth | (81, 415) m, bow to 92 degrees | off the lagoon's channel, held by two bow cables and two stern moorings |
| Calm zones | 20 % within 120 m of her (full by 300 m), 10 % → 7 % in the basin | Storm: +1.06 m under the hull, +0.34 m in the basin |
| Lanterns below decks | 60 cd, 2100 K, lit by day | exposure down to EV 3 inside |

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
