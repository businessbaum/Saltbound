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

### D23 · Ocean smearing: motion vectors for wave displacement (Adopted)
- **Owner brief:** the moving sea smeared. Diagnose with controlled experiments before changing settings.
- **Experiments** (`Tools/ocean_diag.py`, fixed camera and ocean clock; `sb.Ocean.Freeze`, `sb.Ocean.Time`):
  - Waves frozen: the temporal smear disappeared. Some ribbon-like streaks stayed on steep wave faces, so
    part of the artefact was not temporal. That part turned out to be reflections (D26).
  - Motion blur was already off (D18), so it was not involved.
  - The velocity visualiser showed zero motion vectors on the water. The vertex shader displaces the
    tiles with world-position offset, and nothing told TSR where each pixel had been in the previous frame.
    TSR therefore reprojected stale history onto moving water: smeared ripples and reflections.
  - Lumen screen-traced reflections traced against the water's own depth at grazing angles. This produced
    stair-stepped blocks in the reflections that moved with the waves.
- **Decision:**
  - The material evaluates the wave field twice through `PreviousFrameSwitch`, with the previous frame's
    phases (`MPC_Ocean` PrevPhase*, written by `ASaltOcean::PushParameters`). TSR now gets correct
    motion vectors. `UsePrevFramePhases = 0` restores the old behaviour for A/B tests.
  - The water writes TSR temporal responsiveness 0.3 (`r.Velocity.TemporalResponsiveness.Supported=1`).
    TSR then rejects history on the water more readily, without affecting the rest of the image.
  - Lumen reflection screen traces are off in the rig's post-process; hardware ray tracing alone gives
    clean reflections.
- **Tools:**
  - `sb.Dev.Camera X Y Z Pitch Yaw [FOV] [Float]` gives a fixed or water-following camera without the HUD.
  - `Tools/record_clip.py` records the uncooked game with ffmpeg ddagrab.

### D24 · Ocean wave hierarchy: three swells, patchy chop, foam history (Adopted)
- **Waves (C++ defaults of `ASaltOcean`):**
  - Three long swells (74/53/39 m) carry most of the height and of the horizontal displacement.
  - Medium waves (21–6 m) are visible but subordinate.
  - Short waves (under 4 m) are mostly shading detail.
  - The steepness sum is 0.995, so the surface does not loop.
- **Chop patches:**
  - Waves above `SwellCount` (3) are scaled by a slow three-sine field (`ChopVariation` 0.6,
    `ChopPatchSize` 260 m). Wave faces are calmer in some areas and choppier in others instead of
    uniformly crumpled.
  - The field is identical on the CPU (`FSaltWaveField::ChopFactor`) and the GPU (`SaltOcean.ush`), so
    buoyancy matches the picture.
- **Foam:**
  - Whitecaps come from the Jacobian.
  - Two time-shifted Jacobian samples (`JacobianAt`, 0.9 s and 2.2 s back) leave decaying trails behind
    breaking crests.
  - Breaking is patchy (`BreakPatchiness`), so calm water shows no whitecaps (Calm sea state:
    `WhitecapAmount` 0).
- **Bounds:** ocean tile bounds now cover the largest displacement (`bounds_extension` [26, 26, 600]). The
  old bounds let tiles at the screen edge be culled while their displaced waves were still visible.

### D25 · Environment layers: time of day, region, weather, sea state (Adopted)
- **Owner brief:** regional looks after the Sea of Thieves regions (Shores of Plenty, Ancient Isles, the
  Wilds, the Devil's Roar), with region, depth, weather, time of day and sea state adjustable separately,
  and smooth transitions.
- **Decision (`ASaltLightingRig`):** four independent layers.
  - Time of day (`sb.Lighting`).
  - Region (`sb.Region`). Water absorption, scattering, crest light and foam colour are absolute values;
    sky, fog, light and grading multiply the time-of-day preset, so every region works at every time of
    day.
  - Weather (`sb.Weather`: Clear, Cloudy, Overcast, Storm).
  - Sea state (`sb.SeaState`: Calm, Moderate, Rough, Storm). It drives wave height and steepness, chop,
    whitecap threshold and amount, break patchiness and ripple strength.
  - Every layer blends over time (`BlendSeconds`). `ASaltRegionZone` actors blend regions spatially by the
    viewer's position. `sb.Env` prints the composed state.
- **Implementation:**
  - The rig owns the volumetric clouds as a transient component with a dynamic material instance, so
    weather can change coverage without editing assets.
  - The water look goes to `MPC_Ocean` (LookAbsorption, LookScattering, LookCrest, LookFoam, LookSea).
  - Depth colour comes from Single Layer Water absorption over the real seabed.
- **Not yet implemented:** spray (`SprayAmount` is stored but unused).

### D26 · Ocean reflection breakup: causes and corrections (Adopted)
- **Owner brief:** excessive reflection breakup, long parallel silver streaks, tightly crumpled
  highlights. Diagnose first, change one variable at a time, keep wave geometry, colour, foam, layout and
  lighting unchanged, no blur/sharpen/noise.
- **Method:** frozen ocean (`--time 100 --freeze 1`), identical camera and light; views `15_ref_low`
  (low, towards the sun) and `16_ref_landmark` (low, across the light), plus `13_sea_low` and
  `11_dock_fp`. Each candidate was tested alone, then the fixes were applied cumulatively.
  `Tools/water_metrics.py` measures the water region:
  - `silver`: share of pale, unsaturated pixels.
  - `fine`: Laplacian density, i.e. crumpled detail.
  - `streak`/`horiz`: structure-tensor coherence and the share of near-horizontal edges.
- **Findings:**
  - Reflections carry almost all of the contrast. With Specular 0, the luma std fell from 39 to 11.
  - The silver/brown parallel streaks remained with all ripples off, so they came from the macro normals.
    A debug view of the reflected ray (`DebugView` 1, magenta where the ray points at or below the horizon)
    lined up with the streaks. Where short, straight-crested Gerstner waves tilt the normal away from the
    viewer, the ray reflects below the horizon. The ocean is not in the ray-tracing scene, so these rays
    hit the sky atmosphere's ground or the bright horizon band.
  - The 1.5 m ripple layer alone produced the crumpled glints (fine_near 8.1 with only that layer).
  - The 38 m ripple layer produced large smooth blotches and bands.
  - The elongated ridged ripple texture added streaks of its own.
- **Corrections (`Tools/ue_scripts/setup_ocean.py` defaults; every one is an `MI_Ocean` parameter):**
  1. Horizon occlusion: `Specular` fades to 20 % (`HorizonOccFloor`) as the reflected ray drops from 0.08
     to 0.0 (`HorizonOccTop`/`Bottom`). This is the reflection a real wave face would get from the water
     beside it. `HorizonOcclusion 0` turns it off.
  2. `PSMinWavelength` 500: the pixel-shader normal leaves out Gerstner waves shorter than 5 m. Their
     straight crests drew the parallel lines; the vertex displacement still uses them.
  3. Soft ripple texture `T_OceanRipplesSoft_N` (near-isotropic, `gen_textures.py ocean_ripples_soft`). The
     old `T_OceanRipples_N` is kept.
  4. Ripple layer weights 0.85 / 0.5 / 0.35 (were 1.0 / 0.8 / 1.0).
  5. Quiet patches (`QuietAmount` 0.25, `QuietTile` 45 m): ripple strength drops by up to 25 % in broad
     patches. 0.55 made the water read as a mirror.
  6. Specular anti-aliasing cap `SpecAAMax` 0.015 (was 0.01).
- **Results:**

  | View | silver | fine near | fine far | moving clip flicker |
  |---|---|---|---|---|
  | 15_ref_low | 17.4 % → 18.5 % | 6.97 → 3.25 | 13.5 → 7.1 | 5.28 → 2.82 |
  | 16_ref_landmark | 5.3 % → 2.9 % | 3.22 → 1.58 | 10.1 → 5.2 | 2.07 → 0.94 |

  - In view 15 the silver share stays because that view looks into the bright sky reflection; the brown
    and silver streaks are gone.
  - Coherence (`streak`, `horiz`) rose from 0.73 to 0.85, because the fine isotropic noise is gone and
    the swell's own bands dominate. The metric cannot tell wave bands from reflection streaks, so the
    streak verdict comes from the captures.
  - Moving clips: 8 s at 60 fps with the floating camera (`record_clip.py --float`); seconds 1–5 were
    analysed at 960×540. Flicker is the mean absolute difference between each frame and the average of
    its two neighbours, over the water region (luma 0–255). Smooth wave motion scores low; shimmer and
    popping glints score high.
  - Pixel shader: 1540 instructions. Standalone at 2560×1440 native with the editor closed: Day 92 fps,
    Evening and Dusk 84 fps (GPU 10.1–11.2 ms).
- **Evidence:** `Docs/Evidence/Ocean_rev5/` (`reflection_steps_*` shows each step on its own, starting with
  the magenta horizon debug view).
- **Limitations:**
  - Gerstner crests are straight, so on a long swell face some parallel banding remains.
  - Horizon occlusion is an approximation; the ocean does not reflect itself.
  - Clouds drift on wall-clock time, so cloud reflections differ between captures taken minutes apart.

### D27 · FFT ocean: Tessendorf wind sea on Gerstner swells (Adopted; supersedes the ripple layers of D19/D26)
- **Owner brief (2026-10-03/04):** match two Sea of Thieves references (rough sea; calm water beside a ship),
  including their wave shapes. Medium waves must be carried by the swells, crests irregular and
  interrupted, faces partly steep and partly gentle. Low wind gets its own balance, and a separate
  windless preset. Owner hint: "Sea of Thieves uses FFT ocean simulation with vertex displacement".
- **Decision: GPU FFT ocean** (new module `SaltboundRender`, loading phase PostConfigInit for global shaders).
  - Three cascades with their own tile size and rotation split the spectrum by wavelength:
    - 250 m tile (waves ≥ 6 m).
    - 37 m tile (0.9–6 m).
    - 6 m tile (ripples, shading only).
  - Spectrum: JONSWAP in wavenumber space, cos^2s spreading (Mitsuyasu), a Gaussian hash per lattice
    point, frequencies quantised to a 1000 s loop. It is scaled to a significant height (`WindSeaHeight`).
  - Per frame, compute shaders (`Shaders/SaltOceanFFT.usf`) run the spectrum, a 256-point FFT along rows
    and columns, and a resolve pass.
  - Output per cascade (256² RGBA16F render targets with mips, `/Game/Saltbound/Water/FFT`):
    - Displacement (Dx, Dy, h, Dxy).
    - Derivatives (hx, hy, Dxx, Dyy).
    - Slope moments.
    - Last frame's displacement (motion vectors, D23).
  - The three long Gerstner swells stay analytic and are not periodic, which hides the tile repetition.
- **CPU mirror:** `SaltOceanFFT::FLowBand` evaluates the same spectrum's waves ≥ ~12 m (cascade 0,
  |n|,|m| ≤ 21) with a 64² FFT. Buoyancy and the floating camera follow the visible long waves. Shorter
  waves exist only on the GPU.
- **Material (`setup_ocean.py`):**
  - Vertex shader: swells, plus cascades 0–1 sampled at a mip chosen from the vertex spacing (no
    geometric aliasing).
  - Pixel shader: exact tangents of the displaced surface (swell tensor + rotated cascade tensors), so
    the shading normal is the normal of the moving geometry.
  - Short-wave mask: medium waves and ripples gather on high water and calm down in troughs and in broad
    drifting quiet patches. This leaves smooth areas between the ridges (owner feedback).
- **Findings (one variable at a time, frozen sea, identical cameras):**
  - The large white sun smear came from the sub-pixel slope-variance roughness (LEAN). Bloom was not the
    cause; with near-mirror roughness the smear disappeared. That term is now capped (`SlopeVarScale`
    0.25, `MaxAlpha2` 0.004) and only counts once a texture is minified (`MinifyWeight`). Under
    magnification, bilinear filtering alone made the variance peak between texel centres, a plaid grid
    in the roughness.
  - Long, gentle swells barely read from above. The visible SoT character comes from steep wind waves of
    15–35 m with a peaky spectrum (gamma 5) and sharper major crests.
  - From owner feedback, the major ridges are distinct while medium waves and ripples stay subordinate:
    cascade gains 1 / 0.5 / 0.14, Specular 0.24 ("a bit more smoothing, a bit less reflection").
  - Colour (Shores of Plenty): blue body and deep troughs (absorption 1.35/0.26/0.15, scattering
    0.01/0.065/0.1). Turquoise only in thin, backlit crests (crest light 0.22, starting at 15 % of the
    sea height).
- **Sea states (rig, each with its own balance):**
  - Flat (windless biome, new): no swell, 1.5 cm breath.
  - Calm (low wind): 0.6 swell, Hs 55 cm at 5 m, gentle chop; a glitter path under the sun.
  - Moderate.
  - Rough: 0.7 swell, Hs 2.8 m at 26 m, gamma 5, chop 1.2.
  - Storm.
- **Removed at the owner's request:**
  - Spray particles (instanced billboards).
  - All foam: whitecaps, foam simulation, shore foam. A foam rework following a detailed brief
    (crest-segment breaking chance, accumulation, authored patch texture, fresh/residual debug view) was
    implemented and then dropped. The owner: "we abandon the foam for now … remove the foam completely".
- **Tools:**
  - `record_clip.py --dump`: render-side frames at a fixed time step (`sb.Dev.DumpFrames`), reproducible
    and immune to other windows.
  - `ocean_diag.py`: `--warm` (simulate before freezing) and `--hold` (stay frozen for paired captures);
    `--ocean` accepts 3-component vectors.
  - Shots `17_ref_rough`, `18_ref_calm`, `19_elevated`.
- **Cost:** pixel shader 1359 instructions (was 1540; 36 analytic wave evaluations replaced by 9 texture
  samples). The FFT runs 3 × 256² every frame.

### D28 · Underwater: swimming, view from below, seabed (Adopted)
- **Owner brief (2026-10-04):**
  - "Continue with making the movements and vision for under the water. Create a sea [floor], nothing
    special for now. Just a bit sea grass and stuff."
  - Later, with three Sea of Thieves underwater references: "atmosphere without special decorations. Add
    uneven sand to the ground. add a bit of sea grass. comic style."
  - Also: "remove the lines pattern from every stone texture".
- **Swimming (`USaltCharacterMovementComponent`):**
  - The sea moves, so it cannot be a static water volume. The component samples the ocean surface at the
    character every tick and assigns a brushless, transient water `APhysicsVolume` itself (overlap
    updates are off), with hysteresis: it swims from 60 % immersion and stops below 45 % (wading out).
  - `ImmersionDepth()` drives Unreal's buoyancy (1.2: the eyes float about 10–20 cm above calm water).
  - No constant swim braking: it cancelled the buoyant lift every frame, so swimmers sank and stayed down.
    Fluid friction 2.0 (proportional to speed) damps bobbing and stops horizontal drift instead.
  - Controls: W swims where you look (look down to dive), Space rises, Ctrl / C / B (gamepad) dives,
    Shift swims faster.
  - Climbing out: Space at a ledge (pier deck, boat side, flat rock) runs a 0.6 s scripted climb, up first
    and then onto the ledge, in the ledge's component space so it also works on a moving boat. Ledges are
    found by probing for a walkable top ahead (left and right too, for posts), not by requiring a wall;
    piers are open frames. Reach is 3 m from the feet; the pier deck is 2.6 m above a swimmer's feet.
- **View from below (`ASaltOcean`):**
  - The ocean tiles are drawn a second time by `UnderFullTiles` / `UnderQuarterTiles`. These components
    have a mirrored Z scale, so a negative determinant reverses culling and they show only from below.
    They use the same vertex displacement with `M_OceanUnder` (unlit):
    - Snell's window: sky and sun refracted through the local wave normal, exact dielectric Fresnel.
    - Total internal reflection outside the window shows the lit water body.
    - Comic wave shading: faces turned to the sun are lighter.
  - Both the underside tiles and the post-process pass are switched on only when the camera is within 5 m
    of the surface or below it (CPU estimate).
- **Underwater pass (`M_PP_Underwater`, unbound post-process component on the ocean, before DOF):**
  - It finds the surface above the camera on the GPU, from the same displacement as the vertex shader
    (fixed-point inversion of the horizontal displacement), so it matches the rendered waves exactly.
    The CPU mirror lacks the shorter waves.
  - Model (`Shaders/SaltUnderwater.ush`, shared with the underside):
    - Light lost on the way down: by depth below the water level, along the refracted sun.
    - View fog: by distance.
    - In-scattered light: integrated in closed form along the ray, because the light changes with depth.
      A single average depth turned everything below the horizon black.
  - Extinction follows the region's water (`Look*`). The in-scatter colour is stylised: `UnderwaterColor`
    turquoise (0.12, 0.48, 0.62), with visibility about 25–35 m, like the references.
  - Finding: post-process materials see absolute luminance. The engine removes pre-exposure before the
    pass and reapplies it after, so the in-scatter needs no exposure factor. `View.PreExposure` and
    EyeAdaptation both made it vanish. This was proven with a split-screen test of the three scales.
- **Seabed (`Art/Blender/Scripts/build_seabed.py`, `SM_Seabed`, 180 × 140 m, 1 m cells, not Nanite):**
  - Beach behind the coastal rocks, wadeable shallows.
  - About 2.3 m under the pier, whose posts reach 2.5 m down.
  - A drop-off past the pier end to the 17.5 m floor.
  - The former sandbank block is now a plateau of the heightfield, and ridges rise towards the headlands.
  - Uneven sand: broad mounds plus ripples.
  - Not Nanite, because a Nanite mesh collides with its reduced fallback (879 triangles), which lost
    the ripples underfoot.
  - The small shoreline rocks are now placed on the actual ground by line traces.
- **Caustics:** an emissive term in `M_SB_Rock` (sand, rocks), only below the water level on upward
  faces. Two drifting patterns of a simulated caustic texture (`T_Caustics_M`, rays bent by a wavy
  surface and splatted), combined by minimum, projected along the sun.
- **Sea grass:**
  - Three Blender clumps of chunky curved ribbon blades.
  - `M_SB_SeaGrass`: two-sided, root-to-tip colour ramp, per-clump variation, sway by world position
    offset.
  - 34 patches (398 clumps) at 1.2–14 m depth, kept off the pier and the rocks, using `ASaltInstanceScatter`
    (HISM, culled at 50–70 m).
- **Stone textures:** the crack network and the lit/shaded chip outlines are removed from `T_Rock_*`. Only
  tone patches, faint chips and specks remain; the stone's shape comes from the geometry.
- **Pipeline fix:**
  - `setup_ocean.py` used to recreate every `MPC_Ocean` entry, which gave them new IDs. Any material not
    rebuilt in the same run then failed ("invalid parameter") and rendered the default checker material.
  - Entries are now kept and only new ones are appended. The script takes `ocean` / `under` / `post`
    arguments to rebuild parts.
- **Verification:**
  - `Tools/swim_test.py` (PIE) passes 5/5: enter and float, dive, rise, climb out onto the pier, walk off
    the pier into the sea.
  - Underwater shots 20–24.
  - Evidence: `Docs/Evidence/Underwater_rev1/` (contact sheet, two clips).
- **Cost:**
  - Underside material 532 instructions; post-process 754 (only near or below the surface).
  - The ocean vertex work doubles while the camera is within 5 m of the surface.

### D29 · Underwater review: sand texture, softer caustics, clear shallows, detailed stone, smaller player (Adopted)
- **Owner review of D28 (2026-10-04):**
  1. The sand needs more texture: "too plain, but remember, it's not about realism".
  2. Reflections (caustics) on the sea floor should be a bit weaker and blurrier.
  3. "If the terrain is not even, the reflection should not stretch."
  4. "I can't look through the water surface."
  5. Stones: "some quality is missing. Not sharp but detailed."
  6. The character is too tall.
- **Changes:**
  1. **Sand:** its own painted texture set, `T_Sand_D/N/ORM` (`gen_textures.py sand`), used by `MI_Sand`
     (scale 4 m, normal 0.8):
     - Wavy ripple bands (lit crest, shaded trough, soft edges) that fade in and out in patches.
     - Soft tone patches, light and dark grains, a few pale shell flecks.
  2. **Caustics:** strength 0.6 (was 1.0), mip bias 1.5 (`CausticBlur`).
  3. **Stretching:** caustics are projected three ways (top along the sun, two sides) and blended by the
     surface normal, so the pattern keeps its size on slopes. Steep faces get partial light.
  4. **Clarity:**
     - Single Layer Water reads its coefficients per centimetre. The region values (written as 1/m) made
       the sea opaque within centimetres. This was proven with a near-zero volume (the sand showed) after
       ruling out the underside tiles and the underwater pass.
     - `M_Ocean` scales absorption and scattering by `VolumeScale` 0.005. The scattering albedo is
       unchanged, so deep water keeps the approved colour while the shallows turn clear turquoise and show
       sand, grass and caustics.
     - The flat far floor moved from 17.5 m to 60 m, so it no longer tints the open sea.
  5. **Stone (`T_Rock_*`):**
     - Tone patches at three scales: broad crisp, medium soft, faint mottling.
     - Small soft pits (a darker inside with a hint of a lit rim), specks and grain.
     - Soft bumps in the normal map at the same scales.
     - Still no lines.
  6. **Player:** capsule 176 → 160 cm, eye height 165 → 150 cm. `Tools/swim_test.py` still passes 5/5.

### D30 · Review round 2: sand repetition, no scatter rocks, underwater particles, softer floor, rough sea, storm area (Adopted, awaiting owner review)
- **Owner review (2026-10-04)** with screenshots of the beach and the rocks, plus a water brief for the
  rough sea ("dont test it yourself. Ask me about the results"):
  1. Sand patterns repeat visibly; make them fainter.
  2. Remove the small stones around the big rocks.
  3. A few small particles in the water, not too obvious.
  4. The sea floor seen through the water is too strong: "there is water in between and its not a mirror".
  5. Rough sea after the Sea of Thieves reference:
     - Narrower, more varied crests and secondary waves on the faces.
     - Smaller highlights that follow the waves, with no large round glare.
     - Selective turquoise crest light; fewer smeared reflection bands.
     - Calm and Flat unchanged.
  6. A storm area with much bigger waves for comparison.
- **Changes:**
  1. **Sand:**
     - `M_SB_Rock` gained `AntiTile`: a second, rotated (37°) and scaled sample of the top projection
       blends in across large world patches (`T_MacroNoise_M`). It is used by `MI_Sand` only; the rocks
       are unchanged.
     - `PatternContrast` 0.6 for the sand, normal strength 0.5, tile 5.5 m.
     - The ripples in `T_Sand_*` are warped more strongly.
  2. **Scatter rocks:** removed from `create_visual_test_map.py`.
  3. **Particles:** `ASaltOcean::UnderwaterParticles`, 260 instances of a quad, visible only while the
     camera is under water.
     - `M_UnderwaterParticles` (additive, after depth of field) places them from per-instance anchors,
       wrapped into a 12 m box around the camera. They stay put in the world and drift slowly.
     - Soft 1.2 cm dots, faded near, far, above the water and with depth.
  4. **Floor through water:** `VolumeScale` 0.005 → 0.009 (more water tint). Caustics 0.6 → 0.35, blur
     1.5 → 1.8.
  5. **Rough sea:**
     - Swell 0.7 → 0.6; wind-sea spreading 8/2.5 → 6/2 (more crossing, shorter crests).
     - Choppiness 1.2 → 1.35; medium waves ×1.5 (cascade gain 0.75).
     - Day sun source angle 2.2° → 1.3°: the large round glare spots break into strokes.
     - Shores of Plenty scattering slightly greener (0.01, 0.072, 0.095) for blue-green troughs.
     - Calm and Flat presets unchanged.
  6. **Storm area:** `ASaltSeaStateZone` (like `ASaltRegionZone`, blended by the viewer's distance in
     `ASaltLightingRig::ComposeSeaState`).
     - The map places one 3 km south of the cove (full storm within 600 m, gone at 1.6 km).
     - Storm preset: swell 1.8, Hs 6.5 m at 60 m, gamma 4, chop 1.25.
     - The ocean tiles' bounds were extended (±40 cells, ±10 m) for the bigger displacement.
     - The ocean is one global simulation, so the whole visible sea changes with the viewer's position.
       The rig composes zones in play mode only.
- **Tooling:** `Launch-Editor.ps1` refuses to start a second editor. A second instance had locked the
  content files, and saves failed with error 32 (sharing violation).
- **Not verified visually by me** (owner's request); materials compile and the map builds.

### D31 · Review round 3: visible particles, no sparkling pixels on distant crests (Adopted, awaiting owner review)
- **Owner (2026-10-04):** "the particles under water are basically invisible, make them more visible";
  "there are pixel errors in the distance at the top of the waves" (single bright, partly coloured pixels on
  far crests).
- **Particles:** size 1.2 → 2.2 cm, brightness ×3 (`Glow` 0.012), a crisper dot, 380 instead of 260.
- **Distant crests:** highlight aliasing.
  - The smaller sun (D30) and the sharper Rough crests made sub-pixel crests catch the full sun glint in
    single pixels.
  - The roughness cap now rises with distance (`MaxAlpha2` 0.004 near → `FarMaxAlpha2` 0.03 at
    150 m).
  - Far roughness 0.08 from 150 m (was 0.06 from 300 m); specular anti-aliasing may add up to 0.004
    (was 0.001).
  - Near highlights stay crisp. The crest glow fades to 10 % by 80 m (was 30 % at 120 m).

### D32 · Rough sea refinement: one dominant direction, broader crests, swell restored, soft crest light, foam patches (Adopted, awaiting owner review)
- **Owner brief (2026-10-04):**
  - Weaker crossing secondary waves (one dominant direction, no pointed peaks).
  - Broader crests with softer tips.
  - Some broad swell restored under the medium waves (too crowded after D30).
  - No narrow turquoise streaks around sharp peaks: soft, light-dependent crest light over upper faces.
  - Keep the small sun highlights but break up long reflective contours selectively.
  - Then a few soft, irregular crest-foam patches, distinct from sun reflections.
- **Rough preset:**
  - Swell 0.6 → 0.8.
  - Spreading 6/2 → 11/3.5 (more parallel wind waves).
  - Choppiness 1.35 → 1.1 (broader crests).
  - Medium waves 1.5 → 1.15.
- **Crest light:** driven by height over the upper faces (smoothstep, start 20 %, range 75 % of the sea
  height) with a gentle slope term (`ThinScale` 2.5). It no longer uses compression, which had outlined
  the pinched tips. Strength 0.2, still backlit and sun-scaled.
- **Reflection contours:** α² ×5 in broad drifting patches (`ReflBreak`, 30 m tile, the quiet-patch
  noise). Long contours break into segments without added microdetail.
- **Foam (returns after D27):**
  - Driven by crest compression (Jacobian below 0.75, full at 0.45), on the upper part of the wave, and
    only in broad patches (`FoamPatch`, 90 m tile).
  - Irregular soft edges from the lace channel of `T_OceanFoam_M`. It rides with the water and fades by
    400 m.
  - It is the Single Layer Water surface layer (opacity, `FoamColor`, roughness 0.85, no specular), so
    it stays distinct from sun glints.
  - Calm water barely compresses, so Calm and Flat stay clear.
- **Cost:** `M_Ocean` 1500 pixel-shader instructions (was 1362).

### D33 · Foam structure, fresh vs residue, varied crest spacing, subtle reflection patches (Adopted, awaiting owner review)
- **Owner brief (2026-10-04):**
  - Break the foreground foam ribbons into shorter irregular patches with clear water between, varied
    width and tapered ends.
  - Give the foam internal structure (openings, partial coverage, trailing fragments, denser off-white
    only on selected crest sections, no uniform bands, not just blurred edges).
  - Separate fresh foam from fading residue.
  - Reduce the repeated bowl-shaped wave pattern (vary crest length and spacing, keep the swell, no
    crossing waves or pointed peaks).
  - Check the roughness patches and keep them subtle.
- **Foam (`M_Ocean`):**
  - Fresh foam = crest compression now × a crest-segment mask. The mask is the patch noise stretched
    across the waves (`FoamSegLength` 16 m along the crest, `FoamSegAcross` 50 m), so foam forms short
    tapered segments.
  - Residue = compression where the crest passed a few seconds ago. That crest now lies `FoamTrail` (7 m)
    and 2.8 m downwind, evaluated with `FSaltOcean::JacobianAt`. Weights 0.6 and 0.35.
  - Structure: coverage thresholds the lace pattern. Thin coverage leaves filaments with large openings;
    dense coverage fills the cells, with bubble holes. Fresh opacity is 0.6–0.97. Residue uses a finer
    lace at 0.45 opacity: thinner, more open, more transparent.
- **Waves (Rough):** gamma 5 → 3.3 (varied spacing), spreading at the peak 11 → 8 (varied crest length);
  the spreading floor stays 3.5. Swell unchanged (0.8).
- **Reflection patches:** `ReflBreak` 5 → 2 with a softer ramp. `ReflBreak` 1 on `MI_Ocean` switches them
  off for an A/B check.
- **Cost:** `M_Ocean` 1715 pixel-shader instructions (was 1500), mostly the residue samples. A packaged
  benchmark of the ocean is still outstanding.

### D34 · Rainbow specks on distant crests: half-float overflow of the sun specular (Rejected: wrong diagnosis, see D35)
- **Owner (2026-10-04):** red, orange, green, cyan and white specks on far wave tops ("looks like a rainbow
  reflection"). They first appeared after D30; D31 (distance roughness cap) did not fix them.
- **Cause (by analysis, not measured):**
  - At grazing angles far away, near-mirror water (α² ≈ 4·10⁻⁵ where neighbouring normals agree, so
    specular anti-aliasing adds nothing) gives a sun peak of GGX distribution × visibility ≈ 8000 × 130 per
    unit illuminance.
  - With 90 klx and pre-exposure ≈ 5·10⁻⁵ that is about 10⁶ in the half-float scene colour (max 65504).
  - Channels saturate separately (red first in warm sunlight), and TSR and bloom spread them into coloured
    blocks.
  - The smaller sun disc of D30 (1.3° instead of 2.2°) raised the peak about 3×.
- **Fix:** a roughness floor `FarMinAlpha2` 0.004, only at grazing angles (NoV < 0.35) and growing with
  distance (full from 150 m). The peak there drops below about 5000; highlights near the camera are
  unchanged. D31's cap only limited roughness from above, so it could not catch smooth spots.

### D35 · Rainbow specks: unbounded in-scatter in the underside, leaking through folded crests (Adopted)
- **Owner (2026-10-04):** the specks were still there after D34, only when looking low over the sea, not
  from high above.
- **Isolation:** sea frozen, identical camera, one switch at a time.
  - Lumen reflections off: still there.
  - Underside tiles off: gone.
  - From below, the same source showed as a bright line with bloom halos along the far underside.
  - Lens flares off: still there. Underwater fog terms off: gone.
- **Cause:**
  - `FSaltUnderwaterLight::InScatterRay` integrated light along the ray as if it stayed under water. In
    the underside material, strongly tilted normals (folds, sub-pixel crests) can turn the reflected ray
    upward. Traced 1 km "under water" through air (negative depth), the exponential grew without bound
    and single pixels overflowed.
  - From above they were visible through the slivers of the underside that show behind folded crests
    while the camera is within 5 m of the surface (the underside tiles are switched on then).
  - That explains "only low": high above, the underside is off.
- **Fix:**
  - An upward ray only integrates up to the surface (distance ≤ depth / Dir.z). The reflected ray in
    `M_OceanUnder` is kept pointing down.
  - The underside's vertex shader also checks the GPU surface above the camera (as the underwater pass
    does) and collapses every vertex when the camera is above the water. No underside slivers show from
    above.
  - The D34 roughness floor is removed (it had no effect).
  - The Snell window fades into total internal reflection beyond 40 m (`WindowFadeDistance`).

### D36 · Sailing boat: kinematic rigid body with hull columns, foil sail, keel and rudder (Adopted, M4 technical)
- **Goal:** the M4 technical part that does not wait for the art decisions (cabin, hull colour): a boat that
  floats on the shared wave field, sails by wind, sail trim, keel and rudder, collides with the pier, rocks
  and seabed, carries the player, and can be reset. D7 (kinematic deck, our own integrator) still holds.
- **Greybox:** `Art/Blender/Scripts/build_boat.py` builds a 10 m double-ender (3.4 m beam, 0.75 m freeboard,
  long keel to 1.45 m) with an open deck, boarding gaps midship, a gaff main, shrouds, a wheel and an
  outboard rudder. Its hull formulas are the same as `Boat/SaltHullShape.h`, so mesh and physics agree.
  Moving parts (boom, gaff, sail, rudder, wheel) are separate meshes.
- **Physics (`ASaltBoat`, SI units, 120 Hz substeps):**
  - Buoyancy from 67 vertical hull columns (the deck plan in 10 × 6 strips plus the keel), each pushed up
    by the water it displaces at the CPU wave surface and damped against the water's own vertical motion.
    Mass = displacement at the design waterline (10.9 t); the centre of mass sits over the centre of
    buoyancy, 0.15 m above the waterline. Heave, pitch, roll and the righting moment follow from the shape.
  - Hull: forward resistance rising steeply past hull speed (3.75 m/s); the lateral plane as a
    low-aspect wing (leeway, stall at 14°) plus crossflow drag; yaw, roll and pitch damping; added mass.
  - Rudder: a foil at the stern; a little torque at rest ("sculling") so the boat can turn in harbour.
  - Sail: a foil in the apparent wind at its centre of effort. The boom swings free up to the sheet's
    limit, so head to wind the sail luffs (no-go zone emerges) and dead downwind it gybes (with 15° by
    the lee before it crosses). 40 m² stands for the full rig of a boat this heavy; the greybox draws only
    the 27 m² main.
  - Lines: moored (bow and stern springs to the start position) or anchored (one 14 m rode); tension capped
    so a displaced boat is pulled in gently.
- **Collision:** 45 capsules (three rings along the hull, keel, bilges) against static geometry. One box
  overlap per frame skips everything in open water. Near geometry, the capsules' overlaps give contact
  planes (MTD) that act as stiff damped springs with friction inside the substeps, and swept capsules stop
  the boat at the first hit so it cannot pass between thin posts.
- **Recovery:** capsized for 3 s → upright at the last safe position (recorded every 2 s while clear);
  `sb.Boat.Reset` returns to the mooring; `sb.Boat.Recover` on demand.
- **Controls:** at the wheel A / D steer (the rudder recentres when let go), W / S sheet in / out, Space
  raises or lowers the sail, C casts off / moors (near the start) / anchors / weighs anchor. The mast has a
  "Raise the sail" interaction too. Stations gained generic action buttons (Jump and Dive keys).
- **Evidence (`sb.Dev.RunBoatTests`, 18/18 pass, PIE, steady 7 m/s wind unless noted):**

  | Check | Result |
  |---|---|
  | Floats at rest | waterline 0.2 cm off, level |
  | Roll period / decay per half swing | 2.7 s / 0.60 |
  | Beam reach / close-hauled (50°) / broad reach / running | 7.0 / 5.9 / 6.1 / 5.4 kn |
  | Heel close-hauled / leeway | 12.7° / 2.5° |
  | Head to wind (10°) | no headway (drifts back at 0.9 kn) |
  | Turning circle at 2.9 m/s, full rudder | radius 22 m (7.7°/s); 3.9°/s at rest |
  | Into the pier end at 2.5 m/s | stops at the posts, no penetration, bounces back at 0.4 m/s |
  | Aground on the beach at 2 m/s | stops, 5 cm into the sand, lifted 24 cm |
  | Player standing aboard at 6.9 kn | no drift, no jitter, stays based on the deck |
  | Helm at sea | focus, steers 7°/s to starboard, leaves onto the deck |
  | Rough sea, beam reach, 8 m/s | 7.5 kn, max heel 21°, heave range 1.7 m |
  | Storm sea, bare poles, 14 m/s | max heel 41°, max pitch 28°, heave range 10 m, no capsize |
  | Moored in 7 m/s | holds within 30 cm |
- **Trade-offs:** heavy and stiff like a real double-ender, so it gathers way slowly (about half a minute
  to full speed). The tunables (sail area, centre of mass height, damping) are exposed for tuning by feel.
  The CPU wave field carries only the long waves (D27), so the boat ignores short chop. A thin horizontal
  slab hit at the bulwark's height can flip the contact normal upward (the bow rides up). The boat does
  not push a swimmer; the swimmer's movement resolves the overlap.

### D37 · Sand with sculpted ripples (Revised: beach only, see the correction below)
- **Owner (2026-10-04):** "make the texture you added on the sand visible not only through color. Shape is the
  keyword. The sand itself as an object is completely flat."
- **Change:** the ripples moved from the texture into the geometry.
  - `SM_Seabed_Sand` (Nanite, 1.26 M triangles, 0.2 m cells) is the visible sand: the broad shape plus ripples
    1.6 m apart and up to 20 cm high, with rounded crests, wide soft troughs and a steeper lee side. They
    wander in direction and are gently crooked, cover about 90 % of the wet floor (small calm spots), and fade
    out on the dry beach and under the pier.
  - Vertex colours light the crests and shade the troughs (`MI_Sand` vertex masks on); the texture only adds
    broad tone patches and sparse flecks (no painted ripple bands, no fine grain).
  - `SM_Seabed` (1 m cells, broad shape) stays as the hidden collision floor. Sea grass roots on the rippled
    surface. `Art/Blender/Scripts/seabed_shape.py` holds the shape for both meshes and the map script.
  - The sandbank and headland shoals blend smoothly (a hard max showed as a saw-toothed edge on the fine grid).
- **Lesson:** never import assets while a play session runs: the static mesh build fails ("Bad MeshDescription")
  and nothing saves.
- **Correction (owner, 2026-10-04):** "undo the edits on the sea sand. I meant the beach sand." The sea floor is back
  to its broad shape and painted texture (`T_Sand`, as in D30). The sculpting moved to the dry beach: wind ripples
  about 0.95 m apart and up to 9 cm high (mostly covering it), soft mounds on the upper beach and three swash ridges
  along the waterline. One material (`MI_Sand`) does both: `M_SB_Rock` gained an optional second texture set that
  the vertex alpha blends in over the last 40 cm below the water level (the calm `T_SandBeach`), so there is no
  seam; under water the vertex colours are neutral.

### D38 · Boat and materials restyled to the owner's hand-crafted style brief (Adopted, awaiting owner review)
- **Owner (2026-10-04):** "The ship and new added stuff again doesn't match our graphic style. Hand crafted, no
  sharp corners, plain texture": chunky silhouettes, gently crooked geometry, broad bevels, deliberately sculpted
  wear; textures simplified into broad colour variations, readable wood grooves and selective chips, no
  photographic grain or dense noise; warm golden-brown wood, muted paint, dark metal with restrained highlights;
  the painterly feel from geometry and texture design, no global blur, outlines or cel shading.
- **Boat (`build_boat.py`), same hull shape and physics:**
  - Every timber is an individual `sb_timber` board with its own seed (bends, grooves, worn edges, soft bevels).
    New helper `sb_timber.bend_board` lays a straight board along a curve, so hull strakes, wale, bulwark boards,
    cap rail and covering board follow the hull exactly and keep their hand-made irregularities.
  - Hull: 9 carvel strakes a side with staggered butt joints over a dark core (seams read as caulked grooves),
    the waterline painted across the planks; a golden wale; bulwarks of outer and inner boards on square
    stanchions with a chunky cap rail; teak deck planks fore and aft with a covering board; a golden stem
    timber; keel and sternpost with broad bevels.
  - Rig: hewn, gently crooked spars (`sb_timber.spar`) with iron bands and forked jaws, mast cap, chunky rope
    shrouds on deadeyes with lanyards over chain wales, forestay with a slight sag. Sail with eight sewn panels
    (raised seams), hollow leech, boltrope, reef points and wooden mast hoops. Chunky wheel, planked rudder with
    iron straps, bitts, a foredeck hatch, a coiled rope. Hull mesh now Nanite (408 k triangles).
- **Materials (shared with the test deck section):** wood texture simplified (grain about 5.5 cm apart, soft,
  broad colour bands, no fibre noise); paint texture calmer; teak and varnish golden-brown and matte (roughness
  up); hull paint muted brick-oxblood, cream muted; iron darker and less metallic; hemp rope muted and its UVs
  run along the rope.
- **Unchanged:** the sailing physics and tests (18/18 pass after the restyle).

### D39 · Owner round: smaller player, no rock lichen, gulls, gangway (Adopted)
- **Player:** 1.5 m (capsule half height 75 cm, eyes at 1.4 m); the swim test still passes 5/5.
- **Rocks:** "remove the yellow marks at the top of the stones": `MI_Rock_Coast` has no lichen (the ruin keeps its
  green lichen).
- **Birds:** `ASaltBirds` flies stylised gulls (`build_birds.py`: chunky white body, grey wings with dark tips,
  yellow beak) as instanced body and wing meshes. Each bird soars in wide circles round a drifting centre, banks
  into the turn, rises and sinks slowly, and alternates long glides with short flapping bursts (wings rotate at the
  shoulder). Nine circle over the cove at 14–45 m, five further out at sea.
- **Boarding:** a hand-made gangway from the pier down to the boat's gap in the bulwark (WorldDynamic, so the
  boat's collision ignores it).

### D40 · Boat weathering and lived-in gear (Adopted, awaiting owner review)
- **Owner (2026-10-04):** "Shape of the things is way better. Colouring and detail is still needed. Everything looks
  too perfect. The mast for example is pixel perfect flat, with not a single colour mistake." The test deck section
  looked better in colour: its many short boards each carry their own tone, worn edges and grime, while the boat's
  long parts (mast, strakes) came out one even colour.
- **Weathering bake (`build_boat.weather`):** the vertex masks of every boat mesh are broken up. Tone (B) blends
  each part's value with blotches 0.4-1.5 m across; wear (R) is the bevel wear broken up plus scattered chips that
  knock paint off (hull, cream; only a trace on wood, none on rope and metal); grime (G) collects along the deck
  corners at the feet of bulwarks, posts and gear, and runs as rain streaks down the topsides.
- **Materials:** stronger variation where the masks act: teak boards grey in patches, varnish patchy, hull paint
  sun-faded and chipped to the dark wood, cream yellowed; dirt slightly green-brown on the hull.
- **Spars:** adze facets, shallow dents and knot bumps (`sb_timber.spar`), so no stretch is perfectly round.
- **Gear:** a pin rail with belaying pins and hanging coils at the mast, a lashed water barrel and crate (staved,
  iron hoops), a bucket by the wheel, two oars on the side deck, fenders over the pier side, horn cleats fore and
  aft (with collision for barrel, crate and pin rail).

### D41 · Simple sailing, interaction points, living sail, physics ropes, an old boat in shape (Adopted, awaiting owner review)
- **Owner (2026-10-04):** "don't make the sailing too complicated. Later there will be electric motors. Sail down
  speed, sail up slow down. The sail has no animation for catching wind. Raise and lower the sail at the right
  points (the ropes by the shrouds). Fix the physics of the ropes. Add an anchor point. The wheel is ONLY for
  direction." And: "the boat looks too new, too fresh ... that needs to be represented in shape. Look at the mast."
- **Sailing model:** the set sail pushes the boat forward along its centreline (7800 N, about 7.4 kn in a 7 m/s
  breeze, reached in about 20 s); the wind shades it only a little (`WindInfluence` 0.35: wind from ahead 6.7 kn,
  abeam 7.4 kn) and its sideways share heels the boat to leeward (about 6 degrees). Taking the sail in slows the
  boat to under 1 kn within 20 s (`CoastDrag`). No sheet, no no-go zone; the boom trims itself to the wind. Engines
  can drive the same way later.
- **Controls:** the wheel only steers. The sail is let down / raised at the rope points by the shrouds (both
  sides); the windlass on the foredeck casts off, moors near the pier, or drops / raises the anchor.
- **Sail:** brailed up to the gaff when taken in, falling to the boom when set. `M_SB_Sail` (the surface master
  plus vertex animation) moves cloth, boltrope and hoops together: Furl, Fill (a spring that pops full when the
  sail catches the wind and breathes with the gusts), Side (belly to leeward), Flutter (shiver along the leech
  with the wind from ahead).
- **Ropes:** the mainsheet, mooring lines (to the pier while moored) and anchor rode are Cable components (verlet
  physics: they sag, swing and go slack). Deadeyes are laced properly: the shroud turns round the upper deadeye and is
  seized to itself, the lanyard runs through three holes in each deadeye on alternating faces, and the lower deadeye
  sits in an iron strap on the chainplate.
- **Anchor:** a fisherman's anchor hangs at a cathead on the starboard bow; dropped, it sinks to the seabed below
  (traced) and stays there while the boat swings on its rode; raised, it comes back up.
- **Age in shape:** spars hewn into eight flats with sharp corners, seasoning cracks, dents and knots; planks more
  warped and worn with split ends; nails on deck planks and hull frames; a weathered repair board on the hull and a
  replaced grey deck board; two patches sewn on the sail; the wood grain reads again (it had become too faint).
- **Tests:** `sb.Dev.RunBoatTests` rewritten for the simple model, 22/22 pass (speed at every wind angle, slowing
  with the sail in, turning, pier and beach collisions, standing aboard at 7.4 kn and 6 degrees heel, wheel only
  steers, rope points and windlass work, Rough sea max roll 31 degrees, Storm 24 degrees, mooring holds).

### D42 · The sea makes room for the boat (Adopted)
- **Owner (2026-10-04):** "the water needs to make space for the boat."
- **Change:** the ocean material is masked: inside the player boat's hull volume (the `SaltHullShape.h` formula, up
  to the bulwark top) the sea is not drawn, so waves never show inside the hull or on the deck. `ASaltBoat` writes
  its pose to `MPC_Ocean` (`BoatPos`, w = on, and its three axes) every frame; existing MPC entries are kept.
- **Cost:** masked rendering of the ocean (depth pre-pass with the mask). To be measured in the packaged benchmark.

### D43 · Anchor stop, windlass motion, a full-turn wheel, ladders and one-piece collision (Adopted)
- **Owner (2026-10-04):** animate the windlass drum and rope when the anchor goes down or up; turn the wheel a full
  360 degrees each way and set it lower; when the anchor falls the boat comes to a smooth stop and stops turning,
  whether the sail is set or not; two ladders to climb, with a climb animation; "the boat is one thing" (parts
  must not pass through other objects).
- **Anchor:** dropping it is a brake that ramps in as the anchor reaches the bottom (6000 N s/m, 90000 N m s on yaw),
  and the sail's drive and the at-rest steering fade out at the same time. Once the boat has come to rest
  (below 0.25 m/s) it holds that place and heading with soft springs; it no longer swings on a rode. Test: from
  3.8 m/s with the sail set and full rudder, 0.03 m/s and 0.01 deg/s after 14 s, peak deceleration 1.5 m/s2.
- **Windlass:** the drum with its cranks (`SM_Boat_WindlassDrum`) and the rope coil (`SM_Boat_WindlassRope`) are
  separate meshes. The drum turns with the rode paid out (fast going down, slower hove in) and the coil thins.
  A rope lead runs from the drum to the cathead.
- **Wheel:** full rudder is one full turn of the wheel either way (`WheelRatio` 360/35); the hub is 0.68 m above
  the deck (was 1.0 m).
- **Ladders:** a ladder hangs over each side at the boarding gap (two stiles, rungs every 0.3 m, iron hooks).
  E at a ladder climbs it: up from the water onto the deck, or down from the deck into the sea. The climb goes
  rung by rung (30 cm steps of 0.34 s, each eased, with a small dip of the head) and follows the moving boat.
  Climbing out of the water elsewhere (Space at a ledge) uses the same stepped climb for rises over 0.6 m.
- **One piece:** the boat's collision now covers everything that stands out: stem head, cathead with the stowed
  anchor, rudder blade and head, chain wales and deadeyes, ladders, fenders, and the boom wherever it swings (a
  capsule that follows it; the broad phase reaches the swung-out boom). The gangway is part of the boat: laid to
  the pier while moored, stowed otherwise (it was a separate actor that the boat could cut through).
- **Tests:** `sb.Dev.RunBoatTests` 27/27 (new: anchor stop; ladder up from the water and down into the sea, 7
  rungs each); `Tools/swim_test.py` 5/5.

### D44 · Settings menu with display defaults, a wind pennant, the bow wave and wake (Adopted)
- **Settings menu:** Esc in the game, F10 in the editor (Esc ends a play session there), Start on a gamepad. It pauses
  the game and offers V-Sync, frame rate limit (30-240 or unlimited, including the display's refresh rate), resolution
  scale (50-100 %), window mode, field of view (70-110), mouse sensitivity, invert look, restore defaults and quit.
  Changes apply at once and are saved (`USaltGameUserSettings`, GameUserSettings.ini). Built in code like the HUD.
- **Display defaults:** vsync off with the frame rate capped at the display's refresh rate (vsync on paced the game at
  55-60 fps), native resolution. The editor keeps its own vsync off (`[SystemSettingsEditor] r.VSync=0` in
  BaseEngine.ini), so V-Sync reads "(game only)" there, like the window mode.
- **Wind pennant:** a long faded oxblood pennant on an iron spindle at the masthead (`SM_Boat_Pennant`, `M_SB_Flag`).
  It swings to the apparent wind, streams out as the wind freshens, hangs in light air and flutters toward the tail.
- **Bow wave and wake (shape):** the moving boat shapes the sea around it (`SaltBoatWake.ush`, rendering only; the CPU
  sea and everything floating on it ignore it): water piled up at the stem, a crest, trough and stern wave along the
  sides (one transverse wavelength, 2 pi U^2 / g), a bow-wave ridge that hugs the hull and peels off at about 22
  degrees, a Kelvin wake (transverse and divergent waves in the 19.5 degree wedge, softened cusps) and a shallow hollow
  down the track. Heights grow with the square of the speed (0.4 m at 7 kn). The vertex shader evaluates it and hands
  slope, white water and slick to the pixel shader (ocean pixel shader 1732 instructions).
- **Tests:** `sb.Dev.RunMenuTests` 7/7.

### D45 · The first boat steers with an oar; white water and a trail (Adopted)
- **Owner (2026-10-04):** "Remove the wheel but save the data. The first boat will only get a rudder in form of a big
  stick at the side in the back." "I dont see water being pushed back by the boat, i dont see a trail in the water."
- **Steering oar:** a big oar hangs on the starboard quarter in a wooden boss with a rope withy, raked 16.5 degrees aft,
  blade of three boards with iron straps; a handle across the boat turns it about its shaft (`SM_Boat_SteeringOar`).
  The helmsman stands behind the handle (A / D steer as before, full rudder 35 degrees). The physics foil sits at the
  oar's blade; the oar collides as part of the boat. Turning circle 21.7 m (22 m with the rudder).
- **Wheel kept:** `ESaltSteering::Wheel` brings back the wheel, its pedestal (`STEERING = "wheel"` in build_boat.py)
  and the sternpost rudder for later boats; the meshes and code stay.
- **White water and trail:** white water rides the bow wave's crest and hugs the hull at the waterline from the stem
  aft. The boat records its stern position once a second (15 samples) and the sea draws a foam trail along that path,
  so it follows the turns; it starts about 1 m either side, widens by 0.2 m a second and fades over about 12 s, and the
  trail calms the small ripples (the glassy slick behind a boat). The foam uses the crest foam's lace pattern.
  Everything scales with speed (nothing at rest). Tunables: `WakeFoamAmount`, `WakeSlick` (MI_Ocean, Wake group),
  `WakeStrength` on the boat.
- **Tests:** `sb.Dev.RunBoatTests` 28/28 (new: the sea gets the boat's speed and a trail sampled a second apart, 3.79 m
  spacing at 3.79 m/s); `Tools/swim_test.py` 5/5; `sb.Dev.RunMenuTests` 7/7.

### D46 · Save and load: the M5 loop closes (Adopted)
- **Why:** M5's loop is depart, sail, return, moor, save/load; everything but the save existed.
- **What is saved** (`USaltSaveSubsystem`, slot `Main`, `Saved/SaveGames/Main.sav`): the boat's place, heading, hold
  (moored, anchored, free) and sail; the player's feet and view, in the boat's frame while aboard (they wake up on
  their deck wherever it lies); time-of-day preset, region, weather, sea state and wind.
- **When:** mooring at the windlass ends a voyage and saves; quitting from the menu saves; the pause menu has "Save
  game" (with the time of the last save) and "Load last save". A short notice ("Game saved") shows at the top.
- **Loading:** the packaged game continues from the last save on start. Play-in-editor never loads (editor sessions and
  tests start from the map as built), nor do tool runs (`-SaltNewGame`: Run-Build.ps1, record_clip.py). A save from
  another map is ignored. Restoring puts a moored boat at its mooring and an anchored one with the anchor down.
- **Tests:** `sb.Dev.RunSaveTests` 5/5 (in a test slot: anchored boat with the sail set, player aboard, sea and wind
  restored exactly; mooring saves).

### D47 · Ruin Isle and islands on the horizon (Adopted, first pass)
- **Why:** something to sail to (the README's "sea of islands, ruins and traces of a lost civilization"); the art
  direction's haze toward distant islands.
- **Ruin Isle** (`build_islands.py`), 1.7 km out towards the Ancient Isles region, about 110 m across: an irregular
  coast, a gentle beach all round (1.6 m over 14 m) with the cove's sculpted wind ripples and sand material, soft
  dunes, a knoll, a turquoise shelf about 3 m deep and a skirt down to 60 m. Visible mesh (Nanite, 0.5 m cells inside,
  4 m beyond, overlapping so no seam shows) plus a hidden 2 m collision mesh. A calm zone settles the swell round it.
- **Ruins** of chunky bevelled blocks (the landmark tower's masonry): a broken gateway on the knoll facing the cove
  (one pier broken, half the arch standing, the ancient glow in the keystone, fallen arch stones) and a low broken
  wall corner. Coastal rocks on the shore; a few gulls.
- **Horizon:** three big chiselled island masses 4-7 km out (`SM_Island_Far_A/B/C`, rock material); the haze fades them.
- **Tests:** `sb.Dev.RunBoatTests` includes sailing onto Ruin Isle: the boat runs aground on the beach shelf, 10 cm
  deepest contact, and lies still once the sail is in.
- **Open:** the islands are sand and rock only (no vegetation yet); the far masses are rock silhouettes.

### D48 · Hovering in the water; the wake only where the boat has been, fading smoothly (Adopted)
- **Owner (2026-10-04):** "remove the auto swim up when in water. hovering is perfect. so you dont go up nor down when
  not pressing any key." / "when the ship starts you can see a trail way behind the ship. but the ship was never
  there." / "the fade away of the trail is not smooth. its hard and it fades in blocks."
- **Swimming:** fully under water the swimmer is neutral and hovers where they stop (0.0 cm drift in 3 s); Space rises,
  Ctrl / C dives. Buoyancy (1.2) only acts as the head breaks the surface, so a surfaced swimmer floats there with the
  eyes above the water. A fall into the sea ends hovering where the momentum stops.
- **Wake reach:** the bow wave and the Kelvin wake are drawn along the boat's heading, so they now reach back only as
  far as the boat has come on its recent straight course (from the trail: until a sample where it was still or headed
  more than 25 degrees off), eased and capped at 40 m. Setting off, the wake grows from the hull; in a turn it shortens
  and the foam trail (which follows the real path) carries the curve.
- **Trail fade:** the trail fades out smoothly from 5 s and is gone by 13.5 s, before its oldest sample is dropped (it
  used to vanish a one-second segment at a time); the foam's opacity follows its coverage with a softer threshold, so
  thinning foam fades instead of losing lace cells.
- **Tests:** `Tools/swim_test.py` 5/5 (new: hover at depth); `sb.Dev.RunBoatTests` (new: the wake starts at the boat,
  1.5 m after 0.5 s; reach 35-40 m after a long straight run).

### D49 · Sea haze; the wake follows the speed (Adopted)
- **Owner (2026-10-04):** "trail waves and particle count should be based on speed." / "add fog in the vision, so you
  cant look infinite in the distance."
- **Haze:** day fog density 0.01 -> 0.035 (starting at 30 m), evening 0.025 -> 0.05, dusk 0.012 -> 0.035. The horizon
  dissolves into a pale band, the spire 420 m out is softened, Ruin Isle (1.7 km) shows faintly from the cove and comes
  out of the haze on the way, the far islands are ghosts. Finding: the fog's colour values hardly matter, because the
  sky atmosphere lights the height fog (they are small luminances on top); the old blue fog read as more sea and sky.
  The engine's extinction is weaker than its formula suggests (20x density still left the spire faintly visible), so
  the densities were set by A/B captures.
- **Wake by speed:** the trail's strength is the boat's speed over its top speed (4 m/s), no longer full from 2.5 m/s:
  it sets the trail's density (how many foam patches), its starting width (0.4 m slow, 1 m at top speed) and the slick.
  The bow's white water and all wave heights already grow with the square of the speed.

### D50 · The satchel: inventory, pickups and a map, in a hand-crafted UI (Adopted, first pass)
- **Owner (2026-10-04):** "create a simple inventory with fitting gui to the theme. Add a map tab."
- **Items** (`Core/SaltItems.cpp`): driftwood (stacks of 20), seashell (30), coil of rope (10), glass float (10), ancient
  shard (5), each with a name, a description, a painted icon and a pickup mesh. The satchel holds 24 stacks and starts
  with two coils of rope (`USaltInventoryComponent` on the player).
- **Pickups** (`ASaltPickup`, `build_pickups.py`): E picks them up ("+1 Seashell"); taken ones are gone for good (the
  save remembers them by PickupId). Six on the cove's beach, six on Ruin Isle's beach and the ancient shard glowing in
  front of the gateway.
- **The satchel screen** (`USaltSatchelWidget`, built in code): a dark wooden frame with iron corner plates around
  parchment, wooden tabs (the open one parchment), inked slots with the icons and counts, the selected item's name and
  description in ink. Tab or I opens the inventory, M the map, Q / E switch, Tab / Esc close; mouse and gamepad (Back
  opens, B closes). The world goes on while it is open (no pause).
- **Map tab:** two parchment charts generated from the world's shapes (`Tools/textures/gen_ui.py`): home waters (3 km)
  and the open sea (12 km), with ink coastlines, hatched storm waters, a compass rose and a ruin mark; place names,
  your marker and the boat's (turned to their headings, held at the edge when off the chart), the distance to the boat
  and a scale bar are drawn by the UI. North (up) is world +X.
- **UI textures** are generated (frame, parchment, slots, tabs, icons, charts, markers; original content) and imported
  as UI textures without mips. Labels use the engine's Roboto (no font asset).
- **Saved:** the satchel's contents and the taken pickups are part of the save (version 2).
- **Tests:** `sb.Dev.RunInventoryTests` 7/7 (stacking, a full satchel, a pickup, the screen, the map marker exactly
  where the player is, saving). Dev commands: `sb.Dev.Give <Item> [Count]`, `sb.Dev.Satchel 0|1 [chart]|off`.
- **Open:** items have no use yet (repairs, trade or the ancient shards' story come later).

### D51 · The second boat: a 14 m gaff cutter with a top deck and a lower deck (Proposal, awaiting the owner)
- **Owner (2026-10-04):** "start designing an new boat with a top deck and under deck and with more space."
- **Proposal** (`Docs/Design/Boat2.md`, plan sheet `Docs/Design/Boat2_Plans.png`): 14 m on deck, 4.4 m beam, about 30 t;
  transom stern with a raised quarterdeck and the wheel; a deckhouse with skylight and companionway; below deck a hold,
  a saloon (table, bunks, stove, chart table) and a captain's cabin, 1.65 m clear under the beams, 2.45 m under the
  deckhouse. Gaff main, staysail and jib; the same simple sailing.
- **Technical plan:** a hull shape per boat (physics, collision, the sea's cut-out from MPC_Ocean), no underwater look
  and no swimming inside the hull, walkable interior collision, light below deck.
- **Next:** the owner's answers (rig, stern, helm, size, how the player gets it), then a walkable blockout.

### D52 · A more lived-in boat: boards of their own, stores, the anchor rope through a hawse, more on deck (Adopted)
- **Owner (2026-10-04):** the harbour's planks are interesting because every plank is a bit different, the boat's are
  not; remove the rope hanging from the sail, the bucket and the small platform in front of the anchor; make the wooden
  box and the barrel accessible (normal inventory, 10 slots; the UI design comes later); the anchor rope should go into
  the boat and the anchor never be seen from outside (the effect remains), the rope more to the side; more decorations.
- **Boards:** deck planks of 11-17 cm, two to four boards each with scattered butt joints, each a few millimetres proud
  or sunk, slightly rolled and crooked, gaps of 4-9 mm, its own wear and grain and one of three teak shades or (6 %) a
  grey replaced board. Hull boards a little proud or sunk with one, two or three butts per strake, in the hull red, a
  faded red or a darker red; the top strake cream or aged cream. New MIs: TeakLight/Dark, Paint_HullFaded/Dark,
  Paint_CreamAged.
- **Removed:** the mainsheet (the physics rope from the boom that hung and swung while the sail was in), the bucket,
  and the cathead (the timber out over the bow where the anchor hung).
- **Anchor:** the rope runs from the windlass along the starboard side into a hawse pipe in the deck and comes out of a
  hawse hole with an iron ring in the starboard bow planking; dropped, it goes from there into the water. The anchor
  mesh is always hidden; dropping, holding and raising work as before.
- **Stores:** the barrel and the crate open (E) as 10-slot stores beside the satchel; a click moves a stack across. The
  crate starts with four driftwood and a coil of rope. Their contents are saved (save version 3).
- **Decorations:** two lobster pots, a heap of net with glass and cork floats, rope coils on deck, a sea chest, a
  lashed tarp bundle, a boat hook along the starboard bulwark, a brass bell and a lantern on the mast, carved
  trailboards with rosettes on both bows, and an oxblood-and-cream ensign on a staff at the stern that turns and
  flutters with the pennant.
- **Tests:** `sb.Dev.RunInventoryTests` 10/10 (new: the boat's stores, moving items, saving them); boat suite 30/30;
  swim 5/5; save 5/5; menu 7/7.

### D53 · Object overhaul: every object built for its purpose, checked up close, and given its physics (Adopted)
- **Owner (2026-10-04):** check the Blender objects before using them, and their physics; a bell must not just hang on a
  stick: a detailed wooden arm matching the rest, a small iron ring, the bell, physics for everything. The mast "still
  looks so boring", far too perfect, without extras like iron rings; "think more about the meaning of the objects".
  Delete the tarp bundle, the foredeck hatch, the lobster pots and the rope coil on deck.
- **How objects are made now:** purpose first (what holds it, what it holds, how it is used and worn), then the parts
  that purpose needs, then the model; every object is checked in close-up workbench renders with its collision drawn
  over it (`Tools/preview_blend.py ... --ucx`) before it is imported, and hanging things get physics.
- **Mast:** a hewn spar whose head is worked aft and to port by the peak halyard's pull (7 cm and 2 cm at the truck; the
  pennant's swivel follows, `SaltHullShape.h` MastHeadAft/Port) and bowed a little in the hoist; tarred at the foot,
  varnished, painted above the hounds. Partners with eight driven wedges and a lashed canvas mast coat; an old repair
  just above the deck, where masts spring (fish splints fore and aft, rope seizings, two bands); a wooden saddle for the
  boom's jaws; a band over the gaff's throat; hounds cheeks under the shrouds' eyes; eye bolts with the throat and peak
  halyard blocks; the forestay's eye with its seizing; a masthead band and a round truck. Between the boom and the
  throat nothing stands proud, because the sail's hoops slide there (they are now 19 cm, loose round the mast).
- **Halyards:** the gaff now hangs from something: physics ropes from the blocks on the mast to an eye bolt at the
  gaff's heel and to a block on a span along the gaff (taut, `ThroatHalyard` / `PeakHalyard`); their falls come down to
  new pin rails inside the bulwark at the shrouds and hang there coiled. The sail is set and let go at those pin rails.
- **Boom and gaff:** their heels stop at the mast; jaws bolted to their sides reach round it, lined with leather, closed
  by a parrel line threaded with wooden beads; bands with eyes at the outer ends.
- **Bell:** a hewn arm on a band forward of the mast, braced by a grown knee bolted to the mast, an eye bolt through its
  end with a small iron ring; the cast bell (`SM_Boat_Bell`, mouldings, crown loop) hangs in that ring and its clapper
  with the bell rope (`SM_Boat_BellClapper`) hangs inside it. "Ring the bell" swings the clapper across to strike.
- **Lantern:** a forged arm with a hook on a band aft of the mast; the lantern (`SM_Boat_Lantern`: handle, chimney,
  hood, frames, amber horn panes, base) hangs on it and swings; light it or douse it; the lighting rig lights it at dusk
  and in the evening with the harbour's lanterns (same light and flicker).
- **Swing physics** (`USaltSwingComponent`): a damped pendulum in world space that feels the pivot's real motion (roll,
  pitch, heave, turns), stops at a limit cone where it strikes (the clapper at the bell's sound bow), and is pushed away
  by a player walking into it. Bell, clapper and lantern collide (the clapper does not).
- **Gear:** the barrel's lid has a handle and the barrel is lashed to an eye bolt on the bulwark; the crate has rope
  handles and iron corner straps; the windlass has a ratchet wheel on its drum and a pawl on the cheek; the oars are
  seized and lashed to an eye; the boat hook rests low in two lashed cleats; the sea chest is built from boards with
  hinges, hasp, padlock and rope beckets; the net is a soft heap with its cork line and glass floats in rope nets; the
  ensign's staff has its flag line and cleat.
- **Deleted:** the tarp bundle, the foredeck hatch, the lobster pots, the rope coils lying on deck, the old bell and
  lantern on bare brackets.
- **Collision:** everything on deck collides (net, chest, oars, boat hook, pin rails, partners and coat, bell arm, lantern
  arm, ensign staff), the hatch's and the removed items' hulls are gone; bell and lantern have their own.
- **Tests:** boat suite 38/38, new: `BoatHalyardsHoldGaff`, `BoatBellAndLanternSwing`, `BoatSwingsSettle`,
  `BoatBellRope` (+focus), `BoatLantern` (+focus), `BoatBellBumped`; swim 5/5, menu 7/7, save 5/5, inventory 10/10.

### D54 · Calmer flags, a smaller player, ladders you climb yourself (Adopted)
- **Owner (2026-10-04):** "the flag is bouncing like crazy left and right, it looks like the animation speed is 20x";
  "my character is too big to use the boat, make the character a bit smaller again"; ladders: one press takes you on to
  the ladder, W and S go up and down, E leaves it, at the bottom you leave it too, at the top you go on board. And: "you
  never wait for me in things like tools, if you need them take them".
- **Flags:** the cloth's wave (M_SB_Flag) ran at about 2 Hz with a 12 cm swing; now about 0.3 Hz in light air and 0.6 Hz
  in a fresh breeze, the hoist still and the fly swinging a few centimetres. The pennant and the ensign also turned with
  the wind at the sail's centre, which includes the masthead's sway as the boat rolls, so they swung from side to side
  with every roll; they now follow the wind past the moving boat (true wind less the boat's way) and turn slowly.
- **Player:** 1.4 m (capsule half height 70 cm, eyes at 1.3 m) and slimmer (capsule radius 26 cm instead of 34: the
  68 cm wide body was what made the deck feel cramped between the mast, the bell, the pin rails and the stores).
- **Ladders:** E takes hold, from the water where the body is or from the deck at the top (a short glide, facing the
  rungs, the view stays free). W / S climb rung by rung (fast between rungs, a slower grip at each, the head dips with
  each pull); with no key the hands rest on the nearest rung. E lets go (a push off the hull, into the sea); off the
  bottom rung you are in the sea, over the top you step on board. The HUD shows "[E] Let go of the ladder" and
  "W / S climb up and down".
- **Tests:** the boat suite's ladder part rewritten: take hold from the water and from the deck (with focus checks), no
  slipping without keys, climb up on board, climb down into the sea, let go half way.
- **Climbing out of the water** now reaches up to an arm's length ahead regardless of the body's width (the slimmer
  capsule fell short of the pier's edge). Tests: boat 44/44, swim 5/5, movement 10/10.

### D55 · The new Saltbound UI (in progress): fonts and shared parts, the key-scrap prompt and hold, the save sign (Adopted)
- **Owner (2026-10-04):** "Camera perspektive is too high, make it a bit lower." Build the new UI from
  `Docs/Design/UI/UI_Design.md` exactly (UI only; the agent prompt `UI_Agent_Prompt.md`), asking the questions first.
- **Owner's answers (spec section 13):** the tool wheel opens by **holding Q** (not Tab; Tab and I keep opening the
  satchel); the player owns the **compass, lantern and spyglass** from the start; the **barrel is round**; **only raising
  the anchor** is a hold; an island is found **within 300 m of its shore**.
- **Camera:** eyes at 1.2 m (were 1.3 m).
- **Fonts:** IM Fell English (Regular, Italic), IM Fell English SC and Alegreya (Regular, Italic, Bold; static faces cut
  from the variable font), SIL OFL, fetched by `Tools/fonts/get_fonts.py` into `Art/Fonts` (`Docs/ThirdParty.md`), imported
  as font faces under `/Game/Saltbound/UI/Fonts` (`Tools/ue_scripts/import_fonts.py`). The families are put together at
  runtime (`SaltUI::Font`, resolved lazily): a composite font's entries cannot be set from Python. `/Game/Saltbound/UI` is
  now always cooked (UI content is loaded by path).
- **Painted parts from the approved drawing code:** `Tools/textures/gen_ui_parts.py` pulls the design pages' canvas
  toolkit and drawing functions verbatim, adds the part list `Tools/textures/ui_parts.js`, renders it in headless Chrome
  with the UI fonts and writes each part as a transparent PNG to `Art/Textures/UI` (key scrap and its paper mask, inked
  and light key letters, keycap, count coin, the paint speck). So the parts look exactly like the approved pictures; the
  3D pieces (brass, rope, wax, crate) will be Blender renders.
- **UI materials** (`Tools/ue_scripts/setup_ui_materials.py`): `M_UI_PaintedText`, a font material for painted lettering
  (cream graded down the line through the font material's second UV channel, the brown speck at 40 %; the rim is the
  letters' outline at 13 % of the size, the soft halo three fainter outline-only copies behind: Slate text cannot blur);
  `M_UI_KeySoak`, the key scrap with its letter and the hold's ink rising from the bottom with a moving wavy surface,
  the letter turning parchment-light under it.
- **Shared widgets** (`UI/SaltUIStyle`, `UI/SaltPaintedText`): fonts, ink text with bleed, debossed text, keycap, count coin,
  painted lettering.
- **Prompt (section 7):** fixed 66 px under the screen centre: the key on its torn scrap (tilted -3 degrees), 18 px, then
  the action in painted IM Fell English 36 px; fades in and rises 6 px in 0.15 s while the scrap settles from -6 degrees,
  fades out in 0.1 s, a new target cross-fades. Replaces the old `[E] ...` text.
- **Hold (section 8):** `USaltInteractableComponent::HoldTime`; the windlass's "Hold to raise the anchor" takes 0.8 s. The
  ink fills while E is held, drains in 0.25 s when let go early, and when full the scrap pulses (1.06 to 1 in 0.15 s), the
  action runs and the prompt hides for a moment.
- **Save sign (section 14):** the knotted-ring layers in the bottom-right corner (centre 88 px from the right and the
  bottom, 96 px) while the game saves: fades in 0.2 s, the rose turns 45 degrees every 0.7 s with a damped overshoot, stays
  at least 1.4 s, finishes its step and fades out in 0.3 s. `USaltSaveSubsystem::OnSaveStarted / OnSaveFinished`. The old
  "Game saved" notice is gone (the sign says it).
- **Launch intro (section 15):** `Saltbound_Intro.mp4` (8 s, the owner's updated cut: ink from the knot becomes the S,
  the knot's rope ends grow on into the underline) plays as the startup movie (`Saltbound/Content/Movies`, 
  `[/Script/MoviePlayer.MoviePlayerSettings]` in `DefaultGame.ini`), skippable with any key. *Wait for Movies to Complete*
  is **on**, unlike the spec: with it off the engine ends the movie as soon as loading is done (`bAutoCompleteWhenLoadingCompletes`),
  which cut it after ~3 s in a standalone test; on, it plays its 8 s (the map is loaded underneath).
- **Captures:** `Tools/ui_capture.py` takes `shot showui` in a floating 1920 x 1080 PIE window (Editor > Play In window
  size; the editor viewport would capture the whole editor) and puts it on top of its reference picture. Dev cvars
  `sb.Dev.Prompt <text>`, `sb.Dev.Hold <0..1>`, `sb.Dev.SaveSign 1`.
- **Tests:** `sb.Dev.RunInventoryTests` 17/17, new: `HoldFills`, `HoldDrains`, `HoldFires` (the anchor rises after 0.8 s),
  `SaveSignShows`, `SaveSignHides`, `SatchelTag`, `SatchelDrops`. Dev: `sb.Dev.SatchelDemo` (the picture's contents).
- **Satchel (section 5):** the bag thrown open (flap, body, strap and buckle, "SATCHEL" debossed, the 24 dishes pressed
  into the leather, the lip with its hints, the D-ring and twine) is one painted part from the design's code; on it the
  count ("7 / 24", debossed), the pockets (selection glow and gold ring, the item 86 px with its contact and drop shadows,
  lifted to 96 px when chosen, the shard's slow turquoise glow, counts on leather coins) and the parchment tag that hangs on
  the twine and swings when the selection changes (a ~6 degree kick settling in ~0.8 s). The tag: name, "N in this pocket
  . M fit", an ink rule, the description, and E (the item's use, greyed: none works yet; "(later)" when it has none),
  R turns the item in the hand, G drops one in front of the player. I, Tab or Esc close (Tab still opens the satchel: the
  wheel is on Q). The map is its own screen on M (the D50 chart, unchanged, without tabs).
- **Fonts in Slate** use the fonts' Windows metrics (Alegreya 1.123 / 0.345 em, a 1.468 em line; IM Fell English 0.905 em
  ascent): the layout uses them so baselines land where the design puts them.
- **The design pictures** were taken from the design page with its dark border: the frame sits ~9 px in and is 0.990 x
  0.997 the size. `Tools/ui_capture.py` undoes that, so the A/B sheets compare true 1080p positions.
- **Stores (section 6):** the satchel beside the store on one screen (blurred world behind). Left, the satchel as a
  leather panel with "SATCHEL" and its count debossed and 24 dishes (radius 44, 6 x 4); right, the store:
  - **Crate:** the design's open chest, painted by its own code: the lid propped open behind with iron straps, hinges and
    "CRATE" burned in, the box with its rim, iron L-plates, far and side walls, floor planks and dividers into 5 x 2
    compartments. Its count ("2 / 10") is burned in under the name (a layered scorch: Slate text cannot blur).
  - **Barrel (round, owner):** the design has no picture of it, so it is drawn in the same toolkit after the crate: an open
    barrel seen from above (the opening an ellipse 0.7 of the circle) with bulging staves, two iron hoops and the staves'
    tops, its head leaning behind with "BARREL" and the count burned in; inside, on a planked false bottom, a ring divider
    and radial dividers make **3 compartments in the middle and 7 around**. Its items lie at 88 px (96 in the crate) so the
    middle three fit their wedges.
  - **Moving:** click moves the stack, Shift-click one (`USaltInventoryComponent::MoveStack` takes a most-count and says which
    slot took it). The item flies on a quadratic arc in 0.25 s (eased out), growing as if lifted, with its shadow and its
    coin; a dotted path (70 %, fading after the landing) and a dashed warm outline on the slot it goes to (the crate's
    compartment, a ring for the barrel and the satchel). The side it flies to shows its old contents until the item
    lands, which settles with a small drop. Arrows / d-pad move a cursor to the nearest slot in that direction (across
    both sides), Enter / A move the stack, X one; E / Esc close ("Click: move the stack . Shift-click: move one . E close
    the lid").
  - The lids are part of the boat's merged mesh, so nothing swings open in the world (the spec asks only for a separate
    lid).
  - `sb.Dev.Store crate|barrel [demo [t]] | off` (demo: the design picture's contents with the floats in flight, held at t);
    tests `StoreShiftMovesOne`, `StoreCounts`, `StoreCursor`, `StoreFlightLands`. Inventory 21/21.
- **Landfall (section 9):** the places are shared data now (`Core/SaltPlaces`: id, name, centre in world metres, the
  shore as an ellipse; the satchel's chart reads them too). `USaltLandfallSubsystem` checks four times a second whether
  the player is within **300 m of the shore** (owner) of an island not found yet (the Spire, Ruin Isle, Mistholm, Gull
  Rock, Longshoal; not the home cove or the storm waters); it marks it found in the save (`FoundPlaces`, save version 4)
  and the HUD shows the banner once.
  - The banner is the design's `banner()` split into layers per island (`T_UI_Landfall_<Id>_Band`: tails, band and
    rules; `_Name`: the letters on the arch) and the shared wax seal with the island-and-gateway glyph; "A new island"
    and the line under it are painted lettering. A UI material (`M_UI_Reveal`) unrolls the ribbon from its middle and
    inks the name in from the left with a slightly uneven edge.
  - Motion: unroll 0.6 s, the seal stamps at 0.5 s (1.4 to 1 with a slight overshoot), the name inks in 0.6 s, the
    line fades in, a hold to 5.6 s, then it rolls up and fades (0.5 s). A second landfall waits for the first.
  - The line is computed: distance and the 16-point bearing from Saltbound Cove ("1.7 km north-north-east of Saltbound
    Cove"; north is +X). "· now on your chart" waits until the chart marks found places (spec).
  - `sb.Dev.Landfall <Id> [t] | reset`; tests `LandfallFoundOnce`, `LandfallSaved`, `LandfallBanner`, `LandfallRollsUp`.
    Inventory 25/25.
- **Item icons (step 7): skipped (owner, 2026-10-05):** "these icons were only for testing and looks bad and need recreating.
  i didnt even think which items are needed so maybe skip it". The test icons stay until the owner decides the items; the
  icon booth (a show-only scene capture of each pickup) is not built.
- **Tool wheel (section 4):** held open with **Q** (owner) or LB (new `IA_ToolWheel`); Tab and I still open the satchel.
  - The design's own parts: the rope ring on its dark disc, a lashing (placed and turned per medallion as it grows), the
    medallion (leather, inner shadow, brass bezel, rivets; drawn at the chosen size 84 and scaled to 70), the amber halo and
    bright rim of the chosen one, the six painted tools, the divider with its turquoise diamond, the hints ("Q hold, aim,
    let go . 1 - 6 pick directly"). Keys 1 to 6 are keycaps outside the medallions.
  - In the middle: the tool's name (painted lettering, 64 px), its hint and "let go of Q to take it" (plain lettering
    with the soft halo, `USaltPaintedText::SetPlainColor`).
  - While it is held the mouse (a pointer with a 40 px dead zone) or the right stick (0.35) aims instead of turning the
    camera; the medallion in that direction is chosen. Letting go takes it, Esc / B cancel, keys 1 to 6 take a tool
    directly (also without the wheel). It opens on what is in hand, scaling from 0.9 with a fade (0.12 s), the
    medallions popping in 20 ms apart.
  - Tools are data (`Core/SaltTools`): compass, spyglass, chart, lantern, satchel, empty hands. The chart opens the map,
    the satchel the satchel, empty hands puts away; compass, spyglass and lantern are held (`GetHeldTool`). The player
    owns the compass, the spyglass and the lantern from the start (owner); a tool not owned shows an empty medallion
    and is not taken.
  - `sb.Dev.Wheel 1 [n] | 0`; tests `WheelOnQ`, `WheelByAngle`, `WheelTakesTool`, `WheelCancels`, `WheelKeys`,
    `WheelNeedsTool`. Inventory 31/31.
- **Held tools (`USaltHeldToolsComponent`):** shown in front of the camera (there are no arms yet); a tool rises into
  view in 0.25 s and sinks out when put away.
  - **Compass** (`SM_Tool_Compass` and `SM_Tool_CompassNeedle`, `Art/Blender/Scripts/build_tools.py`): a brass pocket
    compass with a bezel, a pendant and a bow, a cream dial with dark ticks and a red north mark, plain shapes and the
    project's materials. Held low on the right, tilted towards the eye; the needle swings to world north (+X) as a damped
    pendulum (0.9 s, damping 0.25) and settles.
  - **Spyglass:** looked through, so no mesh: the view narrows to 20 degrees in 0.35 s, the HUD shows a dark round view
    with a brass rim (`T_UI_SpyglassMask`), and the mouse and stick turn slower in proportion.
  - **Lantern:** the boat's lantern hanging from the left hand at arm's length, swinging under it
    (`USaltSwingComponent`), with the boat lantern's warm light (2100 K) and flicker.
  - Not built: the rolled chart, the small satchel and the gloves were only for medallion pictures from the icon booth,
    which is skipped; the wheel keeps the design's painted pictures.
  - `sb.Dev.Tool <Compass|Spyglass|Lantern|none>`; tests `HeldCompassNorth`, `HeldSpyglass`, `HeldLantern`. Inventory 34/34.
  - `build_boat.py` now runs its builds only as the build script, so other scripts can use its parts.
- **Blender hero pieces (spec section 11): kept as the design's painted parts for now.** A pilot rendered the wheel's
  rope ring as real three-strand hemp (`Art/Blender/Scripts/build_ui_pieces.py`, Cycles, top-down, key light from the
  upper left). Three lighting passes gave a clean but plain CG rope that read weaker than the approved painted ring (its
  fibre highlights, deep grooves and warm gradient), and the owner's style asks for hand-painted detail (D16, D38). So
  the approved canvas parts stay; the script is the starting point if the owner wants 3D pieces (brass, wax, the crate).

### D56 · Flags that move like cloth, a calmer view on a heaving deck, waves that no longer steer the boat (Adopted)
- **Owner (2026-10-05):** "the flag physik looks like concrete, there is no movement like a flag in it"; "the camera is
  bugging up and down a lot when the waves are big"; "the waves should not change the course of the ship, at the moment
  the boat circles because the waves have to much impact on the direction. Reduce the impact on direction and leave the
  other impact."
- **Flags (M_SB_Flag):** D54's wave was a few centimetres with less than one wavelength along the cloth, so the flag
  moved as one slab. Now ripples run out along the fly (two over its length), growing towards the tail to about 6 % of
  the length (pennant ~10 cm, ensign ~5 cm), at about 0.6 Hz in light air and 1 Hz in a fresh breeze (the 2 Hz of D44
  looked frantic), with a slow sway and a small flutter at the tail. Sizes follow the flag's length.
- **Course in waves (diagnosed):** measured with `Tools/boat_course.py` (sail set, rudder amidships, 60 s in open water):
  in a storm the boat wandered through 144-218 degrees of heading and broached, against 17-43 degrees in calm water.
  The cause was not the waves' push but the rig: the sail's drive and the windage act high up on the centreline, and
  when a wave heels the boat that point swings out to the side and the same forces twist it round. Now those forces
  heel and pitch the boat but do not turn it (their twist about the vertical is left out). Storm: 32-55 degrees, near
  the calm drift; rough: as calm. `sb.Boat.RigTurn 1` brings the old behaviour back for comparison.
- **The view on a heaving deck (diagnosed):** `sb.Dev.CameraTrace [s] [deck]` logs the camera against the boat every
  frame. The camera does not jitter against the deck; the up and down is the deck itself: at open sea the swell lifts it
  about 0.5 m (rough) to 1.3 m (storm), with slams. The eyes now ride the deck's average level and the knees take 45 % of
  the swell about it, softly at most 32 cm (`ASaltPlayerCharacter::CalcCamera`, a linear filter: no lag at a steady
  rise, no limit that kinks the view). Rough: the eyes move 35 cm instead of 48, the vertical jolts halve (4.3 m/s2 rms
  instead of 8.3, the worst 25 instead of 144); storm: 103 cm instead of 121. `sb.Camera.DeckKnees 0` turns it off.
- **The boat's water speed:** the sampled surface can jump where steep crests fold; its vertical speed (heave damping) is
  now bounded to 2.5 m/s and eased in, so a jump does not kick the hull. A sudden heave (more than 0.5 m/s in a frame) is
  logged as `SB_BOAT_KICK` with its forces.
- **Lesson:** `sb.Boat.Place` takes centimetres; the first measurements ran in the cove (the boat touched rocks and the
  seabed) and were misleading. Measure in open water (`250000 -80000`).
- Tests: boat 44/44, inventory 34/34, save 5/5, swim 5/5.

### D57 · The chart moves to the mast: no map in the satchel or among the tools (Adopted)
- **Owner (2026-10-05):** "There will be now map in the player inventory or in the tools. There will be 2 map stages. 1. the
  map on the start ship, a small map pinned to the ship mast. This tiny map has a smaller covered area, it updates live
  the position of the ship and its soroundings, and it can be inspected for better view with e. 2. on the larger ships
  there will be a detailed map table with a much bigger area and more options. we will talk about that when desingning
  the new ship."
- **Removed:** the satchel's map screen (and its chart buttons), the M key, the chart on the tool wheel. The wheel now has
  five tools (compass, spyglass, lantern, satchel, empty hands), evenly round the ring, keys 1 to 5 ("1 - 5 pick
  directly"); it lays out any number of tools. The old charts (`T_Map_Home`, `T_Map_Sea`) stay generated but unused.
- **The chart on the mast (stage 1):** `SM_Boat_MastChart` (`build_boat.py`, `build_mast_chart`): a light teak board on the
  mast's aft-starboard side (it faces the helm on the starboard quarter), its middle 1.1 m above the deck, clear below the
  lantern and the boom (the sail's hoops run above the boom), held off the mast by two chocks under two iron bands; the
  chart sheet (0.26 x 0.33 m) on it with four brass tacks.
  - It shows a window of a new seamless region chart (`T_Map_Region`, the open sea's 12 km at 4096 px, about 3 m a
    pixel, `gen_ui.py`) 1.6 km across round the boat, north up, live: the boat is an ink-rimmed red arrow in its middle
    along her heading (`M_SB_Chart` / `MI_MastChart`, `Tools/ue_scripts/setup_chart_materials.py`; `ASaltBoat` sets the
    window's centre and the heading every frame). `SaltChart` in `Core/SaltPlaces.h` holds the numbers.
  - **E** ("Look at the chart") holds it up: the same live chart large (`USaltChartWidget`, `M_UI_Chart`), the places in
    view named in ink, north and a 200 m scale; E / Esc put it back.
- **Stage 2 (the map table on the bigger boat, D51)** waits for that boat's design, with the owner.
- Dev: `sb.Dev.Chart [off]`. Tests: `MastChartFollowsBoat`, `MastChartLook`, the wheel's tests for five tools and no map
  key (`WheelOnQ`); `MapMarksPlayer` is gone. Inventory 35/35.

### D58 · Owner review round: drag and drop, a camera glide to the chart, one prompt style, the ship's motion felt, steady edges, calmer home waters (Adopted)
- **Owner (2026-10-05):** "1. i want drag and drop in inventorys. 2. [interaction] on the map on the mast: smooth but fast camera
  to the mast, not a instant pop up menu with the map. 3. Match every design as we discussed, this design is not what we want use
  the default interaction design [the chart's keycap 'put the chart back']. 4. the island discovery notification is a bit too
  big. 5. if my charakter is on an edge of an physical object on the ship and the ship moves up and down because of the waves,
  my camera sometimes startes bugging - teleporting up and down realy fast. 6. my charakter seems to be not influenced by the
  moving ship in the waves? 7. Make the waves in the start area a bit more calm. 8. small but fast animation for opening the
  satchel." Answers: item 6 = "feel the rocking"; an item dropped off the pockets flies back (nothing is lost by a drag).
- **Drag and drop (satchel and stores):** the screens take the mouse themselves (the pockets' buttons are gone): hover chooses,
  a press picks the item up and moving 6 px drags it, lifted (larger, its shadow further below, its coin with it) while a faint
  trace stays in the pocket. Letting go on a pocket moves it there (`USaltInventoryComponent::MoveToSlot`): into an empty
  one, onto the same item (as much as fits), or the two change places (whole stacks); the target is chosen (satchel) or outlined
  with the dashed warm line (stores); what lands settles with a small drop. Off the pockets it flies back (0.15 s). In the
  stores it works within and across both sides; Shift at the press drags one; a click still sends the stack across, flying.
  Tests `SatchelDragDrop`, `StoreDragDrop`.
- **Opening (satchel and stores):** 0.2 s: the world behind blurs and darkens in, the screen rises 36 px into place growing
  from 97 %, eased out and fading in fast; the satchel's tag gets a swing (`SaltUI::ApplyOpening`). Closing stays instant.
- **The chart on the mast, looked at:** E glides the eyes to the real sheet in 0.45 s (smoother step: no jolt at either
  end) and they stay with it as the boat moves (`ASaltPlayerCharacter::FocusView`, a view fixed in the hull's space,
  `ASaltBoat::GetChartViewLocal`: straight in front, the sheet 78 % of the view's height at the player's field of view); E /
  Esc glide back in 0.35 s. No picture pops up: the place names, north and the 200 m scale ink in on the sheet once the eyes are
  there (the overlay projects the sheet every frame); a tool in hand is lowered meanwhile; no cursor, no focus dot. Tests
  `ChartGlides`, `ChartGlidesBack`.
- **One prompt style:** "E Put the chart back" is the key-scrap prompt (section 7) along the bottom, not a leather keycap
  (keycaps belong to the leather screens only). `USaltKeyPrompt` is the prompt as a widget (one or more keys, the action in
  painted lettering, the HUD's fade-in); the helm's controls use it too ("A D Steer", key letters A, D, W, S added to the painted
  parts) with the boat's state under it in plain painted lettering, instead of the old 14 px Roboto line; the ladder shows
  "W S Climb up and down". Test `HelmControls`. Pickup notices and the pause menu keep their look (not designed yet).
- **Landfall banner:** the whole banner at 75 % (its band's top stays at y 150).
- **Edges on a heaving deck (diagnosed, `sb.Dev.EdgeTest`):** the deck has low steps (the chest's and the rails' feet, 15-25 cm).
  Standing beside one at anchor in the open sea, the capsule's round bottom rode up and down the step's edge as the deck rolled:
  the view jumped 15-25 cm from one frame to the next, dozens of times in 4 s (the crate's and the barrel's tops were fine).
  CharacterMovement's flat base for floor checks (`bUseFlatBaseForFloorChecks`) stands the body on the step or beside it: the
  bouncing stopped at most spots (jerk back to the deck's own 0.1 cm). What is left (the body pushed onto a rail's foot now and
  then by the rolling bulwark) the view takes out: a sudden pop of the body against the deck (faster than 1.5 m/s) is eased
  back by a critically damped spring in ~0.3 s; slow changes (a ramp) pass. 30 spots: the view's largest step per frame
  2.9 cm (was 16-26 cm). Tried and not taken: a perch radius (most spots still bounced), moving with the deck without sweeps
  (worse). `sb.Camera.StepSmooth 0` turns the smoothing off.
- **Feeling the ship (owner's choice):** the view leans with the deck: 30 % of its roll and pitch, eased in 0.08 s
  (`sb.Camera.DeckTilt`, 0 = level as before); the knees take 20 % of the swell instead of 45 % (softly at most 24 cm), so the
  eyes follow the deck more. The held tools hang from the view as seen (`ViewAnchor`), so they lean with it, and the interaction
  probe follows the leaning view.
- **Home waters:** a third calm zone on the ocean round Saltbound Cove: the waves (swells and wind sea, on the GPU and for
  buoyancy) are 70 % as high within 700 m, back to full height by 1.5 km (`Tools/ue_scripts/place_home_waters.py`); Ruin Isle
  (1.7 km) keeps the open sea, the cove's own zone (15 %) still rules inside. It scales whatever sea the rig sets. A sea-state
  zone (65 % towards Moderate) was tried first and dropped: it blends the whole sea towards its state, so it made a flat sea
  rougher (the boat suite's flat-water checks failed).
- **Tests:** inventory 42/42 (new: `SatchelDragDrop`, `SatchelPointerDrag`, `StoreDragDrop`, `StorePointerDrag`, `ChartGlides`,
  `ChartGlidesBack`, `HelmControls`), edge 30/30 (`sb.Dev.EdgeTest`), movement 30/30 (drift split into sliding and height),
  swim 5/5, save 5/5, menu 7/7. The boat suite ran 42/44 with the sea-state zone (its two flat-water checks); it was not run
  again after the switch to the calm zone (owner: the long suites take too much time). Captures: `Docs/Evidence/D58/`.
- Dev: `sb.Dev.Helm [off]`, `sb.Dev.Drag <x> <y> <x2> <y2> | up`, `sb.Dev.EdgeTest [s]`.

### D59 · Owner round 2: a faster boat and a stats table, bigger stores, the ends closed, quicker ladders, snappier walking, a fuller sail you trim at the helm (Adopted)
- **Owner (2026-10-05):** "1. Make the first ship a bit faster, create lists for stats for each object like the ships. 2. Make the
  barrels and chests bigger. 3. fix this hole in the ship [the stern from above]. 4. make the going to the ladder animation
  faster - reduce the steps in the animation, make smooth upwards movement when climbing - you can climb down way faster than
  up - you can only use the ladder if pressing the interaction button. Just running in the ladder does nothing! 4.1 remove the
  climbing system for objects when you run against them. 5. the movement doesnt feel responsive and clean. 6. Make the sail
  bend more in the wind. 7. make a way to control the sail more, so you can control the speed of your ship." Answers: the sail
  with W / S at the helm; one tunable table per kind of object.
- **Stats tables (`Core/SaltStats`, `Docs/Stats.md`):** boats (speed, sail, steering, stores) and the player (walking,
  jumping, swimming, ladders, eyes); the boat reads its row (`BoatClass`, the first boat `DoubleEnder`) when play starts and it
  overrides its tunables; the player reads its row when made. Items and tools keep their own tables; `Docs/Stats.md` lists
  them all with what was measured.
- **Faster:** sail thrust 7 800 → 10 000 N: about 8.4 kn on a reach in a 7 m/s breeze (the resistance climbs steeply past hull
  speed, so the extra force buys about 15 %).
- **Sail control:** at the helm W lets the sail out and S takes it in, smoothly from nothing to full in 3 s; the drive goes with
  how much is set; the pin rails still set it fully or take it in (E). The helm's prompt reads "W S Sail   A D Steer", the status
  line "sail 60 %". Groups of keys are side by side in the HUD (`FSaltControlHint`).
- **The sail bends more:** a new `Belly` in `M_SB_Sail` presses the cloth out between mast, boom and gaff (held at all three,
  deepest a little forward of the middle) up to 70 cm beyond its cut shape in a fresh breeze; the fill follows the apparent
  wind's strength (slack in light air) and the gusts more strongly (0.7-1.35), and its spring is livelier (1.6 Hz).
- **Bigger stores:** barrel, crate and sea chest 30 % bigger (`build_boat.py` BARREL, CRATE, CHEST; the crate and the chest
  moved aft to keep them apart), their collision and the lids' E volumes with them.
- **The hole at the ends:** both ends were open: the deck planks stop short of the stem and the sternpost, the cap rails ended
  either side of the posts and the sternpost stopped below the rails, so one looked down into the dark hull. Now a breasthook
  fills the deck to each post, a cap joins the rails over each end and the sternpost rises between the bulwarks as a stern head.
- **Ladders:** moving into a ladder takes hold of it (swimming at it, or walking out at its head on deck, not on the gangway's
  side while moored); E still works. On to it in 0.12-0.35 s (3.2 m/s), one smooth glide at 1.9 m/s the same up and down, eased
  in and out (no rung-by-rung pulls, no dip of the head), it stops where the keys are let go, and over the top in 0.35 s.
- **No climbing on to things:** Space at a ledge no longer climbs out of the sea on to the pier, the boat's side or a rock (the
  climb-out is gone): out of the sea by the beach or up the ladders.
- **Snappier walking:** acceleration 1 500 → 2 800, braking 1 800 → 2 600, friction 8 → 10 (full speed and a stop in about a
  tenth of a second); the jump the same height but quicker (420 cm/s at 1.5 x gravity, was 340 at 1 x); air control 0.35.
- **Tests (short runs only, owner):** the boat suite runs a part alone now (`sb.Dev.RunBoatTests ladder`, `... WindAbeam`): ladders
  11/11 (up 0.87 s and down 0.88 s in one glide each, `BoatLadderTouch`: swimming at it takes hold), a beam reach 8.37 kn (was
  about 7.4), swim 5/5 (`no_climb_out`). The movement suite and the rest of the boat suite were not run again. Evidence:
  `Docs/Evidence/D59/` (the ends and the stores in Blender, the sail from above in light and fresh wind, the helm's prompt).


### D60 · The prompt on its object or low on the screen; the home island: a tropical ring round a lagoon with the harbour inside (Adopted)
- **Owner (2026-10-05):** "1. there will be 2 modes for the interaction line to stay: First is its pinned on the object so if
  you turn around the interaction message moves with the object. The second option is its how its now but way lower, sub title
  level position... 2. Create (in blender when useful) a much more interesting start island with nature trees, palms etc etc. a
  tropic island in form of a circle with an exit, in the middle water and the harbor".
- **Prompt position (pause menu, `bPromptOnObject` in the user settings, `sb.PromptOnObject 1|0`):** *On the object* (the
  default) pins each prompt row on its object's prompt point, the focus volume's middle (`PromptOffset` moves it), projected
  every frame and kept whole on the screen; a row fading out stays where its object was. *Bottom of the screen* puts it in the
  middle 180 px above the bottom edge (1080p), where subtitles go, just over the controls row. Leaving a station or a ladder has
  no object, so it is always low. Two objects with the same words (two pickups) count as a new prompt when pinned. Checks:
  `MenuPromptPlace`, `PromptOnObject` (on the windlass, looked at off its middle: the row within 3 px of it), `PromptLow`.
- **The home island (`island_shape.py`, `build_home_island.py`, `build_vegetation.py`):** a ring of land 50-95 m wide round a
  lagoon some 220 m across, about 400 m from reef to reef. The old cove's pier, its beach and the 2.3 m of water under it are the
  lagoon's south shore (the mooring, the gangway, the pier and the swimming checks keep their places); its old sandbank and
  headland rocks are gone.
  - Inside: a turquoise rim 2-2.5 m deep, a floor 6 m down with sea grass, four coral heads and a small sand islet with three
    palms. The calm zone keeps the lagoon at 12 % of the swell.
  - The ring: beaches inside and out with the cove's sculpted sand, grass inland (`SM_Home_Turf`: its edge rolls down under the
    sand, a chunky lip; forest floor under the trees), gentle rises, and a 14.5 m hill in the west with a rock crown. A sandy
    footpath leads from the pier's beach to the hilltop (flowering bushes along it).
  - The channel: east, across the usual south-south-west wind, so it is a reach both ways; about 52 m wide and 5 m deep between
    two rocky headlands. Measured: with the rudder left alone the boat sails out at 8.4 kn; coming in unsteered it drifts on to
    the north headland (it needs steering).
  - Outside: a reef flat 2-2.6 m deep (turquoise, passable), the drop-off to the 60 m floor, rocks in the west coast's surf; a
    second calm zone shelters the outer beaches (45 % at the coast, the full swell 450 m out).
  - Plants: 363 palms in groves along the shores (three kinds: tall and curved, leaning far over, young), 206 broadleaf trees
    in clumps inland, 450 bushes, 68 flowering bushes, 159 big-leaf plants, 66 ferns and about 4 900 clumps of beach grass. All
    geometry leaves (no cut-out cards), Nanite with "preserve area" (without it the palms' thin leaflets thinned away to bare
    trunks from afar), the trunks collide (UCX), the rest does not.
  - Materials: `M_SB_Foliage` (two-sided foliage) and `M_SB_FoliageBark` read the plants' vertex masks (tone, age, a random
    value, how free a part is) and bend the whole plant downwind with gusts, the leaves fluttering on top; bark and leaves of a
    plant share the bend. `MI_Turf` puts painted grass and forest-floor textures (`gen_textures.py grass forest_floor`) on the
    rock master.
- **Knock-on changes:** the Spire moved out to (560, 260) m (the island's north beach would have counted as landfall there);
  the charts draw the island from its shape (`gen_ui.py`); the boat tests' open water moved south of the island
  (`TestWater`, the waves part), as their old spot is in the lagoon now.
- **Tests (short runs):** menu 8/8, UI suite 44/44 (with the prompt checks and `LandfallFoundOnce` on the north beach), the boat
  suite's pier, grounding, float and mooring parts, swim 5/5, the channel run. Evidence: `Docs/Evidence/D60/`.

### D61 · The hub ship, phase 1: the blockout in its own map, measured (Provisional: waiting for the owner's review)
- **Owner (2026-10-05):** "Build the hub ship for Saltbound ... follow Docs/Design/HubShip/HubShip_Agent_Prompt.md ... Then do Phase 1
  only: the blockout in its own map, L_HubBlockout. Stop there and give me your measurements for review." And: "Use the defaults in
  section 2 as my answers": the name Hollowkeel, salt as the currency (a purse), she first appears after the first landfall, 160 m,
  entry only by sailing into the stern dock, the ancient hull visible at the waterline, no selling of finds yet.
- **Built:** `Art/Blender/Scripts/hub_layout.py` (the loader: every number from `HubShip_Data.json`), `build_hub_blockout.py` (15
  meshes `SM_HubBlk_*`: the hull's shell in four parts, the basin, the five decks' open floors with their walls, ceilings and rails
  (walls along the open zones' edges, the doors and windows of the data cut in), the stairs (visual steps, ramp collision), the 34
  gates, the props as boxes, the masts, the markers), `Tools/ue_scripts/setup_hub_blockout_materials.py` (flat colours),
  `create_hub_blockout_map.py` (`L_HubBlockout`: sky, sea and rig as in the visual test map, the ship at her berth, the calm zones,
  the player's boat moored at the fitting berth, the player on the starboard quay, labels at the NPC spots and gates, lanterns),
  `hub_checks.py` and `Tools/hub_blockout_checks.py` (the measurements). The boat got `MooringPierHeight` (the gangway's and lines'
  shore end: the cove's pier 111 cm, the hub's quays 150 cm).
- **Data added or changed** (`hub_data.js`, the JSON and the sheets re-rendered): the hull section in numbers (`section`,
  `ancientTop`: the values of `outside.hull_section`, nothing new); the berths moved off the basin wall: fitting berth y 8.3 → 7.9,
  fleet berths -8.3 → -7.9 and -7.8 → -7.5 (at 8.3 the 3.4 m boat lay on the wall: 18 contacts moored; at 7.9 one at 0.0 cm).
- **Measured** (short runs, each part alone):
  - Walking from the fitting berth (2.5 m/s, simulated input steering through every door and flight): the Slip 20 m in 7.7 s, the
    Tally House 27 m in 10.0 s, the Keelgate 85 m in 32.7 s, the Chandlery 162 m in 63.1 s (about 38 s sprinting). Every door,
    stair and gallery on the way passes.
  - Headroom: 3.70 m clear over every open floor (the Tally House 3.15 m in its hull corner), none under 2.2 m; the only low
    readings are within 0.25 m of the bulwark, where the hull's tumblehome leans over the deck's edge.
  - The boat: sails in under the arch at about 8 kn along the centreline (0.9 m off it at most, 7.4 m from the quays), coasts on to
    the slipway and grounds softly; turns on the spot with the oar (171 degrees in 40 s, drifting 4.8 m, 3.6 m clear of the quays);
    sails out cleanly; lies moored at the fitting berth with the gangway on the quay. The arch is 21.5-25.5 m clear, the boat's mast
    10.5 m.
  - The sea at Storm (the CPU's long waves, 60 s): with the prompt's calm zones (30 %, basin 10 %) it reached +1.56 m under the hull
    and +0.51 m in the basin; with 20 % and 7 % it stays under +1.05 m and +0.36 m (targets 1.2 and 0.4).
  - Light: with the game's presets (lanterns out by day, the exposure clamped for the open sea, EV 11-15.5 by day) the Tally House
    was black (mean 0 of 255) and the Keelgate dim (14). With an exposure volume per enclosed space (minimum EV 3) and lanterns that
    burn by day (60 cd at the data's spots): Day / Dusk means of 109 / 40 (Slip), 58 / 35 (Tally House), 116 / 47 (Keelgate),
    111 / 65 (Chandlery). Evidence: `Docs/Evidence/D61/`.
  - The berth: the floor is 47.5 m or more under her keel (8 of 40 points over no floor at all); she does not hide the Spire from
    the pier (292 vs 25 degrees).
- **For the owner's review** (proposals, nothing changed yet):
  1. The berth and the home island (D60): the design's berth (230, -555) m lies west-north-west of the cove with "stern to the cove
     so you sail straight from the pier into her dock", but the pier is now inside the lagoon and its only way out is the east
     channel. Proposal: (81, 415) m, bow to 92 degrees: her stern 130 m off the channel's mouth, framed in the channel from inside the
     lagoon (checked: 47.5 m of water under her, the Spire clear).
  2. The calm zones: the visual test map already uses all four (lagoon, island lee, Ruin Isle, home waters); hers make six. Raise the
     ocean's limit to eight (shader and CPU) in phase 4.
  3. The Chandlery is 63 s away on foot (over the 40 s target): open the Keel Walk (L5, "the first shortcut") to the Keelgate and its
     hoist, or a companion from the dock head to the main deck.
  4. Interiors need their own exposure and lanterns lit by day (both in the review map now).
  5. Data to settle: the derrick (9 m high at y 12.6) stands under the arcade's roof (z 5.5); the slipway starts at x 30 in
     `hub.ramp` but at 32 in `spaces.dock` (built from the latter); the deck outline is 40 m wide but the hull section has 37.8 m at
     the main deck (the Chandlery's back wall at y -19 then meets the bulwark).
- **Logic pass (owner, 2026-10-05):** "Check for Design issues and Logic of the Ship. Everything needs to make Sense think back
  to the Bell…" (D53: every object starts from its purpose; nothing floats or hangs from nothing). Changed in `hub_data.js`
  (the JSON and all sheets re-rendered), the blockout and the map:
  - **Seen from the player's eyes (1.4 m):** the bulwarks and the taffrail were 1.2 m, so the sea did not show from the main deck.
    `hub.rail` is now 0.9 m (the hull's tops 18.4 / 26.4 / 34.4). Counters 0.95, window sills 0.8-0.9, the spyglass on its
    tripod 1.15, the binnacle 1.0.
  - **What holds her and steers her:** the lashed wheel had nothing to steer and nothing held her in place. Now twin rudders hang
    on the wings' sternposts (`hub.rudders`; the dock mouth splits the stern), with the wheel's ropes running to their tillers.
    Two bow cables run from the hawse holes to anchors ahead (the capstan heaves them), and two stern moorings run from the wings'
    quarters, splayed clear of the approach (`hub.cables`). A bobstay holds the bowsprit down.
  - **Hull and water:** the dock's floor (z -5) lay below the stern's bottom, so the stern now goes down to z -6. The Deep
    overlapped the slipway, so it starts at x 46. The slipway begins at x 32 in both `hub.ramp` and the dock.
  - **Ways:** the port quay was a dead end, so its fleet berths could not be reached on foot. A port landing (x 44-48) now joins it
    to the boat hall, and the timber store's grille moved to its side.
  - **The Slip:** customers came in behind the counter. The counter now stands across the shop facing aft, toward the door from
    the quay, with the shipwright behind it (51.4, 13.2). The plans board hangs by that door, and the workbench stands at the hull
    wall.
  - **The Slip's derrick:** it was 9 m high, but stood under the arcade's 4 m roof. It is now a wall crane bracketed to the wing's
    face over an arcade post: the jib is 11 m up (clear of the boat's 10.5 m mast), the hook hangs over the berth, and the fall
    runs to a winch under the arcade.
  - **Mooring:** the quay said "Moor here", but the boat moors from its own windlass. The quay now has a berth board ("Read the
    berth board").
  - **Tally House:** the pay booth had no door; it now has one in its forward side. The purser's-stores door is by the booth.
  - **Keelgate:** the reader stood with the desk behind them. The desk now stands between the reader and the gate they face, with
    a chair on the wall side.
  - **The Well:**
    - The hoist had only ropes. It now has guide posts, and a counterweight on the gantry beam's second sheave that rests in a
      railed bay (no one walks under it). A brake on a hand line is worked from the cage, and the door is on the aft side.
    - A 0.3 m coaming now runs round the opening on the main deck.
    - The ring lantern hung from nothing; it now hangs on a chain from the gantry's apex.
  - **Main deck:** the Chandlery's back wall met the bulwark and the main mast's shrouds, so it moved 1.5 m inboard (y -17.5),
    with its shelves inside. The cargo rope now runs on stanchions every 4 m. Lanterns now hang on the dock's arcade posts, the
    awning and the fife rail (33 lamps in the map).
  - **Her berth:** she now lies at (81, 415) m, bow to 92 degrees, off the lagoon's channel (proposal 1 above; it was behind the
    island from the pier).
  - **The long section:** your boat was drawn at a fleet berth; it is now at the fitting berth. The section also draws the rudder,
    the anchor cables and the bobstay.
- **Measured again** at the new berth:
  - **Walking:** the Slip 11.8 m in 4.6 s, the Tally House 26.6 m in 10.2 s, the Keelgate 86.6 m in 33.4 s, the Chandlery
    162.3 m in 63.1 s.
  - **Headroom:** 3.70 m, and low only within reach of the bulwark.
  - **The boat:**
    - In under the arch: 0.6 m off the centreline at most and 7.7 m from the quays; it grounds softly on the slipway.
    - Turning: 168 degrees in 40 s, 4.97 m clear of the quays. Out: clean.
    - Moored: 0 contacts (lying at y 7.65), with the gangway on the quay at +1.5.
  - **Storm:** +1.06 m under the hull, +0.34 m in the basin.
  - **Area:** 6 466 of 31 234 m2 open (20.7 %). Evidence: `Docs/Evidence/D61/logic_pass.png`.
- **Still open:**
  - The Chandlery is 63 s away (proposal 3).
  - The Well has no skylight.
  - The calm zones (proposal 2).
  - The space sheets 08-11 still show the layouts from before this pass (the derrick, the old counters).
  - The last flight arrives on the main deck facing starboard, away from the Chandlery.

### D62 · The hub ship, phase 2: the art pass, a giant old galleon (In progress)
- **Owner (2026-10-05), after the logic pass:**
  - "It needs to Look like Giant ship from outside. Best case also a Bit older and handcrafted Like everything Else. That also
    Applies for EVERYTHING inside ... you need to Look at Everything, every Wall every Corner floor etc. etc. Style like my
    current ship."
  - "Style like a Giant Gallion."
  - "The Open for Ships to Sail in the hub needs to be able to open and close ist with ropes like a Castel Gate."
  - This starts phase 2 of the hub prompt (the art pass), with the gate added.
- **How it is built:**
  - `Art/Blender/Scripts/hub_timber.py` generates every board, beam, post and turned piece straight into one mesh per part.
    Each board is a chamfered, cupped and grooved section swept along its frame.
  - The boards follow the boat's rules (D38, D52): each has its own seed, drifting width, wobbling edges, a gentle bend and
    twist, and a few millimetres proud or sunk; worn chamfers, ends cut askew, and its own shade from a material's variants.
  - Vertex masks as M_SB_Surface reads them: R wear, G grime and wet, B tone.
  - Sb_timber's object boards (about 3 000 vertices each) would not scale to the hub's tens of thousands. The whole outside
    (about 9 000 boards) builds in about 30 s.
- **Visual only:** the blockout stays in the map as hidden collision, so every D61 measurement still holds while the art
  replaces it piece by piece. The review map is `L_HubReview` (`create_hub_blockout_map.py art`); `L_HubBlockout` stays the
  plain review build.
- **Data changed for the galleon** (`hub_data.js`; the JSON and the sheets re-rendered):
  - The sterncastle rises in two tiers (`hub.poopX` 22): the quarterdeck over x 22..44 (rail 26.4) and the poop over
    x 0..22 (rail 34.4). It was one 34 m wall. The upper storeys sc2 and sc3 shrink to x 0..22, so the total area is now
    30 354 m2 and the open share 21.3 %.
  - A sheer (`hub.sheer`): the bands rise 0.8 m towards the stern and 1.4 m towards the stem; the decks inside stay level.
  - A beakhead (`hub.beakhead`): a grated platform at z 19.5, headrails up to the forecastle, the figurehead at its tip.
  - The rudders get a real blade, 3.2 m at the heel narrowing to 1.2 m at the head (they were 1.2 m posts). They move to
    y +/-15, so they stay inside the stern face down to its foot.
- **Built so far: the outside** (`build_hub_hull.py`, `build_hub_ends.py`, `setup_hub_materials.py`; 9 meshes `SM_Hub_*`,
  Nanite):
  - Strakes 0.55 m wide, with staggered butts, on a dark caulked core.
  - The ancient lower hull: pale seamless stone with two thin turquoise lines, its stern face and the stem foot.
  - Wales: a tarred one over the joint with the stone, ochre ones at the deck lines.
  - 174 lidded ports: 115 shut in ochre-edged frames, 45 propped open (oxblood inside, like the boat's hull), 14 boarded
    over where the lid is gone.
  - Cream-framed castle windows: glass, lit, or shut behind oxblood shutters.
  - Channels on knees with deadeyes and chain plates.
  - Cap rails.
  - The transom:
    - The dock mouth's arch with a heavy rib, ochre springers and keystone.
    - The seven pointed great windows between oxblood pilasters.
    - The frieze with HOLLOWKEEL in carved brass letters, and the taffrail on turned balusters with a brass crest.
    - The great stern lanterns on the corner posts, and the lashed great wheel.
    - Twin rudders with iron straps and their hoods, and the stern moorings through iron fairleads.
  - The quarter galleries: two tiers of windows, oxblood boards, a brass drop and a dome.
  - The castles' teak decks with balustrades round the courtyard, and their fronts with windows.
  - The courtyard walls over the dock, with rows of shuttered windows, and the arch's vault and forward face.
  - The head: stem, knee, beakhead grating, headrails, a carved scroll figurehead with a brass crown, the bowsprit with iron
    bands, catheads, iron-rimmed hawse holes with the bow cables, and the bobstay.
- **The sea gate** (`hub.seagate`, `build_hub_seagate.py`, `Hub/SaltSeaGate`):
  - A 20 m portcullis of tarred oak (iron-shod points, bolted plates, a head beam with four iron eyes) in grooves just
    inside the stern face.
  - Down, it closes the basin to boats: its points sit in a sill on the floor and its head is 4 m over the water.
  - Raised 26.5 m into a slot in the gatehouse over the arch, its points hang at the springing (21.5 m), clear of any mast.
  - Four ropes run up over sheaves and down through the wings to a capstan on each quay by the mouth. E at a capstan
    raises or lowers it in 24 s, easing in and out: the ropes pay in and out, the capstans turn eight times.
  - It starts open (a boat can always come home); since D63 the save keeps its state. `sb.Hub.SeaGate open|close|toggle|snapopen|snapclosed`.
  - The quays' rung ladders moved from x 1.5 to 3.5, out of the grooves.
- **The rig** (`hub.rig`: the numbers of outside.masts, plus the galleon's lateen and spritsail yards; `build_hub_rig.py`):
  - Each mast is stepped in three pieces: the lower mast with rope wooldings up to a round fighting top (plank platform on
    trestle and cross trees, an ochre rim, a rail open forward, the lubber's hole), the topmast fidded forward of it under a
    cap, and the topgallant mast to the truck.
  - Square yards hang from slings and parrels, with lifts and sagging footropes; their sails are furled in rolls bound by
    gaskets.
  - The mizzen's lowest yard is a lateen slung fore and aft. The spritsail yard hangs under the bowsprit.
  - The standing rigging is tarred: shrouds from the channels' deadeyes (pairs with lanyards) with ratlines every 0.5 m,
    futtock, topmast and topgallant shrouds, backstays, and stays forward to the next mast or the bowsprit.
  - The pennant flies from the main truck on the flag material, oxblood over cream, streaming aft: she lies head to wind on
    her anchors.
  - The blockout's masts stay as hidden collision.
- **Inside: every open space** (`build_hub_inside.py`; 9 meshes):
  - **How it lies on the collision:** the art follows the blockout's own construction (the same wall runs along the open
    zones' edges, openings, flights and prop boxes). Floors' tops sit at the deck heights and walls' faces on the collision
    walls' faces, so the hidden blockout still collides and L_HubReview now draws only art.
  - **Materials:** deck planks fore and aft over caulked seams, with the quays in weathered oak.
  - **Walls and ceilings:** walls of planks in rows on frame posts, with a skirting and a top plate, and frames round
    every door and window. Decks overhead sit on beams with hanging knees.
  - **The hull's inner side:** a lining on curved frames that follows the tumblehome, with the ports through it.
  - **Rails:** turned balusters, like the castles' balustrades.
  - **The Stern Dock:**
    - Ancient stone basin walls with oak fenders, mooring rings and iron rung ladders.
    - The slipway's oak ways and sleepers.
    - An iron-shod kerb, and arcades of posts on iron shoes with braces to the edge beam under the wing.
    - Bollards, lanterns on brackets, the berth board, and the wall crane with its fall, hook block and winch.
  - **The Slip:**
    - The 12 m hall on heavy beams.
    - The counter with its ledger, the plans board, the half-models, the workbench with a vice, the timber racks.
    - The 14 m cutter on her cradles in the boat's oxblood and cream, and her mast on trestles.
    - Lanterns on chains, and SHIPWRIGHT over the hall's mouth.
  - **The dock stair:** 22 oak steps with handrails.
  - **The Tally House:**
    - The job board with six slips, the TALLY header and a slate.
    - The pay booth: grilled hatch, ledge, door and PAY.
    - The weighing bench with its brass balance, sacks, salt cakes, the long table and its benches.
  - **The Well:** galleries with balustrades, four flights with balustrades and boarded soffits, and lanterns on the
    corners. The gantry has guys and a brake drum. The hoist's cage stands between its guide posts, with its ropes, the
    counterweight in its frame and its railed bay.
  - **The Keelgate:**
    - The seal in pale stone with its glowing circles and spokes, seven empty sockets, and a stone apron.
    - The reader's desk with books, a brass lamp and an open book, and the chair.
    - Shelves of salvaged pieces, a quarter glowing faintly, and the rubbings.
  - **The main deck:**
    - The deck, the waterways, and the bulwarks' inner side on stanchions.
    - The Well's coaming and rails, and the hooded companion.
    - Fife rails with belaying pins and coils, the masts' coats and wedges.
    - Grated and padlocked cargo hatches, the ship's boats under canvas, the great capstan, the galley's stovepipe.
  - **The Chandlery:**
    - Cream-painted walls, the counter front, the door standing open, the striped oxblood-and-cream awning, CHANDLER.
    - A shingled roof on rafters.
    - Shelves of tins, jars, coils and boxes, with a ladder on a brass rail.
    - The display table with compass, spyglass and lantern, and beams hung with lanterns, coils and a net.
    - The tripod spyglass, the binnacle, barrels and crates.
  - **The gates:** barred doors (G1), rope and a board (G2), chained ladders and padlocked hatches (G3), shuttered bays (G4),
    iron grilles with timber behind (G6), and the cargo rope with its CARGO board.
- **Still to do or to decide:**
  - The figurehead is a carved scroll for now.
  - The sea gate's state is saved since D63; the hoist starts at the quay.
  - The space sheets 08-11 still show the layouts from before D61.
  - The section and exploded sheets draw the sterncastle as one block.

### D63 · The hub ship, phases 3 and 4: her systems, her own level, into the game (Provisional: waiting for the owner's review)
- **Owner (2026-10-05):** "Continue with Phase 03 and 04 I will Review everything when I am back" (the hub prompt's sections 7
  to 9).
- **Her own level, `L_Hub`** (`Tools/ue_scripts/build_hub_level.py`): the ship alone, her pivot (x 80, the waterline) at the
  origin, bow to +X. What goes in is shared with the art review map through `hub_place.py`, so `L_HubReview` and `L_Hub` hold
  the same ship:
  - the art (visual only) over the blockout as hidden collision (its gates and markers left out: the gate actors block);
  - the sea gate, the pennant, the lanterns' lights (35) and the exposure volumes below decks (6);
  - the gates, the E points, the hoist, the quay ladders and the moorings (below);
  - `ASaltHubTestData`: the data's open floors, the walks, the sea's sample points and the dock's numbers for the tests.
- **`ASaltHubShip` and `ASaltHubBerth`** (`Hub/`):
  - The ship streams `L_Hub` in at the active berth (`ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr`) and unloads
    the old instance when she moves. While she lies at a berth, its calm zones go to the ocean (the ocean takes 8 calm zones
    now: shader, `MPC_Ocean` and `MaxCalmZones`; the visual test map uses 6).
  - **Follow rule:** she lies at the berth of the harbour the player last sailed to or moored at (within 1 200 m of it) and
    moves only while the player is more than 2 500 m from both berths, checked every 2 s; never while the boat lies moored to
    her. Today there is one berth; the test spawns a dev berth 6 km away.
  - **First sight:** hidden until the first island is found (`FoundPlaces`), then she appears when the player is beyond 2.5 km
    or looking more than 75 degrees away; `sb.Hub.Show 1` shows her at once.
  - Her berth `SaltboundRoads` and the ship actor are in `L_VisualTest` (`create_visual_test_map.py`, via
    `hub_place.berth_and_ship`).
- **Mooring in the hub:** `USaltMooringPointComponent` (on `ASaltHubMooring`) at the fitting berth and the two fleet berths: the
  boat's pose, which side the quay is and how far, the quay's height. The windlass moors to the nearest mooring within the
  boat's 8 m mooring radius, the boat's own start mooring at the pier included; the lines and the gangway go to the side the
  mooring names. Mooring saves (D46).
- **Gates** (`ASaltHubGate`, 33): a blocker where the blockout's gate box stands (the boxes now come from `hub_layout.gate_box`,
  shared with the blockout and the art), blocking while its zone (L1-L17) is locked; E shows the line. The data got a prompt and a
  line per gate type: "Try the door" / "Barred from inside.", "Read the board" / "Closed for now." (the cargo rope's own line
  "Cargo work, keep clear."), "Try the chain" / "Chained and padlocked.", "Try the shutters" / "Shuttered from inside.", "Look
  through the grille" / "Stacked timber, and no way in.". The Keelgate (G5) blocks nothing.
- **E points** (`ASaltHubPoint`, 12): at every E of the spaces with the data's prompts; they log what they would do.
- **The hoist** (`ASaltHubHoist`): a station in the cage: W up, S down, one stop per press (quay, lower deck, main deck), easing
  in and out; while it moves the rider is held inside ("Hold on"). E at a stop's rail gap calls it. The rail gaps at the lower and
  main deck are roped and blocked while the cage is elsewhere. The counterweight runs the other way on its own rope. The cage and
  the counterweight are their own meshes now (`SM_Hub_HoistCage`, `SM_Hub_HoistWeight`); the gantry, the guide posts and the
  weight's bay stay in the Well's art. The yoke's ropes now run from the cage's corner posts. The brake's hand line is gone (it
  cannot stretch with the cage); the lever stays in the cage.
- **Quay ladders** (`ASaltHubLadder`, 4, the D54 ladder): E or swimming into it takes hold; W climbs onto the quay.
- **Save version 5:** the boat's mooring, the hub's berth, the unlocked zones, the salt purse and the sea gate's state.
- **Dev commands:** `sb.Hub.Show 1|0|-1`, `sb.Hub.Berth <id>`, `sb.Hub.Unlock <L#>|all|none`, `sb.Hub.Info`,
  `sb.Hub.Hoist <stop> [now]`, `sb.Dev.Salt <n>`, `sb.Dev.HubGoto dock|slip|tally|well|keelgate|main|chandlery`.
- **Tests** (`sb.Dev.RunHubTests [part]`, `Dev/SaltDevHubTests.cpp`; in `L_VisualTest` with her streamed in), each part alone:
  - placed: at the berth to 0.00 cm and 0.000 degrees; the sea floor 47.5 m under the keel at 40 points.
  - sealow: Storm for 60 s: +1.20 m under the hull (right at the limit), +0.36 m in the basin.
  - nolowfloor: 5 854 samples, none below +1.5 m (the slipway excepted).
  - headroom: 18 136 spots where a player can stand, the least 3.7 m clear. Spots against a wall's foot are left out (a knee-high
    sphere touches the wall): the first run found 4 samples at the bulwark's foot under its lean, where nobody stands.
  - route: the Slip 4.6 s, the Tally House 10.2 s, the Keelgate 33.4 s each way; the Chandlery 63 s (fails the 40 s target, as in
    D61: proposal 3 is still the owner's to decide).
  - gates: all 33 stop the player and show their line; unlocking all opens all 33, locking closes them again. The forecastle's
    four (L14) stand behind the cargo rope (L15), so nobody reaches them yet: they are checked by their blocker.
  - boatdock: in under the arch with no contact (7.8 m from the quays), moored at the fitting berth (29 cm off the pose) and at a
    fleet berth (11 cm), each mooring kept through a save and a load, out again with no contact.
  - slipway: driven at it at 2.5 m/s, it runs aground softly (0.7 cm deep) and lies still.
  - hoist: up 4 m in 4.8 s, up 12 m in 9.7 s, the rider inside all the way, E while moving does nothing, stepping out onto the
    main deck; down again in 9.8 s and 4.8 s.
  - follow: she stays while the player is in the dev berth's harbour, moves when the player is 3 km from both.
  - save: the berth, the zones and the salt come back. ladder: from the water onto the quay in 2.2 s.
  - The boat's mooring and ladder parts and the save suite pass after the mooring and save changes.
- **Packaging:** the cook had been failing since the FFT ocean (D27), for two reasons:
  - Its in-place FFT reads typed UAVs, which the D3D11 SM5 fallback rejects. The FFT shader now opts in
    (`CFLAG_AllowTypedUAVLoads`); DX12 / SM6 is unchanged.
  - The cook loads the maps with a null RHI and the ocean still dispatched its FFT there, which trips an engine ensure. The
    FFT now does nothing where nothing can render (`FApp::CanEverRender`).
  - The hub's folder is always cooked (`/Game/Saltbound/Hub`): the hoist resolves its meshes by path.
- **Measured in the packaged build** (Development, 2560 x 1440 fullscreen, the editor closed, RTX 4070 SUPER; `Run-Build.ps1` with
  `r.VSync 0, t.MaxFPS 0, sb.Hub.Show 1`, then `sb.Dev.HubGoto <space>` or the pier spot, then `sb.Dev.Benchmark 10`; average fps,
  Day / Evening / Dusk):

  | Where | Day | Evening | Dusk | GPU, frame p95 (worst preset) |
  |---|---|---|---|---|
  | The pier, looking at her | 74.4 | 69.5 | 73.9 | 13.7 ms, 15.2 ms |
  | The pier, her hidden (`sb.Hub.Show 0`) | 75.1 | 69.4 | 73.6 | 13.8 ms, 15.2 ms |
  | In the dock (the starboard quay) | 70.7 | 71.3 | 72.4 | 13.4 ms, 15.3 ms |
  | On the main deck | 75.8 | 74.9 | 78.3 | 12.7 ms, 14.4 ms |
  | At the Keelgate | 75.8 | 75.3 | 75.5 | 12.6 ms, 13.9 ms |

  - Every spot meets D8 (60 fps). Seen from the pier she costs about 0.1 ms.
  - Before the lanterns' reach was cut (below) the Keelgate measured 62.1 / 61.5 / 61.6 (GPU 15.5 ms, frame p95 16.9 ms): the
    lanterns' shadows took 4 ms there (`profilegpu`).
  - Runs vary by 2-3 fps; one whole series came out 5-8 fps lower (the pier 65.9 / 61.3 / 66.1, with her hidden 72.9 / 66.7 /
    68.9 in the same series). The table is the repeat.
- **Two fixes to the art pass (D62) found while measuring:**
  - **Light leaks in the Well:** a pale line ran along the Keelgate chamber's walls under the lower gallery, and the seams of the
    galleries' floors glowed from below. Over the galleries the deck is beams only, and the boards seen from below are the floor
    above; its dark seam backing faced up only, and the walls stopped 0.24 m short of those boards. Every wall now has a dark
    backing behind its top, up into the deck's slab (up to the floor's boards where there is no boarded ceiling), and the
    floors seen from below have a down-facing backing inside their boards. Evidence:
    `Docs/Evidence/D63/D63_keelgate_dusk_light_leak_before_after.jpg`.
  - **The lanterns' reach:** 16 m to 10 m (`hub_place.LANTERN_RADIUS`). 60 cd gives under 1 lux beyond 8 m, but with 16 m each of
    the Well's 14 lanterns lit the whole shaft and their shadows cost 4 ms at the Keelgate. No visible change at Dusk.
- **Still to do or to decide:**
  - The Chandlery is 63 s away on foot (D61 proposal 3: the Keel Walk shortcut, or the hoist from the Keelgate as the way up).
  - A far view (HLOD or a far mesh) for her silhouette, and her shadow at a distance (phase 2's last item).
  - Opening a gate does not remove its art yet; the zones behind are not built (not now, prompt section 11).
  - The hoist's stop is not saved (it starts at the quay).

### D64 · The hub ship redesigned after the owner's review: side doors into a water hall, a galleon's stern, 3 m decks with every room built (Provisional: waiting for the owner's review)
- **Owner's review (2026-10-05):**
  - "Cover the white bottom part of the ship with the same muster you use higher up with the normal planks and windows."
  - "The hull shape is too boring and static, a bit more shape detail overall would be nice."
  - "The boat will have a cavity in the bottom deck filled with water so other ships can enter from THE SIDE. But its a bit hidden
    its not a big obvious gate its the hull that opens inside like a double door ... The ship will get more rounder, wider and
    bigger at the back top like a real galleon. Build all the decks and rooms you dont need to design every room. Make the decks
    cozy and not too big. Look in the details an check if what you build makes sense."
  - Answers to my questions: doors in both sides; decks 3 m apart; boats keep their masts up (first "the boat lowers its mast",
    then "make a concept where the ship can dock with full mast").
- **The layout** (`hub_data.js`, review drawings `Docs/Design/HubShip/HubShip_D64_*.png` from `Tools/design/hub_review.py`):
  - **Floors 3 m apart** (2.7 m under the slab, 2.4 m under the beams): the Deep −4.5, the hold −1.5, the dock deck +1.5, lower,
    middle, upper and gun decks, the main deck +16.5; the sterncastle three storeys over it (the quarterdeck at 22.5 over x 24..48,
    the poop at 28.5 over x 0..24), the forecastle one (22.5).
  - **The Water Hall** (x 26..50, the bottom deck): sea water from side to side between two quays (aft x 26..30, forward x
    46..50), open up to the gun deck (13.2 m clear over the water: a mast up to 12 m passes), galleries over both quays on the
    lower, middle and upper decks, the forward starboard one open (the door into the Tally House), the others the rooms' behind
    them. The slipway runs up from the water through the forward quay into the Slip's hall (x 46..60); over it the galleries
    stop (masts) and the hold is walled off round it.
  - **The hidden doors:** a pair in each side at x 32..40, from under the keel of a boat (z −2.5) to under the gun deck (13.0);
    each leaf 4 m, the hull's own planking (wales and port lids run on across them). Shut, a seam and the hinge straps show.
  - **The stern** is a galleon's again (no dock mouth): round in plan, leaning aft, wider at the top (17.4 m half-breadth at the
    taffrail), two open stern galleries, quarter galleries, one rudder.
  - **Every deck and room built** (`hub_plan.py`): outside the open spaces each floor becomes two corridors fore and aft, corridors
    across at fixed stations and rooms of about 5.5 m between them (893 rooms), every room with a door.
    Where a locked zone's corridor meets an open space a gate stands (32 such gates, 39 with the data's own marks); two corridor
    mouths with a stair in front of them stay walled (no room to stand at a gate there).
- **The doors in the game** (`ASaltHubDoors`, replacing the sea gate):
  - The leaves cannot turn about the hull's skin (a leaf curved like this hull would swing through the fixed planking), so each
    turns on iron arms about a hinge post standing in the hall, 1.4 m beyond its jamb (`build_hub_doors.py`). The seam is
    bevelled and the aft leaf leads by 10 degrees. Open, they stand along x 30.3..33.2 and 38.8..41.8 to |y| 12.3; the clear way
    between them is 7.1 m at the waterline, 5.65 m at its narrowest (z 6).
  - **The dock watch:** they open by themselves for a boat outside the hull within 45 m, or one inside heading for them, and shut
    again 20 s after it is in and clear of their swing; E at a capstan on the aft quay opens or shuts them by hand. They never
    swing into a boat (a leaf holds while one is in the rest of its swing). Not saved: they start shut.
  - **Berths moved clear of the swing:** the fitting berth along the forward quay's starboard side (unchanged), the 10 m fleet
    berth mirrored to its port side (the fore leaves never pass x 41.4), the 14 m fleet berth in the middle of the aft quay (the
    aft leaves sweep the quay's ends beyond |y| 12).
- **Looked at in the details** (eye-level previews of every designed space, then fixed):
  - The 14 m cutter stood on the slipway: she and her mast moved to the hall's sides, so the way up the middle stays clear for a
    boat with her mast; no lanterns over the slipway or the water.
  - The mizzen comes down through the Slip's forward wall to its step (a mast stands on the keel): the timber racks either side.
  - The slipway's sides were open under the Slip's floor into the hold: walled (art and collision).
  - The plans stand on an easel (the shop is open to the quay: no wall there); the berth board stood on a bollard; the pay booth
    stood 0.8 m in front of where the dock stair arrives (moved forward 1 m, the salt cakes out of the stair's way); racks and
    shelves stood in front of gates and grilles.
  - Stair holes had rails where a locked room was behind them (walls now); the Well's rail and coaming were closed at the
    hoist's stops; every ceiling was drawn up to four times over voids (once now); the hoist's guide posts ended in the air.
  - Lanterns under 3 m decks hang from cleats as deep as the beams, their bottoms 1.7 m over the floor (over a 1.5 m head);
    hub_lamps.json gives the lights exactly where the art hangs them.
  - Pillars under the galleries' edges, from the quays to the gun deck.
- **The build:** `build_hub_rooms.py` (every floor, one mesh each: the hand-made kit where the player goes, plank faces in the
  locked rooms), `build_hub_inside.py` (the designed spaces), `build_hub_blockout.py` (collision per floor), `build_hub_hull.py`,
  `build_hub_ends.py`, `build_hub_doors.py`, `build_hub_rig.py` (the outside). Run times: blockout 12 s, floors 50 s, inside 10 s,
  outside about 1 min.
- **Tests** (`sb.Dev.RunHubTests [part]`, run in `L_HubReview`: `L_VisualTest`'s saved boat crashes in PIE after the D65 sail rename,
  the boat's own fix to follow):
  - boatdock (rewritten for the side): from 40 m off the starboard side the watch has the doors fully open before she reaches the
    hull; she takes in her sail 6 m short of it and glides through in 14.5 s with no contact (0.95 m between her beam and the
    leaves at their narrowest height); out again with no contact; moored at a fleet berth and kept through a save and a load.
    The fitting berth's mooring fails in `L_HubReview` only: there the boat's own start mooring lies on the same spot.
  - gates: all 37 stop the player and show their line; unlocking all zones opens all 38 blockers.
  - route: the Slip 3.0 s, the Tally House 6.4 s, the Keelgate 27.5 s each way; the Chandlery 55 s (target 40 s, D61 proposal 3).
  - hoist 5/5, ladder 2.3 s, slipway (0.7 cm deep, lies still), headroom 17 586 spots (the least 2.7 m), no low floor (4 903).
- **Still open:**
  - **The water hall's water glows** pale cyan in play: the sky light lights it as if open to the sky (with the sky lighting off
    it goes dark), and the eye is opened for the interior. Next: a reflection capture or the sea cut out in the hall with its
    own dark water.
  - Pale light slits along some ceilings' seams (the Water Hall's, the Slip's).
  - The starter boat's square sail (D65) is about as wide as the doors' narrowest clear way (5.2 m against 5.54 m at z 6): she
    should take it in to pass the doors (the test does).
  - The Chandlery is 55 s away on foot (D61 proposal 3: a shortcut, or the hoist as the way up).
  - The open spaces are 11.7 % of the floor area now (5 602 of 47 720 m2; 3 m decks give more floors), against about 20 % before.
  - The outside's planking stands up to 0.5 m outside the collision between the section's points; the stern's top still reads boxy.
  - `ASaltSeaGate` is no longer placed but stays in the code: the save (`bSeaGateOpen`) still names it (the menus' file).

### D65 · The starter boat calmer in the waves, with a square sail that always pulls (Adopted: waiting for the owner's look)
- **Owner's review (2026-10-05):** "make the starter ship calmer in the waves"; "make the sail of the starter ship a normal sail, so
  both sides with the same amount of sail not rotatable"; it always pulls (strongest with the wind astern, weaker into it, never
  stuck); W / S still set how much. The boat docks in the hub with her full mast up (D64).
- **Calmer:** the hull feels the sea's slope over 1.6 x its size and its height right under it, with more damping (a new heave
  damping) and added mass. In Rough at anchor: roll -30..36 %, pitch -51..57 %, deck acceleration -31..39 %, the rail no longer
  under water; she still rides the swell (heave ratio 1.01-1.04), roll period 3.56 s. A plane-only wave filter did nothing.
- **Square sail:** a fixed yard (5.2 m, 9.0 m over the deck) on the mast's front, the sail (32.6 m2) laced under it, rolled up
  from the foot by brails, sheets to the pin rails (`M_SB_SquareSail`, `setup_boat_sail.py`); boom and gaff gone, shrouds aft of
  the mast. Drive = thrust x amount x (1.0 astern, 0.6 abeam, 0.28 head to wind): 8.4 / 7.2 / 5.4 / 3.2 kn in 7 m/s.
- **Open:** the furled sail renders almost black (the roll keeps the flat cloth's normals; fix in the material's normals);
  `BoatFloats` and `BoatRollPeriod` to re-run in a fresh PIE; the unused boom and gaff assets.

### D66 · The menus: start, pause, settings, voyages, loading, first start, keys and buttons (Adopted: built to the owner's choices; in-game review pending)
- **Owner (2026-10-05), on the menu ideas (`Docs/Design/Menu_Ideas/Menus.html`):**
  - Start menu: the "Night sea" direction. Its title is the launch intro's own title at the place where the intro leaves it
    (the intro was changed for this: the big rope grows into the underline and the title glides up, 8.5 s,
    `Saltbound/Content/Movies/Saltbound_Intro.mp4`). Whether the intro's last frame hands over to the menu is decided later.
  - One rope everywhere: the intro's big rope is the selection underline, the slider track and the on / off mark; a slider's
    handle is a knot tied in it; the "‹ value ›" arrows are small anchors.
  - Pause: Resume, Save now, Voyages, Settings, Return to the title, Quit to desktop, and a "where you are" card (the paper's
    detail is only a reference). Settings: pages with real settings only. Voyages: three pages with a picture, the place, the
    time and the play time; hold X to scrap one, an empty page starts a new voyage. Loading: the save sign's knot. A question
    before something can't be undone: no hold, E answers yes, Esc no. First start: brightness with three knots. Keys and
    buttons: keys on scraps, "Press a key", a clash explained on a scrap (E swap, Esc keep both). No credits for now.
- **Built in code like the other screens** (`UI/SaltMenuKit.*` and one widget per screen), placed at the design page's 1080p
  numbers. The parts come from the design page's own drawing code: `Tools/textures/menu_parts.js` and `gen_menu_parts.py`
  (the rope as a seamless tile, the knot, the anchor, the card, the voyage page, the question's scrap, the wide key scrap, the
  X key for the hold, the knot as plain shapes for the brightness test, and `T_UI_MenuTitle` cut from the intro's title at its
  menu place). `import_textures.py` takes `--only=<patterns>` so new parts do not reimport the whole UI group.
- **The screens are a stack** on the player controller (`PushScreen` / `PopScreen`): the top one has the keys; a question shows
  over the screen under it; the game pauses while the pause menu or anything opened from it is up; the HUD hides under any
  screen. Keys everywhere: W / S (arrows, d-pad) choose, A / D change, E / Enter / Space use, Esc / B / right click go back,
  Q / E (bumpers) turn the settings' pages; the mouse hovers and clicks (the screens hit-test in design coordinates, D58).
- **Voyages are save slots:** Voyage I is the old `Main` slot (older saves carry over), then `Voyage2` and `Voyage3`. The voyage
  last played is kept in the settings (`LastVoyage`). A save remembers its play time (counted while no menu is up) and takes a
  picture of the world without the UI (`Saved/SaveGames/<slot>.png`). Scrapping deletes both; the voyage in play cannot be
  scrapped from the pause menu.
- **A new voyage without reloading the map:** one tick after the map starts, the world as built is saved quietly to the
  `NewVoyage` slot; a new voyage loads that snapshot (the boat at her mooring, the satchel, stores and pickups as built, no
  island found, the hub at her default berth) and saves it at once to its page.
- **The start of the packaged game:** the first start's brightness check once, then the title over the last voyage, loaded
  behind it (Continue goes straight back to play). Play-in-editor and tool runs (`-SaltNewGame`) start in play as before.
- **Saving and leaving:** Save now works anywhere (as since D46; the page's idea of saving only when moored or anchored was
  not taken). Return to the title and sailing another voyage from the pause menu ask first when more than 10 s were played
  since the last save; the voyage then goes back to that save behind the title. Quit to desktop still saves first (D46).
- **The loading screen is a veil:** the world is loaded in place, so it fades in, applies the save once covered, stays at least
  1.4 s (the place in large letters over the voyage's picture, where she lies, a line from an old log, the knot turning as the
  save sign does), hands over and fades out.
- **Settings:** Game (the prompt's place, every setting back), Display (window, frame cap, vsync, resolution scale,
  brightness), Graphics (quality Calm / Fair / Fresh / Gale = the engine's scalability 0-3, shadows, view distance, field of
  view), Audio (one volume for everything), Controls (mouse sensitivity, invert look, keys and buttons). Brightness is the
  display gamma (1.6-3.0, the engine's 2.2 by default); Slate applies it too, so the knot test changes with it. The volume is
  the main audio device's transient primary volume.
- **Keys and buttons:** the keyboard keys of move, jump, sprint, dive, use, satchel and the tool wheel can be changed; the
  other keys that always work (arrows, C, I) are shown fainter; the gamepad is shown, not changed. Esc, F10 and 1 to 5 are
  kept (the pause menu, the tools). A changed key rebuilds the mapping context with the same actions, so bindings stay valid.
- **Checks:** `sb.Dev.RunMenuTests` (20 checks: the pause menu and its card, the settings applying, the brightness, rebinding
  with a swap, the voyages, the title) passed in a standalone run. `sb.Dev.MenuShots` and `sb.Dev.MenuFlow` (title, new
  voyage, play, save, voyages with the picture, back to the title) capture every screen; evidence:
  `Docs/Evidence/D66/D66_menus_sheet.jpg`.
- **Prompts follow the player's keys (owner, same day: "fix" the two gaps):** every prompt names its key by the default ("E",
  "W", "Q") and shows the player's key now (`SaltMenu::ShownKey`): the focus prompt and its hold, the station hints, the chart,
  the satchel's tag, the store's and the tool wheel's hints, "let go of Q". A key with a painted letter (A D E F G Q R S W X)
  keeps it; any other is drawn once at run time in the same face, size and place into a render target
  (`SaltMenu::KeyLetter`, `FWidgetRenderer`), so the ink-soak hold works for it too; words are set smaller to fit the scrap.
  The store's and the wheel's baked hint pictures stay while their keys are the defaults and are drawn live (leather keycaps)
  otherwise. The chart, the store and the satchel also take the player's Use, satchel and movement keys (the defaults keep
  working there). The shared keycap now stretches for words (its picture filled only a one-letter cap).
- **Separate volumes:** Audio has Master (the device's primary volume), Music, Sea and wind, Effects. They are sound classes
  under `SC_Master` (`/Game/Saltbound/Audio`, `Tools/ue_scripts/setup_audio.py`) whose volumes are overridden in one mix
  (`SM_Volumes`) pushed once on the world's audio device. A sound without a class is an effect (`DefaultSoundClassName`); music
  and the sea's sounds must be given `SC_Music` / `SC_SeaAndWind` when they are added (the game has no sounds yet). The audio
  folder is cooked always (`DirectoriesToAlwaysCook`).
- **Checks after the fixes:** `sb.Dev.RunMenuTests` 22 of 22 (with the volume groups and the prompt's key after a swap);
  `sb.Dev.KeyShots` pictures the prompt, the hold, a store, the wheel and the satchel with Use on J, the wheel on Space and the
  satchel on M, and the drawn E beside the painted one (they match).
- **Not done yet:** hold-to-toggle, text size, subtitles and the seasickness comfort settings; the save picture is the full
  screen as a PNG (about 3 MB); the title shows the world at the time of day it was saved, not always at dusk.

### D67 · The game concept: what the player does, how they progress, why they keep playing (Provisional: the owner's four answers adopted, the rest waits for the owner)
- **Owner (2026-10-06):** develop a concept for what the game is about and what the player can do: a good story, but above all an
  exciting experience that does not get boring; combat, story and progress systems may be part of it; inspiration from other games
  is allowed. Mid-task: everything in English from now on (docs, ideas, notes and the conversation).
- **The owner's answers before writing:** combat **light and tool-based** (the sea and its creatures are the danger; no human
  enemies); **a hand-made open sea with a story** that opens step by step through the boat and has an ending; pressure from **the
  boat and the sea only** (no hunger or thirst: hull damage, weather, night, cargo lost in a wreck and salvageable); **about 10-15
  hours**.
- **The concept** (`Docs/Design/GameConcept.md`, a proposal):
  - Story: a castaway in a sea ringed by the Gale follows the vanished Cartographer's log (their old boat is the starter boat) to six
    Tidestones at the Tidewrights' sites; the shard from Ruin Isle is the seventh. The Keelgate opens to the Deep and the
    Hollowkeel's heart; an ending choice (sail out through the Gale or stay). Three acts: the Inner Waters, the Outer Isles, the
    Storm Waters and the Deep.
  - Progress through boats (the double-ender, the cutter of D51, ancient fittings), tools, knowledge (the log, about 30 ancient
    words), the chart (only charted water shows), the Hollowkeel's 18 zones waking with the people the player rescues, and salt.
    No levels or experience points.
  - Danger by one grammar (a tell, an answer, a cost, a reward): squalls, reefs, night, reef sharks, fog, rams, the Grey (the one
    night horror, never fought), guardians, the storm. Answered with the bell, the lantern, the boat hook, flares, a bow harpoon,
    the ancient lens. Failure is a faint or a wreck (towed home, the stores salvageable), never permanent.
  - Hooks: the Hollowkeel in the haze after the first landfall (within 40 minutes), always three open questions in view, the
    tension and warm-harbour rhythm, something new every hour, every voyage ending at a mooring.
- **The README's scope changes:** light combat, an economy, diving to wrecks and ruins and the boat's damage are now planned;
  multiplayer, human enemies, survival meters, a procedural world and freeform shipbuilding stay out.
- **Next:** the owner's answers to the concept's questions (its section 12), then M6, the first playable hour (its section 10.2).

### D68 · The game concept, revisions 2 and 3: the ancients are our own future; digging and materials; the night's horror; sail and motor (Adopted: the owner's answers; the remaining questions keep their defaults)
- **Owner (2026-10-06), on D67:** the story direction (the Gale as a shelter, the Cartographer who broke it) is fine; "a bit of
  horror is good", especially at night on the sea; people as stylised figures behind counters with text on parchment ("YESS");
  the first playable hour (M6) is a good next step; "also think about playable stuff. you can dig stuff up or you need to collect
  stuff for upgrades"; and: **"The ancient civilisation turns out to be the old world, our current world in 50 years. There you will
  be able to upgrade to futuristic technology."**
- **Revision 2 of `Docs/Design/GameConcept.md`:**
  - **The Old World:** the Tidewrights (the sailors' name) were the people of our world in the 2070s. The pale "stone" is their
    composite, the turquoise lines their lights, the glyphs their worn writing, the islands their drowned coast. The Gale is their
    shelter of weather engines (six stations; the Tidestones are their cores); the Hollowkeel's hidden keel is their ark, whose
    autopilot still goes where the survivors gather (the follow rule of D63 as story). The game plays centuries later. The player
    suspects before the story says it (a reveal ladder: made things, writing, a drowned coast, a time capsule dated 2075 at about
    hour 5-6, the Cartographer's guilt at the midpoint, the ark's message at the end). The ruins' existing look (D47, D61) already
    reads as weathered near-future design; no new style is needed.
  - **Futuristic upgrades in three tiers** ("old wood, new light"): sail-age (the Shipwright), salvaged Old World machines rigged
    into wood (a new person, the Tinker, in L18), restored Old World technology from the stations. Rules that keep the sail at the
    centre: limited power (cells, solar film, the glow as the gauge), a weak motor as a helper, machines draw the Grey at night, one
    capability per upgrade.
  - **Collecting, digging, salvaging, building:** about 15 materials (sail-age and Old World, with Saltbound names that change to the
    real ones), by ring; beachcombing refilled by storms, flotsam with the boat hook, about 40 hand-placed dig spots each with a sign
    (never empty-handed; caches, treasure maps, about 10 Old World time capsules), scavenging rooms with the crowbar, salvage by
    diving and the winch. Upgrades need a plan, materials and salt, and are fitted at the fitting berth. Cargo space is the pressure.
    The prologue teaches digging (the Cartographer's buried spare canvas).
  - **The night:** dread, not shock; sound first; light is safety and danger; harbours always safe; growing by act. The Grey: an Old
    World warden machine overgrown to look like an animal, wild since the southern core was taken; never fought, calmed at the end.
- **The owner's answers on revision 2 (round 3, 2026-10-06):** the game plays **about 300 years** after the Old World; the Grey is
  **an overgrown Old World warden machine**; the player's boats stay **wooden boats with salvaged machines rigged in** (the ark is the
  only true Old World ship); the motor is **a real alternative to the sail**, not a weak helper (my recommendation was the helper).
- **Revision 3 of the concept (the motor):** it comes in Act II (about hour 6), after hours of pure sailing. It beats the sail into
  the wind, in calms, in tight places and in the storm; the sail is silent, never runs out, is faster before a fresh breeze and
  **charges the cells** while sailing (the propeller as a generator), so what the sail fills the motor spends. The motor's costs:
  power (the first one about 5 kn for about 3 km a charge), noise and current (the Grey, the wildlife), wear. Upgrades bring it to
  about 8 kn with a bigger bank: late in the game the player chooses how to travel, never without thinking of power and the dark.
- **Next:** M6, the first playable hour; the concept's remaining questions (section 12) keep their recommended defaults until the
  owner says otherwise.
- **Round 4 (2026-10-06):** "1. ship is the starter ship 2. ship is a bigger ship 3. Will be an ancient ship with full automated
  (unknown power source)"; "on the start island is an helper NPC, he will give out a shovel"; "Make an ship repair system"; "Start
  with the steps". Revision 4 of the concept: the ancient ship (Act III, sails a set course herself, power unknown until the Deep),
  the Harbour Keeper gives the shovel (at the keeper's place the island agent builds, D71), the ship repair system (holes, bilge
  water, patching, bailing, mending the sail, the Shipwright's full repair; the concept's 5.5). M6 is being built (D72 on).

### D69 · The hub after the owner's second review: no haze or sun inside, warm lanterns, a round stern with grand galleries, a glazed sterncastle with grand stairs and the command hall, the Chandlery in the forecastle, rope that is rope, a reinforced hull, surface relief (Adopted: waiting for the owner's look)
- **Owner (2026-10-06), with screenshots:** "there is fog in the hub" (and why games load into levels); "the light from the lanterns
  are not good. Create strong and warm lightsources"; the sterncastle's front "is boring and repetitive ... This is where the
  command center is. It needs to look grand, big rooms, glass, a bit more modern look (dont overdo it). Two grand stairs on each
  side going up, and a Roof above the Back part. This space is completly unused"; "the end needs to be more round. it looks like a
  square"; "the Balconies are a good idea, make them more grand more space, put them around all 3 sides of the back of the
  ship"; "make the sail normal"; "these nets dont look like ropes"; "Implement the Chandler better into the ship, it just stands
  around ... at the front of the ship"; "The whole ship has loose ends and no physik in the ropes"; "a bit more details in all
  textures. Not only through color. Through Surface Finishing too"; the hull side "is boring ... I dont say there cant be windows
  but only? Create reinforcements on the hull".
- **Answers before building:** "make the sail normal" = square yards and a real furl (not set sails); the command centre = a glass
  hall on the poop with grand stairs; the closed quarter galleries are replaced by the galleries round three sides.
- **Levels (the owner's question):** the hub is already its own level (`L_Hub`), streamed in the background at her berth (no
  loading screen; sailing in through the side doors without a cut is worth keeping). The fog was not a level problem: the height
  fog is one for the world and walls do not stop it.
- **Interiors:** `ASaltInteriorZone` boxes (the hull below the main deck, the sterncastle under the quarterdeck and under the
  poop, the forecastle): while the camera is inside, the lighting rig starts the fog 165 m away and halves it (blended over 2.5 m
  at the faces). The water hall's water is sheltered (`FSaltShelterBox` on the berth and the ocean, MPC `ShelterA/B`): no
  in-scattering and no crest light inside the box (the sun lit the water's volume without shadow and it glowed cyan, at the
  interior's open exposure even 6 % did). The hidden collision's shell and decks cast shadows (`bCastHiddenShadow`): watertight,
  they stop the sun and the seams of light the boarded art let in. The interior volumes turn Lumen's screen traces back on (flat
  water there; ray tracing alone missed thin boards and showed the sky), and the side doors' leaves are not Nanite any more (ray
  tracing never saw their boards).
- **Lanterns:** 240 cd at 1 900 K (was 60 cd, 2 100 K), 11 m, a 4 cm flame for soft shadows, a slight flicker (`ASaltHubRopes`);
  new lanterns on the galleries, the grand stairs' newels, the Chandlery's door and inside.
- **The rig:** the mizzen's lateen yard is gone, every yard is square; yards, slings, parrels and lifts hang on the mast piece at
  their height (the upper yards' gear had hung behind the topmast in the air); furled sails are gathered canvas (lumpy, fat at the
  bunt, tapering to the yardarms) pinched by gaskets that go round sail and yard; ratlines are hemp lines sagging between the
  shrouds, seized to each with a knot, every fifth out to the aftmost shroud; ropes sag with their length; blocks at every lift
  and brace. `check_ends()` measures every rope end against the spars, blocks, deadeyes, pins and decks: 0 loose of 4 140.
  Thirty-two braces and clewlines and the entry ports' four man-ropes are real cables (`ASaltHubRopes`, `hub_ropes.json`) that
  hang, sway and gust in the wind blowing aft.
- **The stern:** a half ellipse 9.5 m deep in plan (`form.stern.shape`), meeting the sides without a corner. The galleries are 3.4 m
  deep at sc1 and sc2, round the stern and along both sides to x 21.5, on carved knees, balustraded, with columns, the upper one
  roofed, lanterns on its columns; the cabins open onto them through glazed doors. No quarter galleries.
- **The sterncastle:** the quarterdeck's and the poop's fronts are glazed: tall windows in slim iron grids inside cream
  architraves between oxblood pilasters, glazed double doors where the gates stand, a great window over both storeys in the
  quarterdeck's front. Grand stairs along both bulwarks (2.6 m, 33 risers of 0.18 m) climb from the main deck to the quarterdeck and
  from it to the poop: oak treads over oxblood risers, a panelled side under a moulded stringer, a turned balustrade, a handrail
  on the bulwark, newel posts with lanterns. The quarterdeck and the poop are open decks now (`weather`); the great cabin stays
  locked behind its doors.
- **The command hall** (`hub.command`, `SM_Hub_Command`) on the poop: oak posts on a panelled plinth, glass between them in iron
  frames (some panes lit), glazed doors under a pediment (a gate: "Barred from inside. The council meets here."), a hipped roof of
  green copper with standing seams and a glazed lantern on its ridge (`MI_Hub_Copper`).
- **The Chandlery** moves into the forecastle's front at the main deck, two storeys high (the forecastle's storey is open over
  it): two shop windows of small panes, lit, either side of the door, the CHANDLER board, a striped awning on iron arms, lanterns,
  goods outside (barrels, crates, coils, oars, an anchor); inside the counter facing the door, shelves to the beams with a ladder
  on a brass rail, beams hung with stock, the compass and the spyglass to try. The cargo deck is part of the open waist now (its
  boats and padlocked hatches stay); L14 is the forecastle's rooms (a barred door beside the shop and one in its back wall), L15
  the forecastle deck and the head (the chained ladders).
- **The hull's sides:** each deck's row its own: ochre-framed gunports on the gun deck, tarred lids twice as far apart on the
  upper and middle decks, round brass portholes on the lower and quay decks; six pairs of iron-banded skids a side over the
  wales; carved rosettes along the main deck's band; an entry port amidships each side (posts, a carved hood, batten steps down to
  the water, man-ropes); iron-lipped scuppers along the deck line.
- **Surface finishing** (`gen_textures.py`, colours unchanged): wood has crisp V-cut grain grooves and plane marks across the
  boards (the grooves rougher and darker, the marks' ridges worn smooth); weathered timber adze marks; paint brush ridges and,
  in patches, fine cracks; iron round hammer dents with brighter rims; the hub's tar and ochre show the relief more.
- **Tools:** `Tools/hub_shot.py` (editor captures) and `Tools/hub_pie_shot.py` (in play: the doors shut, the renderer's frames)
  take poses in the ship's frame; `sb.Dev.HubGoto quarterdeck|poop`.

### D72 · M6 step 1: the day's clock, the moon and a sky with stars (Adopted: waiting for the owner's look)
- **Why:** the concept (D67, D68) needs a running day: night is where the measured horror lives, the lanterns' warm pools are the
  harbour's welcome, and voyages are planned around daylight. The rig had three fixed presets (Day, Evening, Dusk) and no night.
- **The clock** (`ASaltLightingRig`): a day of keys (hour, look, sun elevation, sun azimuth unwrapped so the sun always travels the
  same way): Night 0-3.8, Twilight 4.05, Dawn 4.3-4.8, Morning 5.8, Day 8.5-17, Evening 19, Dusk 19.75-20.3, Twilight 20.65, Night 21.
  The look eases between keys, the sun moves evenly. Day, Evening and Dusk stand exactly where their presets stood (Day 32 degrees
  at azimuth 120, Evening 4 and Dusk -3 at 340), so `sb.Lighting Day|Evening|Dusk` (which now jumps the clock there and holds it)
  gives the reviewed looks for captures and benchmarks. 40 real minutes a day; the night runs 1.5 times faster (about 8 minutes of
  full night). New looks: Morning, Dawn, Twilight, Night. The day of the voyage counts up at midnight.
- **The moon:** a second atmosphere light (index 1, a transient component of the rig, 2.5 lux, cool blue, a 1.2 degree disc). It rises
  in the east-south-east at 19, crosses the south (50 degrees high) and sets at 5.5, and takes over as the sun sinks from -2 to -9
  degrees: it lights the sky ("day for night" blue), the clouds and the sea's glitter. Forward shading (the water, translucency) takes
  whichever light rules the sky (the engine warned on screen while both had the same priority). Shadows: the sun's off below -4, the
  moon's on above 40 % of its light.
- **Twilight:** below -4 degrees the sun's light falls off by 0.6 stops a degree (the engine's atmosphere keeps the high sky glowing
  long after sunset: at -11 degrees it burnt the night's open eye white), Dusk and Dawn hold down to -8 degrees, and a Twilight look
  with a wide exposure range (EV -1 to 7) lets the eye follow the last glow; the sun is hidden below -18. Measured sky brightness from
  19.75 to 21.0: 150, 142, 112, 116, 100, 78, 42 (no jump, no white sky).
- **The sun turns in steps of 0.2 degrees** (about once a second): every turn re-renders the cached virtual shadow pages.
- **Lanterns** light below 6 degrees of sun and go out above it, only when the light changes (or the clock is set): a lantern the
  player lit or doused keeps its state until then. Lanterns that start after the rig get the state a second into play.
- **The sky dome** (`Tools/ue_scripts/setup_night_sky.py`: `M_SB_Sky`, `MPC_Sky`, `SM_SkyDome`, a copy of the engine's sky sphere
  scaled to 400 km): a sky material in the scene stops the atmosphere from drawing the sky's pixels (the engine's
  `bSceneHasSkyMaterial`), so the dome draws them: the atmosphere's view luminance, the discs of the sun and the moon, and the stars
  (two layers of crisp dots in a grid on the sphere, dense and faint, sparse and bright, a slow twinkle, faded towards the horizon),
  which come out with the moon (`StarAmount`). A/B by day and at evening with `sb.Sky.Dome 0`: the frames' means agree within 0.6
  of 255 (the rest is the moving clouds and sea).
- **Save:** the hour and the day (`TimeOfDay`, `DayNumber` on the save; older saves keep their preset's hour, running). The pause
  card says "day 3 · afternoon, the sun going west".
- **Commands:** `sb.Time` (print), `sb.Time <hour> [hold]`, `sb.Time run|hold`, `sb.Time day <n>`, `sb.Time.Scale <x>`, `sb.Sky.Dome 1|0`,
  `sb.Lighting Day|Evening|Dusk|Night|Dawn|Morning|Twilight`.
- **Tests:** `sb.Dev.RunClockTests` 6/6 (the preset hold, the night and moon, the clock's rate, the faster night, the new day, a doused
  lantern kept); `sb.Dev.RunSaveTests` 6/6 (new: `LoadTime`, 22.40 h on day 4, running); `sb.Dev.RunMenuTests` 22/22.
- **Open:** the night's look is a first pass for the owner (how dark, how blue, the stars' size and number); the underwater view at
  night has not been looked at; no moon phases; nothing sleeps yet (the bunk and the Inn come with the cutter and the hub's zones).
