"""The home island's places (D71): the Harbour Keeper's hut, the places to explore and the small things on the ground.

Owner (2026-10-06): "the unique touch of the island is non existent. nothing to explore ... Make a place for an NPC."
GameConcept 8.1: "Smoke at the Harbour Keeper's hut. 'Another one from the Gale ...' The old boat at the pier ... 'Make her sail
and she's yours.' A shovel."

Built like the boat's props (D38, D53): from sb_timber boards and spars, every part for its purpose, broad bevels, nothing
floating. Unreal coordinates in metres in the code (build_boat.U mirrors Y for Blender). Each mesh's origin is its place on
the ground; place_home_island.py puts them (Art/Generated/home_places.json, written here from island_shape.PLACES).

  SM_Home_KeeperHut   the Harbour Keeper's hut on the pier's beach: a low floor on stilts, a counter in the front wall
                      under a propped shutter (the Keeper stands behind it, the player in front), a thatched gable roof, a
                      stone chimney at the back, a side door ajar, shelves of stores inside, a net drying on poles with
                      floats, oars, barrels, a fish trap, a rope coil, a bench. Local +X faces the pier.
  SM_Home_Smoke       the chimney's smoke: puffs that rise, grow and fade in the material (M_SB_IslSmoke).

Run: Tools/Build-BlenderAsset.ps1 build_home_props [hut smoke ...]
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_boat as bb  # noqa: E402  (shared board, rod and rope helpers in Unreal coordinates)
import island_shape as S  # noqa: E402
import sb_blender as sb  # noqa: E402
import sb_timber as tb  # noqa: E402

U = bb.U
CATEGORY = "HomeIsland"
PLACES_JSON = os.path.join(sb.REPO_ROOT, "Art", "Generated", "home_places.json")
EXTRA = {"MI_IslThatch": (0.6, 0.5, 0.3), "MI_Rock_Coast": (0.45, 0.42, 0.38), "MI_Wood_Planks": (0.45, 0.35, 0.25),
         "MI_Stone_Ruin": (0.75, 0.73, 0.68), "MI_AncientGlow": (0.35, 0.95, 0.85), "MI_IslSmoke": (0.7, 0.7, 0.7),
         "MI_IslShell": (0.9, 0.85, 0.75), "MI_IslBark_Palm": (0.45, 0.36, 0.26), "MI_IslLeaf_Palm": (0.3, 0.55, 0.18),
         "MI_IslNut": (0.4, 0.4, 0.15), "MI_IslBark_Tree": (0.35, 0.27, 0.2), "MI_IslLeaf_Bush": (0.28, 0.52, 0.2),
         "MI_IslPool": (0.1, 0.35, 0.35), "MI_IslSeaweed": (0.2, 0.25, 0.08), "MI_IslFlower": (0.85, 0.3, 0.15),
         "MI_IslLeaf_Tree": (0.2, 0.45, 0.15), "MI_IslPebble": (0.5, 0.48, 0.45), "MI_IslAncientFaint": (0.35, 0.95, 0.85),
         "MI_IslDriftwood": (0.6, 0.58, 0.54)}


def mats():
    bb.mats()
    for n, c in EXTRA.items():
        bb.M[n] = sb.get_material(n, (*c, 1))


def rng_seed(seed):
    random.seed(seed)
    return random.Random(seed)


def board(name, a, b, width, thick, across, mat, wear=0.6, grooves=(1, 3), split=0.15):
    """A straight rough board from a to b (Unreal metres), its width along `across`."""
    a, b, across = Vector(a), Vector(b), Vector(across).normalized()
    d = (b - a)
    normal = d.normalized().cross(across).normalized()

    def frame(s):
        return (tuple(a + d * s), tuple(across), tuple(normal), 1.0)
    return bb.laid_board(name, d.length, width, thick, frame, mat, wear=wear, grooves=grooves, split=split)


def beam(name, a, b, size, mat, up=(0, 0, 1)):
    """A squared timber (size = (width, depth)) from a to b: a thick board laid on edge."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    upv = Vector(up)
    across = upv.cross(d).normalized() if upv.cross(d).length > 1e-3 else Vector((0, 1, 0))
    return board(name, a, b, size[0], size[1], across, mat, wear=0.5, grooves=(0, 1), split=0.1)


def pole(name, a, b, radius, mat, seed):
    """A round pole (spar) from a to b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    obj = tb.spar(name, d.length, radius, radius * 0.85, bb.M[mat], seed, sides=14, sweep=0.02, axis=(1, 0, 0), facets=7, checks=1)
    obj.data.transform(Vector((1, 0, 0)).rotation_difference(U(tuple(d.normalized())).normalized()).to_matrix().to_4x4())
    obj.data.transform(__import__("mathutils").Matrix.Translation(U(tuple(a))))
    return obj


def stone(name, size, center, rng, mat="MI_Rock_Coast", rot=0.0, bevel=0.28):
    """A rough rounded stone (a chunky bevelled box, its corners pushed about)."""
    obj = sb.add_box(name, size, (0, 0, 0), bb.M[mat])
    for v in obj.data.vertices:
        v.co = Vector((v.co.x * rng.uniform(0.85, 1.12), v.co.y * rng.uniform(0.85, 1.12), v.co.z * rng.uniform(0.9, 1.1)))
    sb.soften(obj, min(size) * bevel, 2)
    sb.apply_modifiers(obj)
    sb.transform_obj(obj, (rng.uniform(-0.08, 0.08), rng.uniform(-0.08, 0.08), rot + rng.uniform(-0.15, 0.15)))
    obj.data.transform(__import__("mathutils").Matrix.Translation(U(center)))
    sb.world_box_uv(obj, 1.0)
    return obj


def finish(parts, name, ucx=(), separate=()):
    """Joins the parts, bakes the surface masks (edge wear on the bevels, grime low down), adds UCX boxes, exports.
    `separate`: parts with vertex colours of their own (the thatch), exported beside it unbaked (one mesh in Unreal)."""
    for p in list(parts) + list(separate):
        if not p.data.uv_layers:
            sb.world_box_uv(p, 1.0)
    for p in separate:
        # Parts without masks of their own: a middle tone, fresh (not dried), rigid (no flutter) for the island's masters.
        if "Col" not in p.data.color_attributes:
            attr = p.data.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
            for d in attr.data:
                d.color = (0.5, 0.0, 0.5, 0.0)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=0.9, crevice=0.8, waterline=0.0, wet_height=0.35, seed=len(name))
    objs = [obj]
    if separate:
        objs.append(sb.join(list(separate), name + "_Own") if len(separate) > 1 else separate[0])
    for k, (size, center) in enumerate(ucx):
        objs.append(sb.add_ucx_box(name, k, size, U(center)))
    print("SB_PROP", name, "faces", len(obj.data.polygons), "ucx", len(ucx))
    sb.save_blend(name)
    sb.export_fbx(objs, os.path.join(CATEGORY, name + ".fbx"))
    return obj


# --- the Harbour Keeper's hut ---------------------------------------------------------------------------------------
HX, HY = 1.8, 2.3            # half the body's depth (X, to the pier) and width (Y)
FLOOR = 0.22                 # the floor's top above the ground at the hut's middle (the counter: 1.15)
COUNTER = 1.15
PLATE = 2.45                 # the walls' top
RIDGE = 3.95
EAVE_X, EAVE_Z = 2.45, 2.15  # the roof's eaves (front and back) and the gables' overhang
GABLE_Y = 2.62
# For the M6 NPC and the interaction point (local; written to home_places.json): the Keeper stands behind the counter.
KEEPER_SPOT = (1.15, 0.0, FLOOR)
KEEPER_POINT = (2.4, 0.0, 1.0)


def roof_z(x):
    """The roof's underside over local x (a gable: the ridge along Y)."""
    return RIDGE - (RIDGE - EAVE_Z) * abs(x) / EAVE_X


def thatch(rng):
    """Palm thatch in overlapping rows down both slopes, each row's lower edge a ragged fringe; a bound ridge bundle."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    col = bm.loops.layers.float_color.new("Col")
    L = math.hypot(EAVE_X, RIDGE - EAVE_Z) + 0.12            # slope length (a little past the eaves)
    rows, row_h = 9, 0.5
    for side in (-1.0, 1.0):
        down = Vector((side * EAVE_X, 0.0, -(RIDGE - EAVE_Z))).normalized()
        up_n = Vector((side * (RIDGE - EAVE_Z), 0.0, EAVE_X)).normalized()
        top = Vector((0.0, 0.0, RIDGE + 0.05))
        for r in range(rows):
            s0 = L * r / rows - 0.05
            s1 = min(s0 + row_h, L + 0.06)
            ncol = int(2 * GABLE_Y / 0.07)
            fringe = [rng.uniform(0.0, 0.14) * (1.0 if rng.random() < 0.7 else 0.3) for _ in range(ncol + 1)]
            tone = rng.uniform(0.85, 1.1)
            grid = []
            for i in range(ncol + 1):
                y = -GABLE_Y + 2 * GABLE_Y * i / ncol + rng.uniform(-0.01, 0.01) * (0 < i < ncol)
                f = fringe[i] * (1.0 + 0.6 * (r == rows - 1))
                pts = []
                for s, lift in ((s0, 0.03), (s0 + (s1 - s0) * 0.55, 0.09), (s1 + f, 0.13), (s1 + f - 0.02, 0.0)):
                    pts.append(top + down * s + up_n * lift + Vector((0.0, y, 0.0)))
                grid.append([bm.verts.new(U(tuple(p))) for p in pts])
            for i in range(ncol):
                for j in range(3):
                    vs = [grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]]
                    if side > 0:
                        vs = vs[::-1]
                    f = bm.faces.new(vs)
                    f.material_index = 0
                    for loop in f.loops:
                        k = grid[i].index(loop.vert) if loop.vert in grid[i] else grid[i + 1].index(loop.vert)
                        ii = i if loop.vert in grid[i] else i + 1
                        loop[uv].uv = (ii * 0.07, (s0 + k * 0.18))
                        t = (0.35 + 0.15 * k) * tone if k < 3 else 0.2
                        loop[col] = (t, 1.0, (i * 0.37 + r * 0.61) % 1.0, 0.0)
    bm.normal_update()
    for f in bm.faces:                                       # faces out of the roof (Y is mirrored for Blender)
        c = f.calc_center_median()
        out = Vector((c.x, 0.0, 0.0)).normalized() + Vector((0.0, 0.0, 1.2))
        if f.normal.dot(out) < 0.0:
            f.normal_flip()
    me = bpy.data.meshes.new("thatch")
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("thatch", me)
    obj.data.materials.append(bb.M["MI_IslThatch"])
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def thatch_underlay():
    """Dark lath-and-straw boards under the thatch (from below and inside no sky shows between the rows)."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    col = bm.loops.layers.float_color.new("Col")
    for side in (-1.0, 1.0):
        down = Vector((side * EAVE_X, 0.0, -(RIDGE - EAVE_Z)))
        up_n = Vector((side * (RIDGE - EAVE_Z), 0.0, EAVE_X)).normalized()
        top = Vector((0.0, 0.0, RIDGE + 0.04)) - up_n * 0.02
        corners = [top + Vector((0, -GABLE_Y, 0)), top + Vector((0, GABLE_Y, 0)), top + down * 1.03 + Vector((0, GABLE_Y, 0)),
                   top + down * 1.03 + Vector((0, -GABLE_Y, 0))]
        vs = [bm.verts.new(U(tuple(c))) for c in corners]
        f = bm.faces.new(vs)
        for loop, (u_, v_) in zip(f.loops, ((0, 0), (5, 0), (5, 3), (0, 3))):
            loop[uv].uv = (u_, v_)
            loop[col] = (0.12, 1.0, 0.4, 0.0)
    me = bpy.data.meshes.new("underlay")
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("underlay", me)
    obj.data.materials.append(bb.M["MI_IslThatch"])
    return obj


def hut():
    mats()
    rng = rng_seed(7101)
    W = "MI_Wood_DockWeathered"
    P = "MI_Wood_Planks"
    parts = []
    # Stilts (driven into the sand, the back ones mostly buried as the ground rises) and sills.
    for x in (-HX + 0.1, 0.0, HX - 0.1):
        for y in (-HY + 0.1, 0.0, HY - 0.1):
            parts.append(bb.post("stilt", 0.085, 0.95, (x, y, -0.75), P, sides=6, taper=0.92))
    for y in (-HY + 0.1, 0.0, HY - 0.1):
        parts.append(beam("sill", (-HX - 0.05, y, FLOOR - 0.12), (HX + 0.05, y, FLOOR - 0.12), (0.14, 0.14), P))
    # Floor planks across the sills, along X.
    n = int(2 * HY / 0.24)
    for i in range(n):
        y = -HY + (i + 0.5) * 2 * HY / n
        parts.append(board("floor", (-HX - 0.04, y, FLOOR - 0.02), (HX + 0.06 + rng.uniform(-0.03, 0.03), y, FLOOR - 0.02), 0.22, 0.04,
                           (0, 1, 0), W))
    # Corner and door posts, the wall plates.
    for x, y in ((-HX, -HY), (-HX, HY), (HX, -HY), (HX, HY), (HX, -1.62), (HX, 1.62), (-0.62, HY), (0.34, HY)):
        parts.append(bb.post("post", 0.075, PLATE - FLOOR + 0.02, (x, y, FLOOR), P, sides=4, taper=0.97))
    for x in (-HX, HX):
        parts.append(beam("plate", (x, -HY - 0.12, PLATE), (x, HY + 0.12, PLATE), (0.13, 0.13), P))
    for y in (-HY, HY):
        parts.append(beam("plate", (-HX - 0.1, y, PLATE - 0.13), (HX + 0.1, y, PLATE - 0.13), (0.12, 0.12), P))

    def wall_x(x, y0, y1, z0, z1, mat=W):
        """Vertical planks of a wall at x (front or back) from y0 to y1, z0 to z1."""
        k = max(1, int(round((y1 - y0) / 0.21)))
        for i in range(k):
            y = y0 + (i + 0.5) * (y1 - y0) / k
            top = z1 + rng.uniform(-0.02, 0.0)
            parts.append(board("wall", (x, y, z0 - 0.05), (x, y, top), (y1 - y0) / k + 0.012, 0.03, (0, 1, 0), mat, wear=0.7))

    def wall_y(y, x0, x1, z0, ztop_fn, mat=W):
        k = max(1, int(round((x1 - x0) / 0.21)))
        for i in range(k):
            x = x0 + (i + 0.5) * (x1 - x0) / k
            parts.append(board("wall", (x, y, z0 - 0.05), (x, y, ztop_fn(x) + rng.uniform(-0.02, 0.0)), (x1 - x0) / k + 0.012, 0.03,
                               (1, 0, 0), mat, wear=0.7))

    wall_x(-HX - 0.04, -HY, HY, FLOOR, PLATE)                                  # back
    wall_x(HX + 0.04, -HY, -1.62, FLOOR, PLATE)                                # front, beside the window
    wall_x(HX + 0.04, 1.62, HY, FLOOR, PLATE)
    wall_x(HX + 0.04, -1.62, 1.62, FLOOR, COUNTER - 0.06)                      # front, under the counter
    parts.append(beam("header", (HX + 0.02, -1.7, 2.3), (HX + 0.02, 1.7, 2.3), (0.13, 0.12), P))
    gable = lambda x: min(roof_z(x) - 0.04, RIDGE - 0.1)                        # noqa: E731
    wall_y(-HY - 0.04, -HX, HX, FLOOR, gable)                                  # left gable (full)
    wall_y(HY + 0.04, -HX, -0.62, FLOOR, gable)                                # right gable, beside the door
    wall_y(HY + 0.04, 0.34, HX, FLOOR, gable)
    wall_y(HY + 0.04, -0.62, 0.34, 2.12, gable)                                # over the door
    # The counter: a thick slab on two brackets, a lip at its front edge; the propped shutter above as an awning.
    parts.append(board("counter", (HX - 0.24, -1.72, COUNTER - 0.035), (HX - 0.24, 1.72, COUNTER - 0.035), 0.5, 0.07, (1, 0, 0), P,
                       wear=0.9, grooves=(2, 4)))
    for y in (-1.2, 1.2):
        parts.append(beam("bracket", (HX + 0.02, y, COUNTER - 0.5), (HX + 0.36, y, COUNTER - 0.09), (0.07, 0.07), P, up=(0, 1, 0)))
    hinge = Vector((HX + 0.08, 0.0, 2.36))
    ang = math.radians(78.0)                                                    # swung out and up from hanging
    out = Vector((math.sin(ang), 0.0, -math.cos(ang)))
    for i in range(8):
        y = -1.7 + (i + 0.5) * 3.4 / 8
        a = hinge + Vector((0.0, y, 0.0))
        parts.append(board("shutter", tuple(a), tuple(a + out * 0.9), 3.4 / 8 + 0.01, 0.03, (0, 1, 0), W, wear=0.8))
    for y in (-1.35, 1.35):
        lip = hinge + Vector((0.0, y, 0.0)) + out * 0.86
        parts.append(beam("ledge", tuple(lip + Vector((0, 0, -0.03))), tuple(hinge + Vector((0.0, y, -0.03)) + out * 0.1), (0.06, 0.04), P,
                          up=(0, 1, 0)))
        parts.append(pole("prop", (HX + 0.32, y * 0.9, COUNTER), tuple(lip + Vector((0.0, 0.0, -0.06))), 0.025, P, 7300 + int(y * 10)))
    # The door: ajar, swung outward about 70 degrees on its hinge post.
    hinge_d = Vector((-0.6, HY + 0.06, FLOOR))
    dang = math.radians(70.0)
    dd = Vector((math.cos(dang), math.sin(dang), 0.0))
    for i in range(4):
        u0 = (i + 0.5) * 0.92 / 4
        p0 = hinge_d + dd * u0
        parts.append(board("door", tuple(p0), tuple(p0 + Vector((0, 0, 1.86))), 0.24, 0.03, tuple(dd), P, wear=0.7))
    for z in (0.35, 1.6):
        parts.append(board("ledge", tuple(hinge_d + Vector((0, 0, z)) + dd * 0.04 - Vector((-dd.y, dd.x, 0)) * 0.03),
                           tuple(hinge_d + Vector((0, 0, z)) + dd * 0.88 - Vector((-dd.y, dd.x, 0)) * 0.03), 0.12, 0.03,
                           (0, 0, 1), P, wear=0.5))
    # Door step: a flat stone and a log.
    parts.append(stone("step", (0.6, 0.45, 0.16), (-0.15, HY + 0.45, 0.04), rng))
    # Rafters, purlins, the thatch and its ridge bundle.
    for y in (-GABLE_Y + 0.08, -1.2, 0.0, 1.2, GABLE_Y - 0.08):
        for side in (-1.0, 1.0):
            parts.append(pole("rafter", (side * (EAVE_X + 0.05), y, EAVE_Z - 0.05), (0.0, y, RIDGE - 0.08), 0.045, P, 7310 + int(y * 7 + side)))
    for side in (-1.0, 1.0):
        for t in (0.25, 0.6):
            x = side * EAVE_X * (1.0 - t)
            parts.append(pole("purlin", (x, -GABLE_Y, roof_z(x) - 0.06), (x, GABLE_Y, roof_z(x) - 0.06), 0.04, P, 7330 + int(t * 10 + side)))
    own = [thatch(rng), thatch_underlay()]
    own.append(pole("ridge", (0.0, -GABLE_Y - 0.08, RIDGE + 0.1), (0.0, GABLE_Y + 0.08, RIDGE + 0.1), 0.14, "MI_IslThatch", 7340))
    for y in (-2.1, -1.0, 0.1, 1.2, 2.2):
        parts.append(bb.ring("binding", 0.135, 0.016, (0.0, y, RIDGE + 0.12), (0, 1, 0), "MI_Rope_Hemp", 16))
    # The chimney: rough stones laid in a square stack against the back wall, past the eaves, a cap stone on top.
    cx, cy = -HX - 0.48, -1.15
    z = -0.3
    layer = 0
    while z < 4.35:
        h = rng.uniform(0.14, 0.24)
        half = 0.44 - 0.12 * min(max(z, 0.0) / 4.3, 1.0)
        # Each face of the square stack is a course of two or three stones of their own length (a running bond).
        for side in range(4):
            a = side * math.pi / 2
            ux, uy = -math.sin(a), math.cos(a)
            nx, ny = math.cos(a), math.sin(a)
            cuts = sorted(rng.uniform(-0.6, 0.6) * half for _ in range(rng.randint(1, 2)))
            edges = [-half] + cuts + [half]
            for e0, e1 in zip(edges[:-1], edges[1:]):
                length = (e1 - e0) - 0.02
                mid = (e0 + e1) / 2 + (0.08 if (layer + side) % 2 else -0.08) * 0.0
                depth = rng.uniform(0.2, 0.3)
                c = (cx + nx * (half - depth / 2) + ux * mid, cy + ny * (half - depth / 2) + uy * mid, z + h / 2)
                parts.append(stone("chim", (length, depth, h * rng.uniform(0.92, 1.0)), c, rng, rot=a + math.pi / 2, bevel=0.16))
        z += h
        layer += 1
    parts.append(stone("cap", (0.62, 0.62, 0.08), (cx, cy, z + 0.04), rng))
    # Inside: shelves of stores on the back wall (jars, boxes, coiled line), a ledger and the call bell on the counter.
    for zz in (1.25, 1.75):
        parts.append(board("shelf", (-HX + 0.05, -1.6, zz), (-HX + 0.05, 1.4, zz), 0.3, 0.03, (1, 0, 0), P))
        for k in range(6):
            yy = -1.45 + k * 0.5 + rng.uniform(-0.08, 0.08)
            if rng.random() < 0.55:
                parts.append(bb.disc("jar", rng.uniform(0.06, 0.09), rng.uniform(0.14, 0.24), (-HX + 0.18, yy, zz + 0.1), (0, 0, 1),
                                     "MI_Glass_Dark", 12, 0.01))
            else:
                parts.append(bb.block("box", (0.22, rng.uniform(0.18, 0.3), rng.uniform(0.12, 0.2)), (-HX + 0.18, yy, zz + 0.09), P, 0.015))
    parts.append(bb.block("ledger", (0.26, 0.34, 0.05), (HX - 0.32, 0.55, COUNTER + 0.025), "MI_Canvas", 0.01))
    parts.append(bb.disc("bell_base", 0.06, 0.02, (HX - 0.25, -0.75, COUNTER + 0.01), (0, 0, 1), "MI_Wood_Varnish", 12))
    bell = sb.add_cylinder("bell", 0.05, 0.06, (0, 0, 0), "Z", 14, bb.M["MI_Metal_Brass"], radius_top=0.025)
    bell.data.transform(__import__("mathutils").Matrix.Translation(U((HX - 0.25, -0.75, COUNTER + 0.05))))
    parts.append(bell)
    # The lantern's bracket at the front right post, an iron arm with a hook.
    parts.append(beam("arm", (HX + 0.02, 1.95, 2.15), (HX + 0.6, 1.95, 2.15), (0.06, 0.06), P, up=(0, 0, 1)))
    parts.append(beam("arm_brace", (HX + 0.02, 1.95, 1.8), (HX + 0.45, 1.95, 2.12), (0.05, 0.05), P, up=(0, 1, 0)))
    parts.append(bb.rod("hook", (HX + 0.55, 1.95, 2.12), (HX + 0.55, 1.95, 2.0), 0.008, "MI_Metal_Iron", 6))
    # A net drying on two poles beside the hut (left), floats along its head rope; a second net heaped below.
    p0, p1 = Vector((1.4, -HY - 1.25, 0.0)), Vector((-1.3, -HY - 1.45, 0.0))
    for k, p in enumerate((p0, p1)):
        parts.append(pole("netpole", tuple(p - Vector((0, 0, 0.4))), tuple(p + Vector((0, 0, 2.15))), 0.05, P, 7350 + k))
    head = bb.sagging(tuple(p0 + Vector((0, 0, 2.05))), tuple(p1 + Vector((0, 0, 2.05))), 0.12, 10)
    parts += bb.rope("headrope", head, 0.012)
    for i in range(1, 10, 2):
        c = Vector(head[i])
        parts.append(bb.disc("cork", 0.045, 0.07, tuple(c), (p1 - p0).normalized(), "MI_Wood_Planks", 10))
    # The net: a mesh of thin cords hanging from the head rope, gathered and sagging (diamond meshes).
    rows_n, cols_n = 9, 18
    grid = []
    for r in range(rows_n + 1):
        row = []
        for c in range(cols_n + 1):
            t = c / cols_n
            top = Vector(head[0]).lerp(Vector(head[-1]), t) - Vector((0, 0, 0.12 * math.sin(math.pi * t)))
            drop = r * 0.17 * (1.0 + 0.25 * math.sin(math.pi * t))
            sway = Vector((0.06 * math.sin(r * 0.8 + t * 5.0), 0.05 * r * math.sin(math.pi * t), 0.0))
            row.append(top - Vector((0, 0, drop)) + sway)
        grid.append(row)
    for r in range(rows_n):
        for c in range(cols_n):
            if (r + c) % 2 == 0:
                parts.append(bb.rod("net", tuple(grid[r][c]), tuple(grid[r + 1][c + 1]), 0.005, "MI_Rope_Hemp", 4))
            else:
                parts.append(bb.rod("net", tuple(grid[r][c + 1]), tuple(grid[r + 1][c]), 0.005, "MI_Rope_Hemp", 4))
    for c in range(0, cols_n + 1, 3):
        parts.append(bb.rod("lead", tuple(grid[-1][c]), tuple(grid[-1][min(c + 3, cols_n)]), 0.012, "MI_Rope_Hemp", 6))
    hc = Vector((0.15, -HY - 0.95, 0.0))
    dome = [[hc + Vector((0.5 * (i / 6 - 0.5) * 2 * 0.9, 0.4 * (j / 6 - 0.5) * 2, 0.0)) for j in range(7)] for i in range(7)]
    for i in range(7):
        for j in range(7):
            q = dome[i][j] - hc
            d2 = (q.x / 0.5) ** 2 + (q.y / 0.4) ** 2
            dome[i][j] = dome[i][j] + Vector((0.0, 0.0, 0.24 * max(0.0, 1.0 - d2) + 0.03 * math.sin(i * 2.1 + j * 1.3)))
    for i in range(6):
        for j in range(6):
            parts.append(bb.rod("netheap", tuple(dome[i][j]), tuple(dome[i + 1][j + 1]), 0.007, "MI_Rope_Hemp", 4))
            parts.append(bb.rod("netheap", tuple(dome[i][j + 1]), tuple(dome[i + 1][j]), 0.007, "MI_Rope_Hemp", 4))
    for k in range(3):
        parts.append(bb.disc("cork", 0.045, 0.07, tuple(hc + Vector((0.3 - 0.25 * k, 0.25 - 0.2 * k, 0.05 + 0.06 * (k == 1)))), (1, 0.3 * k, 0.4),
                             "MI_Wood_Planks", 10))
    # Oars and a boat hook leaning on the front wall left of the counter; glass floats hanging under the eaves.
    for k, (y, top_z) in enumerate(((-1.95, 2.2), (-2.12, 2.05))):
        foot = (HX + 0.55, y, -0.02)
        tip = (HX + 0.1, y + 0.06, top_z)
        parts.append(pole("oar", foot, tip, 0.028, P, 7360 + k))
        blade = Vector(foot) + (Vector(tip) - Vector(foot)).normalized() * 0.32
        parts.append(board("blade", foot, tuple(blade), 0.15, 0.025, (0, 1, 0), P, wear=0.6))
    for k, y in enumerate((-1.95, -2.2, -2.42)):
        c = (HX + 0.42, y, 1.85 - 0.12 * k)
        parts.append(bb.rod("float_line", (HX + 0.42, y, roof_z(HX + 0.42) - 0.05), (c[0], c[1], c[2] + 0.13), 0.006, "MI_Rope_Hemp", 6))
        ball = sb.add_box("float", (0.24, 0.24, 0.24), (0, 0, 0), bb.M["MI_Glass_Dark"])
        sb.soften(ball, 0.11, 4)
        sb.apply_modifiers(ball)
        ball.data.transform(__import__("mathutils").Matrix.Translation(U(c)))
        parts.append(ball)
        parts.append(bb.ring("float_net", 0.125, 0.008, c, (1, 0, 0), "MI_Rope_Hemp", 16))
    # Barrels by the door and the counter's end; a fish trap; a rope coil on the porch; a bench by the door.
    def barrel(c, h=0.78, r=0.3, seed=0):
        rr = random.Random(seed)
        bm = bmesh.new()
        segs, rings = 18, 8
        vs = []
        for j in range(rings + 1):
            t = j / rings
            rad = r * (0.86 + 0.14 * math.sin(math.pi * t))
            row = []
            for i in range(segs):
                a = 2 * math.pi * i / segs
                k = 1.0 - 0.025 * (i % 2)                      # staves: every other edge a little in (grooves)
                row.append(bm.verts.new(U((c[0] + math.cos(a) * rad * k, c[1] + math.sin(a) * rad * k, c[2] + h * t))))
            vs.append(row)
        for j in range(rings):
            for i in range(segs):
                bm.faces.new((vs[j][i], vs[j][(i + 1) % segs], vs[j + 1][(i + 1) % segs], vs[j + 1][i]))
        bm.faces.new(vs[-1][::-1])
        me = bpy.data.meshes.new("barrel")
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.normal_update()
        bm.to_mesh(me)
        bm.free()
        o = sb.new_object("barrel", me)
        o.data.materials.append(bb.M[P])
        sb.soften(o, 0.01, 2)
        out = [o]
        for t in (0.12, 0.5, 0.88):
            rad = r * (0.86 + 0.14 * math.sin(math.pi * t)) + 0.006
            out.append(bb.ring("hoop", rad, 0.012, (c[0], c[1], c[2] + h * t), (0, 0, 1), "MI_Metal_Iron", 24))
        return out
    parts += barrel((0.75, HY + 0.65, -0.05), seed=1)
    parts += barrel((HX + 0.5, 2.0, -0.05), h=0.7, r=0.27, seed=2)
    trap_c = Vector((-1.1, HY + 0.7, 0.28))
    for k in range(14):
        a = 2 * math.pi * k / 14
        p = trap_c + Vector((math.cos(a) * 0.26, 0.0, math.sin(a) * 0.26))
        parts.append(bb.rod("trap", tuple(p + Vector((0, -0.42, 0))), tuple(p + Vector((0, 0.42, 0))), 0.011, "MI_Wood_Planks", 5))
    for t in (-0.38, 0.0, 0.38):
        parts.append(bb.ring("trap_hoop", 0.27, 0.014, tuple(trap_c + Vector((0, t, 0))), (0, 1, 0), "MI_Wood_Planks", 18))
    for k in range(4):
        parts.append(bb.ring("coil", 0.2 - 0.035 * k, 0.022, (HX + 0.95, -0.9, 0.03 + 0.012 * (k % 2)), (0, 0, 1), "MI_Rope_Hemp", 24))
    parts.append(board("bench", (-1.6, HY + 1.0, 0.42), (-0.2, HY + 1.05, 0.42), 0.32, 0.05, (0, 1, 0), P, wear=0.8))
    for x in (-1.45, -0.35):
        parts.append(bb.post("bench_leg", 0.1, 0.62, (x, HY + 1.02, -0.25), P, sides=7, taper=0.9))
    ucx = [((2 * HX + 0.1, 2 * HY + 0.1, FLOOR + 0.6), (0.0, 0.0, FLOOR / 2 - 0.3)),       # the floor on its stilts
           ((0.12, 2 * HY, PLATE - FLOOR), (-HX, 0.0, (FLOOR + PLATE) / 2)),                # back wall
           ((0.12, 0.7, PLATE - FLOOR), (HX + 0.04, -1.95, (FLOOR + PLATE) / 2)),          # front, beside the window
           ((0.12, 0.7, PLATE - FLOOR), (HX + 0.04, 1.95, (FLOOR + PLATE) / 2)),
           ((0.6, 3.5, COUNTER), (HX, 0.0, COUNTER / 2)),                                   # the counter and the wall under it
           ((2 * HX, 0.12, PLATE - FLOOR), (0.0, -HY, (FLOOR + PLATE) / 2)),                # left gable wall
           ((1.2, 0.12, PLATE - FLOOR), (-1.2, HY, (FLOOR + PLATE) / 2)),                   # right wall, beside the door
           ((1.45, 0.12, PLATE - FLOOR), (1.07, HY, (FLOOR + PLATE) / 2)),
           ((0.85, 0.85, 4.6), (cx, cy, 2.0)),                                              # the chimney
           ((0.7, 0.7, 0.9), (0.75, HY + 0.65, 0.4)), ((0.6, 0.6, 0.8), (HX + 0.5, 2.0, 0.35)),   # barrels
           ((1.0, 0.4, 0.5), (-1.1, HY + 0.7, 0.28)),                                        # the fish trap
           ((1.5, 0.4, 0.45), (-0.9, HY + 1.02, 0.22))]                                      # the bench
    return finish(parts, "SM_Home_KeeperHut", ucx, separate=own)


# --- the chimney's smoke ------------------------------------------------------------------------------------------
def smoke():
    """Puffs all centred on the origin (the chimney's mouth); vertex R = each puff's phase, G = its size. M_SB_IslSmoke
    lifts, grows, drifts and fades each puff in a loop, so the mesh itself never moves."""
    mats()
    rng = rng_seed(7102)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    col = bm.loops.layers.float_color.new("Col")
    n = 9
    for k in range(n):
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=2, radius=1.0)
        r = rng.uniform(0.22, 0.32)
        seeds = [rng.uniform(0, 6.28) for _ in range(3)]
        vmap = {}
        for v in tmp.verts:
            d = v.co.normalized()
            bump = 1.0 + 0.18 * math.sin(3 * d.x + seeds[0]) * math.sin(2 * d.y + seeds[1]) + 0.1 * math.sin(4 * d.z + seeds[2])
            vmap[v] = bm.verts.new(U(tuple(d * r * bump * Vector((1.0, 1.0, 0.85)))))
        phase, size = (k + rng.uniform(-0.2, 0.2)) / n % 1.0, rng.random()
        for f in tmp.faces:
            nf = bm.faces.new([vmap[v] for v in f.verts])
            nf.smooth = True
            for loop, v in zip(nf.loops, f.verts):
                d = v.co.normalized()
                loop[uv].uv = (0.5 + math.atan2(d.y, d.x) / (2 * math.pi), 0.5 + 0.5 * d.z)
                loop[col] = (phase, size, rng.random(), 1.0)
        tmp.free()
    me = bpy.data.meshes.new("SM_Home_Smoke")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("SM_Home_Smoke", me)
    obj.data.materials.append(bb.M["MI_IslSmoke"])
    me.color_attributes.active_color = me.color_attributes["Col"]
    print("SB_PROP SM_Home_Smoke faces", len(me.polygons))
    sb.save_blend("SM_Home_Smoke")
    sb.export_fbx([obj], os.path.join(CATEGORY, "SM_Home_Smoke.fbx"))
    return obj


# --- small things on the ground (instanced by build_home_island.details) --------------------------------------------
def veg():
    import build_vegetation as V
    V.MATS.update({k: v for k, v in EXTRA.items() if k not in V.MATS})
    V.MATS.setdefault("MI_IslPebble", (0.5, 0.48, 0.45))
    return V


def shell(name, seed, kind, pink):
    """A cockle (ribbed fan, domed, lying concave side down) or a whelk (a tapering spiral). MI_IslShell: G = pink."""
    V = veg()
    B = V.Builder(name, ["MI_IslShell"], seed)
    rng = B.rng
    if kind == "cockle":
        r = rng.uniform(0.05, 0.07)
        rows, cols = 6, 16
        grid = []
        for i in range(rows + 1):
            q = i / rows
            row = []
            for j in range(cols + 1):
                th = (j / cols - 0.5) * 2.5
                rib = 1.0 + 0.04 * math.cos(th * 9.0)
                rr = r * q * rib
                h = r * 0.45 * math.sqrt(max(0.0, 1.0 - q * q)) * (1.0 - 0.3 * abs(th) / 1.25) + 0.004
                row.append(B.bm.verts.new(Vector((math.sin(th) * rr, -math.cos(th) * rr + r * 0.45, h))))
            grid.append(row)
        for i in range(rows):
            for j in range(cols):
                c = lambda ii, jj: (0.45 + 0.4 * (0.5 + 0.5 * math.cos(((jj / cols - 0.5) * 2.5) * 9.0)) * (ii / rows), pink, 0.5, 0.0)  # noqa: E731
                B.face([grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]],
                       [(j / cols, i / rows), (j / cols, (i + 1) / rows), ((j + 1) / cols, (i + 1) / rows), ((j + 1) / cols, i / rows)],
                       [c(i, j), c(i + 1, j), c(i + 1, j + 1), c(i, j + 1)], "MI_IslShell")
    else:
        L = rng.uniform(0.07, 0.1)
        pts = [Vector((L * t, 0.0, 0.0)) for t in [k / 24 for k in range(25)]]
        V.tube(B, pts, lambda i, s, a: (0.022 * (1.0 - s) + 0.002) * (1.0 + 0.18 * ((s * 6.0 + a / 6.28) % 1.0)) + 0.0, 10,
               "MI_IslShell", lambda i, s, a: (0.4 + 0.5 * ((s * 6.0 + a / 6.28) % 1.0), pink, 0.5, 0.0), u_tiles=1)
        for v in B.bm.verts:
            v.co.z += 0.016
    return B.finish(CATEGORY)


def pebbles(name, seed, count, size):
    V = veg()
    B = V.Builder(name, ["MI_IslPebble"], seed)
    rng = B.rng
    for k in range(count):
        c = Vector((rng.uniform(-1, 1) * size * 2.2, rng.uniform(-1, 1) * size * 2.2, 0.0))
        r = size * rng.uniform(0.5, 1.2)
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=2, radius=1.0)
        sq = (rng.uniform(0.8, 1.2), rng.uniform(0.6, 1.0), rng.uniform(0.45, 0.7))
        rnd = rng.random()
        vmap = {}
        for v in tmp.verts:
            d = v.co.normalized()
            p = Vector((d.x * sq[0], d.y * sq[1], d.z * sq[2])) * r + c + Vector((0, 0, r * sq[2] * 0.55))
            vmap[v] = (B.bm.verts.new(p), d)
        for f in tmp.faces:
            B.face([vmap[v][0] for v in f.verts], [V.sphere_uv(vmap[v][1]) for v in f.verts],
                   [(0.35 + 0.5 * max(vmap[v][1].z, 0.0), 0.0, rnd, 0.0) for v in f.verts], "MI_IslPebble")
        tmp.free()
    return B.finish(CATEGORY)


def husks(name, seed):
    """Two halves of an opened coconut husk (fibrous, the inside paler) and an old whole nut, as they lie under a palm."""
    V = veg()
    B = V.Builder(name, ["MI_IslNut"], seed)
    rng = B.rng
    V.nut(B, Vector((0.12, 0.05, 0.1)), 0.11, rng, 0.85, "MI_IslNut")
    for k, (cx, cy, yaw) in enumerate(((-0.15, -0.05, 0.4), (0.0, -0.25, 2.3))):
        rows, cols = 6, 12
        grid = []
        for i in range(rows + 1):
            q = i / rows
            row = []
            for j in range(cols + 1):
                a = 2 * math.pi * j / cols
                rr = 0.13 * math.sin(math.pi * 0.5 * (0.15 + 0.85 * q)) * (1.0 + 0.05 * math.sin(a * 5 + k))
                p = Vector((math.cos(a) * rr * 1.15, math.sin(a) * rr, 0.13 * (1.0 - q) * 0.9))
                p = Vector((p.x * math.cos(yaw) - p.y * math.sin(yaw), p.x * math.sin(yaw) + p.y * math.cos(yaw), p.z))
                row.append(B.bm.verts.new(p + Vector((cx, cy, 0.0))))
            grid.append(row)
        for i in range(rows):
            for j in range(cols):
                c0 = (0.6 + 0.35 * i / rows, 0.8, 0.3 + 0.2 * k, 0.0)
                c1 = (0.6 + 0.35 * (i + 1) / rows, 0.8, 0.3 + 0.2 * k, 0.0)
                B.face([grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]],
                       [(j / cols, i / rows), ((j + 1) / cols, i / rows), ((j + 1) / cols, (i + 1) / rows), (j / cols, (i + 1) / rows)],
                       [c0, c0, c1, c1], "MI_IslNut")
    return B.finish(CATEGORY)


def frond_bit(name, seed):
    """A dead palm frond lying flat where it fell: the rachis bent to the ground, dry leaflets spread and broken."""
    V = veg()
    B = V.Builder(name, ["MI_IslLeaf_Palm"], seed)
    rng = B.rng
    V.frond(B, Vector((0.0, 0.0, 0.06)), 0.0, math.radians(2.0), rng.uniform(1.6, 2.2), rng, droop=0.02, dry=0.3, torn=0.7,
            slot="MI_IslLeaf_Palm", hang=0.0, pairs=26)
    for v in B.bm.verts:                                     # flatten on to the ground, the leaflets fanned out
        v.co.z = 0.02 + max(v.co.z + 0.6, 0.0) * 0.04
    for f in B.bm.faces:
        for loop in f.loops:
            c = loop[B.col]
            loop[B.col] = (c[0], 1.0, c[2], 0.0)
    return B.finish(CATEGORY)


def twigs(name, seed):
    V = veg()
    B = V.Builder(name, ["MI_IslBark_Tree", "MI_IslLeaf_Tree"], seed)
    rng = B.rng
    for k in range(rng.randint(2, 4)):
        a = rng.uniform(0, 6.28)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        L = rng.uniform(0.35, 0.8)
        start = Vector((rng.uniform(-0.2, 0.2), rng.uniform(-0.2, 0.2), 0.012))
        pts = V.bend_path(start, d, L, rng, step=0.05, wiggle=0.15)
        for p in pts:
            p.z = 0.012
        rnd = rng.random()
        V.tube(B, pts, V.taper(0.012, 0.005), 5, "MI_IslBark_Tree", V.bark_colour(rnd), u_tiles=1)
        if rng.random() < 0.6:
            tip = pts[-1]
            for j in range(rng.randint(1, 3)):
                ld = (d + Vector((rng.uniform(-0.8, 0.8), rng.uniform(-0.8, 0.8), 0.0))).normalized()
                V.leaf(B, tip + Vector((0, 0, 0.01)), Vector((0, 0, 1)), ld, 0.14, 0.07, "MI_IslLeaf_Tree", tone=(0.3, 0.7),
                       dry=rng.choice((0.6, 1.0)), sway=(0.0, 0.0), fold=0.12, rows=3, curl=0.2)
    return B.finish(CATEGORY)


def seaweed(name, seed):
    """A clump of seaweed thrown up on the sand: flat ribbons lying in loose curls, some dried brown."""
    V = veg()
    B = V.Builder(name, ["MI_IslSeaweed"], seed)
    rng = B.rng
    for k in range(rng.randint(7, 12)):
        a = rng.uniform(0, 6.28)
        L = rng.uniform(0.25, 0.6)
        pts = [Vector((rng.uniform(-0.1, 0.1), rng.uniform(-0.1, 0.1), 0.01))]
        for i in range(10):
            a += rng.uniform(-0.5, 0.5)
            pts.append(pts[-1] + Vector((math.cos(a), math.sin(a), 0.0)) * (L / 10))
        dry = 1.0 if rng.random() < 0.3 else 0.0
        w = rng.uniform(0.012, 0.03)
        centre, acr, ws, cols = [], [], [], []
        for i, p in enumerate(pts):
            t = (pts[min(i + 1, 10)] - pts[max(i - 1, 0)]).normalized()
            centre.append(Vector((p.x, p.y, 0.008 + 0.006 * k / 12)))
            acr.append(Vector((-t.y, t.x, 0.0)))
            ws.append(w * (1.0 - 0.6 * (i / 10)))
            cols.append((0.3 + 0.5 * (i / 10), dry, rng.random(), 0.0))
        V.strip(B, centre, acr, ws, "MI_IslSeaweed", cols)
    return B.finish(CATEGORY)


def burrow():
    """A ghost crab's burrow: a round hole going down into the sand and the little balls of sand thrown out round it.
    In the ground's own material (world-projected sand; the vertex colours say 'sand')."""
    rng = rng_seed(7110)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    data = bm.loops.layers.uv.new("Data")
    col = bm.loops.layers.float_color.new("Col")

    def add_faces(rings):
        for i in range(len(rings) - 1):
            n = len(rings[i])
            for j in range(n):
                vs = [rings[i][j], rings[i][(j + 1) % n], rings[i + 1][(j + 1) % n], rings[i + 1][j]]
                f = bm.faces.new([bm.verts.new(U(tuple(p))) if not isinstance(p, bmesh.types.BMVert) else p for p in vs])
                for loop in f.loops:
                    loop[uv].uv = (loop.vert.co.x, loop.vert.co.y)
                    loop[data].uv = (0.35, 0.3)
                    loop[col] = (0.0, 0.0, 0.0, 0.4)
    # The rim and the hole (a funnel, then a shaft).
    profile = [(0.09, 0.0), (0.06, 0.012), (0.04, 0.0), (0.035, -0.04), (0.03, -0.12), (0.005, -0.16)]
    rings = []
    for r, z in profile:
        rings.append([bm.verts.new(U((math.cos(2 * math.pi * j / 14) * r, math.sin(2 * math.pi * j / 14) * r, z))) for j in range(14)])
    add_faces(rings)
    # Sand balls round it.
    for k in range(26):
        a = rng.uniform(0, 6.28)
        d = rng.uniform(0.08, 0.3)
        c = (math.cos(a) * d, math.sin(a) * d, 0.0)
        r = rng.uniform(0.006, 0.012)
        ball = []
        for i in range(4):
            phi = math.pi * (i / 3) - math.pi / 2 if i else -math.pi / 2
            ring = []
            for j in range(6):
                th = 2 * math.pi * j / 6
                ring.append(bm.verts.new(U((c[0] + math.cos(phi) * math.cos(th) * r, c[1] + math.cos(phi) * math.sin(th) * r,
                                            r * 0.8 + math.sin(phi) * r))))
            ball.append(ring)
        add_faces(ball)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("SM_Home_Burrow")
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object("SM_Home_Burrow", me)
    obj.data.materials.append(sb.get_material("MI_HomeGround", (0.76, 0.68, 0.52, 1)))
    me.color_attributes.active_color = me.color_attributes["Col"]
    print("SB_PROP SM_Home_Burrow faces", len(me.polygons))
    sb.save_blend("SM_Home_Burrow")
    sb.export_fbx([obj], os.path.join(CATEGORY, "SM_Home_Burrow.fbx"))


def driftstick(name, seed):
    mats()
    a = tb.spar("stick", random.Random(seed).uniform(0.8, 1.4), 0.04, 0.02, bb.M["MI_IslDriftwood"], seed, sides=12, sweep=0.1,
                axis=(1, 0, 0), facets=6, checks=1)
    a.data.transform(__import__("mathutils").Matrix.Translation(Vector((-0.5, 0.0, 0.025))))
    return finish([a], name)


# --- the places to explore -----------------------------------------------------------------------------------------
def barrel_parts(c, h=0.78, r=0.3, mat="MI_Wood_Planks", sunk=0.0, broken=False):
    """A staved barrel with three iron hoops (c = the middle of its foot, Unreal metres)."""
    bm = bmesh.new()
    segs, rings = 18, 8
    vs = []
    for j in range(rings + 1):
        t = j / rings
        rad = r * (0.86 + 0.14 * math.sin(math.pi * t))
        row = []
        for i in range(segs):
            a = 2 * math.pi * i / segs
            k = 1.0 - 0.025 * (i % 2)
            top = h * t
            if broken and t == 1.0:
                top -= 0.12 * (0.5 + 0.5 * math.sin(a * 3.0))
            row.append(bm.verts.new(U((c[0] + math.cos(a) * rad * k, c[1] + math.sin(a) * rad * k, c[2] + top))))
        vs.append(row)
    for j in range(rings):
        for i in range(segs):
            bm.faces.new((vs[j][i], vs[j][(i + 1) % segs], vs[j + 1][(i + 1) % segs], vs[j + 1][i]))
    if not broken:
        bm.faces.new(vs[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("barrel")
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    o = sb.new_object("barrel", me)
    o.data.materials.append(bb.M[mat])
    out = [o]
    for t in (0.12, 0.5, 0.88):
        rad = r * (0.86 + 0.14 * math.sin(math.pi * t)) + 0.006
        out.append(bb.ring("hoop", rad, 0.012, (c[0], c[1], c[2] + h * t), (0, 0, 1), "MI_Metal_Iron", 24))
    return out


def wreck_hull():
    """The bones of a small boat half swallowed by the sand: its keel buried along local X, curved ribs arching out of the sand
    (some whole, some snapped short, one side of the boat gone), three or four planks still nailed across the low side, one
    with old cream paint. Origin in the sand at its middle."""
    mats()
    rng = rng_seed(7120)
    parts = []
    L = 4.2

    def rib_pt(x, side, h):
        """A point on a rib at x: h 0 at the keel .. 1 at the sheer; the boat lies a little on its side (+Y up)."""
        beam_ = 1.05 * (1.0 - 0.45 * (2 * x / L) ** 2)
        y = side * beam_ * math.sin(math.pi * 0.5 * h)
        z = -0.25 + 1.15 * (1.0 - math.cos(math.pi * 0.5 * h)) ** 0.8 * (1.0 if side > 0 else 0.55)
        return Vector((x, y, z))
    for k, x in enumerate((-1.7, -1.0, -0.3, 0.4, 1.1, 1.75)):
        for side in (1.0, -1.0):
            reach = rng.uniform(0.55, 1.0) if side > 0 else rng.uniform(0.25, 0.6)
            if side < 0 and rng.random() < 0.4:
                continue                                          # this side's rib long gone
            hs = [reach * j / 6 for j in range(7)]
            pts = [rib_pt(x, side, h) for h in hs]
            for a, b in zip(pts[:-1], pts[1:]):
                parts.append(beam("rib", tuple(a), tuple(b), (0.09, 0.08), "MI_Wood_DockWeathered", up=(1, 0, 0)))
    parts.append(beam("keel", (-L / 2 - 0.2, 0.0, -0.22), (L / 2, 0.0, -0.2), (0.14, 0.2), "MI_Wood_Tarred"))
    # Planks still across the ribs on the high side, low down, one painted; snapped ends.
    for k, h in enumerate((0.18, 0.32, 0.47, 0.62)):
        if k == 2:
            continue
        x0, x1 = rng.uniform(-1.9, -1.2), rng.uniform(0.6, 1.9)

        def frame(t, h=h, x0=x0, x1=x1):
            x = x0 + (x1 - x0) * t
            p = rib_pt(x, 1.0, h) + Vector((0.0, 0.05, 0.0))
            up = (rib_pt(x, 1.0, h + 0.02) - rib_pt(x, 1.0, h - 0.02)).normalized()
            along = (rib_pt(x + 0.02, 1.0, h) - rib_pt(x - 0.02, 1.0, h)).normalized()
            n = along.cross(up).normalized()
            return tuple(p), tuple(up), tuple(n), 1.0
        mat = "MI_Paint_CreamAged" if k == 3 else ("MI_Wood_Tarred" if k == 0 else "MI_Wood_DockWeathered")
        parts.append(bb.laid_board("strake", (x1 - x0) * 1.02, 0.17, 0.035, frame, mat, wear=1.0, split=0.7))
    return finish(parts, "SM_Home_WreckHull", ucx=[((4.0, 1.0, 1.0), (0.0, 0.5, 0.3))])


def wreck_mast():
    """A snapped mast lying on the sand with a torn strip of sail and a tangle of rope."""
    mats()
    rng = rng_seed(7121)
    parts = []
    m = tb.spar("mast", 3.4, 0.09, 0.07, bb.M["MI_Wood_DockWeathered"], 7121, sides=18, sweep=0.03, axis=(1, 0, 0), facets=8)
    m.data.transform(__import__("mathutils").Matrix.Translation(Vector((-1.7, 0.0, 0.08))))
    parts.append(m)
    # A torn strip of sail draped over the sand beside it.
    rows, cols = 6, 10
    grid = []
    for i in range(rows + 1):
        row = []
        for j in range(cols + 1):
            x = -1.2 + 2.0 * j / cols + 0.05 * math.sin(i * 1.3)
            y = 0.12 + 0.9 * i / rows
            z = 0.02 + 0.06 * max(0.0, math.sin(j * 0.9 + i * 0.5)) + (0.08 if i == 0 else 0.0)
            if i == rows:
                y -= rng.uniform(0.0, 0.25)                         # the torn edge
            row.append(Vector((x, y, z)))
        grid.append(row)
    bm = bmesh.new()
    vv = [[bm.verts.new(U(tuple(p))) for p in row] for row in grid]
    uvl = bm.loops.layers.uv.new("UVMap")
    for i in range(rows):
        for j in range(cols):
            f = bm.faces.new([vv[i][j], vv[i][j + 1], vv[i + 1][j + 1], vv[i + 1][j]])
            for loop in f.loops:
                loop[uvl].uv = (loop.vert.co.x, loop.vert.co.y)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("sail")
    bm.to_mesh(me)
    bm.free()
    sail = sb.new_object("sail", me)
    sail.data.materials.append(bb.M["MI_Canvas"])
    sol = sail.modifiers.new("thick", "SOLIDIFY")
    sol.thickness = 0.01
    parts.append(sail)
    for k in range(5):
        a = rng.uniform(0, 6.28)
        parts.append(bb.ring("coil", rng.uniform(0.12, 0.25), 0.018, (0.9 + 0.1 * k, -0.35 + rng.uniform(-0.1, 0.1), 0.03 + 0.02 * k),
                             (math.sin(a) * 0.3, math.cos(a) * 0.3, 1.0), "MI_Rope_Hemp", 24))
    parts += bb.rope("rope", bb.sagging((1.0, -0.3, 0.05), (1.7, 0.08, 0.1), -0.02, 5), 0.016)
    return finish(parts, "SM_Home_WreckMast", ucx=[((3.4, 0.25, 0.25), (0.0, 0.0, 0.08))])


def wreck_plank(name, seed):
    mats()
    p = tb.rough_plank("plank", random.Random(seed).uniform(1.4, 2.3), 0.2, 0.035, (0, 0, 0.02), (0, 0, random.Random(seed).uniform(-0.1, 0.1)),
                       bb.M["MI_Wood_DockWeathered"], seed, wear=1.0, split_chance=0.8)
    return finish([p], name)


def wreck_barrel():
    mats()
    return finish(barrel_parts((0.0, 0.0, -0.2), broken=True), "SM_Home_WreckBarrel", ucx=[((0.62, 0.62, 0.7), (0.0, 0.0, 0.15))])


def heightfield(name, size, res, zfn, mat, colfn=None):
    """A grid mesh (Unreal metres, size = (x, y)) with heights zfn(x, y) and vertex colours colfn(x, y, z)."""
    nx, ny = int(size[0] / res), int(size[1] / res)
    bm = bmesh.new()
    vs = []
    for i in range(nx + 1):
        row = []
        for j in range(ny + 1):
            x, y = -size[0] / 2 + i * res, -size[1] / 2 + j * res
            row.append(bm.verts.new(U((x, y, zfn(x, y)))))
        vs.append(row)
    col = bm.loops.layers.float_color.new("Col")
    uvl = bm.loops.layers.uv.new("UVMap")
    for i in range(nx):
        for j in range(ny):
            f = bm.faces.new([vs[i][j], vs[i][j + 1], vs[i + 1][j + 1], vs[i + 1][j]])
            for loop in f.loops:
                c = loop.vert.co
                loop[uvl].uv = (c.x, c.y)
                loop[col] = colfn(c.x, -c.y, c.z) if colfn else (0.0, 0.0, 0.5, 1.0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
    me = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    o = sb.new_object(name, me)
    o.data.materials.append(bb.M[mat])
    for p in o.data.polygons:
        p.use_smooth = True
    return o


def tide_pools():
    """A low shelf of dark rock at the surf's edge with three pools of still water in it (starfish, urchins, anemones and
    shells inside). Origin at its middle on the beach."""
    mats()
    rng = rng_seed(7130)
    pools = [(-0.9, -0.4, 0.75, 0.55), (0.6, 0.35, 0.6, 0.45), (1.4, -0.7, 0.4, 0.32)]

    def z(x, y):
        wob = 0.12 * math.sin(math.atan2(y, x) * 5.0 + 1.3) + 0.06 * math.sin(math.atan2(y, x) * 11.0)
        e = max(abs(x) / 2.6, abs(y) / 1.7) * (1.0 + wob)
        base = 0.62 * (1.0 - max(0.0, e - 0.62) / 0.38) ** 0.6 - 0.25 if e < 1.0 else -0.3
        # Broad facets (terraced, chunky) rather than a smooth slab.
        f = 0.09 * math.sin(x * 2.3 + y * 1.1) + 0.06 * math.sin(x * 1.1 - y * 3.7) + 0.04 * math.sin(x * 6.1 + y * 4.3)
        base += round(f / 0.05) * 0.05 * 0.7 + f * 0.3
        for px, py, rx, ry in pools:
            d = math.hypot((x - px) / rx, (y - py) / ry)
            base -= 0.24 * max(0.0, 1.0 - d * d) ** 0.6
        return max(base, -0.35)

    def c(x, y, zz):
        return (0.0, 0.5 if zz < 0.15 else 0.0, 0.5 + 0.3 * math.sin(x * 2.0 + y), 1.0)
    shelf = heightfield("shelf", (5.6, 3.8), 0.08, z, "MI_Rock_Coast", c)
    parts = [shelf]
    for k in range(7):
        a = rng.uniform(0, 6.28)
        r = rng.uniform(1.4, 2.4)
        px_, py_ = math.cos(a) * r, math.sin(a) * r * 0.62
        if any(math.hypot((px_ - qx) / qr, (py_ - qy) / qs) < 1.2 for qx, qy, qr, qs in pools):
            continue
        parts.append(stone("boulder", (rng.uniform(0.45, 0.85), rng.uniform(0.4, 0.7), rng.uniform(0.3, 0.5)), (px_, py_, z(px_, py_) + 0.08),
                           rng, rot=rng.uniform(0, 3.1), bevel=0.42))
    own = []
    for px, py, rx, ry in pools:
        wz = 0.62 - 0.25 - 0.1
        disk = sb.add_cylinder("pool", 1.0, 0.01, (0, 0, 0), "Z", 20, bb.M["MI_IslPool"])
        disk.data.transform(__import__("mathutils").Matrix.Diagonal((rx * 0.92, ry * 0.92, 1.0, 1.0)))
        disk.data.transform(__import__("mathutils").Matrix.Translation(U((px, py, wz))))
        own.append(disk)
        # Life in the pool: a starfish, an urchin or two, anemones and shells on the bottom.
        if rng.random() < 0.9:
            star_c = (px + rng.uniform(-0.2, 0.2) * rx, py + rng.uniform(-0.2, 0.2) * ry, wz - 0.1)
            for k in range(5):
                a = 2 * math.pi * k / 5 + rng.uniform(-0.1, 0.1)
                tip = (star_c[0] + math.cos(a) * 0.09, star_c[1] + math.sin(a) * 0.09, star_c[2])
                own.append(bb.rod("star", star_c, tip, 0.016, "MI_IslFlower", 6))
        for k in range(rng.randint(1, 3)):
            uc = Vector((px + rng.uniform(-0.5, 0.5) * rx, py + rng.uniform(-0.5, 0.5) * ry, wz - 0.12))
            parts.append(bb.disc("urchin", 0.035, 0.04, tuple(uc), (0, 0, 1), "MI_Metal_Iron", 10))
            for j in range(10):
                a = rng.uniform(0, 6.28)
                d = Vector((math.cos(a), math.sin(a), rng.uniform(0.2, 1.0))).normalized()
                parts.append(bb.rod("spine", tuple(uc), tuple(uc + d * 0.07), 0.003, "MI_Metal_Iron", 4))
        for k in range(rng.randint(2, 4)):
            ac = (px + rng.uniform(-0.6, 0.6) * rx, py + rng.uniform(-0.6, 0.6) * ry, wz - 0.13)
            own.append(bb.disc("anemone", rng.uniform(0.03, 0.05), 0.05, ac, (0, 0, 1), "MI_IslSeaweed", 10))
    ucx = [((5.0, 3.2, 0.5), (0.0, 0.0, 0.0))]
    return finish(parts, "SM_Home_TidePools", ucx, separate=own)


def cairn():
    """The old lookout on the hill's crown: a cairn of flat stones, a weathered post beside it with a sighting board (its
    notch on the Spire, local +X) and a faded red rag, a log to sit on."""
    mats()
    rng = rng_seed(7140)
    parts = []
    z = -0.1
    w = 0.95
    while w > 0.25:
        h = rng.uniform(0.12, 0.2)
        k = rng.randint(1, 3) if w > 0.5 else 1
        for i in range(k):
            off = (rng.uniform(-0.12, 0.12) * w, rng.uniform(-0.12, 0.12) * w)
            parts.append(stone("cairn", (w * rng.uniform(0.6, 0.9), w * rng.uniform(0.55, 0.85), h), (off[0], off[1], z + h / 2), rng,
                               rot=rng.uniform(0, 3.14), bevel=0.25))
        z += h * 0.92
        w *= rng.uniform(0.8, 0.88)
    parts.append(bb.post("sightpost", 0.07, 2.1, (0.9, 0.55, -0.4), "MI_Wood_DockWeathered", sides=6, taper=0.9))
    parts.append(board("sightboard", (0.75, 0.55, 1.55), (1.15, 0.55, 1.55), 0.16, 0.03, (0, 0, 1), "MI_Wood_DockWeathered"))
    for dy in (-0.05, 0.05):
        parts.append(bb.block("notch", (0.06, 0.03, 0.08), (1.13, 0.55 + dy, 1.62), "MI_Wood_DockWeathered", 0.008))
    rag = []
    for i in range(6):
        rag.append((0.92 + 0.02 * i, 0.58 + 0.12 * i, 1.4 - 0.1 * i - 0.04 * i * i))
    parts += bb.rope("rag", rag, 0.025, "MI_Paint_HullFaded")
    log = tb.spar("log", 1.7, 0.16, 0.13, bb.M["MI_Wood_DockWeathered"], 7141, sides=16, sweep=0.05, axis=(0.3, 1.0, 0.0), facets=7)
    log.data.transform(__import__("mathutils").Matrix.Translation(U((-1.2, -0.6, 0.12))))
    parts.append(log)
    return finish(parts, "SM_Home_Cairn", ucx=[((1.0, 1.0, 1.4), (0.0, 0.0, 0.6)), ((0.2, 0.2, 1.8), (0.9, 0.55, 0.8))])


def spring():
    """The spring at the hill's foot: clear water running out of a broken pipe of pale Old World composite (a faint
    turquoise band round its mouth, used sparingly) into a pool ringed with mossy stones. Origin at the pool's middle;
    local +X points from the pipe to the pool."""
    mats()
    rng = rng_seed(7150)
    parts = []
    own = []
    # The pipe: comes out of the slope at -X, tilted down a little, its end broken.
    start = Vector((-4.0, 0.0, 0.58))
    mouth_c = Vector((-1.05, 0.0, 0.44))
    axis = (mouth_c - start).normalized()
    L = (mouth_c - start).length
    outer, inner = 0.4, 0.31
    bm = bmesh.new()
    segs = 28
    rows = []
    for i in range(9):
        t = i / 8
        row_o, row_i = [], []
        for j in range(segs):
            a = 2 * math.pi * j / segs
            brk = (0.22 * (0.5 + 0.5 * math.sin(a * 2 + 0.7)) + 0.06 * math.sin(a * 7)) if i == 8 else 0.0
            c = start + axis * (L * t - brk)
            n1 = Vector((0, 1, 0))
            n2 = axis.cross(n1).normalized()
            row_o.append(bm.verts.new(U(tuple(c + (n1 * math.cos(a) + n2 * math.sin(a)) * outer))))
            row_i.append(bm.verts.new(U(tuple(c + (n1 * math.cos(a) + n2 * math.sin(a)) * inner))))
        rows.append((row_o, row_i))
    for i in range(8):
        for j in range(segs):
            k = (j + 1) % segs
            bm.faces.new((rows[i][0][j], rows[i][0][k], rows[i + 1][0][k], rows[i + 1][0][j]))
            bm.faces.new((rows[i][1][j], rows[i + 1][1][j], rows[i + 1][1][k], rows[i][1][k]))
    for j in range(segs):
        k = (j + 1) % segs
        bm.faces.new((rows[8][0][j], rows[8][1][j], rows[8][1][k], rows[8][0][k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("pipe")
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    pipe = sb.new_object("pipe", me)
    pipe.data.materials.append(bb.M["MI_Stone_Ruin"])
    sb.soften(pipe, 0.02, 2)
    parts.append(pipe)
    # The faint turquoise band, a little back from the mouth (Old World light, almost gone).
    band_c = start + axis * (L - 0.45)
    ring = bb.ring("band", outer + 0.004, 0.012, tuple(band_c), tuple(axis), "MI_IslAncientFaint", 40)
    own.append(ring)
    # The water: a ribbon falling from the mouth to the pool, the pool's surface.
    mouth = start + axis * (L - 0.1) - Vector((0, 0, inner * 0.8))
    drop = mouth.z + 0.02
    fall = [mouth + Vector((0.06 * k, 0.0, -drop * (k / 5) ** 2)) for k in range(6)]
    for a, b in zip(fall[:-1], fall[1:]):
        own.append(bb.rod("trickle", tuple(a), tuple(b), 0.035, "MI_IslPool", 8))
    pool = sb.add_cylinder("pool", 1.0, 0.02, (0, 0, 0), "Z", 24, bb.M["MI_IslPool"])
    pool.data.transform(__import__("mathutils").Matrix.Diagonal((1.15, 0.95, 1.0, 1.0)))
    pool.data.transform(__import__("mathutils").Matrix.Translation(U((-0.1, 0.0, -0.02))))
    own.append(pool)
    # The basin under the water (dark stones and silt), held up above the slope by the ring of stones.
    basin = heightfield("basin", (2.3, 1.9), 0.1, lambda x, y: -0.06 - 0.16 * max(0.0, 1.0 - (x / 1.1) ** 2 - (y / 0.92) ** 2),
                        "MI_Rock_Coast", lambda x, y, z: (0.0, 1.0, 0.4, 1.0))
    parts.append(basin)
    for k in range(13):
        a = 2 * math.pi * k / 13 + rng.uniform(-0.1, 0.1)
        if -0.6 < (a - math.pi) < 0.6:
            continue                                                 # the gap where the pipe pours in
        r = 1.22 + rng.uniform(-0.05, 0.1)
        parts.append(stone("ring", (rng.uniform(0.38, 0.58), rng.uniform(0.32, 0.46), rng.uniform(0.24, 0.34)),
                           (math.cos(a) * r * 1.0, math.sin(a) * r * 0.85, 0.04), rng, rot=a, bevel=0.34))
    # The runoff: a few stones where the pool spills towards the lagoon.
    for k in range(4):
        parts.append(stone("run", (0.25, 0.2, 0.12), (1.5 + 0.35 * k, rng.uniform(-0.25, 0.25), -0.02), rng, bevel=0.35))
    ucx = [((2.9, 0.82, 0.82), (-2.55, 0.0, 0.5))]
    return finish(parts, "SM_Home_Spring", ucx, separate=own)


def fire_pit():
    """A fire pit by the path: a ring of stones round an ash bed, charred logs with a few embers still glowing, a tripod of
    poles with an iron pot on a rope."""
    mats()
    rng = rng_seed(7160)
    parts = []
    own = []
    for k in range(11):
        a = 2 * math.pi * k / 11 + rng.uniform(-0.08, 0.08)
        parts.append(stone("ring", (rng.uniform(0.26, 0.36), rng.uniform(0.2, 0.28), rng.uniform(0.14, 0.22)),
                           (math.cos(a) * 0.62, math.sin(a) * 0.62, 0.04), rng, rot=a, bevel=0.3))
    ash = heightfield("ash", (1.1, 1.1), 0.08, lambda x, y: 0.02 + 0.03 * max(0.0, 1.0 - (x * x + y * y) / 0.3) - 0.04 * (x * x + y * y),
                      "MI_Caulk_Dark", lambda x, y, z: (0.0, 1.0, 0.5, 1.0))
    parts.append(ash)
    for k in range(4):
        a = rng.uniform(0, 6.28)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        lg = tb.spar("log", rng.uniform(0.45, 0.65), 0.05, 0.04, bb.M["MI_Wood_Tarred"], 7161 + k, sides=10, sweep=0.04,
                     axis=(1, 0, 0), facets=6)
        lg.data.transform(__import__("mathutils").Matrix.Rotation(a, 4, "Z"))
        lg.data.transform(__import__("mathutils").Matrix.Translation(U(tuple(-d * 0.25 + Vector((0, 0, 0.07 + 0.03 * k))))))
        parts.append(lg)
    for k in range(7):
        own.append(stone("ember", (0.05, 0.04, 0.03), (rng.uniform(-0.2, 0.2), rng.uniform(-0.2, 0.2), 0.05), rng, mat="MI_CandleFlame", bevel=0.4))
    top = Vector((0.0, 0.0, 1.35))
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.3
        foot = Vector((math.cos(a) * 0.85, math.sin(a) * 0.85, -0.05))
        parts.append(pole("tripod", tuple(foot), tuple(top + (top - foot).normalized() * 0.12), 0.03, "MI_Wood_DockWeathered", 7170 + k))
    parts += bb.rope("potrope", [tuple(top), (0.0, 0.0, 0.62)], 0.01)
    pot = sb.add_cylinder("pot", 0.17, 0.22, (0, 0, 0), "Z", 16, bb.M["MI_Metal_Iron"], radius_top=0.14)
    sb.soften(pot, 0.02, 2)
    pot.data.transform(__import__("mathutils").Matrix.Translation(U((0.0, 0.0, 0.5))))
    parts.append(pot)
    parts.append(bb.ring("handle", 0.15, 0.008, (0.0, 0.0, 0.64), (1, 0, 0), "MI_Metal_Iron", 16))
    return finish(parts, "SM_Home_FirePit", ucx=[((1.6, 1.6, 0.3), (0.0, 0.0, 0.08))], separate=own)


def log_seat(name, seed):
    mats()
    lg = tb.spar("log", random.Random(seed).uniform(1.4, 1.9), 0.17, 0.15, bb.M["MI_Wood_DockWeathered"], seed, sides=16, sweep=0.05,
                 axis=(1, 0, 0), facets=7)
    lg.data.transform(__import__("mathutils").Matrix.Translation(Vector((-0.8, 0.0, 0.13))))
    return finish([lg], name, ucx=[((1.7, 0.34, 0.3), (0.0, 0.0, 0.13))])


def stake():
    """A driftwood stake pushed into the sand by the loose mound (the dig spot's sign, as a sailor would leave it)."""
    mats()
    p = tb.rough_post("stake", 0.035, 0.75, (0, 0, -0.3), sides=6, taper=0.6, material=bb.M["MI_Wood_DockWeathered"], seed=7180)
    sb.transform_obj(p, (0.0, 0.25, 0.0))
    return finish([p], "SM_Home_Stake")


# --- where the places go (home_places.json) ---------------------------------------------------------------------------
def world(place, local):
    """A local point of a place (m) in world cm, and the place's yaw."""
    p = S.PLACES[place]
    a = math.radians(p["yaw"])
    x = p["x"] + local[0] * math.cos(a) - local[1] * math.sin(a)
    y = p["y"] + local[0] * math.sin(a) + local[1] * math.cos(a)
    return x, y


def write_places():
    """The places' actors for place_home_island.py, on the ground of build_home_island.surface (the visible ground)."""
    import numpy as np
    import build_home_island as H
    plants = H.scatter_plants()

    def gz(x, y):
        return float(H.surface(np.array([x]), np.array([y]), plants)["z"][0])

    out = {"meshes": [], "lanterns": []}
    hp = S.PLACES["keeper_hut"]
    z0 = gz(hp["x"], hp["y"])
    out["meshes"].append({"label": "KeeperHut", "mesh": "SM_Home_KeeperHut", "loc": [hp["x"] * 100, hp["y"] * 100, z0 * 100],
                          "rot": [0, hp["yaw"], 0]})
    cx, cy = world("keeper_hut", (-HX - 0.48, -1.15))
    out["meshes"].append({"label": "KeeperSmoke", "mesh": "SM_Home_Smoke", "loc": [cx * 100, cy * 100, (z0 + 4.5) * 100],
                          "rot": [0, 0, 0], "collide": False, "shadow": False})
    lx, ly = world("keeper_hut", (HX + 0.55, 1.95))
    out["lanterns"].append({"loc": [lx * 100, ly * 100, (z0 + 2.0) * 100], "rot": [0, hp["yaw"], 0]})
    lx, ly = world("keeper_hut", (HX - 0.3, -1.45))
    out["lanterns"].append({"loc": [lx * 100, ly * 100, (z0 + COUNTER) * 100], "rot": [0, hp["yaw"] + 20, 0]})
    sx, sy = world("keeper_hut", KEEPER_SPOT[:2])
    px, py = world("keeper_hut", KEEPER_POINT[:2])
    out["keeper"] = {"spot": [sx * 100, sy * 100, (z0 + KEEPER_SPOT[2]) * 100], "point": [px * 100, py * 100, (z0 + KEEPER_POINT[2]) * 100],
                     "yaw": hp["yaw"], "extent": [45.0, 95.0, 60.0]}

    def slope(x, y, yaw):
        """(pitch, roll) that lay a thing on the ground's slope at (x, y), facing yaw (degrees)."""
        e = 0.3
        gx = (gz(x + e, y) - gz(x - e, y)) / (2 * e)
        gy = (gz(x, y + e) - gz(x, y - e)) / (2 * e)
        a = math.radians(yaw)
        sx_, sy_ = gx * math.cos(a) + gy * math.sin(a), -gx * math.sin(a) + gy * math.cos(a)
        return math.degrees(math.atan(sx_)), -math.degrees(math.atan(sy_))

    def piece(place, label, mesh, local, yaw=0.0, sink=0.0, tilt=(0.0, 0.0), lay=True, collide=True, path=None):
        x, y = world(place, local)
        yw = S.PLACES[place]["yaw"] + yaw
        pitch, roll = slope(x, y, yw) if lay else (0.0, 0.0)
        z = gz(x, y) - sink
        entry = {"label": label, "mesh": mesh, "loc": [round(x * 100, 1), round(y * 100, 1), round(z * 100, 1)],
                 "rot": [round(pitch + tilt[0], 2), round(yw, 2), round(roll + tilt[1], 2)], "collide": collide}
        if path:
            entry["path"] = path
        out["meshes"].append(entry)

    # The wreck: the hull's side lying tilted and half buried, the snapped mast and its sail, planks, a stove-in barrel.
    piece("wreck", "WreckHull", "SM_Home_WreckHull", (0.0, 0.0), -10.0, sink=0.05)
    piece("wreck", "WreckMast", "SM_Home_WreckMast", (3.2, 3.6), 24.0, sink=0.03)
    piece("wreck", "WreckPlank1", "SM_Home_PlankA", (-3.4, 1.8), 75.0, sink=0.01, collide=False)
    piece("wreck", "WreckPlank2", "SM_Home_PlankB", (1.8, -3.3), -20.0, sink=0.01, collide=False)
    piece("wreck", "WreckPlank3", "SM_Home_PlankA", (5.4, 0.8), 140.0, sink=0.01, collide=False)
    piece("wreck", "WreckBarrel", "SM_Home_WreckBarrel", (-1.6, -3.4), 0.0, sink=0.05, tilt=(0.0, 24.0))
    piece("wreck", "WreckCrate", "SM_Crate_Small", (-4.4, -1.2), 35.0, sink=0.12, tilt=(9.0, 6.0), path="/Game/Saltbound/Props/")
    # The tide pools, the lookout, the spring, the fire pit and its seats, the dig spot's stake.
    piece("tide_pools", "TidePools", "SM_Home_TidePools", (0.0, 0.0), 0.0, sink=0.12)
    piece("lookout", "Cairn", "SM_Home_Cairn", (0.0, 0.0), 0.0, sink=0.05, lay=False)
    piece("spring", "Spring", "SM_Home_Spring", (0.0, 0.0), 0.0, sink=-0.18, lay=False)        # on the rim (the bowl is 0.18 deep)
    piece("fire_pit", "FirePit", "SM_Home_FirePit", (0.0, 0.0), 0.0, sink=0.03, lay=False)
    piece("fire_pit", "LogSeat1", "SM_Home_LogSeatA", (1.6, 0.9), 70.0, sink=0.04)
    piece("fire_pit", "LogSeat2", "SM_Home_LogSeatB", (-1.4, 1.2), -25.0, sink=0.04)
    piece("dig_spot", "DigStake", "SM_Home_Stake", (0.5, 0.4), 30.0, sink=0.0, lay=False, collide=False)
    piece("fallen_palm", "FallenPalm", "SM_Home_FallenPalm", (0.0, 0.0), 0.0, sink=0.32, lay=False,
          path="/Game/Saltbound/Environment/Vegetation/")
    # Embers in the fire pit and the spring's band give a little light at night.
    fx, fy = world("fire_pit", (0.0, 0.0))
    out["lights"] = [{"label": "FirePit", "loc": [fx * 100, fy * 100, (gz(fx, fy) + 0.35) * 100], "intensity": 6.0,
                      "color": [1.0, 0.45, 0.18], "radius": 600.0}]
    with open(PLACES_JSON, "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=1)
    print("SB_PLACES", PLACES_JSON, "hut ground", round(z0, 3), "keeper", out["keeper"])


BUILDERS = {"hut": hut, "smoke": smoke,
            "shells": lambda: (shell("SM_Home_Shell_A", 7111, "cockle", 0.15), sb.reset_scene(), shell("SM_Home_Shell_B", 7112, "cockle", 0.85),
                               sb.reset_scene(), shell("SM_Home_Shell_C", 7113, "whelk", 0.4)),
            "pebbles": lambda: (pebbles("SM_Home_Pebble_A", 7114, 6, 0.035), sb.reset_scene(), pebbles("SM_Home_Pebble_B", 7115, 3, 0.07)),
            "debris": lambda: (husks("SM_Home_DebrisHusk", 7116), sb.reset_scene(), frond_bit("SM_Home_DebrisFrond", 7117), sb.reset_scene(),
                               twigs("SM_Home_DebrisTwigs", 7118), sb.reset_scene(), driftstick("SM_Home_DebrisStick", 7119)),
            "seaweed": lambda: seaweed("SM_Home_ClumpSeaweed", 7122),
            "burrow": burrow,
            "wreck": lambda: (wreck_hull(), sb.reset_scene(), wreck_mast(), sb.reset_scene(), wreck_plank("SM_Home_PlankA", 7123),
                              sb.reset_scene(), wreck_plank("SM_Home_PlankB", 7124), sb.reset_scene(), wreck_barrel()),
            "tidepools": tide_pools, "cairn": cairn, "spring": spring,
            "firepit": lambda: (fire_pit(), sb.reset_scene(), log_seat("SM_Home_LogSeatA", 7162), sb.reset_scene(),
                                log_seat("SM_Home_LogSeatB", 7163)),
            "stake": stake}

if __name__ == "__main__":
    args = sb.script_args() if "--" in sys.argv else sys.argv[1:]
    if bpy.app.background and "places" not in args:
        for key in (args or list(BUILDERS)):
            sb.reset_scene()
            BUILDERS[key]()
    if "places" in args:
        write_places()
