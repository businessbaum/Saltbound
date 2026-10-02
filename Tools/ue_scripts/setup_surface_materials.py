"""Master materials and the initial material library (script-owned; safe to re-run).

  M_SB_Surface  wood / paint / metal / rope: tinted neutral textures, per-part variation (vertex colour B),
                world macro variation, edge wear (vertex colour R + wear mask), grime/wet (vertex colour G)
  M_SB_Rock     world-aligned triplanar rock, lichen on top faces, wet darkening near the waterline
  M_SB_Glow     emissive (lantern glass, flame, ancient technology accents)
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
float Rough = saturate(ORM.g * RoughScale + RoughOffset);
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
    m = sb_ue.recreate_asset("M_SB_Surface", MASTER, unreal.Material, unreal.MaterialFactoryNew())
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
float3 W = pow(abs(N), 4.0); W /= (W.x + W.y + W.z);
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
float3 Base = Tint * Alb * TexNormalize;
float Macro = Texture2DSample(MacroTex, MacroTexSampler, P.xy / MacroScale).r;
Base *= lerp(1.0 - MacroAmount, 1.0 + MacroAmount, Macro);
// Lichen / moss on faces pointing up, patchy.
float Lichen = saturate((WN.z - LichenUp) / max(1.0 - LichenUp, 0.01)) * saturate((Macro - (1.0 - LichenCoverage)) * 4.0);
Lichen *= saturate((P.z - WaterLevel - LichenMinHeight) / 100.0);
Base = lerp(Base, LichenColor.rgb * Alb * TexNormalize, Lichen);
// Wet band above the waterline: darker and smoother.
float Wet = saturate(1.0 - (P.z - WaterLevel) / max(WetHeight, 1.0));
Base *= lerp(1.0, WetDarken, Wet);
float Rough = lerp(O.y * RoughScale, WetRoughness, Wet);
RoughOut = Rough;
AOOut = O.x;
NormalOut = WN;
return Base;
"""


def build_rock():
    m = sb_ue.recreate_asset("M_SB_Rock", MASTER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("tangent_space_normal", False)
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1800, 0)
    wn = sb_ue.expr(m, unreal.MaterialExpressionVertexNormalWS, -1800, 100)
    texobj = {
        "AlbedoTex": (TEX + "/Rock/T_Rock_D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        "NormalTex": (TEX + "/Rock/T_Rock_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        "ORMTex": (TEX + "/Rock/T_Rock_ORM", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR),
        "MacroTex": (TEX + "/Common/T_MacroNoise_M", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR),
    }
    tnodes = {}
    for i, (n, (p, st)) in enumerate(texobj.items()):
        tnodes[n] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1800, 250 + i * 120,
                               parameter_name=n, texture=tex(p), sampler_type=st, group="Textures")
    params = {
        "Scale": ("s", 700.0, "Textures"), "NormalStrength": ("s", 0.7, "Textures"),
        "Tint": ("v", (0.2, 0.21, 0.21, 1), "Color"), "TexNormalize": ("s", 2.0, "Color"),
        "MacroScale": ("s", 1500.0, "Color"), "MacroAmount": ("s", 0.18, "Color"),
        "LichenColor": ("v", (0.55, 0.5, 0.3, 1), "Lichen"), "LichenUp": ("s", 0.55, "Lichen"),
        "LichenCoverage": ("s", 0.45, "Lichen"), "LichenMinHeight": ("s", 120.0, "Lichen"),
        "WaterLevel": ("s", 0.0, "Wet"), "WetHeight": ("s", 90.0, "Wet"), "WetDarken": ("s", 0.45, "Wet"),
        "WetRoughness": ("s", 0.3, "Wet"), "RoughScale": ("s", 1.0, "Surface"),
    }
    nodes = {}
    for i, (name, (kind, val, group)) in enumerate(params.items()):
        nodes[name] = (sb_ue.vector_param if kind == "v" else sb_ue.scalar_param)(m, name, val, -1300, -400 + i * 70, group)
    inputs = ["P", "N"] + list(texobj) + list(params)
    c = custom(m, -800, 200, "SB rock triplanar", ROCK_CODE, inputs, F3, [("RoughOut", F1), ("AOOut", F1), ("NormalOut", F3)])
    sb_ue.connect(wp, "", c, "P")
    sb_ue.connect(wn, "", c, "N")
    for n in texobj:
        sb_ue.connect(tnodes[n], "", c, n)
    for n in params:
        sb_ue.connect(nodes[n], "", c, n)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(c, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(c, "AOOut", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    sb_ue.to_prop(c, "NormalOut", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


# --- M_SB_Glow -------------------------------------------------------------------------------------
def build_glow():
    m = sb_ue.recreate_asset("M_SB_Glow", MASTER, unreal.Material, unreal.MaterialFactoryNew())
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


def paint_textures():
    return {"BaseTexture": TEX + "/Paint/T_PaintSurface_D", "NormalTexture": TEX + "/Paint/T_PaintSurface_N",
            "ORMTexture": TEX + "/Paint/T_PaintSurface_ORM"}


def canvas_textures():
    return {"BaseTexture": TEX + "/Fabric/T_Canvas_D", "NormalTexture": TEX + "/Fabric/T_Canvas_N",
            "ORMTexture": TEX + "/Paint/T_PaintSurface_ORM"}


def wood_textures():
    return {"BaseTexture": TEX + "/Wood/T_WoodGrain_D", "NormalTexture": TEX + "/Wood/T_WoodGrain_N",
            "ORMTexture": TEX + "/Wood/T_WoodGrain_ORM"}


def main():
    surf = build_surface()
    rock = build_rock()
    glow = build_glow()
    metal_tex = {"NormalTexture": TEX + "/Metal/T_Metal_N", "ORMTexture": TEX + "/Metal/T_Metal_ORM"}
    rope_tex = {"BaseTexture": TEX + "/Fabric/T_Rope_D", "NormalTexture": TEX + "/Fabric/T_Rope_N"}
    lib = {
        # Boat: warm, cared for
        "MI_Wood_Teak": dict(vectors={"Tint": (0.26, 0.135, 0.065, 1), "WearColor": (0.42, 0.24, 0.11, 1), "GrimeColor": (0.55, 0.45, 0.38, 1)},
                             scalars={"RoughScale": 0.95, "WearStrength": 0.9, "GrimeAmount": 0.8}, textures=wood_textures()),
        "MI_Wood_Varnish": dict(vectors={"Tint": (0.12, 0.05, 0.022, 1), "WearColor": (0.3, 0.14, 0.05, 1)},
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
        "MI_Canvas": dict(vectors={"Tint": (0.62, 0.58, 0.5, 1)}, scalars={"TexNormalize": 2.6, "NormalStrength": 0.8, "RoughScale": 1.6, "WearStrength": 0.0, "UVScale": 2.0}, textures=canvas_textures()),
        # Harbour: weathered, cooler
        "MI_Wood_DockWeathered": dict(vectors={"Tint": (0.15, 0.14, 0.125, 1), "WearColor": (0.3, 0.29, 0.27, 1), "GrimeColor": (0.18, 0.24, 0.12, 1), "VarTint": (0.9, 0.95, 1.0, 1)},
                                      scalars={"RoughScale": 1.3, "NormalStrength": 1.4, "VarAmount": 0.18, "VarHue": 0.6, "GrimeAmount": 1.2, "GrimeRoughness": 0.45}, textures=wood_textures()),
        "MI_Wood_Piling": dict(vectors={"Tint": (0.06, 0.05, 0.04, 1), "WearColor": (0.18, 0.15, 0.12, 1), "GrimeColor": (0.6, 1.1, 0.5, 1)},
                               scalars={"RoughScale": 1.1, "NormalStrength": 1.6, "GrimeAmount": 1.3, "GrimeRoughness": 0.3}, textures=wood_textures()),
        "MI_Wood_Planks": dict(vectors={"Tint": (0.3, 0.2, 0.12, 1), "WearColor": (0.4, 0.32, 0.22, 1)}, scalars={"RoughScale": 1.2}, textures=wood_textures()),
    }
    for name, kw in lib.items():
        sb_ue.make_mi(name, MI, surf, **kw)
    sb_ue.make_mi("MI_Rock_Coast", MI, rock)
    sb_ue.make_mi("MI_Stone_Ruin", MI, rock, vectors={"Tint": (0.42, 0.4, 0.36, 1), "LichenColor": (0.4, 0.46, 0.28, 1)},
                  scalars={"Scale": 250.0, "LichenCoverage": 0.35, "NormalStrength": 0.6})
    sb_ue.make_mi("MI_Sand", MI, rock, vectors={"Tint": (0.62, 0.55, 0.42, 1), "LichenColor": (0.5, 0.48, 0.36, 1)},
                  scalars={"Scale": 900.0, "LichenCoverage": 0.0, "NormalStrength": 0.25, "MacroAmount": 0.25, "WetDarken": 0.7})
    sb_ue.make_mi("MI_LanternGlass", MI, glow, vectors={"BaseColor": (0.35, 0.25, 0.14, 1), "GlowColor": (1.0, 0.52, 0.2, 1)}, scalars={"GlowIntensity": 80.0, "Roughness": 0.1})
    sb_ue.make_mi("MI_CandleFlame", MI, glow, vectors={"BaseColor": (0.9, 0.7, 0.4, 1), "GlowColor": (1.0, 0.62, 0.28, 1)}, scalars={"GlowIntensity": 600.0})
    sb_ue.make_mi("MI_AncientGlow", MI, glow, vectors={"BaseColor": (0.05, 0.12, 0.12, 1), "GlowColor": (0.35, 0.95, 0.85, 1)}, scalars={"GlowIntensity": 40.0})
    print("SB_SURFACE_MATERIALS_OK", mel.get_statistics(surf).num_pixel_shader_instructions, mel.get_statistics(rock).num_pixel_shader_instructions)


if __name__ == "__main__":
    main()
