"""Master materials and the initial material library (script-owned; safe to re-run).

  M_SB_Surface  wood / paint / metal / rope: tinted neutral textures, per-part variation (vertex colour B),
                world macro variation, edge wear (vertex colour R + wear mask), grime/wet (vertex colour G)
  M_SB_Rock     world-aligned triplanar rock, lichen on top faces, wet darkening near the waterline,
                caustic light under the water (D28)
  M_SB_Glow     emissive (lantern glass, flame, ancient technology accents)
  M_SB_SeaGrass two-sided sea grass blades: base-to-tip colour ramp (UV.v = height), swaying in the current
Colour values are linear. Palette rationale: Docs/ArtDirection.md.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

MASTER = "/Game/Saltbound/Materials/Master"
MI = "/Game/Saltbound/Materials/Instances"
TEX = "/Game/Saltbound/Materials/Textures"
mel = sb_ue.mel
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3


def tex(path):
    return unreal.load_asset(path)


def custom(m, x, y, desc, code, inputs, out_type, extra=()):
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
    for n, t in extra:
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", n)
        co.set_editor_property("output_type", t)
        outs.append(co)
    c.set_editor_property("additional_outputs", outs)
    return c


def tex_param(m, name, texture, x, y, uv=None, sampler=None, group="Textures"):
    e = sb_ue.expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=texture, group=group)
    if sampler is not None:
        e.set_editor_property("sampler_type", sampler)
    if uv is not None:
        sb_ue.connect(uv, "", e, "UVs")
    return e


# --- M_SB_Surface -------------------------------------------------------------------------------
SURFACE_CODE = """
float3 Base = Tint * Albedo * TexNormalize;
Base *= lerp(1.0 - VarAmount, 1.0 + VarAmount, VC.b);
Base = lerp(Base, Base * VarTint.rgb, VC.b * VarHue);
Base *= lerp(1.0 - MacroAmount, 1.0 + MacroAmount, Macro.r);
float Rough = saturate(ORM.g * RoughScale + RoughOffset + (VC.b - 0.5) * VarRough);
float Metal = Metallic;
// Edge wear: convex edges (vertex colour R) broken up by the wear mask.
float Wear = saturate((VC.r * WearStrength - (1.0 - WearTex.r)) * 3.0);
Base = lerp(Base, WearColor.rgb, Wear);
Rough = lerp(Rough, WearRoughness, Wear);
Metal = lerp(Metal, WearMetallic, Wear);
// Grime / wet: crevices and waterline (vertex colour G).
float Grime = saturate(VC.g * GrimeAmount * lerp(0.6, 1.0, WearTex.b));
Base = lerp(Base, Base * GrimeColor.rgb, Grime);
Rough = lerp(Rough, GrimeRoughness, Grime);
AO = lerp(1.0, ORM.r, AOStrength);
RoughOut = Rough;
MetalOut = Metal;
return Base;
"""


def build_surface():
    m = sb_ue.reset_material("M_SB_Surface", MASTER)
    uv0 = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -2200, 0)
    uv_scale = sb_ue.scalar_param(m, "UVScale", 1.0, -2200, 100, "Textures")
    uv = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -2000, 50)
    sb_ue.connect(uv0, "", uv, "A")
    sb_ue.connect(uv_scale, "", uv, "B")
    albedo = tex_param(m, "BaseTexture", tex(TEX + "/Wood/T_WoodGrain_D"), -1700, -300, uv)
    normal = tex_param(m, "NormalTexture", tex(TEX + "/Wood/T_WoodGrain_N"), -1700, 0, uv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm = tex_param(m, "ORMTexture", tex(TEX + "/Wood/T_WoodGrain_ORM"), -1700, 300, uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    wear_uv_scale = sb_ue.scalar_param(m, "WearUVScale", 0.7, -2200, 500, "Wear")
    wear_uv = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -2000, 500)
    sb_ue.connect(uv0, "", wear_uv, "A")
    sb_ue.connect(wear_uv_scale, "", wear_uv, "B")
    wear = tex_param(m, "WearTexture", tex(TEX + "/Wood/T_PaintWear_M"), -1700, 600, wear_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, "Wear")
    # World-space macro variation (breaks up tiling over large surfaces).
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -2400, 900)
    mask_xy = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -2200, 900, r=True, g=True, b=False, a=False)
    sb_ue.connect(wp, "", mask_xy, "")
    macro_scale = sb_ue.scalar_param(m, "MacroScale", 600.0, -2200, 1000, "Variation")
    macro_uv = sb_ue.expr(m, unreal.MaterialExpressionDivide, -2000, 950)
    sb_ue.connect(mask_xy, "", macro_uv, "A")
    sb_ue.connect(macro_scale, "", macro_uv, "B")
    macro = tex_param(m, "MacroTexture", tex(TEX + "/Common/T_MacroNoise_M"), -1700, 900, macro_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, "Variation")
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -1700, 1200)

    params = {
        "Tint": ("v", (0.5, 0.5, 0.5, 1), "Color"),
        "TexNormalize": ("s", 3.3, "Color"),
        "VarAmount": ("s", 0.12, "Variation"),
        "VarTint": ("v", (1.0, 0.92, 0.85, 1), "Variation"),
        "VarHue": ("s", 0.5, "Variation"),
        "MacroAmount": ("s", 0.08, "Variation"),
        "RoughScale": ("s", 1.0, "Surface"),
        "RoughOffset": ("s", 0.0, "Surface"),
        "VarRough": ("s", 0.0, "Variation"),
        "Metallic": ("s", 0.0, "Surface"),
        "AOStrength": ("s", 1.0, "Surface"),
        "WearStrength": ("s", 1.2, "Wear"),
        "WearColor": ("v", (0.6, 0.55, 0.48, 1), "Wear"),
        "WearRoughness": ("s", 0.7, "Wear"),
        "WearMetallic": ("s", 0.0, "Wear"),
        "GrimeAmount": ("s", 1.0, "Grime"),
        "GrimeColor": ("v", (0.45, 0.42, 0.38, 1), "Grime"),
        "GrimeRoughness": ("s", 0.75, "Grime"),
    }
    nodes = {}
    for i, (name, (kind, val, group)) in enumerate(params.items()):
        if kind == "v":
            nodes[name] = sb_ue.vector_param(m, name, val, -1300, -400 + i * 70, group)
        else:
            nodes[name] = sb_ue.scalar_param(m, name, val, -1300, -400 + i * 70, group)
    inputs = ["Albedo", "ORM", "VC", "Macro", "WearTex"] + list(params)
    c = custom(m, -800, 200, "SB surface combine", SURFACE_CODE, inputs, F3, [("RoughOut", F1), ("MetalOut", F1), ("AO", F1)])
    sb_ue.connect(albedo, "RGB", c, "Albedo")
    sb_ue.connect(orm, "RGB", c, "ORM")
    sb_ue.connect(vc, "", c, "VC")
    sb_ue.connect(macro, "RGB", c, "Macro")
    sb_ue.connect(wear, "RGB", c, "WearTex")
    for n in params:
        sb_ue.connect(nodes[n], "", c, n)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(c, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(c, "MetalOut", unreal.MaterialProperty.MP_METALLIC)
    sb_ue.to_prop(c, "AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    # Normal strength: lerp from flat.
    nstr = sb_ue.scalar_param(m, "NormalStrength", 1.0, -1300, 900, "Surface")
    flat = sb_ue.expr(m, unreal.MaterialExpressionConstant3Vector, -1300, 1000, constant=unreal.LinearColor(0, 0, 1, 1))
    nl = sb_ue.expr(m, unreal.MaterialExpressionLinearInterpolate, -1000, 950)
    sb_ue.connect(flat, "", nl, "A")
    sb_ue.connect(normal, "RGB", nl, "B")
    sb_ue.connect(nstr, "", nl, "Alpha")
    sb_ue.to_prop(nl, "", unreal.MaterialProperty.MP_NORMAL)
    emissive = sb_ue.vector_param(m, "EmissiveColor", (0, 0, 0, 1), -1000, 1100, "Emissive")
    sb_ue.to_prop(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


# --- M_SB_Rock ------------------------------------------------------------------------------------
ROCK_CODE = """
// Triplanar sampling in world space (cm). Blend weights from the world normal.
// Sharp blend: a wide crossfade of two projections smears the painted detail.
float3 W = pow(abs(N), 10.0); W /= (W.x + W.y + W.z);
float2 UVX = P.yz / Scale, UVY = P.xz / Scale, UVZ = P.xy / Scale;
float3 AX = Texture2DSample(AlbedoTex, AlbedoTexSampler, UVX).rgb;
float3 AY = Texture2DSample(AlbedoTex, AlbedoTexSampler, UVY).rgb;
float3 AZ = Texture2DSample(AlbedoTex, AlbedoTexSampler, UVZ).rgb;
float3 Alb = AX * W.x + AY * W.y + AZ * W.z;
float2 OX = Texture2DSample(ORMTex, ORMTexSampler, UVX).rg;
float2 OY = Texture2DSample(ORMTex, ORMTexSampler, UVY).rg;
float2 OZ = Texture2DSample(ORMTex, ORMTexSampler, UVZ).rg;
float2 O = OX * W.x + OY * W.y + OZ * W.z;
// Whiteout-style normal blend of tangent normals swizzled into world space.
float2 NX = Texture2DSample(NormalTex, NormalTexSampler, UVX).xy * 2 - 1;
float2 NY = Texture2DSample(NormalTex, NormalTexSampler, UVY).xy * 2 - 1;
float2 NZ = Texture2DSample(NormalTex, NormalTexSampler, UVZ).xy * 2 - 1;
float3 WN = normalize(N + float3(0, NX.x, NX.y) * W.x * NormalStrength +
                      float3(NY.x, 0, NY.y) * W.y * NormalStrength +
                      float3(NZ.x, NZ.y, 0) * W.z * NormalStrength);
// Detail masks (barnacles, lichen lobes, grit), triplanar at a finer scale.
float2 DX = P.yz / DetailScale, DY = P.xz / DetailScale, DZ = P.xy / DetailScale;
float3 D = Texture2DSample(DetailTex, DetailTexSampler, DX).rgb * W.x + Texture2DSample(DetailTex, DetailTexSampler, DY).rgb * W.y
         + Texture2DSample(DetailTex, DetailTexSampler, DZ).rgb * W.z;
float3 Base = Tint * Alb * TexNormalize;
float Macro = Texture2DSample(MacroTex, MacroTexSampler, P.xy / MacroScale).r;
Base *= lerp(1.0 - MacroAmount, 1.0 + MacroAmount, Macro);
Base *= lerp(1.0 - GritAmount, 1.0 + GritAmount, D.b);
// Painted light: faces turned to the sky are a little lighter.
Base *= 1.0 + TopLighten * saturate(WN.z);
// Strata banding: warm and cool layers alternate with height.
float Band = 0.5 + 0.5 * sin(P.z / StrataScale * 6.2831853 + Macro * 5.0);
Base *= lerp(StrataTintA.rgb, StrataTintB.rgb, Band);
// Baked vertex masks: R convex edges (sunlit, worn), G cavities (dirt), B per-plate tone.
float3 VM = lerp(float3(0, 0, 0.5), VC.rgb, VertexMaskAmount);
Base *= lerp(1.0 - PlateVar, 1.0 + PlateVar, VM.b);
Base *= lerp(1.0, EdgeLighten, VM.r);
Base *= lerp(1.0, CavityDarken, VM.g);
float H = P.z - WaterLevel;
// Lichen: crisp lobed patches on faces pointing up, above the spray zone.
float Up = saturate((WN.z - LichenUp) / max(1.0 - LichenUp, 0.01));
float LichenField = D.g * 0.65 + Macro * 0.5 + Up * 0.35 - (1.0 - LichenCoverage);
float Lichen = smoothstep(0.45, 0.5, LichenField) * saturate((H - LichenMinHeight) / 100.0) * step(0.05, Up);
Base = lerp(Base, LichenColor.rgb * lerp(0.8, 1.15, D.b), Lichen);
// Tide zone: green algae and barnacles from below the waterline up to TideHeight; wet band above it.
float Tide = saturate(1.0 - H / max(TideHeight, 1.0)) * saturate((H + 200.0) / 60.0);
float Algae = Tide * saturate(AlgaeAmount * (0.55 + 0.6 * Macro) - VM.r * 0.4);
Base = lerp(Base, AlgaeColor.rgb * lerp(0.75, 1.2, D.b), Algae);
float Barn = D.r * saturate(1.0 - abs(H - TideHeight * 0.25) / max(TideHeight * 0.4, 1.0)) * BarnacleAmount;
Base = lerp(Base, BarnacleColor.rgb, saturate(Barn));
float Wet = saturate(1.0 - (H - TideHeight) / max(WetHeight, 1.0));
Base *= lerp(1.0, WetDarken, Wet * (1.0 - Barn));
float Rough = lerp(O.y * RoughScale, WetRoughness, Wet);
Rough = lerp(Rough, 0.85, Lichen);
RoughOut = Rough;
AOOut = O.x * lerp(1.0, 0.45, VM.g);
NormalOut = WN;
return Base;
"""


def build_rock():
    m = sb_ue.reset_material("M_SB_Rock", MASTER)
    m.set_editor_property("tangent_space_normal", False)
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1800, 0)
    wn = sb_ue.expr(m, unreal.MaterialExpressionVertexNormalWS, -1800, 100)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -1800, 180)
    texobj = {
        "AlbedoTex": (TEX + "/Rock/T_Rock_D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        "NormalTex": (TEX + "/Rock/T_Rock_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        "ORMTex": (TEX + "/Rock/T_Rock_ORM", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR),
        "MacroTex": (TEX + "/Common/T_MacroNoise_M", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR),
        "DetailTex": (TEX + "/Rock/T_RockDetail_M", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR),
    }
    tnodes = {}
    for i, (n, (p, st)) in enumerate(texobj.items()):
        tnodes[n] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1800, 250 + i * 120,
                               parameter_name=n, texture=tex(p), sampler_type=st, group="Textures")
    params = {
        "Scale": ("s", 250.0, "Textures"), "NormalStrength": ("s", 0.8, "Textures"), "DetailScale": ("s", 250.0, "Textures"),
        "Tint": ("v", (0.29, 0.275, 0.25, 1), "Color"), "TopLighten": ("s", 0.18, "Color"), "TexNormalize": ("s", 2.0, "Color"),
        "MacroScale": ("s", 1500.0, "Color"), "MacroAmount": ("s", 0.18, "Color"), "GritAmount": ("s", 0.04, "Color"),
        "StrataScale": ("s", 160.0, "Color"), "StrataTintA": ("v", (1.02, 1.0, 0.97, 1), "Color"),
        "StrataTintB": ("v", (0.95, 0.97, 0.99, 1), "Color"),
        "VertexMaskAmount": ("s", 1.0, "Vertex masks"), "PlateVar": ("s", 0.12, "Vertex masks"),
        "EdgeLighten": ("s", 1.3, "Vertex masks"), "CavityDarken": ("s", 0.6, "Vertex masks"),
        "LichenColor": ("v", (0.5, 0.36, 0.1, 1), "Lichen"), "LichenUp": ("s", 0.45, "Lichen"),
        "LichenCoverage": ("s", 0.42, "Lichen"), "LichenMinHeight": ("s", 160.0, "Lichen"),
        "WaterLevel": ("s", 0.0, "Wet"), "TideHeight": ("s", 70.0, "Wet"), "AlgaeAmount": ("s", 0.85, "Wet"),
        "AlgaeColor": ("v", (0.07, 0.1, 0.035, 1), "Wet"), "BarnacleAmount": ("s", 0.6, "Wet"),
        "BarnacleColor": ("v", (0.55, 0.53, 0.48, 1), "Wet"),
        "WetHeight": ("s", 80.0, "Wet"), "WetDarken": ("s", 0.5, "Wet"),
        "WetRoughness": ("s", 0.3, "Wet"), "RoughScale": ("s", 1.0, "Surface"),
    }
    nodes = {}
    for i, (name, (kind, val, group)) in enumerate(params.items()):
        nodes[name] = (sb_ue.vector_param if kind == "v" else sb_ue.scalar_param)(m, name, val, -1300, -400 + i * 70, group)
    inputs = ["P", "N", "VC"] + list(texobj) + list(params)
    c = custom(m, -800, 200, "SB rock triplanar", ROCK_CODE, inputs, F3, [("RoughOut", F1), ("AOOut", F1), ("NormalOut", F3)])
    sb_ue.connect(wp, "", c, "P")
    sb_ue.connect(wn, "", c, "N")
    sb_ue.connect(vc, "", c, "VC")
    for n in texobj:
        sb_ue.connect(tnodes[n], "", c, n)
    for n in params:
        sb_ue.connect(nodes[n], "", c, n)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(c, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(c, "AOOut", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    sb_ue.to_prop(c, "NormalOut", unreal.MaterialProperty.MP_NORMAL)
    add_caustics(m, c, wp, wn, nodes["WaterLevel"], -300, 900)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


# Caustics (D28, D29): two drifting patterns of T_Caustics_M, their minimum is a moving web of light. Only below
# the water level. Projected three ways (top along the sun, two sides) and blended by the surface normal, so the
# pattern keeps its size on slopes instead of stretching (owner feedback); a mip bias softens it. Emissive:
# sunlight focused by the waves, scaled like diffuse light.
CAUSTIC_CODE = """
float Depth = WaterLevel - P.z;
float Under = saturate(Depth / 30.0);
if (Under <= 0.0 || CausticStrength <= 0.0)
{
    return 0;
}
float3 L = normalize(SunDir);
float T = Time * CausticSpeed;
float3 W = pow(abs(N), 4.0);
W /= max(W.x + W.y + W.z, 1e-4);
float2 Proj[3] = { (P.xy + L.xy / max(L.z, 0.25) * Depth) / CausticScale, P.xz / CausticScale, P.yz / CausticScale };
float Weights[3] = { W.z, W.y, W.x };
float C = 0.0;
[unroll] for (int i = 0; i < 3; i++)
{
    float A = Texture2DSampleBias(CausticTex, CausticTexSampler, Proj[i] + T * float2(0.71, 0.34), CausticBlur).r;
    float B = Texture2DSampleBias(CausticTex, CausticTexSampler, Proj[i] * 1.17 + float2(0.37, 0.61) - T * float2(0.28, 0.66), CausticBlur).g;
    C += min(A, B) * Weights[i];
}
// Light reaches upward faces fully, steep faces partly.
float Facing = saturate(N.z * 0.8 + 0.2);
return Base * SunIllum * (saturate(L.z) * 0.3183 * 2.0 * C * CausticStrength * Under * Facing * exp(-max(Depth, 0.0) / CausticDepthFade));
"""


def add_caustics(m, base_node, wp, wn, water_level, x, y):
    """Emissive caustics for a lit master: base_node gives the base colour."""
    ctex = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, x - 500, y, parameter_name="CausticTex",
                      texture=tex(TEX + "/Common/T_Caustics_M"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                      group="Caustics")
    params = {"CausticStrength": 0.6, "CausticScale": 350.0, "CausticSpeed": 0.03, "CausticDepthFade": 1500.0,
              "CausticBlur": 1.5}
    pn = {n: sb_ue.scalar_param(m, n, v, x - 500, y + 120 + i * 60, "Caustics") for i, (n, v) in enumerate(params.items())}
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, x - 500, y + 400)
    # Sun illuminance sampled above the water (points below the planet's ground are in the planet's shadow).
    above = custom(m, x - 500, y + 480, "Above the water", "return float3(P.xy, WaterLevel + 1000.0);", ["P", "WaterLevel"], F3)
    sb_ue.connect(wp, "", above, "P")
    sb_ue.connect(water_level, "", above, "WaterLevel")
    sun_dir = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, x - 300, y + 560)
    sun_illum = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightIlluminance, x - 300, y + 640)
    sb_ue.connect(above, "", sun_illum, "World Position")
    c = custom(m, x, y, "SB caustics", CAUSTIC_CODE,
               ["P", "N", "Base", "CausticTex", "Time", "SunDir", "SunIllum", "WaterLevel"] + list(params), F3)
    sb_ue.connect(wp, "", c, "P")
    sb_ue.connect(wn, "", c, "N")
    sb_ue.connect(base_node, "", c, "Base")
    sb_ue.connect(ctex, "", c, "CausticTex")
    sb_ue.connect(time, "", c, "Time")
    sb_ue.connect(sun_dir, "", c, "SunDir")
    sb_ue.connect(sun_illum, "", c, "SunIllum")
    sb_ue.connect(water_level, "", c, "WaterLevel")
    for n in params:
        sb_ue.connect(pn[n], "", c, n)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


# --- M_SB_Glow -------------------------------------------------------------------------------------
def build_glow():
    m = sb_ue.reset_material("M_SB_Glow", MASTER)
    base = sb_ue.vector_param(m, "BaseColor", (0.3, 0.22, 0.12, 1), -800, 0, "Color")
    glow = sb_ue.vector_param(m, "GlowColor", (1.0, 0.5, 0.18, 1), -800, 150, "Glow")
    inten = sb_ue.scalar_param(m, "GlowIntensity", 20.0, -800, 250, "Glow")
    mult = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -500, 200)
    sb_ue.connect(glow, "", mult, "A")
    sb_ue.connect(inten, "", mult, "B")
    rough = sb_ue.scalar_param(m, "Roughness", 0.25, -800, 350, "Surface")
    sb_ue.to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


# --- M_SB_SeaGrass ----------------------------------------------------------------------------------
# Blades are geometry (UV.v = 0 at the root, 1 at the tip). Comic look: a clean dark-to-light ramp along the
# blade, per-clump hue variation, a soft lighter rim towards the tip.
SEAGRASS_COLOR_CODE = """
float V = saturate(UV.y);
float3 C = lerp(RootColor.rgb, TipColor.rgb, pow(V, 0.8));
// Per clump: a little brighter / darker and warmer / cooler.
float R = frac(Rand * 7.31);
C *= lerp(1.0 - Variation, 1.0 + Variation, Rand) * lerp(float3(1.06, 1.0, 0.9), float3(0.92, 1.0, 1.08), R);
return C;
"""

# Sway: tips move most (V^2), slow main swing plus a faster flutter, phase from position and clump.
SEAGRASS_WPO_CODE = """
float V = saturate(UV.y);
float Phase = dot(Pivot.xy, float2(0.013, 0.009)) + Rand * 6.2831853;
float Swing = sin(Time * SwaySpeed + Phase) + 0.35 * sin(Time * SwaySpeed * 2.7 + Phase * 1.7);
float2 Dir = normalize(float2(1.0, 0.45));
float2 Side = float2(-Dir.y, Dir.x) * (0.4 * sin(Time * SwaySpeed * 0.8 + Phase * 2.3));
return float3((Dir * Swing + Side) * (SwayAmount * V * V), -SwayAmount * 0.15 * V * V * abs(Swing));
"""


def build_seagrass():
    m = sb_ue.reset_material("M_SB_SeaGrass", MASTER)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    rand = sb_ue.expr(m, unreal.MaterialExpressionPerInstanceRandom, -1200, 100)
    pivot = sb_ue.expr(m, unreal.MaterialExpressionObjectPositionWS, -1200, 200)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1200, 300)
    params = {"RootColor": ("v", (0.03, 0.12, 0.05, 1)), "TipColor": ("v", (0.32, 0.5, 0.14, 1)),
              "Variation": ("s", 0.15), "SwayAmount": ("s", 14.0), "SwaySpeed": ("s", 0.9),
              "Roughness": ("s", 0.55), "Specular": ("s", 0.3)}
    pn = {}
    for i, (n, (k, v)) in enumerate(params.items()):
        pn[n] = (sb_ue.vector_param if k == "v" else sb_ue.scalar_param)(m, n, v, -1200, 400 + i * 70, "Sea grass")
    col = custom(m, -700, 0, "Sea grass colour", SEAGRASS_COLOR_CODE, ["UV", "Rand", "RootColor", "TipColor", "Variation"], F3)
    sb_ue.connect(uv, "", col, "UV")
    sb_ue.connect(rand, "", col, "Rand")
    for n in ("RootColor", "TipColor", "Variation"):
        sb_ue.connect(pn[n], "", col, n)
    wpo = custom(m, -700, 300, "Sea grass sway", SEAGRASS_WPO_CODE, ["UV", "Rand", "Pivot", "Time", "SwayAmount", "SwaySpeed"], F3)
    sb_ue.connect(uv, "", wpo, "UV")
    sb_ue.connect(rand, "", wpo, "Rand")
    sb_ue.connect(pivot, "", wpo, "Pivot")
    sb_ue.connect(time, "", wpo, "Time")
    sb_ue.connect(pn["SwayAmount"], "", wpo, "SwayAmount")
    sb_ue.connect(pn["SwaySpeed"], "", wpo, "SwaySpeed")
    sb_ue.to_prop(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    sb_ue.to_prop(pn["Roughness"], "", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(pn["Specular"], "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


def paint_textures():
    return {"BaseTexture": TEX + "/Paint/T_PaintSurface_D", "NormalTexture": TEX + "/Paint/T_PaintSurface_N",
            "ORMTexture": TEX + "/Paint/T_PaintSurface_ORM"}


def canvas_textures():
    return {"BaseTexture": TEX + "/Fabric/T_Canvas_D", "NormalTexture": TEX + "/Fabric/T_Canvas_N",
            "ORMTexture": TEX + "/Paint/T_PaintSurface_ORM"}


def weathered_textures():
    return {"BaseTexture": TEX + "/Wood/T_WoodWeathered_D", "NormalTexture": TEX + "/Wood/T_WoodWeathered_N",
            "ORMTexture": TEX + "/Wood/T_WoodWeathered_ORM"}


def wood_textures():
    return {"BaseTexture": TEX + "/Wood/T_WoodGrain_D", "NormalTexture": TEX + "/Wood/T_WoodGrain_N",
            "ORMTexture": TEX + "/Wood/T_WoodGrain_ORM"}


def main():
    surf = build_surface()
    rock = build_rock()
    glow = build_glow()
    grass = build_seagrass()
    metal_tex = {"NormalTexture": TEX + "/Metal/T_Metal_N", "ORMTexture": TEX + "/Metal/T_Metal_ORM"}
    rope_tex = {"BaseTexture": TEX + "/Fabric/T_Rope_D", "NormalTexture": TEX + "/Fabric/T_Rope_N"}
    lib = {
        # Boat: warm, cared for
        "MI_Wood_Teak": dict(vectors={"Tint": (0.2, 0.12, 0.068, 1), "WearColor": (0.42, 0.24, 0.11, 1), "GrimeColor": (0.55, 0.45, 0.38, 1)},
                             scalars={"RoughScale": 0.95, "WearStrength": 0.9, "GrimeAmount": 0.8, "VarAmount": 0.2}, textures=wood_textures()),
        "MI_Wood_Varnish": dict(vectors={"Tint": (0.095, 0.05, 0.028, 1), "WearColor": (0.3, 0.14, 0.05, 1)},
                                scalars={"RoughScale": 0.45, "RoughOffset": -0.05, "NormalStrength": 0.6, "WearStrength": 0.8, "WearRoughness": 0.55}, textures=wood_textures()),
        "MI_Paint_Cream": dict(vectors={"Tint": (0.74, 0.64, 0.45, 1), "WearColor": (0.3, 0.17, 0.08, 1), "VarTint": (1.0, 0.97, 0.92, 1)},
                               scalars={"TexNormalize": 2.6, "NormalStrength": 0.6, "RoughScale": 1.1, "VarAmount": 0.04, "VarHue": 0.3, "WearStrength": 1.15, "WearRoughness": 0.7},
                               textures=paint_textures()),
        "MI_Metal_Brass": dict(vectors={"Tint": (0.5, 0.33, 0.12, 1), "WearColor": (0.85, 0.62, 0.3, 1), "GrimeColor": (0.35, 0.42, 0.3, 1)},
                               scalars={"Metallic": 1.0, "WearMetallic": 1.0, "WearRoughness": 0.2, "TexNormalize": 1.0, "RoughScale": 0.9, "VarAmount": 0.05, "MacroAmount": 0.04},
                               textures=dict(metal_tex, BaseTexture="/Engine/EngineResources/WhiteSquareTexture")),
        "MI_Metal_Iron": dict(vectors={"Tint": (0.045, 0.042, 0.038, 1), "WearColor": (0.12, 0.11, 0.1, 1), "GrimeColor": (2.2, 1.0, 0.45, 1)},
                              scalars={"Metallic": 0.75, "WearMetallic": 0.9, "WearRoughness": 0.35, "TexNormalize": 1.0, "RoughScale": 1.35, "GrimeAmount": 0.7},
                              textures=dict(metal_tex, BaseTexture="/Engine/EngineResources/WhiteSquareTexture")),
        "MI_Rope_Hemp": dict(vectors={"Tint": (0.36, 0.27, 0.15, 1)},
                             scalars={"RoughScale": 1.4, "WearStrength": 0.0, "TexNormalize": 1.8, "UVScale": 6.0, "MacroAmount": 0.0}, textures=rope_tex),
        "MI_Caulk_Dark": dict(vectors={"Tint": (0.025, 0.022, 0.02, 1)}, scalars={"RoughScale": 1.0, "NormalStrength": 0.2, "WearStrength": 0.0}, textures=wood_textures()),
        "MI_Paint_Hull": dict(vectors={"Tint": (0.22, 0.04, 0.025, 1), "WearColor": (0.22, 0.1, 0.05, 1), "GrimeColor": (0.4, 0.5, 0.35, 1)},
                              scalars={"TexNormalize": 2.6, "NormalStrength": 0.6, "RoughScale": 0.9, "VarAmount": 0.03, "WearStrength": 1.0, "GrimeAmount": 1.0, "GrimeRoughness": 0.35},
                              textures=paint_textures()),
        "MI_Paint_Boottop": dict(vectors={"Tint": (0.03, 0.03, 0.032, 1), "GrimeColor": (0.8, 1.2, 0.7, 1)},
                                 scalars={"TexNormalize": 2.6, "NormalStrength": 0.5, "RoughScale": 0.8, "WearStrength": 0.3}, textures=paint_textures()),
        "MI_Glass_Dark": dict(vectors={"Tint": (0.02, 0.03, 0.035, 1)},
                              scalars={"TexNormalize": 1.0, "NormalStrength": 0.0, "RoughScale": 0.0, "RoughOffset": 0.06, "WearStrength": 0.0, "GrimeAmount": 0.0},
                              textures={"BaseTexture": "/Engine/EngineResources/WhiteSquareTexture"}),
        "MI_Canvas": dict(vectors={"Tint": (0.55, 0.51, 0.43, 1)}, scalars={"TexNormalize": 2.6, "NormalStrength": 1.0, "RoughScale": 1.6, "WearStrength": 0.0, "UVScale": 1.0}, textures=canvas_textures()),
        # Harbour: weathered, cooler
        # Weathered dock timber (D20): desaturated grey-brown, olive-brown board variation, matte, broad grain;
        # edge accents only where the wear mask breaks through (no continuous pale outlines).
        "MI_Wood_DockWeathered": dict(vectors={"Tint": (0.074, 0.068, 0.056, 1), "WearColor": (0.19, 0.18, 0.155, 1),
                                               "GrimeColor": (0.55, 0.62, 0.42, 1), "VarTint": (0.9, 1.0, 0.74, 1)},
                                      scalars={"RoughScale": 1.0, "NormalStrength": 0.7, "VarAmount": 0.17, "VarHue": 0.9, "VarRough": 0.12,
                                               "WearStrength": 0.55, "WearRoughness": 0.9, "GrimeAmount": 1.0, "GrimeRoughness": 0.45,
                                               "MacroAmount": 0.06}, textures=weathered_textures()),
        "MI_Wood_Piling": dict(vectors={"Tint": (0.055, 0.048, 0.04, 1), "WearColor": (0.12, 0.105, 0.09, 1), "GrimeColor": (0.6, 0.95, 0.45, 1),
                                        "VarTint": (0.92, 1.0, 0.8, 1)},
                               scalars={"RoughScale": 1.0, "NormalStrength": 1.2, "VarAmount": 0.12, "VarHue": 0.6, "VarRough": 0.1,
                                        "WearStrength": 0.5, "GrimeAmount": 1.2, "GrimeRoughness": 0.35}, textures=weathered_textures()),
        "MI_Wood_Planks": dict(vectors={"Tint": (0.14, 0.1, 0.065, 1), "WearColor": (0.3, 0.24, 0.16, 1), "VarTint": (0.95, 1.0, 0.8, 1)},
                               scalars={"RoughScale": 1.0, "VarAmount": 0.18, "VarHue": 0.6, "VarRough": 0.1, "WearStrength": 0.7},
                               textures=weathered_textures()),
    }
    for name, kw in lib.items():
        sb_ue.make_mi(name, MI, surf, **kw)
    sb_ue.make_mi("MI_Rock_Coast", MI, rock)
    sb_ue.make_mi("MI_Stone_Ruin", MI, rock, vectors={"Tint": (0.42, 0.4, 0.36, 1), "LichenColor": (0.4, 0.46, 0.28, 1)},
                  scalars={"Scale": 250.0, "LichenCoverage": 0.35, "NormalStrength": 0.6, "StrataScale": 1.0e6})
    # Engine primitives carry no vertex colours (they read as white): switch the masks off.
    # Sand has its own painted texture set (ripple bands, grains; D29).
    sb_ue.make_mi("MI_Sand", MI, rock, vectors={"Tint": (0.62, 0.55, 0.42, 1), "LichenColor": (0.5, 0.48, 0.36, 1)},
                  textures={"AlbedoTex": TEX + "/Sand/T_Sand_D", "NormalTex": TEX + "/Sand/T_Sand_N", "ORMTex": TEX + "/Sand/T_Sand_ORM"},
                  scalars={"Scale": 400.0, "LichenCoverage": 0.0, "NormalStrength": 0.8, "MacroAmount": 0.25, "WetDarken": 0.7,
                           "VertexMaskAmount": 0.0, "AlgaeAmount": 0.0, "BarnacleAmount": 0.0, "StrataScale": 1.0e6})
    sb_ue.make_mi("MI_SeaGrass", MI, grass)
    sb_ue.make_mi("MI_LanternGlass", MI, glow, vectors={"BaseColor": (0.35, 0.25, 0.14, 1), "GlowColor": (1.0, 0.52, 0.2, 1)}, scalars={"GlowIntensity": 80.0, "Roughness": 0.1})
    sb_ue.make_mi("MI_CandleFlame", MI, glow, vectors={"BaseColor": (0.9, 0.7, 0.4, 1), "GlowColor": (1.0, 0.62, 0.28, 1)}, scalars={"GlowIntensity": 600.0})
    sb_ue.make_mi("MI_AncientGlow", MI, glow, vectors={"BaseColor": (0.05, 0.12, 0.12, 1), "GlowColor": (0.35, 0.95, 0.85, 1)}, scalars={"GlowIntensity": 40.0})
    print("SB_SURFACE_MATERIALS_OK", mel.get_statistics(surf).num_pixel_shader_instructions, mel.get_statistics(rock).num_pixel_shader_instructions)


if __name__ == "__main__":
    main()
