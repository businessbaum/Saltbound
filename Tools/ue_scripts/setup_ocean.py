"""Creates the ocean rendering assets (script-owned; safe to re-run):

  /Game/Saltbound/Water/MPC_Ocean   wave parameters written by ASaltOcean every frame
  /Game/Saltbound/Water/M_Ocean     Single Layer Water master: CDLOD morph + Gerstner WPO (vertex),
                                    analytic normals, ripples and foam (pixel); HLSL from Saltbound/Shaders/SaltOcean.ush
  /Game/Saltbound/Water/MI_Ocean    default look (cool, clear sea)
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
USH = os.path.join(REPO_ROOT, "Saltbound", "Shaders", "SaltOcean.ush")
DIR = "/Game/Saltbound/Water"
mel = sb_ue.mel

WAVE_PARAMS = ["W%d" % i for i in range(12)]
OTHER_PARAMS = ["Phase0123", "Phase4567", "Phase891011", "Calm0", "Calm1", "Calm2", "Calm3", "CalmMin", "Globals", "RippleOffsets"]


# --- MPC -----------------------------------------------------------------------------------
def make_mpc():
    mpc = sb_ue.get_or_create_asset("MPC_Ocean", DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name in WAVE_PARAMS + OTHER_PARAMS:
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", name)
        default = unreal.LinearColor(0, 0, 0, 0)
        if name.startswith("Calm") and name != "CalmMin":
            default = unreal.LinearColor(1.0e9, 1.0e9, 0, 1)
        if name == "CalmMin":
            default = unreal.LinearColor(1, 1, 1, 1)
        p.set_editor_property("default_value", default)
        params.append(p)
    mpc.set_editor_property("vector_parameters", params)
    sb_ue.eal.save_loaded_asset(mpc)
    return mpc


# --- material helpers -----------------------------------------------------------------------
def custom(m, x, y, desc, code, inputs, out_type, extra_outputs=()):
    c = sb_ue.expr(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("description", desc)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    outs = []
    for n, t in extra_outputs:
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", n)
        co.set_editor_property("output_type", t)
        outs.append(co)
    c.set_editor_property("additional_outputs", outs)
    return c


def mpc_param(m, mpc, name, x, y):
    e = sb_ue.expr(m, unreal.MaterialExpressionCollectionParameter, x, y)
    e.set_editor_property("collection", mpc)
    e.set_editor_property("parameter_name", name)
    return e


def link(a, a_out, b, b_in):
    sb_ue.connect(a, a_out, b, b_in)


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

INIT = """FSaltOcean O;
O.Init(W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, Phase0123, Phase4567, Phase891011,
       Calm0, Calm1, Calm2, Calm3, CalmMin, Globals.x);
"""

VS_CODE = INIT + """
float Dist = distance(CamPos, WorldPos);
float MorphK = saturate((Dist - MorphStart) / max(MorphEnd - MorphStart, 1.0));
// Robust parity of the integer grid coordinate (odd vertices collapse onto the lower even neighbour).
float2 Odd = LocalPos.xy - 2.0 * floor((LocalPos.xy + 0.5) * 0.5);
float2 XY = WorldPos.xy - Odd * (MorphK * CellSize);
// Waves shorter than ~4 grid cells cannot be represented by the mesh: leave them to the pixel normals.
float3 D = O.Displacement(XY, max(Dist * FadeScale, CellSize * 4.0));
P0 = float3(XY, D.z);
return float3(XY - WorldPos.xy, 0.0) + D;
"""

PS_CODE = INIT + """
float Dist = distance(CamPos, WorldPos);
float Height = P0.z;
float2 XY = P0.xy;
float3 GN; float J;
O.NormalAndJacobian(XY, Dist * FadeScale, GN, J);    // wave (geometric) normal and compression

// Frames: the dominant (longest) wave sets the wind direction.
float2 WDir = normalize(O.WA[0].xy + float2(1e-6, 0.0));
float2 WPerp = float2(-WDir.y, WDir.x);
float CrestH = saturate((Height - SSSCrestStart) / max(SSSCrestRange, 1.0));

// Ripples: three wind-driven layers, each in its own rotated frame (tiling never lines up) and travelling
// across the swell at its own speed; slopes are rotated back to world space. The fine layer only near
// the viewer, the mid layer fades early so the distance stays clean instead of grainy.
float RippleFade = saturate(1.0 - Dist / max(RippleFadeDistance, 1.0));
float MidFade = saturate(1.0 - Dist / max(RippleFadeDistance * 0.35, 1.0));
float NearFade = saturate(1.0 - Dist / max(RippleFadeDistance * 0.12, 1.0));
float2 A1 = WDir;
float2 A2 = float2(0.766 * WDir.x - 0.643 * WDir.y, 0.643 * WDir.x + 0.766 * WDir.y);
float2 A3 = float2(0.906 * WDir.x + 0.423 * WDir.y, -0.423 * WDir.x + 0.906 * WDir.y);
float2 B1 = float2(-A1.y, A1.x);
float2 B2 = float2(-A2.y, A2.x);
float2 B3 = float2(-A3.y, A3.x);
float2 S1 = Texture2DSample(RippleTex, RippleTexSampler, float2(dot(XY, A1) - Time * RippleSpeed1, dot(XY, B1)) / RippleTile1).xy * 2.0 - 1.0;
float2 S2 = Texture2DSample(RippleTex, RippleTexSampler, float2(dot(XY, A2) - Time * RippleSpeed2, dot(XY, B2)) / RippleTile2 + 0.37).xy * 2.0 - 1.0;
float2 S3 = Texture2DSample(RippleTex, RippleTexSampler, float2(dot(XY, A3) - Time * RippleSpeed3, dot(XY, B3)) / RippleTile3 + 0.61).xy * 2.0 - 1.0;
float2 RippleSlope = (S1.x * A1 + S1.y * B1) * (RippleStrength * MidFade)
                   + (S2.x * A2 + S2.y * B2) * (RippleStrength * 0.8 * RippleFade)
                   + (S3.x * A3 + S3.y * B3) * (RippleStrength * 0.55 * NearFade);
// Wind ripples are stronger on the exposed faces and crests than down in the troughs.
RippleSlope *= lerp(0.55, 1.25, saturate(Height / 160.0 + 0.5));
float3 N = normalize(GN + float3(RippleSlope, 0.0));

// Foam. Lace texture laid out along the wind, drifting slowly with it (foam stays roughly in place while
// the crests run on, so the down-wave trail sampling leaves streaks behind each breaking crest).
//   R graded lace, G bubble holes, B patchiness. Coverage erodes the lace: thin coverage leaves fragments.
float2 FXY = float2(dot(XY, WDir) - Time * FoamDrift, dot(XY, WPerp));
float2 Distort = RippleSlope * 0.5;
float3 FA = Texture2DSample(FoamTex, FoamTexSampler, FXY / FoamTile + Distort).rgb;
float3 FB = Texture2DSample(FoamTex, FoamTexSampler, FXY / (FoamTile * 2.9) + float2(0.37, 0.61) + Distort * 0.5).rgb;
float Lace = max(FA.r, FB.r * 0.85);
// Only some crests break: a slow patch mask keeps much of the sea clear.
float Breaking = saturate(FB.b * BreakPatchiness - (BreakPatchiness - 1.0) * 0.45);
float3 NT; float JT1; float JT2;
O.NormalAndJacobian(XY + WDir * (FoamTrailLength * 0.45), Dist * FadeScale, NT, JT1);
O.NormalAndJacobian(XY + WDir * FoamTrailLength, Dist * FadeScale, NT, JT2);
float Crest = saturate((FoamJStart - J) / max(FoamJRange, 1e-3)) * saturate(CrestH * 2.5 + 0.15);
float Trail = max(saturate((FoamJStart + 0.06 - JT1) / max(FoamJRange, 1e-3)) * 0.8,
                  saturate((FoamJStart + 0.1 - JT2) / max(FoamJRange, 1e-3)) * 0.45) * FoamTrailStrength;
float Caps = max(Crest, Trail) * Breaking;
// Shore: soft contact foam plus bands that lap outwards from the contact line.
float Depth = max(WaterDepth, 0.0);
float Shore = saturate(1.0 - Depth / max(ShoreFoamDepth * (0.45 + 1.1 * FB.b), 1.0));
float Bands = 0.5 + 0.5 * sin(Depth / max(ShoreBandSpacing, 1.0) * 6.2831853 - Time * ShoreBandSpeed + FB.b * 3.0);
float ShoreCover = pow(Shore, 1.5) * 0.5 + Shore * Bands * 0.3;
float Cover = saturate(max(Caps, ShoreCover));
float Main = smoothstep(1.0 - Cover, 1.0 - Cover + FoamSoftness, Lace);
Main *= 1.0 - FA.g * saturate(Cover * 1.5 - 0.3) * 0.6;
Main *= lerp(0.45, 0.92, Cover);
// A few faint marbled streaks on the open sea (kept sparse: widespread streaks read as a cloudy sheet).
float MarbleFade = saturate(1.0 - Dist / max(MarbleFadeDistance, 1.0));
float MarbleCover = saturate(FB.b * 1.4 - 0.4) * MarbleAmount * MarbleFade;
float Marble = smoothstep(1.0 - MarbleCover, 1.0 - MarbleCover + FoamSoftness, FB.r) * MarbleOpacity;
float Foam = saturate(max(Main, Marble));
FoamOut = Foam;
// Glossy near the viewer (crisp, broken sun glints on the ripple facets), slightly rougher far away.
float BaseRough = lerp(WaterRoughness, FarRoughness, saturate(Dist / max(FarRoughnessDistance, 1.0)));
RoughOut = lerp(BaseRough, 0.7, Foam);

// Light through thin wave tops: only where the wave is both high and steep (thin), strongest when the
// sun is behind the wave and we look into its face. Never a flat tint on every high point.
float3 V = normalize(CamPos - WorldPos);
float3 L = normalize(SunDir);
float Thin = saturate(CrestH * CrestH * saturate(1.0 - GN.z) * ThinScale);
float Forward = pow(saturate(dot(-V, L) * 0.6 + 0.4), SSSBacklitPower);
float Facing = saturate(dot(GN, V) * 1.5);
float Glow = Thin * Forward * (0.35 + 0.65 * Facing) + SSSAmbient * CrestH * CrestH * Facing;
EmissiveOut = SSSColor * SunIllum * (Glow * SSSStrength * (1.0 - Foam));
return N;
"""


def make_material(mpc, ush):
    m = sb_ue.reset_material("M_Ocean", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    # Shared inputs
    wave_nodes = {n: mpc_param(m, mpc, n, -2400, -1200 + i * 60) for i, n in enumerate(WAVE_PARAMS + OTHER_PARAMS)}
    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -2400, 0)

    # --- vertex: morph + displacement
    wp_vs = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -2000, 200,
                       world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -2000, 300)
    cd = [sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -2000, 400 + i * 80, data_index=i) for i in range(3)]
    fade_vs = sb_ue.scalar_param(m, "WaveFadeVS", 0.03, -2000, 650, "Waves")
    vs_inputs = ["WorldPos", "LocalPos", "CamPos", "CellSize", "MorphStart", "MorphEnd", "FadeScale"] + WAVE_PARAMS + OTHER_PARAMS[:-1]
    vs = custom(m, -1500, 300, "SaltOcean VS (morph + Gerstner)", ush + "\n" + VS_CODE, vs_inputs, F3, [("P0", F3)])
    link(wp_vs, "", vs, "WorldPos")
    link(lp, "", vs, "LocalPos")
    link(cam, "", vs, "CamPos")
    link(cd[0], "", vs, "CellSize")
    link(cd[1], "", vs, "MorphStart")
    link(cd[2], "", vs, "MorphEnd")
    link(fade_vs, "", vs, "FadeScale")
    for n in WAVE_PARAMS + OTHER_PARAMS[:-1]:
        link(wave_nodes[n], "", vs, n)
    sb_ue.to_prop(vs, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    interp = sb_ue.expr(m, unreal.MaterialExpressionVertexInterpolator, -1200, 500)
    link(vs, "P0", interp, "")

    # --- pixel: normals, ripples, foam
    wp_ps = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1200, 700)
    fade_ps = sb_ue.scalar_param(m, "WaveFadePS", 0.002, -1200, 800, "Waves")
    ripple_tex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 900, parameter_name="RippleTex",
                            texture=unreal.load_asset(DIR + "/Textures/T_OceanRipples_N"),
                            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, group="Ripples")
    foam_tex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 1050, parameter_name="FoamTex",
                          texture=unreal.load_asset(DIR + "/Textures/T_OceanFoam_M"),
                          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, group="Foam")
    scalars = {
        "RippleTile1": (900.0, "Ripples"), "RippleTile2": (3800.0, "Ripples"), "RippleTile3": (240.0, "Ripples"),
        "RippleSpeed1": (90.0, "Ripples"), "RippleSpeed2": (160.0, "Ripples"), "RippleSpeed3": (45.0, "Ripples"),
        "RippleStrength": (0.2, "Ripples"), "RippleFadeDistance": (25000.0, "Ripples"),
        "FoamTile": (900.0, "Foam"), "FoamJStart": (0.62, "Foam"), "FoamJRange": (0.3, "Foam"),
        "FoamSoftness": (0.12, "Foam"), "FoamTrailLength": (650.0, "Foam"), "FoamTrailStrength": (0.85, "Foam"),
        "FoamDrift": (35.0, "Foam"), "BreakPatchiness": (2.2, "Foam"),
        "ShoreFoamDepth": (90.0, "Foam"), "ShoreBandSpacing": (22.0, "Foam"), "ShoreBandSpeed": (1.6, "Foam"),
        "MarbleAmount": (0.16, "Foam"), "MarbleOpacity": (0.35, "Foam"), "MarbleFadeDistance": (7000.0, "Foam"),
        "WaterRoughness": (0.03, "Surface"), "FarRoughness": (0.1, "Surface"), "FarRoughnessDistance": (30000.0, "Surface"),
        "SSSStrength": (0.11, "Crest light"), "SSSCrestStart": (25.0, "Crest light"), "SSSCrestRange": (160.0, "Crest light"),
        "SSSBacklitPower": (2.5, "Crest light"), "SSSAmbient": (0.12, "Crest light"), "ThinScale": (6.0, "Crest light"),
    }
    sp = {n: sb_ue.scalar_param(m, n, v, -1200, 1200 + i * 60, g) for i, (n, (v, g)) in enumerate(scalars.items())}
    scene_depth = sb_ue.expr(m, unreal.MaterialExpressionSceneDepthWithoutWater, -1500, 1800)
    pixel_depth = sb_ue.expr(m, unreal.MaterialExpressionPixelDepth, -1500, 1900)
    depth = sb_ue.expr(m, unreal.MaterialExpressionSubtract, -1300, 1850)
    link(scene_depth, "", depth, "A")
    link(pixel_depth, "", depth, "B")

    sun_dir = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, -1500, 2000)
    sun_illum = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, -1500, 2100)
    sss_color = sb_ue.vector_param(m, "SSSColor", (0.15, 0.95, 0.85, 1), -1500, 2200, "Crest light")
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1500, 2300)
    ps_inputs = (["P0", "WorldPos", "CamPos", "FadeScale", "RippleTex", "FoamTex", "WaterDepth", "SunDir", "SunIllum", "SSSColor", "Time"]
                 + list(scalars) + WAVE_PARAMS + OTHER_PARAMS)
    ps = custom(m, -700, 900, "SaltOcean PS (normal, ripples, foam, crest light)", ush + "\n" + PS_CODE, ps_inputs, F3,
                [("FoamOut", F1), ("RoughOut", F1), ("EmissiveOut", F3)])
    link(time, "", ps, "Time")
    link(sun_dir, "", ps, "SunDir")
    link(sun_illum, "", ps, "SunIllum")
    link(sss_color, "", ps, "SSSColor")
    link(interp, "", ps, "P0")
    link(wp_ps, "", ps, "WorldPos")
    link(cam, "", ps, "CamPos")
    link(fade_ps, "", ps, "FadeScale")
    link(ripple_tex, "", ps, "RippleTex")
    link(foam_tex, "", ps, "FoamTex")
    link(depth, "", ps, "WaterDepth")
    for n in scalars:
        link(sp[n], "", ps, n)
    for n in WAVE_PARAMS + OTHER_PARAMS:
        link(wave_nodes[n], "", ps, n)

    sb_ue.to_prop(ps, "", unreal.MaterialProperty.MP_NORMAL)
    sb_ue.to_prop(ps, "FoamOut", unreal.MaterialProperty.MP_OPACITY)
    sb_ue.to_prop(ps, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(ps, "EmissiveOut", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    foam_color = sb_ue.vector_param(m, "FoamColor", (0.8, 0.9, 0.9, 1), -400, 600, "Foam")
    sb_ue.to_prop(foam_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    spec = sb_ue.scalar_param(m, "Specular", 0.3, -400, 700, "Surface")
    sb_ue.to_prop(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    # --- water volume (Single Layer Water output)
    slw = sb_ue.expr(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 1200)
    scat = sb_ue.vector_param(m, "ScatteringCoefficients", (0.008, 0.052, 0.078, 1), -400, 1100, "Volume")
    absorb = sb_ue.vector_param(m, "AbsorptionCoefficients", (1.3, 0.27, 0.19, 1), -400, 1200, "Volume")
    phase_g = sb_ue.scalar_param(m, "PhaseG", 0.2, -400, 1300, "Volume")
    behind = sb_ue.vector_param(m, "ColorScaleBehindWater", (0.9, 0.9, 0.9, 1), -400, 1400, "Volume")
    link(scat, "", slw, "ScatteringCoefficients")
    link(absorb, "", slw, "AbsorptionCoefficients")
    link(phase_g, "", slw, "PhaseG")
    link(behind, "", slw, "ColorScaleBehindWater")

    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


def main():
    with open(USH, encoding="utf-8") as f:
        ush = f.read()
    mpc = make_mpc()
    m = make_material(mpc, ush)
    sb_ue.make_mi("MI_Ocean", DIR, m)
    print("SB_OCEAN_ASSETS_OK", m.get_path_name(), mel.get_statistics(m).num_pixel_shader_instructions)


if __name__ == "__main__":
    main()
