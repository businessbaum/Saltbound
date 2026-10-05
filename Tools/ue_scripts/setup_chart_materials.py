"""The chart on the mast (D57): its materials, script-made like the others.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/setup_chart_materials.py

  M_SB_Chart / MI_MastChart  the chart sheet pinned to the board on the mast (a lit surface): a window of the region
                             chart (T_Map_Region, the open sea's 12 km at about 3 m a pixel) round the boat, north up,
                             the boat an ink-rimmed red arrow in its middle along its heading, the sheet's edges browned.
                             ASaltBoat sets CentreU / CentreV (the boat on the region chart) and Heading every frame.
  M_UI_Chart                 the same chart for its close look (USaltChartWidget), as a UI material.
The window: Span (its width as a fraction of the region chart, 1.6 / 12 km), Aspect (the sheet's height / width, 0.33 /
0.26). Keep them in sync with SaltChart in Source/Saltbound/Core/SaltPlaces.h.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

mel = unreal.MaterialEditingLibrary
MASTER = "/Game/Saltbound/Materials/Master"
INSTANCES = "/Game/Saltbound/Materials/Instances"
UI = "/Game/Saltbound/UI/Materials"
CHART = "/Game/Saltbound/UI/Textures/T_Map_Region"

CHART_CODE = """
// The sheet: uv 0..1 (u right, v down). The window round the boat on the region chart, north up.
float2 q = float2(UV.x - 0.5, (UV.y - 0.5) * Aspect);
float2 c = float2(CentreU, CentreV) + q * Span;
float3 paper = pow(float3(222.0, 200.0, 154.0) / 255.0, 2.2);
float inside = step(0.0, c.x) * step(c.x, 1.0) * step(0.0, c.y) * step(c.y, 1.0);
float3 col = lerp(paper, Texture2DSample(Chart, ChartSampler, saturate(c)).rgb, inside);
// Browned edges.
float2 e = min(UV, 1.0 - UV);
float edge = 1.0 - smoothstep(0.0, 0.07, min(e.x, e.y * Aspect));
col *= 1.0 - 0.4 * edge;
// The boat: an arrow along its heading (clockwise from north), dark red with an ink rim.
float s = sin(Heading);
float co = cos(Heading);
float2 p = float2(q.x * co + q.y * s, -q.x * s + q.y * co);
float2 tip = float2(0.0, -0.055);
float2 bl = float2(-0.032, 0.034);
float2 br = float2(0.032, 0.034);
float2 nt = float2(0.0, 0.016);
float a = 0.0;
float rim = 0.0;
for (int k = 0; k < 2; k++)
{
    float g = k == 0 ? 1.28 : 1.0;
    float2 pp = p / g;
    // Two triangles: tip-left-notch and tip-notch-right.
    float e1 = (bl.x - tip.x) * (pp.y - tip.y) - (bl.y - tip.y) * (pp.x - tip.x);
    float e2 = (nt.x - bl.x) * (pp.y - bl.y) - (nt.y - bl.y) * (pp.x - bl.x);
    float e3 = (tip.x - nt.x) * (pp.y - nt.y) - (tip.y - nt.y) * (pp.x - nt.x);
    float f1 = (nt.x - tip.x) * (pp.y - tip.y) - (nt.y - tip.y) * (pp.x - tip.x);
    float f2 = (br.x - nt.x) * (pp.y - nt.y) - (br.y - nt.y) * (pp.x - nt.x);
    float f3 = (tip.x - br.x) * (pp.y - br.y) - (tip.y - br.y) * (pp.x - br.x);
    float in1 = (e1 <= 0.0 && e2 <= 0.0 && e3 <= 0.0) || (e1 >= 0.0 && e2 >= 0.0 && e3 >= 0.0) ? 1.0 : 0.0;
    float in2 = (f1 <= 0.0 && f2 <= 0.0 && f3 <= 0.0) || (f1 >= 0.0 && f2 >= 0.0 && f3 >= 0.0) ? 1.0 : 0.0;
    float hit = max(in1, in2);
    if (k == 0) { rim = hit; } else { a = hit; }
}
col = lerp(col, pow(float3(48.0, 30.0, 16.0) / 255.0, 2.2), rim * 0.9);
col = lerp(col, pow(float3(150.0, 38.0, 24.0) / 255.0, 2.2), a);
return col;
"""


def custom(m, x, y, code, inputs):
    c = sb_ue.expr(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("description", "Chart window round the boat")
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


def chart_nodes(m):
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -900, 100, parameter_name="Chart", texture=unreal.load_asset(CHART))
    params = [sb_ue.scalar_param(m, n, d, -900, 260 + 80 * i, "Chart") for i, (n, d) in enumerate(
        (("CentreU", 0.5), ("CentreV", 0.75), ("Span", 1600.0 / 12000.0), ("Aspect", 0.33 / 0.26), ("Heading", 0.0)))]
    c = custom(m, -450, 200, CHART_CODE, ["UV", "Chart", "CentreU", "CentreV", "Span", "Aspect", "Heading"])
    sb_ue.connect(uv, "", c, "UV")
    sb_ue.connect(tex, "", c, "Chart")
    for node, pin in zip(params, ("CentreU", "CentreV", "Span", "Aspect", "Heading")):
        sb_ue.connect(node, "", c, pin)
    return c


def surface():
    m = sb_ue.reset_material("M_SB_Chart", MASTER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    c = chart_nodes(m)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = sb_ue.expr(m, unreal.MaterialExpressionConstant, -200, 420, r=0.88)
    spec = sb_ue.expr(m, unreal.MaterialExpressionConstant, -200, 500, r=0.3)
    sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    sb_ue.make_mi("MI_MastChart", INSTANCES, m)


def ui():
    m = sb_ue.reset_material("M_UI_Chart", UI)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    c = chart_nodes(m)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -200, 420)
    sb_ue.to_prop(vc, "A", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)


surface()
ui()
print("SB_CHART_MATERIALS_OK")
