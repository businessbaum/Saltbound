"""Import generated/source textures from Art/Textures/<Group>/ into Unreal.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/import_textures.py [Group ...]
Destination: /Game/Saltbound/Water/Textures for the Water group, otherwise
/Game/Saltbound/Materials/Textures/<Group>. Settings follow the name suffix:
  _N   normal map (BC5, linear)        _D  albedo (sRGB, BC1/BC7 default)
  _M / _ORM / _H  masks (linear, BC7)
"""
import glob
import os
import sys

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(REPO_ROOT, "Art", "Textures")


def dest_for(group):
    return "/Game/Saltbound/Water/Textures" if group == "Water" else "/Game/Saltbound/Materials/Textures/" + group


def configure(tex, name):
    if name.endswith("_N"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif name.endswith(("_M", "_ORM", "_H")):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)


def main():
    groups = sys.argv[1:] or sorted(d for d in os.listdir(SRC) if os.path.isdir(os.path.join(SRC, d)))
    tasks = []
    for g in groups:
        for path in sorted(glob.glob(os.path.join(SRC, g, "*.png"))):
            t = unreal.AssetImportTask()
            t.set_editor_property("filename", path)
            t.set_editor_property("destination_path", dest_for(g))
            t.set_editor_property("automated", True)
            t.set_editor_property("replace_existing", True)
            t.set_editor_property("save", False)
            tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for t in tasks:
        for obj_path in t.get_editor_property("imported_object_paths"):
            tex = unreal.load_asset(obj_path)
            if isinstance(tex, unreal.Texture2D):
                configure(tex, tex.get_name())
                unreal.EditorAssetLibrary.save_loaded_asset(tex)
                print("SB_TEXTURE_IMPORTED", obj_path, tex.blueprint_get_size_x(), tex.get_editor_property("compression_settings"))
    print("SB_TEXTURES_DONE", len(tasks))


if __name__ == "__main__":
    main()
