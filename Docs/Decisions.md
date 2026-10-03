# Decisions

Significant decisions, newest last. Format: finding → decision → consequences/trade-offs.
Status: **Adopted**, **Provisional** (pending an experiment), or **Superseded**.

---

### D1 · Repository layout (Adopted, M1)
- **Finding:** The Unreal project sits in `Saltbound_all/Saltbound`. Blender sources, tools and documents
  belong next to it, not inside `Content/`.
- **Decision:** One Git repository at `Saltbound_all/`, containing the Unreal project in `Saltbound/` plus
  `Art/`, `Tools/` and `Docs/`. Git LFS for all binary formats; generated folders and IDE files ignored.
- **Consequences:** Source art and engine content are versioned together. FBX exports are regenerated
  from scripts and not versioned.

### D2 · Template clean-up (Adopted, M1)
- **Finding:** The First Person template shipped Horror and Shooter variants (code, content, AI/StateTree
  dependencies) unrelated to this game.
- **Decision:** Removed the variants and their dependencies. The basic first-person template classes and
  content stay temporarily as a working fallback until our own player replaces them (M2).
- **Consequences:** Smaller project and faster builds. Fully reversible through the baseline commit.

### D3 · Editor automation path (Adopted, M1)
- **Finding:** The UE 5.8 MCP plugin (Experimental) provides viewport capture, logs, PIE control and asset
  tools, but it has no general Python execution. The PythonScriptPlugin's remote execution does.
- **Decision:** Use Python remote execution (`Tools/ue_exec.py`) for content creation and setup scripts, and
  MCP (`Tools/ue_mcp.py`) for captures, logs and PIE. Both are enabled in project config. All editor
  changes are made by versioned scripts in `Tools/ue_scripts/`, so they are repeatable.
- **Consequences:** Python remote execution listens on localhost multicast only (bound to 127.0.0.1).
  Viewport captures must move the real viewport first. Capturing from an unvisited pose uses stale
  occlusion data and drops meshes; this was observed and fixed.
- **Note:** The MCP plugin logs that engine data passed to an LLM counts as Licensed Technology under the UE
  EULA §6(e). The project owner should make sure their LLM provider settings do not allow training on it.

### D4 · Keep Substrate enabled (Adopted, M1)
- **Finding:** The template enables Substrate. Python-authored materials using classic inputs (BaseColor,
  Roughness, …) compile and render correctly; Substrate converts them automatically.
- **Decision:** Keep Substrate (engine default, forward-compatible) and author materials with classic inputs.
- **Consequences:** The option of Substrate-specific layering (varnish, wet wood) stays open. Disabling it
  later would only require a shader recompile.

### D5 · Blender → Unreal import via Interchange with a scripted pipeline (Adopted, M1)
- **Finding:** UE 5.8 imports FBX through Interchange. Several legacy pipeline properties are deprecated.
- **Decision:** `import_assets.py` builds a transient `InterchangeGenericAssetsPipeline` per asset: static
  mesh only, UCX collision by name, no material creation, and slot materials assigned by name. Every
  generated mesh gets box-projected UVs, because a missing UV0 produces degenerate tangents.
- **Consequences:** Imports are repeatable and print verification data (size, collision, slots).

### D6 · Custom shared-Gerstner ocean instead of the engine Water plugin (Adopted, M3)
- **Finding (spike, 2026-10-03):** The Water plugin is still *Experimental* in 5.8 and built around
  Landscape. Without one, the automated setup rendered broken surfaces even after adding a water zone, a
  seabed terrain component and extents (evidence: `Docs/Evidence/M3_WaterPluginSpike_NoLandscape.png`).
  Its CPU wave query approximates the horizontal inversion per wave and uses float time. Its buoyancy
  expects physics-simulated bodies.
- **Decision:** Custom ocean. One wave set (up to 8 Gerstner waves plus 4 calm zones) is evaluated by
  `FSaltWaveField` (C++) and `Saltbound/Shaders/SaltOcean.ush` (embedded in `M_Ocean`). Phases are
  computed in double precision on the CPU and pushed through `MPC_Ocean` each frame. Geometry is a CDLOD
  quadtree of instanced grid tiles with vertex morphing (no cracks, no swimming), using Single Layer Water
  shading, analytic per-pixel normals, two ripple layers, and Jacobian crest foam plus depth-based shore foam.
- **Evidence:** It worked on the first render. CPU-positioned probe spheres stay half-submerged across
  wave phases, and the high oblique view shows no cracks.
- **Trade-offs:** No built-in underwater rendering, rivers or shoreline tooling (not needed now). The two
  wave implementations must be kept in sync. Rendering-only distance fade of short waves means distant
  surfaces differ slightly from CPU queries; this is irrelevant for gameplay near the viewer.

### D7 · Boat motion model: kinematic deck + CharacterMovement based movement (Adopted, M2)
- **Decision:** The boat is a kinematic actor driven by our own integrator. Heave, pitch and roll come
  from wave samples through damped springs; surge, sway and yaw come from sail, keel and rudder forces.
  The player uses CharacterMovement's based movement on the deck; the character ticks after its base.
- **Evidence (M2):** On a kinematic test deck (up to 1 m heave, 16° roll, 6 m/s, 25°/s turning), a
  standing player drifts 1 cm or less over 12 s. Camera jitter RMS is 0.06 cm or less, there is no
  sliding after stopping, and the player never leaves the deck.
- **Additions required:** Kinematic decks report angular velocity (`ISaltKinematicBase`) so leaving the
  deck imparts the tangential velocity. `bStayBasedInAir` keeps jumps in the deck's frame; without it a
  jump on a turning deck landed 24–66 cm off.
- **Why:** Gameplay feel stays controllable, stability is deterministic, and the tick-order lag of
  physics-simulated bases is avoided. Physics collision response (rocks, dock) must be handled by our
  integrator (sweeps), which is the main cost of this choice.

### D8 · Performance target (Adopted, M1; re-measure each milestone)
- **Hardware:** Ryzen 7 7800X3D (8C/16T), RTX 4070 SUPER (12 GB), 32 GB RAM, 2560×1440 @ 165 Hz display.
- **Target:** Stable 60 fps in a standalone Development build at 2560×1440, with Lumen GI/reflections
  (hardware ray tracing), Virtual Shadow Maps and TSR at about 67–75 % screen percentage, on the Epic or
  High scalability level. Editor and PIE numbers are recorded separately and are not used for sign-off.

### D9 · Input, HUD and interaction defined in code (Adopted, M2)
- **Decision:** Enhanced Input actions and mappings are created at runtime (`USaltInputConfig`). The HUD
  builds its widget tree in C++. Interactables are box components on a dedicated `Interaction` trace
  channel, and several can live on one actor. Control stations are interactables that the player steps into.
- **Why:** Bindings and HUD stay reviewable in one place without binary assets, and each part is easy to
  test automatically.
- **Consequence:** A key-rebinding menu will need either player-mappable key settings on these runtime
  actions or a migration to input assets.

### D10 · Legacy input scales disabled (Adopted, M2)
- **Decision:** `bEnableLegacyInputScales=False`. Look sensitivity is explicit: degrees per engine-scaled
  mouse count, times `sb.MouseSensitivity`. Positive pitch input looks up.

### D11 · Physically based lighting units (Adopted, M3)
- **Decision:** The sun is a real 90 klx and the sky atmosphere attenuates and colours it, so presets mostly
  change the sun angle. Lanterns are in candela (28 cd). Auto-exposure ranges are set per preset.
  `r.EyeAdaptation.CachedLightingPreExposure=8` (DefaultEngine.ini) keeps Lumen's cached lighting unclipped
  across EV -4..16.
- **Why:** Day, evening and dusk are consistent automatically, and lantern light reads correctly at dusk.

### D12 · Procedural textures + vertex masks instead of unique texture sets (Adopted, M3)
- **Decision:** All current textures are generated by `Tools/textures/gen_textures.py`. They are original
  and tileable, so no licences are involved. Detail and variation come from vertex masks baked in Blender:
  edge wear on bevel faces, waterline grime, and per-part variation.
- **Finding:** Vertex convexity cannot be used for edge wear on low-poly boxes, because their flat faces
  have no interior vertices and the wear smears across the whole face. Bevel-face detection is used instead.

### D13 · Boat proportions (Adopted, M3; cabin concept pending owner decision)
- **Finding:** The 70 cm minimum walkway (player capsule 68 cm) rules out a narrow boat with a cabin.
- **Decision:** A beamy double-ender, about 10 m long with 3.4 m beam (Colin Archer spirit), with 88 cm
  side decks beside a 1.5 m cabin.

### D14 · Ocean revision after owner feedback (Adopted, M3)
- **Feedback:** "Waves are missing, needs a bit more realism; Sea of Thieves water hits the spot."
- **Changes:**
  - 12 wave components; amplitudes x1.5 in the test scene; gentler harbour calming (factor 0.5).
  - Translucent crest light driven by the atmosphere-attenuated sun.
  - Short waves faded by grid cell size (no geometric aliasing); finer LOD0 (25 cm).
  - Ripple detail out to about 250 m; roughness rising with distance.
- **Cost:** none measurable (GPU about 7.2-7.7 ms before and after, editor closed).

### D15 · Script-owned materials are rebuilt in place (Adopted, M3)
- **Finding:** Deleting a referenced, loaded material fails. `delete_all_material_expressions` removes only
  part of the graph per call in 5.8.
- **Decision:** `sb_ue.reset_material()` repeats the clear until the graph is empty, then rebuilds it. All
  references from instances and placed meshes stay intact.

### D16 · Art direction: stylised and crisp, detail lives in hand-painted materials (Adopted, M3 revision 2)
- **Owner feedback (2026-10-03):** "The water doesn't look like good quality; the objects are missing
  detail, NOT realism." Then: "Maybe detail was the wrong word. The carvings in the stones look horrible.
  I mean the material detail, with no blur and visibly readable material." References: Sea of Thieves
  rocks, deck and sea.
- **Decision:**
  - Shapes stay simple and chunky, with flat facets and soft bevels.
  - Surface quality comes from crisp, hand-painted tiling textures at high texel density (2048 px per
    1–2.5 m tile, about 0.5–1.2 mm per texel). These have flat tone patches with hard edges, painted
    cracks and chips, and straight grain with knots. They are not soft noise, and the detail is not carved
    into the geometry.
  - The renderer runs at native resolution with light sharpening (D18).
- **Tried and rejected:**
  - Carved rocks (Voronoi plates, crack grooves, strata ledges): read as busy and artificial.
  - Chiselled facet normal maps: polygon mosaic.
  - Soft painted noise: blurry, "fur".
  - Contour-line ("cathedral") wood grain: looked like a map.
- **Consequences:**
  - `sb_rock.py` keeps the carving options, but they are off by default.
  - Textures stay procedural (`gen_textures.py`, D12), but are designed as painted shapes.

### D17 · Native classes resolve /Game content lazily (Adopted)
- **Finding:** `ConstructorHelpers` in `ASaltOcean`/`ASaltLantern` loaded `MI_Ocean`, `M_Ocean` and the surface
  materials while class default objects were built at engine start-up. Objects loaded before the
  disregard-for-GC pool closes are added to the root set. `setup_ocean.py` then crashed the editor when it
  deleted a rooted expression.
- **Decision:**
  - Constructors only create components.
  - Default assets are resolved in `OnConstruction`/`BeginPlay` (`Core/SaltContent.h`), and the placed
    instance stores the reference, so the cooker sees it.
  - `/Game/Saltbound/Water` and `/Game/Saltbound/Props` are additionally always cooked.

### D18 · Native screen percentage (Adopted, re-measure pending)
- **Finding:** With the display-based default, the game rendered at ~61 % of 1440p and upscaled with TSR. That
  softened the painted detail the owner asked for.
- **Decision:** `r.ScreenPercentage.Default.Desktop.Mode=0` and `r.ScreenPercentage.Default=100`, plus
  `r.Tonemapper.Sharpen=0.6`. Motion blur is off by default.
- **Cost:** about 2.7× the shaded pixels. The last measured GPU time was 7.2–7.7 ms at 61 %, so this is
  expected to stay within D8. It must be re-measured in a standalone build with the editor closed.

### D19 · Ocean look v3: Sea of Thieves palette and lace foam (Adopted)
- **Body:** deep blue (scattering 0.008/0.04/0.1, absorption 1.4/0.36/0.15). Turquoise appears only as crest light,
  from backlit thin crests.
- **Foam:** one lace texture (graded filaments), with coverage eroding the lace:
  - Whitecaps from the Jacobian, plus a trail sampled down-wave.
  - Faint marbled streaks on the open sea.
  - Shore foam with lapping bands of varying width.
  - Foam stays translucent (40–88 %).
- **Surface:** three ripple layers (a fine near layer stops calm water turning into a mirror), crisper ripple
  normals and roughness 0.028 near the viewer, for the sun glitter of the reference.
- **Cost:** pixel shader 1458 instructions (three Jacobian evaluations). Measure with D18.

### D20 · Hand-made timber module (Adopted)
- **Owner brief (2026-10-03):** the dock should look like a stylised, weathered, hand-made pirate dock:
  chunky planks with irregular profiles, gentle bends and twists, uneven edges, chipped or split ends,
  varied worn bevels, broad grain grooves, rough tapered posts, and several distinct variants.
- **Decision:** `Art/Blender/Scripts/sb_timber.py` builds every board and post individually from its own seed.
  - `rough_plank`: a subdivided box with width and thickness drift, bend, twist, wobbly long edges, cupping,
    2–4 broad grain grooves and local worn chamfers. One end may be slanted, chipped or split. The bevel is
    either rounded or angular. U runs along the board, so the grain follows it.
  - `rough_post`: an irregular polygon section with taper, uneven faces and a sloping worn top.
  - Used by the dock, the crate and the boat deck section. The boat uses less wear: it is cared for.
- **Finding:** On densely subdivided boards, the edge-length rule in `bake_vertex_masks` marked every narrow
  face as a bevel. This painted leopard-like wear blotches across whole boards.
  `sb_blender.soften_marked()` now tags the real bevel faces (face attribute `sb_bevel`), and the mask bake
  uses that tag for tagged meshes.

### D21 · Stylised realism pass: lighting, sky, grading (Adopted)
- **Owner feedback:** "Our graphics sometimes look too realistic. Sea of Thieves is a bit more comic."
  The owner wants simplified, expressive geometry with convincing light and movement.
- **Research:**
  - Rare's GDC 2018 talk: hand-painted textures; simplified structures combined into complex scenes; wear,
    patches and repairs.
  - SIGGRAPH 2018 "The Technical Art of Sea of Thieves": water colour blends deep and subsurface colours by
    view angle, sun direction and a wave-peak mask; foam is softened and blended with hand-painted
    textures; an area sun highlight; stylised, cartoon-edged clouds.
- **Decision (lighting presets, `ASaltLightingRig`):**
  - Golden sun (Day 5400 K) with a 2.2° disc. This gives a large soft sun reflection and softer shadow edges.
  - Cool sky fill (tint 0.8/0.92/1.15) for blue shadows.
  - Deeper blue sky: Rayleigh scale 0.045, Mie scale 0.0018.
  - Low-lying blue haze (fog falloff 0.2) that fades distant islands into the horizon and keeps the sky
    blue overhead.
  - Grading: saturation 1.12, contrast 1.08, cool shadows, warm highlights.
- **Clouds:** `MI_Clouds_Stylized` (`Tools/ue_scripts/setup_sky.py`) is an instance of the engine's simple
  volumetric clouds with less coverage, denser and larger cumulus masses, and little fine erosion. It must
  inherit from `m_SimpleVolumetricCloud_Inst`; the base material lacks its texture overrides and renders
  no clouds.
- **Shapes:** the boat section, crate and dock use chunky, slightly crooked timber (D20). Wood grain is
  broader. Cracks on the rocks are fewer and longer.

### D22 · Ocean v4: big swell, crest light by luminance, no seabed in reflections (Adopted)
- **Waves:**
  - Three long swells (68/47/33 m) from neighbouring directions interfere into irregular groups.
  - Smaller waves layer on top. The steepness sum is 0.975, giving narrow crests.
  - These are the C++ defaults of `ASaltOcean`. `L_VisualTest` uses amplitude 1.6 on the open sea and calms
    the cove to 15 %.
- **Shading:**
  - Ripples come from a flowing, ridged normal map in three rotated layers that travel across the swell.
  - Crest light appears only on thin, backlit crests. It is driven by the sun's luminance, not its colour:
    an amber evening sun times teal turned the sea green.
  - Breaking whitecaps are patchy, with drifting trails.
- **Finding:** Steep wave faces reflect rays downward. Hardware-traced Lumen reflections then hit the sand
  test seabed 18 m below and show beige streaks, which were first mistaken for foam and fog. The seabed is
  hidden from ray tracing and distance fields.
- **Lesson:** isolate such colours by switching inputs off one at a time (specular, then sun, then
  geometry) instead of guessing.
