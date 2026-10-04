"""Hand-made, weathered timber (stylised, Sea of Thieves-like dock reference; see Docs/Decisions.md D20).

Every board or post is built from its own seed, so repeated shapes do not show:
  rough_plank  thick board on a subdivided box: width/thickness drift, gentle bend and twist, wobbly long
               edges, slight cupping, a few broad grain grooves, locally worn (chamfered) edges, and
               occasionally an angled, chipped or split end. Bevel per board: rounded or angular.
  rough_post   rough timber post: irregular polygon section, taper, uneven faces, worn sloping top.
UVs: U runs along the board length (1 UV unit = 1 m), so the wood grain follows each board.
All sizes in metres. Built along local X, then rotated (yaw/pitch/roll in radians) and moved into place.
"""
import math
import random

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

import sb_blender as sb


class Wave1D:
    """Smooth 1D noise on s in [-0.5, 0.5]: a few sines with random phases (deterministic per rng)."""

    def __init__(self, rng, octaves=3, base=1.0):
        self.terms = [(rng.uniform(0.6, 1.4) * base * (2 ** o), rng.uniform(0, 2 * math.pi), 0.5 ** o) for o in range(octaves)]
        self.norm = sum(t[2] for t in self.terms)

    def __call__(self, s):
        return sum(a * math.sin(2 * math.pi * f * s + p) for f, p, a in self.terms) / self.norm


def _grid_box(name, length, width, thick, nx, ny, nz=2):
    """Box centred at the origin (bottom at -thick/2) with edge loops: nx along X, ny across Y, nz up Z."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(length, width, thick), verts=bm.verts)
    for axis, n, size in ((0, nx, length), (1, ny, width), (2, nz, thick)):
        for i in range(1, n):
            co = [0.0, 0.0, 0.0]
            co[axis] = -size / 2 + size * i / n
            no = [0.0, 0.0, 0.0]
            no[axis] = 1.0
            geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
            bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no)
    return bm


def _plank_uv(bm, length_axis_offset, rng):
    """Box UVs in local space with U along the board (X). Random offset per board."""
    uv = bm.loops.layers.uv.verify()
    du, dv = rng.uniform(0, 1), rng.uniform(0, 1)
    for f in bm.faces:
        n = f.normal
        ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
        for loop in f.loops:
            c = loop.vert.co
            if ax > ay and ax > az:          # end grain: across the board
                u, v = c.y, c.z
            elif ay > az:                     # long sides
                u, v = c.x, c.z
            else:                             # top / bottom
                u, v = c.x, c.y
            loop[uv].uv = (u + du + length_axis_offset, v + dv)


def rough_plank(name, length, width, thick, center, rotation=(0.0, 0.0, 0.0), material=None, seed=0,
                wear=1.0, grooves=(2, 4), split_chance=0.25, bevel=1.0):
    """Weathered board along local X. `wear` scales bends, chamfers and end damage (0 = clean board)."""
    rng = random.Random(seed)
    nx = max(4, int(length / 0.12))
    ny = 14
    bm = _grid_box(name, length, width, thick, nx, ny, 2)
    bend_z = rng.uniform(-0.012, 0.008) * wear * min(length / 2.0, 1.5)
    bend_y = rng.uniform(-0.01, 0.01) * wear * min(length / 2.0, 1.5)
    twist = math.radians(rng.uniform(-2.5, 2.5)) * wear
    cup = rng.uniform(0.0, 0.004) * wear
    width_drift = Wave1D(rng, 2, 1.0)
    edge_a, edge_b = Wave1D(rng, 3, 2.0), Wave1D(rng, 3, 2.0)
    thick_drift = Wave1D(rng, 2, 1.0)
    # Broad grain grooves along the board (top face only).
    groove_list = []
    for _ in range(rng.randint(*grooves) if grooves else 0):
        groove_list.append((rng.uniform(-0.75, 0.75), Wave1D(rng, 2, 1.0), rng.uniform(0.0015, 0.0035) * (0.5 + 0.5 * wear),
                            rng.uniform(0.06, 0.13), rng.uniform(-0.5, 0.2), rng.uniform(0.3, 0.5)))
    # Worn chamfers on the top long edges: (side, s centre, half length in s, depth).
    chamfers = []
    for _ in range(rng.randint(1, 3)):
        chamfers.append((rng.choice((-1, 1)), rng.uniform(-0.45, 0.45), rng.uniform(0.06, 0.2), rng.uniform(0.006, 0.016) * wear))
    # End damage: angled end, chip or split, on one end.
    end_side = rng.choice((-1, 1))
    end_slant = rng.uniform(-0.03, 0.03) * wear
    split = rng.random() < split_chance * wear
    split_t, split_depth = rng.uniform(-0.4, 0.4), rng.uniform(0.04, 0.12)
    chip = (not split) and rng.random() < 0.35 * wear
    chip_side, chip_size = rng.choice((-1, 1)), rng.uniform(0.02, 0.05)

    hw, ht = width / 2, thick / 2
    for v in bm.verts:
        x, y, z = v.co
        s = x / length                       # -0.5 .. 0.5
        t = y / hw                           # -1 .. 1
        top = z > 0
        # Width: drift along the board and wobbly long edges.
        scale = 1.0 + 0.05 * wear * width_drift(s)
        y = y * scale + (0.5 * (t + 1) * edge_a(s) + 0.5 * (1 - t) * edge_b(s)) * 0.006 * wear * abs(t)
        # Thickness drift (top only) and cupping (edges a little higher than the middle).
        if top:
            z += 0.004 * wear * thick_drift(s) + cup * (t * t - 0.5)
            for t0, wander, depth, w, s0, s1 in grooves_or_empty(groove_list):
                tg = t0 + 0.12 * wander(s)
                fade = _fade(s, -0.5, 0.5)
                z -= depth * math.exp(-((t - tg) / w) ** 2) * fade
            for side, sc, hl, depth in chamfers:
                if side * t > 0.8:
                    k = max(0.0, 1.0 - abs(s - sc) / hl)
                    k = k * k * (3 - 2 * k) * (side * t - 0.8) / 0.2
                    z -= depth * k
                    y -= side * depth * 0.8 * k
        # End damage on one end.
        if s * end_side > 0.49:
            x += end_slant * t * end_side
            if split and abs(t - split_t) < 0.12:
                x -= end_side * split_depth * (1.0 - abs(t - split_t) / 0.12)
            if chip and t * chip_side > 0.55 and top:
                x -= end_side * chip_size * (t * chip_side - 0.55) / 0.45
                z -= chip_size * 0.3
        # Gentle bend and twist.
        z += bend_z * (1.0 - 4.0 * s * s)
        y += bend_y * (1.0 - 4.0 * s * s)
        a = twist * s
        y, z = y * math.cos(a) - z * math.sin(a), y * math.sin(a) + z * math.cos(a)
        v.co = Vector((x, y, z))
    bm.normal_update()
    _plank_uv(bm, 0.0, rng)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    if material:
        obj.data.materials.append(material)
    # Worn bevels: some boards rounded, some with broad angular facets.
    rounded = rng.random() < 0.55
    sb.soften_marked(obj, bevel * (rng.uniform(0.006, 0.012) if rounded else rng.uniform(0.008, 0.016)), 3 if rounded else 1)
    m = Matrix.Translation(Vector(center)) @ Euler(rotation).to_matrix().to_4x4()
    obj.data.transform(m)
    for p in obj.data.polygons:
        p.use_smooth = rounded
    return obj


def grooves_or_empty(g):
    return g or ()


def _fade(s, a, b, edge=0.06):
    return max(0.0, min(1.0, (s - a) / edge, (b - s) / edge))


def rough_post(name, radius, height, center_bottom, sides=None, taper=0.85, material=None, seed=0, top_wear=1.0):
    """Rough timber post standing on center_bottom (local Z up): irregular polygon section, taper,
    uneven faces, worn sloping top. Returns the object (UV: U along the post)."""
    rng = random.Random(seed)
    sides = sides or rng.randint(6, 8)
    rings = max(4, int(height / 0.25))
    radii = [radius * rng.uniform(0.82, 1.1) for _ in range(sides)]
    angles = [2 * math.pi * (i + rng.uniform(-0.18, 0.18)) / sides for i in range(sides)]
    bulge = [Wave1D(rng, 2, 1.0) for _ in range(sides)]
    tilt = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized()
    top_slope = rng.uniform(0.02, 0.06) * top_wear
    bm = bmesh.new()
    ring_verts = []
    for r in range(rings + 1):
        h = height * r / rings
        k = 1.0 - (1.0 - taper) * (h / height)
        row = []
        for i in range(sides):
            rad = radii[i] * k * (1.0 + 0.06 * bulge[i](h / height - 0.5))
            p = Vector((math.cos(angles[i]) * rad, math.sin(angles[i]) * rad, h))
            if r == rings:
                p.z -= top_slope * (p.x * tilt.x + p.y * tilt.y) / max(radius, 1e-3) + rng.uniform(0, 0.012) * top_wear
            row.append(bm.verts.new(p))
        ring_verts.append(row)
    for r in range(rings):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((ring_verts[r][i], ring_verts[r][j], ring_verts[r + 1][j], ring_verts[r + 1][i]))
    bm.faces.new(list(reversed(ring_verts[0])))
    bm.faces.new(ring_verts[-1])
    bm.normal_update()
    uv = bm.loops.layers.uv.verify()
    du = rng.uniform(0, 1)
    for f in bm.faces:
        for loop in f.loops:
            c = loop.vert.co
            if abs(f.normal.z) > 0.7:
                loop[uv].uv = (c.x + du, c.y)
            else:
                ang = math.atan2(c.y, c.x)
                loop[uv].uv = (c.z + du, ang * radius)     # grain runs up the post
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    if material:
        obj.data.materials.append(material)
    # Hewn look: flat faces with rounded corners (bevel + hardened normals), not a smooth cylinder.
    sb.soften_marked(obj, radius * rng.uniform(0.05, 0.09), 2)
    obj.data.transform(Matrix.Translation(Vector(center_bottom)))
    return obj


def bend_board(obj, length, frame):
    """Lay a straight board (built along local X, centred, by rough_plank at the origin) along a curve.

    frame(s) for s in [0, 1] (from the board's -X end to its +X end) returns (P, A, N, width_scale): the point the
    board's centre line passes, the unit direction its width (local Y) follows, the unit direction its thickness
    (local Z) follows, and a width factor (strakes narrow towards the stem). Every irregularity of the board
    (bends, grooves, worn edges, bevels) is kept, so curved hull planks look as hand-made as straight ones."""
    me = obj.data
    for v in me.vertices:
        x, y, z = v.co
        s = min(max(x / length + 0.5, 0.0), 1.0)
        p, a, n, w = frame(s)
        v.co = p + a * (y * w) + n * z
    me.update()
    return obj


def spar(name, length, radius, end_radius, material=None, seed=0, sides=14, sweep=0.03, axis=(-1.0, 0.0, 0.0)):
    """Hand-shaped round spar (mast, boom, gaff): tapered, faintly faceted, gently crooked along its length.
    Built from the origin along `axis`; returns the object."""
    rng = random.Random(seed)
    rings = max(6, int(length / 0.3))
    bow_y, bow_z = Wave1D(rng, 2, 0.5), Wave1D(rng, 2, 0.5)
    radii = [rng.uniform(0.96, 1.04) for _ in range(sides)]
    bm = bmesh.new()
    rows = []
    for r in range(rings + 1):
        t = r / rings
        rad = radius + (end_radius - radius) * t ** 1.3
        cy = sweep * bow_y(t - 0.5) * math.sin(math.pi * t)
        cz = sweep * bow_z(t - 0.5) * math.sin(math.pi * t)
        row = []
        for i in range(sides):
            a = 2 * math.pi * i / sides
            row.append(bm.verts.new((t * length, cy + math.cos(a) * rad * radii[i], cz + math.sin(a) * rad * radii[i])))
        rows.append(row)
    for r in range(rings):
        for i in range(sides):
            j = (i + 1) % sides
            f = bm.faces.new((rows[r][i], rows[r][j], rows[r + 1][j], rows[r + 1][i]))
            f.smooth = True
    for row, rev in ((rows[0], True), (rows[-1], False)):
        f = bm.faces.new(list(reversed(row)) if rev else row)
        f.smooth = False
    bm.normal_update()
    uv = bm.loops.layers.uv.verify()
    du = rng.uniform(0, 1)
    for f in bm.faces:
        for loop in f.loops:
            c = loop.vert.co
            loop[uv].uv = (c.x + du, math.atan2(c.z, c.y) * radius)      # grain along the spar
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    if material:
        obj.data.materials.append(material)
    d = Vector(axis).normalized()
    obj.data.transform(Vector((1, 0, 0)).rotation_difference(d).to_matrix().to_4x4())
    return obj
