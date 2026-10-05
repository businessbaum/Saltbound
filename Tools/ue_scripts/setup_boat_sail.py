"""The starter boat's square sail material (D65; script-owned, safe to re-run).

  M_SB_SquareSail      the shared surface master (setup_surface_materials.build_surface) with the square sail's motion
  MI_SquareSailCloth   its canvas
  MI_SquareSailRope    its boltrope, cringles, robands, earings and reef points (they move with the cloth)

  python Tools/ue_exec.py Tools/ue_scripts/setup_boat_sail.py

SM_Boat_Sail (build_boat.py build_sail) hangs flat in the yard's frame: local cm, +X forward, +Y starboard, +Z up, the origin on
the yard's axis in the middle. ASaltBoat drives Set (0 = rolled up under the yard .. 1 = let fall), Fill, Belly (cm), Lean (-1..1,
to starboard) and Flutter; the geometry parameters are SaltHullShape.h's Sail* numbers (ASaltBoat sets them on its instances too,
and puts the sheets' and the brails' ends where this code moves the clews and the foot: keep both in sync).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402
import setup_surface_materials as ssm  # noqa: E402

# The cloth hangs from its head (HeadZ under the yard's axis) down to its foot (Drop lower), HalfHead .. HalfFoot wide either side.
# Set lets it fall from the head down to Cut = Set * Drop (at least CutMin); the cloth below the cut is rolled up round a roll
# across the sail, just forward of the cut: the foot innermost, every turn RollLayer further out from a RollCore. What hangs
# bellies forward (Belly at its deepest, a little below the middle; held at the yard and at the leeches), leans to leeward with
# the wind's sideways share (Lean), breathes with the fill and shivers when the wind comes from ahead (Flutter).
SQUARE_SAIL_WPO = """
float3 P = LocalPos;
float D = max(Drop, 1.0);
float Dd = clamp(HeadZ - LocalPos.z, 0.0, D);
float W = lerp(HalfHead, HalfFoot, Dd / D);
float U = saturate(0.5 + LocalPos.y / (2.0 * max(W, 1.0)));
float Cut = max(saturate(Set) * D, CutMin);
float E = Dd - Cut;
float Vh = min(Dd, Cut) / max(Cut, 1.0);
float Hang = sqrt(Cut / D);
float Up = 0.5 + 0.18 * Lean;
float Ua = U < Up ? 0.5 * U / Up : 0.5 + 0.5 * (U - Up) / (1.0 - Up);
float Across = sin(3.14159 * Ua);
float Zh = HeadZ - min(Dd, Cut);
float Breath = sin(Time * 1.6 + Zh * 0.012) * 0.08 * saturate(Fill);
float Depth = Belly * Hang * Across * (1.0 + Breath);
float Side = Lean * 0.3 * Depth;
if (E <= 0.0)
{
    float Shiver = sin(Dd * 0.05 - Time * 9.0 + LocalPos.y * 0.03) * Flutter * (0.3 + 0.7 * abs(2.0 * U - 1.0)) * 4.0 * Hang * sin(3.14159 * Vh);
    P.x = LocalPos.x + Depth * sin(2.4 * Vh) + Shiver;
    P.y = LocalPos.y + Side * sin(2.83 * Vh);
    return P - LocalPos;
}
float Rolled = D - Cut;
float Rout = sqrt(RollCore * RollCore + RollLayer * Rolled / 3.14159);
float Rk = sqrt(max(Rout * Rout - RollLayer * E / 3.14159, RollCore * RollCore));
float Th = 6.28318 / max(RollLayer, 0.1) * (Rout - Rk);
float Wc = lerp(HalfHead, HalfFoot, Cut / D);
float Rr = Rk - LocalPos.x;
P.x = Depth * sin(2.4) + Rout - Rr * cos(Th);
P.y = LocalPos.y * Wc / max(W, 1.0) + Side * sin(2.83);
P.z = HeadZ - Cut - Rr * sin(Th);
return P - LocalPos;
"""
SQUARE_SAIL_PARAMS = {"Set": 0.0, "Fill": 0.0, "Belly": 0.0, "Lean": 0.0, "Flutter": 0.15, "HeadZ": -15.0, "Drop": 665.0,
                      "HalfHead": 230.0, "HalfFoot": 260.0, "RollLayer": 2.5, "RollCore": 4.0, "CutMin": 30.0}


def main():
    sail = ssm.build_surface("M_SB_SquareSail", ("Sail", "Square sail: belly, lean, shiver, rolled up from the foot (local cm)",
                                                 SQUARE_SAIL_WPO, SQUARE_SAIL_PARAMS))
    # The canvas and the rope as on the rest of the boat (setup_surface_materials: MI_Canvas, MI_Rope_Hemp).
    sb_ue.make_mi("MI_SquareSailCloth", ssm.MI, sail, textures=ssm.canvas_textures(), vectors={"Tint": (0.53, 0.5, 0.43, 1)},
                  scalars={"TexNormalize": 2.6, "NormalStrength": 0.5, "RoughScale": 1.6, "WearStrength": 0.0, "UVScale": 1.0})
    sb_ue.make_mi("MI_SquareSailRope", ssm.MI, sail, vectors={"Tint": (0.21, 0.165, 0.105, 1)},
                  textures={"BaseTexture": ssm.TEX + "/Fabric/T_Rope_D", "NormalTexture": ssm.TEX + "/Fabric/T_Rope_N"},
                  scalars={"RoughScale": 1.4, "WearStrength": 0.0, "TexNormalize": 1.6, "UVScale": 6.0, "MacroAmount": 0.0})
    print("SB_SQUARE_SAIL_OK", ssm.mel.get_statistics(sail).num_vertex_shader_instructions)


if __name__ == "__main__":
    main()
