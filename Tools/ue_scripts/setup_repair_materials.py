"""The water of the ship repair system (D74; script-owned, safe to re-run).

  /Game/Saltbound/Materials/Master/M_SB_DeckWater      translucent sea water on the boat (forward-shaded surface lighting, so it
                                                       catches the sky's reflection): a blue-green body, streaks of white water
                                                       running along the mesh's V (UV0) as much as FoamAmount says
  /Game/Saltbound/Materials/Instances/MI_DeckWater     the water over the deck (calm, a little foam)
  /Game/Saltbound/Materials/Instances/MI_WaterSpurt    the sea spurting in at a hole (white, running fast)

Run in the editor: python Tools/ue_exec.py Tools/ue_scripts/setup_repair_materials.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MASTER = "/Game/Saltbound/Materials/Master"
MI = "/Game/Saltbound/Materials/Instances"

FOAM_HLSL = r"""
// Streaks of white water running along V: bands that wobble across U and break up.
float v = UV.y * Bands - Time * Speed;
float wobble = 0.18 * sin(UV.x * 6.2832 * 2.0 + v * 1.7) + 0.1 * sin(UV.x * 6.2832 * 5.0 - v * 2.3);
float band = frac(v + wobble);
float streak = smoothstep(0.35, 0.6, band) * (1.0 - smoothstep(0.75, 0.98, band));
float breakup = 0.5 + 0.5 * sin(UV.x * 41.0 + v * 9.0) * sin(UV.x * 17.0 - v * 5.0);
return saturate(streak * (0.55 + 0.45 * breakup));
"""


def main():
    mel = unreal.MaterialEditingLibrary
    m = sb_ue.reset_material("M_SB_DeckWater", MASTER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    m.set_editor_property("two_sided", True)
    tint = sb_ue.vector_param(m, "Tint", (0.05, 0.26, 0.28, 1), -900, -200, "Water")
    foam_color = sb_ue.vector_param(m, "FoamColor", (0.82, 0.9, 0.92, 1), -900, -50, "Water")
    opacity = sb_ue.scalar_param(m, "Opacity", 0.55, -900, 100, "Water")
    foam_opacity = sb_ue.scalar_param(m, "FoamOpacity", 0.9, -900, 200, "Water")
    amount = sb_ue.scalar_param(m, "FoamAmount", 0.1, -900, 300, "Water")
    bands = sb_ue.scalar_param(m, "Bands", 3.0, -900, 400, "Water")
    speed = sb_ue.scalar_param(m, "Speed", 1.2, -900, 500, "Water")
    rough = sb_ue.scalar_param(m, "Roughness", 0.06, -900, 600, "Water")
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 700)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -900, 800)
    foam = sb_ue.expr(m, unreal.MaterialExpressionCustom, -600, 400)
    foam.set_editor_property("code", FOAM_HLSL)
    foam.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    foam.set_editor_property("description", "WhiteWater")
    inputs = []
    for name in ("UV", "Time", "Bands", "Speed"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    foam.set_editor_property("inputs", inputs)
    sb_ue.connect(uv, "", foam, "UV")
    sb_ue.connect(time, "", foam, "Time")
    sb_ue.connect(bands, "", foam, "Bands")
    sb_ue.connect(speed, "", foam, "Speed")
    f = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -400, 350)
    sb_ue.connect(foam, "", f, "A")
    sb_ue.connect(amount, "", f, "B")
    col = sb_ue.expr(m, unreal.MaterialExpressionLinearInterpolate, -200, -150)
    sb_ue.connect(tint, "", col, "A")
    sb_ue.connect(foam_color, "", col, "B")
    sb_ue.connect(f, "", col, "Alpha")
    op = sb_ue.expr(m, unreal.MaterialExpressionLinearInterpolate, -200, 150)
    sb_ue.connect(opacity, "", op, "A")
    sb_ue.connect(foam_opacity, "", op, "B")
    sb_ue.connect(f, "", op, "Alpha")
    sb_ue.to_prop(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(op, "", unreal.MaterialProperty.MP_OPACITY)
    sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = sb_ue.scalar_param(m, "Specular", 0.6, -900, 900, "Water")
    sb_ue.to_prop(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    sb_ue.make_mi("MI_DeckWater", MI, m, scalars={"FoamAmount": 0.15, "Opacity": 0.6, "Bands": 1.5, "Speed": 0.25})
    sb_ue.make_mi("MI_WaterSpurt", MI, m, scalars={"FoamAmount": 1.0, "Opacity": 0.5, "FoamOpacity": 0.95, "Bands": 4.0, "Speed": 3.0})
    # The spout's own look on its mesh (imported with the deck water's slot): the mesh is what the game loads, so the cook
    # follows this reference to the instance.
    spurt_path = "/Game/Saltbound/Props/Repair/SM_Water_Spurt"
    if unreal.EditorAssetLibrary.does_asset_exist(spurt_path):
        mesh = unreal.load_asset(spurt_path)
        mesh.set_material(0, unreal.load_asset(MI + "/MI_WaterSpurt"))
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    print("SB_REPAIR_MATERIALS_OK")


if __name__ == "__main__":
    main()
