"""The sky dome and its stars (D72; script-owned, safe to re-run).

  /Game/Saltbound/Environment/Sky/MPC_Sky   StarAmount (0 by day .. 1 at full night), written by ASaltLightingRig.
  /Game/Saltbound/Environment/Sky/M_SB_Sky  Unlit "is sky" material for the rig's sky dome. A sky material in the scene
      stops the sky atmosphere from drawing the sky's pixels itself (the engine's bSceneHasSkyMaterial), so this material
      draws them: the atmosphere's own view luminance, the discs of its two lights (the sun and the moon), and the stars.
      Stars are crisp painted dots (owner's style: readable, no noise): two layers in a grid on the sphere of directions,
      a dense faint one and a sparse bright one, twinkling slowly, faded out near the horizon where the haze is.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

DIR = "/Game/Saltbound/Environment/Sky"

STARS_HLSL = r"""
float3 d = normalize(Dir);
float result = 0;
// Two layers: dense and faint, sparse and bright. A star sits at a random point of a grid cell; the sphere of directions
// passes near it in a round patch, so every star is a small round dot.
for (int layer = 0; layer < 2; layer++)
{
    float scale = layer == 0 ? 210.0 : 85.0;
    float threshold = layer == 0 ? 0.78 : 0.92;
    float size = layer == 0 ? 0.2 : 0.17;
    float3 p = d * scale;
    float3 c = floor(p);
    float3 f = p - c;
    float h = frac(sin(dot(c, float3(127.1, 311.7, 74.7)) + layer * 17.0) * 43758.5453);
    if (h > threshold)
    {
        float3 o = float3(frac(h * 13.13), frac(h * 71.7), frac(h * 37.3)) * 0.6 + 0.2;
        float s = saturate(1.0 - length(f - o) / size);
        s = s * s * (3.0 - 2.0 * s);
        float bright = (layer == 0 ? 0.35 : 1.0) * (0.45 + 0.55 * frac(h * 91.3));
        float twinkle = 0.8 + 0.2 * sin(Time * (1.2 + 2.0 * frac(h * 5.1)) + h * 40.0);
        result += s * bright * twinkle;
    }
}
// Faded out towards the horizon (haze), gone below it.
return result * saturate(d.z * 5.0 - 0.05);
"""


def main():
    sb_ue.ensure_folder(DIR)
    mpc = sb_ue.get_or_create_asset("MPC_Sky", DIR, unreal.MaterialParameterCollection,
                                    unreal.MaterialParameterCollectionFactoryNew())
    scalars = list(mpc.get_editor_property("scalar_parameters"))
    if not any(str(p.get_editor_property("parameter_name")) == "StarAmount" for p in scalars):
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", "StarAmount")
        p.set_editor_property("default_value", 0.0)
        scalars.append(p)
        mpc.set_editor_property("scalar_parameters", scalars)
    unreal.EditorAssetLibrary.save_loaded_asset(mpc)

    m = sb_ue.reset_material("M_SB_Sky", DIR)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("is_sky", True)

    view = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereViewLuminance, -900, -300)
    sun_disk = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDiskLuminance, -900, -150, light_index=0)
    moon_disk = sb_ue.expr(m, unreal.MaterialExpressionSkyAtmosphereLightDiskLuminance, -900, 0, light_index=1)

    cam = sb_ue.expr(m, unreal.MaterialExpressionCameraVectorWS, -1300, 200)
    neg = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -1100, 200, const_b=-1.0)
    sb_ue.connect(cam, "", neg, "A")
    time = sb_ue.expr(m, unreal.MaterialExpressionTime, -1100, 320)
    stars = sb_ue.expr(m, unreal.MaterialExpressionCustom, -850, 220)
    stars.set_editor_property("code", STARS_HLSL)
    stars.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    stars.set_editor_property("description", "Stars")
    inputs = []
    for name in ("Dir", "Time"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    stars.set_editor_property("inputs", inputs)
    sb_ue.connect(neg, "", stars, "Dir")
    sb_ue.connect(time, "", stars, "Time")

    amount = sb_ue.expr(m, unreal.MaterialExpressionCollectionParameter, -850, 420, collection=mpc, parameter_name="StarAmount")
    # Star radiance (cd/m2): bright against the night's open eye (EV100 about 1), nothing against the day's.
    brightness = sb_ue.scalar_param(m, "StarBrightness", 6.0, -850, 520, "Stars")
    tint = sb_ue.vector_param(m, "StarTint", (0.85, 0.92, 1.0, 1.0), -850, 620, "Stars")
    s1 = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -600, 260)
    sb_ue.connect(stars, "", s1, "A")
    sb_ue.connect(amount, "", s1, "B")
    s2 = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -450, 300)
    sb_ue.connect(s1, "", s2, "A")
    sb_ue.connect(brightness, "", s2, "B")
    s3 = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -300, 340)
    sb_ue.connect(s2, "", s3, "A")
    sb_ue.connect(tint, "", s3, "B")

    a1 = sb_ue.expr(m, unreal.MaterialExpressionAdd, -600, -250)
    sb_ue.connect(view, "", a1, "A")
    sb_ue.connect(sun_disk, "", a1, "B")
    a2 = sb_ue.expr(m, unreal.MaterialExpressionAdd, -450, -150)
    sb_ue.connect(a1, "", a2, "A")
    sb_ue.connect(moon_disk, "", a2, "B")
    a3 = sb_ue.expr(m, unreal.MaterialExpressionAdd, -150, 0)
    sb_ue.connect(a2, "", a3, "A")
    sb_ue.connect(s3, "", a3, "B")
    sb_ue.to_prop(a3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

    # The dome: the engine's sky sphere (radius 40.96 m, faces inward), copied into the always-cooked sky folder so the
    # packaged game has it; the rig scales it far out (ASaltLightingRig::SkyDomeRadius).
    dome_path = DIR + "/SM_SkyDome"
    if not unreal.EditorAssetLibrary.does_asset_exist(dome_path):
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/EngineSky/SM_SkySphere", dome_path)
    dome = unreal.load_asset(dome_path)
    dome.set_material(0, m)
    unreal.EditorAssetLibrary.save_loaded_asset(dome)
    print("SB_NIGHT_SKY_OK", m.get_path_name(), mpc.get_path_name(), dome.get_path_name())


if __name__ == "__main__":
    main()
