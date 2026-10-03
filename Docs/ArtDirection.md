# Art direction (revision 2, owner feedback applied; see Decisions D16)

Reference: Sea of Thieves for the sea, rocks and wood (owner-supplied reference images, 2026-10-03).
Saltbound aims for a calmer, original look with less exaggerated shapes. **Stylised, not realistic.**
Quality means crisp, readable, hand-painted material detail without blur, on simple chunky shapes.

## Stylised realism (D21)
Sea of Thieves looks like a painted pirate adventure brought into 3D: believable light, water and atmosphere,
combined with deliberately simplified, slightly exaggerated, hand-crafted shapes.
- **Shapes:** bold and slightly crooked. Boards bend, buildings lean, rocks have broad angular faces,
  objects have chunky proportions. Use `sb_timber` for anything made of wood.
- **Textures:** large readable details (grooves, chips, broad colour variation), never dense photographic
  detail.
- **Materials:** wood worn and matte, metal with strong highlights, water glossy and deep.
- **Colour:**
  - Rich but controlled: turquoise sea, warm sand, golden sun, cool shadows.
  - Darker, muted objects let the bright areas stand out.
- **Light:** soft atmospheric light plus strong directional sun. Haze fades distant islands into the sky.

## Principles
- **Shapes:** chunky and readable, with believable proportions. Planks 3–4 cm thick, rails about 8 cm.
  Soft bevels of 3–8 mm on every hard edge (`sb.soften`). Slight irregularity: plank length/height jitter,
  ±0.5–1° rotations, sub-mm vertex jitter.
- **Surface detail:** hand-painted and crisp. Procedural tiling textures at 2048 px per 1–2.5 m tile are
  designed as painted shapes:
  - Stone: flat tone patches with hard edges, sparse painted cracks with a highlight lip, chips.
  - Wood: straight grain lines with knots.
  - Paint: brush strokes over the grain.

  Avoid soft noise (it reads as blur) and carved geometric detail (cracks, plates). Variation also comes
  from vertex masks:
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
| Rock | MI_Rock_Coast | 0.29, 0.275, 0.25 | warm grey painted stone, ochre lichen on top faces, algae and barnacle tide zone, dark wet band |
| Sea | MI_Ocean | absorption 1.4/0.36/0.15, scattering 0.008/0.04/0.1 | deep cobalt body; turquoise crest light 0.12/0.9/0.78; foam 0.7/0.9/0.97, translucent |
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
