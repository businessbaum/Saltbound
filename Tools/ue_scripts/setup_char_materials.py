"""The characters' materials (D80 the player's arms, build_arms.py, and the shovel's spadeful; D82 the Harbour Keeper): warm comic skin with a ruddier
shade for knuckles and creases, pale nails, off-white linen, dark leather, a cream shell. Calm painted surfaces on the surface
master (marked for skeletal meshes by setup_bird_materials.py), no wear. Instances are saved even when only their parameters
changed (D76's lesson: set parameters do not mark an existing instance dirty).

    python Tools/ue_exec.py Tools/ue_scripts/setup_char_materials.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402
import setup_surface_materials as S  # noqa: E402

surf = unreal.load_asset(S.MASTER + "/M_SB_Surface")
if not surf.get_editor_property("used_with_skeletal_mesh"):
    surf.set_editor_property("used_with_skeletal_mesh", True)
    unreal.MaterialEditingLibrary.recompile_material(surf)
    unreal.EditorAssetLibrary.save_loaded_asset(surf, False)
calm = {"TexNormalize": 2.6, "NormalStrength": 0.2, "RoughScale": 1.1, "WearStrength": 0.0, "GrimeAmount": 0.0, "VarAmount": 0.0}
SETS = [
    # name, tint (linear), roughness scale, textures
    ("MI_Char_Skin", (0.58, 0.33, 0.21, 1), 0.85, S.paint_textures()),
    ("MI_Char_SkinShade", (0.48, 0.24, 0.16, 1), 0.85, S.paint_textures()),
    ("MI_Char_Nail", (0.7, 0.5, 0.42, 1), 0.6, S.paint_textures()),
    ("MI_Char_Linen", (0.66, 0.62, 0.52, 1), 1.4, S.canvas_textures()),
    ("MI_Char_Leather", (0.17, 0.08, 0.04, 1), 0.9, S.paint_textures()),
    ("MI_Char_Shell", (0.78, 0.68, 0.55, 1), 0.7, S.paint_textures()),
    # D83: the Harbour Keeper's clothes, beard and eyes.
    ("MI_Char_Vest", (0.075, 0.085, 0.095, 1), 1.3, S.canvas_textures()),
    ("MI_Char_Wool", (0.3, 0.075, 0.05, 1), 1.4, S.canvas_textures()),
    ("MI_Char_Beard", (0.62, 0.6, 0.56, 1), 1.2, S.paint_textures()),
    ("MI_Char_Eye", (0.02, 0.016, 0.014, 1), 0.35, S.paint_textures()),
    ("MI_Char_Trousers", (0.1, 0.12, 0.16, 1), 1.3, S.canvas_textures()),
    # D80: a spadeful of damp sand on the shovel's blade while digging.
    ("MI_Dirt_Load", (0.42, 0.32, 0.19, 1), 1.3, S.canvas_textures()),
]
for name, tint, rough, tex in SETS:
    mi = sb_ue.make_mi(name, S.MI, surf, vectors={"Tint": tint}, scalars=dict(calm, RoughScale=rough), textures=tex)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
print("SB_CHAR_MATERIALS_OK")
