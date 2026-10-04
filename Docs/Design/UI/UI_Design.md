# Saltbound UI design

The owner's decisions on the look of the tool wheel, the satchel, the boat's stores, the interaction prompts, the
landfall banner, the logo and the save sign (2026-10-04). This is the spec for building them in Unreal and Blender. Nothing here is built yet.
The pictures are the target. The engine and Blender can do better than the pictures (real light, real meshes,
motion); section 11 says where.

## 1. Decisions

| Part | Decision | Reference |
|---|---|---|
| Tool wheel | Adopted as designed: a rope ring with six brass medallions, hold Tab | `UI_wheel.png` |
| Satchel | Style A, **pressed rings**: round dishes pressed into leather. No pouch flaps (rejected: "too much"). | `UI_satchel.png` |
| Stores (barrel, crate) | The satchel beside the store. The crate is drawn as an open chest with 5 × 2 compartments. | `UI_store_crate.png` |
| Short interaction (press E) | **Key on a parchment scrap**, the action in painted lettering | `UI_prompt.png` |
| Hold interaction | Same prompt; **ink soaks into the key's scrap** from the bottom | `UI_hold.png`, `UI_hold_states.png` |
| Landfall | The swallowtail **ribbon banner** for a new island, **without the sketch** above it | `UI_landfall.png` |
| System messages ("The satchel is full") | **None for now** | – |
| Save sign | The **knotted-ring logo, no background**, bottom-right corner while the game saves; the rose turns 45° at a time and settles ("Seeking north") | `UI_save_sign.png` |
| Not decided | The look of pickup notices ("+1 Seashell"): keep today's | – |
| Not in scope | Chart/map redesign, logbook, other landfall moods (ancient place, home, danger), other HUD changes | – |
| Logo | **The knotted ring** (variant 8): a compass rose with the shard as north, in a rope ring tied off at the bottom, flat ink | `Logo_variants.png` (no. 8) |
| Launch intro | **The knot becomes the name**: the logo inks in and finds north; its compass points fly out and burst into the letters, ink from the knot becomes the S, the ring stays as the O and the knot's rope ends grow into the underline (8 s) | `Saltbound_Intro.mp4`, `UI_intro_lockup.png` |

## 2. Files

All in `Docs/Design/UI/`:

| File | What it is |
|---|---|
| `UI_Design.md` | This spec |
| `UI_Agent_Prompt.md` | The task prompt for the agent that builds it |
| `UI_wheel.png`, `UI_satchel.png`, `UI_store_crate.png` | Targets, 1920 × 1080 |
| `UI_prompt.png`, `UI_hold.png`, `UI_hold_states.png`, `UI_landfall.png` | Targets, 1920 × 1080 (states: 1920 × 300) |
| `UI_Satchel.html` | Canvas code that draws the wheel, satchel and store (`#wheel`, `#satchel`, `#locker`) |
| `UI_Prompts.html` | Canvas code that draws the prompt, hold and landfall (`#prompt`, `#hold`, `#states`, `#landfall`) |
| `UI_save_sign.png` | Target for the save sign, 1920 × 1080 |
| `T_UI_SaveKnot_Ring.png`, `T_UI_SaveKnot_Rose.png` | The save sign's two layers, painted, transparent, 256 × 256 |
| `T_UI_SaveKnot_Ring_Mask.png`, `T_UI_SaveKnot_Rose_Mask.png` | The same two layers as white masks (for tinting or for a material) |
| `Save_Sign.html` | The save sign live: in a game frame, over sky, sea and wood, and the exported layers (`#frame`, `#ring`, `#rose`, `#ringmask`, `#rosemask` export them) |
| `Logo_Variants.html`, `Logo_variants.png` | The fifteen logo variants; the chosen one is no. 8 |
| `Launch_Intro.html` | The launch intro, live, with replay and a scrubber (`#f<frame>` draws one frame at 30 fps) |
| `Saltbound_Intro.mp4` | The launch intro as a video: 1920 × 1080, 30 fps, 8 s, H.264 |
| `UI_intro_lockup.png` | The intro's last frame: the SALTBOUND lockup |
| `Explorations/` | Options that were compared and **not** chosen. Only for history; do not build from them. |

The HTML files are the exact spec: every position, radius, colour and font is in their code. Open one in a browser;
the `#name` shows that frame alone at 1920 × 1080. All pixel values below are at 1920 × 1080, which is UMG DPI scale
1.0 (the default curve maps a 1080 short side to 1.0).

## 3. Style system (shared by every part)

**Principles**
- Things look like objects from the boat: stitched leather, brass, hemp rope, parchment, ink, golden-brown wood.
  There are no flat panels and no frames around them.
- The world keeps running behind every screen in this spec. Only Esc pauses.
- Stylised and hand-crafted (D16, D38): chunky shapes, broad bevels, calm textures, no photographic grain.
- **Turquoise is reserved** for the lost civilisation (the ancient shard, `MI_AncientGlow` 0.35/0.95/0.85). Never use
  it for ordinary UI.

**Fonts** (both SIL Open Font License 1.1, from the Google Fonts repository; record them in `Docs/ThirdParty.md`)
- **IM Fell English** and **IM Fell English SC**: names, titles, prompts, debossed and burned labels.
- **Alegreya** (Regular, Italic, Bold): descriptions, hints, counts.
- Import them as `UFontFace` / `UFont` under `/Game/Saltbound/UI/Fonts` with a versioned script. Resolve them lazily
  (`Core/SaltContent.h`), never in constructors. They replace Roboto in these screens.

**Colours**
| Use | Value |
|---|---|
| Ink on paper | #3A2616 (text at ~94 % opacity, plus a soft 0.7 px blurred copy at 35 % as ink bleed) |
| Painted lettering over the world | fill gradient top→bottom #FFF8E6 → #F2E2BF → #D6B980, a speckled paint texture at 40 %, a dark rim stroke (13 % of the font size, rgba 18/10/4 at 62 %) and a soft dark halo (blur ≈ 50 % of the size). It must read on bright sky and on dark wood. |
| Parchment | ≈ 230/208/160 sRGB with broad mottling, a few stains and a burnt inner edge (inner shadow rgba 125/72/24 at 55 %, blur 16) |
| Leather | ≈ 118/70/40 sRGB, mottled, fine pebble grain, a few pale scuffs. Darker parts use the same texture at lower brightness. |
| Brass | sharp highlight bands: #5A3B12, #C8973E, #FBE3A0, #D9A748, #7A5418, #B9883A, #E8C77C, #4A300E |
| Wood (crate) | golden-brown ≈ 156/104/60 sRGB, straight grain, a few knots; iron #24211D |
| Wax seal | oxblood: #C9553D (highlight) → #8E2A1E → #3A0D08 (edge) |
| Selection | a warm amber glow (255/200/115) and a gold ring (255/214/140) with a soft glow |

**Reusable parts**
- **Stitching:** a saddle stitch, thread #D9C08E, 9 px stitches every 16 px. Each stitch has a dark hole at both ends,
  a dark shadow 1 px down-right and a thin light highlight.
- **Count coin:** a dark-leather disc (radius 17) with a 2.6 px brass rim and the number in Alegreya Bold 19 px
  (17 px for two digits), #F3E6C8. Only shown for 2 or more.
- **Keycap** (on leather screens only): a dark-leather key (#5B3A22 → #2A170B, radius 7) with a 2.4 px brass rim and
  a cream Alegreya Bold letter. It widens for "Tab". There are gamepad variants.
- **Key scrap** (over the world: prompts): see section 7.
- Minimum text size at 1080p: 21 px for body text, 26 px for hints.

## 4. Tool wheel (hold Tab)

![Tool wheel](UI_wheel.png)

- **Layout:** centre (960, 540). A light background blur (≈ 2) and darkening (~0.78), with a soft dark disc behind the ring.
- **Rope ring:** radius 262, thickness 46, three-strand hemp with a visible lay (strands at ~57°). Cylinder shading,
  light from the top left, and a soft shadow (blur 22, 12 px down).
- **Medallions:** six on the ring at −90° + 60°·i: 1 Compass, 2 Spyglass, 3 Chart, 4 Lantern, 5 Satchel, 6 Empty hands.
  - Each is a dark leather disc (radius 70, selected 84) with a strong inner shadow, a 10 px brass bezel and 8 brass
    rivets, lashed to the rope by 5 turns of pale twine on both sides.
  - Selected: bigger, an amber halo (×2.1 radius) and a bright rim.
- **Key hints:** keycaps 1–6 (30 px) outside each medallion at radius R + r + 34.
- **Centre text:**
  1. The tool's name in IM Fell English 64 px, painted lettering.
  2. A hint in italic 28 px ("Read your heading").
  3. A thin divider with a small turquoise diamond.
  4. "let go of Tab to take it" in 22 px.
- **Bottom hints** at y ≈ 1000: **Tab** "hold, aim, let go" · **1 – 6** "pick directly".
- **Input:**
  - New `IA_ToolWheel`: hold **Tab** or hold **LB** (`Gamepad_LeftShoulder`, free). Remove Tab from `IA_Inventory`;
    I and View stay.
  - While it is held, the mouse or right stick aims instead of turning the camera. The selection is the segment in
    the pointer's direction (mouse deadzone ~40 px, stick ~0.35).
  - Release takes the tool, Esc / B cancels, keys 1–6 pick directly.
- **Motion:** open and close by scaling from 0.9 with a fade (~0.12 s); the medallions pop in 20 ms apart.
- **Entries:** data-driven (`FSaltToolDef`: Id, Name, Hint, Icon, action). A tool not owned yet shows an empty
  medallion (leather and bezel, no picture).
- **Actions:** Satchel opens the satchel. Chart opens the current map (M). Empty hands puts away what you hold.
  Compass, Spyglass and Lantern are held tools (phase 3):
  - Compass: held low on the right, with its needle settling on world north (+X).
  - Spyglass: the field of view eases to ~20° in 0.35 s, with a dark circular vignette and a brass rim. Mouse
    sensitivity scales with the field of view.
  - Lantern: held in the left hand with a warm point light (~2100 K, as `ASaltLantern`).

## 5. Satchel (I)

![Satchel](UI_satchel.png)

| Part | Placement (1080p) | Notes |
|---|---|---|
| Backdrop | full screen | `UBackgroundBlur` ≈ 8, brightness ≈ 0.55, a dark vignette (edges ~70 %) |
| Flap, thrown back | 1120 × 230 at (400, 236), top radius 150 | leather at ~62 %, stitched 24 px inside |
| Bag body | x 290–1630, top y 392, runs off the bottom | top radius 50, a deep inner shadow at the top, a bevel, a stitched border 24 px inside |
| Strap and buckle | strap 76 × 236 at (922, 300); buckle 108 × 66 at (906, 470) | the strap shows through the buckle |
| Header | "SATCHEL" at (352, 490), "7 / 24" right-aligned at x 1328 | debossed: dark letters with a light lip 1–2 px below |
| Slots | 8 × 3, centres x = 401 + 124·col, y = 616 + 140·row | see below |
| Front lip | a curved band from y ≈ 1004 (ends) to ≈ 970 (middle), x 262–1658 | rolled edge, highlight, stitching; the key hints sit on it |
| Tag | 300 × 500, hole at (1498, 540), rotated ≈ 4° | hangs on two strands of twine from a brass D-ring at (1603, 440) |

- **Slot (pressed ring):**
  - A shallow dish of radius 50, slightly darker than the leather. It has a dark inner shadow at the top (6 px down,
    blur 12) and a warm light lip at the bottom, with a stitched ring at radius 60.
  - The item sits fully visible: icon 86 px with a soft contact shadow (ellipse 64 × 18, 20 px below the centre).
  - The count coin sits at (+38, +36).
  - Empty: just the dish and the ring.
- **Selected:** the icon grows to 96 px and lifts 14 px, with a warm glow in the dish and a gold ring at radius 67.
  Animate it in ~0.12 s. The ancient shard has its own slow turquoise glow (≈ 2.5 s period).
- **Tag content** (ink, 28 px inside the tag's left edge):
  1. Name, IM Fell English 44 px.
  2. "1 in this pocket · 5 fit", italic 21 px.
  3. An ink rule.
  4. The description, Alegreya 21 px, line height 29, wrapped to 244 px.
  5. Actions with keycaps, 46 px apart: **E** the item's use (greyed "(later)" if it has none), **R** Turn it in
     your hand, **G** Drop it.
  - An empty slot shows "Empty pocket" and one line of help.
- **Tag motion:** it pivots at the hole. When the selection changes it swings (a ~6° kick, damped, settles in ~0.8 s).
- **Keys:** I or Esc closes; gamepad View opens and B closes. Hints on the lip: **I** "close" · "mouse or d-pad:
  choose a pocket". Tab no longer opens the satchel.
- **Map:** the satchel has no tabs. The current map keeps working on M, unchanged, as its own screen.

## 6. Stores on the boat (E at the barrel or crate)

![Store](UI_store_crate.png)

The stores are the D52 barrel and crate, with 10 slots each.

- **Left, the satchel:** a leather panel 780 × 790 at (120, 180), the same materials, with "SATCHEL" and "6 / 24"
  debossed. Pressed rings of radius 44 in 6 × 4, centres x = 225 + 114·col, y = 374 + 150·row.
- **Right, the crate:** an open chest seen from slightly above, x 1010–1790, y 380–840.
  - **Lid:** propped open behind it, showing its planked inner face, with iron straps near the outer edges and iron
    hinges. It has **"CRATE" burned in** (IM Fell English SC ~50 px, dark brown with a scorch halo) and the count
    under it ("2 / 10").
  - **Box:** a 36 px rim with iron L-plates on the corners. Inside: a shadowed far wall, thin side walls, floor planks,
    and wooden dividers making **5 × 2 compartments**. Items lie on the floor with shadows; counts use the coin.
- **The barrel:** same idea but round (default, owner to confirm): staves, two iron hoops, its head leaning against
  it with the burned name. Ten compartments: 3 inside, 7 around.
- **Moving:** click moves the stack, Shift-click moves one.
  - The icon flies along a quadratic arc (~0.25 s, ease-out) with a fading dotted path.
  - The target slot is outlined with a dashed warm line during the flight, and the coin rides along.
  - E / Esc closes. Hints: "Click: move the stack · Shift-click: move one · **E** close the lid".
  - If the lid is a separate mesh in the world, swing it open while the screen is up.

## 7. Short interaction prompt (press E)

![Prompt](UI_prompt.png)

- **Where:** fixed on screen, under the focus dot. The prompt row is centred 66 px below the screen centre. It is not
  pinned to the object; players of similar games find pinned prompts jumpy.
- **Key scrap:**
  - 58 × 58 torn parchment: a jagged edge (points every 7 px, inward jitter up to 4 px, an occasional deeper tear),
    rotated −3°.
  - A burnt inner edge, a thin dark edge line, and a drop shadow (blur 14, offset 3/6).
  - The key letter is in IM Fell English SC 32 px, ink, centred, with ink bleed. With a gamepad, the face button
    glyph is inked on the same scrap.
- **Action text:** painted lettering, IM Fell English 36 px, 18 px right of the scrap, baseline 12 px below the key's
  centre. The text comes from `USaltInteractableComponent::PromptText` ("Open the barrel").
- **Motion:** fade in and rise 6 px in 0.15 s, while the scrap's rotation settles from −6° to −3°. Fade out in
  0.1 s; a new target cross-fades.
- **Replaces** today's `[E]  …` text prompt in `USaltHUDWidget`.

## 8. Hold interaction

![Hold](UI_hold.png) ![Hold states](UI_hold_states.png)

- The same prompt, with "Hold to …" text. Progress is **ink rising through the key's scrap from the bottom**:
  - The ink has a gently moving wavy surface (amplitude ~2 px, wavelength ~28 px, drifting with time).
  - Ink colour rgba 58/30/12 at 78 %, with a soft edge (blur ~1.2 px).
  - Where the ink covers the letter, the letter turns parchment-light (#EFDFB8).
- **Let go early:** the ink drains back in ~0.25 s.
- **Full:** the scrap soaks through completely with a short pulse (scale 1.06 → 1 in 0.15 s), the action runs and
  the prompt hides.
- **Build it as a UI material** (e.g. `M_UI_KeySoak`: parameters Progress and Time; the mask is a smoothstep on UV.y
  plus a sine wobble; it blends the scrap, the ink colour and the light letter), driven from C++.
- **Which actions are holds is the owner's call.** The example and default is raising the anchor at the windlass
  (about 0.8 s). Everything else stays a short press.

## 9. Landfall banner (a new island)

![Landfall](UI_landfall.png)

- **When:** the first time the player comes near an island they have not found yet. Default: within 300 m of its
  shore, or on entering a trigger around it. It shows once per island, and the found islands are saved (bump the
  save version).
- **Place data:** the names exist today in `USaltSatchelWidget` (`Places()`). Move them to shared data with a
  position, a discovery radius and a "found" flag.
- **Layout (1080p):** centred, with the top of the band at y 150.
  - **Heading:** "A new island", painted lettering, small caps 24 px, 18 px above the band.
  - **Band:** a parchment ribbon, arched (its ends 18 px lower than its middle), 90 px high. Its width is the name
    plus 200 px (at least 600). Light at the top, darker at the bottom, a burnt inner edge, and two pairs of ink rules
    9 and 14 px inside the top and bottom edges.
  - **Tails:** behind the band at both ends. Each starts 46 % down the band, is 92 % of its height, reaches 118 px out
    with 40 px tucked under the band, and has a 36 px swallowtail notch. They are at 74 % brightness, with a dark fold
    triangle where the band turns under.
  - **Name:** IM Fell English SC 66 px, letter spacing 5 px. Each letter sits on the arch and is turned to its curve;
    ink with bleed.
  - **Seal:** an oxblood wax seal (radius 40) pins the left end, centred 8 px inside the band's end and 60 % down. Its
    glyph is the island with the broken gateway.
  - **Subtitle:** painted lettering, italic 28 px, ≈ 56 px below the tails: distance and direction from Saltbound Cove
    ("1.7 km north-east of Saltbound Cove"). Add "· now on your chart" only once the map marks found places.
  - **No sketch** above the banner (the owner removed it).
- **Motion:**
  1. The ribbon unrolls from the centre outward (0.6 s, ease-out; a horizontal mask or scale).
  2. At 0.5 s the seal stamps down (scale 1.4 → 1 with a slight overshoot, 0.2 s).
  3. The name inks in from left to right (0.6 s), then the subtitle fades in.
  4. It holds for ~4 s, then rolls up and fades (0.5 s).
- **Not now:** the other moods (an ancient place, home waters, danger) are in `Explorations/UI_Holds_Landfalls.html`
  for later.

## 10. Not now

- No system messages such as "The satchel is full".
- No chart/map redesign, logbook, or other HUD changes.
- Pickup notices stay as they are until the owner decides their look. Ask before restyling.

## 11. How to build it (Unreal and Blender, UI only)

**Unreal (C++ and editor Python)**
- **Widgets** stay built in C++, like today (`UI/SaltHUDWidget`, `UI/SaltSatchelWidget`, `UI/SaltStoreWidget`,
  `UI/SaltMenuWidget`), with no widget Blueprints. Add a tool wheel widget, a landfall widget, and the key-scrap
  prompt in the HUD.
- **Textures** come from versioned scripts: extend `Tools/textures/gen_ui.py` (numpy + Pillow), write PNGs to
  `Art/Textures/UI/`, and import them with `python Tools/ue_exec.py Tools/ue_scripts/import_textures.py UI` (UI group,
  no mips). Use 9-slice brushes where a part stretches.
- **Materials for motion** (the ink soak, glows, the ribbon's unroll mask) are made by a script, like the other
  materials (`sb_ue`; never delete-and-recreate; use `reset_material()`).
- **Item icons from the real things** (the "icon booth"):
  - Set up a `SceneCaptureComponent2D` with a fixed 3/4 view (≈ 50° down, 30° to the side), the Day light plus a rim
    light, and a transparent background.
  - Render each `SM_Pickup_*` with its real material instances at 512 px, downscale to 256, and export
    (`RenderingLibrary.export_render_target`) to `Art/Textures/UI/T_Icon_*.png`.
- **Inputs:** `Player/SaltInputConfig.cpp`. The satchel and store logic is in `Player/SaltPlayerController.cpp`, the
  stores in `Boat/SaltBoat.cpp`, the items in `Core/SaltItems.cpp`.

**Blender (headless, versioned scripts in `Art/Blender/Scripts/`)**
- **Hero UI pieces as real 3D renders:** model them, render orthographically with the same light, and save as UI
  textures:
  - the rope ring (a circle curve with a 3-strand profile and twist);
  - the brass bezel and rivets, the buckle, the D-ring, the grommet;
  - the count coin, the slot dish (a leather slab with a pressed dish and real stitch holes);
  - the wax seal, the crate.

  Real highlights and real rope twist are what make it look made, not drawn.
- **Tool props for the wheel and the hands:** compass, spyglass, rolled chart, lantern (reuse), small satchel, and a
  pair of worn leather gloves for "Empty hands". Use `sb_timber` and the existing materials. Render their medallion
  pictures in the icon booth; the same meshes are the held tools.
- Check every model with `Tools/preview_blend.py` before rendering or importing (D53). Never import while PIE runs.

**Checks for each step**
- Extend `sb.Dev.RunInventoryTests`. Cover: slot selection; the tag text; the wheel by angle and by 1–6; Tab opens the
  wheel, not the satchel; store moves give the right counts; a hold fills, drains and fires; a landfall shows once
  and is saved; the save sign shows for every save (at least 1.4 s) and hides after it.
- Add dev commands for captures next to `sb.Dev.Satchel`, e.g. `sb.Dev.Wheel 0|1 [i]`, `sb.Dev.Store crate|barrel`,
  `sb.Dev.Landfall <place>`, `sb.Dev.Hold <0..1>`, `sb.Dev.SaveSign 0|1`.
- Capture each screen in PIE with `shot showui` at 1920 × 1080 (never grab the desktop). Put it next to its reference
  PNG in one A/B sheet, and look at it before calling it done.
- Record the decisions in `Docs/Decisions.md` (next free D-number) and update `Docs/Progress.md` (controls: Tab = tool
  wheel, I = satchel). Commit after each verified step.

## 12. Order of work

1. Shared parts: fonts (with `ThirdParty.md`), textures, keycap, count coin, painted lettering, key scrap.
2. Prompt (7) and hold (8) in the HUD.
3. Save sign (14) in the HUD, and the launch intro (15) as a startup movie.
4. Satchel restyle (5), map moved out of it.
5. Store restyle (6): crate first, then the barrel.
6. Landfall banner (9) with place data and saving.
7. Icons from the icon booth.
8. Tool wheel (4) with the input change, then the held tools and their props.

## 13. Questions for the owner (ask before building the part)

1. Only I opens the satchel; Tab becomes the tool wheel. Fine? (Default: yes.)
2. Which tools does the player own from the start? (Default: compass and lantern; the spyglass is found.)
3. Should the barrel be round, or look like the crate? (Default: round.)
4. Which actions are holds? (Default: only raising the anchor.)
5. What counts as "near" an island for the landfall? (Default: 300 m from its shore.)

## 14. Logo and save sign

### Logo (chosen)

![Logo variants](Logo_variants.png)

**Variant 8, the knotted ring.** It is a compass rose (the ancient shard as its north point) inside a hemp rope ring
that is tied off at the bottom, drawn in flat one-colour ink. The owner liked this flat style and asked for nothing
fancier. A lockup with the name and the menu or splash use are not designed yet. The other directions are in
`Explorations/` (`Logo.html`, `Logo_directions.png`).

### Save sign (chosen: "Seeking north")

![Save sign](UI_save_sign.png)

- **What:** the logo on its own, with **no background** (no scrap, no panel), in the bottom-right corner while the
  game saves. There is no text.
- **Look:** the same painted treatment as the prompt lettering:
  - salt-white fill (#FFF8E6 → #F2E2BF → #D6B980, top to bottom);
  - a dark rim (#120A04 at 62 %, 1.6 px all round);
  - a soft dark halo (blur 10, 70 %).

  It reads on bright sky, sea and dark wood (`Save_Sign.html` shows all three).
- **Place and size (1080p):** centre at (1832, 992), 88 px from the right and the bottom edge; drawn 96 × 96 (the ring
  is about 72 px across). It scales with the UMG DPI scale.
- **Motion:**
  - The ring and knot stay still. Only the rose turns: one 45° step every 0.7 s, each with a damped spring and a
    little overshoot (angle per step = 1 − e^(−7u)·cos(11u), u from 0 to 1 over the step).
  - Fade in 0.2 s when saving starts. Stay at least 1.4 s (two steps), even when the save is instant.
  - When the save is done, finish the current step, then fade out in 0.3 s.
  - It does not pause the game and takes no input.
- **When:** every save: mooring, the pause menu's Save, quitting (D46). Hook it to the save subsystem's start and
  end (`Core/SaltSaveSubsystem`).
- **Assets:**
  - `T_UI_SaveKnot_Ring.png` and `T_UI_SaveKnot_Rose.png`: painted, transparent, 256 × 256. Both use the **same
    frame**: lay them on top of each other and rotate the rose about its **image centre** (pivot 0.5, 0.5). The ring
    sits 3.6 px below that centre by design.
  - `*_Mask.png`: the same layers as white masks, if the painted look is rebuilt in a material.
  - Import as UI textures (no mips, user-interface compression).
- **Also possible later:** the same sign, larger and centred, on a loading screen. The owner has not asked for it.
- **Not chosen:** "Hauling the rope" and "Inked in" (`Explorations/Loading_Knot.html` and its flipbooks).

## 15. Launch intro: the knot becomes the name

![Lockup](UI_intro_lockup.png)

An 8 s intro when the game starts. The logo does not just give way to the word; it **turns into** it:
- the compass points become the letters;
- ink from the knot becomes the S;
- the ring with its shard becomes the O, and the knot's two rope ends grow on, seamlessly, into the underline.

`Launch_Intro.html` plays it live (replay and scrub); `Saltbound_Intro.mp4` is the finished video.

| Time (s) | What happens |
|---|---|
| 0 – 0.6 | Fade up from black: a deep sea-night (radial navy), slow drifting mist, salt specks rising |
| 0.35 – 1.45 | The knot inks in, then the rope ring draws itself up from both ends (painted salt-white with a dark rim) |
| 1.2 – 1.9 | The rose inks in, the shard first, then point by point |
| 1.9 – 3.0 | The rose spins two turns and settles on north with an overshoot (ease-out-back) |
| 3.0 – 3.4 | The shard flashes and stays turquoise; a thin turquoise ring of light runs outward |
| 3.3 – 4.4 | The ring shrinks (radius 230 → 52 px) and glides into place as the **O**; its lines thicken to match the letters |
| 3.55 – 4.85 | The **seven compass points break off** one by one (every 0.08 s, nearest letters first). Each flies 0.8 s along an outward arc, spinning, turning upright and growing to the cap height. Mapping: NE→U, E→N, SE→D, S→B, SW→T, W→L, NW→A. |
| 4.0 – 5.1 | 55 % into its flight, each point **bursts into ~340 ink particles** that swirl (a sideways swing up to ±45 px) into its letter's shape over 0.62 s. The painted letter fades in as the particles settle. |
| 4.0 – 4.95 | The knot stays tied. **Its two free rope ends grow on** from where they end in the logo, in the same width, paint and colour, so there is no seam. They curve down and outward and run left and right as the underline. |
| 4.12 – 5.0 | **Ink flows off the knot and flies left to become the S**, at the same time as L and D (not after the others) |
| 5.5 – 6.4 | A sheen of light sweeps across the word |
| 6.4 – 8.0 | Hold; the shard breathes softly. Then the game. |

- **Lockup:**
  - "SALTBOUND" in IM Fell English SC 150 px, letter spacing 0.16 em, painted salt-white with a fine dark rim
    (6 % of the size) and a soft halo. The baseline is at y 569 (1080p), centred.
  - The O is the knotted ring (radius ≈ 0.34 × the size). The **turquoise shard is its needle**; the other points left
    to become letters, and a small pin sits at the centre.
  - The underline is the knot's own rope: its two free ends continue from the tips of the knot (≈ 7 px wide, the knot's paint and colour), curve down and run 1.42 × the ring's radius below the O's centre to 34 px beyond the S and the D, with plain rounded ends. The knot under the O stays as in the logo.
- **In Unreal (simplest):**
  - Play the video as a **startup movie**: copy it to `Saltbound/Content/Movies/Saltbound_Intro.mp4`.
  - Under *Project Settings → Movies*, add it to *Startup Movies*, turn on *Movies Are Skippable*, and turn off
    *Wait for Movies to Complete*. Windows plays MP4 through Media Foundation.
  - The game then continues where the player left off (D46).
- **In Unreal (later, when there is a main menu):** play it in the menu with a Media Player and a Media Texture, and
  let the menu fade in over the last held frame. A live rebuild in Niagara/UMG (the particles as a GPU emitter that
  samples the letter's glyph texture) is possible but not needed.
- **Skip:** any key or button skips it.
- **Sound (optional, CC0 only):** a quill scratch for the ink, a rope creak on the spin, a soft bell on the shard's
  flash, a rush of small ticks as the points burst into ink, a rope running out as the underline grows,
  a low swell of sea under the word.
