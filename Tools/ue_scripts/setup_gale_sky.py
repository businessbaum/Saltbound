"""The Gale in the sky's own clouds (D87; owner: "fog, clouds and the storm form a natural wall with no edges").

  python Tools/ue_exec.py Tools/ue_scripts/setup_gale_sky.py

The sky's volumetric clouds (MI_Clouds_Stylized, D21) are the engine's simple cloud material, which has a storm mode built in:
StormClouds (0..1) blends to dark, dense storm clouds with lightning flickering inside. This script copies that material into the
project (M_SB_VolumetricCloud, the engine's untouched apart from what follows) and makes its storm a function of where the cloud is:
  - a ring round the sea (Gale_Ring: x the radius where the storm begins (km), y where it is full, z the height its tops reach (km),
    w its strength) and up to three breakouts (Gale_Cell0..2: x, y (km), radius (km), strength), smooth-edged;
  - there, StormClouds goes up to 1 (each of its six uses takes max(StormClouds, storm here)) and the coverage rises by
    Gale_CoverageBoost, so the wall is made of the same clouds as the rest of the sky, with the same noise, light and lightning.
ASaltGale sets the breakouts on the rig's cloud material at run time; the defaults put the ring in place in the editor too.
MI_Clouds_Stylized is re-parented to the copy (setup_sky.py). Re-running rebuilds the copy from the engine's material.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
SRC = "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud"
DIR = "/Game/Saltbound/Environment/Sky"
NAME = "M_SB_VolumetricCloud"

MASK = """
// D87: how stormy the cloud is here: the Gale's ring round the sea and the breakouts, up to the tops' height
float2 p = WP.xy / 100000.0;                        // km from the cove
float z = WP.z / 100000.0;                          // km up
float top = 1.0 - smoothstep(Ring.z - 1.5, Ring.z + 1.5, z);
float s = smoothstep(Ring.x, Ring.y, length(p)) * Ring.w;
float4 cells[3] = { Cell0, Cell1, Cell2 };
for (int i = 0; i < 3; i++)
{
    float d = length(p - cells[i].xy);
    s = max(s, (1.0 - smoothstep(cells[i].z * 0.5, cells[i].z * 1.4, d)) * cells[i].w);
}
return saturate(s * top);
"""


def build():
    path = DIR + "/" + NAME
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        # Rebuild: the copy's graph goes back to the engine's (copy the source's expressions by duplicating again is not possible while
        # instances reference it; so undo our own nodes instead).
        for e in list(mel.get_material_expressions(m)):
            if isinstance(e, unreal.MaterialExpressionCustom) and e.get_editor_property("description") == "Gale storm mask":
                raise SystemExit("SB_GALE_SKY already set up (delete %s to rebuild)" % path)
    else:
        if not eal.duplicate_asset(SRC, path):
            raise SystemExit("SB_GALE_SKY could not copy " + SRC)
        m = unreal.load_asset(path)
    exprs = list(mel.get_material_expressions(m))

    wp = sb_ue.expr(m, unreal.MaterialExpressionWorldPosition, -6000, -2000)
    ring = sb_ue.vector_param(m, "Gale_Ring", (9.0, 9.7, 9.0, 1.0), -6000, -1900, "Gale")
    cells = [sb_ue.vector_param(m, "Gale_Cell%d" % i, (0.0, 0.0, 1.0, 0.0), -6000, -1800 + i * 100, "Gale") for i in range(3)]
    boost = sb_ue.scalar_param(m, "Gale_CoverageBoost", 1.2, -6000, -1500, "Gale")
    mask = sb_ue.expr(m, unreal.MaterialExpressionCustom, -5600, -1900)
    mask.set_editor_property("description", "Gale storm mask")
    mask.set_editor_property("code", MASK)
    mask.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ins = []
    for n in ("WP", "Ring", "Cell0", "Cell1", "Cell2"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    mask.set_editor_property("inputs", ins)
    for n, node in (("WP", wp), ("Ring", ring), ("Cell0", cells[0]), ("Cell1", cells[1]), ("Cell2", cells[2])):
        sb_ue.connect(node, "", mask, n)

    def params(name):
        return [e for e in exprs if isinstance(e, unreal.MaterialExpressionScalarParameter) and str(e.get_editor_property("parameter_name")) == name]

    def rewire(src, new):
        """Every input fed by src is fed by new instead."""
        count = 0
        for e in exprs:
            try:
                feeds = mel.get_inputs_for_material_expression(m, e)
                names = mel.get_material_expression_input_names(e)
            except Exception:
                continue
            for i, f in enumerate(feeds):
                if f == src and i < len(names):
                    if not mel.connect_material_expressions(new, "", e, str(names[i])):
                        raise SystemExit("SB_GALE_SKY could not rewire %s.%s" % (e.get_name(), names[i]))
                    count += 1
        return count

    storm = params("StormClouds")
    coverage = params("Cloud_GlobalCoverage")
    rewired = 0
    for k, p in enumerate(storm):
        mx = sb_ue.expr(m, unreal.MaterialExpressionMax, -5200, -1900 + k * 80)
        rewired += rewire(p, mx)
        sb_ue.connect(p, "", mx, "A")
        sb_ue.connect(mask, "", mx, "B")
    for k, p in enumerate(coverage):
        mul = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -5200, -1300 + k * 80)
        sb_ue.connect(mask, "", mul, "A")
        sb_ue.connect(boost, "", mul, "B")
        add = sb_ue.expr(m, unreal.MaterialExpressionAdd, -5000, -1300 + k * 80)
        rewired += rewire(p, add)
        sb_ue.connect(p, "", add, "A")
        sb_ue.connect(mul, "", add, "B")
    mel.recompile_material(m)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    print("SB_GALE_SKY storm params=%d coverage params=%d inputs rewired=%d" % (len(storm), len(coverage), rewired))
    return m


INST_SRC = "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"
INST = "MI_SB_VolumetricCloud_Base"


def instance(base):
    """The engine's instance (its texture and parameter overrides) copied and put on the copy of the material: the sky's own
    instance (MI_Clouds_Stylized) hangs under it as it hung under the engine's."""
    path = DIR + "/" + INST
    if not eal.does_asset_exist(path):
        if not eal.duplicate_asset(INST_SRC, path):
            raise SystemExit("SB_GALE_SKY could not copy " + INST_SRC)
    mi = unreal.load_asset(path)
    mel.set_material_instance_parent(mi, base)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


def main():
    sb_ue.ensure_folder(DIR)
    base = build()
    instance(base)
    # the sky's clouds on the copy (as setup_sky.py makes them)
    import setup_sky
    setup_sky.main()
    st = mel.get_statistics(unreal.load_asset(DIR + "/" + NAME))
    print("SB_GALE_SKY_OK", st.num_pixel_shader_instructions)


if __name__ == "__main__":
    main()
