# Third-party content

All external content must be free and come with a license that allows a possible future public release.
Each entry records the source, license, attribution requirement and where the content is used.

| Content | Source | License | Attribution required | Used in |
|---|---|---|---|---|
| Unreal Engine 5.8 and engine/plugin content | Epic Games | Unreal Engine EULA (royalty terms apply only above the revenue threshold) | No (UE splash/credit rules apply for commercial release) | Whole project |
| First Person template content (temporary) | Epic Games, UE 5.8 template | Unreal Engine EULA | No | `/Game/FirstPerson`, `/Game/LevelPrototyping`, `/Game/Characters`, `/Game/Input` (to be removed) |
| IM Fell English, IM Fell English SC (fonts; the Fell types digitally reproduced by Igino Marini) | Google Fonts repository (`ofl/imfellenglish`, `ofl/imfellenglishsc`), fetched by `Tools/fonts/get_fonts.py` | SIL Open Font License 1.1 (`Art/Fonts/*/OFL.txt`) | Keep the licence with the fonts; no in-game credit required | UI names, titles, prompts, labels (`/Game/Saltbound/UI/Fonts`, D55) |
| Alegreya (font, Huerta Tipográfica; static Regular, Italic and Bold cut from the variable font) | Google Fonts repository (`ofl/alegreya`), fetched by `Tools/fonts/get_fonts.py` | SIL Open Font License 1.1 (`Art/Fonts/Alegreya/OFL.txt`) | Keep the licence with the fonts; no in-game credit required | UI descriptions, hints, counts (`/Game/Saltbound/UI/Fonts`, D55) |
| Music: "Ambient Music", "Ambient Electronics", "Dark Cinematic", "Drama Documentary" (artist: leberch) | Put into `Audio/Music/Background/` by the owner (2026-10-07); the file names (artist-title-id.mp3) are Pixabay's | Pixabay Content License (free use in games, no attribution required; not to be sold or redistributed on their own) **as the names suggest: the owner to confirm the source** | No (a credit to the artist is courteous) | `/Game/Saltbound/Audio/Music` (D84) |

No other external assets are used yet (besides the fonts and the music above). Preferred sources: Poly Haven and ambientCG (CC0), and
freesound.org restricted to CC0.
