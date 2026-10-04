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
  /Game/Saltbound/Water/M_OceanUnder (+ MI_) the sea seen from below (D28): same vertex displacement, drawn by
                                    ASaltOcean's mirrored underside tiles; Snell's window (refracted sky and
                                    sun), total internal reflection of the water body outside it
  /Game/Saltbound/Water/M_UnderwaterParticles (+ MI_) faint drifting specks around a submerged camera (D30),
                                    placed by the vertex shader (ASaltOcean's particle instances carry anchors)
  /Game/Saltbound/Water/M_PP_Underwater (+ MI_) post-process pass for a submerged camera (D28): finds the
                                    surface above the camera on the GPU, then view-distance fog, light lost
                                    with depth and in-scattered light from the region's water (MPC Look*)
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
USH = os.path.join(REPO_ROOT, "Saltbound", "Shaders", "SaltOcean.ush")
UNDER_USH = os.path.join(REPO_ROOT, "Saltbound", "Shaders", "SaltUnderwater.ush")
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
# Underwater look shared by M_OceanUnder and M_PP_Underwater (D28):
#   UnderwaterFog = (view fog scale, in-scatter brightness, light-path scale, sun forward scatter)
#   UnderwaterSky = (sky ambient / sun, sky radiance / sun, sun glint, forward-scatter phase g)
#   UnderwaterColor = (stylised in-scatter colour rgb, share replacing the physical albedo) - turquoise like the
#                     owner's Sea of Thieves references, while extinction still follows the region's water
UNDER_PARAMS = ["UnderwaterFog", "UnderwaterSky", "UnderwaterColor"]
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
    "UnderwaterFog": (0.12, 1.0, 0.3, 0.3), "UnderwaterSky": (0.25, 0.06, 0.3, 0.6), "UnderwaterColor": (0.12, 0.48, 0.62, 1.0),
}


# --- MPC and render targets -------------------------------------------------------------------
def make_mpc():
    mpc = sb_ue.get_or_create_asset("MPC_Ocean", DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    names = (WAVE_PARAMS + PHASE_PARAMS + PREV_PARAMS + ["Calm0", "Calm1", "Calm2", "Calm3", "CalmMin", "Chop", "Globals", "RippleOffsets"]
             + CASCADE_PARAMS + LOOK_PARAMS + UNDER_PARAMS)
    # Keep existing entries: each has an ID that materials reference. Recreated entries get new IDs and every
    # material not rebuilt in the same run fails to compile ("invalid parameter") and renders the default material.
    existing = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property("vector_parameters")}
    params = []
    for name in names:
        p = existing.get(name) or unreal.CollectionVectorParameter()
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
// Swells (Gerstner) plus the wind sea: FFT cascades 0 (waves >= 6 m) and 1 (0.9-6 m) shape the geometry;
// cascade 2 (ripples) is shading only. The mip level follows the vertex spacing, so waves the grid cannot
// carry are filtered out instead of aliasing. The velocity pass (IsPrev = 1) reads last frame's textures.
float FadeK = max(Dist * FadeScale, CellSize * 4.0);
float3 D;
if (IsPrev > 0.5)
{
    D = O.SurfaceDisplacement(XY, FadeK, CellSize, Cascade0, Cascade1, VSMipBias, DispPrev0, DispPrev0Sampler,
        DispPrev1, DispPrev1Sampler, QuietTex, QuietTexSampler, QuietTile, RippleOffsets.w, TroughCalm, QuietAmount, WindSeaVS1);
}
else
{
    D = O.SurfaceDisplacement(XY, FadeK, CellSize, Cascade0, Cascade1, VSMipBias, Disp0, Disp0Sampler,
        Disp1, Disp1Sampler, QuietTex, QuietTexSampler, QuietTile, RippleOffsets.w, TroughCalm, QuietAmount, WindSeaVS1);
}
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
float FarK = saturate(Dist / max(FarRoughnessDistance, 1.0));
float BaseRough = lerp(WaterRoughness, FarRoughness, FarK);
float Alpha2 = BaseRough * BaseRough * BaseRough * BaseRough + min(Variance, SpecAAMax) + SlopeVar * SlopeVarScale;
// Long reflective contours along smooth faces break into segments: broad drifting patches of slightly rougher
// water (D32). No added microdetail; the small sun highlights keep their shape outside the patches.
float ReflPatch = Texture2DSampleLevel(QuietTex, QuietTexSampler, XY / ReflBreakTile + float2(0.41, 0.17), 0.0).b;
Alpha2 *= lerp(1.0, ReflBreak, smoothstep(0.4, 0.8, ReflPatch));
// The cap keeps near highlights crisp (D27); far away a crest is smaller than a pixel, so the cap rises and the
// sun spreads over the pixel instead of sparking in single pixels (owner: "pixel errors at the top of the
// waves in the distance", D31).
float Rough = sqrt(sqrt(min(Alpha2, lerp(MaxAlpha2, FarMaxAlpha2, FarK))));

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
// D32: spread over the upper wave faces (height-driven, a gentle slope term) instead of thin lines around the
// sharpest, most compressed tips.
float Upper = CrestH * CrestH * (3.0 - 2.0 * CrestH);
float Thin = Upper * lerp(0.4, 1.0, saturate((1.0 - Nm.z) * ThinScale));
float Forward = pow(saturate(dot(-V, L) * 0.6 + 0.4), SSSBacklitPower);
float Facing = saturate(dot(Nm, V) * 1.5);
float Glow = Thin * Forward * (0.35 + 0.65 * Facing) + SSSAmbient * CrestH * CrestH * Facing;
Glow *= lerp(1.0, 0.1, saturate(Dist / max(SSSFarDistance, 1.0)));
float SunLum = dot(SunIllum, float3(0.2126, 0.7152, 0.0722));
EmissiveOut = LookCrest.rgb * SunLum * (Glow * SSSStrength * LookCrest.a);
// --- Crest foam (D32, D33). Lagrangian (rides with the water); Single Layer Water surface layer (opacity),
// distinct from sun glints. Calm seas barely compress, so they stay clear.
// Fresh foam: the crest is compressed here now (horizontal Jacobian J < 1), but only on short crest segments
// (a mask that varies along the crest, tapering the ends, clear water between segments).
float2 Along = float2(dot(XY, WDir), dot(XY, WPerp));
float Segments = smoothstep(FoamSegment, FoamSegment + 0.18,
    Texture2DSampleLevel(QuietTex, QuietTexSampler, Along / float2(FoamSegAcross, FoamSegLength) + float2(0.73, 0.29), 0.0).b);
float Fresh = saturate((FoamJStart - J) / max(FoamJStart - FoamJFull, 0.01)) * smoothstep(0.0, 0.5, CrestH + 0.3) * Segments;
// Residue: water a crest passed a few seconds ago (that crest is now a few metres downwind). Older foam is
// thinner, more open and more transparent.
float Residue = 0.0;
[unroll] for (int k = 0; k < 2; k++)
{
    float Behind = (k == 0) ? FoamTrail * 0.4 : FoamTrail;
    float2 P1 = XY + WDir * Behind;
    float J1 = O.JacobianAt(P1, FadeK, F, Mask, Cascade0, Cascade1, Disp0, Disp0Sampler, Deriv0, Deriv0Sampler,
        Disp1, Disp1Sampler, Deriv1, Deriv1Sampler);
    float Seg1 = smoothstep(FoamSegment, FoamSegment + 0.18,
        Texture2DSampleLevel(QuietTex, QuietTexSampler, (Along + float2(Behind, 0.0)) / float2(FoamSegAcross, FoamSegLength) + float2(0.73, 0.29), 0.0).b);
    Residue = max(Residue, saturate((FoamJStart - J1) / max(FoamJStart - FoamJFull, 0.01)) * Seg1 * ((k == 0) ? 0.6 : 0.35));
}
// Internal structure: coverage fills the lace pattern (thin coverage = filaments with large openings, dense =
// filled cells), bubble holes in the dense parts; residue uses a finer, fragmented lace. No uniform bands.
float3 FoamTex = Texture2DSample(QuietTex, QuietTexSampler, Along / FoamTile).rgb;
float FineLace = Texture2DSample(QuietTex, QuietTexSampler, Along / (FoamTile * 0.45) + float2(0.37, 0.53)).r;
float FreshCov = saturate(Fresh * FoamAmount);
float ResidueCov = saturate(Residue * FoamAmount) * (1.0 - FreshCov);
float FreshFoam = smoothstep(1.0 - FreshCov, 1.12 - FreshCov, FoamTex.r) * lerp(0.6, 0.97, FreshCov) * (1.0 - 0.7 * FoamTex.g * FreshCov);
float ResidueFoam = smoothstep(1.0 - ResidueCov, 1.1 - ResidueCov, FineLace * 0.6 + FoamTex.r * 0.4) * 0.45;
float Foam = max(FreshFoam, ResidueFoam) * saturate(1.0 - Dist / max(FoamFadeDistance, 1.0));
FoamOut = Foam;
RoughOut = lerp(Rough, 0.85, Foam);
SpecularOut *= 1.0 - Foam;
EmissiveOut *= 1.0 - Foam;
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
    "WaterRoughness": (0.025, "Surface"), "FarRoughness": (0.08, "Surface"), "FarRoughnessDistance": (15000.0, "Surface"),
    "SpecAAVariance": (0.03, "Surface"), "SpecAAMax": (0.004, "Surface"), "SlopeVarScale": (0.25, "Surface"),
    "MaxAlpha2": (0.004, "Surface"), "FarMaxAlpha2": (0.03, "Surface"),
    "Specular": (0.24, "Surface"), "HorizonOcclusion": (1.0, "Surface"), "HorizonOccTop": (0.08, "Surface"),
    "HorizonOccBottom": (0.0, "Surface"), "HorizonOccFloor": (0.2, "Surface"), "DebugView": (0.0, "Diagnostics"),
    # crest light (CrestStart / CrestRange are fractions of the sea height)
    "SSSStrength": (0.2, "Crest light"), "CrestStart": (0.2, "Crest light"), "CrestRange": (0.75, "Crest light"),
    "SSSBacklitPower": (2.5, "Crest light"), "SSSAmbient": (0.02, "Crest light"), "ThinScale": (2.5, "Crest light"),
    "SSSFarDistance": (8000.0, "Crest light"),
    # long reflection contours (D32)
    # ReflBreak 1 = off (owner A/B); D33: kept subtle, it must not read as drifting cloudy stains
    "ReflBreak": (2.0, "Surface"), "ReflBreakTile": (3000.0, "Surface"),
    # crest foam (D32): Jacobian thresholds, patch threshold and sizes
    "FoamAmount": (1.0, "Foam"), "FoamJStart": (0.75, "Foam"), "FoamJFull": (0.45, "Foam"),
    "FoamSegment": (0.45, "Foam"), "FoamSegLength": (1600.0, "Foam"), "FoamSegAcross": (5000.0, "Foam"),
    "FoamTrail": (700.0, "Foam"), "FoamTile": (900.0, "Foam"), "FoamFadeDistance": (40000.0, "Foam"),
}

VS_TEXTURES = ["Disp0", "Disp1", "DispPrev0", "DispPrev1"]
PS_TEXTURES = ["Disp0", "Disp1", "Disp2", "Deriv0", "Deriv1", "Deriv2", "Aux0", "Aux1", "Aux2"]
UNDER_TEXTURES = ["Disp0", "Disp1", "Disp2", "Deriv0", "Deriv1", "Deriv2"]
VS_ONLY = ["VSMipBias", "WindSeaVS1"]
VS_SCALARS = VS_ONLY + ["TroughCalm", "QuietAmount", "QuietTile"]

# --- the sea seen from below (M_OceanUnder) ---------------------------------------------------------
UNDER_PS_CODE = INIT + """
float Dist = distance(CamPos, WorldPos);
float2 XY = P0.xy;
float FadeK = Dist * FadeScale;
float F = O.Attenuation(XY) * O.ChopFactor(XY);
// Surface normal of the displaced surface, as in M_Ocean (ripples fade with distance).
float3 Tx = float3(1, 0, 0);
float3 Ty = float3(0, 1, 0);
O.SwellTangents(XY, FadeK, Tx, Ty);
float2 UV0 = O.TileUV(XY, Cascade0);
float2 UV1 = O.TileUV(XY, Cascade1);
float2 UV2 = O.TileUV(XY, Cascade2);
float4 A0 = Texture2DSample(Disp0, Disp0Sampler, UV0);
float4 G0 = Texture2DSample(Deriv0, Deriv0Sampler, UV0);
float4 A1 = Texture2DSample(Disp1, Disp1Sampler, UV1);
float4 G1 = Texture2DSample(Deriv1, Deriv1Sampler, UV1);
float4 A2 = Texture2DSample(Disp2, Disp2Sampler, UV2);
float4 G2 = Texture2DSample(Deriv2, Deriv2Sampler, UV2);
float Quiet = Texture2DSampleLevel(QuietTex, QuietTexSampler, O.QuietUV(XY, QuietTile, RippleOffsets.w), 0.0).b;
float Mask = O.ShortWaveMask(P0.z / max(O.Chop.w, 1.0), Quiet, TroughCalm, QuietAmount);
float F1 = F * Mask;
float F2 = F * lerp(RippleFloor, 1.0, Mask) * saturate(1.0 - Dist / max(RippleFadeDistance, 1.0));
float2 H0 = O.ToWorld(G0.xy, Cascade0) * F;
float3 M0 = O.TensorToWorld(float3(G0.z, G0.w, A0.w), Cascade0) * F;
float2 H1 = O.ToWorld(G1.xy, Cascade1) * F1;
float3 M1 = O.TensorToWorld(float3(G1.z, G1.w, A1.w), Cascade1) * F1;
float2 H2 = O.ToWorld(G2.xy, Cascade2) * F2;
float3 M2 = O.TensorToWorld(float3(G2.z, G2.w, A2.w), Cascade2) * F2;
Tx += float3(M0.x + M1.x + M2.x, M0.z + M1.z + M2.z, H0.x + H1.x + H2.x);
Ty += float3(M0.z + M1.z + M2.z, M0.y + M1.y + M2.y, H0.y + H1.y + H2.y);
float3 N = normalize(cross(Tx, Ty));

// --- Seen from below (water -> air, n = 1.333): inside Snell's window (about 49 degrees around the local
// normal) the sky and the sun are refracted into view; outside it, total internal reflection mirrors the
// lit water body. The underwater pass then fogs this like any other surface.
FSaltUnderwaterLight U;
U.Init(SunDir, SunIllum, LookAbsorption.rgb, LookScattering.rgb, UnderwaterFog, UnderwaterSky, UnderwaterColor);
float3 I = normalize(WorldPos - CamPos);
N *= (dot(I, N) < 0.0) ? -1.0 : 1.0;
float CosI = saturate(dot(I, N));
const float Eta = 1.333;
float Sin2T = Eta * Eta * (1.0 - CosI * CosI);
float3 Deep = U.InScatterRay(reflect(I, N), 0.0, 100000.0);
float3 Color = Deep;
if (Sin2T < 1.0)
{
    float CosT = sqrt(1.0 - Sin2T);
    float3 T = normalize(Eta * I + (CosT - Eta * CosI) * N);
    float Rs = (Eta * CosI - CosT) / (Eta * CosI + CosT);
    float Rp = (CosI - Eta * CosT) / (CosI + Eta * CosT);
    float R = saturate(0.5 * (Rs * Rs + Rp * Rp));
    Color = lerp(U.SkyThrough(T), Deep, R);
}
// Comic wave shading: faces turned towards the sun pass more light, the others less, so the moving surface
// reads as rippled light instead of a flat ceiling (WaveShading = strength, WaveShadingSlope = contrast).
float Tilt = dot(N, U.Lw) - U.Lw.z;
return Color * (1.0 + WaveShading * clamp(Tilt * WaveShadingSlope, -1.0, 1.0));
"""

UNDER_SCALARS = dict({n: SCALARS[n] for n in VS_SCALARS + ["RippleFloor", "RippleFadeDistance"]},
                     WaveShading=(0.35, "Underwater"), WaveShadingSlope=(6.0, "Underwater"))

# --- underwater post-process (M_PP_Underwater) --------------------------------------------------------
# Surface height above the camera from the same displacement as the vertex shader at the finest LOD (the
# horizontal displacement is inverted by fixed-point iteration). Positive = the camera is under water.
PP_CAMERA_CODE = INIT + """
float2 P = CamPos.xy;
float3 D = 0;
[unroll] for (int k = 0; k < 4; k++)
{
    D = O.SurfaceDisplacement(P, CameraCellSize * 4.0, CameraCellSize, Cascade0, Cascade1, VSMipBias, Disp0, Disp0Sampler,
        Disp1, Disp1Sampler, QuietTex, QuietTexSampler, QuietTile, RippleOffsets.w, TroughCalm, QuietAmount, WindSeaVS1);
    P = CamPos.xy - D.xy;
}
return Globals.y + D.z - CamPos.z;
"""

PP_UV_CODE = """
// Slight wobble of the image under water.
float2 W = float2(sin(UV.y * 37.0 + Time * 1.7) + sin(UV.y * 13.0 - Time * 1.1), cos(UV.x * 29.0 + Time * 1.3));
return UV + W * (DistortAmount * step(0.0, CamDepth));
"""

PP_FOG_CODE = """
if (CamDepth <= 0.0)
{
    return SceneColor.rgb;
}
FSaltUnderwaterLight U;
U.Init(SunDir, SunIllum, LookAbsorption.rgb, LookScattering.rgb, UnderwaterFog, UnderwaterSky, UnderwaterColor);
float RelLen = length(PixelRel);
float3 Dir = PixelRel / max(RelLen, 1e-3);
float Dist = min(RelLen, MaxFogDistance);   // sky: very far, clamped
float PointDepth = Globals.y - (CamPos.z + Dir.z * Dist);
// Diagnostics (exposure-independent bands on the scene): DebugView 1 = red every 5 m of view distance,
// green every 1 m of depth below the water level.
if (DebugView > 0.5)
{
    return SceneColor.rgb * float3(frac(Dist / 500.0), frac(PointDepth / 100.0), 0.1) * 3.0;
}
// Post-process materials see absolute luminance (pre-exposure is removed before and reapplied after), so
// the in-scattered light (cd/m2) is added as is.
return U.Apply(SceneColor.rgb, Dir, Dist, max(Globals.y - CamPos.z, 0.0), max(PointDepth, 0.0));
"""


def rt_name(param):
    kind, c = param[:-1], param[-1]
    return "RT_Ocean%s%s" % (kind, c)


def ocean_inputs(m, mpc, rts, textures, scalars):
    """MPC parameters, camera, FFT texture objects (shared wrap sampler, so the sampler count stays low),
    the quiet-patch texture and scalar parameters. Returns a node context."""
    names = WAVE_PARAMS + FIELD_PARAMS + ["RippleOffsets"] + LOOK_PARAMS + UNDER_PARAMS
    ctx = {"nodes": {n: mpc_param(m, mpc, n, -2400, -1600 + i * 60) for i, n in enumerate(names)}}
    ctx["cam"] = sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -2400, 0)
    ctx["tex"] = {}
    for i, p in enumerate(sorted(set(textures))):
        ctx["tex"][p] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -2000, -1600 + i * 90, parameter_name=p,
                                   texture=rts[rt_name(p)], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                                   sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS, group="FFT")
    # Low-frequency noise (blue channel of the old foam pattern) for the drifting quiet patches.
    ctx["quiet_tex"] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 1050, parameter_name="QuietTex",
                                  texture=unreal.load_asset(DIR + "/Textures/T_OceanFoam_M"),
                                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, group="Waves")
    ctx["sp"] = {n: sb_ue.scalar_param(m, n, v, -1200, 1200 + i * 60, g) for i, (n, (v, g)) in enumerate(scalars.items())}
    return ctx


def link_inputs(ctx, node, names):
    """Connects context nodes (MPC parameters, textures, scalars) to the custom node inputs of the same name."""
    for n in names:
        src = ctx["nodes"].get(n) or ctx["tex"].get(n) or ctx["sp"].get(n)
        if src is None:
            raise RuntimeError("no input node for " + n)
        link(src, "", node, n)


def ocean_vertex(m, mpc, ush, ctx):
    """Vertex shader of the surface materials: CDLOD morph, swells and FFT wind sea, plus last frame's
    displacement for the velocity pass. Returns the interpolated Lagrangian position P0 (xy) and height (z)."""
    nodes = ctx["nodes"]
    wp_vs = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -2000, 200,
                       world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -2000, 300)
    cd = [sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -2000, 400 + i * 80, data_index=i) for i in range(3)]
    fade_vs = sb_ue.scalar_param(m, "WaveFadeVS", 0.03, -2000, 650, "Waves")
    vs_inputs = (["WorldPos", "LocalPos", "CamPos", "CellSize", "MorphStart", "MorphEnd", "FadeScale", "IsPrev", "QuietTex", "RippleOffsets"]
                 + VS_SCALARS + VS_TEXTURES + WAVE_PARAMS + FIELD_PARAMS)
    vs = custom(m, -1500, 300, "SaltOcean VS (morph + swells + FFT wind sea)", ush + "\n" + VS_CODE, vs_inputs, F3, [("P0", F3)])
    link(wp_vs, "", vs, "WorldPos")
    link(lp, "", vs, "LocalPos")
    link(ctx["cam"], "", vs, "CamPos")
    link(cd[0], "", vs, "CellSize")
    link(cd[1], "", vs, "MorphStart")
    link(cd[2], "", vs, "MorphEnd")
    link(fade_vs, "", vs, "FadeScale")
    link(ctx["quiet_tex"], "", vs, "QuietTex")
    link(nodes["RippleOffsets"], "", vs, "RippleOffsets")
    link_inputs(ctx, vs, VS_TEXTURES + VS_SCALARS)
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
    link_inputs(ctx, vs, [n for n in WAVE_PARAMS + FIELD_PARAMS if n not in PHASE_PARAMS])
    sb_ue.to_prop(vs, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    interp = sb_ue.expr(m, unreal.MaterialExpressionVertexInterpolator, -1200, 500)
    link(vs, "P0", interp, "")
    return interp


def sun_nodes(m, x, y, position=None):
    """Sun direction and illuminance after the atmosphere (optionally evaluated at a given world position)."""
    sun_dir = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, x, y)
    sun_illum = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, x, y + 100)
    if position is not None:
        link(position, "", sun_illum, "World Position")
    return sun_dir, sun_illum


def make_material(mpc, ush, rts):
    m = sb_ue.reset_material("M_Ocean", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    ctx = ocean_inputs(m, mpc, rts, VS_TEXTURES + PS_TEXTURES, SCALARS)
    interp = ocean_vertex(m, mpc, ush, ctx)

    # --- pixel: normals, roughness, crest light
    wp_ps = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1200, 700)
    fade_ps = sb_ue.scalar_param(m, "WaveFadePS", 0.002, -1200, 800, "Waves")
    sun_dir, sun_illum = sun_nodes(m, -1500, 2000)
    ps_scalars = [n for n in SCALARS if n not in VS_ONLY]
    ps_inputs = (["P0", "WorldPos", "CamPos", "FadeScale", "QuietTex", "SunDir", "SunIllum"]
                 + PS_TEXTURES + ps_scalars + WAVE_PARAMS + FIELD_PARAMS + PS_EXTRA)
    ps = custom(m, -700, 900, "SaltOcean PS (surface normal, roughness, crest light)", ush + "\n" + PS_CODE, ps_inputs, F3,
                [("RoughOut", F1), ("EmissiveOut", F3), ("SpecularOut", F1), ("FoamOut", F1)])
    link(sun_dir, "", ps, "SunDir")
    link(sun_illum, "", ps, "SunIllum")
    link(interp, "", ps, "P0")
    link(wp_ps, "", ps, "WorldPos")
    link(ctx["cam"], "", ps, "CamPos")
    link(fade_ps, "", ps, "FadeScale")
    link(ctx["quiet_tex"], "", ps, "QuietTex")
    link_inputs(ctx, ps, PS_TEXTURES + ps_scalars + WAVE_PARAMS + FIELD_PARAMS + PS_EXTRA)

    sb_ue.to_prop(ps, "", unreal.MaterialProperty.MP_NORMAL)
    # Surface layer = crest foam (opacity 0 = pure Single Layer Water elsewhere).
    sb_ue.to_prop(ps, "FoamOut", unreal.MaterialProperty.MP_OPACITY)
    foam_color = sb_ue.vector_param(m, "FoamColor", (0.82, 0.9, 0.92, 1), -300, 1050, "Foam")
    sb_ue.to_prop(foam_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(ps, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(ps, "EmissiveOut", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(ps, "SpecularOut", unreal.MaterialProperty.MP_SPECULAR)

    # --- water volume (Single Layer Water output): absorption and scattering per region (MPC Look*).
    nodes = ctx["nodes"]
    slw = sb_ue.expr(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 1200)
    phase_g = sb_ue.scalar_param(m, "PhaseG", 0.2, -400, 1300, "Volume")
    behind = sb_ue.vector_param(m, "ColorScaleBehindWater", (0.9, 0.9, 0.9, 1), -400, 1400, "Volume")
    # Clarity (owner: "make it more look through", D29). Single Layer Water reads the coefficients per cm; the
    # region values (Look*, written as 1/m) made the sea opaque within centimetres. Both coefficients are
    # scaled by the same factor, so the shallows show the sand while deep water keeps its colour (unchanged
    # scattering albedo). 0.009 (D30, owner: the ground must read as under water, not through glass): about 4 m
    # visibility in blue, turquoise shallows, blue beyond ~6 m depth.
    clarity = sb_ue.scalar_param(m, "VolumeScale", 0.009, -600, 1200, "Volume")
    for look, pin in (("LookScattering", "ScatteringCoefficients"), ("LookAbsorption", "AbsorptionCoefficients")):
        mul = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -200, 1200 if pin.startswith("S") else 1260)
        link(nodes[look], "", mul, "A")
        link(clarity, "", mul, "B")
        link(mul, "", slw, pin)
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


def above_water_position(m, ctx, x, y):
    """World position 10 m above the still water at the camera, where the atmosphere's sun illuminance is
    sampled (a point below the planet's ground would be in the planet's shadow)."""
    c = custom(m, x, y, "Above the water at the camera", "return float3(CamPos.xy, Globals.y + 1000.0);", ["CamPos", "Globals"], F3)
    link(ctx["cam"], "", c, "CamPos")
    link(ctx["nodes"]["Globals"], "", c, "Globals")
    return c


def make_under_material(mpc, ush, under_ush, rts):
    """The sea seen from below: unlit, drawn by ASaltOcean's mirrored underside tiles (reversed culling)."""
    m = sb_ue.reset_material("M_OceanUnder", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    ctx = ocean_inputs(m, mpc, rts, VS_TEXTURES + UNDER_TEXTURES, UNDER_SCALARS)
    interp = ocean_vertex(m, mpc, ush, ctx)
    wp_ps = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1200, 700)
    fade_ps = sb_ue.scalar_param(m, "WaveFadePS", 0.002, -1200, 800, "Waves")
    sun_dir, sun_illum = sun_nodes(m, -1500, 2000, above_water_position(m, ctx, -1800, 2100))
    ps_scalars = ["TroughCalm", "QuietAmount", "QuietTile", "RippleFloor", "RippleFadeDistance", "WaveShading", "WaveShadingSlope"]
    ps_inputs = (["P0", "WorldPos", "CamPos", "FadeScale", "QuietTex", "SunDir", "SunIllum", "RippleOffsets"]
                 + UNDER_TEXTURES + ps_scalars + WAVE_PARAMS + FIELD_PARAMS + LOOK_PARAMS + UNDER_PARAMS)
    ps = custom(m, -700, 900, "SaltOcean underside (Snell's window, total internal reflection)",
                ush + "\n" + under_ush + "\n" + UNDER_PS_CODE, ps_inputs, F3)
    link(sun_dir, "", ps, "SunDir")
    link(sun_illum, "", ps, "SunIllum")
    link(interp, "", ps, "P0")
    link(wp_ps, "", ps, "WorldPos")
    link(ctx["cam"], "", ps, "CamPos")
    link(fade_ps, "", ps, "FadeScale")
    link(ctx["quiet_tex"], "", ps, "QuietTex")
    link_inputs(ctx, ps, ["RippleOffsets"] + UNDER_TEXTURES + ps_scalars + WAVE_PARAMS + FIELD_PARAMS + LOOK_PARAMS + UNDER_PARAMS)
    sb_ue.to_prop(ps, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


def make_post_material(mpc, ush, under_ush, rts):
    """Post-process pass (before depth of field): changes the image only while the camera is under the GPU
    surface; ASaltOcean blends it in near and below the surface."""
    m = sb_ue.reset_material("M_PP_Underwater", DIR)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
    scalars = dict({n: SCALARS[n] for n in VS_SCALARS},
                   CameraCellSize=(25.0, "Underwater"), MaxFogDistance=(30000.0, "Underwater"), DistortAmount=(0.0012, "Underwater"),
                   DebugView=(0.0, "Diagnostics"))
    ctx = ocean_inputs(m, mpc, rts, ["Disp0", "Disp1"], scalars)
    cam_in = ["CamPos", "QuietTex", "RippleOffsets", "Disp0", "Disp1", "CameraCellSize"] + VS_SCALARS + WAVE_PARAMS + FIELD_PARAMS
    camera = custom(m, -1500, 300, "Surface above the camera (GPU)", ush + "\n" + PP_CAMERA_CODE, cam_in, F1)
    link(ctx["cam"], "", camera, "CamPos")
    link(ctx["quiet_tex"], "", camera, "QuietTex")
    link_inputs(ctx, camera, [n for n in cam_in if n not in ("CamPos", "QuietTex")])

    screen = sb_ue.expr(m, unreal.MaterialExpressionScreenPosition, -1500, 700)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1500, 800)
    uv = custom(m, -1100, 700, "Underwater wobble", PP_UV_CODE, ["UV", "Time", "CamDepth", "DistortAmount"],
                unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    link(screen, "ViewportUV", uv, "UV")
    link(time, "", uv, "Time")
    link(camera, "", uv, "CamDepth")
    link(ctx["sp"]["DistortAmount"], "", uv, "DistortAmount")
    scene = sb_ue.expr(m, unreal.MaterialExpressionSceneTexture, -800, 700, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    link(uv, "", scene, "UVs")
    # Camera to pixel, reconstructed from scene depth (post-process domain).
    pixel_rel = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -800, 900,
                           world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    sun_dir, sun_illum = sun_nodes(m, -800, 1200, above_water_position(m, ctx, -1100, 1300))

    fog_in = (["SceneColor", "PixelRel", "CamPos", "CamDepth", "SunDir", "SunIllum", "Globals", "MaxFogDistance", "DebugView"]
              + LOOK_PARAMS + UNDER_PARAMS)
    fog = custom(m, -400, 800, "Underwater fog and light", under_ush + "\n" + PP_FOG_CODE, fog_in, F3)
    link(scene, "Color", fog, "SceneColor")
    link(pixel_rel, "", fog, "PixelRel")
    link(ctx["cam"], "", fog, "CamPos")
    link(camera, "", fog, "CamDepth")
    link(sun_dir, "", fog, "SunDir")
    link(sun_illum, "", fog, "SunIllum")
    link_inputs(ctx, fog, ["Globals", "MaxFogDistance", "DebugView"] + LOOK_PARAMS + UNDER_PARAMS)
    sb_ue.to_prop(fog, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


# --- underwater particles (M_UnderwaterParticles, D30) ---------------------------------------------------
# Each instance carries an anchor (custom data 0-2, 0..1 of the box) and a random value (3). The anchor drifts
# with a slow current plus a little wobble and is wrapped into a box around the camera: particles stay put in
# the world while the camera moves. The quad (100 cm, Engine Plane) is turned to the camera. Drawn after depth of
# field, so the underwater fog does not apply: the fades below stand in for it.
PARTICLE_VS_CODE = """
float3 A = float3(D0, D1, D2) * Box;
float3 Wobble = float3(sin(Time * 0.37 + R * 6.28), cos(Time * 0.29 + R * 4.1), sin(Time * 0.21 + R * 2.3)) * 6.0;
float3 Rel = (frac((A + Drift.xyz * Time + Wobble - CamPos) / Box + 0.5) - 0.5) * Box;
float3 Centre = CamPos + Rel;
float Dist = length(Rel);
float S = Size * lerp(0.5, 1.5, R) * 0.01;
float Below = Globals.y - Centre.z;
FadeOut = smoothstep(15.0, 60.0, Dist) * (1.0 - smoothstep(Box * 0.3, Box * 0.48, Dist)) * saturate(Below / 40.0)
        * exp(-max(Below, 0.0) / 2500.0);
return Centre + (LocalPos.x * Right + LocalPos.y * Up) * S - WorldPos;
"""

PARTICLE_PS_CODE = """
float D = length(UV - 0.5) * 2.0;
float Dot = saturate(1.0 - D);
Dot = Dot * sqrt(Dot);
float SunLum = dot(SunIllum, float3(0.2126, 0.7152, 0.0722));
return Tint.rgb * (SunLum * Glow * Dot * Fade);
"""


def make_particle_material(mpc):
    m = sb_ue.reset_material("M_UnderwaterParticles", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    ctx = {"nodes": {"Globals": mpc_param(m, mpc, "Globals", -1600, 0)},
           "cam": sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -1600, 80)}
    cd = [sb_ue.expr(m, unreal.MaterialExpressionPerInstanceCustomData, -1600, 160 + i * 70, data_index=i) for i in range(4)]
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1600, 480,
                    world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -1600, 560)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1600, 640)
    axes = []
    for i, v in enumerate(((1, 0, 0), (0, 1, 0))):
        c = sb_ue.expr(m, unreal.MaterialExpressionConstant3Vector, -1800, 720 + i * 120, constant=unreal.LinearColor(*v, 1))
        t = sb_ue.expr(m, unreal.MaterialExpressionTransform, -1600, 720 + i * 120,
                       transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW,
                       transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        link(c, "", t, "")
        axes.append(t)
    params = {"Box": sb_ue.scalar_param(m, "Box", 1200.0, -1300, 900, "Particles"),
              "Size": sb_ue.scalar_param(m, "Size", 2.2, -1300, 960, "Particles"),
              "Drift": sb_ue.vector_param(m, "Drift", (8.0, 3.0, 0.8, 0), -1300, 1020, "Particles"),
              "Glow": sb_ue.scalar_param(m, "Glow", 0.012, -1300, 1100, "Particles"),
              "Tint": sb_ue.vector_param(m, "Tint", (0.8, 0.95, 0.9, 1), -1300, 1160, "Particles")}
    vs_in = ["D0", "D1", "D2", "R", "Box", "Drift", "Time", "CamPos", "Size", "Globals", "LocalPos", "WorldPos", "Right", "Up"]
    vs = custom(m, -1000, 300, "Underwater particles (placement, billboard)", PARTICLE_VS_CODE, vs_in, F3, [("FadeOut", F1)])
    for i, n in enumerate(("D0", "D1", "D2", "R")):
        link(cd[i], "", vs, n)
    for n in ("Box", "Drift", "Size"):
        link(params[n], "", vs, n)
    link(time, "", vs, "Time")
    link(ctx["cam"], "", vs, "CamPos")
    link(ctx["nodes"]["Globals"], "", vs, "Globals")
    link(lp, "", vs, "LocalPos")
    link(wp, "", vs, "WorldPos")
    link(axes[0], "", vs, "Right")
    link(axes[1], "", vs, "Up")
    sb_ue.to_prop(vs, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    fade = sb_ue.expr(m, unreal.MaterialExpressionVertexInterpolator, -700, 400)
    link(vs, "FadeOut", fade, "")
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -700, 500)
    _, sun_illum = sun_nodes(m, -700, 600, above_water_position(m, ctx, -1000, 700))
    ps = custom(m, -400, 500, "Underwater particles (soft dot)", PARTICLE_PS_CODE, ["UV", "Fade", "SunIllum", "Glow", "Tint"], F3)
    link(uv, "", ps, "UV")
    link(fade, "", ps, "Fade")
    link(sun_illum, "", ps, "SunIllum")
    link(params["Glow"], "", ps, "Glow")
    link(params["Tint"], "", ps, "Tint")
    sb_ue.to_prop(ps, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


def make_instance(name, parent):
    mi = sb_ue.make_mi(name, DIR, parent)
    # Instance overrides from earlier experiments would mask the master defaults: start clean.
    mel.clear_all_material_instance_parameters(mi)
    mel.update_material_instance(mi)
    sb_ue.eal.save_loaded_asset(mi)
    return mi


def main():
    """Optional arguments pick materials to rebuild: ocean, under, post, particles (default: all)."""
    parts = sys.argv[1:] or ["ocean", "under", "post", "particles"]
    with open(USH, encoding="utf-8") as f:
        ush = f.read()
    with open(UNDER_USH, encoding="utf-8") as f:
        under_ush = f.read()
    mpc = make_mpc()
    rts = make_targets()
    stats = []
    if "ocean" in parts:
        m = make_material(mpc, ush, rts)
        make_instance("MI_Ocean", m)
        stats.append(("ocean", m))
    if "under" in parts:
        under = make_under_material(mpc, ush, under_ush, rts)
        make_instance("MI_OceanUnder", under)
        stats.append(("under", under))
    if "post" in parts:
        post = make_post_material(mpc, ush, under_ush, rts)
        make_instance("MI_PP_Underwater", post)
        stats.append(("post", post))
    if "particles" in parts:
        particles = make_particle_material(mpc)
        make_instance("MI_UnderwaterParticles", particles)
        stats.append(("particles", particles))
    print("SB_OCEAN_ASSETS_OK", " ".join("%s=%d" % (n, mel.get_statistics(x).num_pixel_shader_instructions) for n, x in stats))


if __name__ == "__main__":
    main()
