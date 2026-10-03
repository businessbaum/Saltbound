# Saltbound

Atmospheric first-person sailing and exploration game. The player starts with a small sailing boat in a
sheltered harbor and explores a mysterious sea of islands, ruins and traces of a lost civilization. The
boat is transport, home, storage and workspace, and it grows with the player.

Hobby project · Unreal Engine 5 + Blender · Windows PC · single-player · no paid assets or services.

## Current scope

Initial scope (Milestones 1–5): project foundation, first-person player, visual test scene, starting boat
with navigable water, starting harbor with a complete short gameplay loop (depart → sail → return → moor →
save/load). Larger systems (multiplayer, combat, survival, procedural world, economy, underwater
exploration, freeform shipbuilding) are out of scope for now. See [Docs/Ideas.md](Docs/Ideas.md).

Status and next steps: [Docs/Progress.md](Docs/Progress.md).

## Repository layout

| Path | Contents |
|---|---|
| `Saltbound/` | Unreal project (`Saltbound.uproject`, `Source/`, `Content/`, `Config/`) |
| `Art/Blender/Scripts/` | Python scripts that build Blender assets reproducibly (`sb_blender.py` = shared helpers) |
| `Art/Blender/Source/` | Generated/authored `.blend` source files (Git LFS) |
| `Art/Export/` | FBX exports (generated, not versioned) |
| `Art/asset_manifest.json` | Blender export → Unreal import settings per asset |
| `Tools/` | Build, launch, packaging and editor automation scripts |
| `Tools/ue_scripts/` | Python scripts executed inside the Unreal Editor |
| `Docs/` | Conventions, decisions, progress, ideas, third-party licenses |

## Tool versions

| Tool | Version | Notes |
|---|---|---|
| Unreal Engine | 5.8.3 (CL 58210709), launcher build | `C:\Program Files\Epic Games\UE_5.8` |
| Visual Studio | 2022 Enterprise, MSVC 14.44.35207 | Windows SDK 10.0.26100 |
| Blender | 5.2.2 LTS | `C:\Program Files\Blender Foundation\Blender 5.2` |
| Git / Git LFS | 2.45.1 / 3.5.1 | binary assets tracked by LFS (`.gitattributes`) |
| Python (host) | 3.14 | only for Tools/*.py clients; Unreal embeds Python 3.11 |

Override engine/Blender locations with the `SALTBOUND_ENGINE` / `SALTBOUND_BLENDER` environment variables.

## Setup, launch and build

All scripts are PowerShell (`powershell -ExecutionPolicy Bypass -File Tools\<script>.ps1`).

| Task | Command |
|---|---|
| Compile editor (close the editor first) | `Tools\Build-Editor.ps1` |
| Open the editor | `Tools\Launch-Editor.ps1` |
| Regenerate Visual Studio solution | `Tools\GenerateProjectFiles.ps1` |
| Standalone Windows build → `Builds\Development\Windows\Saltbound.exe` | `Tools\Package-Game.ps1` |
| Build a Blender asset headless | `Tools\Build-BlenderAsset.ps1 <script_name>` |
| Import Blender exports into Unreal (editor running) | `python Tools\ue_exec.py Tools\ue_scripts\import_assets.py [Asset ...]` |
| Save, quit, rebuild and relaunch the editor (after C++ changes) | `Tools\Rebuild-Editor.ps1` |
| Run the packaged build (optional screenshot / console commands) | `Tools\Run-Build.ps1 -Seconds 60 -ExecCmds "sb.Dev.Benchmark 10" -Fullscreen` |
| Automated movement tests in PIE | `python Tools\run_pie_test.py "sb.Dev.RunMovementTests 1 300 8"` |
| Capture the visual test shots (Day/Evening/Dusk) | `python Tools\capture_shots.py visualtest --presets Day Evening Dusk` |
| Regenerate procedural textures | `python Tools\textures\gen_textures.py` (then `import_textures.py`) |

Editor automation: the project enables Python remote execution (`Tools/ue_exec.py` runs Python inside the
running editor) and the engine MCP server at `http://127.0.0.1:8000/mcp` (`Tools/ue_mcp.py`, used for
viewport captures and logs). Both start automatically with the editor.

## Controls

Controls are documented per milestone in [Docs/Progress.md](Docs/Progress.md#controls) until a settings menu exists.

## Documentation

- [Docs/Conventions.md](Docs/Conventions.md) – scale, axes, naming, folders, materials, collision, Blender→Unreal export
- [Docs/Decisions.md](Docs/Decisions.md) – significant technical and visual decisions with rationale
- [Docs/Progress.md](Docs/Progress.md) – milestone status, verification results, known issues, next steps
- [Docs/Ideas.md](Docs/Ideas.md) – future ideas with benefits and prerequisites
- [Docs/ArtDirection.md](Docs/ArtDirection.md) – visual principles, palette, lighting presets, boat proportions
- [Docs/ThirdParty.md](Docs/ThirdParty.md) – external content, sources and licenses
