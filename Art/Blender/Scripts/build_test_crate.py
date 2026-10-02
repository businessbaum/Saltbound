"""Pipeline test asset: small wooden crate with iron corner brackets.

Run: Tools/Build-BlenderAsset.ps1 build_test_crate
Expected in Unreal: SM_Crate_Small, 60 x 40 x 40 cm, pivot bottom centre, 2 material slots, 1 convex collision.
An iron plate on the +X face only verifies the forward axis.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

sb.reset_scene()
wood = sb.get_material("MI_Wood_Planks", (0.55, 0.36, 0.2, 1))
iron = sb.get_material("MI_Metal_Iron", (0.15, 0.15, 0.16, 1))

L, W, H = 0.60, 0.40, 0.40
parts = []
plank_h = H / 3.0
gap = 0.006
# Long side planks (+Y / -Y)
for side in (-1, 1):
    for i in range(3):
        z = plank_h * (i + 0.5)
        p = sb.add_box("plank", (L - 0.04, 0.018, plank_h - gap), (0, side * (W / 2 - 0.009), z), wood)
        parts.append(sb.soften(p, 0.003))
# End planks (+X / -X)
for side in (-1, 1):
    for i in range(3):
        z = plank_h * (i + 0.5)
        p = sb.add_box("plank", (0.018, W - 0.04, plank_h - gap), (side * (L / 2 - 0.009), 0, z), wood)
        parts.append(sb.soften(p, 0.003))
# Lid and bottom
for z in (0.009, H - 0.009):
    p = sb.add_box("lid", (L - 0.004, W - 0.004, 0.018), (0, 0, z), wood)
    parts.append(sb.soften(p, 0.003))
# Corner posts
for sx in (-1, 1):
    for sy in (-1, 1):
        p = sb.add_box("post", (0.04, 0.04, H), (sx * (L / 2 - 0.02), sy * (W / 2 - 0.02), H / 2), wood)
        parts.append(sb.soften(p, 0.004))
# Iron corner brackets (top and bottom corners)
for sx in (-1, 1):
    for sy in (-1, 1):
        for z in (0.03, H - 0.03):
            p = sb.add_box("bracket", (0.07, 0.07, 0.06), (sx * (L / 2 - 0.03), sy * (W / 2 - 0.03), z), iron)
            parts.append(sb.soften(p, 0.003, 1))
# Forward marker: iron plate on the +X face only (axis check)
p = sb.add_box("fwd_plate", (0.006, 0.12, 0.08), (L / 2 + 0.003, 0, H * 0.6), iron)
parts.append(sb.soften(p, 0.002, 1))
# Starboard marker: small iron plate on the -Y face (Blender) = +Y (Unreal right side)
p = sb.add_box("stbd_plate", (0.06, 0.006, 0.06), (0.15, -(W / 2 + 0.003), H * 0.6), iron)
parts.append(sb.soften(p, 0.002, 1))

crate = sb.join(parts, "SM_Crate_Small")
sb.jitter_vertices(crate, 0.0008, seed=7)
sb.world_box_uv(crate, metres_per_uv=1.0)
ucx = sb.add_ucx_box("SM_Crate_Small", 1, (L, W, H), (0, 0, H / 2))
sb.save_blend("SM_Crate_Small")
sb.export_fbx([crate, ucx], os.path.join("Props", "SM_Crate_Small.fbx"))
