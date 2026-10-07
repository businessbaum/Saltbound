"""The Gale's materials (D87; script-owned, safe to re-run). Run before importing the Gale's meshes (their slots take these
instances by name):  python Tools/ue_exec.py Tools/ue_scripts/setup_gale.py

  (cloud(): the first look of the wall, translucent cloud meshes; rejected by the owner, not built, kept for reference)
  M_SB_GaleCloud / MI_Gale_Cloud   the storm clouds (the wall's segments, the breakouts). Translucent and lit in its own code, with
                                   the height fog off: the sea's haze (D49) is dense enough to hide anything 3 km away, and the wall
                                   must stand dark on the horizon from the cove. The sun's light (through the atmosphere, so dusk and
                                   night follow) wraps round the masses, the sky light fills the shadows, creases are darker (vertex
                                   colour: R height, G openness, B per mass), and with distance the colour goes over to the sky behind
                                   it, but never more than HazeMax. A slow boil (world position offset), a silver lining toward the
                                   sun, lightning glowing inside (per-instance custom data 0: the flash 0..1, 1: a seed that moves the
                                   glow), and soft edges where the surface turns away.
  M_SB_GaleRain / MI_Gale_Rain     rain: translucent, unlit, two-sided, in the sky's own colour (darkened for the veils at the wall and
                                   under the breakouts; lit by the sun it glowed orange like shafts of light): falling streaks in two
                                   sizes, curtains of heavier and lighter rain, fading at the top, the bottom and near the camera.
                                   Scalar Rain (0..1) is how much; MI_Gale_RainNear is the rain round the camera (fine, light streaks).
  M_SB_GaleBolt / MI_Gale_Bolt     lightning: unlit, additive; scalar Flash (0..1) set while it strikes.
Everything lives under /Game/Saltbound/Environment/Sky/Gale (always cooked: ASaltGale loads it by path).
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402
import setup_surface_materials as ssm  # noqa: E402

importlib.reload(sb_ue)
mel = unreal.MaterialEditingLibrary
DIR = "/Game/Saltbound/Environment/Sky/Gale"
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
MP = unreal.MaterialProperty

CLOUD_LIGHT = """
// the cloud's own colour: dark under, a dull slate up the heads, creases darker, each mass its own grey, broad mottling
float h = pow(saturate(VC.r), 0.8);
float3 albedo = lerp(Dark, Light, h);
albedo *= lerp(0.35, 1.0, saturate(VC.g));
albedo *= lerp(0.82, 1.18, VC.b);
albedo *= lerp(0.85, 1.12, Noise);
// light: the sun wrapped round the masses (clouds scatter it deep into their shade), the sky filling the rest
float wrap = pow(saturate(dot(N, L) * 0.5 + 0.5), 1.5);
float3 lit = albedo * (Sun * wrap / 3.14159 + Sky * lerp(0.35, 1.0, saturate(VC.g)));
// the silver lining: the rim of a mass seen toward the sun
float fres = pow(1.0 - saturate(dot(N, V)), 4.0);
float sunward = pow(saturate(dot(-V, L)), 6.0);
lit += Lining * Sun / 3.14159 * fres * sunward * saturate(VC.r * 1.4) * saturate(VC.g + 0.3) * 0.05;
// lightning inside: a broad glow round a point the seed moves along the segment, brightest in the middle heights
float3 p = WP / 100.0;
float2 at = float2(sin(Seed * 12.9898), cos(Seed * 78.233)) * 500.0;
float2 q = p.xy - at;
float d = length(q - floor(q / 1300.0 + 0.5) * 1300.0);
float glow = saturate(1.0 - d / 700.0);
float band = saturate(1.0 - abs(VC.r - 0.35) * 2.2);
lit += FlashColour * FlashStrength * Flash * (glow * glow * 0.8 + 0.2) * band * lerp(0.6, 1.0, Noise);
// distance: over to the sky behind, never all the way
float dist = length(WP - Cam) / 100.0;
float haze = HazeMax * (1.0 - exp(-dist / HazeScale));
float3 grey = dot(SkyView, float3(0.3, 0.59, 0.11));
return lerp(lit, lerp(grey, SkyView, 0.4), haze);       // the haze greyer than the sky at the horizon (it was warm at dawn)
"""

CLOUD_OPACITY = """
// soft edges: where the surface turns away from the view it thins out, broken up by the mottling
return saturate((dot(N, V) - EdgeLow) / EdgeWidth + (Noise - 0.5) * 0.5);
"""

CLOUD_WPO = """
// a slow boil: masses swell and settle along their normals (cm)
float t = Time * Speed;
float w = sin(t + dot(WP, float3(0.0003, 0.00021, 0.00037))) + 0.5 * sin(t * 1.7 + dot(WP, float3(-0.00052, 0.00041, 0.0003)));
return N * w * Boil * saturate(VC.r + 0.2);
"""

RAIN_OPACITY = """
float a = 0.0;
for (int k = 0; k < 2; k++)
{
    float cols = Density * (k == 0 ? 1.0 : 2.3);
    float c = floor(UV.x * cols);
    float h = frac(sin(c * 12.9898 + k * 7.13) * 43758.5453);
    float h2 = frac(sin(c * 78.233 + k * 3.7) * 12345.678);
    float v = frac(UV.y * Rep * (0.7 + 0.6 * h2) + Time * Speed * (0.8 + 0.4 * h) + h);
    float s = smoothstep(0.0, 0.06, v) * (1.0 - smoothstep(0.06, 0.45, v));
    s *= 1.0 - abs(frac(UV.x * cols) - 0.5) * 2.0;
    a += s * step(0.3, h2) * (k == 0 ? 0.65 : 0.45);
}
float curtain = 0.55 + 0.45 * sin(UV.x * 10.7 + Time * 0.05) * sin(UV.x * 3.9 + 1.3);
float vert = smoothstep(0.0, 0.06, UV.y) * (1.0 - smoothstep(0.82, 1.0, UV.y));
float nearFade = saturate((Depth - FadeNear) / max(FadeNear, 1.0));
// sheets (the wall's curtains, UV.x 0..4 across) fade at their sides; shafts and the camera's rain wrap round
float sides = lerp(1.0, smoothstep(0.0, 0.6, UV.x) * (1.0 - smoothstep(3.4, 4.0, UV.x)), EdgeFade);
return saturate((Haze + a * Streaks) * curtain * vert * nearFade * sides * Rain);
"""


def try_connect(a, a_out, b, b_in):
    """Optional pins (the atmosphere nodes' position and direction default to the pixel's own): connect when the pin takes it."""
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        print("SB_GALE_NOTE pin %s.%s left at its default" % (b.get_name(), b_in))


def custom(m, x, y, desc, code, ins, out):
    c = ssm.custom(m, x, y, desc, code, list(ins), out)
    for name, node in ins.items():
        sb_ue.connect(node, "", c, name)
    return c


def cloud():
    m = sb_ue.reset_material("M_SB_GaleCloud", DIR)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("use_translucency_vertex_fog", False)       # "Apply Fogging" off: the haze is the shader's own
    m.set_editor_property("used_with_instanced_static_meshes", True)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -1800, -300)
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1800, -200)
    n = sb_ue.expr(m, unreal.MaterialExpressionVertexNormalWS, -1800, -100)
    v = sb_ue.expr(m, unreal.MaterialExpressionCameraVectorWS, -1800, 0)
    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -1800, 80)
    lgt = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, -1800, 160)
    sun = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, -1800, 240)
    try_connect(wp, "", sun, "WorldPosition")
    sky = sb_ue.expr(m, unreal.MaterialExpressionSkyLightEnvMapSample, -1800, 320)
    try_connect(n, "", sky, "Direction")
    sky_rough = sb_ue.expr(m, unreal.MaterialExpressionConstant, -2000, 360, r=1.0)
    try_connect(sky_rough, "", sky, "Roughness")
    view_dir = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -2000, 440, const_b=-1.0)
    sb_ue.connect(v, "", view_dir, "A")
    sky_view = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereViewLuminance, -1800, 440)
    try_connect(view_dir, "", sky_view, "WorldDirection")
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1800, 520)
    flash = sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -1800, 600, data_index=0)
    seed = sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -1800, 680, data_index=1)
    # broad mottling from the common macro noise, 1.5 km a tile
    noise_tex = unreal.load_asset("/Game/Saltbound/Materials/Textures/Common/T_MacroNoise_M")
    uvm = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -1600, 760, r=True, g=True, b=False, a=False)
    sb_ue.connect(wp, "", uvm, "")
    scale = sb_ue.expr(m, unreal.MaterialExpressionDivide, -1450, 760, const_b=150000.0)
    sb_ue.connect(uvm, "", scale, "A")
    ns = sb_ue.expr(m, unreal.MaterialExpressionTextureSample, -1300, 760, texture=noise_tex,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    sb_ue.connect(scale, "", ns, "UVs")
    nsr = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -1150, 760, r=True, g=False, b=False, a=False)
    sb_ue.connect(ns, "", nsr, "")
    p = {k: sb_ue.scalar_param(m, k, val, -1800, 860 + i * 60, "Cloud") for i, (k, val) in enumerate(
        {"FlashStrength": 4000.0, "EdgeLow": 0.02, "EdgeWidth": 0.4, "Boil": 2500.0, "Speed": 0.06, "HazeMax": 0.55, "HazeScale": 9000.0}.items())}
    # storm grey: nearly black under, a dull slate up the heads, hardly any colour (the sun tints it enough)
    cols = {"Dark": (0.008, 0.009, 0.011, 1), "Light": (0.045, 0.048, 0.054, 1), "FlashColour": (0.55, 0.68, 1.0, 1), "Lining": (1.0, 0.92, 0.8, 1)}
    cp = {k: sb_ue.vector_param(m, k, val, -1800, 1300 + i * 80, "Cloud") for i, (k, val) in enumerate(cols.items())}
    light = custom(m, -700, -100, "Gale cloud light", CLOUD_LIGHT,
                   {"VC": vc, "WP": wp, "N": n, "V": v, "L": lgt, "Sun": sun, "Sky": sky, "SkyView": sky_view, "Cam": cam, "Noise": nsr,
                    "Flash": flash, "Seed": seed, "Dark": cp["Dark"], "Light": cp["Light"], "FlashColour": cp["FlashColour"],
                    "FlashStrength": p["FlashStrength"], "Lining": cp["Lining"], "HazeMax": p["HazeMax"], "HazeScale": p["HazeScale"]}, F3)
    opacity = custom(m, -700, 400, "Gale cloud soft edge", CLOUD_OPACITY, {"N": n, "V": v, "Noise": nsr, "EdgeLow": p["EdgeLow"], "EdgeWidth": p["EdgeWidth"]}, F1)
    wpo = custom(m, -700, 650, "Gale cloud boil", CLOUD_WPO, {"VC": vc, "WP": wp, "N": n, "Time": time, "Boil": p["Boil"], "Speed": p["Speed"]}, F3)
    sb_ue.to_prop(light, "", MP.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(opacity, "", MP.MP_OPACITY)
    sb_ue.to_prop(wpo, "", MP.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m, only_if_is_dirty=False)
    sb_ue.make_mi("MI_Gale_Cloud", DIR, m)
    print("SB_GALE_MATERIAL M_SB_GaleCloud")


def rain():
    m = sb_ue.reset_material("M_SB_GaleRain", DIR)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1400, 80)
    depth = sb_ue.expr(m, unreal.MaterialExpressionPixelDepth, -1400, 160)
    p = {k: sb_ue.scalar_param(m, k, val, -1400, 240 + i * 60, "Rain") for i, (k, val) in enumerate(
        {"Rain": 1.0, "Density": 320.0, "Rep": 4.0, "Speed": 0.9, "Haze": 0.5, "FadeNear": 15000.0, "EdgeFade": 0.0, "Streaks": 0.35}.items())}
    o = custom(m, -700, 100, "Gale rain", RAIN_OPACITY, {k: p[k] for k in ("Rain", "Density", "Rep", "Speed", "Haze", "FadeNear", "EdgeFade", "Streaks")}
               | {"UV": uv, "Time": time, "Depth": depth}, F1)
    # the sky's colour in the view direction, darkened for veils (RainColour multiplies it)
    v = sb_ue.expr(m, unreal.MaterialExpressionCameraVectorWS, -1400, -200)
    view_dir = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -1250, -200, const_b=-1.0)
    sb_ue.connect(v, "", view_dir, "A")
    sky_view = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereViewLuminance, -1100, -200)
    try_connect(view_dir, "", sky_view, "WorldDirection")
    mul = sb_ue.vector_param(m, "RainColour", (0.45, 0.48, 0.52, 1), -1100, -100, "Rain")
    grey = custom(m, -950, -250, "Gale rain grey", "float g = dot(S, float3(0.3, 0.59, 0.11)); return lerp(float3(g, g, g), S, 0.25);",
                  {"S": sky_view}, F3)
    col = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -800, -150)
    sb_ue.connect(grey, "", col, "A")
    sb_ue.connect(mul, "", col, "B")
    sb_ue.to_prop(col, "", MP.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(o, "", MP.MP_OPACITY)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m, only_if_is_dirty=False)
    sb_ue.make_mi("MI_Gale_Rain", DIR, m)
    # round the camera: fine, light streaks (the near cylinder is 6 m round the eye)
    sb_ue.make_mi("MI_Gale_RainNear", DIR, m, scalars={"Density": 600.0, "Rep": 24.0, "Speed": 3.5, "Haze": 0.0, "FadeNear": 60.0, "Rain": 0.0,
                                                        "Streaks": 1.0},
                  vectors={"RainColour": (1.15, 1.18, 1.22, 1)})
    print("SB_GALE_MATERIAL M_SB_GaleRain")


def bolt():
    m = sb_ue.reset_material("M_SB_GaleBolt", DIR)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    flash = sb_ue.scalar_param(m, "Flash", 0.0, -900, 0, "Bolt")
    strength = sb_ue.scalar_param(m, "Strength", 80.0, -900, 80, "Bolt")
    colour = sb_ue.vector_param(m, "BoltColour", (0.75, 0.85, 1.0, 1), -900, 160, "Bolt")
    mul = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -600, 0)
    sb_ue.connect(flash, "", mul, "A")
    sb_ue.connect(strength, "", mul, "B")
    mul2 = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -400, 60)
    sb_ue.connect(mul, "", mul2, "A")
    sb_ue.connect(colour, "", mul2, "B")
    sb_ue.to_prop(mul2, "", MP.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m, only_if_is_dirty=False)
    sb_ue.make_mi("MI_Gale_Bolt", DIR, m)
    print("SB_GALE_MATERIAL M_SB_GaleBolt")


def main():
    sb_ue.ensure_folder(DIR)
    rain()
    bolt()
    for name in ("M_SB_GaleRain", "M_SB_GaleBolt"):
        st = mel.get_statistics(unreal.load_asset(DIR + "/" + name))
        print("SB_GALE_STATS", name, st.num_pixel_shader_instructions, st.num_vertex_shader_instructions)
    print("SB_GALE_MATERIALS_OK")


if __name__ == "__main__":
    main()
