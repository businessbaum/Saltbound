"""The hub ship's materials (D62; outside.colours in HubShip_Data.json): script-owned instances of the surface and rock
masters, safe to re-run.

  MI_Hub_HullTar / Faded / Dark   the dark tarred planking (three shades the boards pick from, as the boat's paint does)
  MI_Hub_Ochre / OchreFaded       the muted ochre bands and wales
  MI_Hub_AncientStone             the ancient lower hull: pale, seamless, calm stone (world-aligned), green and wet at the
                                  waterline; its two lines are MI_AncientGlow
  MI_Hub_Void                     the dark behind open ports and hawse holes

    python Tools/ue_exec.py Tools/ue_scripts/setup_hub_materials.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402
import setup_surface_materials as S  # noqa: E402

MI = S.MI
surf = unreal.load_asset(S.MASTER + "/M_SB_Surface")
rock = unreal.load_asset(S.MASTER + "/M_SB_Rock")

tar = dict(vectors={"Tint": (0.052, 0.036, 0.024, 1), "WearColor": (0.15, 0.1, 0.06, 1), "GrimeColor": (0.42, 0.5, 0.34, 1),
                    "VarTint": (0.9, 0.95, 1.05, 1)},
           scalars={"RoughScale": 0.85, "NormalStrength": 0.8, "WearStrength": 0.8, "WearRoughness": 0.7, "VarAmount": 0.22,
                    "VarHue": 0.3, "GrimeAmount": 1.0, "GrimeRoughness": 0.4, "MacroAmount": 0.12},
           textures=S.wood_textures())
ochre = dict(vectors={"Tint": (0.36, 0.215, 0.06, 1), "WearColor": (0.1, 0.06, 0.03, 1), "GrimeColor": (0.5, 0.48, 0.36, 1),
                      "VarTint": (1.06, 0.95, 0.82, 1)},
             scalars={"TexNormalize": 2.6, "NormalStrength": 0.6, "RoughScale": 1.0, "VarAmount": 0.12, "VarHue": 0.35,
                      "WearStrength": 1.2, "WearRoughness": 0.7, "GrimeAmount": 0.9},
             textures=S.paint_textures())


def variant(base, **tints):
    kw = dict(base)
    kw["vectors"] = dict(kw["vectors"], **tints)
    return kw


sb_ue.make_mi("MI_Hub_HullTar", MI, surf, **tar)
sb_ue.make_mi("MI_Hub_HullTarFaded", MI, surf, **variant(tar, Tint=(0.085, 0.066, 0.048, 1)))      # sun-bleached boards
sb_ue.make_mi("MI_Hub_HullTarDark", MI, surf, **variant(tar, Tint=(0.03, 0.021, 0.015, 1)))        # fresh tar
sb_ue.make_mi("MI_Hub_Ochre", MI, surf, **ochre)
sb_ue.make_mi("MI_Hub_OchreFaded", MI, surf, **variant(ochre, Tint=(0.44, 0.32, 0.14, 1)))
sb_ue.make_mi("MI_Hub_Void", MI, surf, vectors={"Tint": (0.004, 0.004, 0.004, 1)},
              scalars={"RoughScale": 1.0, "NormalStrength": 0.0, "WearStrength": 0.0, "GrimeAmount": 0.0, "VarAmount": 0.0, "MacroAmount": 0.0},
              textures=S.paint_textures())
# The ancient hull: pale and calm (big scale, low contrast, no strata or lichen), green slime and barnacles at the waterline.
sb_ue.make_mi("MI_Hub_AncientStone", MI, rock, vectors={"Tint": (0.7, 0.69, 0.63, 1), "LichenColor": (0.4, 0.46, 0.3, 1)},
              scalars={"Scale": 900.0, "PatternContrast": 0.35, "NormalStrength": 0.25, "LichenCoverage": 0.0, "StrataScale": 1.0e6,
                       "MacroAmount": 0.08, "AlgaeAmount": 0.8, "BarnacleAmount": 0.35, "WetDarken": 0.8, "TideHeight": 0.6,
                       "WetHeight": 1.2, "VertexMaskAmount": 0.0, "PlateVar": 0.04})
# The rig (build_hub_rig.py): tarred standing rigging, weathered furled canvas, the pennant on the flag master (both
# colours with the same Length, so the cloth streams as one).
rope_tex = {"BaseTexture": S.TEX + "/Fabric/T_Rope_D", "NormalTexture": S.TEX + "/Fabric/T_Rope_N"}
sb_ue.make_mi("MI_Hub_RopeTar", MI, surf, vectors={"Tint": (0.04, 0.032, 0.024, 1)},
              scalars={"RoughScale": 1.2, "WearStrength": 0.0, "TexNormalize": 1.6, "UVScale": 6.0, "MacroAmount": 0.0}, textures=rope_tex)
canvas = dict(scalars={"TexNormalize": 2.6, "NormalStrength": 0.6, "RoughScale": 1.6, "WearStrength": 0.0, "UVScale": 1.0,
                       "GrimeAmount": 0.6, "VarAmount": 0.15, "VarHue": 0.3},
              textures=S.canvas_textures())
sb_ue.make_mi("MI_Hub_SailFurled", MI, surf, vectors={"Tint": (0.4, 0.37, 0.3, 1), "VarTint": (0.92, 0.95, 1.0, 1),
                                                       "GrimeColor": (0.45, 0.42, 0.36, 1)}, **canvas)
flag = unreal.load_asset(S.MASTER + "/M_SB_Flag")
pennant = dict(scalars={"TexNormalize": 2.6, "NormalStrength": 0.5, "RoughScale": 1.6, "WearStrength": 0.0, "UVScale": 1.0,
                        "Length": 1600.0, "Wind": 0.6}, textures=S.canvas_textures())
sb_ue.make_mi("MI_Hub_Pennant", MI, flag, vectors={"Tint": (0.36, 0.075, 0.05, 1)}, **pennant)
sb_ue.make_mi("MI_Hub_PennantCream", MI, flag, vectors={"Tint": (0.6, 0.53, 0.4, 1)}, **pennant)
print("SB_HUB_MATERIALS_OK")
