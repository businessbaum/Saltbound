"""Plants of the home island (D60, detailed in D71): palms, broadleaf trees, bushes, ferns, big-leaf plants, beach grass and
the beach creeper.

Owner (2026-10-05): "a much more interesting start island with nature, trees, palms etc." Owner (2026-10-06, D71): "the trees
have 0 detail". In the style brief (D16, D38): chunky, hand-made shapes, gently crooked, broad colour; crisp detail. Leaves
are geometry (no cut-out cards), Nanite with "preserve area". Every part is a plain mesh; the colour comes from the island's
foliage materials (setup_island_ground.py: M_SB_IslFoliage, M_SB_IslBark), which read the vertex colours as masks and the
UVs for their detail textures (gen_island_textures.py):

  R  tone along the part: 0 shaded (a leaf's base, the canopy's inside, a ring's scar) .. 1 lit (tips, the top)
  G  age: 0 fresh green .. 1 dry brown (dead fronds, straw in the grass, ripe nuts); on bark: 1 = roots / fresh wood
  B  a random value per part (tone variation)
  A  how freely it moves in the wind: 0 rigid (trunks, the root of a frond) .. 1 a leaf's tip
  UV bark: U round the stem in whole tiles of about 1 m, V along it in metres; a leaf: U across it (0.5 its midrib), V from
     its base (0) to its tip (1).

  SM_Palm_A / B / C   coconut palms: tall and gently curved (9 m), leaning far over (7 m), young (4.5 m). Ringed trunks
                      (every 20 cm a scar and a swelling, as the bark texture), a bulb and a ball of roots at the foot, the
                      crown shaft wrapped in old leaf bases, bunches of nuts, fronds of folded leaflets (older ones torn and
                      gappy), a skirt of dead fronds hanging. Lean towards +X.
  SM_Tree_A / B / C   broadleaf trees (9 m with three limbs, 6.5 m and rounder, 7.5 m leaning with four): buttress roots at
                      island_shape.TREE_ROOTS (the ground's root ridges continue them), a trunk that tapers into limbs,
                      branches and twigs, knots and broken stubs, clusters of folded leaves on stalks at the twigs' ends,
                      massed into chunky crowns.
  SM_Bush_A           a round leafy shrub (1.2 m) on branching stems; SM_Bush_B strap-leaved tufts on thin stems (1.4 m);
  SM_Bush_Flower      the round shrub with hibiscus flowers (petals, a stamen column).
  SM_Fern_A           a fern rosette (1 m), lobed pinnae, a young frond still curled; SM_BigLeaf_A paddle leaves on long
                      stalks (1.6 m), veined, some torn.
  SM_GrassTuft_A / B  beach grass, green and straw (0.5-0.7 m), folded blades, seed heads on B.
  SM_Runner_A / B     beach creeper (morning glory) running over the sand at the grass's edge: stems on the ground, two-lobed
                      leaves on stalks, a few pink trumpet flowers (D71: the grass's edge into the sand).

Pivots at the root on the ground; trunks have UCX collision (palms and trees only). Unreal coordinates in metres
(Blender Y = -Unreal Y; the shapes are symmetric enough that it does not matter, except the trees' roots, mirrored here).
Run: Tools/Build-BlenderAsset.ps1 build_vegetation [palm_a palm_b palm_c tree_a tree_b tree_c bush_a bush_b flower fern bigleaf
grass runner]
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import island_shape as S  # noqa: E402
import sb_blender as sb  # noqa: E402

UP = Vector((0.0, 0.0, 1.0))
MATS = {"MI_IslBark_Palm": (0.45, 0.36, 0.26), "MI_IslLeaf_Palm": (0.3, 0.55, 0.18), "MI_IslNut": (0.4, 0.4, 0.15),
        "MI_IslBark_Tree": (0.35, 0.27, 0.2), "MI_IslLeaf_Tree": (0.2, 0.45, 0.15), "MI_IslLeaf_Bush": (0.28, 0.52, 0.2),
        "MI_IslFlower": (0.85, 0.15, 0.2), "MI_IslGrass_Beach": (0.6, 0.62, 0.3), "MI_IslLeaf_Runner": (0.3, 0.5, 0.18),
        "MI_IslGrass_Lawn": (0.3, 0.5, 0.15)}


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
        print("SB_PLANT", self.name, "faces", len(me.polygons), "verts", len(me.vertices), "ucx", len(self.ucx))
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


def frames(pts, up=None):
    """Parallel-transport frames (tangent, normal, binormal) along a polyline; `up` (optional) sets the first normal's
    direction (angle 0 of a tube points that way)."""
    ts = []
    for i in range(len(pts)):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, len(pts) - 1)]
        ts.append((b - a).normalized())
    if up is not None and (up - ts[0] * up.dot(ts[0])).length > 1e-3:
        n = (up - ts[0] * up.dot(ts[0])).normalized()
    else:
        n = ts[0].cross(Vector((1.0, 0.0, 0.0)) if abs(ts[0].x) < 0.9 else Vector((0.0, 1.0, 0.0))).normalized()
    out = []
    for i, t in enumerate(ts):
        if i > 0:
            n = n - t * n.dot(t)
            n.normalize()
        out.append((t, n, t.cross(n)))
    return out


def tube(B, pts, radius_fn, sides, slot, col_fn, v_scale=1.0, cap_end=True, wobble=0.0, u_tiles=None, v0=0.0, up=None):
    """A tube along pts: radius_fn(i, s, angle) (s = 0..1 along) and col_fn(i, s, angle) -> RGBA. UVs: U round it in
    whole tiles (u_tiles, default about one per metre of girth), V along it in metres. `up`: angle 0 points that way
    (for shapes that are not round, e.g. a buttress root taller than wide). Returns rings."""
    fr = frames(pts, up)
    length = sum((b - a).length for a, b in zip(pts[:-1], pts[1:]))
    if u_tiles is None:
        u_tiles = max(1, round(2 * math.pi * radius_fn(0, 0.0, 0.0) / 1.0))
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
            u0, u1 = k / sides * u_tiles, (k + 1) / sides * u_tiles
            a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * k2 / sides
            B.face([ra[k], ra[k2], rb[k2], rb[k]],
                   [(u0, v0 + la * v_scale), (u1, v0 + la * v_scale), (u1, v0 + lb * v_scale), (u0, v0 + lb * v_scale)],
                   [col_fn(i, sa, a0), col_fn(i, sa, a1), col_fn(i + 1, sb_, a1), col_fn(i + 1, sb_, a0)], slot)
    if cap_end:
        ring, s, l = rings[-1]
        c = B.bm.verts.new(pts[-1] + fr[-1][0] * radius_fn(len(pts) - 1, 1.0, 0.0) * 0.3)
        for k in range(sides):
            k2 = (k + 1) % sides
            B.face([ring[k], ring[k2], c], [(k / sides, 0), ((k + 1) / sides, 0), (k / sides + 0.5 / sides, 0.08)],
                   [col_fn(len(pts) - 1, 1.0, 0.0)] * 3, slot)
    return rings


def strip(B, centre, across, widths, slot, cols, uv_v=None):
    """A flat strip: centre points, an across vector per point, half widths; two-sided material. cols per row."""
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


def fold_strip(B, centre, across, lift, widths, folds, slot, cols, edge_cols=None):
    """A folded strip (a leaf or leaflet with a midrib): per row the midrib and both edges, the edges moved along `lift`
    by fold * width (positive: the leaf cups up like a V; negative: it folds down like a roof). U 0 / 0.5 / 1 across."""
    rows = []
    n = len(centre)
    for c, a, l, w, f in zip(centre, across, lift, widths, folds):
        if w <= 1e-4:
            rows.append([B.bm.verts.new(c)])
        else:
            off = l * (f * w)
            rows.append([B.bm.verts.new(c - a * w + off), B.bm.verts.new(c), B.bm.verts.new(c + a * w + off)])
    ec = edge_cols or cols
    for i in range(n - 1):
        v0, v1 = i / (n - 1), (i + 1) / (n - 1)
        r0, r1 = rows[i], rows[i + 1]
        c0, c1, e0, e1 = cols[i], cols[i + 1], ec[i], ec[i + 1]
        if len(r0) == 3 and len(r1) == 3:
            B.face([r0[0], r0[1], r1[1], r1[0]], [(0, v0), (0.5, v0), (0.5, v1), (0, v1)], [e0, c0, c1, e1], slot)
            B.face([r0[1], r0[2], r1[2], r1[1]], [(0.5, v0), (1, v0), (1, v1), (0.5, v1)], [c0, e0, e1, c1], slot)
        elif len(r0) == 3:
            B.face([r0[0], r0[1], r1[0]], [(0, v0), (0.5, v0), (0.5, v1)], [e0, c0, c1], slot)
            B.face([r0[1], r0[2], r1[0]], [(0.5, v0), (1, v0), (0.5, v1)], [c0, e0, c1], slot)
        elif len(r1) == 3:
            B.face([r0[0], r1[1], r1[0]], [(0.5, v0), (0.5, v1), (0, v1)], [c0, c1, e1], slot)
            B.face([r0[0], r1[2], r1[1]], [(0.5, v0), (1, v1), (0.5, v1)], [c0, e1, c1], slot)


def leaf(B, base, normal, along, length, width, slot, tone=(0.3, 0.9), dry=0.0, rnd=None, sway=(0.3, 1.0), droop=0.0,
         fold=0.18, rows=6, curl=0.0, tear=0.0):
    """A broad leaf (pointed oval) folded along its midrib; `along` its direction, `normal` its face; droop bends its tip
    down, curl rolls the tip back up; tear (0..1) bites a notch out of one side."""
    along = along.normalized()
    across = normal.cross(along).normalized()
    up = across.cross(along).normalized()
    rnd = B.rng.random() if rnd is None else rnd
    side_tear = B.rng.choice((-1, 1))
    centre, acr, lift, ws, fs, cols, ecols = [], [], [], [], [], [], []
    for k in range(rows + 1):
        t = k / rows
        bend = droop * length * t * t - curl * length * max(t - 0.7, 0.0) ** 2 * 3.0
        p = base + along * (length * t) - UP * bend
        centre.append(p)
        acr.append(across)
        lift.append(up)
        w = width * 0.5 * (0.0 if k in (0, rows) else math.sin(math.pi * t ** 0.85))
        if tear > 0 and 0.35 < t < 0.7:
            w *= 1.0 - tear * 0.7
        ws.append(w + (0.01 if k == 0 else 0.0))
        fs.append(fold)
        tn = tone[0] + (tone[1] - tone[0]) * t
        cols.append((tn, dry, rnd, sway[0] + (sway[1] - sway[0]) * t))
        ecols.append((tn * 0.92, min(1.0, dry + 0.08), rnd, min(1.0, sway[0] + (sway[1] - sway[0]) * t + 0.1)))
    fold_strip(B, centre, acr, lift, ws, fs, slot, cols, ecols)


def nut(B, c, r, rng, age, slot):
    """A coconut: a slightly three-sided egg, its tip down-out, on a short stalk."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=8, radius=r)
    rnd = rng.random()
    vmap = {}
    tw = rng.uniform(0, 6.28)
    for v in bm.verts:
        a = math.atan2(v.co.y, v.co.x)
        tri = 1.0 + 0.07 * math.cos(3 * a + tw) * (1.0 - abs(v.co.z / r))
        p = Vector((v.co.x * tri, v.co.y * tri, v.co.z * 1.15 - (0.12 * r if v.co.z < -0.6 * r else 0.0))) + c
        vmap[v] = B.bm.verts.new(p)
    for f in bm.faces:
        vs = [vmap[v] for v in f.verts]
        cols = [(0.35 + 0.5 * max(v.co.z / r, 0.0), age, rnd, 0.0) for v in f.verts]
        B.face(vs, [sphere_uv(v.co) for v in f.verts], cols, slot)
    bm.free()


def sphere_uv(d):
    """UVs round a blob (every face needs real UVs: degenerate ones break the tangents)."""
    d = d.normalized()
    return (0.5 + math.atan2(d.y, d.x) / (2 * math.pi), 0.5 + 0.5 * d.z)


# --- palms --------------------------------------------------------------------------------------------------------
def palm(name, height, lean, n_fronds, seed, frond_len, young=False):
    B = Builder(name, ["MI_IslBark_Palm", "MI_IslLeaf_Palm", "MI_IslNut"], seed)
    rng = B.rng
    top = Vector((lean, rng.uniform(-0.2, 0.2), height))
    # A trunk that rises, bends over and lifts its head again (the classic beach palm).
    curve = bezier(Vector((0, 0, -0.15)), Vector((lean * 0.05, 0, height * 0.4)), Vector((lean * 0.75, top.y * 0.5, height * 0.72)),
                   top, 80)
    pts = resample(curve, 0.04)
    r_base, r_top = (0.19, 0.13) if young else (0.24, 0.15)
    ring_len = 0.2
    ring_rnd = [rng.random() for _ in range(400)]
    total = sum((b - a).length for a, b in zip(pts[:-1], pts[1:]))

    def radius(i, s, a):
        acc = s * total
        u = ((acc + 0.15) / ring_len) % 1.0
        k = int((acc + 0.15) / ring_len) % 400
        bulb = 0.16 * math.exp(-max(acc, 0.0) / 0.45) * (1.0 + 0.2 * math.sin(3 * a + 1.0))
        r = r_top + (r_base - r_top) * (1.0 - s) ** 1.6 + bulb
        ring = 0.955 + 0.075 * u ** 1.8 - 0.035 * math.exp(-((1.0 - u) / 0.05) ** 2)
        return r * ring * (1.0 + 0.025 * math.sin(2 * a + ring_rnd[k] * 6))

    def colour(i, s, a):
        acc = s * total
        u = ((acc + 0.15) / ring_len) % 1.0
        k = int((acc + 0.15) / ring_len) % 400
        scar = 1.0 - math.exp(-(u / 0.1) ** 2) - math.exp(-((1.0 - u) / 0.05) ** 2)
        base_dark = 0.55 + 0.45 * min(max(acc, 0.0) / 1.5, 1.0)
        return (max(0.12, 0.4 + 0.45 * scar) * base_dark, 0.0, ring_rnd[k], 0.0)

    tube(B, pts, radius, 16, "MI_IslBark_Palm", colour, cap_end=False, u_tiles=1, v0=0.15)
    third = len(pts) // 3
    for k in range(3):
        seg = pts[k * third: (k + 1) * third + 1]
        hull = []
        for p in seg[::6] + [seg[-1]]:
            for a in range(8):
                ang = 2 * math.pi * a / 8
                hull.append(p + Vector((math.cos(ang), math.sin(ang), 0.0)) * r_base * 0.85)
        B.ucx.append(hull)
    # The ball of roots at the foot: many thin roots from the bulb into the sand, orange-brown (G = 1 on bark).
    for k in range(26 if young else 44):
        a = rng.uniform(0, 2 * math.pi)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        z0 = rng.uniform(0.0, 0.2)
        out = rng.uniform(0.1, 0.3)
        r0 = r_base * (1.3 + 0.4 * math.exp(-z0 / 0.2))
        root = resample(bezier(d * r0 * 0.75 + UP * z0, d * (r0 + out * 0.35) + UP * (z0 * 0.6),
                               d * (r0 + out * 0.75) + UP * 0.0, d * (r0 + out) - UP * 0.1, 6), 0.04)
        rr = rng.uniform(0.016, 0.028)
        rnd = rng.random()
        tube(B, root, lambda i, s, a_, rr=rr: rr * (1.0 - 0.5 * s), 5, "MI_IslBark_Palm", lambda i, s, a_, rnd=rnd: (0.35 + 0.3 * s, 1.0, rnd, 0.0),
             u_tiles=1)
    # The crown shaft wrapped in old leaf bases (boots), then the fronds' bases.
    t_dir = (pts[-1] - pts[-4]).normalized()
    side = t_dir.cross(Vector((0.0, 1.0, 0.0))).normalized()
    if side.length < 1e-3:
        side = Vector((1.0, 0.0, 0.0))
    shaft = [pts[-1] + t_dir * (0.07 * k) for k in range(8)]
    tube(B, shaft, lambda i, s, a: r_top * (1.22 - 0.85 * s ** 2), 14, "MI_IslLeaf_Palm",
         lambda i, s, a: (0.35 + 0.3 * s, 0.4, 0.5, 0.0), u_tiles=1)
    for k in range(rng.randint(5, 8)):
        a0 = rng.uniform(0, 2 * math.pi)
        h0 = rng.uniform(-0.45, 0.05)
        boot_h = rng.uniform(0.22, 0.38)
        wide = rng.uniform(0.9, 1.4)
        rnd = rng.random()
        rows = []
        for j in range(5):
            q = j / 4
            ring = []
            for m in range(5):
                ang = a0 + (m / 4 - 0.5) * wide
                rr = r_top * (1.18 + 0.12 * q) + 0.012
                b2 = t_dir.cross(side)
                p = pts[-1] + t_dir * (h0 + boot_h * q) + (side * math.cos(ang) + b2 * math.sin(ang)) * rr * (1.0 + 0.25 * q * q)
                ring.append(B.bm.verts.new(p))
            rows.append(ring)
        for j in range(4):
            for m in range(4):
                c0 = (0.3 + 0.2 * j / 4, 0.85, rnd, 0.0)
                c1 = (0.3 + 0.2 * (j + 1) / 4, 0.85, rnd, 0.0)
                B.face([rows[j][m], rows[j][m + 1], rows[j + 1][m + 1], rows[j + 1][m]],
                       [(m / 4, j / 4), ((m + 1) / 4, j / 4), ((m + 1) / 4, (j + 1) / 4), (m / 4, (j + 1) / 4)], [c0, c0, c1, c1], "MI_IslLeaf_Palm")
    crown = shaft[-3]
    # Bunches of nuts under the crown, each on a hanging stalk.
    for k in range(rng.randint(2, 3) if not young else 1):
        a = rng.uniform(0, 2 * math.pi)
        d = (side * math.cos(a) + t_dir.cross(side) * math.sin(a)).normalized()
        st0 = crown - t_dir * 0.12 + d * r_top
        st1 = st0 + d * 0.25 - UP * 0.18
        stalk = resample(bezier(st0, st0 + d * 0.15, st1 + UP * 0.05, st1, 6), 0.04)
        tube(B, stalk, lambda i, s, a_: 0.02 * (1.0 - 0.4 * s), 5, "MI_IslNut", lambda i, s, a_: (0.4, 0.6, 0.5, 0.0), u_tiles=1)
        age = rng.choice((0.0, 0.15, 0.35, 0.7))
        for j in range(rng.randint(3, 6)):
            off = Vector((rng.uniform(-0.14, 0.14), rng.uniform(-0.14, 0.14), rng.uniform(-0.2, 0.0)))
            nut(B, st1 + off, rng.uniform(0.09, 0.12), rng, min(1.0, age + rng.uniform(-0.05, 0.15)), "MI_IslNut")
    # Fronds: the young upright in the middle, the old spread, arching and torn, then the dead skirt hanging.
    for k in range(n_fronds):
        age = k / max(n_fronds - 1, 1)
        az = k * 2.39996 + rng.uniform(-0.15, 0.15)
        elev = math.radians(68.0 - 88.0 * age + rng.uniform(-7, 7))
        length = frond_len * rng.uniform(0.88, 1.08) * (0.8 + 0.25 * min(age * 2.0, 1.0))
        frond(B, crown, az, elev, length, rng, droop=0.25 + 0.4 * age, dry=0.08 * max(age - 0.6, 0.0) / 0.4, torn=max(age - 0.45, 0.0) * 0.5,
              slot="MI_IslLeaf_Palm")
    for k in range(1 if young else rng.randint(2, 4)):
        frond(B, crown - t_dir * (0.08 + 0.05 * k), rng.uniform(0, 2 * math.pi), math.radians(-70 + rng.uniform(-10, 8)),
              frond_len * rng.uniform(0.6, 0.8), rng, droop=0.05, dry=1.0, sparse=True, torn=0.5, slot="MI_IslLeaf_Palm")
    return B.finish()


def frond(B, start, az, elev, length, rng, droop, dry, sparse=False, leaflet_scale=1.0, pairs=34, rachis_r=0.035,
          hang=0.85, slot="MI_IslLeaf_Palm", width=0.042, torn=0.0, fold=-0.35, lobes=0, curl_tip=0.0):
    """A pinnate frond: an arching rachis and folded leaflets on both sides hanging in a V below it. torn (0..1): some
    leaflets broken short or missing (old fronds). lobes > 0: each pinna is a lobed, toothed leaflet (ferns). curl_tip:
    the rachis's end rolls up (a young fern frond)."""
    d0 = Vector((math.cos(elev) * math.cos(az), math.cos(elev) * math.sin(az), math.sin(elev)))
    n = 22
    pts = []
    for i in range(n + 1):
        t = i / n
        p = start + d0 * (length * t) - UP * (droop * length * t * t)
        if curl_tip > 0 and t > 0.6:
            q = (t - 0.6) / 0.4
            p = p + UP * (curl_tip * length * q * q * 0.6) - d0 * (curl_tip * length * q ** 3 * 0.35)
        pts.append(p)
    rnd = rng.random()
    tube(B, pts, lambda i, s, a: rachis_r * (1.25 - 0.95 * s), 6, slot,
         lambda i, s, a: (0.35 + 0.4 * s, dry, rnd, 0.1 + 0.8 * s), cap_end=False, u_tiles=1)
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
        ll = length * 0.3 * leaflet_scale * math.sin(math.pi * (0.12 + 0.8 * u)) * rng.uniform(0.88, 1.1)
        if sparse:
            ll *= 0.85
        for s_ in (-1.0, 1.0):
            if torn > 0 and rng.random() < torn * 0.35:
                continue                                         # missing: torn off by the wind
            l2 = ll * (rng.uniform(0.3, 0.7) if torn > 0 and rng.random() < torn * 0.5 else 1.0)
            # Leaflets alternate a little up and down the rachis (the V of a coconut frond).
            vv = 0.35 + hang * (0.3 + 0.7 * u) + (0.15 if (j % 2) else -0.05)
            d = (side * s_ * 0.75 + tangent * 0.45 - UP * vv).normalized()
            if dry > 0.5:
                d = (d - UP * 0.8).normalized()
            across = tangent - d * tangent.dot(d)
            across.normalize()
            lift = d.cross(across).normalized()
            if lift.z < 0:
                lift = -lift
            rows = 5
            centre, acr, lf, ws, fs, cols, ecols = [], [], [], [], [], [], []
            for r in range(rows + 1):
                q = r / rows
                centre.append(p + d * (l2 * q) - UP * (l2 * 0.28 * q * q))
                acr.append(across)
                lf.append(lift)
                if lobes:
                    w = width * leaflet_scale * (1.0 - 0.75 * q ** 1.5) * (0.75 + 0.25 * abs(math.sin(q * math.pi * lobes)))
                else:
                    w = width * leaflet_scale * (1.0 - 0.85 * q ** 1.5)
                ws.append(w if r < rows else 0.0)
                fs.append(fold)
                tip_dry = 0.0 if dry > 0.5 else max(0.0, q - 0.85) * 2.0 * (0.5 + torn)
                tone = 0.3 + 0.6 * q * (0.6 + 0.4 * u)
                cols.append((tone, min(1.0, dry + tip_dry), rnd, min(1.0, 0.15 + 0.55 * t + 0.35 * q)))
                ecols.append((tone * 0.9, min(1.0, dry + tip_dry + 0.05), rnd, min(1.0, 0.2 + 0.55 * t + 0.35 * q)))
            fold_strip(B, centre, acr, lf, ws, fs, slot, cols, ecols)


# --- broadleaf trees ----------------------------------------------------------------------------------------------
def bark_colour(rnd, wood=0.0):
    def col(i, s, a):
        ridge = 0.5 + 0.5 * math.cos(7 * a + 1.3 * math.sin(i * 0.37))
        return (0.3 + 0.5 * ridge, wood, rnd, 0.0)
    return col


def taper(r0, r1, flare=0.0, knots=()):
    """Radius along a stem: r0 at its foot to r1 at its end, a flare at the foot, knots (s, angle, size) as bumps."""
    def rad(i, s, a):
        ridge = 0.5 + 0.5 * math.cos(7 * a + 1.3 * math.sin(i * 0.37))
        r = (r1 + (r0 - r1) * (1.0 - s) ** 1.3 + flare * math.exp(-s * 10.0)) * (1.0 - 0.05 * ridge)
        for ks, ka, kz in knots:
            da = (a - ka + math.pi) % (2 * math.pi) - math.pi
            r += kz * math.exp(-((s - ks) / 0.035) ** 2 - (da / 0.45) ** 2)
        return r
    return rad


def bend_path(start, direction, length, rng, step=0.12, up_pull=0.0, droop=0.0, wiggle=0.08):
    """A gently crooked stem: from start along direction, curving towards the sky (up_pull) or drooping."""
    pts = [start.copy()]
    d = direction.normalized()
    n = max(3, int(length / step))
    seg = length / n
    for k in range(n):
        d = (d + UP * (up_pull / n) - UP * (droop / n) + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))) * wiggle / math.sqrt(n)).normalized()
        pts.append(pts[-1] + d * seg)
    return pts


def leaf_cluster(B, tip, out, rng, slot, size, count, tone_bias=0.0, rnd=None, aged=0.06):
    """Leaves on short stalks round a twig's tip, facing up and out."""
    rnd = rng.random() if rnd is None else rnd
    out = out.normalized()
    side = out.cross(UP)
    if side.length < 1e-3:
        side = Vector((1.0, 0.0, 0.0))
    side.normalize()
    up2 = side.cross(out).normalized()
    for k in range(count):
        a = k * 2.39996 + rng.uniform(-0.3, 0.3)
        # Leaves lie along the clump's skin round the twig's tip (shingled), pointing away from it, their faces out of the
        # clump and up: lit by a high sun and seen from the side (leaves pointing up have their faces sideways, in shade).
        d = ((side * math.cos(a) + up2 * math.sin(a)) + out * rng.uniform(0.1, 0.35)).normalized()
        stalk = size * rng.uniform(0.12, 0.25)
        base = tip + d * stalk
        normal = (out * 0.6 + UP * 0.6 + Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(-0.15, 0.15)))).normalized()
        normal = normal - d * normal.dot(d)                      # the leaf's face across its own direction
        if normal.length < 1e-3:
            normal = side.copy()
        normal.normalize()
        sz = size * rng.uniform(0.75, 1.2)
        lit = 0.5 + 0.5 * max(min(d.z + 0.3, 1.0), -1.0)
        dry = 0.4 if rng.random() < aged else 0.0
        leaf(B, base, normal, d, sz, sz * rng.uniform(0.42, 0.55), slot,
             tone=(0.27 + 0.3 * lit + tone_bias, 0.6 + 0.38 * lit + tone_bias), dry=dry, rnd=rnd, sway=(0.45, 0.95),
             droop=rng.uniform(0.1, 0.3), fold=rng.uniform(0.1, 0.25), rows=5, curl=rng.uniform(0.0, 0.15))


def crown_clump(B, base, centre, radius, rng, slot, leaf_size, groups, squash=0.62, tone_bias=0.0, flowers=0, core=0.62,
                bark_slot="MI_IslBark_Tree", twig_r=0.03):
    """A clump of the crown, chunky and readable from afar (D60's massing) but made of real parts: a few branches from
    `base` into it, then groups of leaves on short twigs over its skin (more on top and at the sides), each group 3-6
    folded leaves on stalks facing out; a darker core keeps the inside from being see-through."""
    if core > 0:
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
        rnd = rng.random()
        seeds = [rng.uniform(0, 6.28) for _ in range(3)]
        vmap = {}
        for v in bm.verts:
            d = v.co.normalized()
            bump = 1.0 + 0.12 * math.sin(3 * d.x + seeds[0]) * math.sin(3 * d.y + seeds[1]) + 0.08 * math.sin(4 * d.z + seeds[2])
            p = Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * core * bump + centre
            vmap[v] = (B.bm.verts.new(p), d)
        for f in bm.faces:
            vs = [vmap[v][0] for v in f.verts]
            B.face(vs, [sphere_uv(vmap[v][1]) for v in f.verts], [(0.12 + 0.22 * max(vmap[v][1].z, 0.0), 0.0, rnd, 0.3) for v in f.verts], slot)
        bm.free()
    # Branches from the base into the clump (seen from below, between the leaves).
    for k in range(rng.randint(3, 5)):
        a = 2 * math.pi * k / 4 + rng.uniform(-0.4, 0.4)
        tgt = centre + Vector((math.cos(a) * radius * 0.55, math.sin(a) * radius * 0.55, radius * squash * rng.uniform(0.0, 0.35)))
        br = resample([base, base.lerp(tgt, 0.5) + UP * 0.12, tgt], 0.1)
        tube(B, br, taper(twig_r * 2.2, twig_r * 0.9), 6, bark_slot, bark_colour(rng.random()), cap_end=True, u_tiles=1)
    for k in range(groups):
        while True:
            d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)))
            if d.length > 1e-3:
                d.normalize()
                if d.z > -0.2 or rng.random() < 0.25:
                    break
        skin = centre + Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * rng.uniform(0.88, 1.03)
        inner = centre + Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * rng.uniform(0.6, 0.72)
        twig = resample([inner, inner.lerp(skin, 0.5) + UP * 0.04, skin], 0.08)
        rnd = rng.random()
        tube(B, twig, taper(twig_r, twig_r * 0.45), 4, bark_slot, bark_colour(rnd), cap_end=True, u_tiles=1)
        lit = 0.5 + 0.5 * d.z
        out = (d + UP * 0.35).normalized()
        leaf_cluster(B, skin, out, rng, slot, leaf_size, rng.randint(4, 7), tone_bias=tone_bias + 0.12 * (lit - 0.5), rnd=rnd)
    for k in range(flowers):
        d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), abs(rng.gauss(0, 1)) * 0.8 + 0.1)).normalized()
        p = centre + Vector((d.x * radius, d.y * radius, d.z * radius * squash)) * 1.03
        flower(B, p, d, rng)


def flower(B, p, normal, rng, size=0.085):
    """A hibiscus flower: five round petals, cupped, round a pale middle, with a stamen column and its yellow tip."""
    normal = (normal + UP * 0.4).normalized()
    t1 = normal.cross(Vector((0.3, 0.7, 0.1))).normalized()
    t2 = normal.cross(t1)
    rnd = rng.random()
    centre = B.bm.verts.new(p + normal * 0.012)
    span = 2 * math.pi / 5
    for k in range(5):
        a0 = span * k + rng.uniform(-0.1, 0.1)
        rim = []
        for j, (f, rr, lift) in enumerate(((0.0, 0.25, 0.01), (0.2, 0.8, 0.035), (0.5, 1.0, 0.05), (0.8, 0.8, 0.035), (1.0, 0.25, 0.01))):
            a = a0 + span * f * 1.08
            rim.append(B.bm.verts.new(p + (t1 * math.cos(a) + t2 * math.sin(a)) * size * rr + normal * lift))
        for j in range(4):
            tone = 0.55 + 0.45 * (1.0 - abs(j - 1.5) / 1.5)
            B.face([centre, rim[j], rim[j + 1]], [(0.5, 0.0), (j / 4.0, 1.0), ((j + 1) / 4.0, 1.0)],
                   [(0.1, 0.0, rnd, 0.6), (tone, 0.0, rnd, 0.8), (tone, 0.0, rnd, 0.8)], "MI_IslFlower")
    col = [p + normal * 0.01, p + normal * 0.05 + t1 * 0.01, p + normal * 0.085 + t1 * 0.02]
    tube(B, resample(col, 0.02), lambda i, s, a: 0.005, 4, "MI_IslFlower", lambda i, s, a: (0.95, 1.0, 0.9, 0.8), u_tiles=1)


def tree(name, height, seed, limbs, spread, cluster_r, density, leaf_size, extra_top=1, lean=0.0):
    B = Builder(name, ["MI_IslBark_Tree", "MI_IslLeaf_Tree"], seed)
    rng = B.rng
    lean_v = Vector((lean, rng.uniform(-0.2, 0.2) * lean, 0.0))
    fork = Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), height * 0.42)) + lean_v
    trunk = resample(bezier(Vector((0, 0, -0.25)), Vector((0.15, -0.1, height * 0.15)) + lean_v * 0.2,
                            Vector((-0.2, 0.15, height * 0.3)) + lean_v * 0.6, fork + UP * 0.1, 30), 0.1)
    r0 = 0.28 + 0.02 * height
    knots = [(rng.uniform(0.25, 0.8), rng.uniform(0, 6.28), rng.uniform(0.03, 0.06)) for _ in range(rng.randint(2, 4))]

    def trunk_r(i, s, a, base=taper(r0, r0 * 0.62, flare=0.16, knots=knots)):
        return base(i, s, a) * (1.0 - 0.3 * max(s - 0.9, 0.0) / 0.1)        # the top tapers into the limbs (capped)
    tube(B, trunk, trunk_r, 16, "MI_IslBark_Tree", bark_colour(rng.random()), cap_end=True)
    B.ucx.append([p + Vector((math.cos(a), math.sin(a), 0)) * r0 * 0.9 for p in (trunk[0], trunk[len(trunk) // 2], trunk[-1])
                  for a in [2 * math.pi * k / 8 for k in range(8)]])
    # Buttress roots at the island's bearings (Unreal frame; Blender Y = -Unreal Y): flat-topped fins that sink away.
    for bearing, length in S.TREE_ROOTS.get(name, S.TREE_ROOTS["SM_Tree_A"]):
        a = math.radians(bearing)
        d = Vector((math.cos(a), -math.sin(a), 0.0))
        L = length * rng.uniform(0.95, 1.1)
        root = resample(bezier(Vector((0, 0, 1.0)) + d * 0.02, Vector((0, 0, 0.45)) + d * (r0 * 0.9),
                               d * (L * 0.55) + Vector((0, 0, 0.05)), d * L - Vector((0, 0, 0.22)), 14), 0.08)
        rnd = rng.random()

        def root_r(i, s, a_, L=L):
            fin = 1.0 + 0.9 * abs(math.cos(a_)) * (1.0 - s) ** 0.7  # taller than wide near the trunk (a buttress)
            grow = min(1.0, 0.35 + s / 0.18)                         # thin inside the trunk, full where it comes out
            return (0.13 * (1.0 - 0.8 * s) + 0.025) * fin * grow
        tube(B, root, root_r, 10, "MI_IslBark_Tree", bark_colour(rnd), u_tiles=1, up=UP)
    # Limbs up and out; each ends in a clump of the crown, with a smaller clump along it; broken stubs here and there.
    for k in range(limbs):
        a = 2 * math.pi * k / limbs + rng.uniform(-0.35, 0.35)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        end = fork + d * spread * rng.uniform(0.85, 1.1) + UP * height * rng.uniform(0.3, 0.4) + lean_v * 0.4
        root_pt = trunk[max(len(trunk) - 10, 0)]                    # inside the trunk, 0.9 m below its top (leaning trees too)
        limb = resample(bezier(root_pt, fork + UP * 0.6 + d * 0.25, end - d * spread * 0.4 - UP * 0.4, end, 20), 0.1)
        lk = [(rng.uniform(0.3, 0.8), rng.uniform(0, 6.28), 0.025)]
        tube(B, limb, taper(r0 * 0.56, r0 * 0.2, knots=lk), 11, "MI_IslBark_Tree", bark_colour(rng.random()))
        cr = cluster_r * rng.uniform(0.9, 1.15)
        # Branches from the limb's end into the clump.
        for b in range(rng.randint(2, 3)):
            bd = (d * 0.6 + UP * 0.8 + Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0))).normalized()
            br = bend_path(end, bd, cr * rng.uniform(0.6, 0.85), rng, up_pull=0.2)
            tube(B, br, taper(r0 * 0.2, r0 * 0.07), 7, "MI_IslBark_Tree", bark_colour(rng.random()))
        crown_clump(B, end, end + UP * cr * 0.3, cr, rng, "MI_IslLeaf_Tree", leaf_size, int(density * cr * cr))
        mid = limb[len(limb) * 2 // 3]
        side_end = mid + d * cluster_r * 0.5 + UP * cluster_r * 0.45
        br = resample([mid, mid.lerp(side_end, 0.5) + UP * 0.1, side_end], 0.1)
        tube(B, br, taper(r0 * 0.18, r0 * 0.07), 7, "MI_IslBark_Tree", bark_colour(rng.random()))
        crown_clump(B, side_end, side_end + UP * cluster_r * 0.2, cluster_r * 0.75, rng, "MI_IslLeaf_Tree", leaf_size * 0.95,
                    int(density * (cluster_r * 0.75) ** 2), tone_bias=-0.05)
        if rng.random() < 0.6:                                      # a broken stub: a short branch snapped off
            sp = limb[len(limb) // 3]
            sd = (Vector((-d.y, d.x, 0.0)) * rng.choice((-1, 1)) + UP * 0.4).normalized()
            stub = resample([sp, sp + sd * rng.uniform(0.18, 0.32)], 0.05)
            rs = r0 * rng.uniform(0.14, 0.2)
            rnd = rng.random()
            tube(B, stub, lambda i, s, a_, rs=rs: rs * (1.0 - 0.15 * s) * (1.0 + 0.12 * math.sin(5 * a_) * s), 8, "MI_IslBark_Tree",
                 lambda i, s, a_, rnd=rnd: (0.4 + 0.4 * s, 0.9 * s ** 4, rnd, 0.0), cap_end=True, u_tiles=1)
    for k in range(extra_top):
        c = fork + UP * height * 0.48 + Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), 0.0)) + lean_v * 0.5
        stem = resample([fork, fork.lerp(c, 0.5) + Vector((0.2, 0, 0)), c], 0.1)
        tube(B, stem, taper(r0 * 0.45, r0 * 0.15), 9, "MI_IslBark_Tree", bark_colour(rng.random()))
        crown_clump(B, c, c + UP * cluster_r * 0.25, cluster_r * 1.1, rng, "MI_IslLeaf_Tree", leaf_size, int(density * (cluster_r * 1.1) ** 2),
                    tone_bias=0.03)
    # A broken stub on the trunk itself.
    sp = trunk[int(len(trunk) * rng.uniform(0.45, 0.7))]
    sa = rng.uniform(0, 6.28)
    sd = (Vector((math.cos(sa), math.sin(sa), 0.0)) + UP * 0.5).normalized()
    stub = resample([sp, sp + sd * (r0 * 0.9 + 0.22)], 0.05)
    rs = r0 * 0.28
    tube(B, stub, lambda i, s, a_: rs * (1.0 - 0.1 * s) * (1.0 + 0.15 * math.sin(4 * a_ + 1.0) * s ** 2), 9, "MI_IslBark_Tree",
         lambda i, s, a_: (0.35 + 0.45 * s, 0.9 * s ** 4, 0.3, 0.0), cap_end=True, u_tiles=1)
    return B.finish()


# --- bushes, ferns, big leaves, grass, the creeper ----------------------------------------------------------------
def bush_round(name, seed, flowers=0):
    slots = ["MI_IslBark_Tree", "MI_IslLeaf_Bush"] + (["MI_IslFlower"] if flowers else [])
    B = Builder(name, slots, seed)
    rng = B.rng
    for k in range(6):
        a = 2 * math.pi * k / 6 + rng.uniform(-0.3, 0.3)
        r = rng.uniform(0.25, 0.5)
        c = Vector((math.cos(a) * r, math.sin(a) * r, rng.uniform(0.35, 0.75)))
        stem = resample([Vector((0, 0, -0.05)), c * 0.5 + Vector((0, 0, 0.1)), c], 0.08)
        tube(B, stem, taper(0.032, 0.014), 6, "MI_IslBark_Tree", bark_colour(rng.random()), u_tiles=1)
        cr = rng.uniform(0.38, 0.55)
        crown_clump(B, c, c, cr, rng, "MI_IslLeaf_Bush", 0.18, int(230 * cr * cr), squash=0.8, core=0.5,
                    flowers=(flowers // 6 if flowers else 0), twig_r=0.01)
    return B.finish()


def bush_tufts(name, seed):
    """Thin stems with tufts of long strap leaves (a dracaena-like shrub); the leaves folded and drooping."""
    B = Builder(name, ["MI_IslBark_Tree", "MI_IslLeaf_Bush"], seed)
    rng = B.rng
    for k in range(5):
        a = 2 * math.pi * k / 5 + rng.uniform(-0.4, 0.4)
        lean = rng.uniform(0.1, 0.35)
        h = rng.uniform(0.7, 1.3)
        top = Vector((math.cos(a) * lean, math.sin(a) * lean, h))
        stem = resample([Vector((0, 0, -0.05)), Vector((math.cos(a) * lean * 0.3, math.sin(a) * lean * 0.3, h * 0.5)), top], 0.06)
        # Ringed stem: the old leaves' scars.
        tube(B, stem, lambda i, s, a_: 0.028 * (1.0 - 0.4 * s) * (1.0 + 0.08 * (i % 3 == 0)), 7, "MI_IslBark_Tree",
             lambda i, s, a_: (0.3 + 0.35 * (i % 3 != 0), 0.1, 0.5, 0.0), u_tiles=1)
        rnd = rng.random()
        for j in range(18):
            az = j * 2.39996 + rng.uniform(-0.2, 0.2)
            el = math.radians(rng.uniform(-25, 70))
            d = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
            ll = rng.uniform(0.35, 0.6)
            leaf(B, top, (UP - d * d.z).normalized() if abs(d.z) < 0.98 else Vector((1, 0, 0)), d, ll, rng.uniform(0.05, 0.07),
                 "MI_IslLeaf_Bush", tone=(0.3, 0.9), dry=0.5 if j > 15 and rng.random() < 0.5 else 0.0, rnd=rnd, sway=(0.2, 1.0),
                 droop=0.5, fold=0.3, rows=6)
    return B.finish()


def fern(name, seed):
    B = Builder(name, ["MI_IslLeaf_Bush"], seed)
    rng = B.rng
    for k in range(14):
        az = k * 2.39996 + rng.uniform(-0.2, 0.2)
        elev = math.radians(rng.uniform(25, 60))
        frond(B, Vector((0, 0, 0.05)), az, elev, rng.uniform(0.8, 1.15), rng, droop=0.75, dry=0.6 if k == 13 else 0.0,
              leaflet_scale=0.55, pairs=24, rachis_r=0.012, hang=0.15, slot="MI_IslLeaf_Bush", width=0.05, fold=0.2, lobes=4)
    # A young frond still curled (a fiddlehead) in the middle.
    frond(B, Vector((0, 0, 0.05)), rng.uniform(0, 6.28), math.radians(78), 0.45, rng, droop=0.05, dry=0.0, leaflet_scale=0.35,
          pairs=10, rachis_r=0.012, hang=0.05, slot="MI_IslLeaf_Bush", width=0.04, fold=0.4, curl_tip=0.5)
    return B.finish()


def bigleaf(name, seed):
    B = Builder(name, ["MI_IslLeaf_Bush"], seed)
    rng = B.rng
    for k in range(7):
        az = k * 2.39996 + rng.uniform(-0.2, 0.2)
        d = Vector((math.cos(az), math.sin(az), 0.0))
        h = rng.uniform(0.55, 1.05)
        tip = d * rng.uniform(0.25, 0.5) + UP * h
        stalk = resample(bezier(Vector((0, 0, -0.03)), UP * h * 0.5, tip - d * 0.1, tip, 10), 0.06)
        rnd = rng.random()
        tube(B, stalk, lambda i, s, a_: 0.022 * (1.0 - 0.4 * s) + 0.018 * math.exp(-s * 12.0), 7, "MI_IslLeaf_Bush",
             lambda i, s, a_: (0.3 + 0.3 * s, 0.05, rnd, 0.6 * s), u_tiles=1)
        length = rng.uniform(0.7, 1.0)
        width = length * rng.uniform(0.55, 0.7)
        along0 = (d * 0.9 + UP * 0.15).normalized()
        # A broad heart-shaped blade hanging from the stalk, cupped along its midrib, its tip drooping; some torn.
        leaf(B, tip, (UP - along0 * along0.z).normalized(), along0, length, width, "MI_IslLeaf_Bush", tone=(0.3, 0.9),
             dry=0.35 if k == 6 else 0.0, rnd=rnd, sway=(0.5, 1.0), droop=0.45, fold=0.16, rows=8,
             tear=rng.uniform(0.4, 0.9) if rng.random() < 0.35 else 0.0)
    return B.finish()


def grass_tuft(name, seed, blades, h_range, straw, seeds=0):
    B = Builder(name, ["MI_IslGrass_Beach"], seed)
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
        centre, acr, lift, ws, fs, cols = [], [], [], [], [], []
        for s in range(7):
            t = s / 6
            centre.append(base + UP * h * t * (1.0 - 0.25 * t) + out * bend * t * t)
            acr.append(side)
            tang = (UP * (1.0 - 0.5 * t) + out * (2 * bend / h) * t).normalized()
            lift.append(side.cross(tang).normalized())
            ws.append(0.016 * (1.0 - t ** 1.3) if s < 6 else 0.0)
            fs.append(0.5)
            cols.append((0.25 + 0.7 * t, min(1.0, dry + max(0.0, t - 0.8) * 1.5), rnd, t * t))
        fold_strip(B, centre, acr, lift, ws, fs, "MI_IslGrass_Beach", cols)
    for k in range(seeds):
        a = rng.uniform(0, 2 * math.pi)
        out = Vector((math.cos(a), math.sin(a), 0.0))
        h = rng.uniform(h_range[1] * 0.9, h_range[1] * 1.3)
        stalk = resample(bezier(Vector((0, 0, -0.03)), UP * h * 0.5, UP * h * 0.85 + out * 0.08, UP * h + out * 0.18, 8), 0.04)
        tube(B, stalk, lambda i, s, a_: 0.004, 4, "MI_IslGrass_Beach", lambda i, s, a_: (0.4 + 0.4 * s, 0.85, 0.3, s * s), u_tiles=1)
        for j in range(9):
            q = j / 8
            p = stalk[-1].lerp(stalk[-4], q)
            d = (out * 0.4 + UP * 0.5 + Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), 0.0))).normalized()
            leaf(B, p, d.cross(out).normalized() if d.cross(out).length > 1e-3 else UP, d, 0.035, 0.012, "MI_IslGrass_Beach",
                 tone=(0.6, 0.9), dry=0.95, rnd=0.4, sway=(0.9, 1.0), fold=0.0, rows=2)
    return B.finish()


def runner(name, seed, length, flowers):
    """Beach creeper (Ipomoea pes-caprae): stems running over the sand from a root, two-lobed leaves on stalks along them,
    folded a little, and a few pink trumpet flowers. Lies on the ground (z 0); pivot at its root."""
    B = Builder(name, ["MI_IslBark_Tree", "MI_IslLeaf_Runner", "MI_IslFlower"], seed)
    rng = B.rng
    for k in range(rng.randint(4, 6)):
        az = rng.uniform(0, 2 * math.pi) if k else 0.0
        d = Vector((math.cos(az), math.sin(az), 0.0))
        L = length * rng.uniform(0.6, 1.0)
        pts = [Vector((0, 0, 0.01))]
        for i in range(int(L / 0.08)):
            az += rng.uniform(-0.12, 0.12)
            pts.append(pts[-1] + Vector((math.cos(az), math.sin(az), 0.0)) * 0.08)
        for p in pts:
            p.z = 0.012 + 0.006 * math.sin(p.x * 7.0 + p.y * 5.0)
        rnd = rng.random()
        tube(B, pts, lambda i, s, a_: 0.007 * (1.0 - 0.4 * s), 5, "MI_IslBark_Tree", lambda i, s, a_: (0.5, 0.6, rnd, 0.0), u_tiles=1)
        for i in range(2, len(pts) - 1, 2):
            p = pts[i]
            t = (pts[i + 1] - pts[i - 1]).normalized()
            side = t.cross(UP).normalized() * rng.choice((-1, 1))
            stalk_end = p + side * rng.uniform(0.04, 0.09) + UP * rng.uniform(0.03, 0.07)
            tube(B, [p, stalk_end], lambda i_, s, a_: 0.003, 3, "MI_IslBark_Tree", lambda i_, s, a_: (0.5, 0.3, rnd, 0.2), cap_end=False, u_tiles=1)
            sz = rng.uniform(0.09, 0.14) * (1.0 - 0.4 * i / len(pts))
            d = (side * 0.6 + t * 0.4 + UP * 0.25).normalized()
            # Two lobes (the leaf is notched at its tip): two short folded leaves side by side.
            for lob in (-1, 1):
                dl = (d + t.cross(UP).normalized() * lob * 0.25).normalized()
                leaf(B, stalk_end, (UP * 0.9 + d * 0.1).normalized(), dl, sz, sz * 0.75, "MI_IslLeaf_Runner",
                     tone=(0.3, 0.85), dry=0.0, rnd=rnd, sway=(0.2, 0.5), droop=0.1, fold=0.12, rows=3)
            if flowers and rng.random() < 0.12:
                fp = p - side * 0.05 + UP * 0.05
                flower(B, fp, UP, rng, size=0.045)
    return B.finish()


def grass_patch(name, seed, clumps, h_range, blades=(4, 7), straw=0.08, seeds=0, size=1.0, slot="MI_IslGrass_Lawn",
                width=(0.0045, 0.0075)):
    """D71 (owner: "just create real visual grass small and a lot so it blends in. The grass design is important because you
    will copy it a lot"): a square metre of short lawn grass, instanced densely over the island's grass (a reusable kit).

    Clumps of thin tapered blades rise from a tight root, lean out and curve over (a little twist), darker at the root (where the
    ground's colour takes over) and lighter at the tip; now and then a straw blade or a seed stalk. Few triangles per blade
    (four segments, one strip), so tens of thousands of patches stay cheap. Vertex colours as the plants (R root .. tip, G straw,
    B one random value per clump, A how freely it moves: the tip most). UVs: a blade's U across it, V up it."""
    B = Builder(name, [slot], seed)
    rng = B.rng
    half = size * 0.5
    for c in range(clumps):
        cx, cy = rng.uniform(-half, half), rng.uniform(-half, half)
        crnd = rng.random()
        lean_dir = rng.uniform(0, 2 * math.pi)                    # a clump is combed a little one way
        n = rng.randint(*blades)
        ch = rng.uniform(*h_range)
        for k in range(n):
            a = rng.uniform(0, 2 * math.pi)
            r0 = 0.02 * math.sqrt(rng.random())
            base = Vector((cx + math.cos(a) * r0, cy + math.sin(a) * r0, -0.01))
            out_a = a * 0.6 + lean_dir * 0.4 + rng.uniform(-0.5, 0.5)
            out = Vector((math.cos(out_a), math.sin(out_a), 0.0))
            h = ch * rng.uniform(0.65, 1.15)
            lean = rng.uniform(0.15, 0.55)                        # how far over the tip goes (share of the height)
            w = rng.uniform(*width)
            twist = rng.uniform(-0.6, 0.6)
            dry = 1.0 if rng.random() < straw else min(1.0, max(0.0, rng.gauss(0.05, 0.08)))
            centre, acr, ws, cols = [], [], [], []
            side0 = out.cross(UP).normalized()
            for s in range(5):
                t = s / 4
                p = base + UP * (h * (t - 0.35 * lean * t * t)) + out * (h * lean * t * t)
                ang = twist * t
                side = side0 * math.cos(ang) + out * math.sin(ang)
                centre.append(p)
                acr.append(side)
                ws.append(w * (1.0 - t ** 1.2) if s < 4 else 0.0)
                cols.append((0.12 + 0.88 * t ** 0.8, min(1.0, dry + max(0.0, t - 0.85) * 0.6), crnd, t * t))
            strip(B, centre, acr, ws, slot, cols)
    for k in range(seeds):
        cx, cy = rng.uniform(-half, half), rng.uniform(-half, half)
        h = h_range[1] * rng.uniform(1.4, 1.9)
        bend = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0.0)).normalized() * 0.05
        stalk = resample(bezier(Vector((cx, cy, -0.01)), Vector((cx, cy, h * 0.5)), Vector((cx, cy, h * 0.85)) + bend * 0.5,
                                Vector((cx, cy, h)) + bend, 6), 0.03)
        tube(B, stalk, lambda i, s, a_: 0.0025, 3, slot, lambda i, s, a_: (0.4 + 0.5 * s, 0.7, 0.5, s * s), cap_end=False, u_tiles=1)
        tip = stalk[-1]
        for j in range(6):
            q = j / 5
            p = tip.lerp(stalk[-3], q)
            d = (bend.normalized() * 0.3 + UP * 0.6 + Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0.0))).normalized()
            normal = d.cross(Vector((1.0, 0.0, 0.0))).normalized()
            leaf(B, p, normal, d, 0.022, 0.008, slot, tone=(0.7, 1.0), dry=0.85, rnd=0.5, sway=(0.9, 1.0), fold=0.0, rows=2)
    return B.finish()


def fallen_palm(name, seed, length=11.0):
    """D71: a palm the storms threw over: its trunk lying along +X from the torn-up plate of its roots (at the origin), a
    gentle sag where it rests on the sand, the crown at the far end out over the shallows, its fronds dead and broken.
    The trunk collides in four hulls along it, so it can be walked (a bridge out over the water)."""
    B = Builder(name, ["MI_IslBark_Palm", "MI_IslLeaf_Palm"], seed)
    rng = B.rng
    n = 70
    pts = []
    for i in range(n + 1):
        t = i / n
        x = length * t
        z = 0.42 + 0.35 * math.sin(math.pi * t * 0.9) - 0.25 * t + 0.05 * math.sin(t * 9.0)
        y = 0.25 * math.sin(t * 2.2)
        pts.append(Vector((x, y, z)))
    pts = resample(pts, 0.04)
    total = sum((b - a).length for a, b in zip(pts[:-1], pts[1:]))
    r_base, r_top = 0.25, 0.15
    ring_rnd = [rng.random() for _ in range(400)]

    def radius(i, s, a):
        acc = s * total
        u = (acc / 0.2) % 1.0
        r = r_top + (r_base - r_top) * (1.0 - s) ** 1.6 + 0.15 * math.exp(-acc / 0.45)
        return r * (0.955 + 0.075 * u ** 1.8 - 0.035 * math.exp(-((1.0 - u) / 0.05) ** 2)) * (1.0 + 0.025 * math.sin(2 * a + ring_rnd[int(acc / 0.2) % 400] * 6))

    def colour(i, s, a):
        acc = s * total
        u = (acc / 0.2) % 1.0
        scar = 1.0 - math.exp(-(u / 0.1) ** 2) - math.exp(-((1.0 - u) / 0.05) ** 2)
        return (max(0.12, 0.4 + 0.45 * scar) * 0.85, 0.0, ring_rnd[int(acc / 0.2) % 400], 0.0)
    tube(B, pts, radius, 16, "MI_IslBark_Palm", colour, cap_end=True, u_tiles=1)
    q = len(pts) // 4
    for k in range(4):
        seg = pts[k * q: (k + 1) * q + 1]
        hull = []
        for p in seg[::6] + [seg[-1]]:
            for a in range(8):
                ang = 2 * math.pi * a / 8
                hull.append(p + Vector((0.0, math.cos(ang), math.sin(ang))) * r_base * 0.95)
        B.ucx.append(hull)
    # The torn-up root plate: a ring of roots splayed out from the foot (G = 1: the roots' colour), clods of earth.
    tmp = bmesh.new()
    bmesh.ops.create_icosphere(tmp, subdivisions=3, radius=1.0)
    vmap = {}
    for v in tmp.verts:
        d = v.co.normalized()
        bump = 1.0 + 0.12 * math.sin(5 * d.y + 1.0) * math.sin(4 * d.z) + 0.08 * math.sin(9 * d.x + 2.0 * d.y)
        vmap[v] = (B.bm.verts.new(Vector((d.x * 0.22 - 0.12, d.y * 0.62, d.z * 0.55 + 0.42)) * 1.0 * bump), d)
    for f in tmp.faces:
        B.face([vmap[v][0] for v in f.verts], [sphere_uv(vmap[v][1]) for v in f.verts],
               [(0.15 + 0.25 * max(vmap[v][1].z, 0.0), 1.0, 0.3, 0.0) for v in f.verts], "MI_IslBark_Palm")
    tmp.free()
    for k in range(70):
        a = rng.uniform(0, 2 * math.pi)
        d = Vector((0.0, math.cos(a), math.sin(a)))
        l = rng.uniform(0.1, 0.45)
        root = resample(bezier(d * 0.15 + Vector((0.05, 0, 0.45)), d * 0.45 + Vector((-0.05, 0, 0.45)),
                               d * (0.5 + l * 0.6) + Vector((-0.15, 0, 0.42)), d * (0.6 + l) + Vector((-0.1 + rng.uniform(-0.1, 0.1), 0, 0.4)), 6), 0.05)
        for p in root:
            p.z = max(p.z, 0.02)
        rr = rng.uniform(0.012, 0.03)
        rnd = rng.random()
        tube(B, root, lambda i, s, a_, rr=rr: rr * (1.0 - 0.6 * s), 5, "MI_IslBark_Palm", lambda i, s, a_, rnd=rnd: (0.3 + 0.3 * s, 1.0, rnd, 0.0),
             u_tiles=1)
    # The crown at the far end: the shaft and its old leaf bases, dead fronds broken and splayed, some lying in the water.
    end = pts[-1]
    t_dir = (pts[-1] - pts[-4]).normalized()
    for k in range(11):
        az = k * 2.39996 + rng.uniform(-0.3, 0.3)
        el = math.radians(rng.uniform(-35, 25))
        frond(B, end + t_dir * 0.1, az, el, rng.uniform(2.0, 3.0), rng, droop=0.35, dry=1.0 if rng.random() < 0.75 else 0.6,
              torn=rng.uniform(0.4, 0.9), slot="MI_IslLeaf_Palm", hang=0.6)
    return B.finish()


BUILDERS = {
    "palm_a": lambda: palm("SM_Palm_A", 9.0, 2.2, 13, 101, 3.4),
    "palm_b": lambda: palm("SM_Palm_B", 7.0, 3.6, 12, 102, 3.1),
    "palm_c": lambda: palm("SM_Palm_C", 4.4, 0.7, 10, 103, 2.6, young=True),
    "tree_a": lambda: tree("SM_Tree_A", 9.0, 201, 3, 2.6, 1.9, 46.0, 0.34),
    "tree_b": lambda: tree("SM_Tree_B", 6.5, 202, 2, 1.8, 1.6, 46.0, 0.32, extra_top=2),
    "tree_c": lambda: tree("SM_Tree_C", 7.5, 203, 4, 2.2, 1.5, 44.0, 0.3, extra_top=1, lean=0.9),
    "bush_a": lambda: bush_round("SM_Bush_A", 301),
    "bush_b": lambda: bush_tufts("SM_Bush_B", 302),
    "flower": lambda: bush_round("SM_Bush_Flower", 303, flowers=30),
    "fern": lambda: fern("SM_Fern_A", 401),
    "bigleaf": lambda: bigleaf("SM_BigLeaf_A", 402),
    "grass": lambda: (grass_tuft("SM_GrassTuft_A", 501, 34, (0.35, 0.65), 0.15), sb.reset_scene(),
                      grass_tuft("SM_GrassTuft_B", 502, 26, (0.4, 0.75), 0.6, seeds=4)),
    "runner": lambda: (runner("SM_Runner_A", 601, 1.6, True), sb.reset_scene(), runner("SM_Runner_B", 602, 1.1, False)),
    "fallen": lambda: fallen_palm("SM_Home_FallenPalm", 701),
    "lawn": lambda: (grass_patch("SM_Grass_PatchA", 801, 220, (0.07, 0.14)), sb.reset_scene(),
                     grass_patch("SM_Grass_PatchB", 802, 200, (0.08, 0.16), straw=0.12, seeds=4), sb.reset_scene(),
                     grass_patch("SM_Grass_PatchEdge", 803, 60, (0.06, 0.12), blades=(3, 6), straw=0.18)),
}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_VEGETATION_OK", wanted)
