"""Small wooden crate (stylised, hand-made; see Decisions D16/D20). Also the pipeline test asset.

Run: Tools/Build-BlenderAsset.ps1 build_test_crate
Expected in Unreal: SM_Crate_Small, about 62 x 42 x 42 cm, pivot bottom centre, 2 material slots, 1 convex
collision. Chunky, slightly crooked planks (sb_timber) between square timber corner posts, a three-board
lid, heavy iron corner brackets and an iron latch on the +X face (forward-axis marker).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402
import sb_timber as tb  # noqa: E402

sb.reset_scene()
wood = sb.get_material("MI_Wood_Planks", (0.55, 0.36, 0.2, 1))
iron = sb.get_material("MI_Metal_Iron", (0.15, 0.15, 0.16, 1))

L, W, H = 0.60, 0.40, 0.40
T = 0.028                      # plank thickness
post = 0.05                    # corner post size
parts = []
seed = 700
plank_h = (H - 0.03) / 3.0
half_pi = math.pi / 2
# Long sides (+Y / -Y): planks along X, standing on edge.
for side in (-1, 1):
    for i in range(3):
        seed += 1
        z = 0.03 + plank_h * (i + 0.5)
        parts.append(tb.rough_plank("tb_side", L - 2 * post + 0.01, plank_h - 0.008, T, (0, side * (W / 2 - T / 2), z),
                                    (half_pi, 0, 0), wood, seed, wear=0.6, grooves=(1, 2), split_chance=0.1, bevel=0.45))
# Ends (+X / -X): planks along Y.
for side in (-1, 1):
    for i in range(3):
        seed += 1
        z = 0.03 + plank_h * (i + 0.5)
        parts.append(tb.rough_plank("tb_end", W - 2 * post + 0.01, plank_h - 0.008, T, (side * (L / 2 - T / 2), 0, z),
                                    (half_pi, 0, half_pi), wood, seed, wear=0.6, grooves=(1, 2), split_chance=0.1, bevel=0.45))
# Corner posts: square timber.
for sx in (-1, 1):
    for sy in (-1, 1):
        seed += 1
        parts.append(tb.rough_post("tb_post", post * 0.68, H - 0.01, (sx * (L / 2 - post / 2), sy * (W / 2 - post / 2), 0.0),
                                   sides=4, taper=0.97, material=wood, seed=seed, top_wear=0.3))
# Lid: three boards along X, slight overhang.
for k in range(3):
    seed += 1
    w = (W + 0.02) / 3
    parts.append(tb.rough_plank("tb_lid", L + 0.02, w - 0.006, T, (0, -W / 2 - 0.01 + w * (k + 0.5), H + T / 2 - 0.005),
                                (0, 0, 0), wood, seed, wear=0.5, grooves=(1, 3), split_chance=0.15, bevel=0.45))
# Bottom (hidden).
parts.append(sb.add_box("bottom", (L - 0.01, W - 0.01, 0.02), (0, 0, 0.012), wood))
# Heavy iron corner brackets: two plates wrapping each vertical corner, top and bottom.
for sx in (-1, 1):
    for sy in (-1, 1):
        for z in (0.06, H - 0.04):
            parts.append(sb.soften(sb.add_box("bracket", (0.11, 0.008, 0.07), (sx * (L / 2 - 0.045), sy * (W / 2 + 0.004), z), iron), 0.003, 1))
            parts.append(sb.soften(sb.add_box("bracket", (0.008, 0.11, 0.07), (sx * (L / 2 + 0.004), sy * (W / 2 - 0.045), z), iron), 0.003, 1))
# Latch on +X (forward marker).
parts.append(sb.soften(sb.add_box("latch", (0.012, 0.09, 0.07), (L / 2 + 0.008, 0, H - 0.05), iron), 0.003, 1))
parts.append(sb.add_torus("latch_ring", 0.022, 0.006, (L / 2 + 0.02, 0, H - 0.1), (0, math.pi / 2, 0), 16, 6, iron))
for p in parts:
    if p.name.startswith(("bottom", "bracket", "latch")):
        sb.world_box_uv(p, 1.0)

crate = sb.join(parts, "SM_Crate_Small")
sb.bake_vertex_masks(crate, wear=0.8, seed=7, bevel_max=0.02)
ucx = sb.add_ucx_box("SM_Crate_Small", 1, (L + 0.02, W + 0.02, H + T), (0, 0, (H + T) / 2))
print("SB_CRATE faces", len(crate.data.polygons))
sb.save_blend("SM_Crate_Small")
sb.export_fbx([crate, ucx], os.path.join("Props", "SM_Crate_Small.fbx"))
