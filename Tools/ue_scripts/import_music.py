"""Music (D84): imports the owner's tracks from Audio/Music/Background/<set>/ as sound waves in the SC_Music class (so the
settings' Music volume reaches them), one folder per mood that USaltMusicSubsystem plays them in:

  chill -> /Game/Saltbound/Audio/Music/Calm/SW_Music_Calm_<Name>     by day, exploring, the menus
  dark  -> /Game/Saltbound/Audio/Music/Night/SW_Music_Night_<Name>   from dusk
  drama -> /Game/Saltbound/Audio/Music/Drama/SW_Music_Drama_<Name>   storms, a boat filling with water

<Name> is the file's title word after the artist and the style (leberch-ambient-electronics-524300.mp3 -> Electronics).
Re-running replaces them. Runs in the editor: python Tools/ue_exec.py Tools/ue_scripts/import_music.py
"""
import os
import re

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Audio", "Music", "Background")
DEST = "/Game/Saltbound/Audio/Music"
SETS = {"chill": "Calm", "dark": "Night", "drama": "Drama"}
# The files' names: artist-style-title-id.mp3; the name in the game is the title's last word (or the style's).
NAMES = {"leberch-ambient-ambient-music-604092.mp3": "Ambient", "leberch-ambient-electronics-524300.mp3": "Electronics",
         "leberch-dark-cinematic-509801.mp3": "Cinematic", "leberch-drama-documentary-256162.mp3": "Documentary"}


def name_for(file):
    if file in NAMES:
        return NAMES[file]
    words = [w for w in re.split(r"[-_ ]+", os.path.splitext(file)[0]) if w and not w.isdigit()]
    return words[-1].capitalize() if words else "Track"


def main():
    music_class = unreal.load_asset("/Game/Saltbound/Audio/SC_Music")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    done = []
    for folder, mood in SETS.items():
        src_dir = os.path.join(SRC, folder)
        if not os.path.isdir(src_dir):
            continue
        for file in sorted(os.listdir(src_dir)):
            if not file.lower().endswith((".mp3", ".wav", ".ogg")):
                continue
            asset_name = "SW_Music_%s_%s" % (mood, name_for(file))
            task = unreal.AssetImportTask()
            task.filename = os.path.join(src_dir, file)
            task.destination_path = "%s/%s" % (DEST, mood)
            task.destination_name = asset_name
            task.replace_existing = True
            task.automated = True
            task.save = False
            tools.import_asset_tasks([task])
            path = "%s/%s/%s" % (DEST, mood, asset_name)
            wave = unreal.load_asset(path)
            if not wave:
                unreal.log_error("SB_MUSIC_IMPORT failed %s" % file)
                continue
            wave.set_editor_property("sound_class_object", music_class)
            wave.set_editor_property("looping", False)
            unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
            done.append(path)
            print("SB_MUSIC_IMPORTED %s %.1f s <- %s" % (path, wave.get_editor_property("duration"), file))
    print("SB_MUSIC_IMPORT_DONE", len(done))


main()
