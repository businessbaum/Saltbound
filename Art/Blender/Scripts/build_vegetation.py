"""Plants of the home island (D60): palms, broadleaf trees, bushes, ferns, big-leaf plants and beach grass.

Owner (2026-10-05): "a much more interesting start island with nature, trees, palms etc." In the style brief (D16, D38):
chunky, hand-made shapes, gently crooked, broad colour. Leaves are geometry (no cut-out cards): fronds of leaflet strips,
canopies of leaf clusters round a dark core. Every part is a plain mesh; the colour comes from the foliage materials
(M_SB_Foliage, M_SB_FoliageBark), which read the vertex colours as masks:

  R  tone along the part: 0 shaded (a leaf's base, the canopy's inside, a ring's scar) .. 1 lit (tips, the top)
  G  age: 0 fresh green .. 1 dry brown (dead fronds, straw in the grass, coconuts in between)
  B  a random value per part (tone variation)
  A  how freely it moves in the wind: 0 rigid (trunks, the root of a frond) .. 1 a leaf's tip

  SM_Palm_A / B / C   coconut palms: tall and gently curved (9 m), leaning far over (7 m), young (4.5 m). Ringed trunks,
                      a crown of arching fronds, coconuts, one or two dead fronds hanging. Lean towards +X.
  SM_Tree_A / B       broadleaf trees (9 m with three limbs, 6.5 m and rounder): buttress roots, ridged bark, canopies
                      of leaf clusters.
  SM_Bush_A           a round leafy shrub (1.2 m); SM_Bush_B a shrub of strap-leaved tufts on thin stems (1.4 m);
  SM_Bush_Flower      a shrub with red flowers.
  SM_Fern_A           a fern rosette (1 m); SM_BigLeaf_A big paddle leaves on long stalks (1.6 m).
  SM_GrassTuft_A / B  beach grass, green and straw (0.5-0.7 m).

Pivots at the root on the ground; trunks have UCX collision (palms and trees only). Unreal coordinates in metres
(Blender Y = -Unreal Y; the shapes here are symmetric enough that it does not matter).
Run: Tools/Build-BlenderAsset.ps1 build_vegetation [palm_a palm_b palm_c tree_a tree_b bush_a bush_b flower fern bigleaf grass]
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

UP = Vector((0.0, 0.0, 1.0))
MATS = {"MI_Bark_Palm": (0.45, 0.36, 0.26), "MI_Leaf_Palm": (0.3, 0.55, 0.18), "MI_Bark_Tree": (0.35, 0.27, 0.2),
        "MI_Leaf_Tree": (0.2, 0.45, 0.15), "MI_Leaf_Bush": (0.28, 0.52, 0.2), "MI_Flower": (0.85, 0.15, 0.2),
        "MI_Grass_Beach": (0.6, 0.62, 0.3)}


class Builder:
    """One bmesh with UVs, vertex colours and material slots."""

    def __init__(self, name, slots, seed):
        self.name = name
        self.slots = slots
        self.rng = random.Random(seed)
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.col = self.bm.loops.layers.float_color.new("Col")
        self.ucx = []

    def face(self, verts, uvs, cols, slot):
        f = self.bm.faces.new(verts)
        f.material_index = self.slots.index(slot)
        f.smooth = True
        for loop, uv, c in zip(f.loops, uvs, cols):
            loop[self.uv].uv = uv
            loop[self.col] = c
        return f

    def finish(self, category="Vegetation"):
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me)
        self.bm.free()
        obj = sb.new_object(self.name, me)
        for s in self.slots:
            obj.data.materials.append(sb.get_material(s, (*MATS[s], 1.0)))
        me.color_attributes.active_color = me.color_attributes["Col"]
        objs = [obj]
        for k, pts in enumerate(self.ucx):
            hb = bmesh.new()
            for p in pts:
                hb.verts.new(p)
            bmesh.ops.convex_hull(hb, input=hb.verts)
            hm = bpy.data.meshes.new("UCX_%s_%02d" % (self.name, k))
            hb.to_mesh(hm)
            hb.free()
            u = sb.new_object("UCX_%s_%02d" % (self.name, k), hm)
            u.display_type = "WIRE"
            objs.append(u)
        print("SB_PLANT", self.name, "faces", len(me.polygons), "ucx", len(self.ucx))
        sb.save_blend(self.name)
        sb.export_fbx(objs, os.path.join(category, self.name + ".fbx"))
        return obj


# --- shapes -------------------------------------------------------------------------------------------------------
def bezier(p0, p1, p2, p3, n):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 3
        b = 3 * (1 - t) ** 2 * t
        c = 3 * (1 - t) * t * t
        d = t ** 3
        out.append(p0 * a + p1 * b + p2 * c + p3 * d)
    return out


def resample(pts, step):
    """Points along a polyline every `step` metres (the last point kept)."""
    out = [pts[0].copy()]
    acc = 0.0
    for a, b in zip(pts[:-1], pts[1:]):
        seg = (b - a).length
        pos = 0.0
        while acc + seg - pos >= step:
            pos += step - acc
            acc = 0.0
            out.append(a.lerp(b, pos / seg))
        acc += seg - pos
    if (out[-1] - pts[-1]).length > step * 0.3:
        out.append(pts[-1].copy())
    return out


def frames(pts):
    """Parallel-transport frames (tangent, normal, binormal) along a polyline."""
    ts = []
    for i in range(len(pts)):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, len(pts) - 1)]
        ts.append((b - a).normalized())
    n = ts[0].cross(Vector((1.0, 0.0, 0.0)) if abs(ts[0].x) < 0.9 else Vector((0.0, 1.0, 0.0))).normalized()
    out = []
    for i, t in enumerate(ts):
        if i > 0:
            n = n - t * n.dot(t)
            n.normalize()
        out.append((t, n, t.cross(n)))
    return out


def tube(B, pts, radius_fn, sides, slot, col_fn, v_scale=1.0, cap_end=True, wobble=0.0):
    """A tube along pts: radius_fn(i, s, angle) (s = 0..1 along) and col_fn(i, s, angle) -> RGBA. Returns rings."""
    fr = frames(pts)
    length = sum((b - a).length for a, b in zip(pts[:-1], pts[1:]))
    rings = []
    acc = 0.0
    for i, (p, (t, n, b)) in enumerate(zip(pts, fr)):
        if i > 0:
            acc += (p - pts[i - 1]).length
        s = min(acc / max(length, 1e-6), 1.0)
        ring = []
        for k in range(sides):
            a = 2 * math.pi * k / sides + wobble * math.sin(i * 0.7)
            r = radius_fn(i, s, a)
            ring.append(B.bm.verts.new(p + (n * math.cos(a) + b * math.sin(a)) * r))
        rings.append((ring, s, acc))
    for i in range(len(rings) - 1):
        ra, sa, la = rings[i]
        rb, sb_, lb = rings[i + 1]
        for k in range(sides):
            k2 = (k + 1) % sides
            u0, u1 = k / sides, (k + 1) / sides
            a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * k2 / sides
            B.face([ra[k], ra[k2], rb[k2], rb[k]],
                   [(u0, la * v_scale), (u1, la * v_scale), (u1, lb * v_scale), (u0, lb * v_scale)],
                   [col_fn(i, sa, a0), col_fn(i, sa, a1), col_fn(i + 1, sb_, a1), col_fn(i + 1, sb_, a0)], slot)
    if cap_end:
        ring, s, l = rings[-1]
        c = B.bm.verts.new(pts[-1] + fr[-1][0] * radius_fn(len(pts) - 1, 1.0, 0.0) * 0.3)
        for k in range(sides):
            k2 = (k + 1) % sides
            B.face([ring[k], ring[k2], c], [(0, 0), (1, 0), (0.5, 1)], [col_fn(len(pts) - 1, 1.0, 0.0)] * 3, slot)
    return rings


def strip(B, centre, across, widths, slot, cols, uv_v=None):
    """A leaf strip: centre points, an across vector per point, half widths; two-sided material. cols per row."""
    rows = []
    n = len(centre)
    for i, (c, a, w) in enumerate(zip(centre, across, widths)):
        if w <= 1e-4:
            rows.append([B.bm.verts.new(c)])
        else:
            rows.append([B.bm.verts.new(c - a * w), B.bm.verts.new(c + a * w)])
    for i in range(n - 1):
        v0 = (uv_v[i] if uv_v else i / (n - 1))
        v1 = (uv_v[i + 1] if uv_v else (i + 1) / (n - 1))
        r0, r1 = rows[i], rows[i + 1]
        if len(r0) == 2 and len(r1) == 2:
            B.face([r0[0], r0[1], r1[1], r1[0]], [(0, v0), (1, v0), (1, v1), (0, v1)], [cols[i], cols[i], cols[i + 1], cols[i + 1]], slot)
        elif len(r0) == 2:
            B.face([r0[0], r0[1], r1[0]], [(0, v0), (1, v0), (0.5, v1)], [cols[i], cols[i], cols[i + 1]], slot)
        elif len(r1) == 2:
            B.face([r0[0], r1[1], r1[0]], [(0.5, v0), (1, v1), (0, v1)], [cols[i], cols[i + 1], cols[i + 1]], slot)


def leaf(B, base, normal, along, length, width, slot, tone=(0.3, 0.9), dry=0.0, rnd=None, sway=(0.3, 1.0), droop=0.0, fold=0.0):
    """A single broad leaf (pointed oval) of 5 rows; `along` its direction, `normal` its face; droop bends its tip down."""
    along = along.normalized()
    across = normal.cross(along).normalized()
    rnd = B.rng.random() if rnd is None else rnd
    centre, acr, ws, cols = [], [], [], []
    for k, t in enumerate((0.0, 0.2, 0.45, 0.72, 1.0)):
        p = base + along * (length * t) - UP * (droop * length * t * t) - normal * (fold * width * (1.0 - abs(0.5 - t) * 2))
        centre.append(p)
        acr.append(across)
        ws.append(width * 0.5 * (0.0 if t in (0.0, 1.0) else math.sin(math.pi * t ** 0.85)) + (0.012 if t == 0.0 else 0.0))
        cols.append((tone[0] + (tone[1] - tone[0]) * t, dry, rnd, sway[0] + (sway[1] - sway[0]) * t))
    strip(B, centre, acr, ws, slot, cols)


# --- palms --------------------------------------------------------------------------------------------------------
def palm(name, height, lean, n_fronds, seed, frond_len, young=False):
    B = Builder(name, ["MI_Bark_Palm", "MI_Leaf_Palm"], seed)
    rng = B.rng
    top = Vector((lean, rng.uniform(-0.2, 0.2), height))
    # A trunk that rises, bends over and lifts its head again (the classic beach palm).
    curve = bezier(Vector((0, 0, 0)), Vector((lean * 0.05, 0, height * 0.4)), Vector((lean * 0.75, top.y * 0.5, height * 0.72)),
                   top, 60)
    pts = resample(curve, 0.07)
    r_base, r_top = (0.2, 0.13) if young else (0.25, 0.155)
    ring_len = 0.2 if not young else 0.15
    ring_rnd = [rng.random() for _ in range(400)]

    def radius(i, s, a):
        acc = s * total
        u = (acc / ring_len) % 1.0
        r = r_top + (r_base - r_top) * (1.0 - s) ** 1.6 + 0.14 * math.exp(-acc / 0.45) * (1.0 + 0.25 * math.sin(3 * a))
        return r * (0.95 + 0.09 * u ** 1.6) * (1.0 + 0.03 * math.sin(2 * a + ring_rnd[int(acc / ring_len) % 400] * 6))

    def colour(i, s, a):
        acc = s * total
        u = (acc / ring_len) % 1.0
        k = int(acc / ring_len) % 400
        scar = 1.0 - math.exp(-(u / 0.12) ** 2) - math.exp(-((1.0 - u) / 0.06) ** 2)
        base_dark = 0.6 + 0.4 * min(acc / 1.2, 1.0)
        return (max(0.1, 0.35 + 0.5 * scar) * base_dark, 0.0, ring_rnd[k], 0.0)

    total = sum((b - a).length for a, b in zip(pts[:-1], pts[1:]))
    tube(B, pts, radius, 11, "MI_Bark_Palm", colour, cap_end=False)
    # Collision: three hulls along the trunk.
    third = len(pts) // 3
    for k in range(3):
        seg = pts[k * third: (k + 1) * third + 1]
        hull = []
        for p in seg[::4] + [seg[-1]]:
            for a in range(8):
                ang = 2 * math.pi * a / 8
                hull.append(p + Vector((math.cos(ang), math.sin(ang), 0.0)) * r_base * 0.85)
        B.ucx.append(hull)
    # The crown shaft and the fronds' bases.
    t_dir = (pts[-1] - pts[-4]).normalized()
    shaft = [pts[-1] + t_dir * (0.08 * k) for k in range(7)]
    tube(B, shaft, lambda i, s, a: r_top * (1.25 - 0.9 * s ** 2), 10, "MI_Leaf_Palm",
         lambda i, s, a: (0.35 + 0.3 * s, 0.35, 0.5, 0.0))
    crown = shaft[-3]
    # Coconuts under the crown.
    for k in range(rng.randint(3, 7)):
        a = rng.uniform(0, 2 * math.pi)
        c = crown - t_dir * rng.uniform(0.05, 0.2) + Vector((math.cos(a), math.sin(a), 0)) * rng.uniform(0.14, 0.22)
        nut(B, c, rng.uniform(0.085, 0.11), rng)
    # Fronds: the young upright in the middle, the old spread and arching, one or two dead ones hanging.
    for k in range(n_fronds):
        age = k / max(n_fronds - 1, 1)
        az = k * 2.39996 + rng.uniform(-0.15, 0.15)
        elev = math.radians(68.0 - 88.0 * age + rng.uniform(-7, 7))
        length = frond_len * rng.uniform(0.88, 1.08) * (0.8 + 0.25 * min(age * 2.0, 1.0))
        frond(B, crown, az, elev, length, rng, droop=0.25 + 0.4 * age, dry=0.0)
    for k in range(1 if young else rng.randint(1, 2)):
        frond(B, crown - t_dir * 0.1, rng.uniform(0, 2 * math.pi), math.radians(-72 + rng.uniform(-6, 6)), frond_len * 0.75, rng,
              droop=0.05, dry=1.0, sparse=True)
    return B.finish()


def sphere_uv(d):
    """UVs round a blob (every face needs real UVs: degenerate ones break the tangents)."""
    d = d.normalized()
    return (0.5 + math.atan2(d.y, d.x) / (2 * math.pi), 0.5 + 0.5 * d.z)


def nut(B, c, r, rng):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=6, radius=r)
    rnd = rng.random()
    vmap = {}
    for v in bm.verts:
        p = Vector((v.co.x, v.co.y, v.co.z * 1.12)) + c
        vmap[v] = B.bm.verts.new(p)
    for f in bm.faces:
        vs = [vmap[v] for v in f.verts]
        cols = [(0.3 + 0.5 * max(v.co.z / r, 0.0), 0.45, rnd, 0.0) for v in f.verts]
        B.face(vs, [sphere_uv(v.co) for v in f.verts], cols, "MI_Leaf_Palm")
    bm.free()


def frond(B, start, az, elev, length, rng, droop, dry, sparse=False, leaflet_scale=1.0, pairs=24, rachis_r=0.035,
          hang=0.85, slot="MI_Leaf_Palm", width=0.042):
    """A pinnate frond: an arching rachis and leaflet strips on both sides hanging in a V below it."""
    d0 = Vector((math.cos(elev) * math.cos(az), math.cos(elev) * math.sin(az), math.sin(elev)))
    n = 18
    pts = [start + d0 * (length * i / n) - UP * (droop * length * (i / n) ** 2) for i in range(n + 1)]
    rnd = rng.random()
    tube(B, pts, lambda i, s, a: rachis_r * (1.0 - 0.7 * s), 5, slot,
         lambda i, s, a: (0.35 + 0.4 * s, dry, rnd, 0.1 + 0.8 * s), cap_end=False)
    fr = frames(pts)
    count = pairs if not sparse else pairs // 2
    for j in range(count):
        t = 0.1 + 0.87 * j / (count - 1)
        fi = t * n
        i0 = min(int(fi), n - 1)
        p = pts[i0].lerp(pts[i0 + 1], fi - i0)
        tangent = (pts[i0 + 1] - pts[i0]).normalized()
        side = tangent.cross(UP)
        if side.length < 1e-3:
            side = Vector((1.0, 0.0, 0.0))
        side.normalize()
        u = (t - 0.1) / 0.87
        ll = length * 0.3 * leaflet_scale * math.sin(math.pi * (0.12 + 0.8 * u)) * rng.uniform(0.9, 1.1)
        if sparse:
            ll *= 0.85
        for s_ in (-1.0, 1.0):
            d = (side * s_ * 0.75 + tangent * 0.45 - UP * (0.35 + hang * (0.3 + 0.7 * u))).normalized()
            if dry > 0.5:
                d = (d - UP * 0.8).normalized()
            across = tangent - d * tangent.dot(d)
            across.normalize()
            rows = 4
            centre, acr, ws, cols = [], [], [], []
            for r in range(rows + 1):
                q = r / rows
                centre.append(p + d * (ll * q) - UP * (ll * 0.28 * q * q))
                acr.append(across)
                ws.append(width * leaflet_scale * (1.0 - 0.85 * q ** 1.5) if r < rows else 0.0)
                cols.append((0.3 + 0.6 * q * (0.6 + 0.4 * u), dry, rnd, min(1.0, 0.15 + 0.55 * t + 0.35 * q)))
            strip(B, centre, acr, ws, slot, cols)


# --- broadleaf trees ----------------------------------------------------------------------------------------------
def canopy(B, c, radius, rng, slot, n_leaves, leaf_size, squash=0.65, tone_bias=0.0, flowers=0):
    """A cluster: a dark core blob and leaves over its skin, lighter on top and outside."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    rnd = rng.random()
    vmap = {}
    seeds = [rng.uniform(0, 6.28) for _ in range(3)]
    for v in bm.verts:
        d = v.co.normalized()
        bump = 1.0 + 0.12 * math.sin(3 * d.x + seeds[0]) * math.sin(3 * d.y + seeds[1]) + 0.08 * math.sin(4 * d.z + seeds[2])
        p = Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * 0.76 * bump + c
        vmap[v] = (B.bm.verts.new(p), d)
    for f in bm.faces:
        vs = [vmap[v][0] for v in f.verts]
        cols = [(0.12 + 0.3 * max(vmap[v][1].z, 0.0) + tone_bias, 0.0, rnd, 0.3) for v in f.verts]
        B.face(vs, [sphere_uv(vmap[v][1]) for v in f.verts], cols, slot)
    bm.free()
    for k in range(n_leaves):
        # Points over the skin, more on top and at the sides than underneath.
        while True:
            d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)))
            if d.length < 1e-3:
                continue
            d.normalize()
            if d.z > -0.35 or rng.random() < 0.3:
                break
        p = c + Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * rng.uniform(0.8, 1.02)
        normal = (d * 0.6 + UP * 0.5).normalized()
        along = normal.cross(Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.3, 0.3)))).normalized()
        if along.length < 1e-3:
            continue
        along = (along + d * 0.35).normalized()
        lit = 0.5 + 0.5 * d.z
        sz = leaf_size * rng.uniform(0.8, 1.2)
        aged = 0.35 if rng.random() < 0.06 else 0.0
        leaf(B, p - along * sz * 0.3, normal, along, sz, sz * 0.55, slot, tone=(0.25 + 0.3 * lit + tone_bias, 0.55 + 0.4 * lit + tone_bias),
             dry=aged, rnd=rnd, sway=(0.4, 0.9), droop=0.15, fold=0.08)
    for k in range(flowers):
        d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), abs(rng.gauss(0, 1)) * 0.8 + 0.1)).normalized()
        p = c + Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * 1.02
        flower(B, p, d, rng)


def flower(B, p, normal, rng, size=0.085):
    """A hibiscus-like flower: five round petals, cupped a little, round a pale middle."""
    normal = (normal + UP * 0.4).normalized()
    t1 = normal.cross(Vector((0.3, 0.7, 0.1))).normalized()
    t2 = normal.cross(t1)
    rnd = rng.random()
    centre = B.bm.verts.new(p + normal * 0.012)
    span = 2 * math.pi / 5
    for k in range(5):
        a0 = span * k + rng.uniform(-0.1, 0.1)
        rim = []
        for j, (f, rr, lift) in enumerate(((0.0, 0.25, 0.01), (0.2, 0.8, 0.03), (0.5, 1.0, 0.045), (0.8, 0.8, 0.03), (1.0, 0.25, 0.01))):
            a = a0 + span * f * 1.08
            rim.append(B.bm.verts.new(p + (t1 * math.cos(a) + t2 * math.sin(a)) * size * rr + normal * lift))
        for j in range(4):
            tone = 0.55 + 0.45 * (1.0 - abs(j - 1.5) / 1.5)
            B.face([centre, rim[j], rim[j + 1]], [(0.5, 0.0), (j / 4.0, 1.0), ((j + 1) / 4.0, 1.0)],
                   [(0.1, 0.0, rnd, 0.6), (tone, 0.0, rnd, 0.8), (tone, 0.0, rnd, 0.8)], "MI_Flower")


def bark_colour(rnd):
    def col(i, s, a):
        ridge = 0.5 + 0.5 * math.cos(6 * a + 1.3 * math.sin(i * 0.37))
        return (0.25 + 0.55 * ridge, 0.15 * (1.0 - s), rnd, 0.0)
    return col


def bark_radius(r0, r1, flare=0.0):
    def rad(i, s, a):
        ridge = 0.5 + 0.5 * math.cos(6 * a + 1.3 * math.sin(i * 0.37))
        return (r1 + (r0 - r1) * (1.0 - s) ** 1.3 + flare * math.exp(-s * 12.0)) * (1.0 - 0.07 * ridge)
    return rad


def tree(name, height, seed, limbs, spread, cluster_r, leaves, extra_top=1):
    B = Builder(name, ["MI_Bark_Tree", "MI_Leaf_Tree"], seed)
    rng = B.rng
    fork = Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), height * 0.42))
    trunk = resample(bezier(Vector((0, 0, -0.2)), Vector((0.15, -0.1, height * 0.15)), Vector((-0.2, 0.15, height * 0.3)), fork, 24), 0.18)
    r0 = 0.3 + 0.02 * height
    tube(B, trunk, bark_radius(r0, r0 * 0.62, flare=0.12), 12, "MI_Bark_Tree", bark_colour(rng.random()), cap_end=False)
    B.ucx.append([p + Vector((math.cos(a), math.sin(a), 0)) * r0 * 0.9 for p in (trunk[0], trunk[len(trunk) // 2], trunk[-1])
                  for a in [2 * math.pi * k / 8 for k in range(8)]])
    # Buttress roots spreading over the ground.
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-0.3, 0.3)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        root = resample(bezier(Vector((0, 0, 0.7)) + d * 0.1, Vector((0, 0, 0.3)) + d * 0.45, d * 0.9 + Vector((0, 0, 0.05)),
                               d * rng.uniform(1.3, 1.7) - Vector((0, 0, 0.2)), 10), 0.15)
        tube(B, root, lambda i, s, a_: 0.13 * (1.0 - 0.8 * s) + 0.02, 7, "MI_Bark_Tree", bark_colour(rng.random()))
    # Limbs up and out, a canopy cluster at each end; smaller clusters along them and on top.
    for k in range(limbs):
        a = 2 * math.pi * k / limbs + rng.uniform(-0.35, 0.35)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        end = fork + d * spread * rng.uniform(0.85, 1.1) + UP * height * rng.uniform(0.3, 0.4)
        limb = resample(bezier(fork - UP * 0.35, fork + UP * 0.8 + d * 0.3, end - d * spread * 0.4 - UP * 0.4, end, 16), 0.15)
        tube(B, limb, bark_radius(r0 * 0.6, r0 * 0.2), 9, "MI_Bark_Tree", bark_colour(rng.random()))
        canopy(B, end + UP * cluster_r * 0.25, cluster_r * rng.uniform(0.9, 1.15), rng, "MI_Leaf_Tree", leaves, 0.42)
        mid = limb[len(limb) * 2 // 3]
        canopy(B, mid + d * cluster_r * 0.4 + UP * cluster_r * 0.35, cluster_r * 0.75, rng, "MI_Leaf_Tree", int(leaves * 0.6), 0.38,
               tone_bias=-0.05)
    for k in range(extra_top):
        c = fork + UP * height * 0.48 + Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), 0.0))
        stem = resample([fork, fork.lerp(c, 0.5) + Vector((0.2, 0, 0)), c], 0.2)
        tube(B, stem, bark_radius(r0 * 0.45, r0 * 0.18), 8, "MI_Bark_Tree", bark_colour(rng.random()))
        canopy(B, c + UP * cluster_r * 0.2, cluster_r * 1.1, rng, "MI_Leaf_Tree", int(leaves * 1.2), 0.42, tone_bias=0.03)
    return B.finish()


# --- bushes, ferns, big leaves, grass ------------------------------------------------------------------------------
def bush_round(name, seed, flowers=0):
    slots = ["MI_Bark_Tree", "MI_Leaf_Bush"] + (["MI_Flower"] if flowers else [])
    B = Builder(name, slots, seed)
    rng = B.rng
    for k in range(6):
        a = 2 * math.pi * k / 6 + rng.uniform(-0.3, 0.3)
        r = rng.uniform(0.25, 0.5)
        c = Vector((math.cos(a) * r, math.sin(a) * r, rng.uniform(0.35, 0.75)))
        stem = resample([Vector((0, 0, 0)), c * 0.5 + Vector((0, 0, 0.1)), c], 0.12)
        tube(B, stem, lambda i, s, a_: 0.03 * (1.0 - 0.6 * s), 5, "MI_Bark_Tree", bark_colour(rng.random()))
        canopy(B, c, rng.uniform(0.38, 0.55), rng, "MI_Leaf_Bush", 170, 0.21, squash=0.8, flowers=(flowers // 6 if flowers else 0))
    return B.finish()


def bush_tufts(name, seed):
    """Thin stems with tufts of long strap leaves (a dracaena-like shrub)."""
    B = Builder(name, ["MI_Bark_Tree", "MI_Leaf_Bush"], seed)
    rng = B.rng
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-0.4, 0.4)
        lean = rng.uniform(0.1, 0.35)
        h = rng.uniform(0.7, 1.3)
        top = Vector((math.cos(a) * lean, math.sin(a) * lean, h))
        stem = resample([Vector((0, 0, 0)), Vector((math.cos(a) * lean * 0.3, math.sin(a) * lean * 0.3, h * 0.5)), top], 0.1)
        tube(B, stem, lambda i, s, a_: 0.028 * (1.0 - 0.4 * s), 6, "MI_Bark_Tree", bark_colour(rng.random()))
        rnd = rng.random()
        for j in range(16):
            az = j * 2.39996 + rng.uniform(-0.2, 0.2)
            el = math.radians(rng.uniform(-25, 70))
            d = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
            ll = rng.uniform(0.35, 0.6)
            centre, acr, ws, cols = [], [], [], []
            side = d.cross(UP).normalized() if abs(d.z) < 0.98 else Vector((1, 0, 0))
            for r in range(6):
                q = r / 5
                centre.append(top + d * ll * q - UP * ll * 0.45 * q * q)
                acr.append(side)
                ws.append(0.028 * (1.0 - q ** 2) + 0.005)
                cols.append((0.3 + 0.6 * q, 0.15 * q, rnd, 0.2 + 0.8 * q))
            strip(B, centre, acr, ws, "MI_Leaf_Bush", cols)
    return B.finish()


def fern(name, seed):
    B = Builder(name, ["MI_Leaf_Bush"], seed)
    rng = B.rng
    for k in range(14):
        az = k * 2.39996 + rng.uniform(-0.2, 0.2)
        elev = math.radians(rng.uniform(25, 60))
        frond(B, Vector((0, 0, 0.05)), az, elev, rng.uniform(0.8, 1.15), rng, droop=0.75, dry=0.0, leaflet_scale=0.55, pairs=22,
              rachis_r=0.012, hang=0.15, slot="MI_Leaf_Bush", width=0.05)
    return B.finish()


def bigleaf(name, seed):
    B = Builder(name, ["MI_Leaf_Bush"], seed)
    rng = B.rng
    for k in range(7):
        az = k * 2.39996 + rng.uniform(-0.2, 0.2)
        d = Vector((math.cos(az), math.sin(az), 0.0))
        h = rng.uniform(0.55, 1.05)
        tip = d * rng.uniform(0.25, 0.5) + UP * h
        stalk = resample(bezier(Vector((0, 0, 0)), UP * h * 0.5, tip - d * 0.1, tip, 10), 0.08)
        rnd = rng.random()
        tube(B, stalk, lambda i, s, a_: 0.022 * (1.0 - 0.4 * s), 6, "MI_Leaf_Bush", lambda i, s, a_: (0.3 + 0.3 * s, 0.05, rnd, 0.6 * s))
        # The blade: a broad heart shape hanging from the stalk's tip, cupped along its midrib, its tip drooping.
        length = rng.uniform(0.7, 1.0)
        width = length * rng.uniform(0.55, 0.7)
        along0 = (d * 0.9 + UP * 0.15).normalized()
        across = d.cross(UP).normalized()
        rows, cols_n = 9, 7
        grid = []
        for r in range(rows):
            q = r / (rows - 1)
            w = width * 0.5 * (math.sin(math.pi * min(q * 1.25, 1.0) ** 0.7) if q < 0.98 else 0.0)
            centre = tip + along0 * (length * q) - UP * (length * 0.45 * q * q)
            row = []
            for c in range(cols_n):
                v = (c / (cols_n - 1)) * 2.0 - 1.0
                cup = 0.12 * width * (1.0 - v * v) - 0.05 * width
                p = centre + across * (v * w) + UP * cup * (1.0 - q)
                row.append(B.bm.verts.new(p))
            grid.append(row)
        for r in range(rows - 1):
            for c in range(cols_n - 1):
                q0, q1 = r / (rows - 1), (r + 1) / (rows - 1)
                cc = lambda q, cc_: (0.3 + 0.6 * q * (1.0 - 0.3 * abs(cc_ / (cols_n - 1) * 2 - 1)), 0.0, rnd, 0.5 + 0.5 * q)
                B.face([grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]],
                       [(c / (cols_n - 1), q0), ((c + 1) / (cols_n - 1), q0), ((c + 1) / (cols_n - 1), q1), (c / (cols_n - 1), q1)],
                       [cc(q0, c), cc(q0, c + 1), cc(q1, c + 1), cc(q1, c)], "MI_Leaf_Bush")
    return B.finish()


def grass_tuft(name, seed, blades, h_range, straw):
    B = Builder(name, ["MI_Grass_Beach"], seed)
    rng = B.rng
    for k in range(blades):
        a = rng.uniform(0, 2 * math.pi)
        r = 0.12 * math.sqrt(rng.random())
        base = Vector((math.cos(a) * r, math.sin(a) * r, -0.04))
        out = Vector((math.cos(a), math.sin(a), 0.0))
        h = rng.uniform(*h_range)
        bend = rng.uniform(0.2, 0.55) * h
        side = out.cross(UP).normalized()
        dry = min(1.0, max(0.0, rng.gauss(straw, 0.25)))
        rnd = rng.random()
        centre, acr, ws, cols = [], [], [], []
        for s in range(6):
            t = s / 5
            centre.append(base + UP * h * t * (1.0 - 0.25 * t) + out * bend * t * t)
            acr.append(side)
            ws.append(0.018 * (1.0 - t ** 1.3) if s < 5 else 0.0)
            cols.append((0.25 + 0.7 * t, dry, rnd, t * t))
        strip(B, centre, acr, ws, "MI_Grass_Beach", cols)
    return B.finish()


BUILDERS = {
    "palm_a": lambda: palm("SM_Palm_A", 9.0, 2.2, 13, 101, 3.4),
    "palm_b": lambda: palm("SM_Palm_B", 7.0, 3.6, 12, 102, 3.1),
    "palm_c": lambda: palm("SM_Palm_C", 4.4, 0.7, 10, 103, 2.6, young=True),
    "tree_a": lambda: tree("SM_Tree_A", 9.0, 201, 3, 2.6, 1.9, 420),
    "tree_b": lambda: tree("SM_Tree_B", 6.5, 202, 2, 1.8, 1.6, 360, extra_top=2),
    "bush_a": lambda: bush_round("SM_Bush_A", 301),
    "bush_b": lambda: bush_tufts("SM_Bush_B", 302),
    "flower": lambda: bush_round("SM_Bush_Flower", 303, flowers=30),
    "fern": lambda: fern("SM_Fern_A", 401),
    "bigleaf": lambda: bigleaf("SM_BigLeaf_A", 402),
    "grass": lambda: (grass_tuft("SM_GrassTuft_A", 501, 34, (0.35, 0.65), 0.15), sb.reset_scene(),
                      grass_tuft("SM_GrassTuft_B", 502, 26, (0.4, 0.75), 0.6)),
}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_VEGETATION_OK", wanted)
