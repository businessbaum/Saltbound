"""Import generated/source textures from Art/Textures/<Group>/ into Unreal.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/import_textures.py [Group ...] [--only=T_UI_Menu*,...]
(--only imports just the files whose names match one of the patterns, so new parts do not reimport the whole group.)
Destination: /Game/Saltbound/Water/Textures for the Water group, /Game/Saltbound/UI/Textures for the UI group (D50:
UI compression and group, no mips, never streamed), otherwise /Game/Saltbound/Materials/Textures/<Group>.
Settings follow the name suffix:
  _N   normal map (BC5, linear)        _D  albedo (sRGB, BC1/BC7 default)
  _M / _ORM / _H  masks (linear, BC7)
"""
import fnmatch
import glob
import os
import sys

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(REPO_ROOT, "Art", "Textures")


def dest_for(group):
    if group == "UI":
        return "/Game/Saltbound/UI/Textures"
    return "/Game/Saltbound/Water/Textures" if group == "Water" else "/Game/Saltbound/Materials/Textures/" + group


def configure(tex, name, group=""):
    if name == "T_Map_Region":
        # D57: the chart on the mast is seen in the world at a distance (and closer in its close look): mips, BC7.
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
        tex.set_editor_property("srgb", True)
    elif group == "UI":
        # Small parts stay uncompressed (crisp); the big painted screens (the bag, the crate) are BC7 (D55).
        big = max(tex.blueprint_get_size_x(), tex.blueprint_get_size_y()) > 700
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7 if big else unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("srgb", True)
    elif name.endswith("_N"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif name.endswith(("_M", "_ORM", "_H")):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)


def main():
    only = [p for a in sys.argv[1:] if a.startswith("--only=") for p in a[len("--only="):].split(",") if p]
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    groups = args or sorted(d for d in os.listdir(SRC) if os.path.isdir(os.path.join(SRC, d)))
    tasks = []
    group_of = {}
    for g in groups:
        for path in sorted(glob.glob(os.path.join(SRC, g, "*.png"))):
            if only and not any(fnmatch.fnmatch(os.path.splitext(os.path.basename(path))[0], p) for p in only):
                continue
            t = unreal.AssetImportTask()
            t.set_editor_property("filename", path)
            t.set_editor_property("destination_path", dest_for(g))
            t.set_editor_property("automated", True)
            t.set_editor_property("replace_existing", True)
            t.set_editor_property("save", False)
            tasks.append(t)
            group_of[os.path.splitext(os.path.basename(path))[0]] = g
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for t in tasks:
        for obj_path in t.get_editor_property("imported_object_paths"):
            tex = unreal.load_asset(obj_path)
            if isinstance(tex, unreal.Texture2D):
                configure(tex, tex.get_name(), group_of.get(tex.get_name(), ""))
                unreal.EditorAssetLibrary.save_loaded_asset(tex)
                print("SB_TEXTURE_IMPORTED", obj_path, tex.blueprint_get_size_x(), tex.get_editor_property("compression_settings"))
    print("SB_TEXTURES_DONE", len(tasks))


if __name__ == "__main__":
    main()
