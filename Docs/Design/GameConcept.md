# Saltbound: the game concept (proposal)

Owner (2026-10-06): develop a concept for what the game is about and what the player can do. "Besides a good story it is above
all about an exciting experience that does not get boring." Inspiration from other games is allowed; it can include a combat
system, a story system, a progress system. Start with the game concept, then: what should there be, what can the player do, how
do they progress, how do we hook them?

**The owner's answers before writing (2026-10-06):**

| Question | Answer |
|---|---|
| Combat | **Light and tool-based.** The sea and its creatures are the danger; you answer with tools and seamanship. No human enemies. |
| Structure | **A hand-made open sea with a story.** It opens step by step through the boat; the main quest runs through ancient sites, in any order within a tier; it has an ending. |
| Pressure | **Boat and sea only.** No hunger or thirst. Hull damage, weather, night, and cargo lost in a wreck (salvageable). |
| Length | **About 10-15 hours.** |

Everything else is a proposal. Names (Tidewrights, Tidestones, the Gale, the Grey, the Cartographer...) are working names. The
questions are at the end (section 12, recommendation first).

---

## 0. The concept on one page

**Pitch.** Saltbound is a first-person sailing adventure in a hand-made sea that nobody leaves. You wash ashore after a storm,
take over a vanished chart-maker's old boat and follow their log to the ruins of the people who made this sea: through squalls,
fog and the dark of deep water. Your boat is your home and grows with you; the giant ship Hollowkeel is everyone's home and wakes
with you.

**Four pillars**
1. **Sail it by hand.** Every journey is sailed. Wind, sea, weather and the boat's condition decide the route.
2. **Every horizon is a question.** From anywhere at sea something calls: a glow, smoke, birds, a wreck. Every place keeps a
   secret, and knowledge opens doors as much as upgrades do.
3. **The sea is the opponent.** Storms, reefs, fog, night and a few creatures. You survive by seamanship and tools, not by
   killing. Every danger shows itself before it strikes.
4. **Warm home, cool world.** The art direction's rule as a game rule: voyages are cold, dark and tense; your boat and the
   Hollowkeel are warm, lit and safe. Coming home is the reward, and both homes visibly grow.

**The loop.** Pick a goal at the Hollowkeel (a job, a page of the log, a glow on the horizon) → make ready → sail out → meet the
sea on the way → find, explore, solve → come home → get paid, upgrade, learn → sail further than before.

**Progress** comes from six things, and none of them is a level: **boats** (and their upgrades), **tools**, **knowledge** (the log
and the ancient script), **the chart**, **the Hollowkeel** waking deck by deck, and **salt**.

**The hook.** In the first forty minutes, the player wakes on a beach and finds an old boat with a missing owner's log in its
chest. A glow on the Spire calls them out. When they come back from their first island, a ship the size of a town lies in the
haze where there was nothing before. After that, the game always keeps an open question in view, and something in the world
changes after every big step.

---

## 1. What we have, and what it becomes

The project has a strong core: the sailing, the sea and the hub all work. What is missing is the game around them: a reason to
sail, danger, people, story, progress. Everything below reuses what exists.

| What exists | Today | Its role in the game |
|---|---|---|
| The 10 m double-ender (D36-D65): physics, square sail, steering oar, windlass, anchor, mooring, ladders, mast chart | Sails, moors, anchors, saves | **The Cartographer's old boat**, the player's first home. Its repairs and patched sail (D41) are the story of its last owner |
| The FFT ocean, sea states, weather, regions, haze, time-of-day presets (D25, D27, D49) | Looks and feels right | **The opponent.** Sea states and weather become events; haze makes the horizon a question. A running day and night is missing |
| Swimming, diving, the underwater look (D28) | Works | Diving to wrecks and sunken ruins; reef sharks make it tense |
| Home island, Saltbound Cove (D60) | Built | **The prologue's harbour**: where the castaway wakes |
| The Spire, Ruin Isle with its gate and shard (D47, D50) | Built, first pass | **The first two ancient sites** (Act I) |
| Mistholm, Gull Rock, Longshoal (horizon silhouettes), the Storm waters (D30) | Names and shapes | **The Outer Isles** (Act II) and **the wound in the Gale** (Act III) |
| Satchel, stores, pickups, five items (D50, D52) | Items have no use | Driftwood patches the hull, rope mends rigging, finds are sold and delivered |
| Compass, spyglass, lantern, the tool wheel (D55) | Held, no purpose | Navigation, sightings, and the lantern's trade-off at night |
| Landfall (D55), the chart on the mast (D57) | Works | **Charting** as progress: the chart only shows what you have found |
| The bell, the boat hook, oars, sea chest on deck (D52, D53) | Props | The bell answers fog and frightens rams; the hook grabs flotsam; the chest holds the log |
| The hub ship Hollowkeel (D61-D64): Slip, Tally House, Keelgate, Chandlery, 18 locked zones, salt purse, follow rule | Built, no function | **The town that wakes.** Every big step opens a zone with a new person and a new thing to do |
| Save and voyages, menus, the "From an old log" loading lines (D46, D66) | Works | The log lines become the Cartographer's real words |
| The logo: a compass rose with the ancient shard as north, in a rope ring tied off with a knot (UI_Design) | The game's sign | **The story in one picture**: the shard is the compass that leads the way; the knot is being bound to the sea |
| `Audio/Music/Background/chill`, `dark`, `drama` | Folders | Music by mood: chill in harbour and fair weather, dark at night and in ruins, drama in storms and when the Grey comes |
| `Docs/Ideas.md` (fishing, treasure maps, wrecks, winch, diving bell, workbench, harbours, puzzles, weather, journal) | Ideas | Nearly all are placed in this concept (sections 6 and 7) |

---

## 2. The game

### 2.1 Genre and feel
A first-person **sailing adventure** with **exploration and mystery** at its heart, **light tension** from the sea and a
**growing home**. The sailing stays simple to control (D41, D65: set the sail, trim it with W / S, steer). Its depth comes from
the sea, the weather and the route, not from more ropes to pull.

### 2.2 What Saltbound is not
- No human enemies, no ship-to-ship cannon fights, no multiplayer.
- No hunger, thirst or health bars, no levels or experience points, no grinding.
- No job that sends you on the same trip twice.
- No markers floating in the world. Directions come in sailor's words ("two cables south of Gull Rock, where the reef breaks"),
  on the chart and by the compass. An optional assist in Settings shows a bearing pin for players who want it.
- No procedural world. Every island is placed and made by hand.
- No fast travel at the start: the Hollowkeel (always in the harbour you sail to) and, later, the ancient currents shorten trips.

---

## 3. World and story

### 3.1 The premise
- **The sea.** An archipelago ringed by **the Gale**, a storm wall that never ends. Ships from the world outside are caught in
  it; the Gale breaks them and leaves the survivors on its shores. Nobody has ever sailed out. The people here are those
  castaways and their children, from many ages and coasts. They call themselves **the Saltbound**.
- **The Tidewrights.** Long before them, someone built in pale seamless stone with turquoise lines: the Spire's tower, Ruin
  Isle's gate, the Hollowkeel's keel. They are gone. The Saltbound live among their ruins, keep their shards as luck charms and do
  not understand them. Turquoise stays reserved for them (UI_Design, `MI_AncientGlow`).
- **The Hollowkeel.** The town-ship that always lies in the harbour where she is needed. Nobody knows how she moves (she is never
  seen moving, D63). Her great wheel is lashed. At the foot of her Well lies the **Keelgate**, a sealed ancient ring with
  **seven empty sockets**.
- **The player.** The newest castaway: washed ashore in Saltbound Cove at dawn. Silent and nameless (recommended), so the sea
  is theirs to read.
- **The Cartographer.** A castaway like the player, years ago: the best sailor and chart-maker this sea has known, who never
  stopped wanting to go home. Seven years ago they sailed south into the Storm Waters and did not come back. Their old boat
  still lies at the cove's pier. In its sea chest is their log.

### 3.2 The shape of the sea
The sea opens in rings, and the boat decides how far the player can go.

| Ring | Where | Sea | Holds | Opened by |
|---|---|---|---|---|
| **Inner Waters** | about 2.5 km round the cove | Calm to Moderate, reefs, a few islets | Saltbound Cove, the Spire, Ruin Isle, 4-5 islets | the start |
| **The Outer Isles** | 2.5-6 km | Moderate to Rough, squalls, deep water | Mistholm (in the fog of **the Mistreach**), Gull Rock, Longshoal; two more harbours; 6-8 islets | the reinforced hull |
| **The Storm Waters** | south, spreading | Storm | the eye of the storm, the broken Tidestone | five Tidestones restored and a storm-proof keel |
| **The Gale** | the sea's edge, about 7 km out | beyond Storm | nothing: the end of the world | never (until the ending) |
| **The Deep** | under the Hollowkeel | dry, ancient | her heart | the Keelgate |

- **The Gale is the world's edge, inside the story.** There is no invisible wall: the sea grows worse until the boat is wrecked
  and the player wakes on the nearest shore, like every castaway before them.
- **The Storm Waters are always on the southern horizon** (they are already 3 km south of the cove, D30): a dark wall the player
  sees from the first hour and cannot enter. They are a promise and a threat at once.
- **Distances.** At 8 kn (the first boat, D59) a kilometre takes about four minutes. No leg of the main path should be longer than
  about 12 minutes without something on the way, so the Outer Isles move closer than their placeholder silhouettes (question 8).
- **Region names.** The code still uses Sea of Thieves' names as placeholders (`ShoresOfPlenty`, `AncientIsles`, `Wilds`,
  `DevilsRoar`, D25). They should become ours: Inner Waters, the Outer Isles, the Mistreach, the Storm Waters.

### 3.3 The truth (for the owner to decide)
- **The Gale is not a prison; it is a shelter.** The Tidewrights raised it as a harbour for the shipwrecked from a world outside
  that the sea had broken. Six **Tidestones**, each in an ancient site, anchor it. The Hollowkeel is the Tidewrights' last ship,
  the only one that can pass the Gale; she travels by the currents of the Deep, which is why she is always already there.
- **The Cartographer broke it.** They learned enough of the Tidewrights' script to find the southern Tidestone and took it,
  hoping the Gale would open for them. It did not open; it tore inward. The Storm Waters are the wound, and they grow. The
  Cartographer is trapped in its eye.
- **The shard is the seventh stone.** The glowing shard from Ruin Isle, which leads the player like a compass needle all game
  long (the logo's north point), is the key the Keelgate waits for.
- **The ending.** With six stones restored and the shard set, the Keelgate opens. In the Deep the Hollowkeel's heart wakes and
  her lashed wheel turns. The player chooses: **sail her out through the Gale** (the Cartographer goes home; the shelter opens to
  whatever is outside), or **renew the Gale and stay** (Saltbound by choice; the Cartographer stays with them). Each choice has its
  own short final scene; afterwards the sea stays open to sail (post-game).

The emotional line: the player follows a stranger's footsteps, comes to admire them, learns that their longing broke the world,
and finally has to decide whether that longing was right.

### 3.4 The story in three acts
| Part | Hours | Where | What happens |
|---|---|---|---|
| **Prologue: Ashore** | 0:00-0:40 | Saltbound Cove, the Spire | Waking on the beach; the Harbour Keeper; the old boat and its log; mending the sail; first sail; the Spire's glow at dusk; the first landfall. Back in the cove the Hollowkeel lies in the haze |
| **Act I: The Inner Waters** | 0:40-3:30 | Hollowkeel, the Spire, Ruin Isle, islets | The four people aboard; first jobs and salt; the shard and its pull; the first two Tidestones; the first squall, leak and repair; the first night; the Reader reads the gate: "seven stones hold the Gale". The log points to the Outer Isles; the boat is not strong enough yet |
| **Act II: The Outer Isles** | 3:30-10:00 | Mistholm, Gull Rock, Longshoal, two harbours | The reinforced hull, then the cutter. Three big sites in any order. Night and the Grey; diving deep; the Cartographer's trail through every site. **Midpoint** (after the second big site): their last pages show they took the southern stone. Each stone restored pushes the storm's edge back |
| **Act III: The Storm Waters and the Deep** | 10:00-13:00 | The Storm Waters, the eye, the Keelgate | The crossing of the storm (the big seamanship test); the Cartographer in the eye; the sixth stone; the Keelgate opens; the Deep; the wheel turns; the choice |
| **After** | open | everywhere | Free sailing, the rest of the secrets, every zone of the Hollowkeel |

### 3.5 How the story is told
- **The log** (a new screen in the house style: parchment, ink, the satchel's leather): the Cartographer's pages as they are found
  (about 20), the player's own entries written automatically (places, people, clues, jobs), rubbings of ancient glyphs and the
  lexicon of words decoded so far.
- **Places tell it:** wrecks from different ages, the Cartographer's old camps (a cold fire ring, a chart pinned to a rock, a
  cache with a page), the Tidewrights' rooms.
- **People tell it** in short lines on parchment scraps (the prompt's style). No voice acting: murmurs and sounds instead.
- **The Reader** at the Keelgate is the main quest's voice: they translate what the player brings and set the next goal.
- **Short camera glides** for big moments (like the chart glide of D58), no long cutscenes.
- **The loading screen's old-log lines** become the Cartographer's real words, chosen from pages already found.

### 3.6 People
The owner designs names, looks and voices; these are the roles.

| Where | Role | What they do |
|---|---|---|
| Saltbound Cove | **the Harbour Keeper** | The first face. Gives the boat ("Make her sail and she's yours"), the first small jobs |
| The Slip | **the Shipwright** | Upgrades, repairs, the second boat, towing a wrecked boat home |
| The Tally House | **the Purser** | Jobs and pay |
| The Keelgate | **the Reader** | The main quest, the ancient script |
| The Chandlery | **the Chandler** | Tools and consumables |
| Zones as they open | the Cook, the Archivist, the Ropemaker, the Sailmaker, traders, guild masters, the Innkeeper, the Lookout | Each brings a mechanic (section 6.5) |
| Harbours 2 and 3 | a few each | Local jobs, rumours, a local craft |
| At sea | castaways you rescue, a trader's boat, hermits on islets, the Cartographer | Rescued people move aboard the Hollowkeel and can be met there |

**For the scope:** chunky, stylised figures in the art direction's spirit, mostly seen behind counters, hatches and desks (the
hub's spaces were designed that way) with few animations (idle, a gesture, a turn of the head).

---

## 4. What the player does

### 4.1 The verbs
| Verb | How | Exists? |
|---|---|---|
| **Sail** | Set and trim the sail, steer, anchor, moor, row out of a calm (the oars) | Yes (rowing new) |
| **Navigate** | Compass, spyglass, the mast chart, landmarks, the shard's pull; later the lead line, the map table, beacons and ancient currents | Partly |
| **Explore on foot** | Walk, jump, climb ladders; later the grapnel for cliffs | Partly |
| **Swim and dive** | As now; a breath limit and a diving helmet are new | Partly |
| **Gather** | Pickups; flotsam hooked from the deck with the boat hook; salvage with the winch; fishing | Pickups only |
| **Keep the boat** | Patch holes, bail, mend the sail, choose how much sail to carry in a blow | New |
| **Read and decode** | Take rubbings of glyphs, work out their meanings, read the log | New |
| **Answer danger** | The bell, the lantern, the boat hook, flares, the bow harpoon, the ancient lens | Bell and lantern only |
| **Trade and upgrade** | Earn and spend salt at the four services | Spaces only |
| **Help people** | Jobs, rescues, deliveries | New |
| **Make a home** | The boat's look and trophies; a cabin aboard the Hollowkeel | New |
| **Rest** | Sleep in a bunk to dawn or dusk (on the cutter, at the Inn) | New |

### 4.2 Three loops
1. **The moment (seconds):** read the wind and the water, steer, trim; spot something through the haze or the spyglass; decide
   whether to look closer.
2. **The voyage (20-40 minutes):** choose a goal (a job, a log page, a rumour, a glow) → make ready (upgrades, flares, planks,
   the right weather and time of day) → sail out → the sea happens on the way (section 7.4) → arrive, explore, solve, find → come
   home, or push on at a risk → get paid, save at the mooring, upgrade, learn. **Every voyage ends in a warm harbour.**
3. **The campaign (hours):** restore the Tidestones, open the sea ring by ring, wake the Hollowkeel zone by zone, follow the
   Cartographer to the end.

---

## 5. The sea fights back: danger and the light combat system

### 5.1 The grammar of a threat
Every threat follows the same four rules, so danger is exciting and never unfair:
1. **A tell.** It shows itself before it strikes (at sea at least about 10 seconds ahead): a cloud line, pale water, a fin, a
   silence.
2. **An answer.** Seamanship or a tool always works against it; often more than one.
3. **A cost, never instant death.** Damage, lost time, lost cargo, a faint.
4. **A reward for facing it.** Rough weather and night hold rarer finds, shorter routes and the best stories.

### 5.2 The threats
| Threat | Where and when | Tell | Answer | Cost | From |
|---|---|---|---|---|---|
| **Squall** | Anywhere, more often in the Outer Isles | A dark cloud line, the pennant swings, gusts darken the water | Take sail in (S at the helm), bear away, run for a lee | Knockdown, a torn sail, water aboard | Act I |
| **Reefs and rocks** | Shallows | Pale turquoise water, breakers; the lead line | Steer, slow down, sound, anchor | Holes in the hull | Act I |
| **Night** | Everywhere | Dusk | The lantern (shows reefs, draws the Grey), beacons you have relit, the stars | Reefs unseen; the Grey | Act I |
| **Reef sharks** | Reefs, while swimming or diving | Fins, shadows circling | Leave the water, a flare under water, the lantern | A bite (hurt) | Act I |
| **Fog** | The Mistreach, calm dawns | The haze closes in | Compass, bell buoys, ring your own bell and listen, go slow | Lost, aground | Act II |
| **Rams** | Gull Rock's waters at dusk | Wakes racing in | Steer away, the bow harpoon stings them, the bell scares them | Holes in the hull | Act II |
| **The Grey** | Deep water at night | The sea goes quiet, the lantern flickers, a low sound, a shadow under the hull | Douse every light, take in the sail and drift, throw a flare far off as a decoy, run for the shallows | A heavy blow: big holes, a knockdown | Act II |
| **Guardians** | The Tidewrights' sites | A turquoise eye opens, a hum | Stay out of their light, break their line of sight, turn them with the ancient lens | Thrown out to the room's start | Act I-III |
| **The storm** | The Storm Waters | The whole southern horizon | Everything at once: carry little sail, steer the waves, patch, bail | Wreck | Act III |

**The Grey** is the one horror of the game (the "dark" and "drama" music): never fought, never killed, only avoided or driven
off. Its best tool against you is the lantern you need to see the reefs. That one choice, light or dark, is the night's tension.

### 5.3 The tools you answer with ("combat")
Nothing has a health bar. Creatures that are driven off leave. Every tool also has a use outside danger.

| Tool | Against | Also for | Where from |
|---|---|---|---|
| **Bell** (on the boat) | Rams; fog (echoes from rocks) | Calling at a harbour, a ritual at sites | Exists |
| **Lantern** | Night, reefs, sharks | Caves, ruins; but it draws the Grey | Exists |
| **Boat hook** | Pushes off a creature, the boat off a rock | Hooks flotsam from the deck | Exists as a prop |
| **Flares** (consumable) | Sharks, the Grey (decoy) | Light over a wide area, signals | The Chandlery |
| **Bow harpoon** (boat upgrade) | Rams; makes the Grey dive | A line to salvage or to a rock | The Slip |
| **Ancient lens** | Guardians | Shows hidden glyphs and ways | An Act II site |

### 5.4 Damage, failure and recovery
- **The hull:** holes where it is hit (seen and heard: water spurts in), water in the bilge (the boat sits lower, slower, rolls
  more). Full, she founders. **Patch** a hole: hold E with a plank (driftwood) and the repair kit. **Bail** with a bucket (it
  returns with a purpose, D52); later a hand pump.
- **The sail:** too much sail in a squall tears it (less drive until it is mended with the sail kit or at the Slip). This makes
  the existing trim (W / S at the helm, D59) a real decision.
- **The player:** no health bar. Hurt shows as a red-ink vignette that fades; hurt again soon, they faint. Under water a breath
  limit (the diving helmet lengthens it).
- **A faint:** wake aboard your boat if it is near, otherwise at the nearest harbour. The satchel stays where you fell (a bundle
  on the shore or a floating sack, marked on the chart) until you pick it up.
- **A wreck:** wake at the nearest harbour. The Shipwright has your boat towed home (a salt fee; the first time is free). What was
  in her stores floats at the wreck under a buoy for a day, then sinks (salvage it with the winch or by diving).
- **Never lost:** tools, boats, upgrades, knowledge, the salt purse. No permanent death.

---

## 6. Progression

### 6.1 Boats
| Boat | How you get it | Waters | What it adds |
|---|---|---|---|
| **The double-ender** (10 m, exists) | The prologue: the Cartographer's | Inner Waters; Outer Isles once reinforced | Open deck, barrel and crate, the mast chart, shallow draught (1.45 m) |
| **The cutter** (14 m, D51) | About hour 5: the boat on the Slip (already built in the hub), through the Shipwright's line and salt | Outer Isles, Rough seas | Lower deck with a bunk (sleep and save at anchor anywhere), a stove, **the map table** (chart stage 2, D57), a hold for deliveries; faster, points higher; deeper draught (2.05 m) |
| **Ancient fittings** | From the Tidestones | The Storm Waters | The stone-bound keel (storm-proof), the current sense (ancient currents), the lens |
| **The Hollowkeel** | The ending | Through the Gale | Her wheel turns |

- **Both boats stay useful:** the double-ender at a fleet berth (they exist for this, D63) is the boat for Longshoal's shallows and
  the reefs; the cutter is for open water and long trips.
- **Upgrades** (about 12 across both boats; they show on the boat, never only in a menu): a new sail, reef points (a squall does not
  tear it), tarred seams (fewer leaks), copper sheathing (reefs bite less), ribs and knees (the reinforced hull for Rough seas), a
  stern lantern (light without holding it), a salvage winch, the bow harpoon, a hand pump, a locker (more storage), a barometer
  (warns of squalls).
- **The boat as a home:** hull paint (the old open decision 3), sail patches and colours, flags and pennants, a figurehead,
  trophies from sites and events on deck (a Tidewright tile, a ram's horn, a lucky float).

### 6.2 Tools
The tool wheel takes any number of tools (D57). The player starts with the compass, the spyglass and the lantern (Stats).

| Tool | Does | Opens | From |
|---|---|---|---|
| Compass, spyglass, lantern | As now; the spyglass also **logs sightings** for jobs (a whale, a wreck, a smoke) | | Exist |
| **The shard** | Held up to the light (its existing action) it glows brighter towards the nearest Tidestone not yet restored; later its point turns like a needle | The main path | Ruin Isle |
| **Rubbing kit** | Copies a glyph into the log | The script | The Reader |
| **Repair kit** | Patches holes with planks, mends the sail | Longer voyages | The Chandlery |
| **Lead line** | Sounds the depth and the bottom | Longshoal's shallows | The Chandlery |
| **Flares** | Light, signal, decoy | Nights | The Chandlery |
| **Diving helmet** | Longer breath | Deep wrecks and sunken rooms | An Act II site or the Chandlery |
| **Grapnel** | Climbs cliffs and ledges | Gull Rock | The Chandlery, after a job |
| **Shovel** | Digs at a marked spot | Treasure maps | The Chandlery |
| **Fishing line** | Catches fish for salt and jobs | A calm pastime | The Cove |
| **Ancient lens** | Shows hidden glyphs and ways; turns guardians | Late secrets | An Act II site |

### 6.3 Knowledge: the log and the ancient script
- About **30 Tidewright words** in their glyphs. The first time the player sees a glyph, a rubbing puts it in the lexicon as
  unknown. Context teaches its meaning (the glyph over a water channel, the glyph beside a stair) and the player writes their
  guess. When three guesses on one page are right, they are confirmed (the method of Chants of Sennaar).
- The Reader translates longer texts once enough words are known.
- **Knowledge opens doors.** Some doors and riddles at the sites ask for words, not items: a player who knows them can open them
  early (Outer Wilds). Inscriptions point to secrets that are on no chart.

### 6.4 The chart
- The mast chart (D57) shows **only charted water**: islands appear at landfall (D55), coasts when you sail along them; the rest
  is blank parchment (the Wind Waker sea chart).
- More of it comes from the Archivist's regional charts (coasts only), survey jobs and the Cartographer's pages, whose sketch maps
  are inked onto the chart when found.
- **The map table** on the cutter (stage 2): the whole charted sea, your own marks and notes, course lines, job pins, the bearing
  the shard gave you.
- **Beacons:** each restored Tidestone relights an ancient beacon; at night they make whole areas safe to navigate.

### 6.5 The Hollowkeel wakes
The hub has 18 locked zones (`HubShip_Data.json`). Each opens with a story step or a side line, brings a person, a mechanic and
life (lanterns lit, voices, smoke), and **people the player rescued at sea move into it**. The town visibly fills because of the
player.

| Zone | Opens with | Brings |
|---|---|---|
| L5 Keel Walk | An early Shipwright job | The shortcut from the Slip to the Keelgate (and its hoist up) |
| L9 Mess and galley | The first rescued castaway | The Cook: rumours; fish turn into dishes that help for a voyage (warmer in the water: longer breath; steadier legs on deck) |
| L13 Inn and bunkrooms | The second Tidestone | The Innkeeper: rumours board, sleep to dawn or dusk, rescued people live here |
| L12 Archive | First words decoded | The Archivist: regional charts, glyph help, the lore |
| L8 Ropewalk, L18 Sail lofts | Act II jobs | Rigging and sail upgrades |
| L11 Lantern Market | The second harbour found | Traders: selling finds, trade goods, cosmetics |
| L10 Guild rooms | Act II | Three guilds (salvagers, fishers, surveyors), each a short line of jobs with its own reward |
| L2 Stern quarters | Act II | **Your own cabin**: rest, trophies, half-models of your boats, the Cartographer's pages pinned up |
| L6 Holds, L3 Stern stores | A delivery line | Your store room |
| L16 Masts and tops | Gull Rock | The Lookout: the view, sightings reported to you |
| L1 Sterncastle | Act III | The ship's council; the great wheel |
| L17 The Deep | The finale | Her heart |
| L4, L7, L14, L15 | Small jobs | Small rooms, shortcuts, secrets |

### 6.6 Salt
One currency (the hub design's price bands as the start: a job 40-150, gear 60-400, an upgrade 150-900, the cutter about 2 500).

| Earn | Spend |
|---|---|
| Jobs, selling finds (once the Lantern Market opens), salvage, fish, treasure, rewards for rescues | Upgrades, tools, consumables (flares, planks, nails), charts, towing after a wreck, cosmetics, helping to reopen a zone (a share towards the Ropewalk's repair) |

Salt is never the gate to the story: story steps need places reached and things understood, not money.

### 6.7 No levels
There are no experience points and no stats to raise. The player gets better through what they own, what they know and how well
they sail. A strong player can do some things early, by skill and knowledge alone.

---

## 7. Content: what fills the sea

### 7.1 The ancient sites (the main quest)
Every site follows one recipe: **an approach at sea** with its own challenge, **a place to explore** on foot or under water, **one
puzzle theme** built on one tool or system, **one guardian moment**, **a page of the Cartographer's log**, **the Tidestone**, and
**a change in the world** once it is restored.

| Site | Act | Approach | Inside | Theme | Restored, the world changes |
|---|---|---|---|---|---|
| **The Spire** | Prologue / I | Reefs round the stack | The tower's broken stair to the top | Light: the lantern into the band, the first glyphs | Its beacon burns: the Inner Waters can be sailed at night |
| **Ruin Isle** | I | The beach shelf (D47) | The gate opens to a flooded hall under the knoll | Water and diving: the first guardian | The shard wakes and starts to pull |
| **Mistholm** | II | The Mistreach's fog: bell buoys, compass | Terraces of fog catchers | Mirrors and time of day: sunlight at the right hour | The fog lifts: Mistholm's harbour opens (harbour 3) |
| **Gull Rock** | II | Cliffs, rams at dusk, sea caves only on calm days | The wind organ in the cliffs | Wind and sound: the grapnel, the bell | A steady trade wind and the ancient currents: fast lanes between the isles |
| **Longshoal** | II | Sandbanks and shallows (the small boat, the lead line) | The sunken causeway, its own ancient tide machine | Water levels: raise and drain basins, get back before the tide | The drowned Tidewright harbour rises |
| **The storm's eye** | III | The Storm Waters | The broken anchor, the Cartographer's camp | Everything learned | The storm falls back to a wall again; the Keelgate can open |
| **The Deep** | III | The Keelgate | The Hollowkeel's heart | The script; the shard | The wheel turns; the ending |

Spire and Ruin Isle are short (20-30 minutes each); the three Outer Isles sites are long (45-75 minutes each, with their trips);
the storm and the Deep are the finale (one to two hours).

### 7.2 Harbours
Three harbours, each a berth for the Hollowkeel (the follow rule, D63), each with its own look and craft:
1. **Saltbound Cove**: the home lagoon (exists). The Harbour Keeper, the fishing line.
2. **Longshoal**: a village on stilts over the shallows. Shallow-water jobs, the sounding trade.
3. **Mistholm**: a harbour of old stone under the lifted fog; the Archive's sister house. Opens with the Mistholm site.

### 7.3 Islets: one secret each
Twelve to fifteen small places, each with one thing: a hermit, a castaway's camp, a sea cave, a nesting colony, a tidal pool with
a glyph, a lookout ruin, a wreck on its reef, the Cartographer's cache, a bioluminescent bay at night, a treasure. A rule for
placing them (the haze already supports it): **from any spot on the main routes, something worth a look is in sight.**

### 7.4 Sea events
Encounters chosen by region, weather and time of day so the open water is never empty; on the main routes roughly every three
minutes something small happens, and every ten minutes something worth a detour.

| Event | What happens | Gives |
|---|---|---|
| Flotsam | Driftwood, crates and floats drift by; hook them from the deck | Materials, sometimes a bottle |
| Message in a bottle | A note: a rumour, a treasure map, a page | Secrets |
| Birds diving | A shoal under them | Fish |
| A castaway on wreckage | Pick them up, bring them to the Hollowkeel | They move aboard; a reward |
| A wreck on a reef | Dive or winch | Salvage, sometimes a story |
| Whales, dolphins, turtles | They come close; dolphins ride the bow wave | Delight; a sighting for a job |
| Squall line, waterspout | Weather to face or to run from | Tension |
| Fog bank | Navigation by bell and compass | Tension |
| Becalmed | The Flat sea state (D27): row with the oars or wait | A quiet moment |
| A trader's boat | An NPC boat to hail; trade at sea, rumours | Goods, rumours |
| A ghost light | A turquoise light under the water at night | A Tidewright secret |
| The Grey's shadow | Night in deep water | Fear |

### 7.5 Jobs (the Tally House)
Six slips on the board at a time, hand-written from templates with hand-picked places:
- **Bring:** goods from the sea (driftwood, floats, fish).
- **Deliver:** cargo or a passenger to another harbour (later in the cutter's hold).
- **Survey:** sail along a named coast and chart it.
- **Sighting:** find and log something with the spyglass (the white whale, a smoke, a wreck).
- **Salvage:** recover a named crate from a wreck.
- **Search:** find a missing boat or person near a place, and bring them home.

**Rules:** no job repeats a trip; most jobs send the player somewhere new or under new conditions (at night, in a rough sea); many
hide a small turn or a secret on the way (a delivery to a hermit who turns out to have known the Cartographer).

### 7.6 Treasure, wrecks, fishing
- **Treasure maps** drawn by hand from landmarks ("where the Spire's shadow falls at dusk"), dug up with the shovel.
- **Wrecks from many ages** (the Gale has caught ships for centuries): a different hull style each, a reason for different art.
- **Fishing:** a short calm activity (line, bite, haul) with a handful of species by region and time; fish for salt, jobs and the
  Cook. A pause between tense voyages, never a grind.

---

## 8. Hooks: why the player keeps playing

### 8.1 The first forty minutes, beat by beat
| Time | Beat | Teaches |
|---|---|---|
| 0:00 | Black screen: storm, splintering wood, a bell (sound only). The intro's knot. | |
| 0:01 | Dawn on the cove's beach among wreckage. Driftwood, a rope coil, a glass float lie about. | Walking, pickups, the satchel |
| 0:04 | Smoke at the Harbour Keeper's hut. "Another one from the Gale. Nobody leaves; you get used to it." The old boat at the pier: "The Cartographer's. Make her sail and she's yours." | Talking |
| 0:06 | Mend the torn sail (rope, driftwood). E at the sea chest: the log. Its first page: "The light on the Spire burns again. Something is waking." | The boat's stations |
| 0:10 | Cast off, set the sail, steer round the lagoon and out of the channel. | Sailing |
| 0:15 | The Spire in sight. The reef round it: anchor and swim, or find the gap. | Anchoring, swimming, ladders |
| 0:20 | Up the broken tower as dusk falls; the band glows turquoise. The first glyph, the first rubbing. Landfall. | Climbing, the ancient |
| 0:30 | Night sail home with the lantern; the lagoon's lights. | Night |
| 0:35 | Off the cove, in the haze, a ship the size of a town where there was nothing. | **The hook** |
| 0:40 | Into her through the hidden side doors (D64), moor, walk her decks. Four lit places, many barred doors. | The hub |

The player is on the water within ten minutes, sees the first ancient light within twenty and the Hollowkeel within forty.

### 8.2 Always an open question
At any moment the player has at least three things they want to know or reach, and can see at least one of them: the dark storm
in the south, a glow on the horizon, a barred door aboard the Hollowkeel, a glyph not yet understood, a missing page, an empty
socket in the Keelgate. **The Keelgate's seven sockets are the main progress bar, made of stone.**

### 8.3 The rhythm: tension and the warm harbour
Calm sailing → a tell on the horizon → danger → the lights of a harbour. The music follows (chill, dark, drama), and so does the
art direction (cool world, warm home). Every voyage ends in safety, with a save, pay and a lantern-lit place: the relief is the
reward, and the reason to sail out again tomorrow.

### 8.4 Something new every hour
| Hour | New |
|---|---|
| 0-1 | Sailing, the log, the Spire, the Hollowkeel |
| 1-2 | Jobs and salt, upgrades, the shard, the first squall and leak, repairs |
| 2-3 | Ruin Isle: diving, the first guardian, glyphs; sharks |
| 3-4 | The reinforced hull, the Outer Isles, the first big site, fog or cliffs |
| 4-5 | The second harbour, the Lantern Market, night in deep water, **the Grey** |
| 5-6 | **The cutter**: the lower deck, the map table, sleeping aboard, deliveries |
| 6-8 | The other big sites (any order), the ancient lens, the currents, guilds, your cabin |
| 8-9 | **The midpoint truth**: the Cartographer took the stone |
| 9-10 | The storm-proof keel; the Storm Waters fall back |
| 10-12 | **The storm crossing**, the eye, the Cartographer |
| 12-13 | The Keelgate opens, the Deep, the choice |

### 8.5 Ending a session
A voyage always closes at a mooring with a save (D46), and before the player quits the next goal is already in sight: the pause
menu's "where you are" card and the log name it, and the loading screen greets them with a line from the Cartographer's log.

---

## 9. Does this fit what we want? (self-check)

| Check | How the concept meets it |
|---|---|
| The README's premise: atmospheric first-person sailing and exploration; a mysterious sea of islands, ruins and a lost civilisation; the boat is transport, home, storage and workspace and grows | Unchanged and made concrete: the Tidewrights, the Gale, the Tidestones; the boat carries stores, repairs, upgrades and trophies |
| The owner's four answers | Light tool-based danger (section 5), a hand-made sea in rings with a story and an ending (3, 7), only boat and sea pressure (5.4), about 13 hours (8.4) |
| Simple sailing (D41, D65) | Kept: depth from weather, sea and route, not from controls |
| Charts belong to the boats (D57) | Kept: the mast chart and the map table; nothing in the satchel |
| Warm home, cool world; turquoise only for the ancients | The game's rhythm and its story colour |
| Everything already built has a job | Section 1 |
| The owner's taste for objects with a purpose (D53) | Every tool and upgrade is a physical thing on the boat or in the hand, with a use beyond danger |
| A hobby project's scope | Two boats, seven sites, three harbours, about fifteen islets, nine threats, about twenty people (most of them minor); NPCs mostly behind counters |

**Risks of boredom, and the answer to each:**
| Risk | Answer |
|---|---|
| An empty sea between places | Sea events (7.4), the sight rule (7.3), shorter legs (3.2), currents later |
| Jobs become a chore | No repeats, new conditions, turns hidden in them (7.5) |
| Navigation without markers frustrates | Sailor's directions, the shard's pull, beacons, the chart filling in, an optional bearing pin |
| Too little tension | The Grey, squalls, leaks; cargo at risk; night content |
| Too much tension | Every threat has a tell and an answer; nothing is lost for good; harbours are safe |
| The middle drags | The midpoint truth at hour 8-9, the cutter at hour 5, a new thing every hour (8.4) |
| Backtracking | The Hollowkeel waits in the harbour you sail to; currents; the cutter's bunk saves at anchor |

---

## 10. Production

### 10.1 New systems
| Need | System |
|---|---|
| **Must** | A running day and night (the lighting rig has presets only); sleeping. People and their lines (parchment scraps). The log screen. Quests and jobs (data-driven like the stats tables). Shops and spending salt. Boat damage: holes, bilge water, patching, bailing, a torn sail. Hurt, faint and recovery; the dropped satchel; wreck, towing and the floating stores. Weather events (squalls) on the existing weather layers. Sea events. Creatures (sharks, rams, the Grey) with simple behaviour. The second boat (D51). The Gale as the world's edge. Music by mood and the first sounds. Unlocking the hub's zones and building their insides. More islands, two harbours, the sites. |
| **Should** | Glyphs and the lexicon. The shard's pull. The map table. Flares, the harpoon, the winch, the lead line, the grapnel. Beacons. Fishing. Treasure maps. Cosmetics. Rescued people aboard. |
| **Could** | Guilds. The ancient currents. Longshoal's tide machine. A rowing tender. The bearing-pin assist. The barometer. |

### 10.2 Build order: one playable hour first
Build the first hour completely before any more content, so the owner can play the loop and judge the fun.

1. **M6, the first hour (vertical slice):** the prologue (beach, the Harbour Keeper, the mended sail, the log in the chest, the
   Spire site, the first landfall, the Hollowkeel in the haze); the day and night; people v1 (the Keeper, the Purser, the
   Shipwright, the Chandler, the Reader); the log screen v1; jobs v1 (bring, deliver, survey); salt spent on 3 upgrades and the
   repair kit; boat damage v1 and the squall; music by mood. **Owner plays it.**
2. **M7, Act I complete:** Ruin Isle's site, the shard, glyphs, sharks and diving, the first Tidestones in the Keelgate, the Keel
   Walk and the Mess, five sea events, four islets.
3. **M8, the Outer Isles (1):** the reinforced hull, one big site, Longshoal's harbour, night and the Grey, flares and the harpoon.
4. **M9, the cutter and the rest of Act II:** the cutter and its map table, the other two sites, Mistholm's harbour, the zones of
   Act II, the midpoint.
5. **M10, Act III and the ending.**
6. **M11, polish:** sound, balance (prices, damage, event rates), accessibility (Next steps 8), a full playthrough.

Each milestone ends with the owner playing it; the design of the next one changes with what they feel.

### 10.3 Content in numbers
| | Count |
|---|---|
| Ancient sites | 7 (2 short, 3 long, the storm's eye, the Deep) |
| Harbours | 3 |
| Islets | 12-15 |
| Threats | 9 |
| Sea events | about 12 kinds |
| Jobs | 6 templates, about 35 hand-placed jobs |
| Log pages | about 20 |
| Tidewright words | about 30 |
| Boats and upgrades | 2 boats, about 12 upgrades, cosmetics |
| Tools | 13 (3 exist) |
| Hub zones to open | 18 |
| People | 5 main, about 15 minor |

---

## 11. Inspirations

| Game | What we take |
|---|---|
| Sea of Thieves | The look; hands-on ship care (patching, bailing); treasure maps and riddles; the joy of a ship's bell |
| Dredge | Day calm, night dread; a creature you avoid, not fight; upgrades that open waters; cargo at risk; a compact mystery |
| Outer Wilds | Knowledge as progress; a ship's log that collects clues; curiosity instead of quest markers |
| The Legend of Zelda: The Wind Waker | A sea chart filled in by sailing; an island with one secret everywhere; sailing as delight |
| Subnautica | Fear of the deep; a presence under you; tools that open depth |
| Sailwind | Navigating by the sea and the compass, not by markers |
| Chants of Sennaar, Tunic | Learning an unknown script from its context |
| Animal Crossing, Hades | A home that visibly grows; people who move in |
| Suikoden, Death Stranding | People you help come to your home and stay |
| A Short Hike | A small hand-made world, warm and kind |

---

## 12. Questions for the owner (recommendation first)

1. **Story direction:** the shelter and the Cartographer as in 3.3 · your own twist · a lighter story without a twist
   (discovery only).
2. **The player:** a silent castaway · a castaway with a few written replies · someone born in the Cove.
3. **The Cartographer at the end:** alive in the storm's eye · only their last log and their wreck.
4. **The ending:** one choice with two short final scenes · one fixed ending.
5. **People:** stylised figures mostly behind counters, text on parchment, murmurs instead of voices · full figures with voices.
6. **The Grey:** a real night horror (never fought) · milder (sharks and rams only).
7. **Tides:** only Longshoal's ancient machine · tides across the whole sea · none.
8. **Distances:** move the Outer Isles closer (no main leg over about 12 minutes) · keep them and lean on events and currents.
9. **Names:** keep Tidewrights, Tidestones, the Gale, the Grey, the Saltbound, or give your own; and replace the Sea of Thieves
   region names in the code with ours (Inner Waters, the Outer Isles, the Mistreach, the Storm Waters).
10. **Navigation help:** no markers, an optional bearing pin in Settings · always show the goal.
11. **Fishing:** a small calm activity · leave it out.
12. **Next step:** the M6 vertical slice (10.2) · first more design (the log screen, the people, the jobs) on paper.
