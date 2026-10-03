# Art direction (working draft, pending owner feedback at the M3 checkpoint)

Reference for cohesion and ocean presentation: Sea of Thieves. Saltbound aims for a calmer, original look with
less exaggerated shapes. The owner confirmed the Sea of Thieves water as the target for the sea.

## Principles
- **Shapes:** chunky and readable, with believable proportions. Planks 3–4 cm thick, rails about 8 cm.
  Soft bevels of 3–8 mm on every hard edge (`sb.soften`). Slight irregularity: plank length/height jitter,
  ±0.5–1° rotations, sub-mm vertex jitter.
- **Surface detail:** restrained. Low-contrast tiling textures, generated procedurally, carry the material
  identity. Variation comes from vertex masks rather than unique high-resolution textures:
  - R: edge wear on bevel faces
  - G: grime and wetness near the waterline
  - B: per-part tone variation
- **Warm home, cool world:** the boat is warm, cared for and lit by lanterns. The harbour and sea are
  weathered and cooler.
- **Ancient technology:** a restrained turquoise glow, `MI_AncientGlow`, used sparingly.

## Palette (linear values used in the material instances)
| Role | Material | Tint (linear) | Notes |
|---|---|---|---|
| Deck | MI_Wood_Teak | 0.26, 0.135, 0.065 | warm honey teak, dark caulking seams (MI_Caulk_Dark) |
| Trim | MI_Wood_Varnish | 0.12, 0.05, 0.022 | deep amber, glossy |
| Deckhouse paint | MI_Paint_Cream | 0.74, 0.64, 0.45 | wood shows at worn edges |
| Hull | MI_Paint_Hull | 0.22, 0.04, 0.025 | oxblood red with a cream sheer strake (pending decision 3) |
| Boot-top | MI_Paint_Boottop | 0.03, 0.03, 0.032 | waterline band |
| Brass | MI_Metal_Brass | 0.5, 0.33, 0.12 | polished edges, green patina in grime |
| Iron | MI_Metal_Iron | 0.045, 0.042, 0.038 | rust in grime |
| Rope | MI_Rope_Hemp | 0.36, 0.27, 0.15 | |
| Canvas | MI_Canvas | 0.62, 0.58, 0.5 | roofs, covers, later sails |
| Pier planks | MI_Wood_DockWeathered | 0.15, 0.14, 0.125 | silver-grey weathered, algae at the waterline |
| Pilings | MI_Wood_Piling | 0.06, 0.05, 0.04 | tarred, green algae band |
| Rock | MI_Rock_Coast | 0.2, 0.21, 0.21 | cool grey, ochre lichen on top faces, dark wet band |
| Sea | MI_Ocean | absorption 0.6/0.18/0.09, scattering 0.004/0.013/0.022 | deep blue offshore; teal crest light 0.1/0.75/0.62 |
| Lantern | MI_LanternGlass / ASaltLantern | 2100 K, 28 cd | warm accent light |

## Lighting presets (`ASaltLightingRig`)
- **Day:** sun at 38°, 90 klx. Exposure EV100 11–15.5. Clear blue sky, light haze.
- **Evening:** sun at 4°, behind the landmark. Exposure EV100 9.5–12.5, bias −0.7. Warm light with cool
  shadows; lanterns lit.
- **Dusk (blue hour):** sun at −3°. Exposure EV100 5–8, bias −1. Purple twilight sky, lanterns carry the
  scene. This is the strongest "warm refuge in an unfamiliar world" mood so far.

## Boat proportions (for M4)
A sturdy, beamy double-ender in the spirit of a Colin Archer boat: about 10 m long overall with 3.4 m beam,
and about 0.75 m freeboard at the test section.
- Side decks must stay at least 70 cm wide, because the player capsule is 68 cm.
- Rail and bulwark height is about 52 cm with a cap rail.
- The cabin concept is pending decision 2.
