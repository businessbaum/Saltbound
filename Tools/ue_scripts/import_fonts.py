"""Import the UI fonts (Art/Fonts, fetched by Tools/fonts/get_fonts.py) as font faces under /Game/Saltbound/UI/Fonts.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/import_fonts.py

The families are put together in C++ at runtime from these faces (SaltUI::Font in UI/SaltUIStyle.h, resolved lazily):
the composite font's typeface entries are not editable from Python.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEST = "/Game/Saltbound/UI/Fonts"
FACES = {
    "FF_FellEnglish_Regular": "IMFellEnglish/IMFeENrm28P.ttf",
    "FF_FellEnglish_Italic": "IMFellEnglish/IMFeENit28P.ttf",
    "FF_FellEnglishSC_Regular": "IMFellEnglishSC/IMFeENsc28P.ttf",
    "FF_Alegreya_Regular": "Alegreya/Alegreya-Regular.ttf",
    "FF_Alegreya_Italic": "Alegreya/Alegreya-Italic.ttf",
    "FF_Alegreya_Bold": "Alegreya/Alegreya-Bold.ttf",
}


def main():
    tasks = []
    for name, rel in FACES.items():
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(ROOT, "Art", "Fonts", rel))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for t in tasks:
        for path in t.get_editor_property("imported_object_paths"):
            asset = unreal.load_asset(path)
            print("SB_FONT_IMPORTED", path, type(asset).__name__)
            if isinstance(asset, unreal.FontFace):
                # Load the face when it is first used; keep the glyphs crisp at UI sizes.
                asset.set_editor_property("loading_policy", unreal.FontLoadingPolicy.LAZY_LOAD)
                asset.set_editor_property("hinting", unreal.FontHinting.DEFAULT)
                unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print("SB_FONTS_DONE", len(tasks))


main()
