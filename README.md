<div align="center">

# Saltbound

![Unreal Engine 5.8](https://img.shields.io/badge/Unreal%20Engine-5.8-0E1128?logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white)
![Blender](https://img.shields.io/badge/Blender-5.2-F5792A?logo=blender&logoColor=white)
![Status](https://img.shields.io/badge/status-in%20development-E3B341)

<img src="Docs/Images/readme_hero.jpg" width="860" alt="The small boat leaving the big hub ship through its side door">

<sub>The starting boat leaves the hub ship through its side door.</sub>

</div>

## What is this?

Saltbound is a first-person sailing and exploration game I'm making in Unreal Engine 5. You start with a small sailing boat in a calm harbor and explore a sea full of islands, ruins and traces of a lost civilization. The boat is your transport, your home and your workshop, and it grows with you.

It is a hobby project for Windows, single-player, and it uses no paid assets.

More pictures: [the harbor](Docs/Evidence/D70/D70_harbour_sheet.jpg), [the sea at night](Docs/Evidence/D88/D88_sea_light.jpg), [the island meadow](Docs/Evidence/D86/D86_meadow.jpg)

## What already works

- A first-person character that can walk, swim and stand on a moving boat
- A starting boat you can actually sail, with buoyancy based on its real weight
- My own ocean made from waves calculated on the graphics card, with an underwater view
- A home island with grass, palms and rocks, and a big hub ship with a harbor inside
- Day and night, weather and a storm at the edge of the map
- Tools you hold in your hands, like a spade for digging
- Menus, settings and saving

## How I work on it

- **Everything is made by scripts.** The 3D models are built by Python scripts in Blender, and the Unreal content is set up by scripts in the editor. That way I can rebuild anything and see every change.
- **Automated tests.** Tests for walking, sailing, the clock, the menus and saving run inside the game from the command line.
- **Every decision is written down.** [Docs/Decisions.md](Docs/Decisions.md) has more than 80 numbered decisions and the reasons behind them, and [Docs/Progress.md](Docs/Progress.md) shows what is done and what comes next.

## What is planned

The [game concept](Docs/Design/GameConcept.md) describes the full game: a hand-made sea that opens up through your boat, a story with an ending (about 10 to 15 hours), a bit of danger at night, boat damage, diving to wrecks and salt as the currency. Multiplayer and survival meters are not planned.

## Tools I use

| Tool | Version |
|---|---|
| Unreal Engine | 5.8 |
| Visual Studio | 2022 |
| Blender | 5.2 |
| Git and Git LFS | for the big files like models and textures |
| Python | for the build and test scripts |

## Build and run

All scripts are in the Tools folder.

```powershell
Tools\Build-Editor.ps1
Tools\Launch-Editor.ps1
Tools\Package-Game.ps1
```

The first command compiles the project, the second opens the editor and the third makes a Windows build. More details are in [Docs/Conventions.md](Docs/Conventions.md) and [Docs/Progress.md](Docs/Progress.md).

## License

All rights reserved, see [LICENSE](LICENSE). You can read the code, but you can't reuse it. Content from other people keeps its own license, listed in [Docs/ThirdParty.md](Docs/ThirdParty.md).
