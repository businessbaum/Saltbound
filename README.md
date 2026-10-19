<div align="center">

# Saltbound

![Unreal Engine 5.8](https://img.shields.io/badge/Unreal%20Engine-5.8%20C%2B%2B-0E1128?logo=unrealengine&logoColor=white)
![Blender](https://img.shields.io/badge/Blender-5.2%2C%20built%20by%20script-F5792A?logo=blender&logoColor=white)
![HLSL](https://img.shields.io/badge/shaders-FFT%20ocean%20%C2%B7%20HLSL-5C2D91)
![Git LFS](https://img.shields.io/badge/assets-Git%20LFS-F64935?logo=git&logoColor=white)
![Status](https://img.shields.io/badge/status-in%20development-E3B341)

<img src="Docs/Images/readme_hero.jpg" width="860" alt="The starting boat leaving the hub ship's water hall through its side door">

<sub>The starting boat leaving the hub ship through its side door. More: <a href="Docs/Evidence/D70/D70_harbour_sheet.jpg">harbour sheet</a> · <a href="Docs/Evidence/D88/D88_sea_light.jpg">sea at night</a> · <a href="Docs/Evidence/D86/D86_meadow.jpg">the island meadow</a></sub>

</div>

---

| | |
|---|---|
| **Engine work** | own FFT ocean with Single Layer Water and an underwater pass · buoyant boat physics tuned to the real 10.9 t mass · first-person rig with tools held by solved hands · procedural seagulls on a poseable mesh · volumetric storm wall |
| **Pipeline** | every asset is built by a versioned Python script (Blender headless or Unreal editor Python), so content can be regenerated and reviewed as code |
| **Testing** | automated PIE suites for movement, sailing, clock, menus and save/load, run from the terminal; packaged-build benchmarks |
| **Process** | 127 commits, 88 numbered design decisions in [Docs/Decisions.md](Docs/Decisions.md), progress and known issues in [Docs/Progress.md](Docs/Progress.md) |

## About

Atmospheric first-person sailing and exploration game. The player starts with a small sailing boat in a
sheltered harbor and explores a mysterious sea of islands, ruins and traces of a lost civilization. The
boat is transport, home, storage and workspace, and it grows with the player.

Hobby project · Unreal Engine 5 + Blender · Windows PC · single-player · no paid assets or services.

## Current scope

Initial scope (Milestones 1–5): project foundation, first-person player, visual test scene, starting boat
with navigable water, starting harbor with a complete short gameplay loop (depart → sail → return → moor →
save/load).

The game beyond M5 is planned in the game concept ([Docs/Design/GameConcept.md](Docs/Design/GameConcept.md), D67,
a proposal waiting for the owner's answers): a hand-made sea that opens through the boat, a story with an ending
(about 10-15 hours), light tool-based danger from the sea and its creatures and a measured night horror, boat
damage, digging and salvaging materials for upgrades, the Old World's technology (the ruins are our own future),
salt as the economy and diving to wrecks and ruins. Multiplayer, human enemies, survival meters, a procedural world and freeform
shipbuilding stay out of scope. Future ideas: [Docs/Ideas.md](Docs/Ideas.md).

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
| Automated sailing tests in PIE (`L_VisualTest`) | `python Tools\run_pie_test.py "sb.Dev.RunBoatTests" --timeout 600 --done "SB_TEST BoatSuite DONE"` |
| The day's clock checks in PIE (D72) | `python Tools\run_pie_test.py "sb.Dev.RunClockTests" --done "SB_TEST ClockSuite DONE"` |
| Settings menu and save/load checks in PIE | `python Tools\run_pie_test.py "sb.Dev.RunMenuTests" --done "SB_TEST MenuSuite DONE"` (and `sb.Dev.RunSaveTests` / `SaveSuite`) |
| Regenerate procedural textures | `python Tools\textures\gen_textures.py` (then `import_textures.py`) |

Editor automation: the project enables Python remote execution (`Tools/ue_exec.py` runs Python inside the
running editor) and the engine MCP server at `http://127.0.0.1:8000/mcp` (`Tools/ue_mcp.py`, used for
viewport captures and logs). Both start automatically with the editor.

## Controls

Controls are listed in [Docs/Progress.md](Docs/Progress.md#controls); settings (display, look) are in the pause menu (Esc, F10 in the editor).

## Documentation

- [Docs/Conventions.md](Docs/Conventions.md) – scale, axes, naming, folders, materials, collision, Blender→Unreal export
- [Docs/Decisions.md](Docs/Decisions.md) – significant technical and visual decisions with rationale
- [Docs/Progress.md](Docs/Progress.md) – milestone status, verification results, known issues, next steps
- [Docs/Stats.md](Docs/Stats.md) – every object's numbers (boats, the player, items, tools) and where to change them
- [Docs/Design/GameConcept.md](Docs/Design/GameConcept.md) – the game concept: story, what the player does, danger, progression, hooks (D67)
- [Docs/Ideas.md](Docs/Ideas.md) – future ideas with benefits and prerequisites
- [Docs/ArtDirection.md](Docs/ArtDirection.md) – visual principles, palette, lighting presets, boat proportions
- [Docs/ThirdParty.md](Docs/ThirdParty.md) – external content, sources and licenses

---

<sub>**How this was built:** the game concept, art direction, every look and feel decision and the reviews of each step by me. The C++, shaders, Blender and editor scripts, tests and documentation were written by AI coding agents (Claude Code) working in the editor under the rules in [CLAUDE.md](CLAUDE.md).</sub>
