"""Things lying about that the player can pick up (D50, SaltItems.cpp). Origin at the ground contact.

  SM_Pickup_Driftwood     a crooked, sun-bleached branch with a side stub, about 1 m
  SM_Pickup_Seashell      a ribbed scallop shell, 22 cm, slightly domed
  SM_Pickup_RopeCoil      a flat coil of hemp rope with a loose end
  SM_Pickup_GlassFloat    a green glass fishing float in a rope net
  SM_Pickup_AncientShard  a faceted, glowing shard leaning in a broken stone socket
  SM_Pickup_Canvas        D73: a folded bolt of sailcloth tied crosswise with twine, 40 cm
  SM_Pickup_IronNails     D73: an oiled-cloth twist of square nails, a few spilled beside it
  SM_Pickup_Tar           D73: a small tarred keg with two iron hoops, its open head black and glossy
  SM_Pickup_PaleFloat     D73: a hollow pale float of no material the sailors know, a moulded seam round it and an eye on top
                          (an Old World thing; the concept's reveal ladder starts with it)

Chunky and readable from a few metres (the owner's style brief: hand-made, broad bevels). Exported to Props/Pickups.
Run: Tools/Build-BlenderAsset.ps1 build_pickups [driftwood seashell rope float shard]
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402
import sb_timber as tb  # noqa: E402

M = {}


def mats():
    for n, c in {"MI_Wood_DockWeathered": (0.6, 0.57, 0.52), "MI_Paint_Cream": (0.9, 0.82, 0.74), "MI_Rope_Hemp": (0.6, 0.5, 0.3),
                 "MI_Glass_Dark": (0.2, 0.45, 0.32), "MI_Stone_Ruin": (0.42, 0.4, 0.36), "MI_AncientGlow": (0.35, 0.95, 0.85)}.items():
        M[n] = sb.get_material(n, (*c, 1))


def finish(parts, name):
    for p in parts:
        if not p.data.uv_layers:
            sb.world_box_uv(p, 0.5)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=0.8, seed=len(name), bevel_max=0.02)
    sb.save_blend(name)
    sb.export_fbx([obj], os.path.join("Pickups", name + ".fbx"))
    print("SB_PICKUP", name, len(obj.data.polygons))


def sphere(name, radius, center, material, segs=20, rings=12):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=radius)
    for v in bm.verts:
        v.co += Vector(center)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for f in me.polygons:
        f.use_smooth = True
    obj = sb.new_object(name, me)
    obj.data.materials.append(M[material])
    return obj


def driftwood():
    sb.reset_scene()
    mats()
    main = tb.spar("branch", 1.05, 0.065, 0.035, M["MI_Wood_DockWeathered"], 81, sides=14, sweep=0.09, axis=(1, 0, 0), facets=6, checks=1)
    main.data.transform(Matrix.Translation((-0.5, 0.0, 0.055)))
    stub = tb.spar("stub", 0.34, 0.035, 0.02, M["MI_Wood_DockWeathered"], 82, sides=10, sweep=0.05, axis=(0.6, 0.8, 0.0), facets=5)
    stub.data.transform(Matrix.Translation((0.02, 0.02, 0.05)))
    finish([main, stub], "SM_Pickup_Driftwood")


def seashell():
    sb.reset_scene()
    mats()
    bm = bmesh.new()
    ribs, rings, radius = 16, 8, 0.11
    grid = []
    for j in range(rings + 1):
        r = radius * j / rings
        row = []
        for i in range(ribs * 2 + 1):
            a = math.pi * (0.12 + 0.76 * i / (ribs * 2))
            rib = 0.006 * math.cos(i * math.pi) * (j / rings)          # ridges and grooves
            dome = 0.045 * (1 - (j / rings) ** 2)
            row.append(bm.verts.new((r * math.cos(a), r * math.sin(a) - 0.05, dome + rib + 0.012)))
        grid.append(row)
    for j in range(rings):
        for i in range(ribs * 2):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    me = bpy.data.meshes.new("shell")
    bm.to_mesh(me)
    bm.free()
    shell = sb.new_object("shell", me)
    shell.data.materials.append(M["MI_Paint_Cream"])
    sol = shell.modifiers.new("Solidify", "SOLIDIFY")
    sol.thickness = 0.008
    sb.apply_modifiers(shell)
    hinge = sb.add_box("hinge", (0.06, 0.025, 0.02), (0.0, -0.055, 0.012), M["MI_Paint_Cream"])
    sb.soften(hinge, 0.006, 2)
    finish([shell, hinge], "SM_Pickup_Seashell")


def rope_coil():
    sb.reset_scene()
    mats()
    parts = []
    for k, r in enumerate((0.2, 0.165, 0.13, 0.1)):
        parts.append(sb.add_torus("coil", r, 0.024, (0.0, 0.0, 0.024 + (k % 2) * 0.01), (0, 0, k * 0.7), 32, 8, M["MI_Rope_Hemp"]))
    end = tb.spar("end", 0.42, 0.022, 0.02, M["MI_Rope_Hemp"], 83, sides=8, sweep=0.12, axis=(1, 0.4, 0), facets=8)
    end.data.transform(Matrix.Translation((0.18, 0.05, 0.022)))
    parts.append(end)
    finish(parts, "SM_Pickup_RopeCoil")


def glass_float():
    sb.reset_scene()
    mats()
    parts = [sphere("glass", 0.13, (0.0, 0.0, 0.13), "MI_Glass_Dark")]
    for k in range(3):
        parts.append(sb.add_torus("net", 0.136, 0.012, (0.0, 0.0, 0.13), (math.pi / 2, 0, k * math.pi / 3), 28, 6, M["MI_Rope_Hemp"]))
    parts.append(sb.add_torus("band", 0.115, 0.012, (0.0, 0.0, 0.2), (0, 0, 0), 24, 6, M["MI_Rope_Hemp"]))
    finish(parts, "SM_Pickup_GlassFloat")


def ancient_shard():
    sb.reset_scene()
    mats()
    # A broken stone socket, and the shard leaning in it: a tapered six-sided crystal with a chiselled tip.
    socket = sb.add_cylinder("socket", 0.16, 0.12, (0.0, 0.0, 0.06), "Z", 7, M["MI_Stone_Ruin"], radius_top=0.13)
    sb.soften(socket, 0.02, 2)
    bm = bmesh.new()
    sides, h = 6, 0.42
    base = [bm.verts.new((0.055 * math.cos(i * 2 * math.pi / sides), 0.055 * math.sin(i * 2 * math.pi / sides), 0.0)) for i in range(sides)]
    top = [bm.verts.new((0.04 * math.cos(i * 2 * math.pi / sides + 0.2), 0.04 * math.sin(i * 2 * math.pi / sides + 0.2), h * 0.8)) for i in range(sides)]
    tip = bm.verts.new((0.01, 0.0, h))
    for i in range(sides):
        bm.faces.new((base[i], base[(i + 1) % sides], top[(i + 1) % sides], top[i]))
        bm.faces.new((top[i], top[(i + 1) % sides], tip))
    bm.faces.new(list(reversed(base)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("crystal")
    bm.to_mesh(me)
    bm.free()
    crystal = sb.new_object("crystal", me)
    crystal.data.materials.append(M["MI_AncientGlow"])
    crystal.data.transform(Matrix.Translation((0.0, 0.0, 0.06)) @ Matrix.Rotation(math.radians(14), 4, "X"))
    finish([socket, crystal], "SM_Pickup_AncientShard")


# --- D73: materials (M6 step 2) --------------------------------------------------------------------------------------
import build_boat as bb  # noqa: E402  (pieces in Unreal coordinates: block, rod, ring, lathe, staved)


def bb_finish(parts, name):
    """As finish(), for pieces built with build_boat's helpers (their materials are bb.M)."""
    for p in parts:
        if not p.data.uv_layers:
            sb.world_box_uv(p, 0.5)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=0.8, seed=len(name), bevel_max=0.02)
    sb.save_blend(name)
    sb.export_fbx([obj], os.path.join("Pickups", name + ".fbx"))
    print("SB_PICKUP", name, len(obj.data.polygons))


def canvas():
    sb.reset_scene()
    bb.mats()
    parts = []
    # Three folds of cloth, each a little offset and sagging, then twine crosswise.
    for k in range(3):
        fold = bb.block("fold", (0.4 - 0.02 * k, 0.28 - 0.015 * k, 0.04), (0.006 * k, -0.005 * k, 0.022 + 0.038 * k), "MI_Canvas", 0.016)
        sb.jitter_vertices(fold, 0.004, 10 + k)
        parts.append(fold)
    for axis, size in (((1, 0, 0), (0.15, 0.14)), ((0, 1, 0), (0.21, 0.14))):
        tie = bb.ring("twine", 1.0, 0.006, (0.0, 0.0, 0.07), axis, "MI_Rope_Hemp", 28)
        for v in tie.data.vertices:
            # Squash the ring onto the bolt's section (a band round it, not a circle).
            if axis == (1, 0, 0):
                v.co.y, v.co.z = v.co.y * size[0], 0.07 + (v.co.z - 0.07) * size[1] * 0.5
            else:
                v.co.x, v.co.z = v.co.x * size[0], 0.07 + (v.co.z - 0.07) * size[1] * 0.5
        parts.append(tie)
    parts.append(bb.block("twine_knot", (0.03, 0.03, 0.02), (0.0, 0.0, 0.142), "MI_Rope_Hemp", 0.008))
    bb_finish(parts, "SM_Pickup_Canvas")


def iron_nails():
    sb.reset_scene()
    bb.mats()
    parts = [bb.lathe("twist", [(0.0, 0.0), (0.06, 0.0), (0.07, 0.03), (0.055, 0.075), (0.02, 0.1), (0.028, 0.125), (0.0, 0.13)],
                      (0.0, 0.0, 0.0), "MI_Canvas", 14)]
    sb.jitter_vertices(parts[0], 0.004, 5)
    parts.append(bb.ring("twist_tie", 0.022, 0.006, (0.0, 0.0, 0.1), (0, 0, 1), "MI_Rope_Hemp", 12))
    # Spilled nails: square iron shanks with flat heads.
    for k, (x, y, yaw) in enumerate(((0.11, 0.03, 20), (0.09, -0.06, 75), (0.14, -0.02, 130), (-0.1, 0.07, 160))):
        a = math.radians(yaw)
        d = (math.cos(a) * 0.075, math.sin(a) * 0.075)
        parts.append(bb.block("nail_shank", (0.075, 0.008, 0.008), (x, y, 0.006), "MI_Metal_Iron", 0.002))
        sb.transform_obj(parts[-1], (0, 0, -a), (x, -y, 0.006))
        parts.append(bb.disc("nail_head", 0.011, 0.005, (x + d[0] / 2, y + d[1] / 2, 0.008), (math.cos(a), math.sin(a), 0), "MI_Metal_Iron", 8))
    bb_finish(parts, "SM_Pickup_IronNails")


def tar():
    sb.reset_scene()
    bb.mats()
    radius = lambda s: 0.11 + 0.025 * math.sin(math.pi * s)
    parts = bb.staved("keg", radius, 0.3, 12, (0.0, 0.0, 0.0), "MI_Wood_Tarred", hoops=(0.14, 0.86))
    # The open head: black, glossy tar near the top, a few drips over the rim.
    parts.append(bb.disc("tar_top", radius(0.92) - 0.012, 0.02, (0.0, 0.0, 0.27), (0, 0, 1), "MI_Caulk_Dark", 20))
    for k, a in enumerate((0.4, 2.2, 4.1)):
        x, y = math.cos(a) * (radius(0.95) + 0.006), math.sin(a) * (radius(0.95) + 0.006)
        parts.append(bb.block("tar_drip", (0.016, 0.03, 0.06 + 0.02 * k), (x, y, 0.27 - 0.03 - 0.01 * k), "MI_Caulk_Dark", 0.008))
        sb.transform_obj(parts[-1], (0, 0, -a), (x, -y, 0.24))
    bb_finish(parts, "SM_Pickup_Tar")


def pale_float():
    sb.reset_scene()
    bb.mats()
    parts = [bb.lathe("float", [(0.0, 0.0)] + [(0.13 * math.sin(math.pi * k / 16), 0.13 - 0.13 * math.cos(math.pi * k / 16)) for k in range(1, 16)]
                      + [(0.0, 0.26)], (0.0, 0.0, 0.0), "MI_Paint_CreamAged", 24)]
    # The moulding seam round its middle and an eye on top, as a thing made in a mould would have.
    parts.append(bb.ring("seam", 0.131, 0.006, (0.0, 0.0, 0.13), (0, 0, 1), "MI_Paint_CreamAged", 32))
    parts.append(bb.lathe("eye_base", [(0.0, 0.0), (0.03, 0.0), (0.022, 0.02), (0.0, 0.02)], (0.0, 0.0, 0.255), "MI_Paint_CreamAged", 12))
    parts.append(bb.ring("eye", 0.022, 0.008, (0.0, 0.0, 0.295), (0, 1, 0), "MI_Paint_CreamAged", 14))
    # A rounded plaque where the number is moulded (the description says it).
    parts.append(bb.block("plaque", (0.05, 0.012, 0.035), (0.0, -0.126, 0.17), "MI_Paint_CreamAged", 0.005))
    bb_finish(parts, "SM_Pickup_PaleFloat")


BUILDERS = {"driftwood": driftwood, "seashell": seashell, "rope": rope_coil, "float": glass_float, "shard": ancient_shard,
            "canvas": canvas, "nails": iron_nails, "tar": tar, "palefloat": pale_float}

if __name__ == "__main__":
    for key in sb.script_args() or list(BUILDERS):
        BUILDERS[key]()
    print("SB_PICKUPS_OK")
