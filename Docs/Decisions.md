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
