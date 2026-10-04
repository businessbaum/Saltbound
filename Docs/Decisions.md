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

### D34 · Rainbow specks on distant crests: half-float overflow of the sun specular (Adopted, awaiting owner check)
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
