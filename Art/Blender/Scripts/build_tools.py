"""Held tools (D55, UI_Design.md section 4): what the player holds from the tool wheel.

  SM_Tool_Compass        a brass pocket compass, about 7 cm across: a turned case with a bezel, a pendant and a bow at
                         its north edge (+X), a cream dial with dark ticks (the four quarters longer) and a red north
                         mark; pivot at the dial's centre, the dial facing +Z
  SM_Tool_CompassNeedle  its needle, pivot at its centre, north half red along +X, south half dark, a brass cap; the game
                         turns it about +Z so the red end settles on world north

  SM_Tool_Spyglass       D80 (owner: "Create a real spyglass, only using on left click"): a three-draw brass sea telescope. The barrel
                         (pivot at its rear face, +X to the objective): stitched leather between turned ferrules with beads, a band
                         with the lanyard's ring, a sunshade with a rolled lip over the objective lens.
  SM_Tool_SpyglassDraw1-3  its drawtubes (pivot at each one's rear end): polished tubes with collars, beads and an engraved line;
                         the third ends in the flared eyecup with the eye lens and a dust slide. The game slides them out as the
                         glass goes up to the eye (closed 26 cm, drawn 44 cm).
  SM_Tool_Shovel         D80 (owner: "Create a real shovel"): a worked spade, 1.13 m from the D-handle's crossbar (the pivot) along +X
                         to the tip: split cheeks round a turned crossbar, rivets, an iron ferrule, an ash shaft with hemp whipping,
                         a forged socket with riveted straps, a dished round-mouth blade with a ridge behind, treads and nicks.
                         (D73's plain shovel was built by build_dig.py.)
  SM_Tool_ShovelLoad     a spadeful of sand on the blade's face, in the shovel's frame (shown while digging).

The lantern is the boat's (SM_Boat_Lantern).
Plain shapes and the project's materials, no textures (D38). Run: Tools/Build-BlenderAsset.ps1 build_tools [compass spyglass shovel]
"""
import math
import os
import sys

import bmesh

sys.path.insert(0, os.path.dirname(__file__))
import build_boat as bb  # noqa: E402  (the boat script's parts: lathe, ring, rod, block, finish)
import sb_blender as sb  # noqa: E402


def export(obj):
    sb.save_blend(obj.name)
    sb.export_fbx([obj], os.path.join("Tools", obj.name + ".fbx"))


def flat(name, outline, z0, z1, mat):
    """A flat piece: the outline [(x, y)] (Unreal) extruded from z0 to z1."""
    bm = bmesh.new()
    bottom = [bm.verts.new(bb.U((x, y, z0))) for x, y in outline]
    top = [bm.verts.new(bb.U((x, y, z1))) for x, y in outline]
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([bottom[i], bottom[j], top[j], top[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bb.mesh_object(name, bm, [mat])


def build_compass():
    dial_z = 0.0015
    parts = [
        # The case: a shallow turned dish, its rim rolled over, the dial sitting inside.
        bb.lathe("compass_case", [(0.0, -0.009), (0.027, -0.009), (0.031, -0.0075), (0.0335, -0.004), (0.034, 0.0005), (0.0325, 0.0045),
                                  (0.0298, 0.005), (0.0294, 0.0), (0.0, 0.0)], (0.0, 0.0, 0.0), "MI_Metal_Brass", 40),
        bb.ring("compass_bezel", 0.0306, 0.0021, (0.0, 0.0, 0.0047), (0, 0, 1), "MI_Metal_Brass", 40),
        bb.disc("compass_dial", 0.0294, 0.0015, (0.0, 0.0, dial_z / 2), (0, 0, 1), "MI_Paint_CreamAged", 40),
        # Pendant and bow at the north edge.
        bb.rod("compass_pendant", (0.033, 0.0, -0.002), (0.041, 0.0, -0.002), 0.0032, "MI_Metal_Brass", 10),
        bb.ring("compass_bow", 0.0075, 0.0017, (0.0475, 0.0, -0.002), (0, 0, 1), "MI_Metal_Brass", 20),
    ]
    # Sixteen ticks round the dial, the quarters longer; a red north mark inside the north tick.
    for k in range(16):
        a = 2 * math.pi * k / 16
        quarter = k % 4 == 0
        length = 0.0072 if quarter else 0.0034
        r = 0.0282 - length / 2
        tick = bb.block("compass_tick", (length, 0.0016 if quarter else 0.0011, 0.0007), (r, 0.0, dial_z + 0.00035), "MI_Metal_Iron", 0.0002)
        sb.transform_obj(tick, (0.0, 0.0, a), (0.0, 0.0, 0.0))
        parts.append(tick)
    parts.append(flat("compass_north", [(0.0175, -0.0028), (0.0205, 0.0), (0.0175, 0.0028)], dial_z, dial_z + 0.0008, "MI_Paint_Hull"))
    obj = bb.finish(parts, "SM_Tool_Compass", wear=0.5, chips=0.0)
    export(obj)

    sb.reset_scene()
    bb.mats()
    needle = [flat("needle_north", [(0.0, -0.0031), (0.0215, 0.0), (0.0, 0.0031)], 0.0, 0.0008, "MI_Paint_Hull"),
              flat("needle_south", [(0.0, 0.0031), (-0.019, 0.0), (0.0, -0.0031)], 0.0, 0.0008, "MI_Metal_Iron"),
              bb.lathe("needle_cap", [(0.0, 0.0), (0.0032, 0.0), (0.0028, 0.0011), (0.0012, 0.0017), (0.0, 0.0018)], (0.0, 0.0, 0.0006),
                       "MI_Metal_Brass", 16)]
    obj = bb.finish(needle, "SM_Tool_CompassNeedle", wear=0.3, chips=0.0)
    export(obj)


# --- swept pieces (D80) ---------------------------------------------------------------------------------------------------
def sweep(name, pts, rad, mat, segs=12, up=(0.0, 0.0, 1.0), caps=True):
    """A closed tube along Unreal-space points (metres): rad(t) -> (rx, ry), rx across the path's side, ry along its up (frames
    carried along the path from `up`). UVs: U along the path, V round it, both in metres (wood grain and rope twist keep their
    scale)."""
    from mathutils import Vector
    P = [bb.U(p) for p in pts]
    n = len(P)
    T = [(P[min(i + 1, n - 1)] - P[max(i - 1, 0)]).normalized() for i in range(n)]
    upv = bb.U(up).normalized()
    N = upv - T[0] * upv.dot(T[0])
    if N.length < 1e-6:
        N = T[0].orthogonal()
    N.normalize()
    frames = []
    for i in range(n):
        if i > 0:
            N = T[i - 1].rotation_difference(T[i]) @ N
            N = (N - T[i] * N.dot(T[i])).normalized()
        frames.append((N.cross(T[i]).normalized(), N.copy()))
    s_acc = [0.0]
    for i in range(1, n):
        s_acc.append(s_acc[-1] + (P[i] - P[i - 1]).length)
    total = max(s_acc[-1], 1e-6)
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    rings, radii = [], []
    for i in range(n):
        S, Nn = frames[i]
        rx, ry = rad(s_acc[i] / total)
        radii.append(0.5 * (rx + ry))
        rings.append([bm.verts.new(P[i] + S * (rx * math.cos(2 * math.pi * k / segs)) + Nn * (ry * math.sin(2 * math.pi * k / segs)))
                      for k in range(segs)])
    for i in range(n - 1):
        for k in range(segs):
            k1 = (k + 1) % segs
            f = bm.faces.new((rings[i][k], rings[i][k1], rings[i + 1][k1], rings[i + 1][k]))
            f.smooth = True
            for loop, (ii, kk) in zip(f.loops, ((i, k), (i, k + 1), (i + 1, k + 1), (i + 1, k))):
                loop[uvl].uv = (s_acc[ii], 2 * math.pi * kk / segs * radii[ii])
    if caps:
        for i, rev in ((0, True), (n - 1, False)):
            ring = rings[i][::-1] if rev else rings[i]
            c = bm.verts.new(P[i])
            for k in range(segs):
                f = bm.faces.new((c, ring[k], ring[(k + 1) % segs]))
                for loop in f.loops:
                    loop[uvl].uv = (loop.vert.co.x, loop.vert.co.z)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bb.mesh_object(name, bm, [mat])


def catmull(pts, per=6):
    """A smooth curve through Unreal-space points (Catmull-Rom), per points between each pair."""
    P = [tuple(p) for p in pts]
    P = [P[0]] + P + [P[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(per):
            t = k / per
            out.append(tuple(0.5 * ((2 * p1[c]) + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t * t +
                                    (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t ** 3) for c in range(3)))
    out.append(P[-2])
    return out


def helix(center_fn, x0, x1, radius, pitch, phase=0.0):
    """Points of a helix wound round a path along +X (Unreal): center_fn(x) -> (y, z) of the path's middle."""
    pts = []
    turns = (x1 - x0) / pitch
    m = max(int(turns * 16), 8)
    for i in range(m + 1):
        t = i / m
        x = x0 + (x1 - x0) * t
        a = phase + 2 * math.pi * turns * t
        cy, cz = center_fn(x)
        pts.append((x, cy + radius * math.cos(a), cz + radius * math.sin(a)))
    return pts


# --- the spyglass (D80) ---------------------------------------------------------------------------------------------------
# Owner (2026-10-07): "Create a real spyglass, only using on left click". A three-draw brass sea telescope: the barrel wrapped in
# stitched leather between turned brass ferrules, a sunshade over the objective, three drawtubes that slide out of it when it is
# raised to the eye (the game moves them), the last one ending in a flared eyecup with a dust slide. Units: metres, +X from the
# eye end towards the objective. The barrel's pivot is its rear face; each drawtube's pivot is its rear end (the game places it).
SPY_BARREL_R = 0.0195
SPY_DRAWS = [  # (name, tube radius, collar radius, length)
    ("SM_Tool_SpyglassDraw1", 0.0158, 0.0176, 0.100),
    ("SM_Tool_SpyglassDraw2", 0.0134, 0.0151, 0.095),
    ("SM_Tool_SpyglassDraw3", 0.0110, 0.0128, 0.090),
]


def bead(name, x, r, mat="MI_Metal_Brass", w=0.004, h=0.0012):
    """A turned bead round the axis (+X) at x: a raised rounded ring."""
    prof = [(0.0, x - w / 2)]
    for k in range(7):
        a = math.pi * k / 6
        prof.append((r + h * math.sin(a), x - w / 2 + w * k / 6))
    prof.append((0.0, x + w / 2))
    return bb.lathe(name, prof, (0.0, 0.0, 0.0), mat, 32, axis=(1, 0, 0))


def build_spyglass():
    sb.reset_scene()
    bb.mats()
    bb.M["MI_Char_Leather"] = sb.get_material("MI_Char_Leather", (0.38, 0.23, 0.13, 1.0))
    X = (1, 0, 0)
    R = SPY_BARREL_R
    parts = [
        # The rear ferrule: a turned brass cap the drawtubes come out of, two beads and a chamfered back.
        bb.lathe("spy_rear_ferrule", [(0.0, 0.0), (R - 0.0012, 0.0), (R + 0.0012, 0.0015), (R + 0.0016, 0.004), (R + 0.0016, 0.022),
                                      (R + 0.0002, 0.025), (0.0, 0.025)], (0.0, 0.0, 0.0), "MI_Metal_Brass", 32, axis=X),
        bead("spy_rear_bead0", 0.006, R + 0.0016),
        bead("spy_rear_bead1", 0.019, R + 0.0016),
        # The barrel under its leather, swelling a touch in the middle.
        bb.lathe("spy_leather", [(0.0, 0.024), (R + 0.0004, 0.024), (R + 0.0009, 0.06), (R + 0.0011, 0.09), (R + 0.0009, 0.12),
                                 (R + 0.0004, 0.153), (0.0, 0.153)], (0.0, 0.0, 0.0), "MI_Char_Leather", 32, axis=X),
        # The front ferrule and a band in the middle with the lanyard's eye.
        bb.lathe("spy_front_ferrule", [(0.0, 0.151), (R + 0.0016, 0.151), (R + 0.0018, 0.17), (R + 0.0008, 0.173), (0.0, 0.173)],
                 (0.0, 0.0, 0.0), "MI_Metal_Brass", 32, axis=X),
        bead("spy_front_bead", 0.158, R + 0.0018),
        bb.lathe("spy_mid_band", [(0.0, 0.083), (R + 0.0016, 0.083), (R + 0.0022, 0.085), (R + 0.0022, 0.093), (R + 0.0016, 0.095),
                                  (0.0, 0.095)], (0.0, 0.0, 0.0), "MI_Metal_Brass", 32, axis=X),
        bb.block("spy_lanyard_lug", (0.008, 0.005, 0.006), (0.089, 0.0, -(R + 0.0045)), "MI_Metal_Brass", 0.0015),
        bb.ring("spy_lanyard_ring", 0.0055, 0.0011, (0.089, 0.0, -(R + 0.0098)), (0, 1, 0), "MI_Metal_Brass", 16),
        # The sunshade over the objective: a wider brass hood with a rolled lip, the lens set back in it, slightly domed.
        bb.lathe("spy_shade", [(0.0, 0.17), (R + 0.0024, 0.17), (R + 0.0028, 0.176), (R + 0.0028, 0.21), (R + 0.0042, 0.213),
                               (R + 0.0042, 0.216), (R + 0.002, 0.217), (R + 0.0016, 0.214), (R + 0.0012, 0.196), (0.0, 0.196)],
                 (0.0, 0.0, 0.0), "MI_Metal_Brass", 32, axis=X),
        bead("spy_shade_bead", 0.181, R + 0.0028, h=0.0009, w=0.003),
        bb.disc("spy_objective", R - 0.0004, 0.0012, (0.1985, 0.0, 0.0), X, "MI_Glass_Dark", 32),
    ]
    # The leather's seam underneath: a ridge and cross stitches in waxed thread.
    seam_z = -(R + 0.0012)
    parts.append(sweep("spy_seam", [(0.026 + 0.125 * i / 12, 0.0, seam_z) for i in range(13)], lambda t: (0.0009, 0.0007), "MI_Char_Leather", 8))
    for k in range(14):
        x = 0.031 + k * 0.0088
        st = bb.block("spy_stitch", (0.0011, 0.0055, 0.0008), (x, 0.0, seam_z - 0.0004), "MI_Rope_Hemp", 0.0003)
        sb.transform_obj(st, (0.0, 0.0, math.radians(35 if k % 2 else -35)), bb.U((x, 0.0, seam_z)))
        parts.append(st)
    for p in parts:
        sb.jitter_vertices(p, 0.00012, sum(map(ord, p.name)) % 97)
    obj = bb.finish(parts, "SM_Tool_Spyglass", wear=0.6, chips=0.0)
    export(obj)

    for i, (name, r, rc, length) in enumerate(SPY_DRAWS):
        sb.reset_scene()
        bb.mats()
        parts = [
            # The polished tube, a line engraved round it near the collar.
            bb.lathe(name + "_tube", [(0.0, 0.008), (r, 0.008), (r, length), (0.0, length)], (0.0, 0.0, 0.0), "MI_Metal_Brass", 32, axis=X),
            # The collar at its rear: it stops the tube at the next one's mouth.
            bb.lathe(name + "_collar", [(0.0, 0.0), (rc - 0.001, 0.0), (rc, 0.0012), (rc, 0.011), (r + 0.0006, 0.013), (0.0, 0.013)],
                     (0.0, 0.0, 0.0), "MI_Metal_Brass", 32, axis=X),
            bead(name + "_bead", 0.0055, rc, h=0.0009, w=0.003),
            bb.lathe(name + "_line", [(0.0, 0.03), (r + 0.00018, 0.03), (r + 0.00018, 0.0315), (0.0, 0.0315)], (0.0, 0.0, 0.0),
                     "MI_Metal_Iron", 32, axis=X),
        ]
        if i == len(SPY_DRAWS) - 1:
            # The eyecup: flared and rolled over at the back, the small eye lens set deep into it.
            parts.append(bb.lathe("spy_eyecup", [(0.0, -0.005), (0.0055, -0.005), (0.0062, -0.0065), (0.0088, -0.0098), (rc + 0.0016, -0.0108),
                                                 (rc + 0.0022, -0.0095), (rc + 0.0012, -0.004), (rc, 0.002), (0.0, 0.002)], (0.0, 0.0, 0.0),
                                  "MI_Metal_Brass", 32, axis=X))
            parts.append(bb.disc("spy_eyelens", 0.0054, 0.0008, (-0.0053, 0.0, 0.0), X, "MI_Glass_Dark", 24))
        obj = bb.finish(parts, name, wear=0.55, chips=0.0)
        export(obj)


# --- the shovel (D80) -----------------------------------------------------------------------------------------------------
# Owner (2026-10-07): "Create a real shovel". A worked spade, 1.13 m: an ash shaft with a D-handle (two split cheeks round a
# turned crossbar, riveted, an iron ferrule where the shaft splits), hemp whipping where the lower hand holds, a forged socket
# with two riveted straps up the shaft, a dished round-mouth blade with a pointed tip, a central ridge behind, the shoulders
# turned over as treads for the foot, nicks along the edge. +X from the crossbar (the pivot, x 0) to the tip; +Z is the blade's
# concave face. SM_Tool_ShovelLoad: a spadeful of sand on the face (same frame), shown by the game while digging.
SHOVEL_TIP = 1.13


def smooth_end(t, start=0.86):
    """0 along a swept piece, rising to 1 at its very end (a rounded-off end)."""
    q = max(0.0, (t - start) / (1.0 - start))
    return 1.0 - math.sqrt(max(0.0, 1.0 - q * q)) * 0.75 - 0.25 * (1.0 - q) if q > 0 else 0.0


def shaft_mid(x):
    """The shaft's slight crook (y, z) at x: hand-made, not turned."""
    return (0.0035 * math.sin(x * 4.1 + 0.6), 0.0025 * math.sin(x * 6.3 + 1.9))


def blade_half(u):
    """Half the blade's width at u (0 the shoulders .. 1 the tip): straight, then a rounded point, with two small nicks."""
    w = 0.101 * (1.0 + 0.03 * math.sin(math.pi * min(u / 0.45, 1.0)))
    if u > 0.42:
        q = (u - 0.42) / 0.58
        w *= max(0.0, 1.0 - q ** 2.1) ** 0.5
    for c, d in ((0.83, 0.004), (0.66, 0.003)):
        w -= d * max(0.0, 1.0 - abs(u - c) / 0.03)
    return max(w, 0.0)


def build_blade():
    from mathutils import Vector
    x0, x1 = 0.865, SHOVEL_TIP
    nu, nv = 30, 21
    bm = bmesh.new()

    def surf(u, v, side):
        x = x0 + (x1 - x0) * u
        hw = blade_half(u)
        y = hw * v
        k = min(1.0, hw / 0.03)        # towards the point every row closes to one spot
        dish = 0.02 * v * v * (1.0 - 0.55 * u) * k
        thick = (0.0058 - 0.0028 * u) * (1.0 - 0.3 * abs(v) ** 3 * k)
        frog = 0.009 * max(0.0, 1.0 - u / 0.55) ** 1.5 * max(0.0, 1.0 - (y / 0.024) ** 2) if side < 0 else 0.0
        return Vector(bb.U((x, y, dish + side * thick / 2 - frog)))

    top = [[bm.verts.new(surf(i / nu, -1 + 2 * j / nv, 1)) for j in range(nv + 1)] for i in range(nu + 1)]
    bot = [[bm.verts.new(surf(i / nu, -1 + 2 * j / nv, -1)) for j in range(nv + 1)] for i in range(nu + 1)]
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1]))
            bm.faces.new((bot[i][j], bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j]))
    # Walls round the outline: the shoulder line, both sides and the tip (the last row collapses to the point).
    edge = [(0, j) for j in range(nv + 1)] + [(i, nv) for i in range(1, nu + 1)] + [(nu, j) for j in range(nv - 1, -1, -1)] + \
           [(i, 0) for i in range(nu - 1, 0, -1)]
    for a, b in zip(edge, edge[1:] + edge[:1]):
        va, vb = top[a[0]][a[1]], top[b[0]][b[1]]
        wa, wb = bot[a[0]][a[1]], bot[b[0]][b[1]]
        if len({va, vb, wb, wa}) == 4 and (va.co - vb.co).length > 1e-7:
            bm.faces.new((va, vb, wb, wa))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
    blade = bb.mesh_object("shovel_blade", bm, ["MI_Metal_Iron"])
    sb.world_box_uv(blade, 0.4)
    return blade


def build_shovel():
    sb.reset_scene()
    bb.mats()
    parts = []
    wood, dark, iron, rope = "MI_Wood_TeakLight", "MI_Wood_TeakDark", "MI_Metal_Iron", "MI_Rope_Hemp"
    # The shaft, a little oval and crooked, thinning towards the handle.
    xs = [0.145 + (0.79 - 0.145) * i / 26 for i in range(27)]
    parts.append(sweep("shovel_shaft", [(x, *shaft_mid(x)) for x in xs],
                       lambda t: (0.0168 + 0.0012 * t, 0.0158 + 0.0012 * t), wood, 14))
    # The D-handle: the shaft's split cheeks bend out round a turned crossbar; an iron ferrule binds the split.
    for sgn in (1, -1):
        pts = [(0.165, 0.004 * sgn, 0.0), (0.14, 0.012 * sgn, 0.0), (0.11, 0.028 * sgn, 0.0), (0.075, 0.047 * sgn, 0.0),
               (0.042, 0.058 * sgn, 0.0), (0.018, 0.0625 * sgn, 0.0), (0.0, 0.063 * sgn, 0.0), (-0.012, 0.0615 * sgn, 0.0),
               (-0.019, 0.059 * sgn, 0.0)]
        # The cheeks' ends round off just past the crossbar.
        parts.append(sweep("shovel_cheek", catmull(pts, 6), lambda t: ((0.0105 - 0.0015 * t) * (1.0 - smooth_end(t)),
                                                           (0.0125 - 0.001 * t) * (1.0 - smooth_end(t))), wood, 12))
        parts.append(bb.disc("shovel_grip_rivet", 0.0042, 0.003, (0.0, 0.0735 * sgn, 0.0), (0, 1, 0), iron, 10, bevel=0.0012))
    parts.append(bb.lathe("shovel_crossbar", [(0.0, -0.06), (0.0128, -0.06), (0.0148, -0.045), (0.0162, -0.02), (0.0166, 0.0),
                                              (0.0162, 0.02), (0.0148, 0.045), (0.0128, 0.06), (0.0, 0.06)], (0.0, 0.0, 0.0), dark, 20,
                          axis=(0, 1, 0)))
    parts.append(bb.lathe("shovel_ferrule", [(0.0, 0.135), (0.0186, 0.135), (0.0196, 0.138), (0.0196, 0.162), (0.0186, 0.165),
                                             (0.0, 0.165)], (0.0, 0.0, 0.0), iron, 18, axis=(1, 0, 0)))
    # Hemp whipping where the lower hand holds, its ends tucked under.
    parts.append(sweep("shovel_whipping", helix(shaft_mid, 0.415, 0.485, 0.0182, 0.0052), lambda t: (0.0026, 0.0026), rope, 8))
    for x in (0.411, 0.489):
        cy, cz = shaft_mid(x)
        parts.append(bb.ring("shovel_whip_end", 0.0176, 0.0024, (x, cy, cz), (1, 0, 0), rope, 20))
    # The forged socket and its two straps riveted up the shaft (front and back).
    parts.append(bb.lathe("shovel_socket", [(0.0, 0.77), (0.0182, 0.77), (0.0196, 0.775), (0.0198, 0.81), (0.0215, 0.84), (0.026, 0.862),
                                            (0.024, 0.872), (0.0, 0.872)], (0.0, 0.0, 0.0), iron, 18, axis=(1, 0, 0)))
    parts.append(bead("shovel_socket_bead", 0.778, 0.0198, iron, w=0.006, h=0.0014))
    for sgn in (1, -1):
        pts = [(0.64 + 0.135 * i / 8, shaft_mid(0.64 + 0.135 * i / 8)[0], shaft_mid(0.64 + 0.135 * i / 8)[1] + sgn * 0.0186) for i in range(9)]
        parts.append(sweep("shovel_strap", pts, lambda t: (0.0058 + 0.0022 * t, 0.0014), iron, 8))
        for x in (0.665, 0.72):
            cy, cz = shaft_mid(x)
            parts.append(bb.disc("shovel_strap_rivet", 0.0034, 0.003, (x, cy, cz + sgn * 0.0204), (0, 0, sgn), iron, 10, bevel=0.0011))
    # The blade, and its shoulders turned over as treads (towards the back) either side of the socket.
    parts.append(build_blade())
    for sgn in (1, -1):
        parts.append(sweep("shovel_tread", [(0.868, 0.028 * sgn, -0.0025), (0.866, 0.065 * sgn, 0.0042), (0.865, 0.099 * sgn, 0.0156)],
                           lambda t: (0.0055, 0.0035), iron, 8))
    obj = bb.finish(parts, "SM_Tool_Shovel", wear=0.85, chips=0.25)
    export(obj)

    # A spadeful of sand on the face, lumpy, its edges thin.
    sb.reset_scene()
    bb.mats()
    bb.M["MI_Dirt_Load"] = sb.get_material("MI_Dirt_Load", (0.62, 0.5, 0.34, 1.0))
    from mathutils import Vector
    bm = bmesh.new()
    rings, segs = 9, 24
    cx, rx, ry = 0.985, 0.085, 0.07
    centre = bm.verts.new(Vector(bb.U((cx, 0.0, 0.058))))
    rows = []
    for i in range(1, rings + 1):
        r01 = i / rings
        row = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            lump = 1.0 + 0.12 * math.sin(3 * a + 1.3) + 0.07 * math.sin(7 * a + 0.4)
            x, y = cx + rx * r01 * math.cos(a) * lump, ry * r01 * math.sin(a) * lump
            u = (x - 0.865) / (SHOVEL_TIP - 0.865)
            face = 0.02 * (y / max(blade_half(u), 0.03)) ** 2 * (1.0 - 0.55 * u) + 0.003
            h = 0.055 * max(0.0, 1.0 - r01 ** 2) ** 1.2 * (1.0 + 0.2 * math.sin(5 * a + 2 * i))
            row.append(bm.verts.new(Vector(bb.U((x, y, face + h)))))
        rows.append(row)
    for k in range(segs):
        bm.faces.new((centre, rows[0][k], rows[0][(k + 1) % segs]))
    for i in range(rings - 1):
        for k in range(segs):
            k1 = (k + 1) % segs
            bm.faces.new((rows[i][k], rows[i + 1][k], rows[i + 1][k1], rows[i][k1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
    load = bb.mesh_object("SM_Tool_ShovelLoad", bm, ["MI_Dirt_Load"])
    sb.world_box_uv(load, 0.3)
    obj = bb.finish([load], "SM_Tool_ShovelLoad", wear=0.0, chips=0.0)
    export(obj)


BUILDERS = {"compass": build_compass, "spyglass": build_spyglass, "shovel": build_shovel}


def main():
    names = sb.script_args() or list(BUILDERS)
    for n in names:
        sb.reset_scene()
        bb.mats()
        BUILDERS[n]()


main()
