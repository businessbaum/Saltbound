"""The sea's events' materials (D88, FH5: the light under the water).

  M_SB_SeaGlow (+ MI_SeaGlow)   unlit and opaque: the Old World's turquoise, brightest where the body faces the eye and fading
                                to nothing at its outline, so through dark water it reads as a soft glow rather than a cut-out
                                shape; ASaltSeaLight drives GlowIntensity (its fade and slow breath).

    python Tools/ue_exec.py Tools/ue_scripts/setup_sea_events.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MASTER = "/Game/Saltbound/Materials/Master"
MI = "/Game/Saltbound/Materials/Instances"
mel = unreal.MaterialEditingLibrary

m = sb_ue.reset_material("M_SB_SeaGlow", MASTER)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
color = sb_ue.vector_param(m, "GlowColor", (0.35, 0.95, 0.85, 1), -900, 0, "Glow")
inten = sb_ue.scalar_param(m, "GlowIntensity", 30.0, -900, 120, "Glow")
soft = sb_ue.scalar_param(m, "EdgeSoftness", 2.2, -900, 220, "Glow")
fres = sb_ue.expr(m, unreal.MaterialExpressionFresnel, -700, 220)
sb_ue.connect(soft, "", fres, "ExponentIn")
inner = sb_ue.expr(m, unreal.MaterialExpressionOneMinus, -550, 220)
sb_ue.connect(fres, "", inner, "")
shaped = sb_ue.expr(m, unreal.MaterialExpressionPower, -420, 220)
sb_ue.connect(inner, "", shaped, "Base")
two = sb_ue.expr(m, unreal.MaterialExpressionConstant, -550, 320, r=1.6)
sb_ue.connect(two, "", shaped, "Exp")
a = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -300, 60)
sb_ue.connect(color, "", a, "A")
sb_ue.connect(inten, "", a, "B")
b = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -150, 120)
sb_ue.connect(a, "", b, "A")
sb_ue.connect(shaped, "", b, "B")
sb_ue.to_prop(b, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
sb_ue.eal.save_loaded_asset(m, False)
mi = sb_ue.make_mi("MI_SeaGlow", MI, m, scalars={"GlowIntensity": 30.0, "EdgeSoftness": 2.2})
sb_ue.eal.save_loaded_asset(mi, False)
print("SB_SEA_EVENTS_MATERIALS_OK", mel.get_statistics(m).num_pixel_shader_instructions)
