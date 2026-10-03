"""Creates the ocean rendering assets (script-owned; safe to re-run):

  /Game/Saltbound/Water/MPC_Ocean   wave parameters written by ASaltOcean every frame, and the water look
                                    (Look*) written by ASaltLightingRig (region + sea state, D25)
  /Game/Saltbound/Water/M_Ocean     Single Layer Water master: CDLOD morph + Gerstner WPO (vertex, with
                                    previous-frame phases for motion vectors), analytic normals, ripples,
                                    specular anti-aliasing, foam with history and crest light (pixel);
                                    HLSL from Saltbound/Shaders/SaltOcean.ush
  /Game/Saltbound/Water/MI_Ocean    instance (all tuning parameters, see Docs/Decisions.md D23-D26)
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
PHASE_PARAMS = ["Phase0123", "Phase4567", "Phase891011"]
PREV_PARAMS = ["PrevPhase0123", "PrevPhase4567", "PrevPhase891011"]
FIELD_PARAMS = PHASE_PARAMS + ["Calm0", "Calm1", "Calm2", "Calm3", "CalmMin", "Chop", "Globals"]
LOOK_PARAMS = ["LookAbsorption", "LookScattering", "LookCrest", "LookFoam", "LookSea"]
PS_EXTRA = ["RippleOffsets", "LookCrest", "LookFoam", "LookSea"]

# MPC defaults = Shores of Plenty, Rough (the rig overwrites them at runtime and in the editor).
MPC_DEFAULTS = {
    "Calm0": (1.0e9, 1.0e9, 0, 1), "Calm1": (1.0e9, 1.0e9, 0, 1), "Calm2": (1.0e9, 1.0e9, 0, 1), "Calm3": (1.0e9, 1.0e9, 0, 1),
    "CalmMin": (1, 1, 1, 1), "Chop": (1, 0, 3, 250),
    "LookAbsorption": (1.3, 0.3, 0.22, 1), "LookScattering": (0.008, 0.05, 0.07, 1),
    "LookCrest": (0.15, 0.95, 0.85, 1), "LookFoam": (0.85, 0.92, 0.92, 1), "LookSea": (0.6, 2.2, 1.0, 0.6),
}


# --- MPC -----------------------------------------------------------------------------------
def make_mpc():
    mpc = sb_ue.get_or_create_asset("MPC_Ocean", DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    names = WAVE_PARAMS + PHASE_PARAMS + PREV_PARAMS + ["Calm0", "Calm1", "Calm2", "Calm3", "CalmMin", "Chop", "Globals", "RippleOffsets"] + LOOK_PARAMS
    params = []
    for name in names:
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", unreal.LinearColor(*MPC_DEFAULTS.get(name, (0, 0, 0, 0))))
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
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

INIT = """FSaltOcean O;
O.Init(W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10, W11, Phase0123, Phase4567, Phase891011,
       Calm0, Calm1, Calm2, Calm3, CalmMin, Chop, Globals.x);
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
float2 XY = P0.xy;                                   // Lagrangian coordinate: textures ride with the water
float FadeK = Dist * FadeScale;
float3 GN; float J;
O.NormalAndJacobian(XY, FadeK, GN, J);               // wave (geometric) normal and horizontal compression

float SwellH = max(O.Chop.w, 1.0);                   // summed swell amplitude (cm) for this sea state
float CrestH = saturate((Height / SwellH - CrestStart) / max(CrestRange, 0.01));
float ChopF = O.ChopFactor(XY);
float2 WDir = normalize(O.WA[0].xy + float2(1e-6, 0.0));
float2 WPerp = float2(-WDir.y, WDir.x);

// --- Ripples: three wind-driven layers of one flowing normal map, each in its own rotated frame (the
// tiling never lines up) and travelling across the swell (ocean clock, RippleOffsets). Layer 1 (9 m) and
// 3 (1.5 m) are near/mid detail, layer 2 (38 m) carries far shading. Weaker in calm patches and troughs.
float RippleFade = saturate(1.0 - Dist / max(RippleFadeDistance, 1.0));
float MidFade = saturate(1.0 - Dist / max(RippleFadeDistance * 0.35, 1.0));
float NearFade = saturate(1.0 - Dist / max(RippleFadeDistance * 0.12, 1.0));
float2 A1 = WDir;
float2 A2 = float2(0.766 * WDir.x - 0.643 * WDir.y, 0.643 * WDir.x + 0.766 * WDir.y);
float2 A3 = float2(0.906 * WDir.x + 0.423 * WDir.y, -0.423 * WDir.x + 0.906 * WDir.y);
float2 B1 = float2(-A1.y, A1.x);
float2 B2 = float2(-A2.y, A2.x);
float2 B3 = float2(-A3.y, A3.x);
float2 S1 = Texture2DSample(RippleTex, RippleTexSampler, float2(dot(XY, A1), dot(XY, B1)) / RippleTile1 - float2(RippleOffsets.x, 0.0)).xy * 2.0 - 1.0;
float2 S2 = Texture2DSample(RippleTex, RippleTexSampler, float2(dot(XY, A2), dot(XY, B2)) / RippleTile2 + 0.37 - float2(RippleOffsets.y, 0.0)).xy * 2.0 - 1.0;
float2 S3 = Texture2DSample(RippleTex, RippleTexSampler, float2(dot(XY, A3), dot(XY, B3)) / RippleTile3 + 0.61 - float2(RippleOffsets.z, 0.0)).xy * 2.0 - 1.0;
float2 RippleSlope = (S1.x * A1 + S1.y * B1) * (Ripple1Weight * MidFade)
                   + (S2.x * A2 + S2.y * B2) * (Ripple2Weight * RippleFade)
                   + (S3.x * A3 + S3.y * B3) * (Ripple3Weight * NearFade);
RippleSlope *= RippleStrength * LookSea.z * lerp(0.55, 1.0, ChopF) * lerp(0.65, 1.1, saturate(Height / SwellH + 0.5));
// Slopes add in height-field form, so ripples keep their strength on steep wave faces and the shading
// normal stays consistent with the displaced surface (GN is the exact normal of the displacement).
float3 N = normalize(float3(GN.xy / max(GN.z, 0.15) + RippleSlope, 1.0));

// --- Roughness with specular anti-aliasing: normal variation inside a pixel widens the highlight
// (Kaplanyan / Tokuyoshi). Sub-pixel ripples then read as a soft sheen instead of grainy, flickering
// sparkle; broken glints stay where the normal is coherent across pixels.
float3 dNx = ddx(N);
float3 dNy = ddy(N);
float Variance = SpecAAVariance * (dot(dNx, dNx) + dot(dNy, dNy));
float BaseRough = lerp(WaterRoughness, FarRoughness, saturate(Dist / max(FarRoughnessDistance, 1.0)));
float Alpha2 = BaseRough * BaseRough * BaseRough * BaseRough + min(Variance, SpecAAMax);
float Rough = sqrt(sqrt(Alpha2));

// --- Foam. Lace texture along the wind, drifting slowly with it.
//   R graded lace, G bubble holes, B patchiness. Coverage erodes the lace: thin coverage leaves fragments.
float2 FXY = float2(dot(XY, WDir), dot(XY, WPerp));
float2 Distort = RippleSlope * 0.5;
float3 FA = Texture2DSample(FoamTex, FoamTexSampler, FXY / FoamTile - float2(RippleOffsets.w, 0.0) + Distort).rgb;
float3 FB = Texture2DSample(FoamTex, FoamTexSampler, FXY / (FoamTile * 2.9) + float2(0.37, 0.61) + Distort * 0.5).rgb;
float Lace = max(FA.r, FB.r * 0.85);
// Whitecaps with history: the same water (Lagrangian XY) is checked now and FoamAge1/2 seconds ago. Foam
// forms where a crest breaks (strong compression) and stays with the water while the crest runs on, so
// it trails down the back of the wave, thins out and fades. Sea state sets the breaking threshold.
float WStart = LookSea.x;
float Break0 = saturate((WStart - J) / max(FoamJRange, 1e-3)) * saturate(CrestH * 2.5 + 0.1);
float Break1 = saturate((WStart - O.JacobianAt(XY, FadeK, -FoamAge1)) / max(FoamJRange, 1e-3));
float Break2 = saturate((WStart - O.JacobianAt(XY, FadeK, -FoamAge2)) / max(FoamJRange, 1e-3));
float Caps = max(Break0, max(Break1 * FoamDecay1, Break2 * FoamDecay2));
// Only some crests break: a slow patch mask keeps much of the sea clear.
float Breaking = saturate(FB.b * LookSea.y - (LookSea.y - 1.0) * 0.45);
Caps *= Breaking * LookFoam.a;
// Shore: soft contact foam plus bands that lap outwards from the contact line.
float Depth = max(WaterDepth, 0.0);
float Shore = saturate(1.0 - Depth / max(ShoreFoamDepth * (0.45 + 1.1 * FB.b), 1.0));
float Bands = 0.5 + 0.5 * sin(Depth / max(ShoreBandSpacing, 1.0) * 6.2831853 - Globals.z * ShoreBandSpeed + FB.b * 3.0);
float ShoreCover = pow(Shore, 1.5) * 0.5 + Shore * Bands * 0.3;
float Cover = saturate(max(Caps, ShoreCover));
float Main = smoothstep(1.0 - Cover, 1.0 - Cover + FoamSoftness, Lace);
Main *= 1.0 - FA.g * saturate(Cover * 1.5 - 0.3) * 0.6;
Main *= lerp(0.45, 0.92, Cover);
// A few faint marbled streaks on the open sea, only where waves have been breaking (sea state).
float MarbleFade = saturate(1.0 - Dist / max(MarbleFadeDistance, 1.0));
float MarbleCover = saturate(FB.b * 1.4 - 0.4) * MarbleAmount * MarbleFade * LookFoam.a;
float Marble = smoothstep(1.0 - MarbleCover, 1.0 - MarbleCover + FoamSoftness, FB.r) * MarbleOpacity;
float Foam = saturate(max(Main, Marble));
FoamOut = Foam;
FoamColorOut = LookFoam.rgb;
RoughOut = lerp(Rough, 0.7, Foam);

// --- Light through thin wave tops: only where the wave is high and steep (thin), strongest when the sun
// is behind the wave and we look into its face; by the sun's brightness, not its colour (the light comes
// out filtered by the water). Never a flat tint on every high point; far crests only hint at it.
float3 V = normalize(CamPos - WorldPos);
float3 L = normalize(SunDir);
float Thin = saturate(CrestH * CrestH * saturate(1.0 - GN.z) * ThinScale);
float Forward = pow(saturate(dot(-V, L) * 0.6 + 0.4), SSSBacklitPower);
float Facing = saturate(dot(GN, V) * 1.5);
float Glow = Thin * Forward * (0.35 + 0.65 * Facing) + SSSAmbient * CrestH * CrestH * Facing;
Glow *= lerp(1.0, 0.3, saturate(Dist / max(SSSFarDistance, 1.0)));
float SunLum = dot(SunIllum, float3(0.2126, 0.7152, 0.0722));
EmissiveOut = LookCrest.rgb * SunLum * (Glow * SSSStrength * LookCrest.a * (1.0 - Foam));
return N;
"""

SCALARS = {
    # ripples
    "RippleTile1": (900.0, "Ripples"), "RippleTile2": (3800.0, "Ripples"), "RippleTile3": (150.0, "Ripples"),
    "Ripple1Weight": (1.0, "Ripples"), "Ripple2Weight": (0.8, "Ripples"), "Ripple3Weight": (1.0, "Ripples"),
    "RippleStrength": (0.2, "Ripples"), "RippleFadeDistance": (25000.0, "Ripples"),
    # surface
    "WaterRoughness": (0.03, "Surface"), "FarRoughness": (0.1, "Surface"), "FarRoughnessDistance": (30000.0, "Surface"),
    "SpecAAVariance": (0.03, "Surface"), "SpecAAMax": (0.01, "Surface"),
    # foam
    "FoamTile": (900.0, "Foam"), "FoamJRange": (0.3, "Foam"), "FoamSoftness": (0.12, "Foam"),
    "FoamAge1": (0.9, "Foam"), "FoamAge2": (2.2, "Foam"), "FoamDecay1": (0.75, "Foam"), "FoamDecay2": (0.45, "Foam"),
    "ShoreFoamDepth": (90.0, "Foam"), "ShoreBandSpacing": (22.0, "Foam"), "ShoreBandSpeed": (1.6, "Foam"),
    "MarbleAmount": (0.16, "Foam"), "MarbleOpacity": (0.35, "Foam"), "MarbleFadeDistance": (7000.0, "Foam"),
    # crest light (CrestStart / CrestRange are fractions of the swell height)
    "SSSStrength": (0.11, "Crest light"), "CrestStart": (0.2, "Crest light"), "CrestRange": (0.55, "Crest light"),
    "SSSBacklitPower": (2.5, "Crest light"), "SSSAmbient": (0.02, "Crest light"), "ThinScale": (6.0, "Crest light"),
    "SSSFarDistance": (12000.0, "Crest light"),
}


def make_material(mpc, ush):
    m = sb_ue.reset_material("M_Ocean", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    names = WAVE_PARAMS + FIELD_PARAMS + ["RippleOffsets"] + LOOK_PARAMS
    nodes = {n: mpc_param(m, mpc, n, -2400, -1600 + i * 60) for i, n in enumerate(names)}
    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -2400, 0)

    # --- vertex: morph + displacement
    wp_vs = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -2000, 200,
                       world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -2000, 300)
    cd = [sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -2000, 400 + i * 80, data_index=i) for i in range(3)]
    fade_vs = sb_ue.scalar_param(m, "WaveFadeVS", 0.03, -2000, 650, "Waves")
    vs_inputs = ["WorldPos", "LocalPos", "CamPos", "CellSize", "MorphStart", "MorphEnd", "FadeScale"] + WAVE_PARAMS + FIELD_PARAMS
    vs = custom(m, -1500, 300, "SaltOcean VS (morph + Gerstner)", ush + "\n" + VS_CODE, vs_inputs, F3, [("P0", F3)])
    link(wp_vs, "", vs, "WorldPos")
    link(lp, "", vs, "LocalPos")
    link(cam, "", vs, "CamPos")
    link(cd[0], "", vs, "CellSize")
    link(cd[1], "", vs, "MorphStart")
    link(cd[2], "", vs, "MorphEnd")
    link(fade_vs, "", vs, "FadeScale")
    # Last frame's displacement for the velocity pass: PreviousFrameSwitch picks PrevPhase*. Without it
    # the previous position is evaluated with this frame's phases, the waves get zero motion vectors and
    # TSR reprojects history onto moving water (D23). UsePrevFramePhases = 0 restores the old behaviour.
    use_prev = sb_ue.scalar_param(m, "UsePrevFramePhases", 1.0, -2400, 1500, "Diagnostics")
    prev_nodes = {n: mpc_param(m, mpc, n, -2400, 1600 + i * 60) for i, n in enumerate(PREV_PARAMS)}
    for i, n in enumerate(PHASE_PARAMS):
        lerp_node = sb_ue.expr(m, unreal.MaterialExpressionLinearInterpolate, -2150, 1600 + i * 120)
        link(nodes[n], "", lerp_node, "A")
        link(prev_nodes[PREV_PARAMS[i]], "", lerp_node, "B")
        link(use_prev, "", lerp_node, "Alpha")
        sw = sb_ue.expr(m, unreal.MaterialExpressionPreviousFrameSwitch, -1950, 1600 + i * 120)
        link(nodes[n], "", sw, "Current Frame")
        link(lerp_node, "", sw, "Previous Frame")
        link(sw, "", vs, n)
    for n in WAVE_PARAMS + FIELD_PARAMS:
        if n not in PHASE_PARAMS:
            link(nodes[n], "", vs, n)
    sb_ue.to_prop(vs, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    interp = sb_ue.expr(m, unreal.MaterialExpressionVertexInterpolator, -1200, 500)
    link(vs, "P0", interp, "")

    # --- pixel: normals, ripples, specular AA, foam, crest light
    wp_ps = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1200, 700)
    fade_ps = sb_ue.scalar_param(m, "WaveFadePS", 0.002, -1200, 800, "Waves")
    ripple_tex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 900, parameter_name="RippleTex",
                            texture=unreal.load_asset(DIR + "/Textures/T_OceanRipples_N"),
                            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, group="Ripples")
    foam_tex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 1050, parameter_name="FoamTex",
                          texture=unreal.load_asset(DIR + "/Textures/T_OceanFoam_M"),
                          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, group="Foam")
    sp = {n: sb_ue.scalar_param(m, n, v, -1200, 1200 + i * 60, g) for i, (n, (v, g)) in enumerate(SCALARS.items())}
    scene_depth = sb_ue.expr(m, unreal.MaterialExpressionSceneDepthWithoutWater, -1500, 1800)
    pixel_depth = sb_ue.expr(m, unreal.MaterialExpressionPixelDepth, -1500, 1900)
    depth = sb_ue.expr(m, unreal.MaterialExpressionSubtract, -1300, 1850)
    link(scene_depth, "", depth, "A")
    link(pixel_depth, "", depth, "B")
    sun_dir = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, -1500, 2000)
    sun_illum = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, -1500, 2100)

    ps_inputs = (["P0", "WorldPos", "CamPos", "FadeScale", "RippleTex", "FoamTex", "WaterDepth", "SunDir", "SunIllum"]
                 + list(SCALARS) + WAVE_PARAMS + FIELD_PARAMS + PS_EXTRA)
    ps = custom(m, -700, 900, "SaltOcean PS (normal, ripples, specular AA, foam, crest light)", ush + "\n" + PS_CODE, ps_inputs, F3,
                [("FoamOut", F1), ("FoamColorOut", F3), ("RoughOut", F1), ("EmissiveOut", F3)])
    link(sun_dir, "", ps, "SunDir")
    link(sun_illum, "", ps, "SunIllum")
    link(interp, "", ps, "P0")
    link(wp_ps, "", ps, "WorldPos")
    link(cam, "", ps, "CamPos")
    link(fade_ps, "", ps, "FadeScale")
    link(ripple_tex, "", ps, "RippleTex")
    link(foam_tex, "", ps, "FoamTex")
    link(depth, "", ps, "WaterDepth")
    for n in SCALARS:
        link(sp[n], "", ps, n)
    for n in WAVE_PARAMS + FIELD_PARAMS + PS_EXTRA:
        link(nodes[n], "", ps, n)

    sb_ue.to_prop(ps, "", unreal.MaterialProperty.MP_NORMAL)
    sb_ue.to_prop(ps, "FoamOut", unreal.MaterialProperty.MP_OPACITY)
    sb_ue.to_prop(ps, "FoamColorOut", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(ps, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(ps, "EmissiveOut", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    spec = sb_ue.scalar_param(m, "Specular", 0.3, -400, 700, "Surface")
    sb_ue.to_prop(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    # --- water volume (Single Layer Water output): absorption and scattering per region (MPC Look*).
    slw = sb_ue.expr(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 1200)
    phase_g = sb_ue.scalar_param(m, "PhaseG", 0.2, -400, 1300, "Volume")
    behind = sb_ue.vector_param(m, "ColorScaleBehindWater", (0.9, 0.9, 0.9, 1), -400, 1400, "Volume")
    link(nodes["LookScattering"], "", slw, "ScatteringCoefficients")
    link(nodes["LookAbsorption"], "", slw, "AbsorptionCoefficients")
    link(phase_g, "", slw, "PhaseG")
    link(behind, "", slw, "ColorScaleBehindWater")

    # Reflections on the water move with view-dependent parallax that no surface motion vector describes;
    # medium responsiveness makes TSR reject their history more readily on the water only (D23).
    # Needs r.Velocity.TemporalResponsiveness.Supported=1 (DefaultEngine.ini).
    resp = sb_ue.scalar_param(m, "TemporalResponsiveness", 0.3, -400, 1600, "Diagnostics")
    tr = sb_ue.expr(m, unreal.MaterialExpressionTemporalResponsivenessOutput, 0, 1600)
    link(resp, "", tr, "")

    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


def main():
    with open(USH, encoding="utf-8") as f:
        ush = f.read()
    mpc = make_mpc()
    m = make_material(mpc, ush)
    mi = sb_ue.make_mi("MI_Ocean", DIR, m)
    # Instance overrides from earlier experiments would mask the master defaults: start clean.
    mel.clear_all_material_instance_parameters(mi)
    mel.update_material_instance(mi)
    sb_ue.eal.save_loaded_asset(mi)
    print("SB_OCEAN_ASSETS_OK", m.get_path_name(), mel.get_statistics(m).num_pixel_shader_instructions)


if __name__ == "__main__":
    main()
