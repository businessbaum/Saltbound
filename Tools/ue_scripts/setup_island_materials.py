"""The home island's materials (D71, script-owned; safe to re-run): its ground (M_SB_IslandGround, MI_HomeGround) and its
plants (M_SB_IslFoliage, M_SB_IslBark and the MI_Isl* instances that build_vegetation.py's slots name).

Owner (2026-10-06): "the floor is good colored but lacks EVERYTHING ELSE ... no real texture everything is perfectly flat,
there is no good transition between grass and sand ... Keypoints are a lack of surface texture and material realism,
Microtexture, Surface roughness."

One surface for the whole island (SM_Home_Ground, build_home_island.py) instead of D60's sand under a separate turf with a
lip. Five sets, projected from the top in world space (gen_island_textures.py; the sea floor keeps D29's painted sand):
sea floor -> beach sand (by height above the water) -> path earth (vertex B) -> grass (vertex R) -> forest litter (vertex
G). Each step is a height blend: the vertex weight sets where the edge runs, the sets' heights (ORM.b) decide what shows
along it, so sand settles between the grass's clumps, the clumps stand out of the sand and the litter heaps over the grass.

  Sand      wet and glossy within ~25 cm of the water (a wavy line), damp and darker up to 1.5 m (as D60's wet band), dry
            and matte above; the mesh's sculpted ripples light their crests and shade their troughs (UV1.x: 1 crest,
            0 trough, 0.5 neither).
  Ground    curvature (UV1.y: 0.5 flat, higher on rises, lower in hollows) lights rises and darkens hollows; per-area tone
            (vertex A) and a world macro texture vary the colour; a larger, rotated second sample of the sand and the grass
            hides their tiling.
  Colours   calibrated so each set's mean matches D60's look (owner: "The colors are fine").

The plants (owner: "the trees have 0 detail"): D60's foliage masters (M_SB_Foliage / M_SB_FoliageBark in
setup_surface_materials.py) stay as they are; the island's plants use their own copies with the same vertex-mask colours and the
same wind (the code is shared), plus detail textures through the UVs: bark relief (palm rings and fibres, broadleaf plates
and furrows) and leaf veins, with their normals and roughness. The instances keep D60's palette.

Run in the editor (after import_textures.py Island):
    python Tools/ue_exec.py Tools/ue_scripts/setup_island_materials.py [ground] [plants]
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402
import setup_surface_materials as ssm  # noqa: E402  (shared helpers: custom nodes, caustics; nothing is rebuilt there)

importlib.reload(sb_ue)

MASTER = "/Game/Saltbound/Materials/Master"
MI = "/Game/Saltbound/Materials/Instances"
TEX = "/Game/Saltbound/Materials/Textures"
ISL = TEX + "/Island"
mel = sb_ue.mel
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

GROUND_CODE = """
float2 XY = P.xy;
float Z = P.z - WaterLevel;
float3 M = Texture2DSample(MacroTex, MacroTexSampler, XY / MacroScale).rgb;
float3 M2 = Texture2DSample(MacroTex, MacroTexSampler, XY / BreakupScale + 0.37).rgb;
// Away from the eye the painted detail is averaged a little sooner (a mip bias), so mid-distance grass does not shimmer.
float Dist = length(P - CamPos);
float MipB = DistBias * saturate((Dist - DistStart) / DistRange);
// Under the lawn's blades (within their reach) the painted grass is a shade darker, the gaps between the blades.
float Under = lerp(GrassUnder, 1.0, saturate((Dist - LawnFadeStart) / LawnFadeRange));

// --- the sets (top projection, each its own tile size in cm) ---
float2 uS = XY / SandScale, uG = XY / GrassScale, uL = XY / LitterScale, uE = XY / EarthScale, uF = XY / FloorScale;
float3 SA = Texture2DSampleBias(SandD, SandDSampler, uS, MipB).rgb;
float3 SO = Texture2DSample(SandORM, SandORMSampler, uS).rgb;
float2 SN = Texture2DSample(SandN, SandNSampler, uS).xy * 2 - 1;
float3 GA = Texture2DSampleBias(GrassD, GrassDSampler, uG, MipB).rgb;
float3 GO = Texture2DSample(GrassORM, GrassORMSampler, uG).rgb;
float2 GN = Texture2DSample(GrassN, GrassNSampler, uG).xy * 2 - 1;
float3 LA = Texture2DSampleBias(LitterD, LitterDSampler, uL, MipB).rgb;
float3 LO = Texture2DSample(LitterORM, LitterORMSampler, uL).rgb;
float2 LN = Texture2DSample(LitterN, LitterNSampler, uL).xy * 2 - 1;
float3 EA = Texture2DSampleBias(EarthD, EarthDSampler, uE, MipB).rgb;
float3 EO = Texture2DSample(EarthORM, EarthORMSampler, uE).rgb;
float2 EN = Texture2DSample(EarthN, EarthNSampler, uE).xy * 2 - 1;
float3 FA = Texture2DSample(FloorD, FloorDSampler, uF).rgb;
float2 FO = Texture2DSample(FloorORM, FloorORMSampler, uF).rg;
float2 FN = Texture2DSample(FloorN, FloorNSampler, uF).xy * 2 - 1;

// Anti-tiling: the sand and the grass once more, larger and turned by 37 degrees; their ratio to the set's mean varies
// the colour in broad, crisp-edged areas (the detail keeps its sharpness).
const float C = 0.799, S = 0.602;
float2 uSR = float2(C * uS.x - S * uS.y, S * uS.x + C * uS.y) * (1.0 / AntiTileScale) + 0.31;
float2 uGR = float2(C * uG.x - S * uG.y, S * uG.x + C * uG.y) * (1.0 / AntiTileScale) + 0.53;
float3 SR = Texture2DSample(SandD, SandDSampler, uSR).rgb;
float3 GR = Texture2DSample(GrassD, GrassDSampler, uGR).rgb;
SA *= lerp(1.0, SR / max(SandMean.rgb, 0.01), AntiTile);
GA *= lerp(1.0, GR / max(GrassMean.rgb, 0.01), AntiTile);

// --- sand: wet, damp, dry; the sculpted ripples' crests and troughs; the sea floor below ---
float WetLine = WetTop + WetWobble * (M2.g - 0.5) * 2.0;
float Wet = saturate((WetLine - Z) / WetFade);
float Damp = saturate(1.0 - (Z - DampTop) / DampFade);
float3 Sand = SA * SandTint.rgb;
float Crest = saturate(UV1.x * 2.0 - 1.0), Trough = saturate(1.0 - UV1.x * 2.0);
Sand *= lerp(1.0, CrestLighten, Crest) * lerp(1.0, TroughDarken, Trough);
Sand *= lerp(1.0, DampDarken, Damp);
Sand *= lerp(1.0, WetDarken, Wet);
float SandRough = lerp(SO.g * SandRoughScale, WetRough, Wet);
float2 SandN2 = SN * SandNormal * lerp(1.0, WetNormal, Wet);
float Beach = smoothstep(-35.0, 5.0, Z);
float3 Floor = lerp(0.5, FA, FloorContrast) * FloorTint.rgb;
Floor *= lerp(1.0, CrestLighten, Crest) * lerp(1.0, TroughDarken, Trough) * WetDarken;
float3 Alb = lerp(Floor, Sand, Beach);
float Rough = lerp(lerp(FO.y, WetRough, 0.85), SandRough, Beach);
float2 Nrm = lerp(FN * FloorNormal, SandN2, Beach);
float AOv = lerp(1.0, SO.r, Beach);
float H = SO.b;

// --- the land's cover: earth (B), grass (R), litter (G), each height-blended over what lies below ---
// A breakup at 0.3-1 m bends the edges between the mesh's vertices into clumps (only where a weight is partial).
float Br = (M2.g - 0.5) * 2.0 * Breakup;
float wE = saturate(VC.b + Br * 4.0 * VC.b * (1.0 - VC.b));
float wG = saturate(VC.r + Br * 4.0 * VC.r * (1.0 - VC.r));
float wL = saturate(VC.g - Br * 4.0 * VC.g * (1.0 - VC.g));
float s = HeightSoft;
float a;
a = saturate(((EO.b - H) * HeightGain + (2.0 * wE - 1.0) * (1.0 + s)) / (2.0 * s) + 0.5);
Alb = lerp(Alb, EA * EarthTint.rgb, a); Rough = lerp(Rough, EO.g, a); Nrm = lerp(Nrm, EN * EarthNormal, a); AOv = lerp(AOv, EO.r, a);
H = lerp(H, EO.b, a);
a = saturate(((GO.b - H) * HeightGain + (2.0 * wG - 1.0) * (1.0 + s)) / (2.0 * s) + 0.5);
Alb = lerp(Alb, GA * GrassTint.rgb * Under, a); Rough = lerp(Rough, saturate(GO.g * GrassRough), a); Nrm = lerp(Nrm, GN * GrassNormal, a); AOv = lerp(AOv, GO.r, a);
H = lerp(H, GO.b, a);
float Grass = a;
a = saturate(((LO.b - H) * HeightGain + (2.0 * wL - 1.0) * (1.0 + s)) / (2.0 * s) + 0.5);
Alb = lerp(Alb, LA * LitterTint.rgb, a); Rough = lerp(Rough, LO.g, a); Nrm = lerp(Nrm, LN * LitterNormal, a); AOv = lerp(AOv, LO.r, a);

// --- painted light and variation ---
Alb *= lerp(1.0 - MacroAmount, 1.0 + MacroAmount, M.r);
Alb *= lerp(1.0 - ToneVar, 1.0 + ToneVar, VA);
float Curv = (UV1.y - 0.5) * 2.0;
Alb *= 1.0 + RiseLighten * saturate(Curv) - HollowDarken * saturate(-Curv);
AOv *= 1.0 - HollowAO * saturate(-Curv);
float3 WN = normalize(N + float3(Nrm.x, Nrm.y, 0.0) * NormalStrength * saturate(N.z * 2.0));
Alb *= 1.0 + TopLighten * saturate(WN.z);
RoughOut = saturate(Rough * RoughScale);
AOOut = lerp(1.0, AOv, AOStrength);
NormalOut = WN;
return Alb;
"""

TEXTURES = {
    "SandD": (ISL + "/T_IslSand_D", "c"), "SandN": (ISL + "/T_IslSand_N", "n"), "SandORM": (ISL + "/T_IslSand_ORM", "l"),
    "GrassD": (ISL + "/T_IslGrass_D", "c"), "GrassN": (ISL + "/T_IslGrass_N", "n"), "GrassORM": (ISL + "/T_IslGrass_ORM", "l"),
    "LitterD": (ISL + "/T_IslLitter_D", "c"), "LitterN": (ISL + "/T_IslLitter_N", "n"), "LitterORM": (ISL + "/T_IslLitter_ORM", "l"),
    "EarthD": (ISL + "/T_IslEarth_D", "c"), "EarthN": (ISL + "/T_IslEarth_N", "n"), "EarthORM": (ISL + "/T_IslEarth_ORM", "l"),
    "FloorD": (TEX + "/Sand/T_Sand_D", "c"), "FloorN": (TEX + "/Sand/T_Sand_N", "n"), "FloorORM": (TEX + "/Sand/T_Sand_ORM", "l"),
    "MacroTex": (TEX + "/Common/T_MacroNoise_M", "l"),
}

# The sets' mean colours (linear; gen_island_textures.py prints them) for the anti-tiling ratio.
PARAMS = {
    "WaterLevel": ("s", 0.0, "Water"),
    "SandScale": ("s", 200.0, "Scale"), "GrassScale": ("s", 160.0, "Scale"), "LitterScale": ("s", 200.0, "Scale"),
    "EarthScale": ("s", 200.0, "Scale"), "FloorScale": ("s", 550.0, "Scale"), "MacroScale": ("s", 1800.0, "Scale"),
    "BreakupScale": ("s", 420.0, "Scale"), "AntiTileScale": ("s", 4.3, "Scale"), "AntiTile": ("s", 0.45, "Scale"),
    "SandMean": ("v", (0.552, 0.457, 0.312, 1), "Scale"), "GrassMean": ("v", (0.087, 0.156, 0.0144, 1), "Scale"),
    "SandTint": ("v", (0.89, 0.94, 1.03, 1), "Colour"), "GrassTint": ("v", (0.73, 0.98, 0.78, 1), "Colour"),
    "LitterTint": ("v", (0.62, 0.64, 0.6, 1), "Colour"), "EarthTint": ("v", (1.05, 1.12, 1.25, 1), "Colour"),
    "FloorTint": ("v", (1.46, 1.3, 0.99, 1), "Colour"), "FloorContrast": ("s", 0.6, "Colour"),
    "MacroAmount": ("s", 0.16, "Colour"), "ToneVar": ("s", 0.1, "Colour"), "TopLighten": ("s", 0.0, "Colour"),
    "CrestLighten": ("s", 1.18, "Colour"), "TroughDarken": ("s", 0.8, "Colour"),
    "RiseLighten": ("s", 0.12, "Colour"), "HollowDarken": ("s", 0.2, "Colour"), "HollowAO": ("s", 0.3, "Colour"),
    "WetTop": ("s", 22.0, "Wet"), "WetWobble": ("s", 10.0, "Wet"), "WetFade": ("s", 12.0, "Wet"),
    "WetDarken": ("s", 0.66, "Wet"), "WetRough": ("s", 0.3, "Wet"), "WetNormal": ("s", 0.45, "Wet"),
    "DampTop": ("s", 70.0, "Wet"), "DampFade": ("s", 80.0, "Wet"), "DampDarken": ("s", 0.84, "Wet"),
    "SandRoughScale": ("s", 1.0, "Surface"), "RoughScale": ("s", 1.0, "Surface"), "AOStrength": ("s", 1.0, "Surface"),
    "NormalStrength": ("s", 1.0, "Surface"), "SandNormal": ("s", 1.0, "Surface"), "GrassNormal": ("s", 1.0, "Surface"),
    "LitterNormal": ("s", 1.0, "Surface"), "EarthNormal": ("s", 1.0, "Surface"), "FloorNormal": ("s", 0.5, "Surface"),
    "HeightSoft": ("s", 0.08, "Blend"), "HeightGain": ("s", 0.7, "Blend"), "Breakup": ("s", 0.45, "Blend"),
    "DistBias": ("s", 1.2, "Scale"), "DistStart": ("s", 600.0, "Scale"), "DistRange": ("s", 2500.0, "Scale"),
    "GrassUnder": ("s", 0.86, "Lawn"), "GrassRough": ("s", 1.5, "Surface"), "LawnFadeStart": ("s", 3500.0, "Lawn"), "LawnFadeRange": ("s", 2500.0, "Lawn"),
}


def configure_textures():
    """BC7 for the coloured island sets (crisp painted detail; BC1 bands the greens). Normal maps and ORM keep the import's
    settings."""
    for key, (path, kind) in TEXTURES.items():
        if not path.startswith(ISL) or kind != "c":
            continue
        t = unreal.load_asset(path)
        if t.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_BC7:
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
            sb_ue.eal.save_loaded_asset(t)
            print("SB_GROUND_TEX BC7", path)


def build_ground():
    m = sb_ue.reset_material("M_SB_IslandGround", MASTER)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("used_with_nanite", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)      # the crab burrows (instanced) wear it too
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -2000, 0)
    wn = sb_ue.expr(m, unreal.MaterialExpressionVertexNormalWS, -2000, 80)
    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraPositionWS, -2200, 80)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -2000, 160)
    uv1 = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -2000, 240, coordinate_index=1)
    sampler = {"c": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "n": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
               "l": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}
    tn = {}
    for i, (key, (path, kind)) in enumerate(TEXTURES.items()):
        tn[key] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -2000, 420 + i * 110, parameter_name=key,
                             texture=unreal.load_asset(path), sampler_type=sampler[kind], group="Textures",
                             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    pn = {}
    for i, (name, (kind, val, group)) in enumerate(PARAMS.items()):
        pn[name] = (sb_ue.vector_param if kind == "v" else sb_ue.scalar_param)(m, name, val, -1500, -600 + i * 60, group)
    inputs = ["P", "N", "VC", "VA", "UV1", "CamPos"] + list(TEXTURES) + list(PARAMS)
    c = ssm.custom(m, -900, 200, "SB island ground", GROUND_CODE, inputs, F3, [("RoughOut", F1), ("AOOut", F1), ("NormalOut", F3)])
    sb_ue.connect(wp, "", c, "P")
    sb_ue.connect(wn, "", c, "N")
    sb_ue.connect(vc, "", c, "VC")
    sb_ue.connect(vc, "A", c, "VA")
    sb_ue.connect(uv1, "", c, "UV1")
    sb_ue.connect(cam, "", c, "CamPos")
    for key in TEXTURES:
        sb_ue.connect(tn[key], "", c, key)
    for name in PARAMS:
        sb_ue.connect(pn[name], "", c, name)
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(c, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(c, "AOOut", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    sb_ue.to_prop(c, "NormalOut", unreal.MaterialProperty.MP_NORMAL)
    ssm.add_caustics(m, c, wp, wn, pn["WaterLevel"], -300, 1100)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


# --- the plants (D71) --------------------------------------------------------------------------------------------
PLANT_COLOR_CODE = """
float3 Fresh = lerp(DarkColor.rgb, LightColor.rgb, saturate(VC.r));
float3 Old = lerp(DryDark.rgb, DryLight.rgb, saturate(VC.r));
float3 C = lerp(Fresh, Old, saturate(VC.g));
C *= lerp(1.0 - PartVar, 1.0 + PartVar, VC.b);
float R2 = frac(InstRand * 13.37);
C *= lerp(1.0 - InstVar, 1.0 + InstVar, InstRand);
C *= lerp(float3(1.0 + HueShift, 1.0, 1.0 - HueShift), float3(1.0 - HueShift, 1.0, 1.0 + HueShift), R2);
// The detail texture (bark relief, leaf veins): 0.5 leaves the colour as it is.
float3 D = Texture2DSample(DetailD, DetailDSampler, UV * DetailScale.xy).rgb;
C *= lerp(1.0, D * 2.0, DetailAmount);
// D71 lawn: the blades take the ground's own grass colour where they stand (the same texture, its anti-tiling and the
// world macro variation as M_SB_IslandGround), so they melt into the ground; off (0) for every other plant.
if (GroundMatch > 0.0)
{
    float2 uG = WP.xy / GroundScale;
    float3 GA = Texture2DSampleBias(GroundD, GroundDSampler, uG, GroundBlur).rgb;
    const float Cc = 0.799, Ss = 0.602;
    float2 uGR = float2(Cc * uG.x - Ss * uG.y, Ss * uG.x + Cc * uG.y) * (1.0 / 4.3) + 0.53;
    float3 GR = Texture2DSample(GroundD, GroundDSampler, uGR).rgb;
    GA *= lerp(1.0, GR / max(GroundMean.rgb, 0.01), 0.45);
    float Mr = Texture2DSample(MacroTex, MacroTexSampler, WP.xy / 1800.0).r;
    GA *= lerp(0.84, 1.16, Mr);
    C = lerp(C, C * GA / max(GroundMean.rgb, 0.01), GroundMatch);
}
float3 O = Texture2DSample(DetailORM, DetailORMSampler, UV * DetailScale.xy).rgb;
RoughOut = saturate(lerp(Roughness, O.g, DetailAmount) * RoughScale);
AOOut = lerp(1.0, O.r, DetailAmount * AOAmount);
return C;
"""

PLANT_PARAMS = dict(ssm.FOLIAGE_PARAMS)
PLANT_PARAMS.update({"DetailScale": ("v", (1.0, 1.0, 0.0, 0.0), "Detail"), "DetailAmount": ("s", 1.0, "Detail"),
                     "NormalAmount": ("s", 1.0, "Detail"), "RoughScale": ("s", 1.0, "Detail"), "AOAmount": ("s", 1.0, "Detail"),
                     "GroundMatch": ("s", 0.0, "Ground"), "GroundScale": ("s", 160.0, "Ground"), "GroundBlur": ("s", 3.0, "Ground"),
                     "GroundMean": ("v", (0.087, 0.156, 0.0144, 1), "Ground")})
for _k in ("RibDark", "Grain"):
    PLANT_PARAMS.pop(_k, None)


def build_plant_master(name, leaves, detail):
    """M_SB_IslFoliage (two-sided foliage) / M_SB_IslBark: D60's vertex-mask colours and wind, plus detail textures."""
    m = sb_ue.reset_material(name, MASTER)
    for prop, value in (("two_sided", leaves), ("used_with_instanced_static_meshes", True), ("used_with_nanite", True)):
        m.set_editor_property(prop, value)
    if leaves:
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property("max_world_position_offset_displacement", 60.0)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -1500, 0)
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -1500, 100)
    rand = sb_ue.expr(m, unreal.MaterialExpressionPerInstanceRandom, -1500, 180)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -1500, 260)
    pivot = sb_ue.expr(m, unreal.MaterialExpressionObjectPositionWS, -1500, 340)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1500, 420)
    sampler = {"DetailD": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "DetailN": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
               "DetailORM": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}
    tn = {}
    for i, key in enumerate(("DetailD", "DetailN", "DetailORM")):
        tn[key] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1500, 520 + i * 110, parameter_name=key,
                             texture=unreal.load_asset(ISL + "/T_Isl%s_%s" % (detail, key[6:])), sampler_type=sampler[key],
                             group="Detail")
    tn["GroundD"] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1500, 860, parameter_name="GroundD",
                               texture=unreal.load_asset(ISL + "/T_IslGrass_D"), sampler_type=sampler["DetailD"], group="Ground")
    tn["MacroTex"] = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -1500, 970, parameter_name="MacroTex",
                                texture=unreal.load_asset(TEX + "/Common/T_MacroNoise_M"),
                                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, group="Ground")
    wpos = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -1500, 1080)
    pn = {}
    for i, (n, (k, v, g)) in enumerate(PLANT_PARAMS.items()):
        pn[n] = (sb_ue.vector_param if k == "v" else sb_ue.scalar_param)(m, n, v, -1150, -400 + i * 60, g)
    color_in = ["VC", "UV", "InstRand", "DetailD", "DetailORM", "DarkColor", "LightColor", "DryDark", "DryLight", "PartVar",
                "InstVar", "HueShift", "DetailScale", "DetailAmount", "Roughness", "RoughScale", "AOAmount", "WP", "GroundD",
                "MacroTex", "GroundMatch", "GroundScale", "GroundBlur", "GroundMean"]
    col = ssm.custom(m, -650, 0, "Island plant colour", PLANT_COLOR_CODE, color_in, F3, [("RoughOut", F1), ("AOOut", F1)])
    links = {"VC": (vc, ""), "UV": (uv, ""), "InstRand": (rand, ""), "DetailD": (tn["DetailD"], ""), "DetailORM": (tn["DetailORM"], ""),
             "WP": (wpos, ""), "GroundD": (tn["GroundD"], ""), "MacroTex": (tn["MacroTex"], "")}
    for n in color_in:
        a, out = links.get(n, (pn.get(n), ""))
        sb_ue.connect(a, out, col, n)
    # The detail normal (tangent space, from the plant's UVs), faded by NormalAmount.
    scale_uv = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -1150, 900)
    sb_ue.connect(uv, "", scale_uv, "A")
    mask = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -1300, 960, r=True, g=True, b=False, a=False)
    sb_ue.connect(pn["DetailScale"], "", mask, "")
    sb_ue.connect(mask, "", scale_uv, "B")
    nsamp = sb_ue.expr(m, unreal.MaterialExpressionTextureSample, -900, 900, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    sb_ue.connect(tn["DetailN"], "", nsamp, "Tex")
    sb_ue.connect(scale_uv, "", nsamp, "UVs")
    flat = sb_ue.expr(m, unreal.MaterialExpressionConstant3Vector, -900, 1050, constant=unreal.LinearColor(0, 0, 1, 1))
    nl = sb_ue.expr(m, unreal.MaterialExpressionLinearInterpolate, -650, 950)
    sb_ue.connect(flat, "", nl, "A")
    sb_ue.connect(nsamp, "RGB", nl, "B")
    sb_ue.connect(pn["NormalAmount"], "", nl, "Alpha")
    sb_ue.to_prop(nl, "", unreal.MaterialProperty.MP_NORMAL)
    wpo_in = ["LocalPos", "Pivot", "Time", "VA", "InstRand", "BendHeight", "BendAmount", "BendSpeed", "FlutterAmount", "FlutterSpeed",
              "WindX", "WindY"]
    wpo = ssm.custom(m, -650, 500, "Foliage wind (cm)", ssm.FOLIAGE_WPO_CODE, wpo_in, F3)
    links = {"LocalPos": (lp, ""), "Pivot": (pivot, ""), "Time": (time, ""), "VA": (vc, "A"), "InstRand": (rand, "")}
    for n in wpo_in:
        a, out = links.get(n, (pn.get(n), ""))
        sb_ue.connect(a, out, wpo, n)
    sb_ue.to_prop(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(col, "RoughOut", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(col, "AOOut", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    sb_ue.to_prop(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    sb_ue.to_prop(pn["Specular"], "", unreal.MaterialProperty.MP_SPECULAR)
    if leaves:
        sss = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -300, 300)
        sb_ue.connect(pn["SubsurfaceColor"], "", sss, "A")
        sb_ue.connect(col, "", sss, "B")
        sb_ue.to_prop(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    return m


def tex_set(name):
    return {"DetailD": ISL + "/T_Isl%s_D" % name, "DetailN": ISL + "/T_Isl%s_N" % name, "DetailORM": ISL + "/T_Isl%s_ORM" % name}


def plants():
    leaf = build_plant_master("M_SB_IslFoliage", True, "LeafBroad")
    bark = build_plant_master("M_SB_IslBark", False, "BarkTree")
    # A plant's bark and leaves share its bend (BendHeight / Amount / Speed), so the crown stays on its trunk (D60's values).
    palm_wind = {"BendHeight": 900.0, "BendAmount": 32.0, "BendSpeed": 0.55}
    tree_wind = {"BendHeight": 900.0, "BendAmount": 12.0, "BendSpeed": 0.45}
    low_wind = {"BendHeight": 120.0, "BendAmount": 3.0, "BendSpeed": 0.8}
    # (parent, textures, scalars, vectors): D60's colours (setup_surface_materials.foliage_library), the roots' and the fresh
    # wood's colours on the bark's "age" (vertex G), the nuts' greens and browns.
    lib = {
        "MI_IslLeaf_Palm": (leaf, "LeafPalm", dict(palm_wind, FlutterAmount=7.0, FlutterSpeed=2.2, NormalAmount=0.8), {
            "DarkColor": (0.035, 0.075, 0.02, 1), "LightColor": (0.2, 0.37, 0.07, 1), "DryDark": (0.12, 0.075, 0.03, 1),
            "DryLight": (0.42, 0.3, 0.13, 1), "SubsurfaceColor": (0.4, 0.55, 0.12, 1)}),
        "MI_IslBark_Palm": (bark, "BarkPalm", dict(palm_wind, FlutterAmount=0.0, Specular=0.25, NormalAmount=1.0), {
            "DarkColor": (0.06, 0.045, 0.035, 1), "LightColor": (0.3, 0.25, 0.18, 1), "DryDark": (0.12, 0.06, 0.03, 1),
            "DryLight": (0.36, 0.2, 0.09, 1)}),
        "MI_IslNut": (bark, "LeafBroad", dict(palm_wind, FlutterAmount=0.0, Specular=0.4, DetailAmount=0.3, NormalAmount=0.3, Roughness=0.45), {
            "DarkColor": (0.05, 0.08, 0.015, 1), "LightColor": (0.22, 0.3, 0.05, 1), "DryDark": (0.08, 0.04, 0.015, 1),
            "DryLight": (0.32, 0.17, 0.06, 1)}),
        "MI_IslBark_Tree": (bark, "BarkTree", dict(tree_wind, FlutterAmount=0.0, Specular=0.25, NormalAmount=1.0), {
            "DarkColor": (0.04, 0.032, 0.026, 1), "LightColor": (0.2, 0.165, 0.13, 1), "DryDark": (0.2, 0.13, 0.07, 1),
            "DryLight": (0.5, 0.36, 0.2, 1)}),
        "MI_IslLeaf_Tree": (leaf, "LeafBroad", dict(tree_wind, FlutterAmount=4.0, FlutterSpeed=2.6, NormalAmount=0.8), {
            "DarkColor": (0.015, 0.045, 0.012, 1), "LightColor": (0.12, 0.27, 0.05, 1), "DryDark": (0.18, 0.15, 0.03, 1),
            "DryLight": (0.5, 0.42, 0.1, 1), "SubsurfaceColor": (0.3, 0.45, 0.08, 1)}),
        "MI_IslLeaf_Bush": (leaf, "LeafBroad", dict(low_wind, FlutterAmount=2.5, FlutterSpeed=2.8, NormalAmount=0.8), {
            "DarkColor": (0.02, 0.055, 0.018, 1), "LightColor": (0.15, 0.31, 0.06, 1), "DryDark": (0.2, 0.12, 0.03, 1),
            "DryLight": (0.55, 0.32, 0.08, 1), "SubsurfaceColor": (0.35, 0.5, 0.1, 1)}),
        "MI_IslFlower": (leaf, "LeafBroad", dict(low_wind, FlutterAmount=2.5, FlutterSpeed=2.8, HueShift=0.1, DetailAmount=0.4, NormalAmount=0.4), {
            "DarkColor": (0.3, 0.03, 0.025, 1), "LightColor": (0.85, 0.12, 0.09, 1), "DryDark": (0.6, 0.4, 0.03, 1),
            "DryLight": (0.95, 0.75, 0.15, 1), "SubsurfaceColor": (0.9, 0.2, 0.15, 1)}),
        "MI_IslGrass_Beach": (leaf, "LeafPalm", {"BendHeight": 60.0, "BendAmount": 6.0, "BendSpeed": 1.1, "FlutterAmount": 2.0,
                                                 "FlutterSpeed": 3.2, "NormalAmount": 0.6}, {
            "DarkColor": (0.09, 0.13, 0.04, 1), "LightColor": (0.38, 0.46, 0.14, 1), "DryDark": (0.3, 0.24, 0.1, 1),
            "DryLight": (0.7, 0.6, 0.32, 1), "SubsurfaceColor": (0.8, 0.8, 0.35, 1)}),
        "MI_IslLeaf_Runner": (leaf, "LeafBroad", {"BendHeight": 30.0, "BendAmount": 0.5, "BendSpeed": 0.8, "FlutterAmount": 1.2,
                                                  "FlutterSpeed": 2.6, "NormalAmount": 0.8, "Roughness": 0.45}, {
            "DarkColor": (0.02, 0.06, 0.018, 1), "LightColor": (0.13, 0.3, 0.06, 1), "DryDark": (0.2, 0.12, 0.03, 1),
            "DryLight": (0.5, 0.36, 0.1, 1), "SubsurfaceColor": (0.3, 0.45, 0.1, 1)}),
    }
    # The Keeper's hut's palm thatch (build_home_props.py): two-sided (the rows are single sheets, seen from under the eaves),
    # straw on both "ages", no wind (a building), no glow through it.
    lib["MI_IslThatch"] = (leaf, "LeafPalm", {"BendAmount": 0.0, "FlutterAmount": 0.0, "NormalAmount": 1.0, "Roughness": 0.85,
                                             "Specular": 0.25, "InstVar": 0.0, "PartVar": 0.1}, {
        "DarkColor": (0.07, 0.05, 0.025, 1), "LightColor": (0.36, 0.27, 0.13, 1), "DryDark": (0.09, 0.06, 0.03, 1),
        "DryLight": (0.4, 0.3, 0.15, 1), "SubsurfaceColor": (0.0, 0.0, 0.0, 1)})
    lib["MI_IslShell"] = (bark, "LeafBroad", {"BendAmount": 0.0, "FlutterAmount": 0.0, "DetailAmount": 0.3, "NormalAmount": 0.3,
                                            "Roughness": 0.45, "Specular": 0.45}, {
        "DarkColor": (0.45, 0.4, 0.33, 1), "LightColor": (0.85, 0.8, 0.72, 1), "DryDark": (0.5, 0.33, 0.28, 1),
        "DryLight": (0.9, 0.7, 0.62, 1)})
    # D71 lawn (owner: "real visual grass small and a lot so it blends in"): short blades in the ground's own greens, dark at the
    # root, lit at the tip, a little wind.
    lib["MI_IslGrass_Lawn"] = (leaf, "LeafPalm", {"BendHeight": 40.0, "BendAmount": 2.5, "BendSpeed": 1.0, "FlutterAmount": 1.2,
                                                 "FlutterSpeed": 3.0, "NormalAmount": 0.4, "Roughness": 0.55, "Specular": 0.3,
                                                 "GroundMatch": 1.0, "PartVar": 0.1, "InstVar": 0.06, "HueShift": 0.04}, {
        "DarkColor": (0.04, 0.09, 0.012, 1), "LightColor": (0.12, 0.27, 0.024, 1), "DryDark": (0.1, 0.08, 0.025, 1),
        "DryLight": (0.36, 0.3, 0.12, 1), "SubsurfaceColor": (0.25, 0.4, 0.06, 1)})
    lib["MI_IslPebble"] = (bark, "LeafBroad", {"BendAmount": 0.0, "FlutterAmount": 0.0, "DetailAmount": 0.15, "NormalAmount": 0.2,
                                             "Roughness": 0.55, "Specular": 0.4, "InstVar": 0.18, "PartVar": 0.2}, {
        "DarkColor": (0.07, 0.065, 0.06, 1), "LightColor": (0.32, 0.3, 0.27, 1), "DryDark": (0.07, 0.065, 0.06, 1),
        "DryLight": (0.32, 0.3, 0.27, 1)})
    # Driftwood (the small sticks on the beaches): silver-grey, bleached, the bark's relief worn smooth.
    lib["MI_IslDriftwood"] = (bark, "BarkTree", {"BendAmount": 0.0, "FlutterAmount": 0.0, "DetailAmount": 0.6, "NormalAmount": 0.6,
                                               "Roughness": 0.8, "Specular": 0.3}, {
        "DarkColor": (0.16, 0.15, 0.13, 1), "LightColor": (0.42, 0.4, 0.36, 1), "DryDark": (0.12, 0.11, 0.09, 1),
        "DryLight": (0.3, 0.28, 0.24, 1)})
    lib["MI_IslSeaweed"] = (leaf, "LeafPalm", {"BendAmount": 0.0, "FlutterAmount": 0.0, "NormalAmount": 0.6, "Roughness": 0.35,
                                             "Specular": 0.5}, {
        "DarkColor": (0.02, 0.025, 0.006, 1), "LightColor": (0.1, 0.11, 0.025, 1), "DryDark": (0.05, 0.035, 0.015, 1),
        "DryLight": (0.22, 0.16, 0.07, 1), "SubsurfaceColor": (0.1, 0.12, 0.02, 1)})
    for name, (parent, texset, scalars, vectors) in lib.items():
        sb_ue.make_mi(name, MI, parent, scalars=scalars, vectors=vectors, textures=tex_set(texset))
    print("SB_PLANT_MATERIALS_OK", mel.get_statistics(leaf).num_pixel_shader_instructions, mel.get_statistics(bark).num_pixel_shader_instructions)


# --- the chimney's smoke (D71) -------------------------------------------------------------------------------------
# SM_Home_Smoke's puffs all sit on its origin (the chimney's mouth); vertex R is each puff's phase, G its size. In a loop
# each puff rises, grows, drifts downwind (the game's wind from 200 degrees) and fades in and out.
SMOKE_WPO_CODE = """
float Ph = frac(Time * Speed + VC.r);
float Grow = 1.0 + Ph * Spread * (0.7 + 0.6 * VC.g);
float2 W = normalize(float2(WindX, WindY));
float3 Drift = float3(W * (Ph * Ph * DriftAmount), 0.0)
             + float3(sin(Time * 0.7 + VC.r * 6.28), cos(Time * 0.53 + VC.r * 4.0), 0.0) * Wobble * Ph;
return LocalPos * (Grow - 1.0) + float3(0.0, 0.0, Ph * Rise) + Drift;
"""

SMOKE_OPACITY_CODE = """
float Ph = frac(Time * Speed + VC.r);
float Fade = smoothstep(0.0, 0.1, Ph) * (1.0 - smoothstep(0.3, 1.0, Ph));
float Edge = pow(saturate(abs(dot(normalize(N), normalize(V)))), EdgeSoft);
return saturate(Density * Fade * Edge);
"""


POOL_CODE = """
float2 UVa = P.xy / Scale + Time * float2(0.011, 0.007);
float2 UVb = P.xy / (Scale * 0.63) - Time * float2(0.006, 0.013);
float2 Na = Texture2DSample(Ripples, RipplesSampler, UVa).xy * 2 - 1;
float2 Nb = Texture2DSample(Ripples, RipplesSampler, UVb).xy * 2 - 1;
return normalize(float3((Na + Nb) * Strength, 1.0));
"""


def pool():
    """Still fresh water (the spring's pool and trickle, the tide pools): dark, clear-looking, glossy, a slow ripple."""
    m = sb_ue.reset_material("M_SB_IslPool", MASTER)
    m.set_editor_property("used_with_nanite", True)
    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -900, 0)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -900, 100)
    rip = sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, -900, 200, parameter_name="Ripples",
                     texture=unreal.load_asset("/Game/Saltbound/Water/Textures/T_OceanRipplesSoft_N"),
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    scale = sb_ue.scalar_param(m, "Scale", 90.0, -900, 320, "Water")
    strength = sb_ue.scalar_param(m, "Strength", 0.12, -900, 380, "Water")
    n = ssm.custom(m, -500, 200, "Pool ripples", POOL_CODE, ["P", "Time", "Ripples", "Scale", "Strength"], F3)
    for k, a in (("P", wp), ("Time", time), ("Ripples", rip), ("Scale", scale), ("Strength", strength)):
        sb_ue.connect(a, "", n, k)
    colour = sb_ue.vector_param(m, "WaterColor", (0.012, 0.035, 0.03, 1), -500, -100, "Water")
    rough = sb_ue.scalar_param(m, "Roughness", 0.04, -500, 0, "Water")
    spec = sb_ue.scalar_param(m, "Specular", 0.5, -500, 60, "Water")
    sb_ue.to_prop(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    sb_ue.to_prop(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    sb_ue.to_prop(n, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    sb_ue.make_mi("MI_IslPool", MI, m)
    # The Old World's light at the spring: the same turquoise as MI_AncientGlow, almost spent (used sparingly).
    sb_ue.make_mi("MI_IslAncientFaint", MI, unreal.load_asset(MASTER + "/M_SB_Glow"),
                  vectors={"BaseColor": (0.05, 0.12, 0.12, 1), "GlowColor": (0.35, 0.95, 0.85, 1)}, scalars={"GlowIntensity": 5.0})
    print("SB_POOL_MATERIAL_OK")


def smoke():
    m = sb_ue.reset_material("M_SB_IslSmoke", MASTER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", False)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -1200, 0)
    lp = sb_ue.expr(m, unreal.MaterialExpressionLocalPosition, -1200, 100)
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1200, 180)
    nrm = sb_ue.expr(m, unreal.MaterialExpressionVertexNormalWS, -1200, 260)
    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraVectorWS, -1200, 340)
    params = {"Speed": 0.055, "Spread": 2.6, "Rise": 480.0, "DriftAmount": 160.0, "Wobble": 25.0, "WindX": 0.94, "WindY": 0.34,
              "Density": 0.3, "EdgeSoft": 2.2}
    pn = {k: sb_ue.scalar_param(m, k, v, -1200, 420 + i * 60, "Smoke") for i, (k, v) in enumerate(params.items())}
    colour = sb_ue.vector_param(m, "SmokeColor", (0.42, 0.41, 0.4, 1), -600, -100, "Smoke")
    w = ssm.custom(m, -700, 300, "Smoke puffs (cm)", SMOKE_WPO_CODE, ["VC", "LocalPos", "Time", "Speed", "Spread", "Rise",
                                                                    "DriftAmount", "Wobble", "WindX", "WindY"], F3)
    links = {"VC": vc, "LocalPos": lp, "Time": time}
    for n in ("VC", "LocalPos", "Time", "Speed", "Spread", "Rise", "DriftAmount", "Wobble", "WindX", "WindY"):
        sb_ue.connect(links.get(n, pn.get(n)), "", w, n)
    o = ssm.custom(m, -700, 600, "Smoke opacity", SMOKE_OPACITY_CODE, ["VC", "Time", "Speed", "N", "V", "Density", "EdgeSoft"], F1)
    links = {"VC": vc, "Time": time, "N": nrm, "V": cam}
    for n in ("VC", "Time", "Speed", "N", "V", "Density", "EdgeSoft"):
        sb_ue.connect(links.get(n, pn.get(n)), "", o, n)
    sb_ue.to_prop(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(o, "", unreal.MaterialProperty.MP_OPACITY)
    sb_ue.to_prop(w, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    rough = sb_ue.expr(m, unreal.MaterialExpressionConstant, -300, 200, r=1.0)
    sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    sb_ue.make_mi("MI_IslSmoke", MI, m)
    print("SB_SMOKE_MATERIAL_OK")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")] or ["ground", "plants"]
    if "ground" in args:
        configure_textures()
        m = build_ground()
        sb_ue.make_mi("MI_HomeGround", MI, m)
        print("SB_GROUND_MATERIAL_OK", mel.get_statistics(m).num_pixel_shader_instructions)
    if "plants" in args:
        plants()
    if "smoke" in args or "plants" in args:
        smoke()
    if "pool" in args or "plants" in args:
        pool()


if __name__ == "__main__":
    main()
