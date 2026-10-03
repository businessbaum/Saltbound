# Progress

Last updated: 2026-10-03 (session 2, visual revision 3: stylised realism).

## Milestone status

| Milestone | Status |
|---|---|
| M1 Infrastructure & foundation | **Done** (verified) |
| M2 First-person player & moving-platform tests | **Done** (verified in PIE and standalone) |
| M3 Visual test scene | **Revision 3 in review** (stylised realism pass after owner feedback; see below) |
| M4 Starting boat & navigable water | Next (technical parts can start without the decisions) |
| M5 Starting harbor & complete loop | Planned |

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
Map: `/Game/Saltbound/Maps/L_VisualTest` (game and editor default map). Regenerate with
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

## Controls
| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move | W A S D / arrow keys | Left stick |
| Look | Mouse | Right stick |
| Sprint | Left Shift (hold) | Left stick click |
| Jump | Space | A / Cross |
| Interact / leave station | E | X / Square |
| At a station | W/S and A/D are routed to the station | Left stick |

Developer console (`~`, non-shipping builds):
- `sb.Lighting Day|Evening|Dusk`
- `sb.Dev.RunMovementTests [motion speed turn]`, `sb.Dev.Platform <motion> [speed turn]`
- `sb.Dev.Benchmark [s]`, `sb.Dev.Perf [s]`, `sb.Dev.Delayed <s> <cmd>`
- `sb.Dev.ResetPlayer`, `sb.MouseSensitivity <x>`, `sb.Dev.ShowInteractionVolumes 1`

## Known issues and limitations
- **Performance:** revision 2 measured in standalone, 2560×1440 native, editor closed:
  - Day 75.7 fps (GPU 12.5 ms), Evening 69.6 fps (GPU 13.6 ms), Dusk 72.1 fps (GPU 13.1 ms).
  - This is within D8 (60 fps), but the headroom shrank from about 2× to 1.2×.
  - Revision 3 (big swell, 215k-triangle Nanite boat section, denser clouds) has not been measured yet.
- The Jacobian foam trail samples the dominant wave direction only.
- **Vsync frame pacing:** with vsync on, the standalone build runs at about 55–60 fps even though the GPU
  needs only about 7.5 ms. Default display settings (vsync off or a frame cap, later a settings menu) are needed.
- **Benchmarking:** always close the editor first. A running editor renders in the background (throttling
  is disabled for automated tests) and halves standalone performance.
- **Water inside hulls:** the ocean is not masked inside hulls yet. This matters if the cabin floor goes
  below the waterline (decision 2).
- **Test deck section:** it is a slice of the boat with flat end caps (not a full hull), and its deckhouse
  is a shed-like 95 cm box.
- **Floater:** `ASaltDevFloater` is a simple spring-fitted floater, not the M4 boat model.
- About 0.25 cm vertical camera pops on heaving decks (CharacterMovement floor correction), below the visible threshold.
- Mouse sensitivity has not been tested by a human (`sb.MouseSensitivity`). The prompt key label is fixed to "E".
- No audio yet. No player body or hands.
- `BoardingRamp` in `L_DevTest` is not aligned. Template `LevelPrototyping` content is still in use for greyboxing.

## Next steps
1. Owner review of M3 revision 2 (`Docs/Evidence/M3_rev2/`). Remaining checkpoint decisions: cabin
   concept, hull colour, sea state.
2. Benchmark revision 2 in a standalone build with the editor closed (D18, D19).
3. **M4 technical, independent of the decisions:**
   - Boat motion model: buoyancy from multi-point samples of the shared wave field, heave/pitch/roll response.
   - Simplified sailing: wind direction and strength, no-go zone, sail trim/raise, keel lateral resistance,
     rudder, heel.
   - Helm station driving the boat; collision sweeps against the pier and rocks; reset/recovery utility.
   - Water masking inside the hull if needed.
4. M4 art after the decisions: full starter boat in Blender (about 10 m double-ender, 3.4 m beam), interior, sail.
5. Water polish: white sun glitter towards a low sun, bow wave and wake, spray at crests.
6. Display settings menu: vsync/frame cap, screen percentage.
