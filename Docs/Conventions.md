# Conventions

## Units, axes, pivots

| Topic | Rule |
|---|---|
| Units | Blender: metric, unit scale 1.0, 1 BU = 1 m. Unreal: 1 uu = 1 cm. Verified: a 0.60 m crate imports as 60 cm. |
| Forward | +X in both tools. |
| Right / starboard | Blender **−Y** → Unreal **+Y** (verified with marker plates on `SM_Crate_Small`). |
| Up | +Z in both tools. |
| Pivot | Props: bottom centre. Modular kit pieces: bottom corner on the kit grid. Boat: midship on the design waterline. |
| Object transforms | Apply rotation/scale in Blender before export (scripts build geometry in place). |
| Reference sizes | Player capsule 176 cm tall (radius 34 cm), eye height 165 cm. Doors ≥ 190 × 80 cm, walkways ≥ 70 cm. Rails 90–100 cm. Steps ≤ 20 cm. |

## Naming

| Kind | Pattern | Example |
|---|---|---|
| C++ classes | `Salt` prefix after the UE type prefix | `ASaltBoat`, `USaltWaveSubsystem` |
| Static mesh | `SM_<Name>` | `SM_Crate_Small` |
| Collision in FBX | `UCX_<MeshName>_##` (convex), `UBX_` (box) | `UCX_SM_Crate_Small_01` |
| Material / instance / function | `M_`, `MI_`, `MF_` | `M_SB_Surface`, `MI_Wood_Planks` |
| Material parameter collection | `MPC_` | `MPC_Ocean` |
| Texture | `T_<Name>_<Suffix>`; suffix `D` albedo, `N` normal, `ORM` packed, `M` mask | `T_WoodPlanks_N` |
| Blueprint | `BP_` (actor), `WBP_` (widget), `DA_` (data asset) | `BP_Boat_Starter` |
| Input | `IA_` actions, `IMC_` contexts | `IA_Interact` |
| Audio | `MS_` MetaSound source, `SW_` sound wave, `SA_` attenuation | `MS_Wind` |
| Maps | `L_<Name>` | `L_DevTest` |
| Blender source | `Art/Blender/Source/<AssetName>.blend` | `SM_Crate_Small.blend` |
| Blender build script | `Art/Blender/Scripts/build_<asset_or_kit>.py` | `build_test_crate.py` |

## Content folders

```
/Game/Saltbound/Core          game mode, global data, MPCs
/Game/Saltbound/Input         input actions and mapping contexts
/Game/Saltbound/Player        player blueprint and assets
/Game/Saltbound/Boat          boat meshes, blueprints, upgrades
/Game/Saltbound/Water         ocean meshes and materials
/Game/Saltbound/Environment   Harbor/, Rocks/, Sky/
/Game/Saltbound/Props         small reusable props
/Game/Saltbound/Materials     Master/, Functions/, Instances/, Textures/
/Game/Saltbound/Audio
/Game/Saltbound/UI
/Game/Saltbound/Maps
/Game/Saltbound/Dev           test-only assets (never referenced by game maps)
/Game/ThirdParty/<Source>     external assets, each with a license note in Docs/ThirdParty.md
```
Template content (`/Game/FirstPerson`, `/Game/LevelPrototyping`, `/Game/Characters`, `/Game/Input`) is
temporary and will be removed once replaced.

## Materials

- Blender material names are the Unreal slot names and must equal the target `MI_` asset name. The importer
  never creates materials; it assigns existing instances by name (fallback `MI_Placeholder`).
- Few master materials, many instances. Prefer tiling textures + mesh UVs at a consistent density
  (`world_box_uv(metres_per_uv=1.0)` for generated meshes) over unique high-resolution textures.
- Texture budget guideline: 1024² for tiling surface sets, 2048² only for hero surfaces seen up close
  (boat deck, helm). Packed ORM (R occlusion, G roughness, B metallic).
- Substrate is enabled (engine default); materials are authored with classic inputs, which Substrate converts.

## Collision

- Walkable boat and dock surfaces: simple boxes/convex hulls (`UCX_`/`UBX_`), smooth and slightly
  generous; avoid trimesh for anything the player walks on while it moves.
- Static environment (rocks, cliffs): `collision: "complex"` in the manifest (complex as simple) is acceptable.
- Small props: one convex hull or box.

## Blender → Unreal workflow

1. Write/adjust `Art/Blender/Scripts/build_<asset>.py` using `sb_blender` helpers.
2. `Tools\Build-BlenderAsset.ps1 build_<asset>` → saves `.blend` to `Art/Blender/Source`, FBX to `Art/Export`.
3. Add or update the asset entry in `Art/asset_manifest.json` (fbx path, destination, nanite, collision).
4. With the editor running: `python Tools\ue_exec.py Tools\ue_scripts\import_assets.py <AssetName>`.
   The script prints size, collision primitive count and slot assignments for verification.
FBX export settings live in `sb_blender.export_fbx()` (scale 1.0, apply unit scale, face smoothing,
tangent space, no leaf bones). Import uses Interchange with a scripted pipeline (`import_assets.py`).

## Code

- Gameplay systems in C++ (`Saltbound/Source/Saltbound`), tunables as `UPROPERTY(EditAnywhere)` grouped by
  category; Blueprints only as thin asset-binding subclasses where convenient.
- Development utilities live behind `sb.Dev.*` console commands and are compiled out of Shipping builds.
- Comments and identifiers in English; no engine source modifications.
