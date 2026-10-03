"""Underwater content for the visual test cove (D28): the sculpted sand seabed and sea-grass clumps.

  SM_Seabed           180 x 140 m sand floor, 1 m cells, pivot = world origin at the water level (placed at 0,0,0).
                      Beach behind the coastal rocks (wadeable shallows), about 2.3 m under the pier (its posts
                      reach 2.5 m down), a drop-off past the pier end to the 17.5 m floor of the open water,
                      shallower ridges towards the headlands. Uneven sand: broad soft mounds plus ripples.
  SM_SeaGrass_A/B/C   clumps of chunky, curved ribbon blades (tall / medium / short and sparse); pivot at the root.
                      UV.v runs from the root (0) to the tip (1) in Unreal, used for colour and sway.

Shapes are defined in Unreal coordinates (X towards the sea along the pier, Y across); Blender Y = -Unreal Y.
Run: Tools/Build-BlenderAsset.ps1 build_seabed [seabed] [grass]
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

FLOOR = -17.5          # top of the flat far seabed block in the map (metres)
X_RANGE = (-75.0, 105.0)
Y_RANGE = (-70.0, 70.0)
CELL = 1.0

# Depth profile along X (Unreal metres, z relative to the still water level).
PROFILE = [(-75, FLOOR), (-58, -7.0), (-45, -1.6), (-34, 0.4), (-24, 0.6), (-16, 0.1), (-11, -0.5), (-5, -1.4),
           (-1.5, -2.15), (15.0, -2.3), (20.0, -3.4), (30.0, -6.5), (45.0, -10.5), (65.0, -14.5), (88.0, -17.0),
           (105.0, FLOOR)]
HEADLANDS = [(9.0, 39.0), (20.0, 33.0), (34.0, 43.0)]   # rock positions (m)
SANDBANK = (32.0, 18.0, 25.0)   # centre x, y (m) and orientation (deg): light ground about 2 m deep (region looks)


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def profile(x):
    for (x0, z0), (x1, z1) in zip(PROFILE[:-1], PROFILE[1:]):
        if x <= x1:
            return z0 + (z1 - z0) * smooth((x - x0) / (x1 - x0))
    return PROFILE[-1][1]


def noise(x, y, scale, seed):
    """Smooth deterministic value: a few incommensurate sine waves (no tiling within the seabed)."""
    s = 0.0
    for i, (a, b, c) in enumerate(((0.83, 0.31, 1.3), (-0.27, 0.91, 4.1), (0.58, -0.66, 2.6), (0.12, 0.47, 5.3))):
        s += math.sin((a * x + b * y) / scale * (1.0 + 0.17 * i) + c + seed * 1.7)
    return s / 4.0


def height(x, y):
    # Wavy coastline: shifts the shallow profile along X (only near the coast).
    coast = smooth((8.0 - x) / 18.0)
    xs = x + coast * (3.0 * math.sin(y / 11.0) + 1.6 * math.sin(y / 5.3 + 1.0))
    z = profile(xs)
    # Deeper away from the pier, so the pier stands on a gentle sandy ridge.
    near_pier = smooth((x + 6.0) / 8.0) * smooth((24.0 - x) / 6.0)
    z -= near_pier * min(0.06 * max(abs(y) - 3.0, 0.0), 1.8)
    # Shallower towards the headland rocks (always submerged under them).
    for hx, hy in HEADLANDS:
        d = math.hypot(x - hx, y - hy)
        z = max(z, min(z + 7.0 * math.exp(-(d / 13.0) ** 2), -1.2))
    # Sandbank east of the pier: a long shallow plateau.
    bx, by, ba = SANDBANK
    a = math.radians(ba)
    u = (x - bx) * math.cos(a) + (y - by) * math.sin(a)
    v = -(x - bx) * math.sin(a) + (y - by) * math.cos(a)
    bank = math.exp(-((u / 15.0) ** 2 + (v / 8.0) ** 2) ** 1.5)
    z = max(z, -1.9 - 8.0 * (1.0 - bank))
    # Uneven sand: broad mounds and ripples, calm under the pier (posts) and on the dry beach.
    calm = 1.0 - 0.8 * smooth((3.5 - abs(y)) / 2.0) * smooth((x + 4.0) / 3.0) * smooth((17.0 - x) / 3.0)
    wet = smooth((0.2 - z) / 0.8)
    mounds = 0.55 * noise(x, y, 7.0, 1) + 0.25 * noise(x, y, 3.1, 2)
    phase = (x + 1.8 * math.sin(y / 7.0) + 0.9 * math.sin(y / 3.3)) / 3.4 * 2.0 * math.pi
    ripple = 0.11 * (math.sin(phase) + 0.25 * math.sin(2.0 * phase + 0.6)) * (0.6 + 0.4 * noise(x, y, 9.0, 3))
    z += calm * (mounds * (0.4 + 0.6 * wet) + ripple * wet)
    # Edges blend into the flat far floor.
    edge = max(smooth((abs(y) - 55.0) / 15.0), smooth((x - 95.0) / 10.0), smooth((-65.0 - x) / 10.0))
    return z + (FLOOR - z) * edge


def seabed():
    sb.reset_scene()
    mat = sb.get_material("MI_Sand", (0.76, 0.68, 0.52, 1))
    nx = int((X_RANGE[1] - X_RANGE[0]) / CELL)
    ny = int((Y_RANGE[1] - Y_RANGE[0]) / CELL)
    bm = bmesh.new()
    verts = []
    for j in range(ny + 1):
        y = Y_RANGE[0] + j * CELL
        row = []
        for i in range(nx + 1):
            x = X_RANGE[0] + i * CELL
            row.append(bm.verts.new((x, -y, height(x, y))))
        verts.append(row)
    for j in range(ny):
        for i in range(nx):
            # Blender Y is mirrored: keep the faces facing up.
            bm.faces.new((verts[j][i], verts[j + 1][i], verts[j + 1][i + 1], verts[j][i + 1]))
    bm.normal_update()
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
        f.smooth = True
    me = bpy.data.meshes.new("SM_Seabed")
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("SM_Seabed", me)
    obj.data.materials.append(mat)
    sb.world_box_uv(obj, 1.0)
    zs = [v.co.z for v in me.vertices]
    print("SB_SEABED faces", len(me.polygons), "z", round(min(zs), 2), round(max(zs), 2),
          "pier", round(height(5.0, 0.0), 2), round(height(13.6, 1.3), 2), "boat", round(height(9.0, 3.3), 2),
          "bank", round(height(*SANDBANK[:2]), 2))
    sb.save_blend("SM_Seabed")
    sb.export_fbx([obj], os.path.join("VisualTest", "SM_Seabed.fbx"))


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
    args = sb.script_args() or ["seabed", "grass"]
    if "seabed" in args:
        seabed()
    if "grass" in args:
        grass()
