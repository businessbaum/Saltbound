"""Starter boat for the M4 sailing model, hand-made style (D36, restyled in D38). The cabin concept and the hull
colour are still owner decisions (Docs/Progress.md), so the deck is open and the paint is a muted oxblood / cream.

Style (owner brief): hand-crafted, chunky silhouettes, gently crooked geometry, broad bevels, sculpted wear; no
sharp corners. Every timber is an individual sb_timber board or spar (own seed): carvel strakes bent along the
hull, a golden wale and cap rail, planked bulwarks on stanchions, a teak deck with a covering board, hewn spars,
chunky rope rigging with deadeyes, a canvas sail with sewn panels, boltropes, reef points and mast hoops.

The hull follows Saltbound/Source/Saltbound/Boat/SaltHullShape.h exactly (same formulas and constants), because
ASaltBoat floats on columns computed from that shape: keep both in sync. The planking lies on that surface.

  SM_Boat_Hull    hull, deck, bulwarks with boarding gaps midship, keel, stem, mast, standing rigging, wheel
                  pedestal, bitts, hatch; UCX collision (walkable deck, bulwarks, keel, mast foot). Pivot: midship,
                  design waterline. Nanite.
  SM_Boat_Boom    pivot at the gooseneck, spar along -X
  SM_Boat_Gaff    pivot at the jaws, spar along -X (the actor raises it to the gaff angle)
  SM_Boat_Sail    gaff mainsail in the boom's frame (X/Z plane), bellied towards +Y; the actor scales Y to leeward
                  and Z to lower it (the mast hoops are part of it and stack up on the boom)
  SM_Boat_Rudder  pivot at the heel of the stock, stock along +Z, blade aft (-X)
  SM_Boat_Wheel   pivot at the hub, axle along X

Coordinates below are Unreal body coordinates in metres (x forward, y starboard, z up); U() converts to Blender,
where starboard is -Y.
Run: Tools/Build-BlenderAsset.ps1 build_boat [hull] [spars] [sail] [rudder] [wheel]
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
import sb_timber as tb  # noqa: E402

# --- hull shape: mirrors SaltHullShape.h ------------------------------------------------------------------
HALF_LENGTH, HALF_BEAM, FREEBOARD, SHEER_RISE, DRAFT = 5.0, 1.7, 0.75, 0.3, 0.65
BILGE_EXP, ENDS_START_X = 0.8, 3.0
KEEL_AFT_X, KEEL_AFT_Z, KEEL_FORE_X, KEEL_HALF_T = -4.3, -1.45, 3.6, 0.14
BULWARK_H, GATE_HALF = 0.45, 0.5
MAST_X, MAST_H, BOOM_H, BOOM_L, THROAT_H, GAFF_L, GAFF_ANGLE = 1.2, 10.5, 2.05, 5.0, 6.8, 4.6, 35.0
WHEEL_X, WHEEL_H = -3.2, 0.85
RUDDER_HEEL, RUDDER_HEAD, RUDDER_CHORD = (-4.38, -1.45), (-4.98, 1.0), 0.6
CAMBER = 0.04            # deck crown
BOOTTOP = 0.03           # top of the dark bottom paint above the waterline

# --- hand-made build (D38) -----------------------------------------------------------------------------------
STRAKES = 9              # planks per side from the keel to the sheer
PLANK_T = 0.05           # hull plank thickness
BULWARK_T = 0.11         # outer board, stanchion space, inner board
DECK_T = 0.045           # deck plank thickness (its top is the walkable deck)
DECK_W = 0.14            # deck plank width
MARGIN_W = 0.2           # covering board on the deck edge

M = {}
SEED = [700]


def clamp(v, lo=0.0, hi=1.0):
    return min(max(v, lo), hi)


def sheer_z(x):
    t = x / HALF_LENGTH
    return FREEBOARD + SHEER_RISE * t * t


def half_beam(x):
    t = min(abs(x) / HALF_LENGTH, 1.0)
    return HALF_BEAM * max(1.0 - t ** 2.2, 0.0) ** 0.55


def bottom_z(x):
    s = clamp((abs(x) - ENDS_START_X) / (HALF_LENGTH - ENDS_START_X))
    return -DRAFT + (DRAFT + sheer_z(HALF_LENGTH)) * s ** 3


def keel_z(x):
    a = clamp((x - KEEL_AFT_X) / (KEEL_FORE_X - KEEL_AFT_X))
    return KEEL_AFT_Z + (bottom_z(KEEL_FORE_X) - KEEL_AFT_Z) * a


def deck_z(x, y):
    bs = half_beam(x)
    return sheer_z(x) + (CAMBER * (1.0 - min((y / bs) ** 2, 1.0)) if bs > 1e-4 else 0.0)


def section_point(x, theta, side):
    """Hull surface at station x, angle theta (0 keel .. pi/2 sheer), side +1 starboard / -1 port."""
    zs, zb = sheer_z(x), bottom_z(x)
    return (x, side * half_beam(x) * math.sin(theta) ** BILGE_EXP, zs - (zs - zb) * math.cos(theta))


def stem_x(z):
    """X of the stem / stern profile at height z (where the canoe body's bottom reaches z)."""
    r = clamp((z + DRAFT) / (DRAFT + sheer_z(HALF_LENGTH)))
    return ENDS_START_X + (HALF_LENGTH - ENDS_START_X) * r ** (1.0 / 3.0)


def U(p):
    return Vector((p[0], -p[1], p[2]))


def nseed():
    SEED[0] += 1
    return SEED[0]


def mats():
    for n, c in {"MI_Paint_Hull": (0.45, 0.12, 0.08), "MI_Paint_Cream": (0.9, 0.85, 0.7), "MI_Paint_Boottop": (0.1, 0.1, 0.1),
                 "MI_Wood_Teak": (0.55, 0.35, 0.18), "MI_Wood_Varnish": (0.4, 0.2, 0.08), "MI_Metal_Iron": (0.1, 0.1, 0.1),
                 "MI_Caulk_Dark": (0.05, 0.05, 0.05), "MI_Canvas": (0.85, 0.8, 0.7), "MI_Rope_Hemp": (0.6, 0.5, 0.3)}.items():
        M[n] = sb.get_material(n, (*c, 1))


def mesh_object(name, bm, materials):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    for m in materials:
        obj.data.materials.append(M[m])
    return obj


# --- generic pieces ------------------------------------------------------------------------------------------
def laid_board(name, length, width, thick, frame, mat, wear=0.25, grooves=(1, 2), bevel=0.8):
    """A rough_plank laid along frame(s) -> (P, A, N, width_scale), all in Unreal coordinates (see bend_board)."""
    b = tb.rough_plank(name, length, width, thick, (0, 0, 0), (0, 0, 0), M[mat], nseed(), wear=wear, grooves=grooves,
                       split_chance=0.0, bevel=bevel)

    def blender_frame(s):
        p, a, n, w = frame(s)
        t = U(frame(min(s + 0.003, 1.0))[0]) - U(frame(max(s - 0.003, 0.0))[0])
        av, nv = U(a), U(n)
        # Keep the board's frame right-handed so its faces stay outward (the Y mirror would flip them).
        if t.cross(av).dot(nv) < 0.0:
            av = -av
        return U(p), av, nv, w

    tb.bend_board(b, length, blender_frame)
    return b


def path_length(frame, n=40):
    pts = [Vector(frame(i / n)[0]) for i in range(n + 1)]
    return sum((b - a).length for a, b in zip(pts[:-1], pts[1:]))


ROPE_UV_SCALE = 6.0      # MI_Rope_Hemp's UVScale: one texture tile around the rope


def along_uv(obj, length):
    """UVs for a cylinder built along local Z: U along it (metres, so rope twist and wood grain keep their scale),
    V once around (one rope-texture tile; faces across the seam are unwrapped)."""
    me = obj.data
    uv = me.uv_layers[0] if me.uv_layers else me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        angs = [math.atan2(me.vertices[me.loops[li].vertex_index].co.y, me.vertices[me.loops[li].vertex_index].co.x)
                for li in poly.loop_indices]
        wrap = max(angs) - min(angs) > math.pi
        for li, ang in zip(poly.loop_indices, angs):
            if wrap and ang < 0.0:
                ang += 2.0 * math.pi
            co = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = (co.z + length * 0.5, ang / (2.0 * math.pi) / ROPE_UV_SCALE)


def rod(name, a, b, radius, mat, segments=10):
    """Cylinder between two Unreal-space points (ropes, spokes)."""
    a, b = U(a), U(b)
    d = b - a
    obj = sb.add_cylinder(name, radius, d.length, (0, 0, 0), "Z", segments, M[mat])
    along_uv(obj, d.length)
    rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
    obj.data.transform(Matrix.Translation((a + b) / 2) @ rot)
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def rope(name, pts, radius=0.02, mat="MI_Rope_Hemp"):
    """Chunky rope through Unreal-space points, with a slight sag between the ends."""
    return [rod(name, a, b, radius, mat) for a, b in zip(pts[:-1], pts[1:])]


def sagging(a, b, sag, n=6):
    a, b = Vector(a), Vector(b)
    return [tuple(a.lerp(b, i / n) - Vector((0, 0, sag * math.sin(math.pi * i / n)))) for i in range(n + 1)]


def block(name, size, center, mat, bevel=0.02):
    """Chunky box with broad soft bevels."""
    obj = sb.add_box(name, size, U(center), M[mat])
    sb.soften(obj, bevel, 3)
    return obj


def post(name, radius, height, bottom, mat, sides=4, taper=0.92):
    return tb.rough_post(name, radius, height, U(bottom), sides=sides, taper=taper, material=M[mat], seed=nseed(), top_wear=0.3)


def sheer_dirs(x, side):
    """Unit tangent along the sheer line and the horizontal outward normal, in plan, at x."""
    e = 0.01
    p0 = Vector((x - e, side * half_beam(x - e), 0.0))
    p1 = Vector((x + e, side * half_beam(x + e), 0.0))
    t = (p1 - p0).normalized()
    out = Vector((-t.y, t.x, 0.0))
    if out.y * side < 0.0:
        out = -out
    return t, out


def paint_below(obj, z, mat):
    """Faces whose centre lies below z get another material (the waterline paint across planks)."""
    me = obj.data
    me.materials.append(M[mat])
    idx = len(me.materials) - 1
    for p in me.polygons:
        if p.center.z < z:
            p.material_index = idx


# --- hull --------------------------------------------------------------------------------------------------------
def hull_core():
    """Closed inner shell just inside the planking: dark, so the seams between planks read as caulked grooves."""
    bm = bmesh.new()
    n_st, n_th, deck_k = 72, 24, 10
    xs = [-HALF_LENGTH * math.cos(math.pi * i / (n_st - 1)) for i in range(n_st)]
    rows = []
    for x in xs:
        ths = [(math.pi / 2) * k / n_th for k in range(n_th + 1)]
        port = [bm.verts.new(U(section_point(x, t, -1))) for t in reversed(ths)]
        stbd = [bm.verts.new(U(section_point(x, t, 1))) for t in ths[1:]]
        shell = port + stbd
        bs = half_beam(x)
        deck = [shell[0]] + [bm.verts.new(U((x, y, deck_z(x, y) - DECK_T + 0.005)))
                             for y in (-bs + 2 * bs * k / deck_k for k in range(1, deck_k))] + [shell[-1]]
        rows.append((shell, deck))
    for (s0, d0), (s1, d1) in zip(rows[:-1], rows[1:]):
        for k in range(len(s0) - 1):
            bm.faces.new((s0[k], s0[k + 1], s1[k + 1], s1[k]))
        for k in range(len(d0) - 1):
            bm.faces.new((d0[k], d1[k], d1[k + 1], d0[k + 1]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object("hull_core", bm, ["MI_Caulk_Dark"])
    obj.data.shade_smooth()
    return obj


def strake_bounds():
    """Section angles of the plank edges: equal plank widths around the midship girth."""
    ts = [(math.pi / 2) * i / 600 for i in range(601)]
    pts = [Vector(section_point(0.0, t, 1)) for t in ts]
    cum = [0.0]
    for a, b in zip(pts[:-1], pts[1:]):
        cum.append(cum[-1] + (b - a).length)
    out = []
    for k in range(STRAKES + 1):
        target = cum[-1] * k / STRAKES
        out.append(ts[min(i for i, c in enumerate(cum) if c >= target - 1e-9)])
    return out


def strake_frame(side, ta, tb_, xa, xb, w0):
    def f(s):
        x = xa + (xb - xa) * s
        pa, pb = Vector(section_point(x, ta, side)), Vector(section_point(x, tb_, side))
        acr = pb - pa
        w = acr.length
        a = acr / w if w > 1e-6 else Vector((0.0, 0.0, 1.0))
        n = Vector((0.0, a.z, -a.y))
        mid = (pa + pb) * 0.5
        if n.dot(Vector((0.0, mid.y, mid.z - 0.2))) < 0.0:
            n = -n
        return mid + n * (PLANK_T * 0.5), a, n, max(w / w0, 0.02)
    return f


def planking():
    """Carvel strakes, each in two or three boards with staggered butt joints; the waterline is painted across them."""
    parts = []
    th = strake_bounds()
    rng = random.Random(41)
    for side in (-1, 1):
        for k in range(STRAKES):
            ta, tb_ = th[k], th[k + 1]
            w0 = (Vector(section_point(0.0, tb_, 1)) - Vector(section_point(0.0, ta, 1))).length
            cuts = sorted(rng.uniform(-3.2, 3.2) for _ in range(rng.choice((1, 2))))
            xs = [-HALF_LENGTH + 0.02] + cuts + [HALF_LENGTH - 0.02]
            top = k == STRAKES - 1
            for xa, xb in zip(xs[:-1], xs[1:]):
                frame = strake_frame(side, ta, tb_, xa + 0.004, xb - 0.004, w0)
                b = laid_board("strake", path_length(frame), w0 - 0.01, PLANK_T, frame,
                               "MI_Paint_Cream" if top else "MI_Paint_Hull", wear=0.25, grooves=(0, 1), bevel=0.9)
                if not top:
                    paint_below(b, BOOTTOP, "MI_Paint_Boottop")
                parts.append(b)
    return parts


def sheer_board(side, xa, xb, width, thick, at, mat, wear=0.3, grooves=(1, 2)):
    """A board following the sheer line in plan. at(x, out) -> (centre P, width dir A, thickness dir N)."""
    def f(s):
        x = xa + (xb - xa) * s
        _, out = sheer_dirs(x, side)
        p, a, n = at(x, out)
        return p, a, n, 1.0
    return laid_board("sheer", path_length(f), width, thick, f, mat, wear=wear, grooves=grooves)


def bulwark_spans():
    end = HALF_LENGTH - 0.06
    return [(-end, -GATE_HALF), (GATE_HALF, end)]


def bulwarks():
    """Golden wale, planked bulwark (outer hull-coloured, inner cream) on square stanchions, chunky cap rail,
    covering board on the deck edge. Gaps for boarding midship."""
    parts = []
    up = Vector((0.0, 0.0, 1.0))
    for side in (-1, 1):
        # Wale: a chunky rubbing strake just below the sheer, standing proud of the planking.
        parts.append(sheer_board(side, -HALF_LENGTH + 0.08, HALF_LENGTH - 0.08, 0.11, 0.085,
                                 lambda x, out: (Vector((x, side * half_beam(x), sheer_z(x) - 0.06)) + out * (PLANK_T + 0.03), up, out),
                                 "MI_Wood_Varnish", wear=0.45))
        for xa, xb in bulwark_spans():
            for zc, h in ((0.115, 0.2), (0.325, 0.2)):
                parts.append(sheer_board(side, xa, xb, h, 0.035,
                                         lambda x, out, zc=zc: (Vector((x, side * half_beam(x), sheer_z(x) + zc)) - out * 0.0175, up, out),
                                         "MI_Paint_Hull", wear=0.3, grooves=(0, 1)))
                parts.append(sheer_board(side, xa, xb, h, 0.03,
                                         lambda x, out, zc=zc: (Vector((x, side * half_beam(x), sheer_z(x) + zc)) - out * (BULWARK_T - 0.015), up, out),
                                         "MI_Paint_Cream", wear=0.25, grooves=(0, 1)))
            parts.append(sheer_board(side, xa - (0.0 if xa < 0 else 0.03), xb + (0.03 if xb < 0 else 0.0), 0.18, 0.065,
                                     lambda x, out: (Vector((x, side * half_beam(x), sheer_z(x) + BULWARK_H + 0.03)) - out * 0.045, out, up),
                                     "MI_Wood_Varnish", wear=0.5))
            # Stanchions between the boards, about every 0.55 m.
            n = max(2, int((xb - xa) / 0.55))
            for i in range(n + 1):
                x = xa + 0.05 + (xb - xa - 0.1) * i / n
                _, out = sheer_dirs(x, side)
                p = Vector((x, side * half_beam(x), 0.0)) - out * (BULWARK_T * 0.5)
                parts.append(post("stanchion", 0.04, BULWARK_H + 0.01, (p.x, p.y, deck_z(x, p.y) - 0.03), "MI_Wood_Varnish"))
        # Covering board along the deck edge, inside the bulwark.
        def cover(x, out):
            bs = half_beam(x)
            y = max(bs - BULWARK_T - MARGIN_W * 0.5, 0.0)
            return Vector((x, side * y, deck_z(x, side * y) - 0.025)), out, up
        parts.append(sheer_board(side, -HALF_LENGTH + 0.25, HALF_LENGTH - 0.25, MARGIN_W, 0.05, cover, "MI_Wood_Varnish", wear=0.35))
    return parts


def deck_planks():
    """Teak planks fore and aft (tops at the walkable deck height), each in two boards, ending at the covering
    board. Narrow gaps show the dark caulking below."""
    parts = []
    rng = random.Random(53)
    inner = HALF_BEAM - BULWARK_T - MARGIN_W + 0.01
    n = int(2 * inner / DECK_W)
    up = Vector((0.0, 0.0, 1.0))
    across = Vector((0.0, 1.0, 0.0))
    for i in range(n):
        y = -inner + (i + 0.5) * (2 * inner / n)
        need = abs(y) + DECK_W * 0.5 + BULWARK_T + MARGIN_W - 0.02
        xs = [x * 0.02 for x in range(-250, 251) if half_beam(x * 0.02) >= need]
        if len(xs) < 10:
            continue
        x0, x1 = xs[0] + 0.02, xs[-1] - 0.02
        cut = rng.uniform(x0 + 0.3 * (x1 - x0), x0 + 0.7 * (x1 - x0))
        for xa, xb in ((x0, cut), (cut, x1)):
            def f(s, xa=xa, xb=xb, y=y):
                x = xa + (xb - xa) * s
                return Vector((x, y, deck_z(x, y) - DECK_T * 0.5)), across, up, 1.0
            parts.append(laid_board("deck", xb - xa - 0.006, DECK_W - 0.008, DECK_T, f, "MI_Wood_Teak", wear=0.3, grooves=(1, 2)))
    return parts


def keel_and_posts():
    """Keel and deadwood (dark bottom paint, broad bevels), a golden stem timber over the bow profile and a
    sternpost carrying the rudder."""
    parts = []
    # Keel: a solid fin from the hull bottom to the keel line.
    bm = bmesh.new()
    xs = [KEEL_AFT_X + (KEEL_FORE_X - KEEL_AFT_X) * i / 30 for i in range(31)]
    rings = []
    for x in xs:
        top = bottom_z(x) + 0.12
        bot = min(keel_z(x), top - 0.03)
        t = KEEL_HALF_T * (0.6 + 0.4 * clamp((KEEL_FORE_X - x) / 1.2))
        rings.append([bm.verts.new(U(p)) for p in ((x, t, bot), (x, t, top), (x, -t, top), (x, -t, bot))])
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for k in range(4):
            bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
    bm.faces.new(rings[0])
    bm.faces.new(list(reversed(rings[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    keel = mesh_object("keel", bm, ["MI_Paint_Boottop"])
    sb.soften(keel, 0.035, 3)
    parts.append(keel)
    # Sternpost / deadwood between the stern profile and the raked rudder stock (the rudder hangs on it).
    (hx, hz), (tx, tz) = RUDDER_HEEL, RUDDER_HEAD
    bm = bmesh.new()
    rings = []
    for k in range(17):
        z = hz + (0.95 - hz) * k / 16
        xf = KEEL_AFT_X if z <= bottom_z(KEEL_AFT_X) else -stem_x(z)
        xf += 0.03
        xa = min(hx + (tx - hx) * (z - hz) / (tz - hz) + 0.04, xf - 0.02)
        rings.append([bm.verts.new(U(p)) for p in ((xa, 0.065, z), (xf, 0.09, z), (xf, -0.09, z), (xa, -0.065, z))])
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for k in range(4):
            bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
    bm.faces.new(rings[0])
    bm.faces.new(list(reversed(rings[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    stern = mesh_object("sternpost", bm, ["MI_Paint_Hull"])
    paint_below(stern, BOOTTOP, "MI_Paint_Boottop")
    sb.soften(stern, 0.025, 3)
    parts.append(stern)
    # Stem: a chunky golden timber over the bow profile, rising above the bulwarks as a stem head.
    z0, z1 = bottom_z(KEEL_FORE_X) + 0.02, sheer_z(HALF_LENGTH) + 0.62

    def stem(s):
        z = z0 + (z1 - z0) * s
        top = sheer_z(HALF_LENGTH)
        x = stem_x(min(z, top)) + max(z - top, 0.0) * 0.12
        e = 0.01
        za, zb = z - e, z + e
        xa_ = stem_x(min(za, top)) + max(za - top, 0.0) * 0.12
        xb_ = stem_x(min(zb, top)) + max(zb - top, 0.0) * 0.12
        t = Vector((xb_ - xa_, 0.0, zb - za)).normalized()
        n = Vector((t.z, 0.0, -t.x))
        return Vector((x, 0.0, z)) + n * 0.02, Vector((0.0, 1.0, 0.0)), n, 1.0
    parts.append(laid_board("stem", path_length(stem), 0.17, 0.13, stem, "MI_Wood_Varnish", wear=0.5))
    return parts


def rig_and_fittings():
    """Hewn mast with iron bands and a cap, chunky rope shrouds on deadeyes and chain wales, forestay, wheel
    pedestal, bitts, a hatch and a coiled rope."""
    parts = []
    deck_m = sheer_z(MAST_X)
    mast = tb.spar("mast", MAST_H + 0.1, 0.13, 0.085, M["MI_Wood_Varnish"], nseed(), sides=16, sweep=0.04, axis=(0, 0, 1))
    mast.data.transform(Matrix.Translation(U((MAST_X, 0, deck_m - 0.1))))
    parts.append(mast)
    top = deck_m + MAST_H
    parts.append(block("mast_cap", (0.32, 0.26, 0.12), (MAST_X, 0, top - 0.02), "MI_Wood_Varnish", 0.03))
    for z, r in ((deck_m + BOOM_H, 0.135), (deck_m + THROAT_H + 0.45, 0.115), (top - 0.25, 0.1)):
        parts.append(sb.add_torus("band", r, 0.022, U((MAST_X, 0, z)), (0, 0, 0), 24, 8, M["MI_Metal_Iron"]))
    parts.append(sb.add_torus("partners", 0.16, 0.05, U((MAST_X, 0, deck_m + 0.03)), (0, 0, 0), 24, 10, M["MI_Wood_Varnish"]))
    # Shrouds on deadeyes over chain wales (forward of the mast: the boom swings aft of them).
    hounds = (MAST_X, 0, deck_m + THROAT_H + 0.45)
    for side in (-1, 1):
        x_w = MAST_X + 0.35
        _, out = sheer_dirs(x_w, side)
        wale_p = Vector((x_w, side * half_beam(x_w), sheer_z(x_w) - 0.03)) + out * 0.11
        parts.append(block("chainwale", (1.1, 0.2, 0.07), tuple(wale_p), "MI_Wood_Varnish", 0.02))
        for dx in (0.12, 0.55):
            x = MAST_X + dx
            _, out = sheer_dirs(x, side)
            edge = Vector((x, side * half_beam(x), 0.0)) + out * 0.18
            low = (edge.x, edge.y, sheer_z(x) + 0.55)
            high = (edge.x, edge.y, sheer_z(x) + 0.95)
            parts += rope("shroud", sagging(hounds, high, 0.04, 8), 0.02)
            for c in (low, high):
                de = sb.add_cylinder("deadeye", 0.075, 0.06, U(c), "Y", 16, M["MI_Wood_Varnish"])
                sb.transform_obj(de, (0, 0, 0))
                sb.soften(de, 0.012, 2)
                parts.append(de)
            for k in (-0.035, 0.0, 0.035):
                parts.append(rod("lanyard", (low[0] + k, low[1], low[2] + 0.06), (high[0] + k, high[1], high[2] - 0.06), 0.009, "MI_Rope_Hemp"))
            parts.append(block("chainplate", (0.06, 0.03, 0.9), (edge.x, edge.y, sheer_z(x) + 0.1), "MI_Metal_Iron", 0.008))
    parts += rope("forestay", sagging((MAST_X, 0, top - 0.3), (HALF_LENGTH + 0.02, 0, sheer_z(HALF_LENGTH) + 0.55), 0.12, 10), 0.022)
    # Wheel pedestal: a chunky square post with a head block.
    hub_z = sheer_z(WHEEL_X) + WHEEL_H
    parts.append(post("pedestal", 0.15, hub_z - 0.12 - sheer_z(WHEEL_X), (WHEEL_X, 0, sheer_z(WHEEL_X) - 0.02), "MI_Wood_Varnish", 4, 0.88))
    parts.append(block("pedestal_head", (0.24, 0.2, 0.2), (WHEEL_X, 0, hub_z - 0.06), "MI_Wood_Varnish", 0.03))
    # Bitts fore and aft (mooring lines).
    for x in (4.0, -4.15):
        for y in (-0.22, 0.22):
            parts.append(post("bitt", 0.075, 0.5, (x, y, deck_z(x, y) - 0.03), "MI_Wood_Varnish", 4, 0.9))
        parts.append(block("bitt_bar", (0.09, 0.62, 0.09), (x, 0, deck_z(x, 0) + 0.3), "MI_Wood_Varnish", 0.02))
    # Foredeck hatch: coaming and planked lid.
    hx, hs = 2.75, 0.8
    for s in (-1, 1):
        parts.append(tb.rough_plank("coaming", hs, 0.14, 0.07, U((hx, s * (hs / 2 - 0.035), deck_z(hx, 0) + 0.06)), (math.pi / 2, 0, 0),
                                    M["MI_Wood_Varnish"], nseed(), wear=0.35, grooves=(1, 1), split_chance=0.0))
        parts.append(tb.rough_plank("coaming", hs - 0.14, 0.14, 0.07, U((hx + s * (hs / 2 - 0.035), 0, deck_z(hx, 0) + 0.06)), (math.pi / 2, 0, math.pi / 2),
                                    M["MI_Wood_Varnish"], nseed(), wear=0.35, grooves=(1, 1), split_chance=0.0))
    for k in range(5):
        w = (hs + 0.06) / 5
        parts.append(tb.rough_plank("lid", hs + 0.06, w - 0.008, 0.045, U((hx, -(hs + 0.06) / 2 + (k + 0.5) * w, deck_z(hx, 0) + 0.15)), (0, 0, 0),
                                    M["MI_Wood_Teak"], nseed(), wear=0.3, grooves=(1, 2), split_chance=0.0))
    # Coiled rope by the mast foot.
    for k in range(4):
        r = 0.22 - k * 0.035
        parts.append(sb.add_torus("coil", r, 0.022, U((MAST_X - 0.55, -0.45, deck_z(MAST_X - 0.55, -0.45) + 0.02 + k * 0.006)), (0, 0, k * 0.5), 32, 8, M["MI_Rope_Hemp"]))
    return parts


# --- collision ------------------------------------------------------------------------------------------
def convex(name, points):
    bm = bmesh.new()
    for p in points:
        bm.verts.new(U(p))
    bmesh.ops.convex_hull(bm, input=bm.verts)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    obj.display_type = "WIRE"
    return obj


def bulwark_section(x, top_extra=0.0):
    """Collision cross-section of the bulwark at station x (starboard): outer bottom, outer top, inner top, inner bottom."""
    bs, zs = half_beam(x), sheer_z(x)
    yi = max(bs - BULWARK_T, 0.0)
    return [(x, bs, zs - 0.03), (x, bs, zs + BULWARK_H + top_extra), (x, yi, zs + BULWARK_H + top_extra),
            (x, yi, deck_z(x, yi) - 0.02)]


def hull_collision(mesh_name):
    cols = []
    n = 0

    def add(points):
        nonlocal n
        n += 1
        cols.append(convex("UCX_%s_%02d" % (mesh_name, n), points))

    # Hull body in slices: section and deck points at three stations each (the deck is the walkable top).
    bounds = [-5.0, -3.75, -2.5, -1.25, 0.0, 1.25, 2.5, 3.75, 5.0]
    ths = [(math.pi / 2) * k / 16 for k in range(17)]
    for x0, x1 in zip(bounds[:-1], bounds[1:]):
        pts = []
        for x in (x0, (x0 + x1) / 2, x1):
            x = clamp(x, -HALF_LENGTH + 0.02, HALF_LENGTH - 0.02)
            for t in ths:
                pts += [section_point(x, t, 1), section_point(x, t, -1)]
            bs = half_beam(x)
            pts += [(x, y, deck_z(x, y)) for y in (-0.6 * bs, -0.3 * bs, 0.0, 0.3 * bs, 0.6 * bs)]
        add(pts)
    # Bulwarks in pieces along the sheer (both sides, both spans), denser towards the ends where they curve.
    for side in (-1, 1):
        for xa, xb in bulwark_spans():
            cuts = [xa + (xb - xa) * (0.5 - 0.5 * math.cos(math.pi * i / 5)) for i in range(6)]
            for a, b in zip(cuts[:-1], cuts[1:]):
                pts = []
                for x in (a, (a + b) / 2, b):
                    pts += [(p[0], side * p[1], p[2]) for p in bulwark_section(x, 0.08)]
                add(pts)
    # Keel (aft and forward half), mast foot, pedestal, bitts, hatch.
    for a, b in ((KEEL_AFT_X, 0.0), (0.0, KEEL_FORE_X)):
        pts = []
        for i in range(7):
            x = a + (b - a) * i / 6
            for y in (-KEEL_HALF_T, KEEL_HALF_T):
                pts += [(x, y, keel_z(x)), (x, y, bottom_z(x) + 0.1)]
        add(pts)
    deck_m = sheer_z(MAST_X)
    add([(MAST_X + 0.14 * math.cos(a), 0.14 * math.sin(a), z) for a in [i * math.pi / 4 for i in range(8)] for z in (deck_m - 0.1, deck_m + 2.6)])
    hub_z = sheer_z(WHEEL_X) + WHEEL_H
    add([(WHEEL_X + dx, dy, z) for dx in (-0.15, 0.15) for dy in (-0.15, 0.15) for z in (sheer_z(WHEEL_X) - 0.05, hub_z + 0.04)])
    for x in (4.0, -4.15):
        add([(x + dx, dy, z) for dx in (-0.08, 0.08) for dy in (-0.32, 0.32) for z in (deck_z(x, 0) - 0.05, deck_z(x, 0) + 0.36)])
    hx, hs = 2.75, 0.8
    add([(hx + dx, dy, z) for dx in (-hs / 2, hs / 2) for dy in (-hs / 2, hs / 2) for z in (deck_z(hx, 0) - 0.05, deck_z(hx, 0) + 0.18)])
    return cols


# --- assembly -------------------------------------------------------------------------------------------------
def finish(parts, name, waterline=None, wear=0.6):
    for p in parts:
        if not p.data.uv_layers:
            sb.world_box_uv(p, 1.0)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=wear, waterline=waterline, wet_height=0.15, seed=7)
    return obj


def export(obj, cols=()):
    sb.save_blend(obj.name)
    sb.export_fbx([obj] + list(cols), os.path.join("Boat", obj.name + ".fbx"))


def build_hull():
    parts = [hull_core()] + planking() + bulwarks() + deck_planks() + keel_and_posts() + rig_and_fittings()
    obj = finish(parts, "SM_Boat_Hull", waterline=0.0)
    cols = hull_collision("SM_Boat_Hull")
    print("SB_BOAT_HULL faces", len(obj.data.polygons), "parts", len(parts), "collision pieces", len(cols))
    export(obj, cols)


# --- moving parts -------------------------------------------------------------------------------------------
def build_spar(name, length, radius, end_radius, jaw_gap):
    """Hewn spar along -X from the pivot with forked jaws around the mast and an iron band at the outer end."""
    parts = [tb.spar("spar", length, radius, end_radius, M["MI_Wood_Varnish"], nseed(), sides=14, sweep=0.03)]
    for s in (-1, 1):
        parts.append(tb.rough_plank("jaw", 0.42, 0.07, 0.16, (0.06, s * (jaw_gap + 0.035), 0.0), (math.pi / 2, 0, 0),
                                    M["MI_Wood_Varnish"], nseed(), wear=0.4, grooves=(0, 1), split_chance=0.0))
    parts.append(sb.add_torus("band", end_radius + 0.012, 0.018, (-length + 0.15, 0, 0), (0, math.pi / 2, 0), 20, 8, M["MI_Metal_Iron"]))
    obj = finish(parts, name)
    export(obj)


def build_sail():
    """Gaff mainsail in the boom's frame (aft distance, height above the boom): eight sewn panels parallel to the
    leech with raised seams, a belly of 8 % of the chord towards +Y (Unreal), a hollow leech, a chunky boltrope
    round the edges, a row of reef points and wooden mast hoops along the luff."""
    g = math.radians(GAFF_ANGLE)
    rise = THROAT_H - BOOM_H
    tack, throat = Vector((0.0, 0.0)), Vector((0.0, rise))
    peak = Vector((GAFF_L * math.cos(g), rise + GAFF_L * math.sin(g)))
    clew = Vector((BOOM_L - 0.18, 0.0))
    panels, nv = 8, 22
    us = []
    for k in range(panels):
        a = k / panels
        us += [a, a + 0.012, a + 0.5 / panels, a + 1.0 / panels - 0.012]
    us.append(1.0)

    def point(u, v):
        luff = tack.lerp(throat, v)
        leech = clew.lerp(peak, v)
        leech = leech - Vector((0.13 * math.sin(math.pi * v), 0.0))          # hollow leech
        p = luff.lerp(leech, u)
        chord = (leech - luff).length
        belly = 0.08 * chord * (u / 0.4 if u < 0.4 else (1.0 - u) / 0.6) ** 0.8 * (1.0 - 0.3 * v)
        seam = 0.0
        for k in range(1, panels):
            seam = max(seam, 0.009 * clamp(1.0 - abs(u - k / panels) / 0.012))
        return (-p.x, belly + seam, p.y)

    bm = bmesh.new()
    grid = [[bm.verts.new(U(point(u, j / nv))) for u in us] for j in range(nv + 1)]
    for j in range(nv):
        for i in range(len(us) - 1):
            f = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            f.smooth = True
    sail = mesh_object("cloth", bm, ["MI_Canvas"])
    solid = sail.modifiers.new("Solidify", "SOLIDIFY")
    solid.thickness = 0.016
    solid.offset = 0.0
    sb.apply_modifiers(sail)
    parts = [sail]
    # Boltrope round the edges.
    edge = [point(0.0, j / nv) for j in range(nv + 1)] + [point(u, 1.0) for u in us[1:]] + \
           [point(1.0, j / nv) for j in range(nv - 1, -1, -1)] + [point(u, 0.0) for u in reversed(us[:-1])]
    parts += rope("boltrope", edge, 0.024)
    # Reef points: short ties through the cloth along a line a fifth of the way up.
    for k in range(panels):
        u = (k + 0.5) / panels
        p = Vector(point(u, 0.2))
        parts.append(rod("reef", tuple(p - Vector((0, 0.07, 0.0))), tuple(p + Vector((0, 0.07, -0.08))), 0.01, "MI_Rope_Hemp", 6))
    # Mast hoops (centred on the mast axis at the sail's luff).
    n_hoops = int(rise / 0.55)
    for k in range(n_hoops):
        z = 0.35 + (rise - 0.5) * k / max(n_hoops - 1, 1)
        hoop = sb.add_torus("hoop", 0.175, 0.026, U((0.0, 0.0, z)), (0, 0, 0), 24, 8, M["MI_Wood_Varnish"])
        parts.append(hoop)
    obj = finish(parts, "SM_Boat_Sail", wear=0.0)
    export(obj)


def build_rudder():
    """Rudder in the stock frame: three thick vertical boards with iron straps, a square head post above."""
    hx, hz = RUDDER_HEEL
    tx, tz = RUDDER_HEAD
    stock_len = math.hypot(tx - hx, tz - hz)
    rake = math.atan2(hx - tx, tz - hz)
    blade_top = (0.0 - hz) / math.cos(rake)
    parts = []
    bw = RUDDER_CHORD / 3
    for k in range(3):
        h = blade_top * (1.0 - 0.06 * k)
        parts.append(tb.rough_plank("blade", h, bw - 0.008, 0.07 - 0.012 * k, (-(k + 0.5) * bw, 0.0, h / 2), (math.pi / 2, -math.pi / 2, 0),
                                    M["MI_Paint_Boottop"], nseed(), wear=0.35, grooves=(1, 1), split_chance=0.0))
    head = tb.rough_post("head", 0.075, stock_len - blade_top + 0.05, (-0.07, 0.0, blade_top - 0.05), sides=4, taper=0.92,
                         material=M["MI_Paint_Hull"], seed=nseed(), top_wear=0.4)
    parts.append(head)
    for s in (0.35, 0.95):
        parts.append(sb.add_box("strap", (RUDDER_CHORD + 0.03, 0.095, 0.055), (-RUDDER_CHORD / 2 + 0.01, 0.0, s), M["MI_Metal_Iron"]))
        sb.soften(parts[-1], 0.01, 2)
    obj = finish(parts, "SM_Boat_Rudder")
    export(obj)


def build_wheel():
    """Chunky ship's wheel in the Y/Z plane (axle along X): thick rim, turned spokes and handles, iron hub."""
    parts = [sb.add_torus("rim", 0.42, 0.042, (0, 0, 0), (0, math.pi / 2, 0), 40, 12, M["MI_Wood_Varnish"]),
             sb.add_cylinder("hub", 0.09, 0.14, (0, 0, 0), "X", 18, M["MI_Metal_Iron"]),
             sb.add_cylinder("axle", 0.035, 0.24, (0.12, 0, 0), "X", 12, M["MI_Metal_Iron"])]
    for p in parts:
        for f in p.data.polygons:
            f.use_smooth = True
    for k in range(8):
        a = k * math.pi / 4
        d = Vector((0.0, math.cos(a), math.sin(a)))
        parts.append(rod("spoke", (0, 0, 0), tuple(d * 0.42), 0.03, "MI_Wood_Varnish"))
        parts.append(rod("handle", tuple(d * 0.45), tuple(d * 0.6), 0.036, "MI_Wood_Varnish"))
        parts.append(sb.add_cylinder("knob", 0.045, 0.06, tuple(U(tuple(d * 0.6))), "Z", 12, M["MI_Wood_Varnish"]))
        knob = parts[-1]
        rot = Vector((0, 0, 1)).rotation_difference(U(tuple(d)).normalized()).to_matrix().to_4x4()
        c = U(tuple(d * 0.6))
        knob.data.transform(Matrix.Translation(c) @ rot @ Matrix.Translation(-c))
    obj = finish(parts, "SM_Boat_Wheel")
    export(obj)


def main():
    args = sb.script_args() or ["hull", "spars", "sail", "rudder", "wheel"]
    builds = [("hull", build_hull),
              ("spars", lambda: build_spar("SM_Boat_Boom", BOOM_L, 0.085, 0.068, 0.14)),
              ("spars", lambda: build_spar("SM_Boat_Gaff", GAFF_L, 0.075, 0.06, 0.13)),
              ("sail", build_sail), ("rudder", build_rudder), ("wheel", build_wheel)]
    for part, fn in builds:
        if part in args:
            sb.reset_scene()
            mats()
            fn()


main()
