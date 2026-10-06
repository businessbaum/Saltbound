"""The ship repair system (D74, M6 step 3): what a hole looks like, the patch nailed over it, the bucket, the water.

  SM_Hull_Hole       sprung planking on the bulwark's inner face: two board ends split and bent inward round a dark gap,
                     splinters at the break; 46 x 30 cm, built facing -Y (into the boat from the starboard side); origin on
                     the face at the gap's middle
  SM_Hull_Patch      a short fresh board nailed across a hole: 50 x 15 x 3 cm, four nails, a little crooked; same frame
  SM_Water_Spurt     the sea coming in: a twisted, tapering spout from the gap arcing down onto the deck (55 cm), its own
                     water material (MI_DeckWater); same frame
  SM_Boat_Bucket     the bucket for bailing: staved, two iron hoops, a rope bail; 26 cm across, 30 cm high, origin at its foot
  SM_Boat_BilgeWater the water that rises over the deck when she is low: the deck's outline inside the bulwarks as one
                     sheet (boat frame, z 0 = the deck at midship; the game raises it with the water aboard), MI_DeckWater

Unreal axes (+X forward, +Y starboard). Run: Tools/Build-BlenderAsset.ps1 build_repair [hole patch spurt bucket bilge]
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_boat as bb  # noqa: E402
import sb_blender as sb  # noqa: E402
import sb_timber as tb  # noqa: E402

WATER = "MI_DeckWater"


def mats():
    bb.mats()
    bb.M[WATER] = sb.get_material(WATER, (0.2, 0.55, 0.6, 1))


def export(obj, folder="Boat"):
    sb.save_blend(obj.name)
    sb.export_fbx([obj], os.path.join(folder, obj.name + ".fbx"))
    print("SB_REPAIR", obj.name, len(obj.data.polygons))


def jagged_board(name, x0, x1, z, width, break_side, seed, mat):
    """A plank from x0 to x1 (Unreal, along X) on the bulwark's inner face, broken at its end towards the gap: the break is a
    zig-zag of splits, and the last few centimetres bend inward (-Y) as if the sea had punched them in."""
    rng = random.Random(seed)
    bm = bmesh.new()
    n = 10
    rows = []
    teeth = [rng.uniform(-0.05, 0.05) for _ in range(5)]
    for i in range(n + 1):
        t = i / n
        row = []
        for j, zz in enumerate((-width / 2, -width / 4, 0.0, width / 4, width / 2)):
            x = x0 + (x1 - x0) * t
            if (break_side > 0 and i == n) or (break_side < 0 and i == 0):
                x += break_side * teeth[j]
            k = (x - (x1 if break_side > 0 else x0)) * break_side          # 0 at the break, < 0 away from it
            bend = 0.06 * max(0.0, 1.0 + k / 0.12) ** 2
            row.append((x, -bend, z + zz))
        rows.append(row)
    vs_front = [[bm.verts.new(bb.U(p)) for p in row] for row in rows]
    vs_back = [[bm.verts.new(bb.U((p[0], p[1] + 0.028, p[2]))) for p in row] for row in rows]
    for i in range(n):
        for j in range(4):
            bm.faces.new([vs_front[i][j], vs_front[i + 1][j], vs_front[i + 1][j + 1], vs_front[i][j + 1]])
            bm.faces.new([vs_back[i][j + 1], vs_back[i + 1][j + 1], vs_back[i + 1][j], vs_back[i][j]])
    for i in range(n):
        for a, b in ((0, 0), (4, 4)):
            bm.faces.new([vs_front[i][a], vs_back[i][a], vs_back[i + 1][a], vs_front[i + 1][a]])
    for i in (0, n):
        for j in range(4):
            bm.faces.new([vs_front[i][j], vs_front[i][j + 1], vs_back[i][j + 1], vs_back[i][j]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = bb.mesh_object(name, bm, [mat])
    sb.world_box_uv(obj, 0.5)
    return obj


def build_hole():
    parts = []
    rng = random.Random(7)
    # The gap: an uneven dark hole (the sea's side), smaller than the break.
    bm = bmesh.new()
    ring = []
    for k in range(14):
        a = 2 * math.pi * k / 14
        r = 1.0 + rng.uniform(-0.25, 0.2)
        ring.append(bm.verts.new(bb.U((0.11 * r * math.cos(a), 0.012, 0.085 * r * math.sin(a)))))
    bm.faces.new(ring)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    gap = bb.mesh_object("hole_gap", bm, ["MI_Caulk_Dark"])
    sb.world_box_uv(gap, 0.5)
    parts.append(gap)
    # Two strakes broken across it: each in two halves whose ends bend in round the gap.
    for k, (z, w) in enumerate(((0.06, 0.115), (-0.062, 0.12))):
        parts.append(jagged_board("strake", -0.34, -0.03 - 0.02 * k, z, w, +1, 70 + k, "MI_Wood_TeakDark"))
        parts.append(jagged_board("strake", 0.04 + 0.015 * k, 0.33, z, w, -1, 80 + k, "MI_Wood_TeakDark"))
    # Splinters sticking out of the break.
    for k in range(6):
        x = rng.uniform(-0.09, 0.09)
        z = rng.choice((-1, 1)) * rng.uniform(0.02, 0.1)
        parts.append(bb.rod("splinter", (x, -0.03, z), (x + rng.uniform(-0.04, 0.04), -0.08 - 0.04 * rng.random(), z + rng.uniform(-0.03, 0.03)),
                            0.005, "MI_Wood_TeakLight", 5))
    obj = bb.finish(parts, "SM_Hull_Hole", wear=1.0, chips=0.9)
    export(obj)


def build_patch():
    parts = [tb.rough_plank("patch", 0.5, 0.15, 0.03, bb.U((0.0, -0.025, 0.0)), (math.pi / 2, 0, math.radians(4)),
                           bb.M["MI_Wood_TeakLight"], seed=61, wear=0.6, grooves=(1, 2))]
    for x in (-0.21, -0.18, 0.18, 0.21):
        z = 0.035 if abs(x) > 0.19 else -0.035
        parts.append(bb.disc("patch_nail", 0.011, 0.006, (x, -0.044, z + 0.0035 * x / 0.2), (0, -1, 0), "MI_Metal_Iron", 8))
    obj = bb.finish(parts, "SM_Hull_Patch", wear=0.4, chips=0.2)
    export(obj)


def build_spurt():
    # A spout along a falling arc: out of the gap (-Y), then down onto the deck; round at the gap, a thin fan where it lands.
    bm = bmesh.new()
    rings = []
    n, segs = 16, 10
    for i in range(n + 1):
        t = i / n
        y = -0.42 * t
        z = -0.32 * t * t - 0.02
        r_w = 0.035 * (1.0 - 0.35 * t) + 0.05 * t * t
        r_h = 0.03 * (1.0 - 0.7 * t)
        twist = 1.4 * t
        ring = []
        for j in range(segs):
            a = 2 * math.pi * j / segs + twist
            ring.append(bm.verts.new(bb.U((r_w * math.cos(a), y, z + r_h * math.sin(a)))))
        rings.append(ring)
    for i in range(n):
        for j in range(segs):
            bm.faces.new([rings[i][j], rings[i][(j + 1) % segs], rings[i + 1][(j + 1) % segs], rings[i + 1][j]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = bb.mesh_object("SM_Water_Spurt", bm, [WATER])
    for p in obj.data.polygons:
        p.use_smooth = True
    # UV: U round the spout, V along it (the material runs the water along V).
    uv = obj.data.uv_layers.new(name="UVMap")
    for poly in obj.data.polygons:
        for li in poly.loop_indices:
            vi = obj.data.loops[li].vertex_index
            uv.data[li].uv = ((vi % segs) / segs, (vi // segs) / n)
    export(obj)


def build_bucket():
    radius = lambda s: 0.115 + 0.02 * s
    parts = bb.staved("bucket", radius, 0.28, 10, (0.0, 0.0, 0.0), "MI_Wood_DockWeathered", hoops=(0.18, 0.82))
    # The rope bail over the top, through two ears.
    for side in (-1, 1):
        parts.append(bb.block("bucket_ear", (0.03, 0.02, 0.05), (0.0, side * 0.14, 0.27), "MI_Wood_DockWeathered", 0.006))
    pts = [(0.0, -0.14, 0.28)] + [(0.0, -0.14 * math.cos(math.pi * k / 8), 0.28 + 0.16 * math.sin(math.pi * k / 8)) for k in range(1, 8)] + [(0.0, 0.14, 0.28)]
    parts += bb.rope("bucket_bail", pts, 0.009)
    obj = bb.finish(parts, "SM_Boat_Bucket", wear=0.8, chips=0.5)
    export(obj)


def build_bilge():
    # The deck inside the bulwarks, as one sheet (a fan from the middle), at the deck's own height along it (the sheer
    # rises to the ends): the game lifts it by the water's depth.
    inner = bb.BULWARK_T if hasattr(bb, "BULWARK_T") else 0.07
    xs = [-4.7 + 9.4 * k / 40 for k in range(41)]
    bm = bmesh.new()
    port, stbd = [], []
    for x in xs:
        hb = max(bb.half_beam(x) - inner, 0.02)
        z = bb.sheer_z(x) - bb.sheer_z(0.0)
        port.append(bm.verts.new(bb.U((x, -hb, z))))
        stbd.append(bm.verts.new(bb.U((x, hb, z))))
    for i in range(len(xs) - 1):
        bm.faces.new([port[i], port[i + 1], stbd[i + 1], stbd[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    if sum(f.normal.z for f in bm.faces) < 0:
        bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
    obj = bb.mesh_object("SM_Boat_BilgeWater", bm, [WATER])
    sb.world_box_uv(obj, 1.0)
    export(obj)


BUILDERS = {"hole": build_hole, "patch": build_patch, "spurt": build_spurt, "bucket": build_bucket, "bilge": build_bilge}


def main():
    for n in sb.script_args() or list(BUILDERS):
        sb.reset_scene()
        mats()
        BUILDERS[n]()


if __name__ == "__main__":
    main()
