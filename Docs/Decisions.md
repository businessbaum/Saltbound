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
