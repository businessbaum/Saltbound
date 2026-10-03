"""Creates the ocean rendering assets (script-owned; safe to re-run):

  /Game/Saltbound/Water/MPC_Ocean   swell parameters written by ASaltOcean every frame, FFT cascade frames
                                    (Cascade0-2), and the water look (Look*) written by ASaltLightingRig
                                    (region + sea state, D25)
  /Game/Saltbound/Water/FFT/RT_Ocean{Disp,DispPrev,Deriv,Aux}{0,1,2}
                                    FFT ocean outputs (SaltOceanFFT, written every frame by compute shaders)
  /Game/Saltbound/Water/M_Ocean     Single Layer Water master: CDLOD morph + Gerstner swells + FFT wind sea
                                    (vertex, with previous-frame data for motion vectors); exact normals of the
                                    displaced surface, slope-variance roughness, specular anti-aliasing,
                                    and light through thin crests (pixel); no foam (removed, D27);
                                    HLSL from Saltbound/Shaders/SaltOcean.ush
  /Game/Saltbound/Water/MI_Ocean    instance (all tuning parameters, see Docs/Decisions.md D23-D27)
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
USH = os.path.join(REPO_ROOT, "Saltbound", "Shaders", "SaltOcean.ush")
DIR = "/Game/Saltbound/Water"
FFT_DIR = DIR + "/FFT"
mel = sb_ue.mel

FFT_SIZE = 256
# Must match SaltOceanFFT::GetCascade (SaltOceanSpectrum.cpp): patch size (cm), rotation (deg).
CASCADES = [(25000.0, 0.0), (3700.0, 23.4), (600.0, 51.7)]

WAVE_PARAMS = ["W%d" % i for i in range(12)]
PHASE_PARAMS = ["Phase0123", "Phase4567", "Phase891011"]
PREV_PARAMS = ["PrevPhase0123", "PrevPhase4567", "PrevPhase891011"]
CASCADE_PARAMS = ["Cascade0", "Cascade1", "Cascade2"]
FIELD_PARAMS = PHASE_PARAMS + ["Calm0", "Calm1", "Calm2", "Calm3", "CalmMin", "Chop", "Globals"] + CASCADE_PARAMS
LOOK_PARAMS = ["LookAbsorption", "LookScattering", "LookCrest"]
PS_EXTRA = ["RippleOffsets", "LookCrest"]


def cascade_default(i):
    size, rot = CASCADES[i]
    r = math.radians(rot)
    return (1.0 / size, math.cos(r), math.sin(r), float(FFT_SIZE))


# MPC defaults = Shores of Plenty, Rough (the rig overwrites them at runtime and in the editor).
MPC_DEFAULTS = {
    "Calm0": (1.0e9, 1.0e9, 0, 1), "Calm1": (1.0e9, 1.0e9, 0, 1), "Calm2": (1.0e9, 1.0e9, 0, 1), "Calm3": (1.0e9, 1.0e9, 0, 1),
    "CalmMin": (1, 1, 1, 1), "Chop": (1, 0, 3, 250),
    "Cascade0": cascade_default(0), "Cascade1": cascade_default(1), "Cascade2": cascade_default(2),
    "LookAbsorption": (1.35, 0.26, 0.15, 1), "LookScattering": (0.01, 0.065, 0.1, 1),
    "LookCrest": (0.1, 0.95, 0.9, 1),
}


# --- MPC and render targets -------------------------------------------------------------------
def make_mpc():
    mpc = sb_ue.get_or_create_asset("MPC_Ocean", DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    names = (WAVE_PARAMS + PHASE_PARAMS + PREV_PARAMS + ["Calm0", "Calm1", "Calm2", "Calm3", "CalmMin", "Chop", "Globals", "RippleOffsets"]
             + CASCADE_PARAMS + LOOK_PARAMS)
    params = []
    for name in names:
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", unreal.LinearColor(*MPC_DEFAULTS.get(name, (0, 0, 0, 0))))
        params.append(p)
    mpc.set_editor_property("vector_parameters", params)
    sb_ue.eal.save_loaded_asset(mpc)
    return mpc


def make_targets():
    """FFT outputs: 256^2 RGBA16F with a full mip chain, wrap addressing and UAV access (compute writes)."""
    rts = {}
    for kind in ("Disp", "DispPrev", "Deriv", "Aux"):
        for c in range(3):
            name = "RT_Ocean%s%d" % (kind, c)
            rt = sb_ue.get_or_create_asset(name, FFT_DIR, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
            rt.set_editor_property("size_x", FFT_SIZE)
            rt.set_editor_property("size_y", FFT_SIZE)
            rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA16F)
            rt.set_editor_property("clear_color", unreal.LinearColor(0, 0, 0, 0))
            rt.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
            rt.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
            rt.set_editor_property("auto_generate_mips", True)
            rt.set_editor_property("mips_address_u", unreal.TextureAddress.TA_WRAP)
            rt.set_editor_property("mips_address_v", unreal.TextureAddress.TA_WRAP)
            rt.set_editor_property("supports_uav", True)
            sb_ue.eal.save_loaded_asset(rt)
            rts[name] = rt
    return rts


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
// Swells (Gerstner).
float3 D = O.Displacement(XY, max(Dist * FadeScale, CellSize * 4.0));
// Wind sea: FFT cascades 0 (waves >= 6 m) and 1 (0.9-6 m) shape the geometry; cascade 2 (ripples) is
// shading only. The mip level follows the vertex spacing, so waves the grid cannot carry are filtered
// out instead of aliasing. The velocity pass (IsPrev = 1) reads last frame's textures.
float F = O.Attenuation(XY) * O.ChopFactor(XY);
float2 UV0 = O.TileUV(XY, Cascade0);
float2 UV1 = O.TileUV(XY, Cascade1);
float Mip0 = max(log2(CellSize * Cascade0.x * Cascade0.w) + VSMipBias, 0.0);
float Mip1 = max(log2(CellSize * Cascade1.x * Cascade1.w) + VSMipBias, 0.0);
float4 S0;
float4 S1;
if (IsPrev > 0.5)
{
    S0 = Texture2DSampleLevel(DispPrev0, DispPrev0Sampler, UV0, Mip0);
    S1 = Texture2DSampleLevel(DispPrev1, DispPrev1Sampler, UV1, Mip1);
}
else
{
    S0 = Texture2DSampleLevel(Disp0, Disp0Sampler, UV0, Mip0);
    S1 = Texture2DSampleLevel(Disp1, Disp1Sampler, UV1, Mip1);
}
D += float3(O.ToWorld(S0.xy, Cascade0), S0.z) * F;
// Medium waves follow the short-wave mask (same as the pixel shader: geometry and normals agree).
float2 WDir = normalize(O.WA[0].xy + float2(1e-6, 0.0));
float2 QuietUV = float2(dot(XY, WDir), dot(XY, float2(-WDir.y, WDir.x))) / QuietTile + float2(0.13, 0.71) - float2(RippleOffsets.w * 0.5, 0.0);
float Quiet = Texture2DSampleLevel(QuietTex, QuietTexSampler, QuietUV, 0.0).b;
float Mask = O.ShortWaveMask(D.z / max(O.Chop.w, 1.0), Quiet, TroughCalm, QuietAmount);
D += float3(O.ToWorld(S1.xy, Cascade1), S1.z) * (F * WindSeaVS1 * Mask);
P0 = float3(XY, D.z);
return float3(XY - WorldPos.xy, 0.0) + D;
"""

PS_CODE = INIT + """
float Dist = distance(CamPos, WorldPos);
float Height = P0.z;
float2 XY = P0.xy;                                   // Lagrangian coordinate: all detail rides with the water
float FadeK = Dist * FadeScale;
float F = O.Attenuation(XY) * O.ChopFactor(XY);

// --- Surface tangents dP/dx0, dP/dy0 of the displaced surface: swells (analytic) + FFT cascades. The
// shading normal is their cross product, so it is the normal of the moving geometry at every scale and
// small waves ride on the large ones; the textures are mip-filtered with distance.
float3 Tx = float3(1, 0, 0);
float3 Ty = float3(0, 1, 0);
O.SwellTangents(XY, FadeK, Tx, Ty);
float2 UV0 = O.TileUV(XY, Cascade0);
float2 UV1 = O.TileUV(XY, Cascade1);
float2 UV2 = O.TileUV(XY, Cascade2);
float4 A0 = Texture2DSample(Disp0, Disp0Sampler, UV0);
float4 G0 = Texture2DSample(Deriv0, Deriv0Sampler, UV0);
float4 X0 = Texture2DSample(Aux0, Aux0Sampler, UV0);
float4 A1 = Texture2DSample(Disp1, Disp1Sampler, UV1);
float4 G1 = Texture2DSample(Deriv1, Deriv1Sampler, UV1);
float4 X1 = Texture2DSample(Aux1, Aux1Sampler, UV1);
float4 A2 = Texture2DSample(Disp2, Disp2Sampler, UV2);
float4 G2 = Texture2DSample(Deriv2, Deriv2Sampler, UV2);
float4 X2 = Texture2DSample(Aux2, Aux2Sampler, UV2);
float SlopeVar = 0.0;
// Short-wave mask: medium waves and ripples gather on high water and calm down in troughs and in broad
// quiet patches (same formula as the vertex shader, so the medium waves' normals match their geometry).
float2 WDir = normalize(O.WA[0].xy + float2(1e-6, 0.0));
float2 WPerp = float2(-WDir.y, WDir.x);
float2 QuietUV = float2(dot(XY, WDir), dot(XY, WPerp)) / QuietTile + float2(0.13, 0.71) - float2(RippleOffsets.w * 0.5, 0.0);
float Quiet = Texture2DSampleLevel(QuietTex, QuietTexSampler, QuietUV, 0.0).b;
float Mask = O.ShortWaveMask(Height / max(O.Chop.w, 1.0), Quiet, TroughCalm, QuietAmount);
// Slopes, displacement tensor and sub-pixel slope variance (mip moments: E[s^2] - E[s]^2) per cascade.
{
    float2 H0 = O.ToWorld(G0.xy, Cascade0) * F;
    float3 M0 = O.TensorToWorld(float3(G0.z, G0.w, A0.w), Cascade0) * F;
    Tx += float3(M0.x, M0.z, H0.x);  Ty += float3(M0.z, M0.y, H0.y);
    SlopeVar += max(X0.y + X0.z - dot(G0.xy, G0.xy), 0.0) * F * F * O.MinifyWeight(UV0, Cascade0);
    float F1 = F * Mask;
    float2 H1 = O.ToWorld(G1.xy, Cascade1) * F1;
    float3 M1 = O.TensorToWorld(float3(G1.z, G1.w, A1.w), Cascade1) * F1;
    Tx += float3(M1.x, M1.z, H1.x);  Ty += float3(M1.z, M1.y, H1.y);
    SlopeVar += max(X1.y + X1.z - dot(G1.xy, G1.xy), 0.0) * F1 * F1 * O.MinifyWeight(UV1, Cascade1);
}
float3 Nm = normalize(cross(Tx, Ty));                // wave-scale normal (swells + cascades 0, 1)
float J = Tx.x * Ty.y - Tx.y * Ty.x;                 // horizontal Jacobian (< 1 on compressed crests)
{
    // Ripples keep a floor in troughs and quiet patches: they barely show in the sky reflection, but break
    // the sun reflection into glints instead of a smooth glare.
    float F2 = F * lerp(RippleFloor, 1.0, Mask) * saturate(1.0 - Dist / max(RippleFadeDistance, 1.0));
    float2 H2 = O.ToWorld(G2.xy, Cascade2) * F2;
    float3 M2 = O.TensorToWorld(float3(G2.z, G2.w, A2.w), Cascade2) * F2;
    Tx += float3(M2.x, M2.z, H2.x);  Ty += float3(M2.z, M2.y, H2.y);
    SlopeVar += max(X2.y + X2.z - dot(G2.xy, G2.xy), 0.0) * F2 * F2 * O.MinifyWeight(UV2, Cascade2);
}
float3 N = normalize(cross(Tx, Ty));
N = normalize(float3(N.xy, max(N.z, 0.08)));         // folded crest tips: keep the normal facing up

// --- Roughness: sub-pixel slope variance from the mip moments widens the highlight with distance (sun
// glitter turns into a sheen instead of aliasing), plus specular anti-aliasing for the remaining
// pixel-to-pixel normal change (Kaplanyan / Tokuyoshi).
float3 dNx = ddx(N);
float3 dNy = ddy(N);
float Variance = SpecAAVariance * (dot(dNx, dNx) + dot(dNy, dNy));
float BaseRough = lerp(WaterRoughness, FarRoughness, saturate(Dist / max(FarRoughnessDistance, 1.0)));
float Alpha2 = BaseRough * BaseRough * BaseRough * BaseRough + min(Variance, SpecAAMax) + SlopeVar * SlopeVarScale;
float Rough = sqrt(sqrt(min(Alpha2, MaxAlpha2)));

// --- Reflection horizon occlusion (D26): where the reflected ray points at or below the horizon it would
// hit the next wave; the ocean is not in the ray-tracing scene, so dim the reflectance there.
float3 V = normalize(CamPos - WorldPos);
float3 Rv = reflect(-V, N);
float HorizonT = saturate((HorizonOccTop - Rv.z) / max(HorizonOccTop - HorizonOccBottom, 1e-3));
float HorizonOcc = HorizonT * HorizonT * (3.0 - 2.0 * HorizonT) * HorizonOcclusion;
SpecularOut = Specular * lerp(1.0, HorizonOccFloor, HorizonOcc);

RoughOut = Rough;

// --- Light through thin water: wave tops that are high and steep (thin), lit from behind (the sun shines
// through the crest towards the viewer) or by the sky, scaled by the sun's brightness after the
// atmosphere. Never a flat tint on every high point; troughs and faces keep the deep body colour.
float3 L = normalize(SunDir);
float SeaH = max(O.Chop.w, 1.0);
float CrestH = saturate((Height / SeaH - CrestStart) / max(CrestRange, 0.01));
float Thin = saturate(CrestH * saturate(1.0 - Nm.z) * ThinScale) * saturate(1.5 - J);
float Forward = pow(saturate(dot(-V, L) * 0.6 + 0.4), SSSBacklitPower);
float Facing = saturate(dot(Nm, V) * 1.5);
float Glow = Thin * Forward * (0.35 + 0.65 * Facing) + SSSAmbient * CrestH * CrestH * Facing;
Glow *= lerp(1.0, 0.3, saturate(Dist / max(SSSFarDistance, 1.0)));
float SunLum = dot(SunIllum, float3(0.2126, 0.7152, 0.0722));
EmissiveOut = LookCrest.rgb * SunLum * (Glow * SSSStrength * LookCrest.a);
// Diagnostics: DebugView 1 = magenta where reflections are occluded; 3 = slope variance.
float Dbg = saturate(DebugView);
EmissiveOut = lerp(EmissiveOut, float3(20000.0, 0.0, 20000.0) * HorizonT, Dbg);
EmissiveOut = lerp(EmissiveOut, float3(20000.0, 20000.0, 0.0) * saturate(SlopeVar * 50.0), saturate(DebugView - 2.0));
return N;
"""

SCALARS = {
    # geometry
    "VSMipBias": (0.5, "Waves"), "WindSeaVS1": (1.0, "Waves"), "RippleFadeDistance": (8000.0, "Waves"),
    # short-wave mask: TroughCalm = medium waves / ripples left in troughs, QuietAmount = calmer patches
    "TroughCalm": (0.2, "Waves"), "QuietAmount": (0.55, "Waves"), "QuietTile": (7000.0, "Waves"),
    "RippleFloor": (0.5, "Waves"),
    # surface
    "WaterRoughness": (0.025, "Surface"), "FarRoughness": (0.06, "Surface"), "FarRoughnessDistance": (30000.0, "Surface"),
    "SpecAAVariance": (0.03, "Surface"), "SpecAAMax": (0.001, "Surface"), "SlopeVarScale": (0.25, "Surface"),
    "MaxAlpha2": (0.004, "Surface"),
    "Specular": (0.24, "Surface"), "HorizonOcclusion": (1.0, "Surface"), "HorizonOccTop": (0.08, "Surface"),
    "HorizonOccBottom": (0.0, "Surface"), "HorizonOccFloor": (0.2, "Surface"), "DebugView": (0.0, "Diagnostics"),
    # crest light (CrestStart / CrestRange are fractions of the sea height)
    "SSSStrength": (0.22, "Crest light"), "CrestStart": (0.15, "Crest light"), "CrestRange": (0.55, "Crest light"),
    "SSSBacklitPower": (2.5, "Crest light"), "SSSAmbient": (0.02, "Crest light"), "ThinScale": (6.0, "Crest light"),
    "SSSFarDistance": (12000.0, "Crest light"),
}

VS_TEXTURES = ["Disp0", "Disp1", "DispPrev0", "DispPrev1"]
PS_TEXTURES = ["Disp0", "Disp1", "Disp2", "Deriv0", "Deriv1", "Deriv2", "Aux0", "Aux1", "Aux2"]


def rt_name(param):
    kind, c = param[:-1], param[-1]
    return "RT_Ocean%s%s" % (kind, c)


def make_material(mpc, ush, rts):
    m = sb_ue.reset_material("M_Ocean", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    names = WAVE_PARAMS + FIELD_PARAMS + ["RippleOffsets"] + LOOK_PARAMS
    nodes = {n: mpc_param(m, mpc, n, -2400, -1600 + i * 60) for i, n in enumerate(names)}
    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -2400, 0)

    # FFT textures: one parameter node each, shared by the vertex and pixel code (shared wrap sampler,
    # so the sampler count stays low).
    tex = {}
    for i, p in enumerate(sorted(set(VS_TEXTURES + PS_TEXTURES))):
        tex[p] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -2000, -1600 + i * 90, parameter_name=p,
                            texture=rts[rt_name(p)], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                            sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS, group="FFT")

    # --- vertex: morph + displacement
    wp_vs = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -2000, 200,
                       world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -2000, 300)
    cd = [sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -2000, 400 + i * 80, data_index=i) for i in range(3)]
    fade_vs = sb_ue.scalar_param(m, "WaveFadeVS", 0.03, -2000, 650, "Waves")
    vs_only = ["VSMipBias", "WindSeaVS1"]
    vs_scalars = vs_only + ["TroughCalm", "QuietAmount", "QuietTile"]
    vs_inputs = (["WorldPos", "LocalPos", "CamPos", "CellSize", "MorphStart", "MorphEnd", "FadeScale", "IsPrev", "QuietTex", "RippleOffsets"]
                 + vs_scalars + VS_TEXTURES + WAVE_PARAMS + FIELD_PARAMS)
    # Low-frequency noise (blue channel of the old foam pattern) for the drifting quiet patches.
    quiet_tex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 1050, parameter_name="QuietTex",
                           texture=unreal.load_asset(DIR + "/Textures/T_OceanFoam_M"),
                           sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, group="Waves")
    vs = custom(m, -1500, 300, "SaltOcean VS (morph + swells + FFT wind sea)", ush + "\n" + VS_CODE, vs_inputs, F3, [("P0", F3)])
    link(wp_vs, "", vs, "WorldPos")
    link(lp, "", vs, "LocalPos")
    link(cam, "", vs, "CamPos")
    link(cd[0], "", vs, "CellSize")
    link(cd[1], "", vs, "MorphStart")
    link(cd[2], "", vs, "MorphEnd")
    link(fade_vs, "", vs, "FadeScale")
    link(quiet_tex, "", vs, "QuietTex")
    link(nodes["RippleOffsets"], "", vs, "RippleOffsets")
    for p in VS_TEXTURES:
        link(tex[p], "", vs, p)
    # Last frame's displacement for the velocity pass: PreviousFrameSwitch picks PrevPhase* for the swells
    # and IsPrev = 1 (last frame's FFT textures). Without it the waves get zero motion vectors and TSR
    # reprojects history onto moving water (D23). UsePrevFramePhases = 0 restores that for A/B tests.
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
    zero = sb_ue.expr(m, unreal.MaterialExpressionConstant, -2150, 2000, r=0.0)
    is_prev = sb_ue.expr(m, unreal.MaterialExpressionPreviousFrameSwitch, -1950, 2000)
    link(zero, "", is_prev, "Current Frame")
    link(use_prev, "", is_prev, "Previous Frame")
    link(is_prev, "", vs, "IsPrev")
    for n in WAVE_PARAMS + FIELD_PARAMS:
        if n not in PHASE_PARAMS:
            link(nodes[n], "", vs, n)
    sp = {n: sb_ue.scalar_param(m, n, v, -1200, 1200 + i * 60, g) for i, (n, (v, g)) in enumerate(SCALARS.items())}
    for n in vs_scalars:
        link(sp[n], "", vs, n)
    sb_ue.to_prop(vs, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    interp = sb_ue.expr(m, unreal.MaterialExpressionVertexInterpolator, -1200, 500)
    link(vs, "P0", interp, "")

    # --- pixel: normals, roughness, crest light
    wp_ps = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1200, 700)
    fade_ps = sb_ue.scalar_param(m, "WaveFadePS", 0.002, -1200, 800, "Waves")
    sun_dir = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, -1500, 2000)
    sun_illum = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, -1500, 2100)

    ps_scalars = [n for n in SCALARS if n not in vs_only]
    ps_inputs = (["P0", "WorldPos", "CamPos", "FadeScale", "QuietTex", "SunDir", "SunIllum"]
                 + PS_TEXTURES + ps_scalars + WAVE_PARAMS + FIELD_PARAMS + PS_EXTRA)
    ps = custom(m, -700, 900, "SaltOcean PS (surface normal, roughness, crest light)", ush + "\n" + PS_CODE, ps_inputs, F3,
                [("RoughOut", F1), ("EmissiveOut", F3), ("SpecularOut", F1)])
    link(sun_dir, "", ps, "SunDir")
    link(sun_illum, "", ps, "SunIllum")
    link(interp, "", ps, "P0")
    link(wp_ps, "", ps, "WorldPos")
    link(cam, "", ps, "CamPos")
    link(fade_ps, "", ps, "FadeScale")
    link(quiet_tex, "", ps, "QuietTex")
    for p in PS_TEXTURES:
        link(tex[p], "", ps, p)
    for n in ps_scalars:
        link(sp[n], "", ps, n)
    for n in WAVE_PARAMS + FIELD_PARAMS + PS_EXTRA:
        link(nodes[n], "", ps, n)

    sb_ue.to_prop(ps, "", unreal.MaterialProperty.MP_NORMAL)
    # No surface layer on the water (opacity 0 = pure Single Layer Water).
    zero_opacity = sb_ue.expr(m, unreal.MaterialExpressionConstant, -300, 1100, r=0.0)
    sb_ue.to_prop(zero_opacity, "", unreal.MaterialProperty.MP_OPACITY)
    sb_ue.to_prop(ps, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(ps, "EmissiveOut", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(ps, "SpecularOut", unreal.MaterialProperty.MP_SPECULAR)

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
    rts = make_targets()
    m = make_material(mpc, ush, rts)
    mi = sb_ue.make_mi("MI_Ocean", DIR, m)
    # Instance overrides from earlier experiments would mask the master defaults: start clean.
    mel.clear_all_material_instance_parameters(mi)
    mel.update_material_instance(mi)
    sb_ue.eal.save_loaded_asset(mi)
    print("SB_OCEAN_ASSETS_OK", m.get_path_name(), mel.get_statistics(m).num_pixel_shader_instructions)


if __name__ == "__main__":
    main()
