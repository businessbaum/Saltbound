# Saltbound: the game concept (proposal, revision 2)

Owner (2026-10-06): develop a concept for what the game is about and what the player can do. "Besides a good story it is above
all about an exciting experience that does not get boring." Inspiration from other games is allowed; it can include a combat
system, a story system, a progress system. Start with the game concept, then: what should there be, what can the player do, how
do they progress, how do we hook them?

**The owner's answers, round 1 (before writing, 2026-10-06):**

| Question | Answer |
|---|---|
| Combat | **Light and tool-based.** The sea and its creatures are the danger; you answer with tools and seamanship. No human enemies. |
| Structure | **A hand-made open sea with a story.** It opens step by step through the boat; the main quest runs through ancient sites, in any order within a tier; it has an ending. |
| Pressure | **Boat and sea only.** No hunger or thirst. Hull damage, weather, night, and cargo lost in a wreck (salvageable). |
| Length | **About 10-15 hours.** |

**Round 2 (on revision 1, 2026-10-06):**

| Question | Answer |
|---|---|
| Story direction | **Yes:** the Gale as a shelter, the Cartographer who broke it. |
| The Grey | **Yes**, "a bit of horror is good", especially at night on the sea. |
| People | **Yes:** stylised figures mostly behind counters, text on parchment, murmurs instead of voices. |
| Next step | The first playable hour (M6) is good. **Also think about playable things:** digging things up, collecting things for upgrades. |
| **The ancients** | **They are the Old World: our own world about fifty years from now.** Through them the player upgrades to futuristic technology. |

Everything else is a proposal. Names (the Tidewrights, Tidestones, the Gale, the Grey, the Cartographer, material names...) are
working names. Open questions are at the end (section 12, recommendation first).

---

## 0. The concept on one page

**Pitch.** Saltbound is a first-person sailing adventure in a hand-made sea that nobody leaves. You wash ashore after a storm,
take over a vanished chart-maker's old boat and follow their log through squalls, fog and the dark of deep water to the ruins the
sailors call the Tidewrights'. The Tidewrights turn out to be us: the world of about fifty years from now, drowned and
forgotten. What you dig up, salvage and rig into your wooden boat turns her, piece by piece, into something this sea has not seen
for centuries.

**Five pillars**
1. **Sail it by hand.** Every journey is sailed. Wind, sea, weather and the boat's condition decide the route.
2. **Every horizon is a question.** From anywhere at sea something calls: a glow, smoke, birds, a wreck. Every place keeps a
   secret, and the biggest secret is who built the ruins.
3. **The sea is the opponent, and the night is its worst.** Storms, reefs, fog and a few creatures; at night a quiet dread. You
   survive by seamanship and tools, not by killing. Every danger shows itself before it strikes.
4. **Old wood, new light.** Dig, salvage and scavenge what the Old World left, then build it into your boat. Progress is
   something you find with your own hands and can see on deck.
5. **Warm home, cool world.** The art direction's rule as a game rule: voyages are cold, dark and tense; your boat and the
   Hollowkeel are warm, lit and safe. Coming home is the reward, and both homes visibly grow.

**The loop.** Pick a goal at the Hollowkeel (a job, a page of the log, a plan that needs parts, a glow on the horizon) → make
ready → sail out → meet the sea on the way → explore, dig, salvage, solve → come home → sell, build upgrades from what you
brought, learn → sail further than before.

**Progress** comes from seven things, and none of them is a level: **boats and their technology** (from rope and canvas to the
Old World's machines), **materials** (what you collect), **tools**, **knowledge** (the log, the Old World's writing, the truth),
**the chart**, **the Hollowkeel** waking deck by deck, and **salt**.

**The hook.** The player wakes on a beach and finds an old boat whose missing owner left a log and a buried cache. A glow on the
Spire calls them out. When they come back from their first island, a ship the size of a town lies in the haze where there was
nothing. After that the game always keeps an open question in view, the boat changes after every voyage, and the player slowly
realises whose ruins these are.

---

## 1. What we have, and what it becomes

The project has a strong core: the sailing, the sea and the hub work. What is missing is the game around them: a reason to sail,
danger, people, story, progress. Everything below reuses what exists.

| What exists | Today | Its role in the game |
|---|---|---|
| The 10 m double-ender (D36-D65): physics, square sail, steering oar, windlass, anchor, mooring, ladders, mast chart | Sails, moors, anchors, saves | **The Cartographer's old boat**, the player's first home. Its repairs and patched sail (D41) are the story of its last owner |
| The FFT ocean, sea states, weather, regions, haze, time-of-day presets (D25, D27, D49) | Looks and feels right | **The opponent.** Sea states and weather become events; haze makes the horizon a question. A running day and night is missing |
| Swimming, diving, the underwater look (D28) | Works | Diving to wrecks and to the Old World's drowned streets |
| Home island, Saltbound Cove (D60) | Built | **The prologue's harbour**: where the castaway wakes |
| The Spire, Ruin Isle with its gate and shard (D47, D50) | Built, first pass | **The first two Old World sites** (Act I) |
| The ruins' look: pale, seamless "stone" with faint turquoise lines (D47, D61, `MI_AncientGlow`) | The lost civilisation | **Already the Old World:** weathered near-future composite with light strips. The twist needs no new style |
| Mistholm, Gull Rock, Longshoal (horizon silhouettes), the Storm waters (D30) | Names and shapes | **The Outer Isles** (Act II) and **the wound in the Gale** (Act III) |
| Satchel, stores, pickups, five items (D50, D52) | Items have no use | **Materials:** driftwood and rope build and repair; finds are sold and delivered; the shard is the Old World's key |
| Compass, spyglass, lantern, the tool wheel (D55) | Held, no purpose | Navigation, sightings, the lantern's trade-off at night; the wheel takes the new tools |
| Landfall (D55), the chart on the mast (D57) | Works | **Charting** as progress: the chart only shows what you have found |
| The bell, the boat hook, oars, sea chest on deck (D52, D53) | Props | The bell answers fog and frightens rams; the hook grabs flotsam; the oars row out of a calm; the chest holds the log |
| The hub ship Hollowkeel (D61-D64): Slip, Tally House, Keelgate, Chandlery, 18 locked zones, salt purse, follow rule, the fitting berth | Built, no function | **The town that wakes.** Every big step opens a zone with a new person and a new thing to do. Her hidden keel is an Old World ark |
| Save and voyages, menus, the "From an old log" loading lines (D46, D66) | Works | The log lines become the Cartographer's real words |
| The logo: a compass rose with the shard as north, in a rope ring tied off with a knot | The game's sign | **The story in one picture:** an Old World key leading a sail-age sailor; the knot is being bound to the sea |
| `Audio/Music/Background/chill`, `dark`, `drama` | Folders | Music by mood: chill in harbour and fair weather, dark at night and in the ruins, drama in storms and when the Grey comes |
| `Docs/Ideas.md` (fishing, treasure maps, wrecks, winch, diving bell, workbench, harbours, puzzles, weather, journal) | Ideas | Nearly all are placed in this concept (sections 6 and 7) |

---

## 2. The game

### 2.1 Genre and feel
A first-person **sailing adventure** with **exploration and mystery** at its heart, **hands-on gathering and building**, a
**measured night horror** and a **growing home**. The sailing stays simple to control (D41, D65: set the sail, trim it with W / S,
steer). Its depth comes from the sea, the weather and the route, not from more ropes to pull.

### 2.2 What Saltbound is not
- No human enemies, no ship-to-ship cannon fights, no multiplayer.
- No hunger, thirst or health bars, no levels or experience points, no grinding.
- No job that sends you on the same trip twice; no material you must farm in the same spot.
- **No technology that makes sailing pointless.** The Old World's machines need power, do one thing each, and draw the Grey at
  night (6.1).
- No markers floating in the world. Directions come in sailor's words ("two cables south of Gull Rock, where the reef breaks"),
  on the chart and by the compass. An optional assist in Settings shows a bearing pin for players who want it.
- No procedural world. Every island, wreck and dig spot is placed by hand.
- No gore and no cheap jump scares: the horror is dread, sound and darkness.

---

## 3. World and story

### 3.1 The premise (what the player is told)
- **The sea.** An archipelago ringed by **the Gale**, a storm wall that never ends. Ships from outside are caught in it; the Gale
  breaks them and leaves the survivors on its shores. Nobody has ever sailed out. The people here are those castaways and their
  children, from many coasts and generations. They call themselves **the Saltbound**.
- **The Tidewrights.** The Saltbound's name for whoever built in pale seamless "stone" with turquoise lines: the Spire's tower,
  Ruin Isle's gate, the keel under the Hollowkeel. Sailors' tales make them giants who raised the tides. The Saltbound keep their
  shards as luck charms and do not understand them.
- **The Hollowkeel.** The town-ship that always lies in the harbour where she is needed; nobody knows how she moves (she is never
  seen moving, D63). Her great wheel is lashed. Under generations of timber (D64) her keel is Tidewright work, and at the foot of
  her Well lies the **Keelgate**, a sealed ring with **seven empty sockets**.
- **The player.** The newest castaway, washed ashore in Saltbound Cove at dawn. Silent and nameless (recommended).
- **The Cartographer.** A castaway like the player, years ago: the best sailor and chart-maker this sea has known, who never
  stopped wanting to go home. Seven years ago they sailed south into the Storm Waters and did not come back. Their old boat still
  lies at the cove's pier; its sea chest holds their log.

### 3.2 The shape of the sea
The sea opens in rings, and the boat decides how far the player can go.

| Ring | Where | Sea | Holds | Opened by |
|---|---|---|---|---|
| **Inner Waters** | about 2.5 km round the cove | Calm to Moderate, reefs, a few islets | Saltbound Cove, the Spire, Ruin Isle, 4-5 islets | the start |
| **The Outer Isles** | 2.5-6 km | Moderate to Rough, squalls, deep water | Mistholm (in the fog of **the Mistreach**), Gull Rock, Longshoal; two more harbours; 6-8 islets | the reinforced hull |
| **The Storm Waters** | south, spreading | Storm | the eye of the storm, the broken southern station | five stations restored and the stone-bound keel |
| **The Gale** | the sea's edge, about 7 km out | beyond Storm | nothing: the end of the world | never (until the ending) |
| **The Deep** | under the Hollowkeel | dry, Old World | the ark's heart | the Keelgate |

- **The Gale is the world's edge, inside the story.** There is no invisible wall: the sea grows worse until the boat is wrecked
  and the player wakes on the nearest shore, like every castaway before them.
- **The Storm Waters are always on the southern horizon** (they are already 3 km south of the cove, D30): a dark wall the player
  sees from the first hour and cannot enter. They are a promise and a threat at once.
- **Distances.** At 8 kn (the first boat, D59) a kilometre takes about four minutes. No leg of the main path should be longer than
  about 12 minutes without something on the way, so the Outer Isles move closer than their placeholder silhouettes (question 11).
- **Region names.** The code still uses Sea of Thieves' names as placeholders (`ShoresOfPlenty`, `AncientIsles`, `Wilds`,
  `DevilsRoar`, D25). They should become ours: Inner Waters, the Outer Isles, the Mistreach, the Storm Waters (question 12).

### 3.3 The truth: the Old World
- **The Tidewrights were us.** They were the people of our own world about fifty years from now (the 2070s). The pale "stone"
  is their composite and concrete; the turquoise lines are their lights; the "glyphs" are their worn signs and writing; the
  islands are the hilltops, towers and roofs of their drowned coast.
- **The Gale was their last great work.** As the seas rose and the storms grew, they ringed one sea with weather engines that
  hold a wall of storm round it: a shelter where life could go on until the world outside healed. Six stations anchor it. The
  **Tidestones** are their cores.
- **The Hollowkeel was their ark.** Her autopilot still follows its last order, "be where the survivors gather". That is why she
  is always already in the harbour you sail to, and why she only moves when nobody is near.
- **The game plays centuries later** (the gap is question 1). The Old World has become myth; its things are "Tidewright stone"
  and luck charms; people sail wooden boats again. Outside the Gale people live a hard sail-age life on a wild sea. The Gale still
  rescues their wrecked crews, but it lets nobody out: it was never opened.
- **The Cartographer came from outside** and wanted to go home. They learned to read the Old World's writing, found the southern
  station and took its core, hoping the Gale would open. It tore inward instead. The Storm Waters are the wound, and they grow.
  The Cartographer is trapped in its eye.
- **The shard is the ark's key.** The glowing shard from Ruin Isle leads the player like a compass needle all game long (the
  logo's north point). It is the key the Keelgate waits for, the seventh "stone".
- **The Grey** is the Old World's too (section 5.5): a warden machine the size of a whale, overgrown until it looks like an animal,
  running wild since the southern core was taken.
- **The ending.** With six stations restored and the shard set, the Keelgate opens. In the Deep the ark's heart wakes, with the
  last message of her crew, and her lashed wheel turns. The player chooses: **open the Gale and sail her out** (the sheltered sea
  joins the world again; the Cartographer goes home), or **renew it and stay** (the shelter holds; Saltbound by choice). Each
  choice has its own short final scene; afterwards the sea stays open to sail.

The emotional line: the player follows a stranger's footsteps, comes to admire them, finds out that the ruins are our own future
and that the stranger's longing broke the world, and finally has to decide whether that longing was right. The climate behind
the story is told through people's letters and places, never as a lecture.

### 3.4 How the truth comes out: let the player suspect first
Players will recognise our world before the story says it. That is the point: suspicion is a hook. The game feeds it step by step.

| When | What the player finds | What they can guess |
|---|---|---|
| Prologue | An odd pale thing in the Cartographer's cache: a light, hollow, perfectly round float with a moulded number | These things were made |
| Act I | Bolts with threads, a hatch with a gasket, numbers and arrows on panels; the Spire's top is an instrument mast | The Tidewrights were engineers, not giants |
| Act I end | The first worn word in letters, half legible | The glyphs are writing |
| Act II, early | Drowned streets and roofs under Longshoal; a road sign under water | The islands were a coast |
| Act II, about hour 5-6 | The first time capsule dug up: a letter dated 2075, in plain words ("To whoever finds this...") | **They were us** |
| Midpoint, hour 8-9 | The Cartographer's last pages | They broke the Gale |
| Act III | The ark's recorded message in the Deep | Why the Gale exists; the choice |

**Old World writing** (question 5): worn letters and pictograms early, which the player learns to read through the log; plain
text only from the time capsules on. Saltbound names for Old World things change in the log when the player learns what they are
(a "spark jar" becomes "a battery cell, made 2074").

### 3.5 The story in three acts
| Part | Hours | Where | What happens | Technology |
|---|---|---|---|---|
| **Prologue: Ashore** | 0:00-0:40 | Saltbound Cove, the Spire | Waking on the beach; the Harbour Keeper; the old boat, its log and the buried cache; mending the sail; first sail; the Spire's glow at dusk; the first landfall. Back in the cove the Hollowkeel lies in the haze | Wood, rope, canvas |
| **Act I: The Inner Waters** | 0:40-3:30 | Hollowkeel, the Spire, Ruin Isle, islets | The four people aboard; jobs, salt, the first materials and upgrades; the shard and its pull; the first two Tidestones; the first squall, leak and repair; the first eerie night; the Reader: "seven stones hold the Gale". The log points to the Outer Isles; the boat is not strong enough yet | Sail-age upgrades; the first Old World scraps |
| **Act II: The Outer Isles** | 3:30-10:00 | Mistholm, Gull Rock, Longshoal, two harbours | The reinforced hull, the first Old World upgrades, the cutter. Three big sites in any order; the drowned town; the first time capsule (**they were us**). Night in deep water and the Grey. **Midpoint:** the Cartographer took the southern core. Each station restored pushes the storm's edge back | Salvaged Old World machines rigged into wood |
| **Act III: The Storm Waters and the Deep** | 10:00-13:00 | The Storm Waters, the eye, the Keelgate | The storm crossing (the big seamanship test); the Cartographer in the eye; the sixth core; the Keelgate opens; the Deep; the wheel turns; the choice | Restored Old World technology |
| **After** | open | everywhere | Free sailing, the rest of the secrets and dig sites, every zone of the Hollowkeel | |

### 3.6 How the story is told
- **The log** (a new screen in the house style: parchment, ink, the satchel's leather): the Cartographer's pages as they are found
  (about 20), the player's own entries written automatically (places, people, clues, jobs), rubbings of Old World writing, what
  each strange thing is, and the plans the player owns.
- **Places tell it:** wrecks from many generations, the Cartographer's old camps (a cold fire ring, a chart pinned to a rock, a
  buried cache), the Old World's rooms with their lockers, notices and personal things.
- **Time capsules:** about ten, buried by Old World people in 2074-2076 and dug up by the player: a letter, a photo, one object. The
  human side of the Old World.
- **The radio** (an Old World upgrade): automatic broadcasts still looping after centuries, clearer at night: a weather report, a
  list of names, a calm voice reading evacuation routes. Story and horror in one.
- **People** speak in short lines on parchment scraps (the prompt's style), with murmurs instead of voices.
- **The Reader** at the Keelgate is the main quest's voice: they read what the player brings and set the next goal.
- **Short camera glides** for big moments (like the chart glide of D58), no long cutscenes.
- **The loading screen's old-log lines** become the Cartographer's real words, chosen from pages already found.

### 3.7 People
The owner designs names and looks. Presentation (owner, round 2): chunky stylised figures in the art direction's spirit, mostly
behind counters, hatches and desks, few animations, text on parchment, murmurs instead of voices.

| Where | Role | What they do |
|---|---|---|
| Saltbound Cove | **the Harbour Keeper** | The first face. Gives the boat ("Make her sail and she's yours") and a shovel, the first small jobs |
| The Slip | **the Shipwright** | Sail-age upgrades from your materials, repairs, the second boat, towing a wrecked boat home |
| The Tally House | **the Purser** | Jobs and pay; buys finds once the Lantern Market is open |
| The Keelgate | **the Reader** | The main quest, the Old World's writing |
| The Chandlery | **the Chandler** | Tools and consumables |
| L18 Sail lofts and workshops | **the Tinker** | Rigs Old World technology into wooden boats from the plans and parts you bring (opens in Act II) |
| Zones as they open | the Cook, the Archivist, the Ropemaker, the Sailmaker, traders, guild masters, the Innkeeper, the Lookout | Each brings a mechanic (section 6.7) |
| Harbours 2 and 3 | a few each | Local jobs, rumours, a local craft |
| At sea | castaways you rescue, a trader's boat, hermits on islets, the Cartographer | Rescued people move aboard the Hollowkeel and can be met there |

---

## 4. What the player does

### 4.1 The verbs
| Verb | How | Exists? |
|---|---|---|
| **Sail** | Set and trim the sail, steer, anchor, moor, row out of a calm (the oars); later the motor for short runs | Yes (rowing and motor new) |
| **Navigate** | Compass, spyglass, the mast chart, landmarks, the shard's pull; later the lead line, the map table, beacons, sonar | Partly |
| **Explore on foot** | Walk, jump, climb ladders; later the grapnel for cliffs | Partly |
| **Swim and dive** | As now; a breath limit, the diving helmet and later a rebreather are new | Partly |
| **Collect** | Pickups on beaches (refilled by storms); flotsam hooked from the deck with the boat hook; fishing | Pickups only |
| **Dig** | Find the signs of something buried, dig it up with the shovel; later the detector finds Old World metal | New |
| **Scavenge** | Search Old World rooms: lockers, crates and panels pried open with the crowbar, by lantern light | New |
| **Salvage** | Dive to wrecks for small things, raise heavy ones with the winch | New |
| **Build** | Bring plans, materials and salt to the Shipwright or the Tinker; make small things at a workbench | New |
| **Keep the boat** | Patch holes, bail, mend the sail, carry the right sail in a blow, keep the cells charged | New |
| **Read and decode** | Take rubbings of Old World writing, work out what it means, read the log | New |
| **Answer danger** | The bell, the lantern, the boat hook, flares, the harpoon, the searchlight, the lens | Bell and lantern only |
| **Trade** | Earn and spend salt at the services and the Lantern Market | Spaces only |
| **Help people** | Jobs, rescues, deliveries | New |
| **Make a home** | The boat's look and trophies; a cabin aboard the Hollowkeel | New |
| **Rest** | Sleep in a bunk to dawn or dusk (on the cutter, at the Inn) | New |

### 4.2 Three loops
1. **The moment (seconds):** read the wind and the water, steer, trim; spot something through the haze or the spyglass; decide
   whether to look closer.
2. **The voyage (20-40 minutes):** choose a goal (a job, a log page, a plan that needs parts, a rumour, a glow) → make ready
   (upgrades, flares, planks, charged cells, the right weather and time of day) → sail out → the sea happens on the way (7.4) →
   arrive, explore, dig, scavenge, solve → choose what to carry home (space is short) → come home, or push on at a risk → sell,
   build, save at the mooring, learn. **Every voyage ends in a warm harbour.**
3. **The campaign (hours):** restore the stations, open the sea ring by ring, turn the boat from wood to wood and light, wake the
   Hollowkeel zone by zone, follow the Cartographer, learn who the Tidewrights were.

---

## 5. The sea fights back: danger, the light combat system, the night

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
| **Reefs and rocks** | Shallows | Pale turquoise water, breakers; the lead line, later sonar | Steer, slow down, sound, anchor | Holes in the hull | Act I |
| **Night** | Everywhere | Dusk | Lantern, searchlight, beacons you have relit, the stars | Reefs unseen; the Grey | Act I |
| **Reef sharks** | Reefs, while swimming or diving | Fins, shadows circling | Leave the water, a flare under water, the lantern | A bite (hurt) | Act I |
| **Fog** | The Mistreach, calm dawns | The haze closes in | Compass, bell buoys, ring your own bell and listen, go slow, sonar | Lost, aground | Act II |
| **Rams** | Gull Rock's waters at dusk | Wakes racing in | Steer away, the harpoon stings them, the bell scares them | Holes in the hull | Act II |
| **The Grey** | Deep water at night | Section 5.5 | Darkness and silence, a decoy, the shallows | A heavy blow: big holes, a knockdown | Act II |
| **Wardens** (small Old World drones) | The Old World's sites | A turquoise eye opens, a hum | Stay out of their light, break their line of sight, cut their power, turn them with the lens | Thrown out to the room's start | Act I-III |
| **The storm** | The Storm Waters | The whole southern horizon | Everything at once: carry little sail, steer the waves, patch, bail | Wreck | Act III |

### 5.3 The tools you answer with ("combat")
Nothing has a health bar. Creatures and machines that are driven off leave. Every tool also has a use outside danger.

| Tool | Against | Also for | Where from |
|---|---|---|---|
| **Bell** (on the boat) | Rams; fog (echoes from rocks) | Calling at a harbour | Exists |
| **Lantern** | Night, reefs, sharks | Caves, ruins; but it draws the Grey | Exists |
| **Boat hook** | Pushes off a creature, the boat off a rock | Hooks flotsam from the deck | Exists as a prop |
| **Flares** (consumable) | Sharks, the Grey (a decoy far off) | Light over a wide area, signals | The Chandlery, the workbench |
| **Bow harpoon**, later the **stinger** (an electric tip) | Rams; the stinger makes the Grey dive | A line to salvage or to a rock | The Shipwright, then the Tinker |
| **Searchlight** (Old World) | Night, fog, wardens (blinds them) | Long-range light; but it draws the Grey hardest | The Tinker |
| **The lens** (Old World spectacles) | Wardens (turns them friendly) | Reads Old World writing, shows hidden marks | A site in Act II |

### 5.4 Damage, failure and recovery
- **The hull:** holes where it is hit (seen and heard: water spurts in), water in the bilge (the boat sits lower, slower, rolls
  more). Full, she founders. **Patch** a hole: hold E with a plank and the repair kit. **Bail** with a bucket (it returns with a
  purpose, D52); later a hand pump, then an electric one.
- **The sail:** too much sail in a squall tears it (less drive until it is mended with a sail patch or at the Slip). This makes the
  existing trim (W / S at the helm, D59) a real decision.
- **Power:** Old World machines run on cells that charge by day and run down at night (6.1). An empty cell at the wrong moment is
  a threat of its own.
- **The player:** no health bar. Hurt shows as a red-ink vignette that fades; hurt again soon, they faint. Under water a breath limit.
- **A faint:** wake aboard your boat if it is near, otherwise at the nearest harbour. The satchel stays where you fell (a bundle on
  the shore or a floating sack, marked on the chart) until you pick it up.
- **A wreck:** wake at the nearest harbour. The Shipwright has your boat towed home (a salt fee; the first time is free). What was
  in her stores floats at the wreck under a buoy for a day, then sinks (salvage it with the winch or by diving).
- **Never lost:** tools, boats, upgrades, plans, knowledge, the salt purse. No permanent death.

### 5.5 The night: a bit of horror
Owner (round 2): "a bit of horror is good", especially at night on the sea. The day is for exploring; the night is for dread.

**Principles**
- **Dread, not shock.** Long build-ups, rare payoffs. A night can pass with nothing happening, and the player should not know in
  advance whether it will.
- **Sound first.** The horror lives in the audio (the `dark` and `drama` folders): the sea going quiet, a knock against the hull,
  a low tone through the planks, the radio's hiss.
- **Light is safety and danger.** The lantern shows the reefs and draws the Grey. Every Old World light and machine draws it more.
- **The harbour is always safe.** Its lanterns are the relief. The horror never follows the player inside.
- **It grows with the acts.** Act I nights are only eerie: a light under the water that goes out, a derelict with no crew, sharks.
  The Grey appears in Act II, in deep water only. Act III has the storm.

**The Grey**
- **What it is** (question 2): an Old World warden machine the size of a whale, built to keep the sheltered sea clear. Centuries of
  barnacles and weed make it look like an animal; the player should believe it is one until late in Act II. Since the southern
  core was taken it runs wild: at night it hunts light, noise and anything that carries current.
- **Its tells, in order:** the sea goes flat and the birds fall silent; the lantern flickers (its field); a low tone through the
  hull; on sonar, a blip that is too big; in bioluminescent water, its outline; a shadow passing under the keel.
- **The answers:** douse every light and switch off every machine, take in the sail and drift; throw a flare far off as a decoy;
  run for the shallows (it cannot follow over reefs); the stinger makes it dive for a while.
- **Never fought, never killed.** In Act III, with the Gale restored (or in the ending), it calms and its eye turns from white to
  turquoise: it becomes a warden again.
- **Tech and fear are tied:** the searchlight, the motor and the sonar's ping make the night more dangerous. Every Old World upgrade
  is also a choice about the dark.

**Other night things:** old broadcasts on the radio; a derelict whose Old World light still blinks; knocking on the hull from
below (a drifting log... or not); fog with shapes in it; bioluminescent bays that are pure beauty, so that night is also a reward.

---

## 6. Progression

### 6.1 Old wood, new light: three tiers of technology
The boat changes from a sail-age boat into a hybrid: Old World machines lashed and bolted into wooden mounts with rope and iron.
Turquoise stays reserved for the Old World (UI_Design); on the boat it now means power.

| Tier | Look | Examples | Built by, from |
|---|---|---|---|
| **Sail-age** | The boat as now: wood, rope, canvas, iron | A new sail, reef points, tarred seams, copper sheathing, ribs and knees (the reinforced hull), the winch, the bow harpoon, a hand pump, a locker, a barometer, a stern lantern | The Shipwright: timber, rope, canvas, iron, tar and salt |
| **Salvaged Old World** | Pale parts with turquoise light, rigged into wood | Cells and solar film (power), the searchlight, the radio, the sonar, the electric pump, the motor, the stinger, the metal detector, the rebreather, composite hull plates | The Tinker: an Old World plan, Old World parts and salt |
| **Restored Old World** | The clean pale technology of the ark | The stone-bound keel (storm-proof), the gyro (a calm deck in any sea), the lens, the current drive (rides the Old World's current lanes) | Gifts of the restored stations |

**Rules that keep the sailing at the centre:**
1. **Power is limited.** Old World machines run on cells. Cells charge from solar film by day and run down at night; the cells'
   glow is the gauge (no HUD bar). More cells and film are upgrades.
2. **The motor is a helper:** slow (about 3 kn), draining; for calms, harbours, tight places and escapes. The sail stays the way
   to travel.
3. **Machines are loud to the Grey** (5.5).
4. **Every upgrade adds a capability** (something new to do or reach), not just a bigger number.

### 6.2 Boats
| Boat | How you get it | Waters | What it adds |
|---|---|---|---|
| **The double-ender** (10 m, exists) | The prologue: the Cartographer's | Inner Waters; Outer Isles once reinforced | Open deck, barrel and crate, the mast chart, shallow draught (1.45 m) |
| **The cutter** (14 m, D51) | About hour 5: the boat on the Slip (already built in the hub), through the Shipwright's line, materials and salt | Outer Isles, Rough seas | Lower deck with a bunk (sleep and save at anchor anywhere), a stove, a workbench, **the map table** (chart stage 2, D57), a hold for big finds and deliveries; faster, points higher; deeper draught (2.05 m) |
| **The Hollowkeel** | The ending | Through the Gale | Her wheel turns |

- **Both boats stay useful:** the double-ender at a fleet berth (they exist for this, D63) is the boat for Longshoal's shallows and
  the reefs; the cutter is for open water, long trips and heavy salvage.
- **Upgrades show on the boat,** never only in a menu (D53: every part made for its purpose and attached properly).
- **The boat as a home:** hull paint (the old open decision 3), sail patches and colours, flags and pennants, a figurehead,
  trophies from sites and digs on deck (a road sign, a warden's eye, a lucky float).

### 6.3 Materials: collecting, digging, salvaging, building
Owner (round 2): "you can dig stuff up or you need to collect stuff for upgrades". Upgrades cost **materials you find** plus salt,
so every trip brings home something to build with.

**Materials** (Saltbound names; the log adds the real name once the player knows it, 3.4)

| Material | What it really is | Where | For |
|---|---|---|---|
| Timber | Driftwood, planks | Beaches, wrecks, the Market | Repairs, every sail-age upgrade |
| Rope | Hemp | Beaches, wrecks, the Ropewalk | Rigging, the winch, lashing Old World parts |
| Canvas | Sailcloth | Wrecks, the Sail lofts | Sails, patches |
| Iron | Nails, fittings, chain | Wrecks, digging | Fittings, the harpoon, the winch |
| Tar | Pitch | Wreck barrels, the Market | Seams |
| Shells, floats, curios | | Beaches | Salt, jobs, trophies |
| Fish | | Fishing | Salt, jobs, the Cook |
| **Pale plate** | Composite panel | Ruins, drowned roofs | Hull plates, mounts |
| **Copper vine** | Cable | Ruins, digging | Everything electric |
| **Spark jar** | Battery cell | Ruins, time capsules, deep wrecks | Power |
| **Sunglass** | Solar film | Roofs of drowned buildings, the Mistholm station | Charging |
| **Glass leaf** | Circuit board | Ruins, capsules | Radio, sonar, lens, detector |
| **Eye-glass** | Optical lens | The sites | The searchlight, a better spyglass |
| **Hum-heart** | Electric motor | A big wreck or a site (rare) | The motor, the pumps, the winch's drive |

**Materials by ring.** Each ring yields what the next ring's upgrades need: the Inner Waters give sail-age materials and the first
Old World scraps; the Outer Isles give cable, cells, film and plates; the Storm Waters give the cores.

**Where they come from: the playable part**

| Activity | How it plays | Tool | Gives |
|---|---|---|---|
| **Beachcombing** | Walk the shore. Storms wash new things up, so a beach is worth a look after bad weather | Hands | Timber, rope, shells, floats, now and then an Old World bit |
| **Hooking flotsam** | Things drift past the boat; reach over the rail with the boat hook | Boat hook | Timber, crates, bottles with notes |
| **Digging** | See below | Shovel, later the detector | Caches, time capsules, Old World parts, treasure |
| **Scavenging** | Search Old World rooms room by room: lockers, crates, panels pried open; dark rooms need light | Crowbar, lantern | Old World parts, plans, letters |
| **Salvaging** | Dive to a wreck for small things; hook the winch to a heavy one and haul it up | Diving helmet, winch | Iron, cargo, big parts |
| **Fishing** | Line, bite, haul; a few species by region and time | Line | Fish |
| **Trading** | The Lantern Market, a trader's boat | Salt | Any common material |
| **Jobs and rescues** | Rewards | | Salt, sometimes plans |

**Digging**
- **Dig spots are placed by hand, and each has a sign:** a mound of loose sand, a stake or a cairn, a ring of stones, a gull that
  keeps landing on one spot, a line on a treasure map, a capsule's beacon heard on the radio, later the detector's beeps (faster
  when closer).
- **Digging feels physical:** hold the button for three to five strokes. Each stroke throws sand (a hole that deepens, a heap that
  grows) until the find rises out of the hole: a chest, a rusted box, a capsule, a part.
- **Never for nothing:** digging in a wrong spot still turns up a little (a shell, a nail, a coin), so the shovel always feels good.
- **What lies buried:** the Cartographer's caches (with log pages), sailors' treasure (hand-drawn maps with landmark riddles),
  Old World parts, and the **time capsules** (3.6).
- **The prologue teaches it:** the first page of the log says where the Cartographer buried their spare canvas.

**Building**
- **Every upgrade needs its plan.** Sail-age plans hang on the Shipwright's plans board. Old World plans are found (in ruins,
  capsules and at the sites) and brought to the Tinker, who works out how to rig them into a wooden boat.
- **A plan shows what it needs:** a parchment with a slot for each material (the satchel's look). Hand in the materials from the
  satchel or the boat's stores, pay the salt, and moor at **the fitting berth** (it exists, D63): the next time you come aboard
  your boat, the upgrade is on her.
- **The workbench** (the cutter's lower deck, your cabin): small things on the spot: planks and patches for repairs, flares, bait,
  a charged cell from the solar film.

**Space is the pressure.** The satchel holds 24 stacks, the barrel and the crate 10 each, the cutter's hold more. Big Old World
parts need the hold or the winch. Every trip ends with a choice of what to carry home, and a wreck puts it all at risk (5.4).

### 6.4 Tools
The tool wheel takes any number of tools (D57). The player starts with the compass, the spyglass and the lantern (Stats).

| Tool | Does | Opens | From |
|---|---|---|---|
| Compass, spyglass, lantern | As now; the spyglass also **logs sightings** for jobs (a whale, a wreck, a smoke) | | Exist |
| **Shovel** | Digs | Caches, treasure, capsules | The Harbour Keeper (prologue) |
| **The shard** | Held up to the light (its existing action) it glows brighter towards the nearest station not yet restored; later its point turns like a needle | The main path | Ruin Isle |
| **Rubbing kit** | Copies Old World writing into the log | Reading | The Reader |
| **Repair kit** | Patches holes, mends the sail | Longer voyages | The Chandlery |
| **Crowbar** | Pries open lockers, panels and hatches | Old World rooms | The Chandlery |
| **Lead line** | Sounds the depth and the bottom | Longshoal's shallows | The Chandlery |
| **Flares** | Light, signal, decoy | Nights | The Chandlery, the workbench |
| **Fishing line** | Catches fish | A calm pastime | The Cove |
| **Grapnel** | Climbs cliffs and ledges | Gull Rock | The Chandlery, after a job |
| **Diving helmet**, later the **rebreather** | Longer breath | Deep wrecks, drowned rooms | The Chandlery, then the Tinker |
| **Metal detector** (Old World) | Beeps over buried metal | Old World digs | The Tinker |
| **The lens** (Old World) | Reads Old World writing, shows hidden marks, turns wardens | Late secrets | An Act II site |

### 6.5 Knowledge: the log and the Old World's writing
- About **30 Old World words and signs**, worn on panels and walls. The first time the player sees one, a rubbing puts it in the
  log as unknown. Context teaches its meaning (the sign over a channel of water, the arrow by a stair, the red one on a sealed
  door), and the player writes their guess. When three guesses on one page are right, they are confirmed (the method of Chants of
  Sennaar). Later the lens reads everything at a glance.
- The Reader translates longer texts once enough words are known.
- **Knowledge opens doors.** Some doors and riddles at the sites ask for words, not items: a player who knows them can open them
  early (Outer Wilds). Inscriptions and notices point to caches and rooms that are on no chart.
- **Things get their real names:** the log's list of strange things fills in as the player learns (3.4).

### 6.6 The chart
- The mast chart (D57) shows **only charted water**: islands appear at landfall (D55), coasts when you sail along them; the rest
  is blank parchment (the Wind Waker sea chart).
- More of it comes from the Archivist's regional charts, survey jobs and the Cartographer's pages, whose sketch maps are inked
  onto the chart when found.
- **An Old World map** found in Act II shows the coast as it was, with its towns' names, laid under your chart: a reveal and a
  treasure map at once (where the drowned towns lie).
- **The map table** on the cutter (stage 2): the whole charted sea, your marks and notes, course lines, job pins, the shard's
  bearing; with sonar, the seabed drawn under your track.
- **Beacons:** each restored station relights an Old World beacon; at night they make whole areas safe to navigate.

### 6.7 The Hollowkeel wakes
The hub has 18 locked zones (`HubShip_Data.json`). Each opens with a story step or a side line, brings a person, a mechanic and life
(lanterns lit, voices, smoke), and **people the player rescued at sea move into it**. The town visibly fills because of the player.

| Zone | Opens with | Brings |
|---|---|---|
| L5 Keel Walk | An early Shipwright job | The shortcut from the Slip to the Keelgate (and its hoist up) |
| L9 Mess and galley | The first rescued castaway | The Cook: rumours; fish become dishes that help for a voyage (longer breath, steadier legs on deck) |
| L13 Inn and bunkrooms | The second station restored | The Innkeeper: rumours board, sleep to dawn or dusk, rescued people live here |
| L12 Archive | First words decoded | The Archivist: regional charts, help with the writing, the lore |
| L18 Sail lofts and workshops | The first Old World plan found | **The Tinker** (Old World upgrades) and the Sailmaker |
| L8 Ropewalk | Act II jobs | Rigging upgrades; rope for materials |
| L11 Lantern Market | The second harbour found | Traders: selling finds, buying materials, cosmetics |
| L10 Guild rooms | Act II | Three guilds (salvagers, fishers, surveyors), each a short line of jobs with its own reward |
| L2 Stern quarters | Act II | **Your own cabin:** rest, a workbench, trophies, half-models of your boats, the Cartographer's pages pinned up, the time capsules' letters |
| L6 Holds, L3 Stern stores | A delivery line | Your store room |
| L16 Masts and tops | Gull Rock | The Lookout: the view, sightings reported to you |
| L1 Sterncastle | Act III | The ship's council; the great wheel |
| L17 The Deep | The finale | The ark's heart |
| L4, L7, L14, L15 | Small jobs | Small rooms, shortcuts, secrets |

### 6.8 Salt
One currency (the hub design's price bands as the start: a job 40-150, gear 60-400, an upgrade 150-900 plus materials, the cutter
about 2 500 plus materials).

| Earn | Spend |
|---|---|
| Jobs, selling finds and materials (once the Lantern Market opens), fish, treasure, rescues | Upgrades (with materials), tools, consumables (flares, planks), charts, materials you lack, towing after a wreck, cosmetics, a share towards reopening a zone |

Salt is never the gate to the story: story steps need places reached and things understood, not money.

### 6.9 No levels
There are no experience points and no stats to raise. The player gets better through what they own, what they know and how well
they sail. A strong player can do some things early, by skill and knowledge alone.

---

## 7. Content: what fills the sea

### 7.1 The Old World's sites (the main quest)
Every site follows one recipe: **an approach at sea** with its own challenge, **a place to explore** on foot or under water, **one
puzzle theme** built on one tool or system, **one warden moment**, **a page of the Cartographer's log**, **plans and parts**, **the
station's core**, and **a change in the world** once it is restored. Each site was a real place with a purpose (D53's rule for
objects, for whole places).

| Site | Act | What it was | Approach | Inside | Theme | Restored, the world changes |
|---|---|---|---|---|---|---|
| **The Spire** | Prologue / I | An instrument and beacon mast of the northern station | Reefs round the stack | The tower's broken stair to the top | Light: the lantern into the band, the first writing | Its beacon burns: the Inner Waters can be sailed at night |
| **Ruin Isle** | I | The door of a pump station under the knoll | The beach shelf (D47) | The gate opens to a flooded hall | Water and diving: the first warden | The shard wakes and starts to pull |
| **Mistholm** | II | A cloud-seeding station: its fog catchers still work | The Mistreach's fog: bell buoys, compass | Terraces of collectors and mirrors | Power and time of day: route the sun's light at the right hour | The fog lifts: Mistholm's harbour opens (harbour 3) |
| **Gull Rock** | II | A wind farm on the cliffs: broken turbines howl in the wind | Cliffs, rams at dusk, sea caves only on calm days | The turbine halls inside the cliff | Wind and sound: the grapnel, the bell | A steady trade wind and the Old World's current lanes (the current drive) |
| **Longshoal** | II | A drowned town behind a flood barrier | Sandbanks and shallows (the small boat, the lead line) | The barrier's gates and pumps | Water levels: close the gates, pump the basin, get out before it floods again | The basin stays dry: **walk the Old World's drowned street** |
| **The storm's eye** | III | The southern station, the Gale's main engine | The Storm Waters | The broken engine, the Cartographer's camp | Everything learned | The storm falls back to a wall again; the Keelgate can open |
| **The Deep** | III | The ark's heart under the Hollowkeel | The Keelgate | Her reactor and her crew's last message | The writing; the shard | The wheel turns; the ending |

The Spire and Ruin Isle are short (20-30 minutes each); the three Outer Isles sites are long (45-75 minutes each, with their trips);
the storm and the Deep are the finale (one to two hours).

### 7.2 Harbours
Three harbours, each a berth for the Hollowkeel (the follow rule, D63), each with its own look and craft:
1. **Saltbound Cove:** the home lagoon (exists). The Harbour Keeper, the fishing line, the shovel.
2. **Longshoal:** a village on stilts over the shallows, built from Old World scrap and driftwood. Shallow-water jobs, the sounding
   trade, scrap traders.
3. **Mistholm:** a harbour of old pale walls under the lifted fog; the Archive's sister house. Opens with the Mistholm site.

### 7.3 Islets: one secret each
Twelve to fifteen small places, each with one thing: a hermit, a castaway's camp, a sea cave, a nesting colony, a tidal pool with a
sign, a drowned rooftop to dive, a wreck on its reef, the Cartographer's cache, a bioluminescent bay at night, a buried capsule, a
treasure. A rule for placing them (the haze already supports it): **from any spot on the main routes, something worth a look is in
sight.** About 40 dig spots are spread over beaches, islets and sites.

### 7.4 Sea events
Encounters chosen by region, weather and time of day so the open water is never empty; on the main routes roughly every three
minutes something small happens, and every ten minutes something worth a detour.

| Event | What happens | Gives |
|---|---|---|
| Flotsam | Driftwood, crates and floats drift by; hook them from the deck | Materials, sometimes a bottle |
| Message in a bottle | A note: a rumour, a treasure map, a page | Secrets |
| After a storm | Old World debris has come up from the deep and floats in a field; beaches are refilled | Old World bits |
| Birds diving | A shoal under them | Fish |
| A castaway on wreckage | Pick them up, bring them to the Hollowkeel | They move aboard; a reward |
| A wreck on a reef | Dive or winch | Salvage, sometimes a story |
| A signal on the radio | An old distress loop; follow it by its strength | A wreck, a capsule, a room |
| Whales, dolphins, turtles | They come close; dolphins ride the bow wave | Delight; a sighting for a job |
| Squall line, waterspout | Weather to face or to run from | Tension |
| Fog bank | Navigation by bell, compass and sonar | Tension |
| Becalmed | The Flat sea state (D27): row with the oars, wait, or run the motor | A quiet moment |
| A trader's boat | An NPC boat to hail; trade at sea, rumours | Goods, rumours |
| A derelict at night | A blinking Old World light, nobody aboard | Salvage and dread |
| The Grey's shadow | Night in deep water | Fear |

### 7.5 Jobs (the Tally House)
Six slips on the board at a time, hand-written from templates with hand-picked places:
- **Bring:** goods from the sea (timber, floats, fish).
- **Deliver:** cargo or a passenger to another harbour (later in the cutter's hold).
- **Survey:** sail along a named coast and chart it.
- **Sighting:** find and log something with the spyglass (the white whale, a smoke, a wreck).
- **Salvage:** recover a named crate from a wreck.
- **Dig:** a cache at a place described in sailor's words.
- **Parts:** Old World parts the Tinker or a harbour needs.
- **Search:** find a missing boat or person near a place, and bring them home.

**Rules:** no job repeats a trip; most jobs send the player somewhere new or under new conditions (at night, in a rough sea); many
hide a small turn or a secret on the way (a delivery to a hermit who turns out to have known the Cartographer).

### 7.6 Treasure maps and fishing
- **Treasure maps** drawn by hand from landmarks ("where the Spire's shadow falls at dusk, twelve paces from the split rock"),
  found in bottles, caches and jobs; dug up with the shovel.
- **Fishing:** a short calm activity (line, bite, haul) with a handful of species by region and time; fish for salt, jobs and the
  Cook. A pause between tense voyages, never a grind.

---

## 8. Hooks: why the player keeps playing

### 8.1 The first forty minutes, beat by beat
| Time | Beat | Teaches |
|---|---|---|
| 0:00 | Black screen: storm, splintering wood, a bell (sound only). The intro's knot. | |
| 0:01 | Dawn on the cove's beach among wreckage. Driftwood, a rope coil, a glass float lie about. | Walking, pickups, the satchel |
| 0:04 | Smoke at the Harbour Keeper's hut. "Another one from the Gale. Nobody leaves; you get used to it." The old boat at the pier, its sail torn: "The Cartographer's. Make her sail and she's yours." A shovel. | Talking |
| 0:06 | E at the sea chest: the log. Its first page has a sketch: "Spare canvas under the leaning palm, three paces seaward. I never trusted a store." | The boat's stations, the log |
| 0:08 | Find the palm, the loose sand; dig. Up come the canvas and an odd pale float with a moulded number. | **Digging** |
| 0:11 | Mend the sail. Cast off, set it, steer round the lagoon and out of the channel. | Repair, sailing |
| 0:16 | The Spire in sight. The reef round it: anchor and swim, or find the gap. | Anchoring, swimming, ladders |
| 0:21 | Up the broken tower as dusk falls; the band glows turquoise. Numbers, a bolt, worn letters: the first rubbing. Landfall. | Climbing, the Old World |
| 0:31 | Night sail home with the lantern. Halfway, a light under the water, there and gone. The lagoon's lanterns ahead. | Night, the first chill |
| 0:36 | Off the cove, in the haze, a ship the size of a town where there was nothing. | **The hook** |
| 0:40 | Into her through the hidden side doors (D64), moor, walk her decks. Four lit places, many barred doors. | The hub |

The player is on the water within twelve minutes, digs within ten, sees the first Old World light within twenty-five and the
Hollowkeel within forty.

### 8.2 Always an open question
At any moment the player has at least three things they want to know or reach, and can see at least one of them: the dark storm in
the south, a glow on the horizon, a barred door aboard the Hollowkeel, a plan missing two parts, an unread sign, a missing page, an
empty socket in the Keelgate, and, all game long, **who the Tidewrights were**. **The Keelgate's seven sockets are the main progress
bar, made of stone.**

### 8.3 The rhythm: tension and the warm harbour
Calm sailing by day → a tell on the horizon or the dark coming on → danger → the lights of a harbour. The music follows (chill,
dark, drama), and so does the art direction (cool world, warm home). Every voyage ends in safety, with a save, pay, a new part on
the boat and a lantern-lit place: the relief is the reward, and the reason to sail out again.

### 8.4 Something new every hour
| Hour | New |
|---|---|
| 0-1 | Sailing, digging, the log, the Spire, the Hollowkeel |
| 1-2 | Jobs and salt, materials and the first upgrades, the shard, the first squall and leak, repairs |
| 2-3 | Ruin Isle: diving, the first warden, the writing; sharks; the crowbar and Old World rooms |
| 3-4 | The reinforced hull, the Outer Isles, the first big site, fog or cliffs |
| 4-5 | The Tinker and **the first Old World upgrades** (cells, searchlight, radio); the second harbour; night in deep water; **the Grey** |
| 5-6 | **The cutter** (the hold, the bunk, the map table, the workbench); **the first time capsule: they were us** |
| 6-8 | The other big sites in any order, the drowned street, the lens, the motor, sonar, the current lanes, guilds, your cabin |
| 8-9 | **The midpoint truth:** the Cartographer took the core |
| 9-10 | The stone-bound keel; the Storm Waters fall back |
| 10-12 | **The storm crossing**, the eye, the Cartographer |
| 12-13 | The Keelgate opens, the Deep, the ark's message, the choice |

### 8.5 Ending a session
A voyage always closes at a mooring with a save (D46), and before the player quits the next goal is already in sight: the pause
menu's "where you are" card and the log name it (a plan two parts short is a strong reason to come back), and the loading screen
greets them with a line from the Cartographer's log.

---

## 9. Does this fit what we want? (self-check)

| Check | How the concept meets it |
|---|---|
| The README's premise: atmospheric first-person sailing and exploration; a mysterious sea of islands, ruins and a lost civilisation; the boat is transport, home, storage and workspace and grows | Made concrete: the Old World, the Gale, the stations; the boat carries stores, repairs, upgrades, machines and trophies |
| The owner's answers (rounds 1 and 2) | Light tool-based danger (5), a hand-made sea in rings with a story and an ending (3, 7), only boat and sea pressure (5.4), about 13 hours (8.4); the night horror (5.5); people behind counters (3.7); digging and materials for upgrades (6.3); the ancients as our world in fifty years and futuristic upgrades (3.3, 6.1) |
| Simple sailing (D41, D65) | Kept: depth from weather, sea and route; the motor is only a helper (6.1) |
| Charts belong to the boats (D57) | Kept: the mast chart and the map table; nothing in the satchel |
| The hand-crafted look (D16, D38) with futuristic technology | The Old World's look already exists (pale, seamless, turquoise lines). On the boat its parts are rigged into wood with rope and iron, each made for its purpose (D53). Turquoise stays the Old World's colour, now meaning power |
| Everything already built has a job | Section 1 |
| A hobby project's scope | Two boats, seven sites, three harbours, about fifteen islets, about 40 dig spots, about 15 materials, nine threats, about twenty people (most minor); people mostly behind counters |

**Risks, and the answer to each:**
| Risk | Answer |
|---|---|
| An empty sea between places | Sea events (7.4), the sight rule (7.3), shorter legs (3.2, question 11), current lanes later |
| Jobs and collecting become a chore | No repeats; materials by ring, not farmed in one place; digging never empty-handed; storms refill beaches |
| Technology makes sailing pointless | Limited power, a weak motor, machines that draw the Grey, one capability per upgrade (6.1) |
| The twist is guessed too early | Wanted: the reveal ladder (3.4) turns suspicion into the hook and confirms it at hour 5-6 |
| Navigation without markers frustrates | Sailor's directions, the shard's pull, beacons, the chart filling in, an optional bearing pin |
| Too little tension | The night, the Grey, squalls, leaks; cargo at risk |
| Too much tension | Every threat has a tell and an answer; nothing is lost for good; harbours are always safe; quiet nights happen |
| The climate theme lectures | Told only through letters, broadcasts and places |
| The middle drags | The Tinker and the Grey at hour 4-5, the cutter and the capsule at 5-6, the midpoint at 8-9, something new every hour |
| Backtracking | The Hollowkeel waits in the harbour you sail to; current lanes; the cutter's bunk saves at anchor |

---

## 10. Production

### 10.1 New systems
| Need | System |
|---|---|
| **Must** | A running day and night (the lighting rig has presets only); sleeping. People and their lines (parchment scraps). The log screen. Quests and jobs (data-driven, like the stats tables). **Materials and plans** (data tables), shops and spending salt. **Digging** (dig spots, the hole and heap, what rises). Boat damage: holes, bilge water, patching, bailing, a torn sail. Hurt, faint and recovery; the dropped satchel; wreck, towing and the floating stores. Weather events (squalls) on the existing weather layers. Sea events. Creatures and machines (sharks, rams, wardens, the Grey) with simple behaviour. **Power** (cells, charging, draw). The second boat (D51). The Gale as the world's edge. Music by mood and the sounds of the night. Unlocking the hub's zones and building their insides. More islands, two harbours, the sites. |
| **Should** | Scavenging (lockers, panels, the crowbar). The Old World's writing and the log's words. The shard's pull. The map table. Flares, the harpoon, the winch, the lead line, the grapnel. The radio and its broadcasts. The searchlight, sonar, the motor. Beacons. Fishing. Treasure maps. Time capsules. Cosmetics. Rescued people aboard. |
| **Could** | Guilds. The current lanes and drive. The detector. The gyro. A rowing tender. The bearing-pin assist. The barometer. Carrying big finds in both hands. |

### 10.2 Build order: one playable hour first
Build the first hour completely before any more content, so the owner can play the loop and judge the fun.

1. **M6, the first hour (vertical slice):** the prologue (beach, the Harbour Keeper, the log in the chest, **the buried canvas**,
   the mended sail, the Spire site, the first landfall, the Hollowkeel in the haze); the day and night; people v1 (the Keeper, the
   Purser, the Shipwright, the Chandler, the Reader); the log screen v1; **materials v1** (timber, rope, canvas, iron, tar and two
   Old World scraps) and **digging v1** (about 8 dig spots in the Inner Waters); jobs v1 (bring, survey, dig); three sail-age
   upgrades built from materials and salt; boat damage v1 and the squall; the first eerie night (the light under the water); music
   by mood. **The owner plays it.**
2. **M7, Act I complete:** Ruin Isle's site, the shard, the writing, sharks and diving, the crowbar and the first Old World room,
   the first stations in the Keelgate, the Keel Walk and the Mess, five sea events, four islets.
3. **M8, the Outer Isles (1):** the reinforced hull, the Tinker and power, the searchlight and the radio, one big site, Longshoal's
   harbour, night and the Grey, flares and the harpoon.
4. **M9, the cutter and the rest of Act II:** the cutter and its map table, the other two sites, the drowned street, the time
   capsules, Mistholm's harbour, the zones of Act II, the midpoint.
5. **M10, Act III and the ending.**
6. **M11, polish:** sound, balance (prices, materials, damage, event rates), accessibility (Next steps 8), a full playthrough.

Each milestone ends with the owner playing it; the design of the next one changes with what they feel.

### 10.3 Content in numbers
| | Count |
|---|---|
| Old World sites | 7 (2 short, 3 long, the storm's eye, the Deep) |
| Harbours | 3 |
| Islets | 12-15 |
| Dig spots | about 40 (about 10 of them time capsules) |
| Materials | about 15 |
| Plans (upgrades) | about 25 (12 sail-age, 10 salvaged Old World, 3 restored) |
| Threats | 9 |
| Sea events | about 14 kinds |
| Jobs | 8 templates, about 35 hand-placed jobs |
| Log pages | about 20 |
| Old World words and signs | about 30 |
| Boats | 2 (and the Hollowkeel at the end) |
| Tools | 15 (3 exist) |
| Hub zones to open | 18 |
| People | 6 main, about 15 minor |

---

## 11. Inspirations

| Game or film | What we take |
|---|---|
| Sea of Thieves | The look; hands-on ship care (patching, bailing); treasure maps, riddles and digging at the X; the ship's bell |
| Horizon Zero Dawn, Planet of the Apes | The ancients are our own future; old technology among a younger culture; the slow reveal |
| Dredge | Day calm, night dread; a thing you avoid, not fight; upgrades from salvaged materials that open waters; cargo at risk |
| Subnautica | Fear of the deep; building from what you find; tools that open depth |
| Outer Wilds | Knowledge as progress; a ship's log that collects clues; curiosity instead of quest markers |
| The Legend of Zelda: The Wind Waker | A sea chart filled in by sailing; an island with one secret everywhere; sailing as delight |
| Valheim | Each region gives the materials that open the next |
| Waterworld | Salvaged machines rigged onto sailing boats |
| Sailwind | Navigating by the sea and the compass, not by markers |
| Chants of Sennaar, Tunic | Learning unknown writing from its context |
| Animal Crossing, Hades | Digging that always gives something; a home that visibly grows; people who move in |
| Suikoden, Death Stranding | People you help come to your home and stay |

---

## 12. Questions for the owner (recommendation first)

**New with the Old World (round 2):**
1. **The gap:** how long after the Old World does the game play? About **300 years** (its concrete and composites still stand, its
   memory has become myth) · about 100 years (grandparents' stories; the technology much easier to read) · over 1 000 years.
2. **The Grey:** an **Old World warden machine** overgrown until it looks like an animal (ties into the twist, makes every machine
   on the boat a risk at night) · a real animal.
3. **How futuristic at most:** **salvaged machines rigged into wooden boats**, on limited power, with the ark as the one true Old
   World ship · a fully Old World boat of your own by the end (a motor yacht or a hydrofoil).
4. **The motor:** **a weak helper on limited power** · none (sailing only) · a real alternative to the sail.
5. **Old World writing:** **worn letters and signs the player learns to read, plain text from the time capsules on** · readable
   from the start (the twist comes from the story, not the signs).
6. **Carrying big finds:** **only by the winch and the hold** · also carried in both hands, slowly (needs a held-object view).
7. **Material names:** **Saltbound names that change to the real names** once known ("spark jar" → battery cell) · real names from
   the start.

**Still open from round 1 (the recommendations stand as defaults until you say otherwise):**
8. **The player:** a silent castaway · a castaway with a few written replies · someone born in the Cove.
9. **The Cartographer at the end:** alive in the storm's eye · only their last log and their wreck.
10. **The ending:** one choice with two short final scenes · one fixed ending.
11. **Distances:** move the Outer Isles closer (no main leg over about 12 minutes) · keep them and lean on events and current lanes.
12. **Names:** keep the Tidewrights, Tidestones, the Gale, the Grey, the Saltbound, or give your own; and replace the Sea of Thieves
    region names in the code (Shores of Plenty, Ancient Isles, Wilds, Devil's Roar) with ours (Inner Waters, the Outer Isles, the
    Mistreach, the Storm Waters).
13. **Navigation help:** no markers, an optional bearing pin in Settings · always show the goal.
14. **Fishing:** a small calm activity · leave it out.
