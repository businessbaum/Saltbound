"""Starter boat for the M4 sailing model, hand-made style (D36, restyled in D38). The cabin concept and the hull
colour are still owner decisions (Docs/Progress.md), so the deck is open and the paint is a muted oxblood / cream.

Style (owner brief): hand-crafted, chunky silhouettes, gently crooked geometry, broad bevels, sculpted wear; no
sharp corners. Every timber is an individual sb_timber board or spar (own seed): carvel strakes bent along the
hull, a golden wale and cap rail, planked bulwarks on stanchions, a teak deck with a covering board, hewn spars,
chunky rope rigging with deadeyes, a canvas sail with sewn panels, boltropes, reef points and mast hoops.

The hull follows Saltbound/Source/Saltbound/Boat/SaltHullShape.h exactly (same formulas and constants), because
ASaltBoat floats on columns computed from that shape: keep both in sync. The planking lies on that surface.

  SM_Boat_Hull    hull, deck, bulwarks with boarding gaps midship, keel, stem, mast, standing rigging, the yard's
                  halyard, lifts and braces, the brails' falls, the steering oar's boss (or the wheel pedestal),
                  bitts, hatch; UCX collision (walkable deck, bulwarks, keel, mast foot). Pivot: midship,
                  design waterline. Nanite.
  SM_Boat_Yard    D65: the yard slung across the mast's front, fixed square to the boat; pivot on its axis in the
                  middle, the spar along Y; its parrel round the mast, the slings, the yardarms' and the brails' bands
  SM_Boat_Sail    D65: the square sail in the yard's frame, hanging flat (Y/Z plane) from under the yard, laced to it;
                  M_SB_SquareSail bellies it forward and rolls it up from the foot to under the yard (ASaltBoat)
  SM_Boat_Rudder  pivot at the heel of the stock, stock along +Z, blade aft (-X)
  SM_Boat_Wheel   pivot at the hub, axle along X (wheel steering, kept for later boats)
  SM_Boat_SteeringOar  pivot in the boss, +Z up along the raked shaft, blade aft (-X), handle inboard (-Y)

Coordinates below are Unreal body coordinates in metres (x forward, y starboard, z up); U() converts to Blender,
where starboard is -Y.
Run: Tools/Build-BlenderAsset.ps1 build_boat [hull] [yard] [sail] [rudder] [wheel] ...
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
MAST_X, MAST_H = 1.2, 10.5
HOUNDS_H = 7.55                          # the shrouds' eyes over the hounds, above the deck at the mast
# D65 (owner: "a normal sail, so both sides with the same amount of sail, not rotatable"): a square sail on a yard slung across
# the mast's front, fixed square to the boat (SaltHullShape.h Yard* / Sail*). The yard's axis lies YARD_X along the boat and
# YARD_H above the deck at the mast: SM_Boat_Yard's and SM_Boat_Sail's origin. The sail's head is laced under the yard
# (SAIL_HEAD_Z below its axis); it hangs SAIL_DROP down to its foot, which spreads a little wider than its head; the foot
# hangs 2.2 m over the deck, so one walks under it.
YARD_X, YARD_H, YARD_HALF, YARD_R, YARD_END_R = 1.46, 9.0, 2.6, 0.1, 0.062
SAIL_HEAD_Z, SAIL_DROP, SAIL_HALF_HEAD, SAIL_HALF_FOOT = -0.15, 6.65, 2.3, 2.6
YARDARM_Y, BRAIL_Y = 2.42, 1.2          # the yardarms' bands (lifts and braces) and the brails' blocks under the yard
BRACE_X = -1.2                           # the braces are made fast on cleats inside the bulwark here
WHEEL_X, WHEEL_H = -3.2, 0.68
# Anchor rope (D52): the hawse pipe in the deck (x, y) and the hawse hole in the starboard bow planking (x, z).
HAWSE_DECK, HAWSE_HULL = (4.1, 0.55), (4.2, 0.66)
ENSIGN_X, ENSIGN_H = -4.62, 1.45          # flag staff on the stern deck and its height
# Steering (D45): the first boat steers with a big oar on the starboard quarter (SaltHullShape.h Oar*). "wheel" builds
# the pedestal for the wheel and sternpost rudder of D36-D43 instead (kept for later boats, ESaltSteering::Wheel).
STEERING = "oar"
OAR_BOSS_X, OAR_OUT, OAR_BOSS_H, OAR_RAKE = -3.35, 0.14, 0.1, 16.5
OAR_TIP, OAR_BLADE_TOP, OAR_CHORD, OAR_HEAD, OAR_HANDLE = -2.05, -1.15, 0.5, 0.72, 0.85
PENNANT_H, PENNANT_L, PENNANT_HOIST = 0.42, 1.6, 0.28   # swivel above the mast top; length and hoist of the cloth
# Windlass (its drum and rope are moving parts, D43) and the boarding ladders over both sides at the gap (D43).
WINDLASS_X, WINDLASS_DRUM_H = 3.55, 0.34
LADDER_X, LADDER_OUT, LADDER_BOTTOM_Z, RUNG_STEP = 0.0, 0.07, -0.75, 0.3
# The stores and the sea chest on the port side aft (D58: 30 % bigger, owner): centre (x, y) and size (m). The barrel is
# radius and height; the crate and the chest are length (X), depth (Y), height. ASaltBoat's lids follow these.
BARREL = (-1.65, -1.0, 0.27, 0.86)
CRATE = (-2.42, -0.95, 0.68, 0.5, 0.48)
CHEST = (-3.3, -0.6, 0.8, 0.52, 0.42)
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
                 "MI_Caulk_Dark": (0.05, 0.05, 0.05), "MI_Canvas": (0.85, 0.8, 0.7), "MI_Rope_Hemp": (0.6, 0.5, 0.3),
                 "MI_Wood_DockWeathered": (0.5, 0.45, 0.4), "MI_SquareSailCloth": (0.85, 0.8, 0.7),
                 "MI_SquareSailRope": (0.6, 0.5, 0.3), "MI_Pennant": (0.5, 0.1, 0.07),
                 # D52: every board a bit different (two more teaks, faded and dark hull paint, aged cream).
                 "MI_Wood_TeakLight": (0.62, 0.42, 0.22), "MI_Wood_TeakDark": (0.42, 0.26, 0.13),
                 "MI_Paint_HullFaded": (0.55, 0.2, 0.14), "MI_Paint_HullDark": (0.35, 0.08, 0.06),
                 "MI_Paint_CreamAged": (0.82, 0.74, 0.56), "MI_EnsignRed": (0.5, 0.1, 0.07), "MI_EnsignCream": (0.85, 0.78, 0.6),
                 "MI_Glass_Dark": (0.2, 0.3, 0.3), "MI_Metal_Brass": (0.6, 0.45, 0.2), "MI_Canvas": (0.7, 0.66, 0.56),
                 "MI_Wood_Tarred": (0.12, 0.09, 0.07), "MI_CandleFlame": (1.0, 0.7, 0.3), "MI_LanternGlass": (0.75, 0.5, 0.25),
                 # D57: the chart sheet on the mast (M_SB_Chart draws the live chart on it from its UV).
                 "MI_MastChart": (0.85, 0.78, 0.6)}.items():
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
def laid_board(name, length, width, thick, frame, mat, wear=0.25, grooves=(1, 2), bevel=0.8, split=0.0):
    """A rough_plank laid along frame(s) -> (P, A, N, width_scale), all in Unreal coordinates (see bend_board)."""
    b = tb.rough_plank(name, length, width, thick, (0, 0, 0), (0, 0, 0), M[mat], nseed(), wear=wear, grooves=grooves,
                       split_chance=split, bevel=bevel)

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


def oriented(obj, center, axis):
    """Rotates a mesh built around the origin along +Z so +Z follows `axis`, then moves it to `center` (Unreal)."""
    rot = Vector((0, 0, 1)).rotation_difference(U(tuple(axis)).normalized()).to_matrix().to_4x4()
    obj.data.transform(Matrix.Translation(U(tuple(center))) @ rot)
    return obj


def disc(name, radius, thick, center, axis, mat, segments=16, bevel=0.0):
    obj = sb.add_cylinder(name, radius, thick, (0, 0, 0), "Z", segments, M[mat])
    if bevel > 0:
        sb.soften(obj, bevel, 2)
    return oriented(obj, center, axis)


def ring(name, major, minor, center, axis, mat, segments=24):
    obj = sb.add_torus(name, major, minor, (0, 0, 0), (0, 0, 0), segments, 8, M[mat])
    return oriented(obj, center, axis)


def nail(center, normal):
    """A dark iron nail head flush with a surface (center on the surface, normal outward)."""
    n = Vector(normal).normalized()
    return disc("nail", 0.009, 0.006, tuple(Vector(center) + n * 0.002), tuple(n), "MI_Metal_Iron", 8)


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
            cuts = sorted(rng.uniform(-3.4, 3.4) for _ in range(rng.choice((1, 2, 2, 3))))
            xs = [-HALF_LENGTH + 0.02] + cuts + [HALF_LENGTH - 0.02]
            top = k == STRAKES - 1
            for xa, xb in zip(xs[:-1], xs[1:]):
                # Each board of its own (D52): a few millimetres proud or sunk, its own shade of paint (repainted,
                # faded or darkened boards), its own wear.
                off = rng.uniform(-0.003, 0.005)
                base = strake_frame(side, ta, tb_, xa + 0.004, xb - 0.004, w0)

                def frame(sf, base=base, off=off):
                    pm, am, nm, wm = base(sf)
                    return pm + nm * off, am, nm, wm
                r = rng.random()
                if top:
                    mat = "MI_Paint_CreamAged" if r < 0.3 else "MI_Paint_Cream"
                else:
                    mat = "MI_Paint_HullFaded" if r < 0.2 else "MI_Paint_HullDark" if r < 0.34 else "MI_Paint_Hull"
                b = laid_board("strake", path_length(frame), w0 - rng.uniform(0.008, 0.016), PLANK_T, frame, mat,
                               wear=rng.uniform(0.4, 0.75), grooves=(0, rng.randint(1, 2)), bevel=0.9, split=0.15)
                # Nails into the frames, about every 0.6 m, and two at each butt end.
                xn = [xa + 0.06, xb - 0.06] + [x for x in [-4.2 + 0.6 * i for i in range(15)] if xa + 0.15 < x < xb - 0.15]
                for x_n in xn:
                    sfrac = (x_n - xa) / max(xb - xa, 1e-3)
                    pm, am, nm, wm = frame(clamp(sfrac))
                    if wm * w0 > 0.08:
                        for off in ((-0.25, 0.25) if x_n in (xa + 0.06, xb - 0.06) else (0.0,)):
                            parts.append(nail(tuple(pm + nm * (PLANK_T * 0.5) + am * (off * wm * w0)), tuple(nm)))
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
                                         "MI_Paint_Hull", wear=0.5, grooves=(0, 1)))
                parts.append(sheer_board(side, xa, xb, h, 0.03,
                                         lambda x, out, zc=zc: (Vector((x, side * half_beam(x), sheer_z(x) + zc)) - out * (BULWARK_T - 0.015), up, out),
                                         "MI_Paint_Cream", wear=0.45, grooves=(0, 1)))
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
    """Teak planks fore and aft (tops at the walkable deck height), every one a bit different, like the pier's boards
    (owner, D52): widths from 11 to 17 cm, two to four boards per plank with the butt joints scattered, each a few
    millimetres proud or sunk, slightly rolled and crooked, with its own shade (three teaks, now and then a grey
    replaced board), wear and grain; gaps of 4-9 mm show the dark caulking below."""
    parts = []
    rng = random.Random(53)
    inner = HALF_BEAM - BULWARK_T - MARGIN_W + 0.01
    widths, total = [], 0.0
    while total < 2 * inner - 0.11:
        w = rng.uniform(0.11, 0.17)
        widths.append(w)
        total += w
    widths = [w * 2 * inner / total for w in widths]
    edge = -inner
    for w in widths:
        y = edge + w / 2
        edge += w
        gap = rng.uniform(0.004, 0.009)
        need = abs(y) + w * 0.5 + BULWARK_T + MARGIN_W - 0.02
        xs = [x * 0.02 for x in range(-250, 251) if half_beam(x * 0.02) >= need]
        if len(xs) < 10:
            continue
        x0, x1 = xs[0] + 0.02, xs[-1] - 0.02
        boards = rng.choice((2, 3, 3, 4)) if x1 - x0 > 2.5 else 1
        cuts = sorted(rng.uniform(x0 + 0.5, x1 - 0.5) for _ in range(boards - 1))
        ends = [x0] + [c for k, c in enumerate(cuts) if k == 0 or c - cuts[k - 1] > 0.6] + [x1]
        for xa, xb in zip(ends[:-1], ends[1:]):
            dz = rng.uniform(-0.006, 0.004)
            roll = math.radians(rng.uniform(-1.3, 1.3))
            drift = rng.uniform(-0.012, 0.012)
            across = Vector((0.0, math.cos(roll), math.sin(roll)))
            normal = Vector((0.0, -math.sin(roll), math.cos(roll)))

            def f(s, xa=xa, xb=xb, y=y, dz=dz, drift=drift, across=across, normal=normal):
                x = xa + (xb - xa) * s
                yy = y + drift * (2.0 * s - 1.0)                     # a gently crooked board
                return Vector((x, yy, deck_z(x, yy) - DECK_T * 0.5 + dz)), across, normal, 1.0
            r = rng.random()
            mat = ("MI_Wood_DockWeathered" if r < 0.06 else "MI_Wood_TeakDark" if r < 0.3 else
                   "MI_Wood_TeakLight" if r < 0.52 else "MI_Wood_Teak")
            parts.append(laid_board("deck", xb - xa - rng.uniform(0.005, 0.014), w - gap, DECK_T, f, mat,
                                    wear=rng.uniform(0.4, 0.8), grooves=(rng.randint(0, 1), rng.randint(1, 3)),
                                    split=rng.uniform(0.1, 0.4)))
            for xe in (xa + 0.05, xb - 0.05):
                for yo in (-w * 0.25, w * 0.25):
                    parts.append(nail((xe, y + yo, deck_z(xe, y) + dz), (0, 0, 1)))
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


def end_closures():
    """D58 (owner: "fix this hole in the ship"): both ends were open. The deck planks stop short of the stem and the
    sternpost, so between them and the bulwarks' ends one looked down into the dark hull, the cap rails ended either side
    of the posts, and the sternpost stopped below the rails. Now a breasthook (a thick timber filling the deck's V) at
    both ends, the sternpost carried up between the bulwarks to above the rails as a stern head (as the stem's head), and
    a cap over each end where the two rails meet."""
    parts = []
    top = sheer_z(HALF_LENGTH)
    for end in (1, -1):
        # The breasthook: between the bulwarks' inner faces from 0.75 m inboard of the end to the post, its inboard
        # edge a shallow curve, 1.2 cm proud of the deck planks it covers.
        x_in, x_tip = HALF_LENGTH - 0.75, HALF_LENGTH - 0.07
        def inner(x):
            return max(half_beam(x) - BULWARK_T + 0.01, 0.07)
        y_in = inner(x_in)
        plan = [(x_in + 0.12 * (1.0 - (y / y_in) ** 2), y) for y in [y_in * (-1.0 + 2.0 * i / 10) for i in range(11)]]
        plan += [(x, inner(x)) for x in [x_in + (x_tip - x_in) * i / 8 for i in range(1, 9)]]
        plan += [(x, -inner(x)) for x in [x_in + (x_tip - x_in) * i / 8 for i in range(8, 0, -1)]]
        bm = bmesh.new()
        tops = [bm.verts.new(U((end * x, y, deck_z(x, y) + 0.012))) for x, y in plan]
        bots = [bm.verts.new(U((end * x, y, deck_z(x, y) - 0.05))) for x, y in plan]
        bm.faces.new(tops)
        bm.faces.new(list(reversed(bots)))
        n = len(plan)
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((tops[j], tops[i], bots[i], bots[j]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
        hook = mesh_object("breasthook", bm, ["MI_Wood_TeakDark"])
        sb.soften(hook, 0.012, 2)
        parts.append(hook)
        for y in (-0.12, 0.12):
            parts.append(nail((end * (x_in + 0.32), y, deck_z(x_in + 0.32, y) + 0.012), (0, 0, 1)))
        # A cap over the post where the two cap rails end.
        parts.append(block("end_cap", (0.26, 0.36, 0.07), (end * (HALF_LENGTH - 0.06), 0.0, top + BULWARK_H + 0.035), "MI_Wood_Varnish", 0.02))
    # The stern head: the sternpost carried up between the bulwarks' ends to above the cap (the stem has its head).
    z0, z1 = top - 0.45, top + BULWARK_H + 0.16

    def stern_head(s):
        z = z0 + (z1 - z0) * s
        def xs(zz):
            return -stem_x(min(zz, top)) - max(zz - top, 0.0) * 0.08
        e = 0.01
        t = Vector((xs(z + e) - xs(z - e), 0.0, 2 * e)).normalized()
        nrm = Vector((-t.z, 0.0, t.x))
        return Vector((xs(z), 0.0, z)) + nrm * 0.02, Vector((0.0, 1.0, 0.0)), nrm, 1.0
    parts.append(laid_board("stern_head", path_length(stern_head), 0.17, 0.13, stern_head, "MI_Wood_Varnish", wear=0.5))
    return parts


def rig_and_fittings():
    """The mast and all its fittings (mast_and_fittings), chunky rope shrouds on deadeyes and chain wales, forestay,
    the steering oar's boss (or the wheel pedestal), bitts."""
    parts = mast_and_fittings()
    deck_m = sheer_z(MAST_X)
    top = deck_m + MAST_H
    # Shrouds on deadeyes over chain wales, aft of the mast (D65: the square sail hangs forward of it, clear of them); their
    # eyes lie over the hounds cheeks, so each ends at the mast's side at the hounds.
    for side in (-1, 1):
        x_w = MAST_X - 0.325
        _, out = sheer_dirs(x_w, side)
        wale_p = Vector((x_w, side * half_beam(x_w), sheer_z(x_w) - 0.03)) + out * 0.11
        parts.append(block("chainwale", (0.75, 0.2, 0.07), tuple(wale_p), "MI_Wood_Varnish", 0.02))
        for dx in (-0.15, -0.5):
            x = MAST_X + dx
            along, out = sheer_dirs(x, side)
            upv = Vector((0.0, 0.0, 1.0))
            edge = Vector((x, side * half_beam(x), 0.0)) + out * 0.18
            low = Vector((edge.x, edge.y, sheer_z(x) + 0.55))
            high = Vector((edge.x, edge.y, sheer_z(x) + 0.95))
            r_de, t_de = 0.075, 0.06
            for c in (low, high):
                parts.append(disc("deadeye", r_de, t_de, tuple(c), tuple(out), "MI_Wood_Varnish", 16, 0.012))
            # Three holes per deadeye (dark, both faces) and the lanyard laced through them, alternating faces.
            hu = [high + along * -0.032 + upv * 0.018, high + along * 0.032 + upv * 0.018, high + upv * -0.034]
            hl = [low + along * -0.032 + upv * -0.018, low + along * 0.032 + upv * -0.018, low + upv * 0.034]
            for h in hu + hl:
                for f_side in (-1, 1):
                    parts.append(disc("hole", 0.013, 0.004, tuple(h + out * (f_side * (t_de * 0.5 + 0.001))), tuple(out), "MI_Caulk_Dark", 8))
            face = t_de * 0.5 + 0.012
            runs = [(hu[0], hl[0], 1), (hl[0], hu[1], -1), (hu[1], hl[1], 1), (hl[1], hu[2], -1), (hu[2], hl[2], 1)]
            for a_p, b_p, f_side in runs:
                parts.append(rod("lanyard", tuple(a_p + out * (f_side * face)), tuple(b_p + out * (f_side * face)), 0.009, "MI_Rope_Hemp", 8))
            for h in hu + hl:
                parts.append(rod("lanyard", tuple(h - out * face), tuple(h + out * face), 0.009, "MI_Rope_Hemp", 8))
            # The shroud, taut, turns round the upper deadeye's rim and is seized to itself above it.
            parts.append(ring("shroud_eye", r_de + 0.017, 0.019, tuple(high), tuple(out), "MI_Rope_Hemp"))
            seize = high + upv * 0.36
            for s_side in (-1, 1):
                parts.append(rod("shroud_leg", tuple(high + along * (s_side * 0.05) + upv * 0.075), tuple(seize), 0.019, "MI_Rope_Hemp"))
            for k in range(3):
                parts.append(ring("seizing", 0.028, 0.009, tuple(seize + upv * (0.05 + k * 0.05)), (0, 0, 1), "MI_Rope_Hemp", 12))
            parts.append(rod("shroud", tuple(seize), MAST_INFO["hounds"](side), 0.02, "MI_Rope_Hemp"))
            # The lower deadeye sits in an iron strap on the chainplate.
            parts.append(ring("strap", r_de + 0.012, 0.011, tuple(low), tuple(out), "MI_Metal_Iron"))
            parts.append(block("chainplate", (0.06, 0.03, 0.8), (edge.x - out.x * 0.01, edge.y - out.y * 0.01, sheer_z(x) + 0.07), "MI_Metal_Iron", 0.008))
    parts += rope("forestay", sagging(MAST_INFO["forestay"], (HALF_LENGTH + 0.02, 0, sheer_z(HALF_LENGTH) + 0.55), 0.12, 10), 0.022)
    parts += yard_running_rigging()
    if STEERING == "wheel":
        # Wheel pedestal: a chunky square post with a head block.
        hub_z = sheer_z(WHEEL_X) + WHEEL_H
        parts.append(post("pedestal", 0.15, hub_z - 0.12 - sheer_z(WHEEL_X), (WHEEL_X, 0, sheer_z(WHEEL_X) - 0.02), "MI_Wood_Varnish", 4, 0.88))
        parts.append(block("pedestal_head", (0.24, 0.2, 0.2), (WHEEL_X, 0, hub_z - 0.06), "MI_Wood_Varnish", 0.03))
    else:
        # Steering oar boss on the starboard quarter: a chunky block on the sheer strake and a rope withy round the
        # oar's shaft (SM_Boat_SteeringOar turns in it), lashed to the block.
        bx, by, bz = OAR_BOSS_X, half_beam(OAR_BOSS_X), sheer_z(OAR_BOSS_X)
        parts.append(block("oar_boss", (0.36, 0.12, 0.28), (bx, by + 0.05, bz - 0.04), "MI_Wood_Varnish", 0.03))
        rake = math.radians(OAR_RAKE)
        c = (bx, by + OAR_OUT, bz + OAR_BOSS_H)
        parts.append(ring("oar_withy", 0.088, 0.024, c, (math.sin(rake), 0.0, math.cos(rake)), "MI_Rope_Hemp", 16))
        for dz in (-0.06, 0.04):
            parts.append(rod("withy_lash", (c[0], c[1] - 0.07, c[2] + dz), (bx, by + 0.1, bz + dz), 0.016, "MI_Rope_Hemp", 6))
    # Bitts fore and aft (mooring lines).
    for x in (4.0, -4.15):
        for y in (-0.22, 0.22):
            parts.append(post("bitt", 0.075, 0.5, (x, y, deck_z(x, y) - 0.03), "MI_Wood_Varnish", 4, 0.9))
        parts.append(block("bitt_bar", (0.09, 0.62, 0.09), (x, 0, deck_z(x, 0) + 0.3), "MI_Wood_Varnish", 0.02))
    return parts


# --- the mast and its fittings (D53) -------------------------------------------------------------------------
# The mast is a worked spar, not a turned pole (owner, D53): its head is bent a little aft and to port by years of the
# stay's and the rig's pull (SaltHullShape.h MastHeadAft / MastHeadPort move the pennant's swivel with it) and its middle
# is bowed a little.
MAST_HEAD_OFFSET = (-0.07, -0.02)       # Unreal metres at the truck: aft, to port
MAST_INFO = {}                          # filled by mast_and_fittings(): points other parts and ASaltBoat attach to


def mast_offset(h):
    """Unreal (dx, dy) of the mast's axis h metres above the deck."""
    head = smooth01((h - 6.95) / (MAST_H - 6.95))
    bow = math.sin(math.pi * clamp((h - 2.2) / 4.45))
    return MAST_HEAD_OFFSET[0] * head ** 1.4, MAST_HEAD_OFFSET[1] * head + 0.012 * bow


def mast_r(h):
    """Radius of the mast h metres above the deck (tb.spar's taper)."""
    t = clamp((h + 0.1) / (MAST_H + 0.1))
    return 0.13 + (0.085 - 0.13) * t ** 1.3


def axis_at(z):
    """Centre (Unreal x, y) of the mast at height z (absolute)."""
    dx, dy = mast_offset(z - sheer_z(MAST_X))
    return MAST_X + dx, dy


def girth_at(mast, z, half):
    """Largest radius of the hewn mast (knots and adze marks included) within z +- half: a band must clear it."""
    ax, ay = axis_at(z)
    rs = [math.hypot(v.co.x - ax, -v.co.y - ay) for v in mast.data.vertices if abs(v.co.z - z) < half + 0.051]
    return max(rs) if rs else mast_r(z - sheer_z(MAST_X))


def lathe(name, profile, center, mat, segments=24, axis=(0, 0, 1), phase=0.0):
    """A turned or cast piece: the profile [(radius, height)] revolved about the axis through center (Unreal)."""
    bm = bmesh.new()
    rings_ = []
    for r, z in profile:
        if r < 1e-5:
            rings_.append([bm.verts.new((0.0, 0.0, z))])
        else:
            rings_.append([bm.verts.new((r * math.cos(phase + 2 * math.pi * k / segments), r * math.sin(phase + 2 * math.pi * k / segments), z))
                           for k in range(segments)])
    for a, b in zip(rings_[:-1], rings_[1:]):
        if len(a) == 1 and len(b) == 1:
            continue
        if len(a) == 1:
            for k in range(segments):
                bm.faces.new((a[0], b[(k + 1) % segments], b[k]))
        elif len(b) == 1:
            for k in range(segments):
                bm.faces.new((a[k], a[(k + 1) % segments], b[0]))
        else:
            for k in range(segments):
                bm.faces.new((a[k], a[(k + 1) % segments], b[(k + 1) % segments], b[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object(name, bm, [mat])
    for f in obj.data.polygons:
        f.use_smooth = segments > 8
    return oriented(obj, center, axis)


def oval_torus(name, rx, ry, minor, center, mat, segments=28):
    """A ring round the mast that is wider fore and aft (rx) than across (ry): seizings and bands over fish splints."""
    obj = sb.add_torus(name, 1.0, minor, (0, 0, 0), (0, 0, 0), segments, 6, M[mat])
    for v in obj.data.vertices:
        d = math.hypot(v.co.x, v.co.y)
        if d > 1e-6:
            ux, uy = v.co.x / d, v.co.y / d
            off = d - 1.0                                  # the tube's own radius
            v.co.x, v.co.y = ux * (rx + off), uy * (ry + off)
    obj.data.transform(Matrix.Translation(U(center)))
    return obj


def iron_band(name, mast, z, h=0.06, over=0.0):
    """A forged band shrunk on round the mast, a little proud, with rolled edges; `over` widens it fore and aft (over
    fish splints)."""
    ax, ay = axis_at(z)
    r = girth_at(mast, z, h / 2) + 0.006
    band = sb.add_cylinder(name, 1.0, h, (0, 0, 0), "Z", 28, M["MI_Metal_Iron"])
    for v in band.data.vertices:
        v.co.x *= r + over
        v.co.y *= r
    band.data.transform(Matrix.Translation(U((ax, ay, z))))
    parts = [band]
    for dz in (-h / 2, h / 2):
        parts.append(oval_torus(name + "_edge", r + over, r, 0.006, (ax, ay, z + dz), "MI_Metal_Iron"))
    return parts


def eye_bolt(name, mast, z, ang, out=0.07):
    """An iron eye bolt driven into the mast at angle ang (0 = forward, pi/2 = starboard): shank, a washer on the wood
    and an eye standing out; returns (parts, the eye's centre)."""
    ax, ay = axis_at(z)
    r = girth_at(mast, z, 0.02)
    d = Vector((math.cos(ang), math.sin(ang), 0.0))
    base = Vector((ax, ay, z)) + d * (r - 0.01)
    eye = Vector((ax, ay, z)) + d * (r + out)
    parts = [rod(name + "_shank", tuple(base), tuple(eye - d * 0.02), 0.011, "MI_Metal_Iron", 8),
             disc(name + "_washer", 0.026, 0.008, tuple(Vector((ax, ay, z)) + d * (r + 0.004)), tuple(d), "MI_Metal_Iron", 10),
             ring(name, 0.022, 0.008, tuple(eye), (-d.y, d.x, 0.0), "MI_Metal_Iron", 12)]
    return parts, eye


def tackle_block(name, eye, toward):
    """A wooden block hanging from an eye: an iron hook in the eye, the strop round the shell, the sheave across;
    `toward` (Unreal, horizontal) turns the sheave to face the rope's lead. Returns (parts, the rope's point under it)."""
    eye = Vector(eye)
    c = eye - Vector((0.0, 0.0, 0.15))
    yaw = math.atan2(toward[1], toward[0])
    side = (-math.sin(yaw), math.cos(yaw), 0.0)        # sheave axle
    shell = sb.add_box(name, (0.08, 0.06, 0.13), (0, 0, 0), M["MI_Wood_Varnish"])
    sb.soften(shell, 0.025, 3)
    shell.data.transform(Matrix.Translation(U(tuple(c))) @ Matrix.Rotation(-yaw, 4, "Z"))
    parts = [shell,
             ring(name + "_strop", 0.07, 0.007, tuple(c), side, "MI_Metal_Iron", 16),
             disc(name + "_sheave", 0.046, 0.064, tuple(c - Vector((0, 0, 0.015))), side, "MI_Wood_TeakDark", 14, 0.004),
             rod(name + "_hook", tuple(eye - Vector((0, 0, 0.015))), tuple(c + Vector((0, 0, 0.07))), 0.009, "MI_Metal_Iron", 8),
             ring(name + "_hook_turn", 0.016, 0.007, tuple(eye - Vector((0, 0, 0.012))), (toward[0], toward[1], 0.0), "MI_Metal_Iron", 10)]
    return parts, c - Vector((0.0, 0.0, 0.075))


def mast_and_fittings():
    """The mast (D53, owner: "there is no way for a mast this perfect, also without any extras like iron rings"):
    - a hewn spar, its head worked aft, tarred at the foot, varnished, painted above the hounds;
    - stepped through partners with eight driven wedges and a canvas mast coat lashed round it;
    - an old repair just above the partners (where masts spring): fish splints fore and aft, rope seizings, two bands;
    - the bell on a hewn arm with a knee and an eye bolt (SM_Boat_Bell hangs there and swings), the lantern's forged
      hook on the aft face (SM_Boat_Lantern), each on its own band;
    - hounds cheeks carrying the shrouds' eyes (D65: below the yard, which hangs on the mast's front);
    - the forestay's eye; a masthead band with the yard's lifts' eye bolts; the halyard's sheave through the masthead
      (the yard hangs from it, SM_Boat_Yard); a round truck with the pennant's iron spindle;
    - pin rails inside the bulwark by the mast, where the halyard, the brails and the sheets are made fast (E sets and
      takes in the sail there)."""
    parts = []
    deck_m = sheer_z(MAST_X)
    top = deck_m + MAST_H
    mast = tb.spar("mast", MAST_H + 0.1, 0.13, 0.085, M["MI_Wood_Varnish"], nseed(), sides=36, sweep=0.0, axis=(0, 0, 1),
                   facets=8, checks=4)
    mast.data.transform(Matrix.Translation(U((MAST_X, 0, deck_m - 0.1))))
    for v in mast.data.vertices:
        dx, dy = mast_offset(v.co.z - deck_m)
        v.co.x += dx
        v.co.y -= dy                                       # Unreal -> Blender
    hounds_z = deck_m + HOUNDS_H
    mast.data.materials.append(M["MI_Wood_Tarred"])
    mast.data.materials.append(M["MI_Paint_CreamAged"])
    for poly in mast.data.polygons:
        zc = poly.center.z
        poly.material_index = 1 if zc < deck_m + 0.42 else (2 if zc > hounds_z + 0.14 else 0)
    parts.append(mast)

    # Partners: a square collar of four timbers round the mast, eight wedges driven between, the canvas coat above.
    ax0, ay0 = axis_at(deck_m + 0.1)
    r0 = girth_at(mast, deck_m + 0.1, 0.1)
    for s_ in (-1, 1):
        parts.append(tb.rough_plank("partner", 0.5, 0.1, 0.08, U((ax0, ay0 + s_ * 0.2, deck_m + 0.04)), (0, 0, 0),
                                    M["MI_Wood_Varnish"], nseed(), wear=0.6, grooves=(0, 1), split_chance=0.0))
        parts.append(tb.rough_plank("partner", 0.3, 0.1, 0.08, U((ax0 + s_ * 0.2, ay0, deck_m + 0.04)), (0, 0, math.pi / 2),
                                    M["MI_Wood_Varnish"], nseed(), wear=0.6, grooves=(0, 1), split_chance=0.0))
    for k in range(8):
        a = k * math.pi / 4 + math.pi / 8
        rw = r0 + 0.012
        c = (ax0 + rw * math.cos(a), ay0 + rw * math.sin(a), deck_m + 0.1)
        w = block("wedge", (0.03, 0.06, 0.13), c, "MI_Wood_TeakDark", 0.006)
        w.data.transform(Matrix.Translation(U(c)) @ Matrix.Rotation(-a, 4, "Z") @ Matrix.Translation(-U(c)))
        parts.append(w)
    parts.append(lathe("mast_coat", [(r0 + 0.07, 0.0), (r0 + 0.055, 0.035), (r0 + 0.03, 0.09), (r0 + 0.012, 0.15), (r0 + 0.006, 0.17)],
                       (ax0, ay0, deck_m + 0.15), "MI_Canvas", 28))
    parts.append(sb.add_torus("coat_lashing", r0 + 0.016, 0.011, U((ax0, ay0, deck_m + 0.3)), (0, 0, 0), 28, 6, M["MI_Rope_Hemp"]))

    # The old repair: fish splints fore and aft, seized at both ends, a band over each end.
    zf0, zf1 = deck_m + 0.46, deck_m + 1.24
    axf, ayf = axis_at((zf0 + zf1) / 2)
    rf = girth_at(mast, (zf0 + zf1) / 2, 0.4)
    for s_ in (-1, 1):
        parts.append(tb.rough_plank("fish", zf1 - zf0, 0.075, 0.03, U((axf + s_ * (rf + 0.01), ayf, (zf0 + zf1) / 2)), (0, math.pi / 2, 0),
                                    M["MI_Wood_DockWeathered"], nseed(), wear=0.8, grooves=(0, 1), split_chance=0.25))
        for zc in (zf0 + 0.1, zf1 - 0.1):
            parts.append(nail((axf + s_ * (rf + 0.026), ayf, zc), (s_, 0, 0)))
    for zc in (zf0 + 0.2, zf0 + 0.235, zf0 + 0.27, zf1 - 0.27, zf1 - 0.235, zf1 - 0.2):
        axs, ays = axis_at(zc)
        rs = girth_at(mast, zc, 0.02)
        parts.append(oval_torus("fish_seizing", rf + 0.04, rs + 0.012, 0.012, (axs, ays, zc), "MI_Rope_Hemp"))
    parts += iron_band("band_fish", mast, zf0 + 0.06, h=0.05, over=0.035)
    parts += iron_band("band_fish", mast, zf1 - 0.06, h=0.05, over=0.035)

    # The bell: a hewn arm forward, a band holding its heel to the mast, a grown knee under it, an eye bolt through
    # its end with a small iron ring; SM_Boat_Bell's crown hangs in that ring.
    za = deck_m + 1.6
    axb, ayb = axis_at(za)
    rb = girth_at(mast, za, 0.06)
    parts += iron_band("band_bell", mast, za, h=0.1)
    x_front = axb + rb
    parts.append(tb.rough_plank("bell_arm", 0.5, 0.07, 0.085, U((x_front + 0.22, ayb, za)), (0, 0, 0), M["MI_Wood_Varnish"], nseed(),
                                wear=0.6, grooves=(1, 1), split_chance=0.0))
    parts.append(lathe("bell_arm_knob", [(0.0, 0.0), (0.032, 0.004), (0.045, 0.025), (0.04, 0.048), (0.024, 0.062), (0.0, 0.066)],
                       (x_front + 0.465, ayb, za), "MI_Wood_Varnish", 16, axis=(1, 0, 0)))
    R = 0.22

    def knee(sf):
        th_ = math.pi - (math.pi / 2) * sf
        cx_, cz_ = x_front + 0.035 + R, za - 0.04 - R
        return (Vector((cx_ + R * math.cos(th_), ayb, cz_ + R * math.sin(th_))), Vector((0.0, 1.0, 0.0)),
                Vector((math.cos(th_), 0.0, math.sin(th_))), 1.0)
    parts.append(laid_board("bell_knee", path_length(knee), 0.06, 0.07, knee, "MI_Wood_Varnish", wear=0.6, grooves=(1, 1)))
    parts.append(block("knee_foot", (0.05, 0.08, 0.1), (x_front + 0.02, ayb, za - 0.04 - R + 0.03), "MI_Wood_Varnish", 0.012))
    parts.append(nail((x_front + 0.046, ayb, za - 0.04 - R + 0.03), (1, 0, 0)))
    xe = x_front + 0.36
    parts.append(rod("bell_eyebolt", (xe, ayb, za + 0.055), (xe, ayb, za - 0.07), 0.009, "MI_Metal_Iron", 8))
    parts.append(disc("bell_nut", 0.018, 0.012, (xe, ayb, za + 0.048), (0, 0, 1), "MI_Metal_Iron", 6))
    parts.append(ring("bell_eye", 0.014, 0.006, (xe, ayb, za - 0.084), (1, 0, 0), "MI_Metal_Iron", 12))
    parts.append(ring("bell_ring", 0.02, 0.006, (xe, ayb, za - 0.098 - 0.006), (0, 1, 0), "MI_Metal_Iron", 14))
    MAST_INFO["bell_pivot"] = (xe, ayb, za - 0.106 - deck_m)       # the crown loop round the ring's wire

    # The lantern: a forged arm out of a band on the aft face, its tip turned up into a hook.
    zl = deck_m + 1.76
    axl, ayl = axis_at(zl)
    rl = girth_at(mast, zl, 0.04)
    parts += iron_band("band_lantern", mast, zl, h=0.06)
    x_back = axl - rl - 0.006
    parts.append(rod("lantern_arm", (x_back + 0.01, ayl, zl), (x_back - 0.2, ayl, zl), 0.012, "MI_Metal_Iron", 8))
    parts.append(lathe("lantern_arm_end", [(0.0, 0.0), (0.016, 0.0), (0.02, 0.012), (0.0, 0.022)], (x_back - 0.2, ayl, zl), "MI_Metal_Iron", 10,
                       axis=(-1, 0, 0)))
    parts.append(rod("lantern_hook", (x_back - 0.15, ayl, zl - 0.005), (x_back - 0.15, ayl, zl - 0.045), 0.008, "MI_Metal_Iron", 8))
    parts.append(ring("lantern_hook_curl", 0.018, 0.007, (x_back - 0.15 + 0.018, ayl, zl - 0.05), (0, 1, 0), "MI_Metal_Iron", 12))
    MAST_INFO["lantern_pivot"] = (x_back - 0.15 + 0.018, ayl, zl - 0.056 - deck_m)   # the handle round the hook's wire

    # Hounds: cheeks on both sides, the shrouds' eyes over them; a band under them.
    axh, ayh = axis_at(hounds_z - 0.24)
    rh = girth_at(mast, hounds_z - 0.24, 0.24)
    for s_ in (-1, 1):
        parts.append(tb.rough_plank("hounds_cheek", 0.44, 0.065, 0.08, U((axh, ayh + s_ * (rh + 0.026), hounds_z - 0.24)), (0, math.pi / 2, 0),
                                    M["MI_Wood_Varnish"], nseed(), wear=0.6, grooves=(0, 1), split_chance=0.0))
        for zc in (hounds_z - 0.38, hounds_z - 0.1):
            parts.append(nail((axh, ayh + s_ * (rh + 0.06), zc), (0, s_, 0)))
    for k in range(2):
        ze = hounds_z + 0.035 + k * 0.045
        axe, aye = axis_at(ze)
        parts.append(sb.add_torus("shroud_eyes", girth_at(mast, ze, 0.03) + 0.03, 0.021, U((axe, aye, ze)), (0, 0, 0), 28, 8, M["MI_Rope_Hemp"]))
    MAST_INFO["hounds"] = lambda side: (axh, ayh + side * (rh + 0.07), hounds_z + 0.06)

    # Masthead (D65): the forestay's eye just under the truck; the halyard's sheave through the mast below it (the yard
    # hangs from the halyard, which comes out of the mast's back and goes down to the starboard pin rail); a band with the
    # lifts' eye bolts, one either side, below the sheave.
    zfe = top - 0.12
    axe, aye = axis_at(zfe)
    rfe = girth_at(mast, zfe, 0.03)
    parts.append(sb.add_torus("forestay_eye", rfe + 0.026, 0.021, U((axe, aye, zfe)), (0, 0, 0), 28, 8, M["MI_Rope_Hemp"]))
    MAST_INFO["forestay"] = (axe + rfe + 0.06, aye, zfe - 0.01)
    stay = (Vector((HALF_LENGTH + 0.02, 0.0, sheer_z(HALF_LENGTH) + 0.55)) - Vector(MAST_INFO["forestay"])).normalized()
    for k in range(3):                                  # the eye's seizing, just below the mast
        parts.append(ring("forestay_seizing", 0.032, 0.009, tuple(Vector(MAST_INFO["forestay"]) + stay * (0.08 + k * 0.035)), tuple(stay),
                          "MI_Rope_Hemp", 12))
    zh = top - 0.36
    axh_, ayh_ = axis_at(zh)
    rh_ = girth_at(mast, zh, 0.12)
    for s_ in (-1, 1):                                  # the sheave's slot, cut fore and aft through the masthead
        slot = block("sheave_slot", (0.03, 0.05, 0.22), (axh_ + s_ * (rh_ - 0.012), ayh_, zh), "MI_Caulk_Dark", 0.006)
        parts.append(slot)
        parts.append(disc("sheave_pin", 0.022, 0.012, (axh_, ayh_ + s_ * (rh_ + 0.002), zh), (0, 1, 0), "MI_Metal_Iron", 10, 0.003))
    parts.append(disc("sheave", 0.092, 0.036, (axh_, ayh_, zh), (0, 1, 0), "MI_Wood_TeakDark", 24, 0.006))
    # Where the halyard runs over the sheave: up into the slot in front, over the top, out at the back.
    over = [(axh_ + 0.078 * math.cos(a), ayh_, zh + 0.078 * math.sin(a)) for a in [math.radians(d) for d in range(0, 181, 30)]]
    MAST_INFO["halyard_over"] = over
    zb_ = top - 0.6
    parts += iron_band("band_head", mast, zb_, h=0.07)
    for s_ in (-1, 1):
        p_, eye = eye_bolt("lift_eye", mast, zb_, s_ * math.radians(70))
        parts += p_
        MAST_INFO["lift_eye", s_] = tuple(eye)

    # The truck, with the pennant's iron spindle (SM_Boat_Pennant swivels on it, D44).
    axt, ayt = axis_at(top)
    parts.append(lathe("truck", [(0.0, -0.02), (0.1, -0.02), (0.115, 0.01), (0.11, 0.045), (0.075, 0.07), (0.0, 0.078)],
                       (axt, ayt, top), "MI_Wood_Varnish", 24))
    parts.append(rod("pennant_spindle", (axt, ayt, top + 0.05), (axt, ayt, top + PENNANT_H + 0.06), 0.012, "MI_Metal_Iron", 8))
    parts.append(ring("spindle_knob", 0.014, 0.013, (axt, ayt, top + PENNANT_H + 0.07), (0, 0, 1), "MI_Metal_Iron", 10))

    # Pin rails inside the bulwark by the mast (D65): three belaying pins each. Aft pins: the brails' falls; middle pin: the
    # halyard's fall (starboard) or a spare coil (port); forward pins: the sheets (ASaltBoat's ropes from the clews). Each line
    # is made fast with a few turns round its pin, the rest coiled and hung over it inboard.
    for side in (-1, 1):
        xr = MAST_X + 0.35
        yr = side * (half_beam(xr) - BULWARK_T - 0.055)
        zr = sheer_z(xr) + 0.36
        parts.append(tb.rough_plank("pinrail", 0.62, 0.09, 0.06, U((xr, yr, zr)), (0, 0, 0), M["MI_Wood_Varnish"], nseed(),
                                    wear=0.5, grooves=(1, 1), split_chance=0.0))
        for xb in (xr - 0.22, xr + 0.22):                # knees under it, against the bulwark
            parts.append(block("pinrail_knee", (0.05, 0.05, 0.16), (xb, yr + side * 0.02, zr - 0.11), "MI_Wood_Varnish", 0.01))
        for k, xp in enumerate((xr - 0.17, xr, xr + 0.17)):
            parts.append(rod("belaying_pin", (xp, yr, zr - 0.09), (xp, yr, zr + 0.16), 0.014, "MI_Wood_TeakDark", 8))
            parts.append(lathe("pin_head", [(0.0, 0.0), (0.02, 0.0), (0.024, 0.03), (0.016, 0.05), (0.0, 0.055)], (xp, yr, zr + 0.16),
                               "MI_Wood_TeakDark", 10))
            MAST_INFO["pin", side, k] = (xp, yr, zr + 0.12)
            for t in range(3):
                parts.append(ring("pin_turn", 0.024, 0.009, (xp, yr, zr + 0.045 + t * 0.022), (0.0, 0.0, 1.0), "MI_Rope_Hemp", 10))
            for t in range(3):
                parts.append(ring("pin_coil", 0.075 - t * 0.008, 0.012, (xp + 0.005 * t, yr - side * (0.055 + 0.012 * k), zr - 0.04 - t * 0.012),
                                  (0.0, 1.0, 0.0), "MI_Rope_Hemp", 20))
    print("SB_MAST bell", MAST_INFO["bell_pivot"], "lantern", MAST_INFO["lantern_pivot"], "head offset", MAST_HEAD_OFFSET)
    print("SB_RIG sheet pins", MAST_INFO["pin", -1, 2], MAST_INFO["pin", 1, 2])
    return parts


# --- lived-in gear (D40) ------------------------------------------------------------------------------------
def staved(name, radius_at, height, staves, base, mat, hoops=(), hoop_mat="MI_Metal_Iron"):
    """Barrel or bucket from bent staves around radius_at(s) (s 0 bottom .. 1 top), with hoops and a bottom."""
    parts = []
    r_max = max(radius_at(i / 10) for i in range(11))
    w0 = 2 * math.pi * r_max / staves
    bx, by, bz = base
    for k in range(staves):
        a = 2 * math.pi * (k + 0.5) / staves

        def f(s, a=a):
            r = radius_at(s)
            ds = 0.01
            dr = (radius_at(min(s + ds, 1.0)) - radius_at(max(s - ds, 0.0))) / (2 * ds * height)
            radial = Vector((math.cos(a), math.sin(a), 0.0))
            n = (radial - Vector((0.0, 0.0, dr))).normalized()
            return (Vector((bx, by, bz + height * s)) + radial * (r - 0.012), Vector((-math.sin(a), math.cos(a), 0.0)), n,
                    r / r_max)
        parts.append(laid_board(name, height, w0 - 0.006, 0.024, f, mat, wear=0.4, grooves=(0, 1), bevel=0.6))
    for h in hoops:
        parts.append(sb.add_torus(name + "_hoop", radius_at(h) + 0.006, 0.012, U((bx, by, bz + height * h)), (0, 0, 0), 32, 6, M[hoop_mat]))
    parts.append(sb.add_cylinder(name + "_bottom", radius_at(0.04) - 0.01, 0.03, U((bx, by, bz + 0.04)), "Z", 16, M[mat]))
    return parts


def deck_gear():
    """Pin rail with belaying pins and hanging coils at the mast, a lashed water barrel and crate (both open as stores,
    D52), oars on the side deck, fenders over the pier side and horn cleats fore and aft."""
    parts = []
    # Pin rail aft of the mast.
    px = MAST_X - 0.42
    dz0 = deck_z(px, 0.0)
    for y in (-0.42, 0.42):
        parts.append(post("pinrail_post", 0.05, 0.62, (px, y, dz0 - 0.03), "MI_Wood_Varnish", 4, 0.9))
    parts.append(tb.rough_plank("pinrail", 1.06, 0.12, 0.07, U((px, 0.0, dz0 + 0.6)), (0, 0, math.pi / 2), M["MI_Wood_Varnish"], nseed(),
                                wear=0.4, grooves=(1, 1), split_chance=0.0))
    for k, y in enumerate((-0.33, -0.17, 0.0, 0.17, 0.33)):
        parts.append(rod("pin", (px, y, dz0 + 0.5), (px, y, dz0 + 0.78), 0.016, "MI_Wood_Varnish"))
        parts.append(sb.add_cylinder("pin_head", 0.022, 0.05, U((px, y, dz0 + 0.78)), "Z", 10, M["MI_Wood_Varnish"]))
        if k % 2 == 0:
            for j in range(3):
                coil = sb.add_torus("hang_coil", 0.13 - j * 0.012, 0.016, U((px - 0.03, y, dz0 + 0.42 - j * 0.01)),
                                    (math.pi / 2, 0, math.pi / 2 + j * 0.25), 24, 6, M["MI_Rope_Hemp"])
                parts.append(coil)
    # Water barrel and crate, port side aft of the gap, lashed together.
    bx, by, br, bh = BARREL
    k = br / 0.21                                       # D58: 30 % bigger than D52's barrel
    parts += staved("barrel", lambda s: br + 0.045 * k * math.sin(math.pi * s), bh, 14, (bx, by, deck_z(bx, by) - 0.01), "MI_Wood_Varnish",
                    hoops=(0.12, 0.3, 0.7, 0.88))
    parts.append(sb.add_cylinder("barrel_lid", br - 0.01 * k, 0.03 * k, U((bx, by, deck_z(bx, by) + bh - 0.03 * k)), "Z", 22, M["MI_Wood_Teak"]))
    parts.append(block("barrel_lid_handle", (0.16 * k, 0.035 * k, 0.035 * k), (bx, by, deck_z(bx, by) + bh + 0.002), "MI_Wood_TeakDark", 0.01))
    # Lashed to an eye bolt on the bulwark, so it does not walk about in a sea.
    yb = -(half_beam(bx) - BULWARK_T) + 0.006
    zb = deck_z(bx, by) + 0.42 * k
    rl = br + 0.046 * k                                 # the lashing round its belly
    parts.append(block("eye_plate", (0.08, 0.012, 0.08), (bx, yb, zb), "MI_Metal_Iron", 0.004))
    parts.append(ring("eye_bolt", 0.025, 0.007, (bx, yb + 0.03, zb), (1, 0, 0), "MI_Metal_Iron", 12))
    parts.append(sb.add_torus("barrel_lashing", rl, 0.012, U((bx, by, zb)), (0, 0, 0), 32, 6, M["MI_Rope_Hemp"]))
    parts += rope("barrel_lashing", [(bx, by - rl, zb), (bx, yb + 0.03, zb - 0.02)], 0.012)
    cx, cy, cl, cw, ch = CRATE
    cz = deck_z(cx, cy)
    row = (ch - 0.025) / 3                              # three boards high, the lid on top
    for w, rot, off in ((cl, (math.pi / 2, 0, 0), (0, cw / 2 - 0.0125)), (cl, (math.pi / 2, 0, 0), (0, -cw / 2 + 0.0125)),
                        (cw - 0.025, (math.pi / 2, 0, math.pi / 2), (cl / 2 - 0.0125, 0)), (cw - 0.025, (math.pi / 2, 0, math.pi / 2), (-cl / 2 + 0.0125, 0))):
        for r in range(3):
            parts.append(tb.rough_plank("crate", w, row - 0.005, 0.025, U((cx + off[0], cy + off[1], cz + 0.01 + (r + 0.5) * row)), rot, M["MI_Wood_Teak"], nseed(),
                                        wear=0.6, grooves=(0, 1), split_chance=0.2))
    lid_w = (cw + 0.02) / 4
    for r in range(4):
        parts.append(tb.rough_plank("crate_lid", cl + 0.02, lid_w - 0.005, 0.025, U((cx, cy - (cw + 0.02) / 2 + (r + 0.5) * lid_w, cz + ch)), (0, 0, 0),
                                    M["MI_Wood_Teak"], nseed(), wear=0.6, grooves=(0, 1), split_chance=0.2))
    parts += rope("lashing", [(bx - br * 0.9, by - br * 0.3, deck_z(bx, by) + 0.45), (bx - br * 0.4, by - br * 1.1, deck_z(bx, by) + 0.5),
                              (cx + cl * 0.2, cy - cw * 0.6, cz + ch * 0.8), (cx - cl * 0.5 - 0.01, cy - cw * 0.25, cz + ch * 0.8)], 0.014)
    # The crate: rope handles through its ends, iron straps on its corners.
    for dx in (-cl / 2, cl / 2):
        parts.append(ring("crate_handle", 0.06, 0.012, (cx + dx * 1.04, cy, cz + ch * 0.72), (1, 0, 0), "MI_Rope_Hemp", 12))
        for dy in (-cw / 2, cw / 2):
            parts.append(block("crate_corner", (0.036, 0.036, ch), (cx + dx, cy + dy, cz + ch / 2 + 0.01), "MI_Metal_Iron", 0.005))
    # Oars lying along the port side deck.
    for k, y in enumerate((-1.08, -0.96)):
        x0, x1 = 0.7 + k * 0.1, 3.3 + k * 0.05
        z = deck_z((x0 + x1) / 2, y) + 0.04 + k * 0.035
        loom = tb.spar("oar", x1 - x0, 0.03, 0.026, M["MI_Wood_Varnish"], nseed(), sides=16, sweep=0.012, axis=(1, 0, 0), facets=6, checks=0)
        loom.data.transform(Matrix.Translation(U((x0, y, z))))
        parts.append(loom)
        parts.append(tb.rough_plank("oar_blade", 0.7, 0.14, 0.022, U((x1 + 0.3, y, z)), (0, 0, 0), M["MI_Wood_Varnish"], nseed(),
                                    wear=0.5, grooves=(1, 1), split_chance=0.1))
        for xl in (1.2, 2.9):                           # seized at two places...
            for j in range(3):
                parts.append(ring("oar_seizing", 0.038, 0.007, (xl - 0.02 + j * 0.02, y, z), (1, 0, 0), "MI_Rope_Hemp", 12))
    for xl in (1.2, 2.9):                               # ...and made fast to an eye on the bulwark
        yi = -(half_beam(xl) - BULWARK_T)
        ze = sheer_z(xl) + 0.2
        parts.append(block("oar_eye_plate", (0.07, 0.012, 0.07), (xl, yi + 0.006, ze), "MI_Metal_Iron", 0.004))
        parts.append(ring("oar_eye", 0.022, 0.007, (xl, yi + 0.03, ze), (1, 0, 0), "MI_Metal_Iron", 12))
        parts += rope("oar_lashing", [(xl, -1.115, deck_z(xl, -1.08) + 0.045), (xl, yi + 0.03, ze - 0.02)], 0.009)
    # Fenders over the pier (starboard) side.
    for fx in (-1.8, 1.8):
        bs = half_beam(fx)
        top = (fx, bs + 0.02, sheer_z(fx) + BULWARK_H + 0.07)
        fy = bs + 0.14
        fz = sheer_z(fx) - 0.3
        fender = sb.add_cylinder("fender", 0.11, 0.44, U((fx, fy, fz)), "Z", 14, M["MI_Rope_Hemp"])
        for f in fender.data.polygons:
            f.use_smooth = True
        sb.soften(fender, 0.04, 3)
        parts.append(fender)
        parts += rope("fender_line", [(fx, fy, fz + 0.22), (fx, bs + 0.1, sheer_z(fx) + 0.3), top], 0.012)
    # Windlass on the foredeck: two cheek posts (the drum, its rope and the cranks are SM_Boat_WindlassDrum /
    # SM_Boat_WindlassRope, turned by ASaltBoat). The anchor rope runs from the drum along the starboard side into a
    # hawse pipe in the deck, and comes out of a hawse hole in the bow planking (ASaltBoat's Hawse point, D52); the anchor
    # itself is never seen.
    wx = WINDLASS_X
    wz = deck_z(wx, 0.0)
    for y in (-0.5, 0.5):
        parts.append(post("windlass_cheek", 0.07, 0.62, (wx, y, wz - 0.03), "MI_Wood_Varnish", 4, 0.9))
        parts.append(ring("windlass_bearing", 0.05, 0.016, (wx, y * 0.9, wz + WINDLASS_DRUM_H), (0, 1, 0), "MI_Metal_Iron", 12))
    # The pawl: an iron bar pivoted on the starboard cheek, its tip in the ratchet's teeth (it holds the anchor rope).
    pz = wz + WINDLASS_DRUM_H
    parts.append(disc("pawl_bolt", 0.022, 0.05, (wx - 0.05, 0.415, pz + 0.235), (0, 1, 0), "MI_Metal_Iron", 8))
    parts.append(rod("pawl", (wx - 0.05, 0.4, pz + 0.235), (wx - 0.018, 0.4, pz + 0.172), 0.014, "MI_Metal_Iron", 8))
    hx, hy = HAWSE_DECK
    hz = deck_z(hx, hy)
    lead_a = (wx + 0.08, 0.36, wz + WINDLASS_DRUM_H + 0.1)
    parts += rope("anchor_lead", sagging(lead_a, (hx - 0.04, hy, hz + 0.03), 0.04, 6) + [(hx, hy, hz - 0.06)], 0.022)
    parts.append(ring("hawse_pipe", 0.05, 0.016, (hx, hy, hz + 0.005), (0, 0, 1), "MI_Metal_Iron", 14))
    parts.append(disc("hawse_hole", 0.045, 0.012, (hx, hy, hz - 0.001), (0, 0, 1), "MI_Caulk_Dark", 14))
    ox, oz = HAWSE_HULL
    out = Vector((0.35, 1.0, 0.0)).normalized()
    zs_, zb_ = sheer_z(ox), bottom_z(ox)
    th_ = math.acos(clamp((zs_ - oz) / (zs_ - zb_)))
    hp = (ox, half_beam(ox) * math.sin(th_) ** BILGE_EXP + PLANK_T + 0.008, oz)     # on the planking
    parts.append(ring("hawse_ring", 0.06, 0.018, hp, tuple(out), "MI_Metal_Iron", 16))
    parts.append(disc("hawse_hole", 0.05, 0.02, tuple(Vector(hp) - out * 0.006), tuple(out), "MI_Caulk_Dark", 14))
    # Boarding ladders hanging over both sides at the gap: two stiles, rungs every 0.3 m, iron hooks over the edge.
    for side in (-1, 1):
        y = side * (half_beam(LADDER_X) + LADDER_OUT)
        top = sheer_z(LADDER_X) + 0.08
        for xs in (LADDER_X - 0.22, LADDER_X + 0.22):
            def stile(sf, xs=xs):
                z = top + (LADDER_BOTTOM_Z - top) * sf
                return Vector((xs, y, z)), Vector((1.0, 0.0, 0.0)), Vector((0.0, side, 0.0)), 1.0
            parts.append(laid_board("ladder_stile", top - LADDER_BOTTOM_Z, 0.07, 0.045, stile, "MI_Wood_Varnish", wear=0.7, grooves=(1, 1)))
            hook = (xs, y, top - 0.02)
            parts.append(rod("ladder_hook", hook, (xs, y - side * 0.2, top + 0.06), 0.014, "MI_Metal_Iron"))
            parts.append(rod("ladder_hook", (xs, y - side * 0.2, top + 0.06), (xs, y - side * 0.24, top - 0.04), 0.014, "MI_Metal_Iron"))
        z = top - 0.2
        while z > LADDER_BOTTOM_Z + 0.08:
            parts.append(rod("ladder_rung", (LADDER_X - 0.2, y, z), (LADDER_X + 0.2, y, z), 0.022, "MI_Wood_Varnish"))
            z -= RUNG_STEP
    # Repair: a weathered board nailed over the starboard planking aft, just above the waterline.
    th = strake_bounds()
    pa_r, pb_r = th[4], th[5]

    def patch_frame(sfrac):
        x = -2.55 + 0.6 * sfrac
        pa, pb = Vector(section_point(x, pa_r, 1)), Vector(section_point(x, pb_r, 1))
        a = (pb - pa).normalized()
        n = Vector((0.0, a.z, -a.y))
        if n.y < 0.0:
            n = -n
        return (pa + pb) * 0.5 + n * (PLANK_T + 0.012), a, n, 1.0
    parts.append(laid_board("repair", 0.6, 0.2, 0.022, patch_frame, "MI_Wood_DockWeathered", wear=0.9, grooves=(1, 2), split=0.5))
    for sf in (0.08, 0.92):
        pm, am, nm, _ = patch_frame(sf)
        for off in (-0.07, 0.07):
            parts.append(nail(tuple(pm + nm * 0.011 + am * off), tuple(nm)))

    # Horn cleats on the covering boards fore and aft.
    for cxx in (3.55, -3.55):
        for side in (-1, 1):
            bs = half_beam(cxx)
            y = side * (bs - BULWARK_T - 0.1)
            z = deck_z(cxx, y)
            parts.append(block("cleat_base", (0.12, 0.06, 0.06), (cxx, y, z + 0.03), "MI_Wood_Varnish", 0.012))
            parts.append(rod("cleat_horn", (cxx - 0.15, y, z + 0.075), (cxx + 0.15, y, z + 0.075), 0.022, "MI_Wood_Varnish"))
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
    # Keel (aft and forward half), mast foot, pedestal, bitts.
    for a, b in ((KEEL_AFT_X, 0.0), (0.0, KEEL_FORE_X)):
        pts = []
        for i in range(7):
            x = a + (b - a) * i / 6
            for y in (-KEEL_HALF_T, KEEL_HALF_T):
                pts += [(x, y, keel_z(x)), (x, y, bottom_z(x) + 0.1)]
        add(pts)
    deck_m = sheer_z(MAST_X)
    add([(MAST_X + 0.14 * math.cos(a), 0.14 * math.sin(a), z) for a in [i * math.pi / 4 for i in range(8)] for z in (deck_m - 0.1, deck_m + 2.6)])
    if STEERING == "wheel":
        hub_z = sheer_z(WHEEL_X) + WHEEL_H
        add([(WHEEL_X + dx, dy, z) for dx in (-0.15, 0.15) for dy in (-0.15, 0.15) for z in (sheer_z(WHEEL_X) - 0.05, hub_z + 0.04)])
    for x in (4.0, -4.15):
        add([(x + dx, dy, z) for dx in (-0.08, 0.08) for dy in (-0.32, 0.32) for z in (deck_z(x, 0) - 0.05, deck_z(x, 0) + 0.36)])
    bx, by, br, bh = BARREL
    add([(bx + (br + 0.04) * math.cos(a), by + (br + 0.04) * math.sin(a), z) for a in [i * math.pi / 4 for i in range(8)]
         for z in (deck_z(bx, by) - 0.03, deck_z(bx, by) + bh)])
    cx, cy, cl, cw, ch = CRATE
    add([(cx + dx, cy + dy, z) for dx in (-cl / 2 - 0.02, cl / 2 + 0.02) for dy in (-cw / 2, cw / 2)
         for z in (deck_z(cx, cy) - 0.03, deck_z(cx, cy) + ch + 0.02)])
    px = MAST_X - 0.42
    add([(px + dx, dy, z) for dx in (-0.07, 0.07) for dy in (-0.5, 0.5) for z in (deck_z(px, 0) - 0.03, deck_z(px, 0) + 0.66)])
    wx = WINDLASS_X
    add([(wx + dx, dy, z) for dx in (-0.14, 0.14) for dy in (-0.58, 0.58) for z in (deck_z(wx, 0) - 0.03, deck_z(wx, 0) + 0.6)])
    # Everything on deck collides (D53): net heap, sea chest, ensign staff, the oars, the shroud pin rails, the boat hook,
    # the mast's partners and coat, the bell's arm and knee, the lantern's arm.
    for (cx_, cy_, hx_, hy_, h_) in ((-2.3, 0.85, 0.5, 0.38, 0.32), (CHEST[0], CHEST[1], CHEST[2] / 2 + 0.04, CHEST[3] / 2 + 0.04, CHEST[4] + 0.1),
                                     (ENSIGN_X, 0.0, 0.07, 0.07, ENSIGN_H),
                                     (2.3, -1.02, 1.7, 0.1, 0.13)):
        add([(cx_ + dx, cy_ + dy, z) for dx in (-hx_, hx_) for dy in (-hy_, hy_) for z in (deck_z(cx_, cy_) - 0.03, deck_z(cx_, cy_) + h_)])
    for side in (-1, 1):
        xr = MAST_X + 0.35
        yr = side * (half_beam(xr) - BULWARK_T - 0.055)
        add([(xr + dx, yr + dy, z) for dx in (-0.33, 0.33) for dy in (-0.06, 0.06) for z in (sheer_z(xr) + 0.2, sheer_z(xr) + 0.55)])
    yh = half_beam(2.0) - BULWARK_T - 0.05
    add([(x, yh + dy, z) for x in (0.95, 3.45) for dy in (-0.045, 0.045) for z in (sheer_z(2.0) + 0.03, sheer_z(2.0) + 0.17)])
    add([(MAST_X + 0.26 * math.cos(a), 0.26 * math.sin(a), z) for a in [i * math.pi / 4 for i in range(8)] for z in (deck_m - 0.03, deck_m + 0.32)])
    xf = MAST_X + 0.11
    add([(x, dy, z) for dy in (-0.045, 0.045) for (x, z) in ((xf, deck_m + 1.3), (xf, deck_m + 1.65), (xf + 0.56, deck_m + 1.55), (xf + 0.56, deck_m + 1.65))])
    xa = MAST_X - 0.12
    add([(x, dy, z) for x in (xa, xa - 0.22) for dy in (-0.025, 0.025) for z in (deck_m + 1.7, deck_m + 1.79)])
    # Ladders: thin slabs, so a swimmer or a walker does not pass through them.
    for side in (-1, 1):
        y = side * (half_beam(LADDER_X) + LADDER_OUT)
        add([(LADDER_X + dx, y + dy, z) for dx in (-0.26, 0.26) for dy in (-0.03, 0.03) for z in (LADDER_BOTTOM_Z, sheer_z(LADDER_X) + 0.08)])
    return cols


# --- weathering (D40) -----------------------------------------------------------------------------------------
def smooth01(t):
    t = clamp(t)
    return t * t * (3.0 - 2.0 * t)


def weather(obj, chips=0.85, deck_grime=True, seed=0):
    """Breaks up the baked vertex masks so no part is one perfect colour (owner: "everything looks too perfect"):
    B tone = the part's own value blended with blotches 0.4-1.5 m across; R wear = the bevel wear broken up, plus
    scattered chips where paint or varnish is knocked off; G grime = dirt along the deck corners (feet of bulwarks,
    posts and fittings) and rain streaks down the topsides."""
    from mathutils import noise as mnoise
    me = obj.data
    col = me.color_attributes["Col"]
    n_l, n_v = len(me.loops), len(me.vertices)
    data = [0.0] * (n_l * 4)
    col.data.foreach_get("color", data)
    co = [0.0] * (n_v * 3)
    me.vertices.foreach_get("co", co)
    vi = [0] * n_l
    me.loops.foreach_get("vertex_index", vi)
    # Chips knock paint off (hull, cream); wood only shows a faint trace, rope and metal none.
    chip_scale = []
    for m in me.materials:
        name = m.name if m else ""
        chip_scale.append(1.0 if "Paint" in name else (0.2 if "Wood" in name else 0.0))
    loop_chip = [0.0] * n_l
    starts, totals, mat_idx = [0] * len(me.polygons), [0] * len(me.polygons), [0] * len(me.polygons)
    me.polygons.foreach_get("loop_start", starts)
    me.polygons.foreach_get("loop_total", totals)
    me.polygons.foreach_get("material_index", mat_idx)
    for pi in range(len(starts)):
        c = chip_scale[mat_idx[pi]] if mat_idx[pi] < len(chip_scale) else 0.0
        for li in range(starts[pi], starts[pi] + totals[pi]):
            loop_chip[li] = c
    off = Vector((seed * 3.7, seed * 1.3, seed * 2.1))
    tone, chip, wear_mod, grime = [0.0] * n_v, [0.0] * n_v, [0.0] * n_v, [0.0] * n_v
    for v in range(n_v):
        x, yb, z = co[3 * v], co[3 * v + 1], co[3 * v + 2]
        y = -yb                                              # Unreal y
        p = Vector((x, yb, z)) + off
        tone[v] = 0.5 + 0.5 * (0.65 * mnoise.noise(p * 1.4) + 0.35 * mnoise.noise(p * 3.1 + Vector((5.0, 1.0, 2.0))))
        chip[v] = smooth01((mnoise.noise(p * 6.5 + Vector((13.1, 7.7, 3.3))) - 0.42) / 0.12) * chips
        wear_mod[v] = 0.45 + 0.55 * (0.5 + 0.5 * mnoise.noise(p * 2.7 + Vector((2.0, 9.0, 4.0))))
        g = 0.0
        if deck_grime and abs(x) < HALF_LENGTH:
            bs = half_beam(x)
            if abs(y) < bs + 0.02:
                dz = z - deck_z(x, min(abs(y), bs))
                if -0.06 < dz < 0.16:
                    g = max(g, smooth01((0.16 - dz) / 0.16) * (0.4 + 0.6 * (0.5 + 0.5 * mnoise.noise(p * 2.0))))
            if 0.05 < z < sheer_z(x) and abs(y) > bs * 0.8:
                streak = 0.5 + 0.5 * mnoise.noise(Vector((x * 3.0, yb * 3.0, z * 0.3)) + off)
                g = max(g, smooth01((streak - 0.58) / 0.22) * 0.65)
        grime[v] = g
    for li in range(n_l):
        v = vi[li]
        k = 4 * li
        data[k] = max(data[k] * wear_mod[v], chip[v] * loop_chip[li])
        data[k + 1] = max(data[k + 1], grime[v])
        data[k + 2] = 0.55 * data[k + 2] + 0.45 * tone[v]
    col.data.foreach_set("color", data)
    me.update()
    return obj


# --- assembly -------------------------------------------------------------------------------------------------
def finish(parts, name, waterline=None, wear=0.6, chips=0.85, deck_grime=False):
    for p in parts:
        if not p.data.uv_layers:
            sb.world_box_uv(p, 1.0)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=wear, waterline=waterline, wet_height=0.15, seed=7)
    weather(obj, chips=chips, deck_grime=deck_grime, seed=len(name))
    return obj


def export(obj, cols=()):
    sb.save_blend(obj.name)
    sb.export_fbx([obj] + list(cols), os.path.join("Boat", obj.name + ".fbx"))


def net_heap(cx, cy, cz):
    """A heap of fishing net on deck: a lumpy mound of folds, flattening out towards a wobbly edge; the net's cork line
    runs over it with the corks threaded on, two glass floats in rope nets lie on top."""
    from mathutils import noise as mnoise
    sx, sy, n_u, n_v = 0.5, 0.37, 32, 24

    def surf(u, v):                                     # u, v in [-1, 1] -> Unreal point on the heap
        th = math.atan2(v, u)
        wob = 1.0 + 0.09 * math.sin(3 * th + 1.0) + 0.05 * math.sin(5 * th - 0.4)
        r = math.hypot(u, v) / wob
        mound = max(0.0, 1.0 - r * r) ** 0.75
        folds = 0.028 * math.sin(7.0 * u + 3.0 * v + 0.6) * math.cos(4.5 * v - 2.0 * u) + 0.016 * math.sin(14.0 * (0.6 * u + 0.8 * v))
        lump = 0.025 * mnoise.noise(Vector((u * 2.3 + 4.1, v * 2.3 - 1.7, 0.37)))
        z = 0.012 + 0.26 * mound + (folds + lump) * min(1.0, mound * 3.0 + 0.15)
        return (cx + u * sx * wob, cy + v * sy * wob, cz + max(z, 0.008))
    bm = bmesh.new()
    grid = [[bm.verts.new(U(surf(-1.0 + 2.0 * i / n_u, -1.0 + 2.0 * j / n_v))) for i in range(n_u + 1)] for j in range(n_v + 1)]
    for j in range(n_v):
        for i in range(n_u):
            f = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            f.smooth = True
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    heap = mesh_object("net_heap", bm, ["MI_Rope_Hemp"])
    if heap.data.polygons[len(heap.data.polygons) // 2].normal.z < 0.0:
        for poly in heap.data.polygons:
            poly.flip()
    sol = heap.modifiers.new("Solidify", "SOLIDIFY")
    sol.thickness = 0.012
    sol.offset = -1.0
    sb.apply_modifiers(heap)
    parts = [heap]

    def on_heap(u, v, lift):
        x, y, z = surf(u, v)
        return (x, y, z + lift)
    # The cork line (the net's head rope) wanders over the heap, corks threaded on it.
    line = [on_heap(-0.95 + 1.9 * k / 14, 0.35 * math.sin(2.6 * k / 14 * math.pi) - 0.1, 0.012) for k in range(15)]
    parts += rope("cork_line", line, 0.009)
    for k in range(1, 14, 2):
        a = Vector(line[k + 1]) - Vector(line[k - 1])
        parts.append(lathe("cork", [(0.0, -0.035), (0.03, -0.033), (0.042, -0.02), (0.045, 0.0), (0.042, 0.02), (0.03, 0.033), (0.0, 0.035)],
                           line[k], "MI_Wood_DockWeathered", 10, axis=tuple(a.normalized())))
    # Two glass floats in rope nets lying in the folds.
    for u, v in ((-0.35, 0.3), (0.3, -0.25)):
        fz = on_heap(u, v, 0.055)
        parts.append(lathe("glass_float", [(0.0, -0.065), (0.033, -0.056), (0.056, -0.033), (0.065, 0.0), (0.056, 0.033), (0.033, 0.056),
                                           (0.0, 0.065)], fz, "MI_Glass_Dark", 16))
        for a in (0.0, 1.05, 2.1):
            parts.append(ring("float_net", 0.067, 0.005, fz, (math.cos(a), math.sin(a), 0.0), "MI_Rope_Hemp", 16))
        parts.append(ring("float_net", 0.058, 0.005, fz, (0.0, 0.0, 1.0), "MI_Rope_Hemp", 16))
    return parts


def boat_decor():
    """Things that make a boat lived on (D52, reworked D53): a heap of net with floats and netting over its edge, a
    planked sea chest with hinges and a padlock, a boat hook lashed along the bulwark, the ensign's staff with its flag
    line and cleat, and carved trailboards with rosettes on both bows. (The mast carries the bell and the lantern.)"""
    parts = []
    rng = random.Random(97)
    # A heap of net with floats, aft on the starboard side.
    nx, ny = -2.3, 0.85
    nz = deck_z(nx, ny)
    parts += net_heap(nx, ny, nz)
    # A sea chest aft on the port side: planked box, iron corners, rope handles.
    sx, sy, cl, cw, ch = CHEST                          # length (X), depth (Y), height of the box (D58: 30 % bigger)
    sz = deck_z(sx, sy)
    for r in range(3):                                  # three boards high on each side, ends between them
        z = sz + 0.02 + (r + 0.5) * (ch / 3)
        for yy in (-cw / 2 + 0.013, cw / 2 - 0.013):
            parts.append(tb.rough_plank("chest_side", cl, ch / 3 - 0.006, 0.026, U((sx, sy + yy, z)), (math.pi / 2, 0, 0),
                                        M["MI_Wood_Varnish"], nseed(), wear=0.6, grooves=(0, 1), split_chance=0.1))
        for xx in (-cl / 2 + 0.013, cl / 2 - 0.013):
            parts.append(tb.rough_plank("chest_end", cw - 0.05, ch / 3 - 0.006, 0.026, U((sx + xx, sy, z)), (math.pi / 2, 0, math.pi / 2),
                                        M["MI_Wood_Varnish"], nseed(), wear=0.6, grooves=(0, 1), split_chance=0.1))
    parts.append(block("chest_bottom", (cl - 0.03, cw - 0.03, 0.03), (sx, sy, sz + 0.02), "MI_Wood_Varnish", 0.008))
    for k in range(4):                                  # lid of four boards, a little proud of the box
        parts.append(tb.rough_plank("chest_lid", cl + 0.04, (cw + 0.04) / 4 - 0.005, 0.03,
                                    U((sx, sy - (cw + 0.04) / 2 + (k + 0.5) * (cw + 0.04) / 4, sz + ch + 0.035)), (0, 0, 0),
                                    M["MI_Wood_TeakDark"], nseed(), wear=0.7, grooves=(0, 1), split_chance=0.15))
    for dx in (-cl / 2, cl / 2):                        # iron corner bands
        for dy in (-cw / 2, cw / 2):
            parts.append(block("chest_corner", (0.045, 0.045, ch + 0.02), (sx + dx, sy + dy, sz + ch / 2 + 0.02), "MI_Metal_Iron", 0.006))
    for dx in (-cl * 0.29, cl * 0.29):                  # hinge straps from the back over the lid
        parts.append(block("chest_hinge", (0.05, 0.012, 0.16), (sx + dx, sy - cw / 2 - 0.006, sz + ch - 0.05), "MI_Metal_Iron", 0.004))
        parts.append(block("chest_hinge_top", (0.05, cw * 0.42, 0.012), (sx + dx, sy - cw * 0.25, sz + ch + 0.055), "MI_Metal_Iron", 0.004))
        parts.append(rod("chest_hinge_pin", (sx + dx - 0.03, sy - cw / 2 - 0.01, sz + ch + 0.03), (sx + dx + 0.03, sy - cw / 2 - 0.01, sz + ch + 0.03),
                         0.009, "MI_Metal_Iron", 8))
    parts.append(block("chest_hasp", (0.05, 0.012, 0.1), (sx, sy + cw / 2 + 0.008, sz + ch - 0.01), "MI_Metal_Iron", 0.004))
    parts.append(block("chest_lock", (0.06, 0.025, 0.055), (sx, sy + cw / 2 + 0.022, sz + ch - 0.07), "MI_Metal_Iron", 0.008))
    parts.append(ring("chest_shackle", 0.02, 0.005, (sx, sy + cw / 2 + 0.022, sz + ch - 0.03), (0, 1, 0), "MI_Metal_Iron", 10))
    for dx in (-cl / 2 - 0.012, cl / 2 + 0.012):        # rope beckets on the ends
        parts.append(ring("chest_becket", 0.05, 0.011, (sx + dx, sy, sz + ch * 0.7), (1, 0, 0), "MI_Rope_Hemp", 12))
    # A boat hook along the inside of the starboard bulwark.
    by_ = lambda x: half_beam(x) - BULWARK_T - 0.05
    hook = tb.spar("boathook", 2.3, 0.025, 0.022, M["MI_Wood_Varnish"], nseed(), sides=12, sweep=0.01, axis=(1, 0, 0), facets=6, checks=0)
    hook.data.transform(Matrix.Translation(U((0.95, by_(2.0), sheer_z(2.0) + 0.12))))
    parts.append(hook)
    hx_ = 3.25
    parts.append(rod("hook_iron", (hx_, by_(2.0), sheer_z(2.0) + 0.12), (hx_ + 0.16, by_(2.0), sheer_z(2.0) + 0.12), 0.016, "MI_Metal_Iron", 8))
    parts.append(rod("hook_tip", (hx_ + 0.12, by_(2.0), sheer_z(2.0) + 0.12), (hx_ + 0.1, by_(2.0), sheer_z(2.0) + 0.22), 0.012, "MI_Metal_Iron", 8))
    for xl in (1.05, 2.85):                             # lashed to two small cleats on the inside of the bulwark
        yb = half_beam(xl) - BULWARK_T
        parts.append(block("hook_cleat", (0.14, 0.05, 0.04), (xl, yb - 0.022, sheer_z(xl) + 0.065), "MI_Wood_Varnish", 0.01))
        for k in range(3):
            parts.append(ring("hook_lashing", 0.046, 0.008, (xl - 0.03 + k * 0.03, (by_(2.0) + yb - 0.022) / 2, sheer_z(2.0) + 0.095), (1, 0, 0),
                              "MI_Rope_Hemp", 10))
    # The ensign's staff at the stern (SM_Boat_Ensign flies from its top, ASaltBoat).
    ez = deck_z(ENSIGN_X, 0.0)
    parts.append(block("ensign_socket", (0.14, 0.14, 0.12), (ENSIGN_X, 0, ez + 0.05), "MI_Wood_Varnish", 0.02))
    parts.append(rod("ensign_staff", (ENSIGN_X, 0, ez), (ENSIGN_X, 0, ez + ENSIGN_H + 0.04), 0.022, "MI_Wood_Varnish", 10))
    parts.append(ring("ensign_knob", 0.012, 0.022, (ENSIGN_X, 0, ez + ENSIGN_H + 0.06), (0, 0, 1), "MI_Metal_Brass", 10))
    # The flag line: from the staff's top down to a small cleat on the staff (the flag is bent on to it).
    parts.append(block("ensign_cleat", (0.025, 0.03, 0.14), (ENSIGN_X - 0.03, 0, ez + 0.75), "MI_Wood_Varnish", 0.008))
    parts.append(rod("ensign_line", (ENSIGN_X - 0.035, 0.0, ez + ENSIGN_H), (ENSIGN_X - 0.045, 0.0, ez + 0.8), 0.006, "MI_Rope_Hemp", 6))
    for k in range(3):
        parts.append(ring("ensign_line_turn", 0.022, 0.006, (ENSIGN_X - 0.045, 0.0, ez + 0.74 + k * 0.025), (0, 0, 1), "MI_Rope_Hemp", 8))
    # Carved trailboards on both bows: a cream board on the second strake with a rosette at its forward end.
    th = strake_bounds()
    ta, tb_ = th[STRAKES - 2], th[STRAKES - 1]
    w0 = (Vector(section_point(0.0, tb_, 1)) - Vector(section_point(0.0, ta, 1))).length
    for side in (-1, 1):
        base = strake_frame(side, ta, tb_, 3.3, 4.55, w0)

        def tframe(sf, base=base):
            pm, am, nm, wm = base(sf)
            return pm + nm * (PLANK_T * 0.5 + 0.012), am, nm, 0.55 * (1.0 - 0.45 * sf)
        parts.append(laid_board("trailboard", path_length(tframe), w0 * 0.55, 0.022, tframe, "MI_Paint_CreamAged", wear=0.5, grooves=(1, 1)))
        pm, am, nm, wm = tframe(0.97)
        parts.append(disc("rosette", 0.055, 0.03, tuple(pm + nm * 0.015), tuple(nm), "MI_Paint_CreamAged", 14, 0.008))
        parts.append(ring("rosette_ring", 0.06, 0.01, tuple(pm + nm * 0.03), tuple(nm), "MI_Metal_Brass", 14))
    return parts


def build_hull():
    parts = ([hull_core()] + planking() + bulwarks() + deck_planks() + end_closures() + keel_and_posts() + rig_and_fittings() + deck_gear()
             + boat_decor())
    obj = finish(parts, "SM_Boat_Hull", waterline=0.0, deck_grime=True)
    cols = hull_collision("SM_Boat_Hull")
    print("SB_BOAT_HULL faces", len(obj.data.polygons), "parts", len(parts), "collision pieces", len(cols))
    export(obj, cols)


# --- the yard and the square sail (D65) ------------------------------------------------------------------------
# Owner (D65): "make the sail of the starter ship a normal sail, so both sides with the same amount of sail, not rotatable".
# A square sail as the Nordland boats carry it: a hewn yard slung on the mast's front by its halyard, held to the mast by a
# parrel and steadied by lifts from the masthead and braces made fast aft; the sail laced under it; brails from blocks under
# the yard to its foot roll it up; sheets from its clews to the pin rails (ASaltBoat's ropes). SM_Boat_Yard and SM_Boat_Sail
# are built in the yard's frame (origin on its axis in the middle, Unreal axes), which lies at YARD_POS in the hull's.
YARD_SEEDS = (5301, 5302)
YARD_POS = Vector((YARD_X, 0.0, sheer_z(MAST_X) + YARD_H))
SLING_H, ARM_H, BRAIL_H = 0.14, 0.06, 0.05      # the iron bands' widths: slings, yardarms, brails
BRAIL_DIR = (-0.75, -0.66)                      # the brails' eyes stand out aft and down (Unreal x, z)


def yard_spars():
    """The yard's two halves: hewn spars from the slings out to the yardarms along Y, tapering both ways. Fixed seeds, so the
    hull's ropes reach the same yard SM_Boat_Yard is built from."""
    halves = []
    for side, seed in ((1, YARD_SEEDS[0]), (-1, YARD_SEEDS[1])):
        halves.append(tb.spar("yard_half", YARD_HALF, YARD_R, YARD_END_R, M["MI_Wood_Varnish"], seed, sides=28, sweep=0.02,
                              axis=(0, -side, 0), facets=8, checks=2))   # spar axes are Blender space: +Y = Unreal -Y
    return halves


def yard_section(spars, y, half=0.04):
    """Centre (as an Unreal point at y) and largest radius of the hewn yard near y (its rings lie 0.1 m apart)."""
    half = max(half, 0.06)
    pts = [v.co for s in spars for v in s.data.vertices if abs(-v.co.y - y) < half]
    cx = sum(p.x for p in pts) / len(pts)
    cz = sum(p.z for p in pts) / len(pts)
    return Vector((cx, y, cz)), max(math.hypot(p.x - cx, p.z - cz) for p in pts)


def band_eye(spars, y, h, d):
    """The eye of an iron band of width h round the yard at y, standing out towards d (Unreal (x, z))."""
    c, r = yard_section(spars, y, h / 2 + 0.03)
    dv = Vector((d[0], 0.0, d[1])).normalized()
    return c + dv * (r + 0.006 + 0.035)


def yard_band(name, spars, y, h, eye_dirs=()):
    """An iron band round the yard at y with eyes standing out towards eye_dirs (Unreal (x, z)); returns (parts, eyes)."""
    c, r = yard_section(spars, y, h / 2 + 0.03)
    r += 0.006
    parts = [sb.add_cylinder(name, r, h, U(tuple(c)), "Y", 24, M["MI_Metal_Iron"])]
    for dy in (-h / 2, h / 2):
        parts.append(ring(name + "_edge", r, 0.006, tuple(c + Vector((0.0, dy, 0.0))), (0, 1, 0), "MI_Metal_Iron", 24))
    eyes = []
    for d in eye_dirs:
        dv = Vector((d[0], 0.0, d[1])).normalized()
        eye = c + dv * (r + 0.035)
        parts.append(rod(name + "_neck", tuple(c + dv * (r - 0.004)), tuple(eye - dv * 0.018), 0.009, "MI_Metal_Iron", 8))
        parts.append(ring(name + "_eye", 0.022, 0.008, tuple(eye), tuple(dv.cross(Vector((0.0, 1.0, 0.0)))), "MI_Metal_Iron", 12))
        eyes.append(eye)
    return parts, eyes


def yard_points(spars):
    """Where the ropes reach the yard (its frame, Unreal metres): the slings' eye (the halyard), the yardarms' eyes (lifts up,
    braces aft) and the rope's point under each brail block (tackle_block hangs it 0.225 m under its eye)."""
    p = {"sling": band_eye(spars, 0.0, SLING_H, (0.0, 1.0))}
    for side in (-1, 1):
        p["lift", side] = band_eye(spars, side * YARDARM_Y, ARM_H, (0.0, 1.0))
        p["brace", side] = band_eye(spars, side * YARDARM_Y, ARM_H, (-1.0, 0.0))
        p["brail", side] = band_eye(spars, side * BRAIL_Y, BRAIL_H, BRAIL_DIR) - Vector((0.0, 0.0, 0.225))
    return p


def build_yard():
    """SM_Boat_Yard (D65) in its own frame: the hewn yard in two halves meeting under the slings' band, whose eye takes the
    halyard, with a cleat either side; the parrel round the mast's back, threaded with beads, its ends taken round the yard;
    the yardarms' bands with the lifts' and the braces' eyes; the brails' bands with their blocks hanging aft."""
    spars = yard_spars()
    parts = list(spars)
    p_, _ = yard_band("sling_band", spars, 0.0, SLING_H, [(0.0, 1.0)])
    parts += p_
    for s_ in (-1, 1):
        c, r = yard_section(spars, s_ * 0.13, 0.03)
        parts.append(block("sling_cleat", (0.07, 0.06, 0.05), tuple(c + Vector((0.0, 0.0, r + 0.014))), "MI_Wood_TeakDark", 0.01))
        parts.append(nail(tuple(c + Vector((0.0, 0.0, r + 0.04))), (0, 0, 1)))
    # The parrel: from the yard's aft face either side of the slings round the mast's back, beads on it where it rubs.
    mdx, mdy = mast_offset(YARD_H)
    mc = Vector((MAST_X + mdx - YARD_X, mdy, 0.0))
    rp = mast_r(YARD_H) + 0.034
    arc = [mc + Vector((rp * math.cos(a), rp * math.sin(a), 0.0)) for a in [math.radians(80.0 + 200.0 * i / 20) for i in range(21)]]
    ends = []
    for s_ in (1, -1):
        c, r = yard_section(spars, s_ * 0.22, 0.03)
        ends.append(c + Vector((-r - 0.006, 0.0, 0.0)))
        for k in range(2):
            parts.append(ring("parrel_turn", r + 0.012, 0.009, tuple(c + Vector((0.0, s_ * (k * 0.02 - 0.01), 0.0))), (0, 1, 0), "MI_Rope_Hemp", 16))
    parts += rope("parrel", [tuple(ends[0])] + [tuple(a) for a in arc] + [tuple(ends[1])], 0.009)
    for i in range(7):
        a = math.radians(120.0 + 120.0 * i / 6)
        c = mc + Vector((rp * math.cos(a), rp * math.sin(a), 0.0))
        parts.append(lathe("parrel_bead", [(0.0, -0.022), (0.012, -0.022), (0.019, -0.012), (0.021, 0.0), (0.019, 0.012), (0.012, 0.022),
                                            (0.0, 0.022)], tuple(c), "MI_Wood_TeakDark", 12, axis=(-math.sin(a), math.cos(a), 0.0)))
    for s_ in (-1, 1):
        p_, _ = yard_band("yardarm_band", spars, s_ * YARDARM_Y, ARM_H, [(0.0, 1.0), (-1.0, 0.0)])
        parts += p_
        p_, eyes = yard_band("brail_band", spars, s_ * BRAIL_Y, BRAIL_H, [BRAIL_DIR])
        parts += p_
        p_, _ = tackle_block("brail_block", tuple(eyes[0]), (1.0, 0.0))
        parts += p_
    pts = yard_points(spars)
    print("SB_YARD sling", tuple(round(v, 3) for v in pts["sling"]), "brail rope", tuple(round(v, 3) for v in pts["brail", 1]),
          "lift", tuple(round(v, 3) for v in pts["lift", 1]), "brace", tuple(round(v, 3) for v in pts["brace", 1]),
          "mid radius", round(yard_section(spars, 0.0, 0.1)[1], 3), "arm radius", round(yard_section(spars, YARDARM_Y)[1], 3))
    obj = finish(parts, "SM_Boat_Yard", wear=0.6)
    export(obj)


def cleat(name, x, side, z):
    """A cleat on the inside of the bulwark at x (a block and a horn along the boat); returns (parts, the horn's middle)."""
    y = side * (half_beam(x) - BULWARK_T - 0.02)
    parts = [block(name, (0.09, 0.04, 0.06), (x, y, z), "MI_Wood_Varnish", 0.01)]
    horn = Vector((x, y - side * 0.045, z + 0.01))
    parts.append(rod(name + "_horn", tuple(horn - Vector((0.12, 0.0, 0.0))), tuple(horn + Vector((0.12, 0.0, 0.0))), 0.02, "MI_Wood_Varnish", 10))
    parts.append(nail((x, y - side * 0.021, z + 0.015), (0, -side, 0)))
    return parts, horn


def yard_running_rigging():
    """D65, the ropes between the hull and the yard (in the hull's frame): the halyard from the yard's slings up over the
    masthead's sheave and down to the starboard pin rail; the lifts from the masthead band's eye bolts out to the yardarms; the
    braces from the yardarms aft to cleats inside the bulwark (they hold the yard square); the brails' falls from their blocks
    under the yard down to the pin rails."""
    spars = yard_spars()
    pts = yard_points(spars)
    for s in spars:
        bpy.data.objects.remove(s, do_unlink=True)

    def at(v):
        return tuple(Vector(v) + YARD_POS)
    parts = []
    sling = Vector(at(pts["sling"]))
    over = MAST_INFO["halyard_over"]
    parts += rope("halyard", [tuple(sling + Vector((0.0, 0.0, 0.02)))] + over + [MAST_INFO["pin", 1, 1]], 0.016)
    lead = (Vector(over[0]) - sling).normalized()
    parts.append(ring("halyard_splice", 0.03, 0.01, tuple(sling + Vector((0.0, 0.0, 0.01))), (1, 0, 0), "MI_Rope_Hemp", 12))
    for k in range(3):                                  # the eye's seizing
        parts.append(ring("halyard_seizing", 0.022, 0.008, tuple(sling + lead * (0.07 + k * 0.03)), tuple(lead), "MI_Rope_Hemp", 10))
    for side in (-1, 1):
        parts += rope("lift", [MAST_INFO["lift_eye", side], at(pts["lift", side])], 0.013)
        p_, horn = cleat("brace_cleat", BRACE_X, side, sheer_z(BRACE_X) + BULWARK_H - 0.12)
        parts += p_
        parts += rope("brace", [at(pts["brace", side]), tuple(horn + Vector((0.06, 0.0, 0.02)))], 0.013)
        for k in range(3):
            parts.append(ring("brace_turn", 0.03, 0.009, tuple(horn + Vector((-0.06 + k * 0.05, 0.0, 0.0))), (1, 0, 0), "MI_Rope_Hemp", 10))
        parts += rope("brail_fall", [at(pts["brail", side] - Vector((0.02, 0.0, 0.0))), MAST_INFO["pin", side, 0]], 0.012)
    print("SB_RIG yard at", tuple(round(v, 3) for v in YARD_POS), "brail fall from", at(pts["brail", 1]))
    return parts


def build_sail():
    """The square sail (D65) in the yard's frame: nine cloths sewn side by side with raised seams, a little wider at the foot
    than at the head; laced to the yard with robands, the head's ends stretched out to the yardarms with earings; a chunky
    boltrope round its edges with rope cringles at the corners (the sheets go to the clews, ASaltBoat), two rows of reef points
    near the foot and two sewn patches. It hangs flat: M_SB_SquareSail bellies it forward and rolls it up from the foot (its
    geometry is the material's parameters and SaltHullShape.h Sail*)."""
    panels, nv = 9, 72
    us = []
    for k in range(panels):
        a = k / panels
        us += [a, a + 0.008, a + 0.5 / panels, a + 1.0 / panels - 0.008]
    us.append(1.0)

    def half_w(v):
        return SAIL_HALF_HEAD + (SAIL_HALF_FOOT - SAIL_HALF_HEAD) * v

    def point(u, v):
        seam = 0.0
        for k in range(1, panels):
            seam = max(seam, 0.009 * clamp(1.0 - abs(u - k / panels) / 0.008))
        return (seam, (2.0 * u - 1.0) * half_w(v), SAIL_HEAD_Z - SAIL_DROP * v)

    bm = bmesh.new()
    grid = [[bm.verts.new(U(point(u, j / nv))) for u in us] for j in range(nv + 1)]
    for j in range(nv):
        for i in range(len(us) - 1):
            f = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            f.smooth = True
    # Two patches sewn on where the cloth wore through (both faces), a little proud of the cloth.
    for (u0, u1, v0, v1) in ((0.57, 0.66, 0.3, 0.36), (0.23, 0.31, 0.55, 0.6)):
        for f_side in (-1, 1):
            n_u, n_v = 4, 4
            pg = []
            for j in range(n_v + 1):
                row = []
                for i in range(n_u + 1):
                    q = point(u0 + (u1 - u0) * i / n_u, v0 + (v1 - v0) * j / n_v)
                    row.append(bm.verts.new(U((q[0] + f_side * 0.011, q[1], q[2]))))
                pg.append(row)
            for j in range(n_v):
                for i in range(n_u):
                    face = (pg[j][i], pg[j][i + 1], pg[j + 1][i + 1], pg[j + 1][i])
                    fc = bm.faces.new(face if f_side < 0 else tuple(reversed(face)))
                    fc.smooth = True
    cloth = mesh_object("cloth", bm, ["MI_SquareSailCloth"])
    solid = cloth.modifiers.new("Solidify", "SOLIDIFY")
    solid.thickness = 0.016
    solid.offset = 0.0
    sb.apply_modifiers(cloth)
    parts = [cloth]
    # Boltrope round the edges: the head, the starboard leech, the foot, the port leech.
    edge = [point(u, 0.0) for u in us] + [point(1.0, j / nv) for j in range(1, nv + 1)] + \
           [point(u, 1.0) for u in reversed(us[:-1])] + [point(0.0, j / nv) for j in range(nv - 1, -1, -1)]
    parts += rope("boltrope", edge, 0.024, "MI_SquareSailRope")
    # Rope cringles at the four corners.
    for v_ in (0.0, 1.0):
        for u_ in (0.0, 1.0):
            parts.append(ring("cringle", 0.04, 0.013, point(u_, v_), (1, 0, 0), "MI_SquareSailRope", 14))
    # Reef points: two rows of short ties through the cloth near the foot, hanging on both faces.
    for v_ in (0.8, 0.9):
        for k in range(panels):
            p = Vector(point((k + 0.5) / panels, v_))
            for f_side in (-1, 1):
                parts.append(rod("reef", tuple(p + Vector((f_side * 0.01, 0.0, 0.0))), tuple(p + Vector((f_side * 0.03, 0.0, -0.13))), 0.009,
                                 "MI_SquareSailRope", 6))
    # Robands lace the head to the yard (round the yard and the head rope), and the earings stretch the head's ends out to the
    # yardarms (a few turns round the yard inside its band).
    spars = yard_spars()
    for y in (-2.1, -1.75, -1.4, -0.95, -0.65, -0.3, 0.3, 0.65, 0.95, 1.4, 1.75, 2.1):
        c, r = yard_section(spars, y, 0.03)
        top, bot = c.z + r + 0.012, SAIL_HEAD_Z - 0.03
        rb = sb.add_torus("roband", 1.0, 0.008, (0, 0, 0), (0, 0, 0), 20, 6, M["MI_SquareSailRope"])
        for v in rb.data.vertices:
            d = math.hypot(v.co.x, v.co.y)
            off = d - 1.0
            v.co.x, v.co.y = v.co.x / d * (r + 0.014 + off), v.co.y / d * ((top - bot) / 2 + off)
        rb.data.transform(Matrix.Translation(U((c.x, y, (top + bot) / 2))) @ Matrix.Rotation(math.pi / 2, 4, "X"))
        parts.append(rb)
    for s_ in (-1, 1):
        corner = Vector(point(0.5 + 0.5 * s_, 0.0))
        for k in range(3):
            c, r = yard_section(spars, s_ * (SAIL_HALF_HEAD + 0.035 + k * 0.022), 0.02)
            parts.append(ring("earing", r + 0.012, 0.009, tuple(c), (0, 1, 0), "MI_SquareSailRope", 14))
        c, r = yard_section(spars, s_ * (SAIL_HALF_HEAD + 0.035), 0.02)
        parts += rope("earing_lead", [tuple(corner), tuple(c + Vector((0.0, 0.0, -r - 0.01)))], 0.01, "MI_SquareSailRope")
    for s in spars:
        bpy.data.objects.remove(s, do_unlink=True)
    print("SB_SAIL head z %.2f drop %.2f half head %.2f half foot %.2f" % (SAIL_HEAD_Z, SAIL_DROP, SAIL_HALF_HEAD, SAIL_HALF_FOOT))
    obj = finish(parts, "SM_Boat_Sail", wear=0.0, chips=0.0)
    export(obj)


# --- moving parts -------------------------------------------------------------------------------------------
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


def build_steering_oar():
    """Steering oar (SM_Boat_SteeringOar) in its pivot's frame: origin in the boss, +Z up along the hewn shaft, a blade
    of three boards below trailing aft (-X) with iron straps, a rope wrap where the withy holds it, and the handle at
    the head pointing inboard (-Y: the oar hangs on the starboard quarter)."""
    shaft = tb.spar("shaft", OAR_HEAD + 0.1 - OAR_TIP, 0.058, 0.066, M["MI_Wood_Varnish"], nseed(), sides=24, sweep=0.02,
                    axis=(0, 0, 1), facets=8, checks=1)
    shaft.data.transform(Matrix.Translation(U((0.0, 0.0, OAR_TIP))))
    parts = [shaft]
    h = OAR_BLADE_TOP - OAR_TIP
    bw = OAR_CHORD / 3
    for k in range(3):
        hk = h * (1.0 - 0.12 * k)
        parts.append(tb.rough_plank("blade", hk, bw - 0.008, 0.06 - 0.01 * k, (-0.04 - (k + 0.5) * bw, 0.0, OAR_TIP + hk / 2),
                                    (math.pi / 2, -math.pi / 2, 0), M["MI_Wood_Varnish"], nseed(), wear=0.45, grooves=(1, 1),
                                    split_chance=0.0))
    for z in (OAR_TIP + 0.2, OAR_BLADE_TOP - 0.2):
        parts.append(sb.add_box("strap", (OAR_CHORD + 0.1, 0.085, 0.05), (-OAR_CHORD / 2 - 0.01, 0.0, z), M["MI_Metal_Iron"]))
        sb.soften(parts[-1], 0.01, 2)
    for k in range(7):
        parts.append(ring("wrap", 0.07, 0.012, (0.0, 0.0, -0.12 + k * 0.04), (0, 0, 1), "MI_Rope_Hemp", 14))
    zh = OAR_HEAD - 0.04
    handle = tb.spar("handle", OAR_HANDLE, 0.042, 0.034, M["MI_Wood_Varnish"], nseed(), sides=16, sweep=0.0, axis=(0, 1, 0), facets=8)
    handle.data.transform(Matrix.Translation(U((0.0, 0.0, zh))))       # spar axes are Blender space: +Y = Unreal -Y
    parts.append(handle)
    parts.append(rod("grip", (0.0, -OAR_HANDLE + 0.02, zh), (0.0, -OAR_HANDLE - 0.1, zh), 0.045, "MI_Wood_Varnish", 12))
    parts.append(ring("collar", 0.072, 0.014, (0.0, 0.0, zh), (0, 0, 1), "MI_Metal_Iron", 16))
    obj = finish(parts, "SM_Boat_SteeringOar", wear=0.7)
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


def build_gangway():
    """Boarding plank from the pier to the boat's gap in the bulwark: two thick boards on cross battens, a little
    worn. Pivot at the pier end on the walking surface, lying along +X (1.3 m); the map tilts it down to the deck."""
    parts = []
    for y in (-0.15, 0.15):
        parts.append(tb.rough_plank("board", 1.3, 0.29, 0.05, (0.65, y, -0.025), (0, 0, 0), M["MI_Wood_Varnish"], nseed(),
                                    wear=0.6, grooves=(1, 2), split_chance=0.2))
    for x in (0.12, 0.5, 0.85, 1.2):
        parts.append(tb.rough_plank("batten", 0.62, 0.07, 0.04, (x, 0.0, -0.07), (0, 0, math.pi / 2), M["MI_Wood_Varnish"], nseed(),
                                    wear=0.5, grooves=(0, 1), split_chance=0.0))
    obj = finish(parts, "SM_Boat_Gangway")
    cols = [sb.add_ucx_box("SM_Boat_Gangway", 1, (1.3, 0.6, 0.06), (0.65, 0.0, -0.03))]
    export(obj, cols)


def build_windlass():
    """The windlass drum with its crank handles (SM_Boat_WindlassDrum) and the rope wound on it
    (SM_Boat_WindlassRope); both pivot on the drum's axis (along Y), so ASaltBoat turns them and thins the coil."""
    parts = [tb.spar("windlass_drum", 0.86, 0.12, 0.12, M["MI_Wood_Varnish"], nseed(), sides=24, sweep=0.0, axis=(0, 1, 0), facets=8, checks=1)]
    parts[0].data.transform(Matrix.Translation(U((0.0, 0.43, 0.0))))       # spar axes are Blender space: +Y = Unreal -Y
    for y in (-0.6, 0.6):
        parts.append(rod("crank", (0.0, y * 0.95, 0.0), (0.0, y * 1.08, 0.0), 0.02, "MI_Metal_Iron"))
        parts.append(rod("crank_arm", (0.0, y * 1.08, 0.0), (-0.05, y * 1.08, -0.26), 0.018, "MI_Metal_Iron"))
        parts.append(rod("crank_grip", (-0.05, y * 1.08, -0.26), (-0.05, y * 1.2, -0.26), 0.022, "MI_Wood_Varnish"))
    # Ratchet wheel at the starboard end (the pawl on the cheek drops into its teeth).
    parts.append(disc("ratchet", 0.16, 0.03, (0.0, 0.4, 0.0), (0, 1, 0), "MI_Metal_Iron", 24, 0.004))
    for k in range(16):
        a = 2 * math.pi * k / 16
        tooth = block("ratchet_tooth", (0.035, 0.03, 0.03), (0.0, 0.4, 0.0), "MI_Metal_Iron", 0.003)
        tooth.data.transform(Matrix.Translation(U((0.175 * math.cos(a), 0.4, 0.175 * math.sin(a)))) @ Matrix.Rotation(-a, 4, "Y") @
                             Matrix.Translation(-U((0.0, 0.4, 0.0))))
        parts.append(tooth)
    obj = finish(parts, "SM_Boat_WindlassDrum", wear=0.7)
    export(obj)
    sb.reset_scene()
    mats()
    coil = []
    for layer in range(2):
        for k in range(9):
            coil.append(ring("windlass_rope", 0.135 + layer * 0.034, 0.018, (0.0, -0.26 + k * 0.065 + layer * 0.03, 0.0), (0, 1, 0), "MI_Rope_Hemp"))
    obj = finish(coil, "SM_Boat_WindlassRope", wear=0.0, chips=0.0)
    export(obj)


def build_pennant():
    """Wind pennant (SM_Boat_Pennant): a long tapered cloth flying along +X from the swivel (pivot) at the top of its
    hoist, two rope grommets round the spindle. M_SB_Flag streams it out, droops it and makes it flutter."""
    nu, nv = 44, 3
    bm = bmesh.new()

    def point(u, v):
        x = 0.03 + (PENNANT_L - 0.03) * u
        half = 0.5 * (PENNANT_HOIST * (1.0 - u) ** 0.85 + 0.03 * u)
        zc = -0.5 * PENNANT_HOIST
        # A soft static crease, so it never looks cut from sheet metal.
        y = 0.012 * math.sin(u * 9.0 + v * 2.0) * u
        return (x, y, zc + half * (2.0 * v - 1.0))
    grid = [[bm.verts.new(U(point(i / nu, j / nv))) for i in range(nu + 1)] for j in range(nv + 1)]
    for j in range(nv):
        for i in range(nu):
            f = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            f.smooth = True
    cloth = mesh_object("pennant", bm, ["MI_Pennant"])
    solid = cloth.modifiers.new("Solidify", "SOLIDIFY")
    solid.thickness = 0.006
    solid.offset = 0.0
    sb.apply_modifiers(cloth)
    parts = [cloth]
    for z in (-0.02, -PENNANT_HOIST + 0.02):
        parts.append(ring("grommet", 0.022, 0.006, (0.0, 0.0, z), (0, 0, 1), "MI_Rope_Hemp", 10))
    parts.append(rod("hoist_seam", (0.03, 0.0, 0.0), (0.03, 0.0, -PENNANT_HOIST), 0.007, "MI_Rope_Hemp", 6))
    obj = finish(parts, "SM_Boat_Pennant", wear=0.0, chips=0.0)
    export(obj)


def build_ensign():
    """Ensign (SM_Boat_Ensign): an oxblood and cream flag flying along +X from its hoist (pivot at the top of the hoist),
    rope grommets at the hoist. M_SB_Flag streams it out and makes it flutter, like the pennant (D52)."""
    nu, nv, fly, hoist = 30, 6, 0.9, 0.56
    parts = []
    for band, mat in ((0, "MI_EnsignRed"), (1, "MI_EnsignCream")):
        bm = bmesh.new()
        grid = []
        for j in range(nv // 2 + 1):
            v = (band * (nv // 2) + j) / nv
            grid.append([bm.verts.new(U((0.03 + (fly - 0.03) * i / nu, 0.01 * math.sin(i * 0.5) * i / nu, -hoist * v)))
                         for i in range(nu + 1)])
        for j in range(nv // 2):
            for i in range(nu):
                f = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
                f.smooth = True
        cloth = mesh_object("ensign_" + str(band), bm, [mat])
        sol = cloth.modifiers.new("Solidify", "SOLIDIFY")
        sol.thickness = 0.006
        sol.offset = 0.0
        sb.apply_modifiers(cloth)
        parts.append(cloth)
    for z in (-0.02, -hoist + 0.02):
        parts.append(ring("grommet", 0.022, 0.006, (0.0, 0.0, z), (0, 0, 1), "MI_Rope_Hemp", 10))
    parts.append(rod("hoist_seam", (0.03, 0.0, 0.0), (0.03, 0.0, -hoist), 0.007, "MI_Rope_Hemp", 6))
    obj = finish(parts, "SM_Boat_Ensign", wear=0.0, chips=0.0)
    export(obj)


def build_bell():
    """Ship's bell (SM_Boat_Bell), pivot at the top of its crown loop (it hangs in the arm's ring), hanging down -Z: a
    cast bronze bell with crown loop, shoulder, waist, flaring sound bow and lip, and the clapper's staple inside.
    The clapper and the bell rope are SM_Boat_BellClapper (pivot on the staple), so the clapper swings in the bell
    and strikes it (ASaltBoat)."""
    parts = [ring("bell_crown", 0.018, 0.006, (0.0, 0.0, -0.018), (1, 0, 0), "MI_Metal_Brass", 12),
             lathe("bell_crown_boss", [(0.0, -0.03), (0.014, -0.03), (0.02, -0.036), (0.0, -0.036)], (0.0, 0.0, 0.0), "MI_Metal_Brass", 12)]
    outer = [(0.0, -0.034), (0.03, -0.036), (0.047, -0.046), (0.056, -0.066), (0.053, -0.096), (0.057, -0.126), (0.067, -0.156),
             (0.083, -0.181), (0.093, -0.198), (0.095, -0.207)]
    inner = [(0.084, -0.205), (0.074, -0.188), (0.059, -0.157), (0.049, -0.127), (0.045, -0.097), (0.042, -0.068), (0.031, -0.052), (0.0, -0.049)]
    parts.append(lathe("bell", outer + inner, (0.0, 0.0, 0.0), "MI_Metal_Brass", 32))
    for z in (-0.064, -0.17):                           # cast mouldings round the bell
        parts.append(ring("bell_moulding", 0.0555 if z > -0.1 else 0.0755, 0.0035, (0.0, 0.0, z), (0, 0, 1), "MI_Metal_Brass", 32))
    parts.append(ring("bell_staple", 0.008, 0.003, (0.0, 0.0, -0.056), (1, 0, 0), "MI_Metal_Iron", 8))
    obj = finish(parts, "SM_Boat_Bell", wear=0.4, chips=0.0)
    cols = [convex("UCX_SM_Boat_Bell_01", [(0.096 * math.cos(a), 0.096 * math.sin(a), z) for a in [i * math.pi / 4 for i in range(8)]
                                           for z in (-0.208, -0.035)])]
    export(obj, cols)
    sb.reset_scene()
    mats()
    # The clapper on its staple (pivot), hanging down -Z, the bell rope tied to the ball with a knot at its end.
    parts = [ring("clapper_eye", 0.007, 0.003, (0.0, 0.0, -0.007), (0, 1, 0), "MI_Metal_Iron", 8),
             rod("clapper", (0.0, 0.0, -0.012), (0.0, 0.0, -0.118), 0.005, "MI_Metal_Iron", 8),
             lathe("clapper_ball", [(0.0, -0.017), (0.012, -0.012), (0.017, 0.0), (0.012, 0.012), (0.0, 0.017)], (0.0, 0.0, -0.128),
                   "MI_Metal_Iron", 12)]
    parts += rope("bell_rope", [(0.0, 0.0, -0.14), (0.003, 0.0, -0.26), (0.0, 0.0, -0.37)], 0.008)
    for k in range(2):
        parts.append(ring("bell_rope_knot", 0.012, 0.009, (0.0, 0.0, -0.352 - k * 0.016), (0, 0, 1), "MI_Rope_Hemp", 10))
    obj = finish(parts, "SM_Boat_BellClapper", wear=0.3, chips=0.0)
    export(obj)


def build_lantern():
    """Ship's lantern (SM_Boat_Lantern), pivot at the top of its handle (it hangs on the mast's hook), hanging down -Z:
    a wire handle, a chimney with its cap, a square pyramid hood, four corner posts in top and bottom frames round amber
    horn panes (MI_LanternGlass glows when it is lit, ASaltBoat) and a base tray on little feet."""
    parts = [ring("lantern_handle", 0.03, 0.005, (0.0, 0.0, -0.03), (1, 0, 0), "MI_Metal_Iron", 14)]
    for s_ in (-1, 1):
        parts.append(rod("lantern_bail", (0.0, s_ * 0.028, -0.04), (0.0, s_ * 0.045, -0.088), 0.005, "MI_Metal_Iron", 6))
    parts.append(lathe("lantern_chimney", [(0.0, -0.058), (0.026, -0.058), (0.028, -0.066), (0.019, -0.07), (0.019, -0.095), (0.0, -0.095)],
                       (0.0, 0.0, 0.0), "MI_Metal_Iron", 12))
    parts.append(lathe("lantern_hood", [(0.0, -0.09), (0.03, -0.09), (0.088, -0.118), (0.096, -0.128), (0.0, -0.128)], (0.0, 0.0, 0.0),
                       "MI_Metal_Iron", 4, phase=math.pi / 4))
    h = 0.066
    for z in (-0.13, -0.302):
        for a, b in (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h))):
            parts.append(rod("lantern_frame", (a[0], a[1], z), (b[0], b[1], z), 0.007, "MI_Metal_Iron", 6))
    for x_, y_ in ((-h, -h), (h, -h), (h, h), (-h, h)):
        parts.append(rod("lantern_post", (x_, y_, -0.13), (x_, y_, -0.302), 0.008, "MI_Metal_Iron", 6))
    for (x_, y_, sx_, sy_) in ((0.0, -h, 0.124, 0.005), (0.0, h, 0.124, 0.005), (-h, 0.0, 0.005, 0.124), (h, 0.0, 0.005, 0.124)):
        parts.append(sb.add_box("lantern_glass", (sx_, sy_, 0.166), U((x_, y_, -0.216)), M["MI_LanternGlass"]))
    for (x_, y_, ax_) in ((0.0, -h - 0.006, (0, 1, 0)), (0.0, h + 0.006, (0, 1, 0)), (-h - 0.006, 0.0, (1, 0, 0)), (h + 0.006, 0.0, (1, 0, 0))):
        parts.append(rod("lantern_guard", (x_ - 0.06 * ax_[1], y_ - 0.06 * ax_[0], -0.216), (x_ + 0.06 * ax_[1], y_ + 0.06 * ax_[0], -0.216),
                         0.004, "MI_Metal_Iron", 6))
    parts.append(block("lantern_base", (0.16, 0.16, 0.025), (0.0, 0.0, -0.316), "MI_Metal_Iron", 0.004))
    for x_, y_ in ((-0.06, -0.06), (0.06, -0.06), (0.06, 0.06), (-0.06, 0.06)):
        parts.append(block("lantern_foot", (0.022, 0.022, 0.016), (x_, y_, -0.336), "MI_Metal_Iron", 0.003))
    obj = finish(parts, "SM_Boat_Lantern", wear=0.4, chips=0.0)
    cols = [convex("UCX_SM_Boat_Lantern_01", [(x, y, z) for x in (-0.096, 0.096) for y in (-0.096, 0.096) for z in (-0.345, -0.09)])]
    export(obj, cols)


# The chart on the mast (D57, owner): a small chart pinned to a board on the mast, where the helmsman sees it.
MAST_CHART_UP = 1.10                    # centre above the deck (m): chest height, below the lantern (1.36 m) and the boom
MAST_CHART_YAW = 135.0                  # the way it faces: aft and to starboard (the steering oar is on the starboard quarter)
MAST_CHART_SIZE = (0.30, 0.38)          # the board (m); the sheet is 0.26 x 0.33 (aspect 1.27, SaltChart in C++)


def build_mast_chart():
    """The chart on the mast (SM_Boat_MastChart), in the boat's frame (it sits on the hull at no offset): a light teak
    board held off the mast by two chocks and two iron bands round the mast, the chart sheet on its face (its own
    material slot MI_MastChart, UV 0..1 across it, U to the right as you face it, V down) and four brass tacks."""
    deck = sheer_z(MAST_X)
    zc = deck + MAST_CHART_UP
    ax, ay = axis_at(zc)
    yaw = math.radians(MAST_CHART_YAW)
    n = Vector((math.cos(yaw), math.sin(yaw), 0.0))          # out of the board, towards the viewer
    t = Vector((n.y, -n.x, 0.0))                              # the viewer's right (facing f = -n, Unreal right = (-fy, fx))
    axis = Vector((ax, ay, 0.0))
    r_mast = girth_at_height(zc)
    w, h = MAST_CHART_SIZE
    thick = 0.022
    back = r_mast + 0.035                                     # chocks between the mast and the board
    centre = axis + n * (back + thick / 2) + Vector((0, 0, zc))
    parts = []
    rot = (0.0, 0.0, math.atan2(t.y, t.x))                    # board along t (its length), height along Z
    board = block("chart_board", (w, thick, h), tuple(centre), "MI_Wood_TeakLight", 0.006)
    sb.transform_obj(board, (0.0, 0.0, -rot[2]), tuple(U(tuple(centre))))
    parts.append(board)
    for dz in (-0.13, 0.13):
        c = axis + n * (r_mast + 0.0175) + Vector((0, 0, zc + dz))
        chock = block("chart_chock", (0.24, 0.035, 0.05), tuple(c), "MI_Wood_TeakDark", 0.006)
        sb.transform_obj(chock, (0.0, 0.0, -rot[2]), tuple(U(tuple(c))))
        parts.append(chock)
        parts.append(ring("chart_band", r_mast + 0.006, 0.007, (ax, ay, zc + dz), (0, 0, 1), "MI_Metal_Iron", 28))
    # The chart sheet: a thin quad pair on the board's face, with its UV across it.
    sw, sh = 0.26, 0.33
    face = centre + n * (thick / 2 + 0.0015)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    corners = [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]  # (u, v) offsets; v down
    def corner(cu, cv, off):
        p = face + t * (cu * sw) + Vector((0, 0, -cv * sh)) + n * off
        return bm.verts.new(U(tuple(p)))
    front = [corner(cu, cv, 0.0) for cu, cv in corners]
    backv = [corner(cu, cv, -0.0015) for cu, cv in corners]
    f = bm.faces.new(front)
    for loop, (cu, cv) in zip(f.loops, corners):
        loop[uv].uv = (cu + 0.5, 1.0 - (cv + 0.5))
    bk = bm.faces.new(list(reversed(backv)))
    for loop in bk.loops:
        loop[uv].uv = (0.0, 0.0)
    for i in range(4):
        j = (i + 1) % 4
        e = bm.faces.new((front[j], front[i], backv[i], backv[j]))
        for loop in e.loops:
            loop[uv].uv = (0.0, 0.0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    sheet = mesh_object("chart_sheet", bm, ["MI_MastChart"])
    parts.append(sheet)
    for cu, cv in ((-0.46, -0.46), (0.46, -0.46), (0.46, 0.46), (-0.46, 0.46)):
        p = face + t * (cu * sw) + Vector((0, 0, -cv * sh)) + n * 0.002
        parts.append(lathe("chart_tack", [(0.0, 0.0), (0.006, 0.0), (0.0055, 0.0018), (0.003, 0.0028), (0.0, 0.003)], (0, 0, 0), "MI_Metal_Brass", 12))
        tack = parts[-1]
        # Turn the tack's axis (+Z) to the board's normal and move it onto the sheet.
        m = Matrix.Translation(U(tuple(p))) @ Vector((0, 0, 1)).rotation_difference(U(tuple(n))).to_matrix().to_4x4()
        tack.data.transform(m)
    obj = finish(parts, "SM_Boat_MastChart", wear=0.4, chips=0.0)
    export(obj)


def girth_at_height(z):
    """The mast's radius at height z (absolute), a little over its turned radius for the hewn surface."""
    return mast_r(z - sheer_z(MAST_X)) + 0.008


def build_anchor():
    """Fisherman's anchor, hanging from its ring (pivot at the ring, shank down -Z): iron shank, crown and arms with
    flukes, a wooden stock with iron bands across the top."""
    parts = [ring("ring", 0.075, 0.018, (0, 0, 0), (0, 1, 0), "MI_Metal_Iron"),
             rod("shank", (0, 0, -0.06), (0, 0, -1.02), 0.034, "MI_Metal_Iron", 10)]
    for p in parts:
        for f in p.data.polygons:
            f.use_smooth = True
    # Arms: an arc up from the crown to both sides, flukes on their ends.
    c = Vector((0.0, 0.0, -0.62))
    for side in (-1, 1):
        pts = [tuple(c + Vector((side * 0.4 * math.sin(math.radians(a)), 0.0, -0.4 * math.cos(math.radians(a))))) for a in range(0, 66, 11)]
        for a_p, b_p in zip(pts[:-1], pts[1:]):
            parts.append(rod("arm", a_p, b_p, 0.032, "MI_Metal_Iron", 10))
        tip = Vector(pts[-1])
        fl = block("fluke", (0.17, 0.035, 0.2), tuple(tip + Vector((side * 0.02, 0.0, -0.06))), "MI_Metal_Iron", 0.01)
        sb.transform_obj(fl, (0, side * 0.6, 0), tuple(U(tuple(tip))))
        parts.append(fl)
    parts.append(tb.rough_plank("stock", 0.85, 0.085, 0.085, (0.0, 0.0, -0.17), (0, 0, math.pi / 2), M["MI_Wood_Varnish"], nseed(),
                                wear=0.7, grooves=(1, 1), split_chance=0.0))
    for y in (-0.33, 0.33):
        parts.append(ring("stock_band", 0.06, 0.012, (0, y, -0.17), (0, 1, 0), "MI_Metal_Iron", 16))
    obj = finish(parts, "SM_Boat_Anchor", wear=0.8, chips=0.0)
    export(obj)


def main():
    args = sb.script_args() or ["hull", "yard", "sail", "rudder", "wheel", "gangway", "anchor", "windlass", "pennant", "oar", "ensign", "bell", "lantern",
                                "mastchart"]
    builds = [("hull", build_hull), ("yard", build_yard), ("sail", build_sail), ("rudder", build_rudder), ("wheel", build_wheel), ("gangway", build_gangway),
              ("anchor", build_anchor), ("windlass", build_windlass), ("pennant", build_pennant), ("oar", build_steering_oar), ("ensign", build_ensign),
              ("bell", build_bell), ("lantern", build_lantern), ("mastchart", build_mast_chart)]
    for part, fn in builds:
        if part in args:
            sb.reset_scene()
            mats()
            fn()


# Run as the build script (Blender's -P runs it as __main__); build_tools.py imports the parts.
if __name__ == "__main__":
    main()
