"""Sky assets (script-owned; safe to re-run).

  /Game/Saltbound/Environment/Sky/MI_Clouds_Stylized
      Instance of the engine's simple volumetric cloud material, tuned for stylised realism (D21): less
      coverage (more blue sky), denser and larger cumulus masses with crisp, simple edges (little
      high-frequency erosion), bright albedo.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

PARENT = "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"  # keeps its texture overrides
DIR = "/Game/Saltbound/Environment/Sky"


def main():
    sb_ue.ensure_folder(DIR)
    mi = sb_ue.make_mi("MI_Clouds_Stylized", DIR, unreal.load_asset(PARENT),
                       scalars={"Cloud_GlobalCoverage": -0.3, "Cloud_GlobalDensity": 0.03, "Layout_CloudGlobalScale": 380.0},
                       vectors={"Cloud_AlbedoColor": (1.0, 1.0, 1.0, 0.5),
                                # r: base shape, g: mid erosion, b: fine erosion, a: overall (engine defaults 0.8/0.08/0.03/2.5)
                                "Noise_Strength": (0.8, 0.045, 0.006, 2.5)})
    print("SB_SKY_OK", mi.get_path_name())


if __name__ == "__main__":
    main()
