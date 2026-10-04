"""UI materials for the Saltbound UI (D55), script-made like the other materials.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/setup_ui_materials.py

  M_UI_PaintedText  font material for painted lettering over the world (spec section 3): a cream fill graded from top
                    to bottom across the line (UV channel 1 of a font material runs 0..1 down the line), the brown speck
                    texture at 40 %. The rim and the soft halo are outlines (SaltUI::PaintedText).
  M_UI_KeySoak      the key on its torn parchment scrap (sections 7 and 8): the scrap, the inked key letter, and for a
                    hold the ink rising through the paper from the bottom with a gently moving wavy surface; where the
                    ink covers the letter it turns parchment-light. Progress 0..1, Time (s); the frame is 120 x 120 px
                    with the 58 px scrap in its middle (Tools/textures/ui_parts.js).
  M_UI_Reveal       a picture revealed by two masks (section 9, the landfall banner): from its middle outward (Half, the
                    revealed half-width as a fraction of the half-frame; the ribbon unrolls and rolls up) and from the left
                    (Front, the ink's edge in U, a little uneven; the name inks in). Soft edges; Tex is the picture.
Colours in the code are the spec's sRGB values, converted to linear (Slate blends in linear and writes sRGB).
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

mel = unreal.MaterialEditingLibrary
FOLDER = "/Game/Saltbound/UI/Materials"
TEX = "/Game/Saltbound/UI/Textures/"


def custom(m, x, y, desc, code, inputs, out_type):
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
    return c


def ui_material(name):
    m = sb_ue.reset_material(name, FOLDER)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    return m


def tex_object(m, name, path, x, y):
    return sb_ue.expr(m, unreal.MaterialExpressionTextureObjectParameter, x, y, parameter_name=name, texture=unreal.load_asset(TEX + path))


PAINTED_CODE = """
float3 A = pow(float3(255.0, 248.0, 230.0) / 255.0, 2.2);
float3 B = pow(float3(242.0, 226.0, 191.0) / 255.0, 2.2);
float3 D = pow(float3(214.0, 185.0, 128.0) / 255.0, 2.2);
float t = saturate((UV1.y - LineTop) / max(LineBottom - LineTop, 0.01));
float3 G = t < 0.55 ? lerp(A, B, t / 0.55) : lerp(B, D, (t - 0.55) / 0.45);
float S = Texture2DSample(Speck, SpeckSampler, Pixel / float2(1400.0, 80.0)).a;
G = lerp(G, pow(float3(95.0, 62.0, 30.0) / 255.0, 2.2), S * 0.4);
return G * VC.rgb;
"""

SOAK_CODE = """
float2 px = UV * 120.0;
float f = saturate(Progress);
float on = step(0.001, f);
float top = 91.0 - 62.0 * f + sin((px.x - 60.0) * 0.22 + Time * 2.4) * 2.0;
float ink = smoothstep(top - 1.2, top + 1.2, px.y) * on;
float4 S = Texture2DSample(Scrap, ScrapSampler, UV);
float P = Texture2DSample(Paper, PaperSampler, UV).a;
float4 L = Texture2DSample(Letter, LetterSampler, UV);
float4 LL = Texture2DSample(LetterLight, LetterLightSampler, UV);
float3 c = lerp(S.rgb, L.rgb, L.a);
c = lerp(c, pow(float3(58.0, 30.0, 12.0) / 255.0, 2.2), 0.78 * ink * P);
c = lerp(c, LL.rgb, LL.a * ink * P);
return float4(c * VC.rgb, S.a * VA);
"""


def painted_text():
    m = ui_material("M_UI_PaintedText")
    uv1 = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0, coordinate_index=1)
    pix = sb_ue.expr(m, unreal.MaterialExpressionScreenPosition, -900, 120)
    speck = tex_object(m, "Speck", "T_UI_PaintSpeck", -900, 240)
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -900, 400)
    top = sb_ue.scalar_param(m, "LineTop", 0.14, -900, 520, "Paint")
    bot = sb_ue.scalar_param(m, "LineBottom", 0.82, -900, 600, "Paint")
    c = custom(m, -500, 200, "Painted lettering fill", PAINTED_CODE, ["UV1", "Pixel", "Speck", "VC", "LineTop", "LineBottom"],
               unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sb_ue.connect(uv1, "", c, "UV1")
    sb_ue.connect(pix, "PixelPosition", c, "Pixel")
    sb_ue.connect(speck, "", c, "Speck")
    sb_ue.connect(vc, "", c, "VC")
    sb_ue.connect(top, "", c, "LineTop")
    sb_ue.connect(bot, "", c, "LineBottom")
    sb_ue.to_prop(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(vc, "A", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)


def key_soak():
    m = ui_material("M_UI_KeySoak")
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    scrap = tex_object(m, "Scrap", "T_UI_KeyScrap", -900, 100)
    paper = tex_object(m, "Paper", "T_UI_KeyScrap_Paper", -900, 220)
    letter = tex_object(m, "Letter", "T_UI_KeyLetter_E", -900, 340)
    light = tex_object(m, "LetterLight", "T_UI_KeyLetterLight_E", -900, 460)
    prog = sb_ue.scalar_param(m, "Progress", 0.0, -900, 580, "Hold")
    time = sb_ue.scalar_param(m, "Time", 0.0, -900, 660, "Hold")
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -900, 760)
    c = custom(m, -500, 300, "Key scrap and ink soak", SOAK_CODE, ["UV", "Scrap", "Paper", "Letter", "LetterLight", "Progress", "Time", "VC", "VA"],
               unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    for node, pin in ((uv, "UV"), (scrap, "Scrap"), (paper, "Paper"), (letter, "Letter"), (light, "LetterLight"), (prog, "Progress"),
                      (time, "Time"), (vc, "VC")):
        sb_ue.connect(node, "", c, pin)
    sb_ue.connect(vc, "A", c, "VA")
    rgb = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -250, 250, r=True, g=True, b=True, a=False)
    alpha = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -250, 380, r=False, g=False, b=False, a=True)
    sb_ue.connect(c, "", rgb, "")
    sb_ue.connect(c, "", alpha, "")
    sb_ue.to_prop(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)


REVEAL_CODE = """
float4 T = Texture2DSample(Tex, TexSampler, UV);
float d = abs(UV.x - 0.5) * 2.0;
float mc = saturate((Half - d) / max(Soft, 0.0001) + 0.5);
float front = Front + 0.004 * sin(UV.y * 37.0) + 0.003 * sin(UV.y * 91.0 + 1.3);
float ms = saturate((front - UV.x) / max(FrontSoft, 0.0001) + 0.5);
return float4(T.rgb * VC.rgb, T.a * mc * ms * VA);
"""


def reveal():
    m = ui_material("M_UI_Reveal")
    uv = sb_ue.expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tex = tex_object(m, "Tex", "T_UI_LandfallSeal", -900, 100)
    half = sb_ue.scalar_param(m, "Half", 1.0, -900, 260, "Reveal")
    soft = sb_ue.scalar_param(m, "Soft", 0.02, -900, 340, "Reveal")
    front = sb_ue.scalar_param(m, "Front", 2.0, -900, 420, "Reveal")
    fsoft = sb_ue.scalar_param(m, "FrontSoft", 0.012, -900, 500, "Reveal")
    vc = sb_ue.expr(m, unreal.MaterialExpressionVertexColor, -900, 600)
    c = custom(m, -500, 300, "Reveal from the middle and from the left", REVEAL_CODE, ["UV", "Tex", "Half", "Soft", "Front", "FrontSoft", "VC", "VA"],
               unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    for node, pin in ((uv, "UV"), (tex, "Tex"), (half, "Half"), (soft, "Soft"), (front, "Front"), (fsoft, "FrontSoft"), (vc, "VC")):
        sb_ue.connect(node, "", c, pin)
    sb_ue.connect(vc, "A", c, "VA")
    rgb = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -250, 250, r=True, g=True, b=True, a=False)
    alpha = sb_ue.expr(m, unreal.MaterialExpressionComponentMask, -250, 380, r=False, g=False, b=False, a=True)
    sb_ue.connect(c, "", rgb, "")
    sb_ue.connect(c, "", alpha, "")
    sb_ue.to_prop(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)


painted_text()
key_soak()
reveal()
print("SB_UI_MATERIALS_OK")
