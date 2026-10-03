"""Milestone 1 placeholder surface material and instances used by the pipeline test asset.

Superseded by the material library built during the visual test scene (Milestone 3);
MI names stay stable so imported meshes keep their slot assignments.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MASTER_DIR = "/Game/Saltbound/Materials/Master"
MI_DIR = "/Game/Saltbound/Materials/Instances"

m = sb_ue.reset_material("M_SB_Placeholder", MASTER_DIR)
base = sb_ue.vector_param(m, "BaseColor", (0.5, 0.5, 0.5, 1), -500, -100)
rough = sb_ue.scalar_param(m, "Roughness", 0.6, -500, 100)
metal = sb_ue.scalar_param(m, "Metallic", 0.0, -500, 250)
sb_ue.to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
sb_ue.to_prop(metal, "", unreal.MaterialProperty.MP_METALLIC)
sb_ue.mel.recompile_material(m)
sb_ue.eal.save_loaded_asset(m)

sb_ue.make_mi("MI_Placeholder", MI_DIR, m, vectors={"BaseColor": (0.5, 0.5, 0.5, 1)})
sb_ue.make_mi("MI_Wood_Planks", MI_DIR, m, scalars={"Roughness": 0.7}, vectors={"BaseColor": (0.36, 0.2, 0.09, 1)})
sb_ue.make_mi("MI_Metal_Iron", MI_DIR, m, scalars={"Roughness": 0.45, "Metallic": 1.0}, vectors={"BaseColor": (0.12, 0.11, 0.1, 1)})
print("SB_PLACEHOLDER_MATERIALS_OK")
