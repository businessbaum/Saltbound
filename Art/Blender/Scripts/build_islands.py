"""Islands beyond the cove (D47): a first destination with a ruin, and island masses on the horizon.

  SM_Island_Ruin      "Ruin Isle", about 1.7 km out: a low sandy island some 110 m across with a gentle beach all
                      round, soft dunes and a knoll where the ruin stands, a turquoise shelf about 3 m deep and a skirt
                      down to the deep floor (60 m). Visible mesh (Nanite): 0.5 m cells inside +-90 m with sculpted
                      wind ripples on the dry sand, 4 m cells beyond (overlapping, a little lower, so no seam shows).
                      Vertex colours as the cove sand (MI_Sand): R lit crests, G shaded troughs, B tone, A 1 = sea
                      floor sand / 0 = beach sand. Origin = island centre at the water level.
  SM_Island_Ruin_Col  the same broad shape at 2 m cells for collision (hidden in the map; Nanite meshes collide with
                      their reduced fallback).
  SM_Ruin_Arch        a broken gateway of bevelled stone blocks: a step, two piers (one broken), half an arch with
                      the keystone still in place and the ancient glow set into it, fallen arch stones.
  SM_Ruin_Wall        a low broken wall corner, courses of uneven blocks with a jagged top.
  SM_Island_Far_A/B/C big chiselled island masses for the horizon, 4-7 km out (the haze fades them).

Unreal coordinates in metres (Blender Y = -Unreal Y). Run: Tools/Build-BlenderAsset.ps1 build_islands [ruin] [col]
[arch] [wall] [far]
"""
import math
import os
import random
import sys

import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402
from seabed_shape import RIPPLE_HEIGHT, RIPPLE_SPACING, noise, smooth  # noqa: E402

M = {}
R0 = 55.0                      # mean coast radius (m)
KNOLL = (14.0, -8.0, 16.0)     # centre x, y and radius of the knoll the ruin stands on
INNER, INNER_CELL = 90.0, 0.5  # fine grid half size and cell
OUTER, OUTER_CELL = 220.0, 4.0
COL_CELL = 2.0
DEEP = -60.0


def mats():
    for n, c in {"MI_Sand": (0.76, 0.68, 0.52), "MI_Rock_Coast": (0.45, 0.42, 0.38), "MI_Stone_Ruin": (0.42, 0.4, 0.36),
                 "MI_AncientGlow": (0.35, 0.95, 0.85)}.items():
        M[n] = sb.get_material(n, (*c, 1))


def coast_radius(a):
    """An irregular coast: a few lobes and bays."""
    return R0 * (1.0 + 0.16 * math.sin(3 * a + 0.7) + 0.09 * math.sin(5 * a + 2.1) + 0.05 * math.sin(9 * a + 4.0))


def base(x, y):
    """Broad island shape (m above the still water)."""
    t = math.hypot(x, y) / coast_radius(math.atan2(y, x))
    if t < 1.0:
        # Beach rising 1.6 m over the outer quarter, the inland a little higher with soft dunes.
        z = 1.6 * smooth((1.0 - t) / 0.25) + 1.2 * smooth((0.75 - t) / 0.4)
        z += 0.9 * max(noise(x, y, 9.0, 3), 0.0) * smooth((0.8 - t) / 0.3)
    else:
        # The shelf (about 3 m deep, turquoise water) and the skirt down to the deep floor.
        z = -3.0 * smooth((t - 1.0) / 0.35) - (-DEEP - 3.0) * smooth((t - 1.3) / 1.7)
    kx, ky, kr = KNOLL
    z += 5.5 * smooth(1.0 - math.hypot(x - kx, y - ky) / kr)
    return z


def beach_weight(z):
    """0 on the sea floor, 1 on the beach (the sand material blends to its beach texture just below the water)."""
    return smooth((z + 0.35) / 0.4)


def detail(x, y, z):
    """Wind ripples on the dry sand (as the cove's beach): (dz, crest, trough, tone)."""
    tone = 0.5 + 0.5 * noise(x, y, 4.5, 7)
    dry = smooth((z - 0.05) / 0.2)
    if dry <= 1e-3:
        return 0.0, 0.0, 0.0, tone
    patch = smooth((0.5 + 0.5 * noise(x, y, 9.0, 11) + 0.25 * noise(x, y, 3.5, 12) + 0.12) / 0.25)
    theta = 0.35 * noise(x, y, 18.0, 13)
    u = x * math.cos(theta) + y * math.sin(theta)
    phi = 2.0 * math.pi * u / RIPPLE_SPACING + 3.2 * noise(x, y, 6.0, 14) + 1.3 * noise(x, y, 2.4, 15)
    phi += 0.5 * math.sin(phi)
    p = (0.5 + 0.5 * math.cos(phi)) ** 2.0
    amt = dry * patch * (0.7 + 0.3 * noise(x, y, 3.0, 16))
    dz = RIPPLE_HEIGHT * amt * (p - 0.375)
    crest = smooth((p - 0.55) / 0.35) * amt
    trough = smooth((0.18 - p) / 0.18) * amt
    return dz, crest, trough, tone


def grid_faces(n):
    faces = []
    for j in range(n):
        for i in range(n):
            a = j * (n + 1) + i
            faces.append((a, a + n + 1, a + n + 2, a + 1))   # Blender Y is mirrored: this winding faces up
    return faces


def make_mesh(name, verts, faces, cols=None):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    me.shade_smooth()
    obj = sb.new_object(name, me)
    obj.data.materials.append(M["MI_Sand"])
    co = np.array([v.co[:] for v in me.vertices], dtype=np.float32)
    vi = np.empty(len(me.loops), dtype=np.int32)
    me.loops.foreach_get("vertex_index", vi)
    uv = me.uv_layers.new(name="UVMap")
    uv.data.foreach_set("uv", co[vi, :2].ravel())       # top projection (tangents only; the material is world space)
    if cols is not None:
        attr = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
        attr.data.foreach_set("color", cols)
        me.color_attributes.active_color = attr
    return obj


def island_ruin():
    sb.reset_scene()
    mats()
    verts, cols, faces = [], [], []
    # Fine grid with the sculpted beach.
    n = int(round(2 * INNER / INNER_CELL))
    for j in range(n + 1):
        y = -INNER + j * INNER_CELL
        for i in range(n + 1):
            x = -INNER + i * INNER_CELL
            zb = base(x, y)
            dz, crest, trough, tone = detail(x, y, zb)
            w = beach_weight(zb)
            verts.append((x, -y, zb + dz))
            cols.extend((crest, trough, 0.5 + (tone - 0.5) * w, 1.0 - w))
    faces += grid_faces(n)
    # Coarse skirt beyond, overlapping the fine grid's border and 0.2 m lower (deep water: no seam can show).
    off = len(verts)
    m = int(round(2 * OUTER / OUTER_CELL))
    for j in range(m + 1):
        y = -OUTER + j * OUTER_CELL
        for i in range(m + 1):
            x = -OUTER + i * OUTER_CELL
            verts.append((x, -y, base(x, y) - 0.2))
            cols.extend((0.0, 0.0, 0.5, 1.0))
    hole = INNER - 2 * OUTER_CELL
    for (a, b, c, d) in grid_faces(m):
        xs = [verts[off + k][0] for k in (a, b, c, d)]
        ys = [-verts[off + k][1] for k in (a, b, c, d)]
        if max(abs(v) for v in xs) <= hole and max(abs(v) for v in ys) <= hole:
            continue
        faces.append((off + a, off + b, off + c, off + d))
    obj = make_mesh("SM_Island_Ruin", verts, faces, cols)
    zs = [v[2] for v in verts]
    print("SB_ISLAND ruin faces", len(faces), "z", round(min(zs), 2), round(max(zs), 2), "knoll", round(base(*KNOLL[:2]), 2))
    sb.save_blend("SM_Island_Ruin")
    sb.export_fbx([obj], os.path.join("Islands", "SM_Island_Ruin.fbx"))


def island_collision():
    sb.reset_scene()
    mats()
    n = int(round(2 * OUTER / COL_CELL))
    verts = []
    for j in range(n + 1):
        y = -OUTER + j * COL_CELL
        for i in range(n + 1):
            x = -OUTER + i * COL_CELL
            verts.append((x, -y, base(x, y)))
    obj = make_mesh("SM_Island_Ruin_Col", verts, grid_faces(n))
    print("SB_ISLAND collision faces", n * n)
    sb.save_blend("SM_Island_Ruin_Col")
    sb.export_fbx([obj], os.path.join("Islands", "SM_Island_Ruin_Col.fbx"))


# --- ruins ------------------------------------------------------------------------------------------------------
def stone(rng, size, center, yaw=0.0, mat="MI_Stone_Ruin", tilt=0.025):
    """One bevelled stone block (Blender coordinates), a little out of true like old masonry."""
    b = sb.add_box("block", size, (0, 0, 0), M[mat])
    sb.transform_obj(b, (rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt), yaw + rng.uniform(-tilt, tilt)))
    for v in b.data.vertices:
        v.co.x += center[0]
        v.co.y += center[1]
        v.co.z += center[2]
    sb.soften(b, min(size) * 0.09, 2)
    return b


def finish_ruin(parts, name, seed):
    for p in parts:
        sb.world_box_uv(p, 1.0)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=1.0, seed=seed, bevel_max=0.06)
    sb.save_blend(name)
    sb.export_fbx([obj], os.path.join("Islands", name + ".fbx"))
    print("SB_RUIN", name, len(obj.data.polygons))


def ruin_arch():
    """Gateway facing +X (the way to the sea): span 3.6 m, piers 1.2 x 1.0 m in 0.55 m courses on a step."""
    sb.reset_scene()
    mats()
    rng = random.Random(47)
    parts = [stone(rng, (2.2, 6.6, 0.3), (0.0, 0.0, 0.15), tilt=0.004)]           # the step under both piers
    course, pier_w, depth, span = 0.55, 1.2, 1.0, 3.6
    for side, courses in ((-1, 6), (1, 4)):                                        # the +Y pier has broken off
        cy = side * (span / 2 + pier_w / 2)
        for k in range(courses):
            z = 0.3 + course * (k + 0.5)
            # Two blocks per course, the joint shifting from course to course.
            cut = 0.45 + (0.3 if k % 2 else 0.0)
            for (y0, y1) in ((-pier_w / 2, -pier_w / 2 + cut), (-pier_w / 2 + cut, pier_w / 2)):
                parts.append(stone(rng, (depth, y1 - y0 - 0.03, course - 0.03), (0.0, cy + (y0 + y1) / 2, z)))
    # The arch: voussoirs round a 1.8 m inner radius from the left pier's top; the right half has fallen, the keystone
    # and one stone beside it hold (stylised: ruins defy a little physics). The keystone carries the ancient glow.
    spring = 0.3 + 6 * course
    rad, n = span / 2 + 0.3, 11
    for k in range(n):
        if k < 4:
            continue                                                              # fallen (right side, +Y)
        a = math.pi * (k + 0.5) / n
        cy, cz = rad * math.cos(a), spring + rad * math.sin(a)
        b = stone(rng, (depth, 0.6, 0.55), (0.0, cy, cz), tilt=0.01)
        sb.transform_obj(b, (math.pi / 2 - a, 0.0, 0.0), (0.0, cy, cz))
        parts.append(b)
        if k == n // 2:
            parts.append(sb.add_box("glow", (0.06, 0.22, 0.3), (depth / 2 + 0.01, cy, cz), M["MI_AncientGlow"]))
    # Fallen arch stones on the ground beside the broken pier.
    for k, (dx, dy, yaw, roll) in enumerate(((1.4, 2.6, 0.4, 0.3), (0.9, 3.4, 1.2, -0.5), (-1.3, 3.1, 2.0, 0.9))):
        b = stone(rng, (depth * 0.95, 0.6, 0.55), (dx, dy, 0.27), yaw=yaw)
        sb.transform_obj(b, (roll, 0.0, 0.0), (dx, dy, 0.27))
        parts.append(b)
    finish_ruin(parts, "SM_Ruin_Arch", 48)


def ruin_wall():
    """Low broken wall corner: legs of 6 m (along X) and 4 m (along Y), 0.7 m thick, 0.45 m courses, jagged top."""
    sb.reset_scene()
    mats()
    rng = random.Random(49)
    parts = []
    course, thick = 0.45, 0.7
    # Leg 0 runs along X from the corner, leg 1 along -Y (Blender) from the corner block's side.
    for leg, length in ((0, 6.0), (1, 4.0)):
        for k in range(5):
            pos = -thick / 2 if leg == 0 else thick / 2
            first = True
            while pos < length - 0.2:
                ln = rng.uniform(0.75, 1.2) * (0.5 if first and k % 2 else 1.0)   # joints shift course to course
                ln = min(ln, length - pos)
                first = False
                along = pos + ln / 2
                # Jagged top: each spot survives to its own height.
                top = 1.0 + 1.3 * (0.5 + 0.5 * math.sin(along * 1.3 + leg * 2.0)) + rng.uniform(-0.5, 0.5)
                if k * course < top * (1.0 - 0.1 * along / length):
                    z = course * (k + 0.5)
                    if leg == 0:
                        parts.append(stone(rng, (ln - 0.03, thick, course - 0.03), (along, 0.0, z)))
                    else:
                        parts.append(stone(rng, (thick, ln - 0.03, course - 0.03), (0.0, -along, z)))
                pos += ln
    # A few blocks fallen from the top.
    for dx, dy, yaw in ((2.2, 1.1, 0.5), (3.6, 1.4, 1.4), (1.1, -2.6, 2.3)):
        parts.append(stone(rng, (0.9, thick * 0.9, course - 0.03), (dx, dy, course / 2), yaw=yaw, tilt=0.12))
    finish_ruin(parts, "SM_Ruin_Wall", 50)


def far_islands():
    import sb_rock
    for name, size, seed in (("SM_Island_Far_A", (420.0, 300.0, 85.0), 71), ("SM_Island_Far_B", (300.0, 260.0, 120.0), 72),
                             ("SM_Island_Far_C", (520.0, 200.0, 60.0), 73)):
        sb.reset_scene()
        mats()
        r = sb_rock.stylised_rock(name, size, seed, chunks=6, cuts=14, bottom=-40.0, target_faces=60000, material=M["MI_Rock_Coast"])
        print("SB_FAR_ISLAND", name, len(r.data.polygons))
        sb.save_blend(name)
        sb.export_fbx([r], os.path.join("Islands", name + ".fbx"))


BUILDERS = {"ruin": island_ruin, "col": island_collision, "arch": ruin_arch, "wall": ruin_wall, "far": far_islands}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        BUILDERS[key]()
    print("SB_ISLANDS_OK", wanted)
