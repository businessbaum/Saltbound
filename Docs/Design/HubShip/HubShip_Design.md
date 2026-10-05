# The hub ship: game concept and design (proposal)

Owner (2026-10-05): "design a big ship hub for the player. One big ship with multiple decks and multiple shops and some
areas you can't access yet. The big ship is the shop and it travels with the player: when the player goes to another
harbour it's already there. You never see it moving." The player unlocks only about 20 % of it; the rest is planned space,
not designed yet. Four NPCs: ships and ship upgrades, side quests for money (with a currency), the main quest, equipment.
Only the spaces around the NPCs are designed here, not the NPCs.

The sketches are in `HubShip_Plans.html` (open it in a browser) and as pictures beside this file:

| Sheet | What it shows |
|---|---|
| `HubShip_01_concept.jpg` | The idea: the loop, "always already there", salt |
| `HubShip_02_approach.jpg` | Arriving: the stern arch from the sea (mood, at one scale with your boat) |
| `HubShip_03_section.jpg` | Long section on the centreline: every deck, the masts, open and locked |
| `HubShip_04_exploded.jpg` | The decks pulled apart, with the player's way through |
| `HubShip_05_cross.jpg` | Cross-sections through the Stern Dock and through the Well |
| `HubShip_06_plans.jpg` | The five deck plans with walls, doors, gates, NPC spots and interaction points |
| `HubShip_07_zones.jpg` | The locked 80 %: the register, the six gate types, open and locked per floor |
| `HubShip_08_slip.jpg` | The shipwright's space (the Slip) |
| `HubShip_09_tally.jpg` | The jobs space (the Tally House) |
| `HubShip_10_keelgate.jpg` | The main quest's space (the Keelgate) |
| `HubShip_11_chandlery.jpg` | The equipment space (the Chandlery) |

The drawings come from code: `hub_data.js` holds every number (the plans, the zones and the area count all read it),
`hub_kit.js` the ink-and-wash drawing kit, `hub_sheets1.js` / `hub_sheets2.js` the sheets.
`python Tools/design/render_hub_ship.py [sheet-id ...]` renders them to the JPEGs, prints the areas and writes
`HubShip_Data.json`: every number (the ship, the zones, the NPC spots, interaction points, props and gates) as plain data
for the build's Blender and Unreal scripts. They are review drawings, not
game content.

Nothing in this document is built. It is a proposal for the owner's review; the questions are at the end.

---

## 1. Game concept

### What the hub is for
Saltbound stays a sailing game: the sea is where you play. The hub ship is where a voyage ends and the next one
begins. You do not open a shop menu from your boat; you sail into a ship as big as a town and walk her decks to the
people who build boats, pay for jobs, sell gear and know something about the sea's past.

It also answers the art direction's "warm home, cool world": the open sea and the ruins are weathered and cool, your
boat is your warm small home, and the hub is the warm big home of everyone else: lanterns, voices, hammering, smoke
from a galley chimney.

### The loop
1. **Sail out** from the hub's dock.
2. **Explore** islands, ruins and wrecks.
3. **Gather**: finds, and what jobs ask for.
4. **Sail back** to the nearest harbour; the hub is already there.
5. **Get paid** in salt at the Tally House.
6. **Spend** at the Shipwright (boats, upgrades) and the Chandlery (equipment). Follow the main quest at the Keelgate.
7. Sail further than before.

### Always already there
- She lies at the berth of the harbour you last moored at, or the one you are sailing to.
- She moves only while you are more than **2.5 km** from both berths, where the sea haze (D49) hides both. There is no
  sailing and no animation: one moment she is behind you in the haze, the next she lies ahead.
- Today there is one harbour, so she lies off Saltbound Cove (the "Saltbound Roads", about 600 m west-north-west of the
  pier, stern to the cove so you sail straight from the pier into her dock). The rule matters from the second harbour on.
- She lies still. A ship this big at anchor barely moves, and a still hub is simple, cheap and comfortable to walk:
  static geometry, no moving-platform physics. Only lanterns, flags and pennants move.

### Salt, the currency (recommended)
- **Salt**: cakes of pressed sea salt in waxed paper, stamped with the knotted ring of the logo. Counted as one number
  ("240 salt").
- Why: the game is called Saltbound; sailors are "worth their salt" (the word salary comes from it); the sea gives salt,
  but clean pressed salt is what every harbour needs to keep fish and to trade. One unit, nothing to change.
- It is a **purse**, not a satchel item: no slot, shown at counters and on the satchel's lip. Saved with the game.
- **Earn**: jobs at the Tally House (and, if wanted, selling finds at its weighing bench). **Spend**: the Shipwright and
  the Chandlery. The main quest costs nothing.
- Price bands to start from (tuned later in a price table, like the stats tables of D59): a job 40-150, a piece of gear
  60-400, a boat upgrade 150-900, the second boat about 2 500.
- Alternatives: **Knots** (tallies tied in a trade cord, like the logo's knot; but "knots" is also the speed unit) or
  **Bells** (brass tokens).

### The ship's identity (a suggestion; the owner decides the story)
- Working name **Hollowkeel**. Alternatives: Mother Brine, the Long Harbour.
- Below her waterline she is not timber. Her keel and lower hull are **ancient**: pale, seamless, with faint turquoise
  lines (the reserved colour of the lost civilisation, `MI_AncientGlow`). Generations of sailors built a timber town of
  decks, galleries and houses on top of it.
- Nobody aboard knows how she gets from harbour to harbour. The great wheel on her poop is lashed and never turns.
- That is the main quest's door: the Keelgate, a sealed ancient hatch at the foot of her central well.

## 2. The ship

| | |
|---|---|
| Length | 160 m on deck, 188 m with the bowsprit (the first boat: 10 m; the second boat proposal: 14 m) |
| Beam | 40 m |
| Draught | 12 m (the ancient keel) |
| Main deck | 17.5 m above the water, twelve and a half times the player's height |
| Poop | 33.5 m above the water, over the Stern Arch |
| Masts | three: mizzen 54 m, main 76 m, fore 64 m above the main deck; sails always furled |
| Floors | 31 747 m², counted on a 0.5 m grid (sea dock included) |
| Open at the start | **6 446 m², 20 %** (planned and locked: 25 301 m², 80 %) |

Coordinates in the plans: x forward from the stern face (0) to the stem (160), y to starboard, z up from the waterline.
In Unreal: +X forward, +Y starboard, +Z up, 1 m = 100 uu, the actor's pivot at x 80 on the waterline.

### Decks (floor heights above the waterline)
| Floor | z | What is there |
|---|---|---|
| Poop | +33.5 | the great wheel, lashed; stern lanterns (locked) |
| Sterncastle storeys | +21.5, +25.5, +29.5 | the two wings and the Stern Arch (locked) |
| Forecastle | +21.5 | (locked) |
| **Main deck** | +17.5 | the Waist (open), the Chandlery, the Well's opening; cargo deck, sterncastle and forecastle (locked) |
| Upper deck | +13.5 | the Well's gallery (open); Archive, Inn (locked) |
| Middle deck | +9.5 | the Well's gallery (open); Guild rooms, Lantern Market (locked) |
| Lower deck | +5.5 | Tally House, the lane, the Well's gallery (open); Ropewalk, Mess, Purser's stores, wing quarters (locked) |
| **Quay deck** | +1.5 | Stern Dock and quays, the Slip, the Keelgate (open); Holds, stores, Keel Walk (locked) |
| The Deep | -8 | ancient, behind the Keelgate (locked) |

4 m from floor to floor, about 3.5 m clear under the beams: generous for a 1.4 m player, so the decks feel like halls of
a big ship and the camera never scrapes.

### The big shapes
- **Stern Dock**: the stern is split. Two wings (each 10 m wide) stand either side of a basin of sea water 44 m long
  and 20 m wide, open to the sky, 5 m deep. A **Stern Arch** joins the wings 25.5 m above the water and carries the
  great cabin and the poop. You **sail in under the arch** and moor inside: the dock is the hub's front door.
  Arcaded quays (4 m wide) run along both sides at +1.5 under the wings.
- **The Slip** at the head of the basin: a slipway rises out of the water into a boat hall 12 m high.
- **The Well**: a 12 x 12 m shaft through the middle of the ship from the Keelgate's floor (+1.5) to the sky, with
  galleries 5 m wide on every deck, a stair spiralling up round it (one flight of 22 steps per deck, starboard, forward,
  port, aft) and a hoist cage in its corner. It is how you move up and down, and it shows you every floor you cannot
  enter yet.
- **The main deck**: 72 m of open deck from the dock head rail to the cargo rope, under the masts.

## 3. The open 20 %: the way through

Measured from the zone rectangles in `hub_data.js`:

| Open space | Floor | m² |
|---|---|---|
| Stern Dock (basin 880, quays 352) | quay deck | 1 232 |
| The Slip (boat hall 360, shop 200) | quay deck | 560 |
| Dock stair | quay deck | 42 |
| The Keelgate chamber | quay deck | 576 |
| Tally House | lower deck | 226 |
| Starboard lane | lower deck | 54 |
| The Well's galleries | lower, middle, upper | 3 x 340 |
| Main deck, the Waist (with the Chandlery, 176) | main deck | 2 736 |
| **Total** | | **6 446 (20 %)** |

The way through (the dotted line on sheets 4 and 6):
1. **Sail in** under the arch. The lit boat hall and the boat on the slip are straight ahead.
2. **Moor** at the fitting berth (starboard quay, by the derrick) or a fleet berth (port quay). Mooring saves (D46).
3. **The Slip**: step off onto the quay; the shipwright's counter is 15 m forward.
4. **Dock stair** up to the lower deck, into the **Tally House**: the job board ahead, the pay hatch to the left, the boat
   below through the windows to the right.
5. **The lane** forward to **the Well**.
6. **Down** one flight (or the hoist) to the **Keelgate**.
7. **Up** three flights (or the hoist) to the **main deck**: the masts, the view over the harbour, the **Chandlery**.

Everything is within about 150 m of walking (a minute) from the mooring. The loop closes back down the Well and the
dock stair, or by the hoist to the lower deck.

## 4. The four spaces (the NPCs are not designed here, only their places)

Each space has: one NPC spot (a dashed ring on the sheets, with the way the NPC faces), the interaction points (E), what
the player sees first, and its closed edges.

### S · The Slip: ships and upgrades (`HubShip_08_slip.jpg`)
- **Where:** quay deck at the head of the dock. Boat hall 20 x 18 m, 12 m high, open to the basin at its aft end; the
  slipway (10 m wide) runs from the hall floor down into the water. Shop 20 x 10 m on its starboard side, under the
  Tally House.
- **For:** showing the next boat for sale, whole, on the slip (the 14 m cutter of D51, unrigged, her mast on trestles);
  fitting upgrades to your boat at the **fitting berth** beside the derrick; choosing and paying at the counter. Later:
  the **fleet berths** on the port quay keep the boats you own; you change boats here.
- **NPC spot:** behind the counter (8 m long, at the hall's edge), facing the hall and the slip.
- **E:** the counter (buy, talk), the plans board on the shop's hull wall (the upgrade list), the boat on the slip (look
  her over), the fitting berth (moor there to have upgrades fitted).
- **Also:** half-models of the boats for sale on the bulkhead, timber racks, a workbench, three hanging lanterns, the
  "SHIPWRIGHT" sign over the hall mouth.
- **Seen first:** from the sea, through the arch: the hall lit at the end of the dock.
- **Closed edges:** Keel Walk door (G1), timber store grille (G6), quay stores (G4).

### J · The Tally House: side jobs and pay (`HubShip_09_tally.jpg`)
- **Where:** lower deck, one room 20 x 11 m, 4 m high, over the shipwright's shop. The dock stair arrives through its aft
  wall; the lane leaves from its forward wall to the Well. Its inboard windows look down into the boat hall.
- **For:** reading the jobs on the board (six slips), taking one, coming back, being paid in salt. Goods for delivery jobs
  go on the weighing bench.
- **NPC spot:** inside the pay booth on the hull wall, behind a grilled hatch, facing into the room.
- **E:** the job board, the pay hatch (hand in, get paid), the weighing bench (deliver).
- **Also:** a long table with benches (sailors waiting for pay), salt cakes stacked by the booth, two open ports, three
  lanterns, a chalk slate under the board.
- **Seen first:** up the dock stair: the board lit straight ahead.
- **Closed edges:** the purser's door (G1).
- **Job kinds this space is made for** (the content is the owner's): bring goods (driftwood, glass floats), find
  something near a named place, deliver to another harbour, survey (sail past places and report), salvage (once there is
  a winch). They use what exists: items (D50), places and landfall (D55), the boat.

### Q · The Keelgate: the main quest (`HubShip_10_keelgate.jpg`)
- **Where:** quay deck at the foot of the Well. Chamber 24 x 24 m, 4 m high at its edge; its middle 12 x 12 m is the
  Well, open 16 m up to the sky. The Keelgate is a pale ancient ring 8 m across set in the floor, with turquoise lines.
- **For:** the quiet heart of the ship, where the main quest is given and returned to. The gate is the way into the
  ancient Deep, sealed for now.
- **NPC spot:** at the reading desk by the port wall, facing the gate (the gate lies between them and the stair).
- **E:** the NPC (talk), the gate (look at it), the hoist.
- **Also:** shelves of salvaged ancient pieces (a few faintly glowing), rubbings of the gate's lines pinned up, the desk
  with books and a lamp, two lanterns. Seven empty sockets round the gate's rim are an idea for the owner (the ancient
  shard of D50 lies at Ruin Isle), not a rule.
- **Mood:** the desk's warm lamp against the pale gate; daylight falling down the Well by day, the gate's own faint glow
  by night.
- **Closed edges:** grilles into the Holds (G6), the Keel Walk door (G1), the Keelgate itself (G5).

### E · The Chandlery: equipment (`HubShip_11_chandlery.jpg`)
- **Where:** main deck, a deckhouse 16 x 11 m (4.2 m to the eaves) on the port side between the Well and the main mast.
  Its shop front with an awning faces the open deck and the mast.
- **For:** equipment: the tools of the wheel today (compass, spyglass, lantern) and later gear. Buy at the counter from
  the deck, or walk in among the shelves.
- **NPC spot:** behind the counter inside, facing out to the deck.
- **E:** the counter (buy, talk); two try-it spots on deck: a spyglass on a tripod looking over the harbour, a compass
  binnacle.
- **Also:** shelves floor to roof with a ladder, stock hanging from the beams (lanterns, coils, a net), a display table,
  the "CHANDLER" sign on the roof edge.
- **Seen first:** up the Well's stair onto the main deck: the awning and the sign ahead to port, under the masts.
- **Closed edges:** none in the shop; the cargo deck's rope (G2) is 2 m forward of it.

## 5. The locked 80 %: planned, not designed

Seventeen zones with working names (the owner names and designs each when it opens). Areas from the same count.

| Id | Zone | Where | m² | Gate |
|---|---|---|---|---|
| L1 | Sterncastle and Stern Arch | main deck aft and three storeys above; the poop and the great wheel | 3 880 | G1 |
| L2 | Wing quarters | both wings over the dock, lower and upper deck | 1 734 | G4 |
| L3 | Quay stores | behind both arcades | 389 | G4 |
| L4 | Timber store | port of the Slip | 200 | G6 |
| L5 | Keel Walk | between the Slip and the Keelgate (the first shortcut to open) | 304 | G1 |
| L6 | The Holds | quay deck forward and round the Keelgate | 2 316 | G6, G3 |
| L7 | Purser's stores | lower deck, between the Tally House and the Well | 225 | G1 |
| L8 | The Ropewalk | lower deck, port side, 100 m long | 993 | G1 |
| L9 | Mess and galley | lower deck forward | 1 825 | G1 |
| L10 | Guild rooms | middle deck aft | 1 670 | G1 |
| L11 | Lantern Market | middle deck forward | 2 521 | G2, G4 |
| L12 | Archive | upper deck over the Slip | 1 160 | G1 |
| L13 | Inn and bunkrooms | upper deck forward and round the Well | 2 521 | G1 |
| L14 | Forecastle and bow | two storeys and the bowsprit | 1 226 | G1, G3 |
| L15 | Cargo deck | main deck between the Waist and the forecastle | 672 | G2 |
| L16 | Masts and tops | three masts, their tops and yards | - | G3 |
| L17 | The Deep | under the waterline, behind the Keelgate | 3 661 | G5 |

What the player sees of each from outside is on sheet 7 (for example: the great windows glow at night; the Mess's
chimney smokes on the main deck; a rope across the Market's door with a board, "Market closed").

### Gates (sheet 7)
| Gate | Look | Used for |
|---|---|---|
| G1 Barred door | a timber door, an iron bar, a padlock | rooms and businesses not open to you yet |
| G2 Rope and board | a rope across the way, a hand-painted board | work going on, closed for now |
| G3 Chained ladder or hatch | a chain through rungs or a grating, padlocked | masts, forecastle, cargo hatches |
| G4 Shutters | closed shutters | windows and bays of locked rooms |
| G5 Ancient seal | pale seamless stone, turquoise lines, no handle | the Deep (main quest) |
| G6 Iron grille | bars you see through into a shallow set | a glimpse into stores and holds |

### How a locked zone is built now
- Only its shell (the outside of the ship) and its gate. No room behind the gate, no props, no collision past it.
- Every opening into it is closed: doors barred, windows shuttered, grilles with a set 2-3 m deep and no further.
  Nothing that would need an interior is visible.
- E at a gate gives one short line of why ("Barred from inside."). No promise of what is behind it.
- Each zone has an id (L1-L17) in the save: unlocked or not. Opening one later means designing it then.

## 6. How it is set up (for the build)

### Scale and kit
- Player 1.4 m (eyes 1.2 m). Doors 2.2 x 1.2 m (double doors 3 x 2.4 m), stairs 18 cm risers on 30 cm treads, flights
  3 m wide, rails 1.0 m, walkways never under 1.5 m. Steps that the player walks are ramps in the collision (CharacterMovement
  steps 25 cm at most).
- A modular timber kit from `sb_timber` (D20, D38): hull planking panels, deck slabs with beams every 2 m, posts, rails,
  stair flights (4 m rise), gallery ring pieces, doors (3 sizes), windows and ports with shutters, the six gates, lanterns,
  signs. Hand-crafted style (D16, D38): chunky, gently crooked, broad bevels, plain textures. Nothing as clean primitives.
- Colours: dark tarred hull planking with ochre bands at the deck lines, cream window frames, muted brass on the arch's
  carvings and the quarter galleries, golden-brown decks; the ancient hull pale with turquoise lines (used sparingly).
- Every object from its purpose (D53): the derrick has its post, boom, blocks and hook; the hoist its gantry, ropes,
  counterweight and brake; lanterns hang from hooks and swing (`USaltSwingComponent`).

### Collision and walking
- The hub is **static**. Floors, stairs and rails are simple boxes (UBX/UCX) as on the boat; the ship's outside is
  complex-as-simple for the boats' hull capsules (the basin walls, quays and the slipway must stop a boat; the slipway
  is a beach the boat runs up on, as at Ruin Isle).
- **No open space below the waterline.** All open floors are at +1.5 or higher. The ocean renders through closed hulls
  (D42 masks only the player's boat), so nothing below the water may be walkable or visible from inside.
- The sea under and round the hub stays below +1.2 m in every sea state: a calm zone round her (10 % in the basin, 30 %
  within 300 m), on top of the home waters zone (D58).

### Light
- The dock and the Well are open to the sky: daylight reaches the quays, the Slip's mouth and, down the Well, the Keelgate.
- Lanterns everywhere people are (2100 K, as `ASaltLantern`); the lighting rig lights them at evening and dusk.
- Locked windows are dark by day; some glow at night (lights only, no rooms).
- Interiors must read as lit, not black (Lumen): check with a capture per space at Day and Dusk.

### Systems the build needs
1. **The hub actor and berths:** `ASaltHubShip` (the whole ship as one streamed level instance) and `ASaltHubBerth` per
   harbour (position, heading, its calm zone). The follow rule above. The berth id is saved.
2. **Mooring inside the hub:** today the boat moors only at its start position (`ASaltBoat::MooringRadius`). The hub needs
   mooring points (the fitting berth, the fleet berths) the windlass can moor to; mooring there saves.
3. **Unlock flags:** `L1`-`L17` in the save; gates read them (all locked for now).
4. **The purse:** salt as a saved number; shown at counters and on the satchel's lip (the UI is not designed yet).
5. **Interaction points** at every E on the sheets, with placeholder prompts ("Talk", "Read the board") until the NPCs
   and shop screens are designed.
6. **The hoist:** a station like the windlass: E to ride, three stops (Keelgate, lower deck, main deck).
7. **Swimmers:** rung ladders from the water onto the quays (the D54 ladder).
8. **Performance:** Nanite for the hull and the kit, HLOD for the far view, the interiors of the open spaces only; measure
   in a packaged build with the editor closed (D8: 60 fps).

## 7. Not in this design
NPCs (looks, names, voices, dialogue), quest and job content, shop screens and prices beyond the bands, the locked
zones' insides, the hub ever moving in view, a second harbour, the second boat itself (D51), the map table.

## 8. Steps
1. **Owner review** of this sheet set (questions below).
2. **Blockout:** the hull and the open 20 % in Blender and in the game, walkable, with the gates as simple markers; check
   scale, headroom, walking times, light and the boat in the dock. Owner review in the editor.
3. **Hand-crafted art pass,** space by space: the dock and the Slip first (the first thing seen), then the Tally House,
   the Well and the Keelgate, the main deck and the Chandlery, the outside last.
4. **Systems:** berths and the follow rule, hub moorings, the purse, unlock flags, the hoist.
5. NPCs, jobs, shop screens: designed with the owner after that.

The build prompt for the agent is `HubShip_Agent_Prompt.md`; it builds from `HubShip_Data.json`.

## 9. Questions for the owner (recommendation first)
1. **Name:** Hollowkeel · Mother Brine · the Long Harbour · your own.
2. **Currency:** salt · knots · bells.
3. **First sight:** she appears off the cove after your first landfall (you come back from your first island and she is
   simply there) · she is there from the start.
4. **Size:** 160 m (as drawn) · 120 m (cheaper to build, less "really really big") · 200 m.
5. **Entry:** only by sailing into the stern dock (as drawn) · also a landing stage on her side.
6. **The ancient hull:** visible at the waterline (as drawn) · hidden (all timber outside; the mystery only inside).
7. **Selling finds** at the Tally House's weighing bench for a little salt: yes (jobs pay better) · no (only jobs pay).
