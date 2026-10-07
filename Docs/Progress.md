# Progress

Last updated: 2026-10-06 (session 4: M4 sailing boat D36-D45, M5 save/load D46, Ruin Isle D47, swimming and wake D48, haze D49, satchel D50, lived-in boat D52, object overhaul D53, flags / player / ladders D54, new UI D55; session 6: the owner's review rounds D58, D59; the prompt's position and the home island D60; session 7: the hub ship D61-D64; the menus D66; session 8: the game concept D67, D68; the hub's second review D69; the hub's harbour D70; M6 steps 1-3: the day's clock D72, digging D73, the ship repair system D74; session 9: the hub's light D82; the owner's review D79, hands and tools D80, the Harbour Keeper's figure D83, music D84, the meadow D86, FH5 the light under the water D88).

## Milestone status

| Milestone | Status |
|---|---|
| M1 Infrastructure & foundation | **Done** (verified) |
| M2 First-person player & moving-platform tests | **Done** (verified in PIE and standalone) |
| M3 Visual test scene | **Revision 3 in review** (stylised realism pass after owner feedback; see below) |
| M4 Starting boat & navigable water | **Technical part done** (greybox boat sails, D36); art waits for the decisions |
| M5 Starting harbor & complete loop | **In progress**: the loop (depart, sail, return, moor, save/load) works (D46) |
| M6 The first playable hour (D67) | **Planned**: the game concept (`Docs/Design/GameConcept.md`) waits for the owner's answers |

## Decisions needed from the owner (visual checkpoint)
Recommendation is listed first. Evidence images: `Docs/Evidence/M3/` (Day / Evening / Dusk × 10 shots,
plus `Standalone_Dusk_PlayerStart.jpg`).
1. **Overall look:** answered (D16): stylised like Sea of Thieves (not realistic), with crisp hand-painted
   material detail on simple chunky shapes.
2. **Living space aboard:**
   - (a) A raised deckhouse with standing headroom and the floor above the waterline. It is technically
     safe and gets windows and a lantern-lit interior.
   - (b) A low classic cabin trunk with the saloon below deck. Nicer silhouette, but the ocean would render
     inside the hull, so it needs water masking and carries more risk.
   - (c) A mostly open deck with a small cuddy. Simplest, but less of a home.
3. **Hull colour:** (a) oxblood red with a cream sheer stripe (current) · (b) dark green · (c) natural varnished wood.
4. **Default sea state:** (a) the current rolling swell (calmer inside the harbour) · (b) calmer by default,
   with big seas reserved for weather.

Owner feedback received so far (2026-10-03):
- Session 1: "The water is okay, waves are missing, needs a bit more realism – the water from Sea of
  Thieves hits the spot." The waves were revised (D14).
- Session 2: "The water doesn't look like good quality; the objects are missing detail, NOT realism."
  Then: "The carvings in the stones look horrible – I mean material detail, no blur, visibly readable
  material", with Sea of Thieves reference images for rocks, deck wood and sea. Applied in revision 2
  (D16–D19). Rock shapes were confirmed as better; material and wood were iterated after that.
- Session 2, later: detailed briefs to restyle the dock and the ocean to Sea of Thieves reference images,
  then: "Our graphics sometimes look too realistic – Sea of Thieves is a bit more comic: stylised realism."
  Applied in revision 3 (D20–D22).

## M1 – Infrastructure (done)
- Git + LFS repository at `Saltbound_all/` (template baseline is the first commit), clean template.
- Automation: Python remote execution (`Tools/ue_exec.py`), MCP (`Tools/ue_mcp.py`), headless Blender,
  build/launch/package/run scripts.
- Blender → Unreal pipeline verified on `SM_Crate_Small`: 1 m = 100 uu, axes, slot materials, UCX collision.
- The standalone build packages and runs.

## M2 – Player & moving platform (done)
- First-person character (176 cm capsule, 165 cm eye height, walk/sprint/jump, no head bob or camera roll).
- Interaction volumes with a subtle HUD, and control stations (helm) you step into and out of.
- Walking, standing, jumping and helm use on a kinematic heaving, rolling, turning deck.
- Automated suite `sb.Dev.RunMovementTests`: 30/30 checks pass at three motion levels; 10/10 pass in the
  standalone build. Details in git history and Decisions D7.

## M3 – Visual test scene (built)
Map: `/Game/Saltbound/Maps/L_VisualTest` (game and editor default map; since D60 the home island round the old cove's pier).
Regenerate with
`python Tools/ue_exec.py Tools/ue_scripts/create_visual_test_map.py`.

Implemented:
- **Ocean (custom, D6):**
  - 12 shared Gerstner waves (42 m to 0.8 m), calm zones, and CDLOD instanced tiles (25 cm cells near the viewer).
  - Single Layer Water shading, analytic normals, two ripple layers out to about 250 m.
  - Jacobian crest foam and depth-based shore foam.
  - Translucent crest light, scaled by the sun after the atmosphere (fades in the evening and at dusk).
  - Distance-based roughness against specular sparkle.
  - CPU queries via `USaltOceanSubsystem`. CPU/GPU agreement checked visually with probe spheres (`L_OceanTest`).
- **Materials:**
  - Procedural tileable textures (`Tools/textures/gen_textures.py`).
  - `M_SB_Surface`: tint, per-part variation, macro noise, edge wear on bevel faces, grime and wetness from vertex colours.
  - `M_SB_Rock`: triplanar rock with lichen and a wet band. `M_SB_Glow` for emissive parts.
  - About 20 material instances following the palette (Docs/ArtDirection.md).
- **Assets (Blender scripts):**
  - The 4 m test deck section of the boat at 3.4 m beam: teak deck, bulwark, oxblood hull, cream deckhouse,
    portholes, hatch, cleat and rope.
  - Lantern, pier section, three coastal rocks, and a landmark sea stack with a ruined tower.
- **Lighting:** `ASaltLightingRig` with Day, Evening and Dusk presets (`sb.Lighting <name>`), using a
  physically based sun (90 klx) and preset exposure ranges.
- **Lantern:** `ASaltLantern` (28 cd, 2100 K, flicker; interact to light or douse it).
- **Floating test boat:** `ASaltDevFloater` makes the deck section ride the waves; it is walkable.
- **Tools:** `Tools/capture_shots.py visualtest` captures each named shot per preset and builds a contact
  sheet. `sb.Dev.Benchmark` and `sb.Dev.Perf` measure performance; `sb.Dev.Delayed <s> <cmd>` runs a command later.

Performance, measured in the standalone Development build at 2560×1440 fullscreen with vsync off,
**editor closed**, TSR at the default 60.6 % (internal 1552×873), Lumen hardware ray tracing and Virtual
Shadow Maps, from the player start on the pier:

| Preset | fps avg | frame avg / p95 | GPU avg |
|---|---|---|---|
| Day | 126.3 | 7.92 / 8.76 ms | 7.24 ms |
| Evening | 120.0 | 8.33 / 9.11 ms | 7.68 ms |
| Dusk | 120.5 | 8.30 / 9.03 ms | 7.65 ms |

GPU frame breakdown (ProfileGPU): volumetric clouds about 1.5–1.8 ms, TSR about 1.3 ms, lighting and
Lumen reflections about 0.9 ms, shadows about 0.9 ms, water about 0.4–0.8 ms. There is plenty of headroom
for a higher screen percentage (for example 75 %) and more content.

### M3 revision 2 (session 2, after owner feedback)
- **Water (D19):**
  - Deep-blue body with turquoise light in backlit crests.
  - Lace foam: whitecaps with trails, marbled open-sea streaks, softer shore foam with lapping bands,
    translucent overall.
  - A third ripple layer and lower near-field roughness give crisp sun glitter.
- **Rocks (D16):**
  - `Art/Blender/Scripts/sb_rock.py` builds chunky blobs with planar facet cuts, filled pockets and soft
    bevels. Carving options exist but are off.
  - Two new scatter rocks sit along the shoreline.
  - Stone texture (2048, 2.5 m tile): crisp tone patches, sparse painted cracks with a highlight lip,
    recessed chips, specks.
  - `M_SB_Rock`: sharp triplanar blend, vertex masks (edge light, cavity dark, plate tone), painted top
    light, lichen, and an algae/barnacle tide zone.
- **Wood:**
  - Straight-grain painted wood (2048 per m): wobbling grain lines, rare long arches, small knots, fibre
    streaks.
  - Each built part gets a random UV offset, and planks vary more in tone.
- **Paint:** brush strokes along the boards, with the grain telegraphing through.
- **Canvas:** visible weave, a sewn seam with stitches, creases.
- **Landmark:** soft-faceted sea stack, with a ruined round tower of bevelled stone blocks (broken crown,
  window, glow band).
- **Image:** native screen percentage, sharpening 0.6, no motion blur (D18).
- **Fix:** editor crash when rebuilding materials (D17). Content defaults no longer load in constructors.
- **Evidence:** latest captures in `Docs/Evidence/M3_rev2/`.

### M3 revision 3 (session 2, stylised realism)
- **Hand-made timber (D20):**
  - `sb_timber.py` builds individually bent, twisted and worn boards and hewn posts.
  - It is used for the dock (68 pieces), the crate, and the boat deck section. The boat has planked
    bulwarks, a planked deckhouse with an inward lean, chunky rails, a timber door, big portholes and a thick
    roof.
- **Lighting, sky and grading (D21):**
  - Golden sun with a large disc, cool sky fill, deeper blue sky, low blue haze.
  - Split-tone grading.
  - Stylised cumulus clouds (`MI_Clouds_Stylized`).
- **Ocean v4 (D22):**
  - Big rolling swell with layered waves, flowing ridged ripples, patchy breaking whitecaps.
  - Crest light from sun luminance.
  - Crisp white sun glints. The seabed is hidden from reflections.
- **Materials:**
  - Broad-grain warm wood; weathered grey-brown dock timber with per-board colour and roughness.
  - Teak and varnish are less orange. Canvas has sewn panels. Rock cracks are fewer and longer.
- **Captures:** first-person and low-sea shots were added (`11_dock_fp` … `14_sea_low_cross`). Evidence is
  in `Docs/Evidence/M3_rev3/`.

### Ocean revision 5 (session 2, ocean brief and reflection-breakup brief)
- **Smearing (D23):**
  - Cause: the displaced water wrote no motion vectors, so TSR smeared stale history.
  - Fix: previous-frame wave phases give real motion vectors; the water writes temporal responsiveness
    0.3; Lumen screen-traced reflections are off.
- **Wave hierarchy (D24):** three long swells, subordinate medium waves in calmer/choppier patches
  (CPU = GPU), foam trails from a time-shifted Jacobian, and tile bounds that cover the full displacement.
- **Environment layers (D25):**
  - Independent, blendable time of day, region (Shores of Plenty, Ancient Isles, Wilds, Devil's Roar),
    weather and sea state.
  - Region zones blend by position. The rig owns the clouds.
- **Reflection breakup (D26):**
  - Causes: below-horizon reflection rays on short straight crests drew the silver/brown streaks; the
    1.5 m ripple layer drew the crumpled glints.
  - Fixes: horizon occlusion of specular, macro normal without waves under 5 m, softer ripple texture,
    rebalanced layers, quiet patches, specular AA cap.
  - Results: fine near-field detail roughly halved; shimmer in moving clips down by 47–55 %.
  - Evidence: `Docs/Evidence/Ocean_rev5/`. It holds the step-by-step chain with the horizon debug view, a
    four-view before/after, motion frames, and two side-by-side clips (left before, right after).
- **Tools:**
  - `sb.Ocean.Freeze`/`sb.Ocean.Time`, `sb.Dev.Camera … [Float]`.
  - `Tools/ocean_diag.py` (controlled A/B captures), `Tools/water_metrics.py`,
    `Tools/record_clip.py --float`, `Tools/tune_env.py`.

### Ocean revision 6 (session 3, FFT ocean, D27)
- **Wind sea:**
  - Tessendorf FFT on the GPU (three cascades, 250 m / 37 m / 6 m) on top of three analytic Gerstner
    swells. The CPU mirrors the long waves (≥ ~12 m) for buoyancy.
  - New module `SaltboundRender` with compute shaders `Shaders/SaltOceanFFT.usf` and
    `SaltOceanSpectrum.ush`.
- **Shapes (owner feedback):**
  - Distinct major ridges (peaky spectrum, sharper crests); medium waves and ripples subordinate.
  - Smooth areas in troughs and drifting quiet patches; shading normals equal to the displaced surface.
- **Sea states:** Flat (new windless biome), Calm (own low-wind balance with a sun glitter path),
  Moderate, Rough, Storm.
- **Look:** blue body with turquoise only in thin backlit crests; less reflection (Specular 0.24);
  roughness from sub-pixel slope variance (grid artefact fixed).
- **Removed at the owner's request:** all foam (whitecaps and shore foam) and the spray particles. A
  detailed foam rework was tried and dropped.
- **Evidence:** `Docs/Evidence/Ocean_rev6/` (references compared with before and after, Rough / Calm /
  Flat, low and elevated views).

### Underwater revision 1 (session 3, D28)
- **Swimming:**
  - Enter the sea anywhere: from the pier, by wading in, or by teleporting in. The swimmer floats with the
    eyes just above the water and rides the waves.
  - W swims where you look, Space rises, Ctrl / C dives, Shift swims faster.
  - Space at the pier, the boat or a flat rock climbs out.
  - Walking off the pier drops you into the sea; in the shallows you wade out.
- **View from below:**
  - The sea surface seen from underneath: Snell's window with sky and sun, total internal reflection
    outside it, comic wave shading.
  - Turquoise underwater fog after the owner's Sea of Thieves references (light lost with depth,
    visibility about 30 m).
- **Seabed:** sculpted sand (beach, pier ridge, drop-off, sandbank, mounds and ripples), animated caustic
  light on sand and rocks, 34 swaying sea-grass patches.
- **Stone textures:** no crack lines and no chip outlines any more (owner feedback).
- **Evidence:** `Docs/Evidence/Underwater_rev1/` (contact sheet, a drop-off clip, a look-up clip in the
  open sea).
- **Test:** `python Tools/swim_test.py` (PIE, 5 checks, all pass).
- **Owner review round (D29):**
  - Painted sand texture, and softer caustics that no longer stretch on slopes.
  - Clear turquoise shallows: the sea was 100× too dense (per-cm coefficients). Deep water is unchanged.
  - More detailed (soft) stone texture.
  - Player 1.6 m tall (eyes at 1.5 m).
- **Review round 2 (D30, awaiting the owner's check):**
  - Sand without visible repetition and with fainter ripples; no small scatter rocks.
  - Faint underwater particles; more water tint over the floor and softer caustics.
  - Rough sea with narrower crests, stronger medium waves and smaller highlights.
  - A storm area 3 km south of the cove. In play mode, use `sb.Dev.Camera 0 -300000 350 -8 90 90 1` to
    view it (`sb.Dev.Camera 2500 -900 350 -8 90 90 1` for the cove, `sb.Dev.Camera off` to return), or
    `sb.Dev.Teleport 0 -300000 0` to swim in it.
- **Later review rounds (awaiting the owner's check):** more visible particles and calmer distant crests
  (D31); Rough sea with one dominant direction, broader crests and crest foam patches (D32); foam structure,
  fresh foam vs residue, varied crest spacing (D33); the coloured specks on distant crests fixed at the
  cause, an overflow in the underside's underwater light (D35; D34 was a wrong guess and is reverted).

- **Beach with shape (D37, awaiting the owner's check):** the dry beach has sculpted wind ripples, soft mounds and
  swash ridges (Nanite sand mesh); the sea floor keeps its earlier look.
- **D39:** player 1.5 m; no lichen on the coastal rocks; gulls over the cove and at sea; a gangway to the boat.
- **D41 / D42 (awaiting the owner's check):** simple sailing (sail down = go, sail up = slow), wheel only steers, rope
  points and a windlass with an anchor, a sail that fills and breathes, physics ropes, properly laced deadeyes, an
  old boat in shape (hewn spars, cracks, nails, repairs, patched sail); the sea is cut out inside the hull.
- **D69 (the owner's second review of the hub, waiting for the next look):** no sea haze inside the hull (interior zones push the fog
  out), the water hall's water dark and mirroring the lanterns (sheltered water, the hidden shell casting shadows, screen traces
  inside), warm 240 cd lanterns that flicker. The stern is a round half ellipse with grand galleries round three sides; the
  quarterdeck's and the poop's fronts are glazed in iron frames with grand stairs along both bulwarks up to the quarterdeck and the
  poop (both open decks now); the command hall stands on the poop in glass under a copper roof. The Chandlery is a shop in the
  forecastle's front (the cargo deck is part of the waist). The rig: square yards only, gathered furls, hemp ratlines with knots,
  every rope end attached, 36 hanging ropes as real cables in the wind. The hull's sides: gunports, lids, round portholes, skids,
  rosettes, entry ports, scuppers. Wood, paint and metal got relief (plane marks, grain grooves, brush ridges, cracks, dents).
  `sb.Dev.HubGoto quarterdeck|poop`; `Tools/hub_pie_shot.py` for captures in play.
- **D70 (the hub's harbour, waiting for the owner's look):** a canal the whole length of the dock deck with two basins and four
  hidden doors (two each side), quays, galleries, lifting bridges and footbridges, two docking stations; the Keelgate out of the hub
  for now; the Slip a shop on the quay. Sailing in and out are cinematics (`ASaltHubPassage`: the camera starts at the player's view
  and ends at their eyes, the first time long, never skippable); casting off at a hub berth asks "Sail out?". Her inside is a level
  of its own (`L_HubInside`), streamed in near her. The command hall has a glass bridge on top, the ancient helm in it (locked).
  Try: `sb.Hub.Show 1`, sail at a door, or `sb.Hub.Passage arrive 0 1 long`; `sb.Dev.HubGoto dock|foredock|wellfoot`.
- **D71 (the home island's detail pass, waiting for the owner's look):** the colours and every place kept; the ground is one
  surface (`SM_Home_Ground`) with painted sand, grass, litter and footpath sets blended by height, relief where it reads (berm,
  tussocks, ruts, root ridges), wet glossy sand at the water, an edge where the grass breaks into tufts and creepers over the sand,
  and a lawn of real blades (37 600 instanced square-metre patches that take the ground's colour);
  palms and trees rebuilt with ringed and furrowed bark, roots, branches, folded veined leaves (a third tree kind); shells, seaweed,
  pebbles, husks, fronds, twigs and crab burrows on the ground; places to find (a wreck, tide pools, a lookout cairn aimed at the Spire,
  a spring from an Old World pipe, a fire pit, a walkable fallen palm, the dig spot under the leaning palm); the Harbour Keeper's hut on
  the pier's beach with its counter, smoking chimney, nets and lanterns, the Keeper's spot and `Keeper_Point` ("Talk to the Harbour
  Keeper"). Scripts: `place_home_island.py` (the island's actors), `setup_island_materials.py`, `build_home_props.py`,
  `gen_island_textures.py`; views: `Tools/island_shots.py`.
- **D78 (the island's grass and forest floor, waiting for the owner's look):** a dense meadow of real blades (about 4 500 a
  square metre, combed with the wind, no star tufts), a calm darker green floor under it, real 3D fallen leaves on the brown parts,
  and the ground material fixed for DX11 (shared sampler). Views: `Tools/island_shots.py` (grass_top, meadow_eye, litter_top ...).
- **D76 (seagulls and the hub at once, waiting for the owner's look):** the gulls are a skinned herring gull (`SK_Gull`: hooked beak
  with the red spot, eyes, a fanned tail, tapered wings with the black tip wedge and white mirror) whose bones are posed every
  frame: a wingbeat with the hand trailing the arm and the wing folding on the upstroke, the crooked glide, banking with the tail
  twisting into the turn, the head level and glancing about. `sb.Hub.Appear` (your boat 60 m off her doors), `sb.Hub.Appear dock`
  (on the quay in her harbour), `sb.Hub.Appear show`; watch a gull with `sb.Dev.BirdCam 0` and `sb.Birds.Flap 1` plus `slomo 0.1`.
- **D79 (the owner's review, waiting for the owner's look):** the player 1.6 m with the eyes at 1.5 m (over the Keeper's counter); the
  tool wheel picks on a flick of the mouse; the underwater view splits where the lens meets the water (a thin waterline); the meadow
  even from near to far and from every side of the sun (its blades lit as the ground, matte); sprint 600 cm/s; the sun turns every
  frame; ladders only with E; Continue fades the title out. Evidence: `Docs/Evidence/D79/D79_sheet.jpg`.
- **D80 (hands and tools, waiting for the owner's look):** first-person arms (comic hands, rolled sleeves) holding every tool by its
  grips; the left mouse button uses what is in hand: the compass close, the spyglass (a real three-draw brass one) to the eye drawing
  out, the lantern lifted, the spade (a real worked one) digging: aim, drive in, lever, lift the spadeful, toss it (clods fly), the body
  leaning into it; dig spots are dug with the left button (E takes the find). Try it: keys 1-6 or the wheel, then left click; `sb.Dev.GiveTool
  Shovel`. Evidence: `Docs/Evidence/D80/`.
- **D83 (the Harbour Keeper's figure, waiting for the owner's look):** an old salt behind his counter (cap, beard, pipe, waistcoat), who
  breathes, looks at you when you come near and gestures while he talks (D81's lines). Evidence: `Docs/Evidence/D83/`.
- **D88 (FH5, the light under the water, waiting for the owner's look):** on the first night at sea after casting off, something long
  and glowing passes under the boat 8 m down, lights her hull from below and is gone. Try it: `sb.Hub.Appear`, `sb.Lighting Night`,
  `sb.Event.SeaLight`.
- **D86 (the meadow, waiting for the owner's look):** daisies, buttercups and violets in drifts over the meadow; the bare squares in
  the lawn are gone (its patches wider, worn-earth holes filled, round thin patches only at the grass's edges).
- **D84 (music, waiting for the owner's listen):** the owner's four tracks by mood: calm by day, the dark one from dusk, the drama one in a
  storm or with the boat filling; long rests between tracks; the settings' Music volume. `sb.Music Calm|Night|Drama|off|auto|next|info`.
- **D82 (the hub's light, waiting for the owner's look):** no more horror ship: below decks the eye may open to EV 0 with one exposure
  bias (-1) at every hour (the rooms went black at night), the lanterns at 2 800 K (1 900 K turned the golden wood orange-red) with a
  12 cm source, and the lanterns' own parts cast no shadow (the dark disc under each lantern and the stripes round it). A/B sheets in
  play: `Tools/hub_light_ab.py`.
- **D85 (the letter, waiting for the owner's look):** the castaway wakes with a letter in the pocket, for the Cartographer from their
  sibling Wren: soaked, the ink run. L reads it; drying it at the fire pit by the footpath (hold E) makes it readable, and the Keeper
  knows the name (Rowan) and points south, to the Cartographer's camp past the Spire. Try it: New voyage, L, the fire pit, the Keeper;
  or `sb.Story.Letter [dry]`.
- **D81 (the first hour, FH2, waiting for the owner's look):** people's lines: the Harbour Keeper speaks on a torn half sheet with his
  wax mark, his words written out in ink, the answers on key scraps (1-3, E). What he says follows the story (the first meeting and his
  gift of the shovel and a lantern, the cache, the mending, ready to sail, the morning after); he calls after you when you cast off. The
  lines are data (`Content/Saltbound/Story/Lines.json`). Try it: New voyage, then E at the Keeper's counter, or `sb.Talk Keeper`.
- **D77 (the first hour, FH1, waiting for the owner's look):** the first hour planned beat by beat (`Docs/Design/FirstHour.md`) and
  built step by step. FH1: a new voyage starts as a castaway on the wreck beach at dawn with nothing; the Keeper gives the shovel and a
  lantern, the sea chest the Cartographer's compass and spyglass; the ripped sail sets only after the cache's sailcloth has mended it;
  the story's beats are saved. Try it: New voyage from the menu, or `sb.Story.Prologue` in play.
- **D74 (M6 step 3, waiting for the owner's look):** the ship repair system: hard knocks spring planks (holes on the bulwark's inner
  face, the sea spurting in), the water aboard weighs her down and shows over the deck, at 4 t she founders (towed home for now);
  patch with driftwood and nails (hold E), bail with the bucket by the starboard gap (hold E), a sail torn by too much wind mended
  with sailcloth at the mast. Try it: `sb.Boat.Hull hole 2 1 2`, `sb.Boat.Hull tear`, `sb.Dev.Give Driftwood 3`, `sb.Dev.Give IronNails 6`.
- **D73 (M6 step 2, waiting for the owner's look):** materials (sailcloth, iron nails, tar, the pale float), the shovel (from the
  Harbour Keeper at his place by the pier's beach; key 6), seven dig spots on the home island (mounds and stakes; dig with the shovel
  in hand, hold E), digging anywhere on sand and earth for small finds. Try it: `sb.Dev.GiveTool Shovel`, then the mound two paces
  seaward of the palm by the pier's beach.
- **D72 (M6 step 1, waiting for the owner's look):** a running day and night (40 minutes, the night faster): the sun moves through
  Morning, Day, Evening, Dusk and Twilight into the Night, a moon lights the night sky, the sea and the clouds, stars come out
  (a sky dome that draws the atmosphere itself), lanterns light at dusk and go out in the morning (a lantern you doused stays out until
  the light changes), the hour and the day are saved, the pause card says "day 2 · evening, the sun low in the west". Try it:
  `sb.Time 22`, `sb.Time.Scale 60`, `sb.Lighting Night`.
- **D65:** the starter boat is calmer in the waves (`sb.Dev.BoatMotion`) and carries a square sail that always pulls. Known
  issue: the furled sail renders dark; `sb.Dev.RunBoatTests Float,RollKick` to re-run in a fresh PIE.
- **D64 (the owner's review of the hub, waiting for the next look):** boats come in from the side through hidden doors in the hull
  (both sides, x 32..40) that open inwards like a double door into the Water Hall, a cavity of sea water across the bottom deck
  with quays, galleries and the slipway; masts stay up (13.2 m clear). The stern is a galleon's again; the hull is planked to the
  keel with more shape; the decks are 3 m apart and every deck and room is built (893 rooms behind 38 gates). The doors open for a
  boat by themselves (the dock watch, `sb.Hub.Doors`) or with E at a capstan. In `L_HubReview`: the side entry, the gates, the
  walks (Slip 3 s, Tally House 6 s, Keelgate 28 s, Chandlery 55 s), the hoist, ladders, slipway and headroom pass. Open: the
  hall's water glows (sky light), seams of light in some ceilings, the square sail is about as wide as the doors' narrowest
  clear way, the Chandlery walk.
- **D63 (phases 3 and 4, waiting for the owner's review):** the hub is in the game. Her level `L_Hub` streams in at her berth in
  `L_VisualTest` (`ASaltHubShip`; hidden until the first island is found, `sb.Hub.Show 1` shows her now) and follows the player
  between harbours (moves only beyond 2.5 km from both berths). The boat moors at the fitting and fleet berths (the windlass; it
  saves). 33 gates block the locked zones and say why; E points at every service spot log what they will do; the Well's hoist
  rides to the lower and main deck; ladders up the quays; the salt purse and the unlocked zones are saved.
  `sb.Dev.RunHubTests [part]`: every part passes except the Chandlery walk (63 s, as D61). The packaged build runs 70-78 fps at the pier, in the dock, on the
  main deck and at the Keelgate (D63).
- **D62 (art pass, waiting for the owner's look):** the hub as a giant old galleon in `L_HubReview`. Only the art draws there;
  the blockout stays as hidden collision, so the D61 measurements hold.
  - Outside: the planked hull with the sheer, wales, lidded ports, windows, the tiered sterncastle, the transom with its
    arch, great windows and nameboard, quarter galleries, castles, the beakhead.
  - The sea gate on ropes and capstans (`sb.Hub.SeaGate`).
  - The rig: three masts, round tops, furled yards, the lateen, ratlined shrouds, the pennant.
  - Inside, every open space by hand: dock, Slip, dock stair, Tally House, Well, Keelgate, main deck, Chandlery, gates.
- **D61 (phase 1, waiting for the owner's review):** the hub ship Hollowkeel as a blockout in its own map `L_HubBlockout`
  (plain shapes, flat colours; open it in the editor and play): every deck of the open 20 %, the stairs, gates, props as boxes,
  markers and labels at the NPC spots, the player's boat moored at the fitting berth. After the logic pass (D61 addendum:
  rail 0.9 m, rudders, anchor cables and moorings, the crane, counters facing the customers, the hoist's counterweight, the
  port landing, her berth off the island's channel): walking 5-33 s to the Slip, the Tally House and the Keelgate, 63 s to
  the Chandlery; 3.7 m clear everywhere; the boat in and out under the arch; the Storm sea under +1.06 m (hull) and +0.34 m
  (basin). Open: a shortcut to the main deck, the space sheets 08-11 (the eight calm zones came with D63).
- **D60 (awaiting the owner's check):** the interaction prompt pinned on its object or low on the screen (pause menu: Prompt
  position; on the object by default). The start is a tropical ring island round a lagoon with the harbour inside: palms in
  groves, broadleaf trees, bushes, flowers, big-leaf plants, ferns and beach grass swaying in the wind, a footpath to a hill in
  the west, a sand islet in the lagoon, one channel out to the east between rocky headlands, a turquoise reef round it all
  (`island_shape.py`, `build_home_island.py`, `build_vegetation.py`). The Spire moved further out; the charts show the island.
- **D59 (awaiting the owner's check):** the first boat about 15 % faster (8.4 kn on a reach); stats tables for boats and the
  player (`Core/SaltStats`, `Docs/Stats.md`); W / S at the helm let the sail out or take it in (0-100 %, the speed follows);
  the sail bellies much more in a fresh breeze; barrel, crate and sea chest 30 % bigger; the open ends of the boat closed
  (breasthooks, rail caps, a stern head); ladders taken by moving into them, one smooth quick climb the same up and down; no
  more climbing out of the sea on to ledges; snappier walking and jumping.
- **D58 (awaiting the owner's check):** drag and drop in the satchel and the stores (off the pockets it flies back); the satchel
  and the stores open with a quick rise (0.2 s); E at the mast chart glides the eyes to the sheet itself (0.45 s; the names,
  north and the scale ink in on it) instead of a picture popping up; "Put the chart back" and the helm's controls ("A D Steer",
  the boat's state under it) in the key-scrap prompt; the landfall banner at 75 %; the view leans with the deck (30 %) and the
  knees take less of the swell (20 %); standing by low steps on a heaving deck no longer bounces the view (flat base for floor
  checks, pops eased out); calmer home waters round the cove (the waves 70 % as high within 700 m).
- **Waiting for the owner (2026-10-05):** a look at D53-D57 in the game; which items the game needs (then the item icons,
  rendered from their meshes); the second boat (D51) with its map table (chart stage 2); whether the UI's hero pieces
  should become 3D renders (a Blender pilot of the rope ring read plainer than the painted parts, D55).
- **D57 (awaiting the owner's check):** no map in the satchel or on the tool wheel (five tools now); a small live chart is
  pinned to the mast (1.6 km round the boat, north up), E to look at it closely. The map table waits for the bigger boat.
- **D56 (awaiting the owner's check):** flags that ripple like cloth; in big waves the eyes take part of the deck's swell
  (knees) and the waves no longer turn the boat round (the rig's forces heel it but do not twist it).
- **D55 (in progress, awaiting the owner's check):** the new UI from the approved design. Done: the fonts, the shared parts,
  the key-scrap prompt with painted lettering, the hold (raising the anchor) with the ink soaking up through the key's
  scrap, the save sign in the corner, the launch intro (startup movie, any key skips), the satchel (leather bag, pressed pockets, the tag on twine), the stores (the satchel beside the open crate or the
  round barrel, items flying between them), the landfall banner for a new island (found within 300 m of its shore,
  saved); the camera at 1.2 m. The item icons wait for the owner to decide the items. The tool wheel (hold Q) and the held tools (compass, spyglass, lantern).
- **D54 (awaiting the owner's check):** calm flags (a slow wave, no side-to-side swing with the boat's roll); the player
  1.4 m and slimmer; ladders climbed with W / S after taking hold with E, E lets go, off the bottom into the sea, over
  the top on board.
- **D53 (awaiting the owner's check):** object overhaul. The mast is a worked spar with partners, wedges and a coat, an
  old fish repair, bands, a boom saddle, hounds cheeks, halyard blocks and a truck; the gaff hangs from real halyards
  whose falls belay at new pin rails (the sail is set there); boom and gaff jaws with leather and a beaded parrel; the
  bell hangs from a ring under a hewn arm and rings, the lantern hangs on a hook and lights; both swing with the boat
  and when bumped; barrel, crate, windlass, oars, boat hook, chest, net and ensign staff detailed; the tarp, hatch,
  lobster pots and deck coils removed; collision for everything on deck.
- **D52 (awaiting the owner's check):** the boat's planks each a bit different, more on deck (lobster pots, net, chest,
  tarp, bell, lantern, trailboards, ensign), the barrel and crate open as 10-slot stores, the anchor rope through a
  hawse hole (the anchor is never seen), no mainsheet, bucket or cathead.
- **D51 (proposal):** the second boat, a 14 m gaff cutter with a top deck and a lower deck: `Docs/Design/Boat2.md` and its
  plan sheet; waiting for the owner's answers before the blockout.
- **D49 / D50 (awaiting the owner's check):** sea haze limits the view; the trail's density and width follow the speed;
  a satchel (Tab / I) with 24 slots, pickups on the beaches and the ancient shard at the ruin, and a map tab (M) with
  two parchment charts and markers for you and your boat.
- **D47 / D48 (awaiting the owner's check):** Ruin Isle 1.7 km out (beach, dunes, a broken stone gateway and wall),
  three island masses on the horizon; swimmers hover under water (no auto rise); the wake only reaches where the boat
  has been and the trail fades out smoothly.
- **D46:** save and load. Mooring saves, the pause menu saves and loads, quitting saves; the packaged game continues
  where the player left off (play-in-editor always starts fresh).
- **D44 / D45 (awaiting the owner's check):** a settings menu (Esc / F10) with vsync off and a frame cap by default;
  a wind pennant at the masthead; the boat steers with an oar on the starboard quarter (the wheel is kept for later
  boats); the sea shows a bow wave, white water along the hull, a Kelvin wake and a foam trail that follows the turns.
- **D43 (awaiting the owner's check):** the anchor brings the boat to a smooth stop and holds it (sail set or
  not); the windlass drum and rope turn; the wheel turns a full turn each way and sits lower; ladders on both
  sides with a rung-by-rung climb; the whole boat collides as one piece (boom, rudder, cathead, ladders); the
  boat carries its own gangway.
- **D40 (awaiting the owner's check):** the boat weathered (tone patches, paint chips, grime, rain streaks), hewn
  spars with dents and knots, and lived-in gear (pin rail, barrel, crate, bucket, oars, fenders, cleats).

- **Hand-crafted boat and materials (D38, awaiting the owner's check):** the boat rebuilt from individual hand-made
  boards, spars, ropes and a paneled sail; wood golden-brown and matte with simplified grain, paint muted, metal dark.

## M4 – Sailing boat, technical part (D36)
A 10 m double-ender (hand-made style since D38) lies moored on the south side of the pier in `L_VisualTest` (`ASaltBoat`, meshes
from `Art/Blender/Scripts/build_boat.py`). Walk aboard through the gap in the bulwark, take the steering oar (D45), raise
the sail and cast off.
- **Physics:** buoyancy from 67 hull columns on the shared wave field (heave, pitch, roll, heel), keel and
  hull resistance, a rudder foil, the gaff sail as a foil in the apparent wind with a free-swinging boom
  limited by the sheet (luffs head to wind, gybes downwind). 10.9 t, gathers way in about half a minute.
- **Collision:** capsules along the hull against the pier, rocks and seabed (contact springs and sweeps);
  it stops at the pier and runs aground on the beach.
- **Holding:** moored at the start (bow and stern lines, gangway to the pier); at the windlass: cast off, moor
  near the pier, or drop the anchor anywhere (smooth stop, holds place and heading, D43).
- **Boarding:** across the gangway while moored, or up the ladders on either side from the water (D43).
- **Recovery:** capsized boats right themselves at their last safe position; `sb.Boat.Reset` returns to the
  mooring.
- **Test:** `python Tools/run_pie_test.py "sb.Dev.RunBoatTests" --timeout 560 --done "SB_TEST BoatSuite DONE"`
  (28 checks, all pass: 6.7–7.4 kn at every wind angle, the wake and trail data, slowing with the sail in, anchor
  stop, 22 m turning circle with the steering oar, pier and beach collisions, player aboard, helm only steers, rope points, windlass, ladders up and
  down, Rough and Storm seas, mooring).

## Controls
| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move | W A S D / arrow keys | Left stick |
| Look | Mouse | Right stick |
| Sprint | Left Shift (hold) | Left stick click |
| Jump / swim up (under water you hover when no key is pressed) | Space | A / Cross |
| Dive (while swimming) | Left Ctrl / C | B / Circle |
| Interact / leave station (raising the anchor: hold E) | E | X / Square |
| Use what is in hand (D80): compass close, spyglass to the eye, lantern lifted, a stroke of the spade; hold on a dig spot to dig | Left mouse button | Right trigger |
| At a station | W/S and A/D are routed to the station | Left stick |
| Pause menu (D66: Resume, Save now, Voyages, Settings, Return to the title, Quit to desktop) | Esc (in the editor: F10) | Start |
| In the menus (D66) | W / S or the arrows choose, A / D change, E / Enter use, Esc or right click back, Q / E turn the settings' pages; hold X on a voyage to scrap it; the mouse hovers and clicks | D-pad, A use, B back, bumpers turn pages, X (hold) scrap |
| Satchel | Tab or I (arrows or the mouse choose a pocket, drag an item to another pocket, R turn the item, G drop one; Tab, I or Esc close) | Back |
| Tool wheel | Hold Q, aim with the mouse, let go to take the tool; Esc cancels; 1 to 5 take a tool directly | Hold LB, aim with the right stick; B cancels |
| On the boat | Steering oar (starboard quarter, E at its handle): A / D steer, W / S let the sail out or take it in. Pin rails by the shrouds: E lowers (sets) / raises the sail. Windlass on the foredeck: E casts off, moors, drops / raises the anchor. Bell rope: E rings the bell; lantern: E lights / douses it. Chart on the mast (aft, to starboard): E looks at it closely (E / Esc put it back). Ladders (both sides, midship): swim or walk into one (or E) to take hold, W / S climb, E lets go; off the bottom into the sea, over the top on board | |
| Read the letter (D85) | L (E, Esc or L put it away) | |
| Talking (D81: E at a person) | E finishes the line being written, then gives the plain answer; 1 / 2 / 3 the numbered ones; walk away to leave | X; d-pad up / right / down |
| Store (barrel or crate on the boat, E at its lid) | Click moves the stack, Shift-click one; drag to a chosen slot on either side (Shift: one); arrows choose, Enter moves the stack; E / Esc close | D-pad, A (the stack), X (one), B close |

Developer console (`~`, non-shipping builds):
- `sb.Lighting Day|Evening|Dusk|Night|Dawn|Morning|Twilight` (jumps the clock there and holds it, D72)
- D72: `sb.Time` (print), `sb.Time <hour 0-24> [hold]`, `sb.Time run|hold`, `sb.Time day <n>`, `sb.Time.Scale <x>`, `sb.Sky.Dome 1|0`,
  `sb.Dev.RunClockTests`
- D85: `sb.Story.Letter` (read it), `sb.Story.Letter dry` (dry it at the fire pit), `sb.Story.Letter give`
- D81: `sb.Talk <Speaker>` (talk to them), `sb.Talk answer | choose <1-3> | end | call <CallId>`, `sb.Talk.Reload` (read Lines.json again,
  print its problems), `sb.Dev.RunTalkTests`
- D73: `sb.Dev.RunDigTests`, `sb.Dev.GiveTool <Tool> [0]`. D74: `sb.Dev.RunRepairTests`, `sb.Boat.Hull hole <x> <side> <size> | water <kg> | tear | repair | info | spots`,
  `sb.Boat.Damage 0|1`
- `sb.Region ShoresOfPlenty|AncientIsles|Wilds|DevilsRoar [blend s]`, `sb.Weather Clear|Cloudy|Overcast|Storm [blend s]`,
  `sb.SeaState Calm|Moderate|Rough|Storm [blend s]`, `sb.Env` (print the composed state)
- `sb.SeaState Flat|Calm|Moderate|Rough|Storm [blend s]` (Flat = windless biome)
- `sb.Ocean.Freeze 0|1`, `sb.Ocean.Time <s>`, `sb.Dev.Camera X Y Z Pitch Yaw [FOV] [Float]` (`sb.Dev.Camera off` returns to the player)
- `sb.Dev.DumpFrames <n>`: the renderer writes the next n frames to `Saved/Screenshots` (with `-UseFixedTimeStep -FPS=<n>`, see `Tools/record_clip.py --dump`)
- `sb.Dev.RunMovementTests [motion speed turn]`, `sb.Dev.Platform <motion> [speed turn]`
- `sb.Dev.Benchmark [s]`, `sb.Dev.Perf [s]`, `sb.Dev.Delayed <s> <cmd>`
- Menus (D66): `sb.Dev.RunMenuTests`, `sb.Dev.Menu <title|pause|settings [page]|voyages|keys [listen|clash]|first|ask|loading [s]|close>`,
  `sb.Dev.MenuShots [quit]` (every screen to `Saved/MenuShots`), `sb.Dev.MenuFlow [quit]` (title, new voyage, save, voyages, back
  to the title; uses the real voyage slots, so run it in a test install), `sb.Dev.KeyShots [quit]` (prompts with other keys),
  `sb.Dev.DrawnKeyLetters 1` (draw every key letter at run time, to compare with the painted ones)
- D80: `sb.Dev.Use 1|0` (the left mouse button on the player), `sb.Arms.World 1|0` (draw the arms and tools in the world, to look at them
  from outside with `sb.Dev.Camera`), `sb.Tools.DigPhase <0..1>|-1` (hold the spade at a stroke's phase). D84: `sb.Music Calm|Night|Drama|off|auto|next|info`.
  `Tools/pie_shot.py <dir> "<cmd>; @wait s; @shot name; @use 1"` (frames from play).
- `sb.Dev.ResetPlayer`, `sb.MouseSensitivity <x>`, `sb.PromptOnObject 1|0`, `sb.Dev.ShowInteractionVolumes 1`,
  `sb.Dev.RunSaveTests` (test slot; the player's own save is untouched), `sb.Dev.RunInventoryTests`,
  `sb.Dev.Give <Item> [Count]`, `sb.Dev.Satchel [off]`, `sb.Dev.Store crate|barrel [demo [t]]|off`, `sb.Dev.Landfall <Id> [t]|reset`, `sb.Dev.Wheel 1 [n]|0`, `sb.Dev.Chart [off]`, `sb.Dev.Tool <Compass|Spyglass|Lantern|none>`, `sb.Dev.CameraTrace [s] [deck]`, `sb.Dev.EdgeTest [s]` (the view beside the deck's low steps in the open sea), `sb.Boat.RigTurn 0|1`, `sb.Camera.DeckKnees 0|1`, `sb.Camera.DeckTilt <share>` (0 = level view), `sb.Camera.StepSmooth 0|1`
- `sb.Wind <FromDeg> <Speed m/s> [Gustiness]` (no arguments: print the wind)
- `sb.Dev.RunBoatTests [part]` (a part alone: `ladder`, `WindAbeam`, ...), `sb.Dev.Helm [off]`
- Hub blockout (D61, on `L_HubBlockout`): `python Tools/hub_blockout_checks.py [headroom] [walk | walk:<space>] [boat] [water]`
- Hub (D63): `sb.Hub.Show 1|0|-1`, `sb.Hub.Berth <id>`, `sb.Hub.Unlock <L#>|all|none`, `sb.Hub.Info`, `sb.Hub.Hoist <stop> [now]`,
  `sb.Hub.Doors open|close|toggle|snapopen|snapclosed|watchon|watchoff` (D64; the sea gate is gone), `sb.Dev.Salt <n>`,
  `sb.Dev.HubGoto dock|slip|tally|well|keelgate|main|chandlery|quarterdeck|poop`,
  `sb.Dev.RunHubTests [placed|sealow|nolowfloor|headroom|route|gates|boatdock|slipway|hoist|follow|save|ladder]`
  (`python Tools/run_pie_test.py "sb.Dev.RunHubTests hoist" --done "SB_TEST HubSuite DONE"`)
- `sb.Boat.Reset` (back to the mooring), `sb.Boat.Recover` (upright at the last safe position), `sb.Boat.Info`
  (SB_BOAT state line), `sb.Boat.Place X Y Yaw [Speed m/s]`, `sb.Boat.Hold Free|Moored|Anchored`,
  `sb.Boat.Control <Rudder> <Sheet> [Sail 0|1]` / `sb.Boat.Control off`, `sb.Dev.RunBoatTests`
- `sb.Dev.Teleport X Y Z [Yaw] [Pitch]` (feet position), `sb.Dev.Input <Forward> <Right> [Up] [Dive]` (simulated
  input), `sb.Dev.WaterState` (logs `SB_WATER`: movement mode, immersion, surface, camera height above the water)

## Known issues and limitations
- **The save sign's fade-in (inventory suite, `SaveSignShows`):** fails every run (opacity 0.66 at 0.3 s, 1.00 at 1.3 s; no
  earlier result recorded, D66 did not run the suite). The save itself takes 1 ms and the frame after it 48 ms, and the fade-out check
  passes on time, so about 0.17 s of the fade is lost right after the save; not found yet (the picture D66 takes with each save is the
  first suspect). Not caused by D73/D74, which do not touch the HUD or the sign.
- **Menus (D66):** the game has no sounds yet, so the Music, Sea and wind and Effects volumes have nothing to act on until sounds
  are added with their classes. The packaged game now opens on the first start (once) and the title; it was checked in a
  standalone run of the editor build, not yet in a packaged build. Open items: Next steps 8 and 9.
- **Performance:** revision 2 measured in standalone, 2560×1440 native, editor closed:
  - Day 75.7 fps (GPU 12.5 ms), Evening 69.6 fps (GPU 13.6 ms), Dusk 72.1 fps (GPU 13.1 ms).
  - This is within D8 (60 fps), but the headroom shrank from about 2× to 1.2×.
  - Revision 3, same setup: Day 82.5 fps (GPU 11.4 ms, p95 13.0), Evening 81.3 fps (GPU 11.6 ms),
    Dusk 72.4 fps (GPU 13.1 ms). This meets D8. Fewer cloud pixels and the Nanite boat section offset the
    bigger swell.
  - Ocean revision 5 (D23–D26), same setup: Day 92.2 fps (GPU 10.1 ms, frame p95 11.5 ms), Evening 84.2 fps
    (GPU 11.2 ms), Dusk 84.3 fps (GPU 11.2 ms). The water shader grew to 1540 instructions with the
    second (previous-frame) wave evaluation in the vertex shader. Overall the build is faster than
    revision 3. It was not profiled pass by pass; one known saving is that Lumen screen traces are off.
  - The hub (D63), same setup, RTX 4070 SUPER: the pier looking at her 74.4 / 69.5 / 73.9 fps (Day / Evening / Dusk; her hidden
    75.1 / 69.4 / 73.6), the dock 70.7 / 71.3 / 72.4, the main deck 75.8 / 74.9 / 78.3, the Keelgate 75.8 / 75.3 / 75.5.
    This is also the first packaged build with the FFT ocean and the underwater pass (the cook had failed since D27).
- The Jacobian foam trail samples the dominant wave direction only.
- **Ocean (D27):**
  - No foam and no spray (removed at the owner's request; a foam system needs a new attempt).
  - The CPU mirrors only the wind sea's long waves (≥ ~12 m): small floating objects ignore shorter waves.
  - The FFT tiles repeat every 250 m (hidden by the swells and the chop patches; visible only from far above).
- **Ocean (D26):**
  - The ocean does not reflect itself: it is not in the ray-tracing scene, and horizon occlusion only
    approximates this.
  - No spray yet (`SprayAmount` is unused).
  - Cloud positions follow wall-clock time, so captures minutes apart reflect different clouds.
- **Underwater (D28):**
  - No god rays, bubbles or particles yet. The underside does not show objects above the water (boat hulls
    above the waterline) through Snell's window.
  - Sea grass ignores the player (no bending). Caustics are emissive, so they also show in shadows under the
    pier.
  - Performance with the underside tiles and the underwater pass has not been benchmarked in a packaged
    build yet (also outstanding for the FFT ocean itself).
- **Vsync frame pacing:** with vsync on, the standalone build ran at about 55–60 fps even though the GPU needed only
  about 7.5 ms. Since D44 the default is vsync off with a frame cap at the display's refresh rate (settings menu);
  not yet measured in a packaged build.
- **Boat wake (D44, D45):** rendering only: swimmers and floating objects do not feel the bow wave. Only the player
  boat makes a wake. The trail is a polyline of 15 one-second samples (sharp turns at speed show slight corners).
- **Benchmarking:** always close the editor first. A running editor renders in the background (throttling
  is disabled for automated tests) and halves standalone performance.
- **Water inside hulls:** masked inside the player boat's hull (D42); other hulls (the test deck section) are not.
- **Test deck section:** it is a slice of the boat with flat end caps (not a full hull), and its deckhouse
  is a shed-like 95 cm box.
- **Floater:** `ASaltDevFloater` (the moored test deck section) is a simple spring-fitted floater, not the M4
  boat model.
- **Sailing boat (D36):**
  - Open deck (no cabin until decision 2). Halyards are not modelled as working ropes; the climb has no body
    animation (first-person camera only, no hands yet).
  - Heavy and stiff: half a minute to full speed. Tunables are on the actor (Boat category) for tuning by feel.
  - Ignores short chop (the CPU mirrors only the long waves).
  - The boat does not push a swimmer away; a thin slab hit at bulwark height can make the bow ride up.
- About 0.25 cm vertical camera pops on heaving decks (CharacterMovement floor correction), below the visible threshold.
- Pressed against the bulwark by the pin rails in a rough sea, the rolling deck can still lift the body onto a rail's foot now
  and then (D58); the view glides through it (at most 3 cm a frame). With the flat base the feet ride 1-4 cm higher on a
  steeply tilted deck (its corner rests on it).
- Mouse sensitivity has not been tested by a human (`sb.MouseSensitivity`). The prompt key label is fixed to "E".
- No sounds yet besides the music (D84). The player has arms and hands (D80) but no body.
- `BoardingRamp` in `L_DevTest` is not aligned. Template `LevelPrototyping` content is still in use for greyboxing.

## Next steps
0. **The game concept (D67, D68):** the owner's answers to the new questions in `Docs/Design/GameConcept.md` (section 12), then M6, the first playable hour (its section 10.2): the prologue with the buried canvas and the Spire,
   a running day and night, the first people, the log, materials and digging, jobs and salt, three upgrades built from materials, boat damage and the squall, the first eerie night, music by mood.
1. Owner review of M3 revision 2 (`Docs/Evidence/M3_rev2/`). Remaining checkpoint decisions: cabin
   concept, hull colour, sea state.
2. Owner feedback on revision 3 (`Docs/Evidence/M3_rev3/`); then M4 sailing (wind subsystem is in place).
3. **M4:** owner feedback on the sailing feel (speed, heel, turning, controls, D36). Then the boat art after
   the decisions; water masking inside the hull if the cabin goes below the waterline; sail and rope visuals;
   a wind indicator (flag or telltale).
4. M4 art after the decisions: full starter boat in Blender (about 10 m double-ender, 3.4 m beam), interior, sail.
5. Water polish: spray at crests (`SprayAmount`), verification captures of all
   region × weather × time-of-day combinations, curved (non-Gerstner) crests against the remaining banding.
6. Packaged benchmark with the D44 defaults (frame cap), the masked ocean and the wake (editor closed).
7. Underwater after the owner's review: god rays and bubbles (the waterline split came with D79), sea grass
   bending around the player, a packaged-build benchmark of the FFT ocean plus the underwater rendering.
8. **Menus (D66), accessibility settings:** hold actions as press-to-toggle (raising the anchor, the tool wheel), text size,
   subtitles (with a parchment backing) and seasickness comfort (a steady horizon, less roll with the boat, a soft vignette in
   sharp turns), each a real setting on its page (the menu page's ideas, `Docs/Design/Menu_Ideas/Menus.html`).
9. **Menus (D66), smaller voyage pictures:** a save's picture is the whole screen as a PNG (about 3 MB at 1440p, loaded when the
   voyages page opens). Scale it down to the page's print (about 460 x 260, a JPEG or a small PNG) when it is written.
