"""The sea's events (D88, FH5: the light under the water).

  SM_SeaLight   what passes under the boat on the first night at sea, seen only as a glow through dark water: a long body, 11 m,
                its nose rounded, thickest a third of the way back, its tail drawn out long; a little flattened; four raised
                bands round it, the Old World's lines (the Spire's ring, the spring's pipe). One slot, MI_SeaGlow (setup_sea_events.py:
                soft-edged): the game drives its glow. Pivot at its middle, along +X (where it swims).
Run: Tools/Build-BlenderAsset.ps1 build_sea_events
"""
import math
import os
import sys

import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_boat as bb  # noqa: E402
import sb_blender as sb  # noqa: E402

LENGTH = 11.0
GIRTH = 1.2


def radius(t):
    """Half width at t (0 the nose .. 1 the tail's tip): a round nose, thickest at a third, a long thin tail."""
    if t < 0.33:
        r = 0.8 * math.sqrt(max(0.0, 1.0 - ((0.33 - t) / 0.33) ** 2))
    else:
        r = 0.8 * (1.0 - ((t - 0.33) / 0.67) ** 1.6) + 0.04
    for c in (0.2, 0.38, 0.56, 0.74):
        r += 0.06 * math.exp(-((t - c) / 0.012) ** 2) * (r / 0.8 + 0.3)
    return max(r, 0.0) * GIRTH


def build_body():
    sb.reset_scene()
    bb.mats()
    bb.M["MI_SeaGlow"] = sb.get_material("MI_SeaGlow", (0.35, 0.95, 0.85, 1.0))
    bm = bmesh.new()
    rings, segs = 120, 32
    verts = []
    for i in range(rings + 1):
        t = i / rings
        x = LENGTH * (0.5 - t)                       # the nose at +X
        r = radius(t)
        sag = -0.15 * t * t                          # the tail hangs a little
        row = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            row.append(bm.verts.new(bb.U((x, r * math.cos(a), sag + r * 0.62 * math.sin(a)))))
        verts.append(row)
    for i in range(rings):
        for k in range(segs):
            k1 = (k + 1) % segs
            bm.faces.new((verts[i][k], verts[i][k1], verts[i + 1][k1], verts[i + 1][k]))
    nose = bm.verts.new(bb.U((LENGTH * 0.5 + 0.02, 0.0, 0.0)))
    tail = bm.verts.new(bb.U((-LENGTH * 0.5 - 0.02, 0.0, -0.15)))
    for k in range(segs):
        k1 = (k + 1) % segs
        bm.faces.new((nose, verts[0][k1], verts[0][k]))
        bm.faces.new((tail, verts[-1][k], verts[-1][k1]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
    obj = bb.mesh_object("SM_SeaLight", bm, ["MI_SeaGlow"])
    sb.world_box_uv(obj, 1.0)
    sb.save_blend(obj.name)
    sb.export_fbx([obj], os.path.join("Events", obj.name + ".fbx"))
    print("SB_SEA_EVENT", obj.name, len(obj.data.polygons))


build_body()
