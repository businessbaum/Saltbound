"""Underwater content for the visual test cove (D28): the sculpted sand seabed and sea-grass clumps.

  SM_Seabed           collision floor: 180 x 140 m, 1 m cells, the broad shape only (hidden in the map). Pivot =
                      world origin at the water level (placed at 0,0,0).
  SM_Seabed_Sand      the visible sand (Nanite, D37): the same floor at 0.2 m cells. The dry beach is sculpted (wind
                      ripples, mounds, swash ridges) with vertex colours for lit crests and shaded troughs and its calm
                      beach texture (vertex alpha blends MI_Sand's second texture set in); the sea floor below keeps
                      the broad shape and the painted sand.
                      Shapes: seabed_shape.py (shared with the map script). The broad shape:
                      Beach behind the coastal rocks (wadeable shallows), about 2.3 m under the pier (its posts
                      reach 2.5 m down), a drop-off past the pier end to the 17.5 m floor of the open water,
                      shallower ridges towards the headlands. Uneven sand: broad soft mounds plus ripples.
  SM_SeaGrass_A/B/C   clumps of chunky, curved ribbon blades (tall / medium / short and sparse); pivot at the root.
                      UV.v runs from the root (0) to the tip (1) in Unreal, used for colour and sway.

Shapes are defined in Unreal coordinates (X towards the sea along the pier, Y across); Blender Y = -Unreal Y.
Run: Tools/Build-BlenderAsset.ps1 build_seabed [seabed] [sand] [grass]
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

from seabed_shape import FLOOR, SANDBANK, X_RANGE, Y_RANGE, base, beach_weight, detail  # noqa: E402

COLLISION_CELL = 1.0
SAND_CELL = 0.2


def grid(cell):
    nx = int(round((X_RANGE[1] - X_RANGE[0]) / cell))
    ny = int(round((Y_RANGE[1] - Y_RANGE[0]) / cell))
    xs = [X_RANGE[0] + i * cell for i in range(nx + 1)]
    ys = [Y_RANGE[0] + j * cell for j in range(ny + 1)]
    faces = []
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            # Blender Y is mirrored (y -> -y): this winding keeps the faces facing up.
            faces.append((a, a + nx + 1, a + nx + 2, a + 1))
    return xs, ys, faces


def make_mesh(name, verts, faces, cell):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    me.shade_smooth()
    obj = sb.new_object(name, me)
    obj.data.materials.append(sb.get_material("MI_Sand", (0.76, 0.68, 0.52, 1)))
    # UV0 (top projection, 1 m per unit): only for tangents, the sand material projects in world space.
    import numpy as np
    co = np.empty(len(me.vertices) * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    vi = np.empty(len(me.loops), dtype=np.int32)
    me.loops.foreach_get("vertex_index", vi)
    uv = me.uv_layers.new(name="UVMap")
    uv.data.foreach_set("uv", co[vi, :2].ravel())
    return obj


def seabed_collision():
    """SM_Seabed: the broad shape at 1 m cells, used only for collision (hidden in the map)."""
    sb.reset_scene()
    xs, ys, faces = grid(COLLISION_CELL)
    verts = [(x, -y, base(x, y)) for y in ys for x in xs]
    obj = make_mesh("SM_Seabed", verts, faces, COLLISION_CELL)
    zs = [v[2] for v in verts]
    print("SB_SEABED collision faces", len(faces), "z", round(min(zs), 2), round(max(zs), 2),
          "pier", round(base(5.0, 0.0), 2), round(base(13.6, 1.3), 2), "boat", round(base(9.0, -3.5), 2),
          "bank", round(base(*SANDBANK[:2]), 2))
    sb.save_blend("SM_Seabed")
    sb.export_fbx([obj], os.path.join("VisualTest", "SM_Seabed.fbx"))


def seabed_sand():
    """SM_Seabed_Sand: the visible sand (Nanite), 0.2 m cells with the sculpted ripples. Vertex colours for the
    sand material: R lit crests, G shaded troughs, B broad tone patches."""
    sb.reset_scene()
    xs, ys, faces = grid(SAND_CELL)
    verts, cols = [], []
    for y in ys:
        for x in xs:
            zb = base(x, y)
            dz, crest, trough, tone = detail(x, y, zb)
            verts.append((x, -y, zb + dz))
            # Alpha 1 = sea-floor sand, 0 = beach sand (blended over the last 40 cm below the water level).
            w = beach_weight(zb)
            cols.extend((crest, trough, 0.5 + (tone - 0.5) * w, 1.0 - w))
    obj = make_mesh("SM_Seabed_Sand", verts, faces, SAND_CELL)
    attr = obj.data.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", cols)
    obj.data.color_attributes.active_color = attr
    print("SB_SEABED sand faces", len(faces), "verts", len(verts))
    sb.save_blend("SM_Seabed_Sand")
    sb.export_fbx([obj], os.path.join("VisualTest", "SM_Seabed_Sand.fbx"))


GRASS = (
    # name, blades, height range (m), clump radius (m), seed
    ("SM_SeaGrass_A", 14, (0.75, 1.25), 0.28, 31),
    ("SM_SeaGrass_B", 11, (0.45, 0.85), 0.24, 32),
    ("SM_SeaGrass_C", 7, (0.28, 0.55), 0.3, 33),
)


def blade(bm, rnd, base, h):
    """Chunky ribbon with a soft centre ridge, curving away from the clump; UV.v = 1 - t (Unreal: root 0)."""
    segs = 6
    yaw = rnd.uniform(0.0, 2.0 * math.pi)
    lean = Vector((math.cos(yaw), math.sin(yaw), 0.0))
    side = Vector((-lean.y, lean.x, 0.0))
    out = Vector((base.x, base.y, 0.0))
    if out.length > 1e-4:
        lean = (lean * 0.4 + out.normalized() * 0.6).normalized()
        side = Vector((-lean.y, lean.x, 0.0))
    bend = rnd.uniform(0.15, 0.45) * h
    twist = rnd.uniform(-0.6, 0.6)
    width = rnd.uniform(0.055, 0.085)
    uv = []
    rows = []
    for k in range(segs + 1):
        t = k / segs
        centre = base + Vector((0.0, 0.0, h * t)) + lean * (bend * t * t)
        w = width * (1.0 - 0.75 * t ** 1.5) * (1.0 if k < segs else 0.35)
        a = twist * t
        s = side * math.cos(a) + lean * math.sin(a)
        ridge = lean * (-0.012 * (1.0 - t))
        row = [bm.verts.new(centre - s * w), bm.verts.new(centre + ridge), bm.verts.new(centre + s * w)]
        rows.append(row)
        uv.append(1.0 - t)
    faces = []
    for k in range(segs):
        for c in range(2):
            f = bm.faces.new((rows[k][c], rows[k][c + 1], rows[k + 1][c + 1], rows[k + 1][c]))
            f.smooth = True
            faces.append((f, k, c))
    return faces, uv


def grass():
    for name, count, (h0, h1), radius, seed in GRASS:
        sb.reset_scene()
        mat = sb.get_material("MI_SeaGrass", (0.2, 0.45, 0.15, 1))
        rnd = random.Random(seed)
        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.new("UVMap")
        for _ in range(count):
            r = radius * math.sqrt(rnd.random())
            a = rnd.uniform(0.0, 2.0 * math.pi)
            base = Vector((r * math.cos(a), r * math.sin(a), -0.05))
            faces, vs = blade(bm, rnd, base, rnd.uniform(h0, h1))
            # Corners were created as (row k, col c), (k, c+1), (k+1, c+1), (k+1, c); u across the blade.
            for f, k, c in faces:
                corners = ((c, k), (c + 1, k), (c + 1, k + 1), (c, k + 1))
                for loop, (col, row) in zip(f.loops, corners):
                    loop[uv_layer].uv = (col * 0.5, vs[row])
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        obj = sb.new_object(name, me)
        obj.data.materials.append(mat)
        print("SB_GRASS", name, "faces", len(me.polygons))
        sb.save_blend(name)
        sb.export_fbx([obj], os.path.join("VisualTest", name + ".fbx"))


if __name__ == "__main__":
    args = sb.script_args() or ["seabed", "sand", "grass"]
    if "seabed" in args:
        seabed_collision()
    if "sand" in args:
        seabed_sand()
    if "grass" in args:
        grass()
