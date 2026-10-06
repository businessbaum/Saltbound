"""The seagull's extra materials (D76, build_birds.py SK_Gull): the beak's red spot, the pale pink feet, the pale iris. The
white, grey, dark and beak instances are setup_surface_materials.py's (MI_Bird_*). The surface master is marked for skeletal
meshes (the gull is skinned).

    python Tools/ue_exec.py Tools/ue_scripts/setup_bird_materials.py
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
    unreal.EditorAssetLibrary.save_loaded_asset(surf)
calm = {"TexNormalize": 2.6, "NormalStrength": 0.2, "RoughScale": 1.1, "WearStrength": 0.0, "GrimeAmount": 0.0, "VarAmount": 0.0}
# D76: the mantle a shade darker than D39's, so the wings read grey against the white body from above.
# Parameters set on an existing instance do not mark it dirty: save each one regardless.
mis = [sb_ue.make_mi("MI_Bird_Grey", S.MI, surf, vectors={"Tint": (0.16, 0.18, 0.205, 1)}, scalars=dict(calm, RoughScale=1.3), textures=S.paint_textures())]
for name, tint, rough in (("MI_Bird_Red", (0.5, 0.05, 0.025, 1), 0.9), ("MI_Bird_Leg", (0.6, 0.34, 0.28, 1), 1.2),
                          ("MI_Bird_Iris", (0.68, 0.62, 0.36, 1), 0.6)):
    mis.append(sb_ue.make_mi(name, S.MI, surf, vectors={"Tint": tint}, scalars=dict(calm, RoughScale=rough), textures=S.paint_textures()))
for mi in mis:
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
print("SB_BIRD_MATERIALS_OK")
