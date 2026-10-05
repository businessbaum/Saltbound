"""Flat colours for the hub ship's blockout (D61): M_HubBlk_Flat and its instances MI_HubBlk_* (slot names in
Art/Blender/Scripts/build_hub_blockout.py). Only for the review map L_HubBlockout; the art pass uses the game's palette.

Runs inside the editor:  python Tools/ue_exec.py Tools/ue_scripts/setup_hub_blockout_materials.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

FOLDER = "/Game/Saltbound/Hub/Blockout/Materials"
mel = sb_ue.mel

# Linear colours: open warm, open walls lighter, locked grey, ancient stone pale, the timber hull dark, props brown,
# gates red, the Keelgate's seal turquoise, markers bright (and a little emissive, readable at dusk).
COLOURS = {
    "MI_HubBlk_Open": ((0.55, 0.3, 0.12), 0.0), "MI_HubBlk_Wall": ((0.68, 0.53, 0.32), 0.0), "MI_HubBlk_Locked": ((0.2, 0.2, 0.22), 0.0),
    "MI_HubBlk_Stone": ((0.7, 0.68, 0.6), 0.0), "MI_HubBlk_Hull": ((0.05, 0.03, 0.02), 0.0), "MI_HubBlk_Prop": ((0.22, 0.12, 0.05), 0.0),
    "MI_HubBlk_Rail": ((0.09, 0.05, 0.02), 0.0), "MI_HubBlk_Gate": ((0.5, 0.03, 0.02), 0.0), "MI_HubBlk_Seal": ((0.1, 0.6, 0.5), 2.0),
    "MI_HubBlk_MarkNPC": ((0.7, 0.03, 0.55), 4.0), "MI_HubBlk_MarkE": ((0.02, 0.6, 0.85), 4.0), "MI_HubBlk_MarkBerth": ((0.9, 0.7, 0.02), 3.0),
    "MI_HubBlk_Canvas": ((0.6, 0.55, 0.43), 0.0),
}


def build():
    m = sb_ue.reset_material("M_HubBlk_Flat", FOLDER)
    m.set_editor_property("two_sided", True)
    col = sb_ue.vector_param(m, "Color", (0.5, 0.5, 0.5, 1), -600, 0, "Blockout")
    glow = sb_ue.scalar_param(m, "Glow", 0.0, -600, 150, "Blockout")
    rough = sb_ue.scalar_param(m, "Roughness", 0.85, -600, 250, "Blockout")
    em = sb_ue.expr(m, unreal.MaterialExpressionMultiply, -300, 100)
    sb_ue.connect(col, "", em, "A")
    sb_ue.connect(glow, "", em, "B")
    sb_ue.to_prop(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sb_ue.to_prop(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sb_ue.to_prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    sb_ue.eal.save_loaded_asset(m)
    for name, (rgb, glow_v) in COLOURS.items():
        sb_ue.make_mi(name, FOLDER, m, scalars={"Glow": glow_v}, vectors={"Color": (*rgb, 1.0)})
    print("SB_HUB_MATERIALS_OK", len(COLOURS))


if __name__ == "__main__":
    build()
