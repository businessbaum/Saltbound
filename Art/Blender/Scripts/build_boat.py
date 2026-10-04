"""Starter boat greybox for the M4 sailing model (D36). Not the final art: the cabin concept and the hull colour
are still owner decisions (Docs/Progress.md), so the deck is open and the paint is the current oxblood / cream.

The hull follows Saltbound/Source/Saltbound/Boat/SaltHullShape.h exactly (same formulas and constants), because
ASaltBoat floats on columns computed from that shape: keep both in sync.

  SM_Boat_Hull    hull, deck, bulwarks with boarding gaps midship, keel, mast, standing rigging, wheel pedestal,
                  bitts; UCX collision (walkable deck, bulwarks, keel, mast foot). Pivot: midship, design waterline.
  SM_Boat_Boom    pivot at the gooseneck, spar along -X
  SM_Boat_Gaff    pivot at the jaws, spar along -X (the actor raises it to the gaff angle)
  SM_Boat_Sail    gaff mainsail in the boom's frame (X/Z plane), bellied towards +Y; the actor scales Y to leeward
                  and Z to lower it
  SM_Boat_Rudder  pivot at the heel of the stock, stock along +Z, blade aft (-X)
  SM_Boat_Wheel   pivot at the hub, axle along X

Coordinates below are Unreal body coordinates in metres (x forward, y starboard, z up); U() converts to Blender,
where starboard is -Y.
Run: Tools/Build-BlenderAsset.ps1 build_boat
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

# --- hull shape: mirrors SaltHullShape.h ------------------------------------------------------------------
HALF_LENGTH, HALF_BEAM, FREEBOARD, SHEER_RISE, DRAFT = 5.0, 1.7, 0.75, 0.3, 0.65
BILGE_EXP, ENDS_START_X = 0.8, 3.0
KEEL_AFT_X, KEEL_AFT_Z, KEEL_FORE_X, KEEL_HALF_T = -4.3, -1.45, 3.6, 0.14
BULWARK_H, BULWARK_T, GATE_HALF = 0.45, 0.07, 0.5
MAST_X, MAST_H, BOOM_H, BOOM_L, THROAT_H, GAFF_L, GAFF_ANGLE = 1.2, 10.5, 2.05, 5.0, 6.8, 4.6, 35.0
WHEEL_X, WHEEL_H = -3.2, 0.85
RUDDER_HEEL, RUDDER_HEAD, RUDDER_CHORD = (-4.38, -1.45), (-4.98, 1.0), 0.6
CAMBER = 0.04            # deck crown
STRIPE = 0.13            # cream sheer stripe
BOOTTOP = 0.03           # top of the dark bottom paint above the waterline

M = {}


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
    return sheer_z(x) + (CAMBER * (1.0 - (y / bs) ** 2) if bs > 1e-4 else 0.0)


def section_point(x, theta, side):
    """Hull surface at station x, angle theta (0 keel .. pi/2 sheer), side +1 starboard / -1 port."""
    zs, zb = sheer_z(x), bottom_z(x)
    return (x, side * half_beam(x) * math.sin(theta) ** BILGE_EXP, zs - (zs - zb) * math.cos(theta))


def theta_at(x, z):
    """Section angle where the hull surface reaches height z (0 if the section starts above it)."""
    zs, zb = sheer_z(x), bottom_z(x)
    if zs - zb < 1e-5:
        return 0.0
    return math.acos(clamp((zs - z) / (zs - zb)))


def U(p):
    return Vector((p[0], -p[1], p[2]))


def mats():
    for n, c in {"MI_Paint_Hull": (0.45, 0.12, 0.08), "MI_Paint_Cream": (0.9, 0.85, 0.7), "MI_Paint_Boottop": (0.1, 0.1, 0.1),
                 "MI_Wood_Teak": (0.55, 0.35, 0.18), "MI_Wood_Varnish": (0.4, 0.2, 0.08), "MI_Metal_Iron": (0.1, 0.1, 0.1),
                 "MI_Metal_Brass": (0.7, 0.55, 0.25), "MI_Canvas": (0.85, 0.8, 0.7), "MI_Rope_Hemp": (0.6, 0.5, 0.3)}.items():
        M[n] = sb.get_material(n, (*c, 1))


def mesh_object(name, bm, materials):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = sb.new_object(name, me)
    for m in materials:
        obj.data.materials.append(M[m])
    return obj


def stations(n=48):
    """Stations from stern to stem, denser towards the ends (cosine spacing)."""
    return [-HALF_LENGTH * math.cos(math.pi * i / (n - 1)) for i in range(n)]


def section_thetas(x):
    """Per side: 8 steps keel -> boot top, 6 -> sheer stripe, 2 -> sheer (the paint lines are exact edges)."""
    t_wl = theta_at(x, BOOTTOP)
    t_st = max(theta_at(x, sheer_z(x) - STRIPE), t_wl)
    out = [t_wl * k / 8 for k in range(8)]
    out += [t_wl + (t_st - t_wl) * k / 6 for k in range(6)]
    out += [t_st + (math.pi / 2 - t_st) * k / 2 for k in range(3)]
    return out    # 17 values, 0 .. pi/2


# --- hull ---------------------------------------------------------------------------------------------
def hull_body():
    """Closed shell (keel line to sheer) plus the cambered deck. Material per face band: bottom paint, topsides,
    cream stripe, teak deck."""
    bm = bmesh.new()
    xs = stations()
    rows = []                         # per station: port sheer .. keel .. starboard sheer, then deck interior
    deck_k = 8
    for x in xs:
        th = section_thetas(x)
        port = [bm.verts.new(U(section_point(x, t, -1))) for t in reversed(th)]
        stbd = [bm.verts.new(U(section_point(x, t, 1))) for t in th[1:]]
        shell = port + stbd
        bs = half_beam(x)
        deck = [shell[0]] + [bm.verts.new(U((x, -bs + 2 * bs * k / deck_k, deck_z(x, -bs + 2 * bs * k / deck_k))))
                             for k in range(1, deck_k)] + [shell[-1]]
        rows.append((shell, deck))
    n_side = len(section_thetas(0.0)) - 1           # 16 faces per side
    for (s0, d0), (s1, d1) in zip(rows[:-1], rows[1:]):
        for k in range(len(s0) - 1):
            f = bm.faces.new((s0[k], s0[k + 1], s1[k + 1], s1[k]))
            j = k if k < n_side else 2 * n_side - 1 - k     # band index from the keel: 0..15
            band = n_side - 1 - j
            f.material_index = 2 if band < 8 else (0 if band < 14 else 1)
        for k in range(len(d0) - 1):
            f = bm.faces.new((d0[k], d1[k], d1[k + 1], d0[k + 1]))
            f.material_index = 3
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object("hull_body", bm, ["MI_Paint_Hull", "MI_Paint_Cream", "MI_Paint_Boottop", "MI_Wood_Teak"])
    obj.data.shade_smooth()
    obj.data.set_sharp_from_angle(angle=math.radians(35))
    return obj


def bulwark_section(x, top_extra=0.0):
    """Cross-section corners at station x (starboard): outer bottom, outer top, inner top, inner bottom."""
    bs, zs = half_beam(x), sheer_z(x)
    yi = max(bs - BULWARK_T, 0.0)
    return [(x, bs, zs - 0.03), (x, bs, zs + BULWARK_H + top_extra), (x, yi, zs + BULWARK_H + top_extra),
            (x, yi, deck_z(x, yi) - 0.02)]


def sweep(name, sections, mat_faces, materials, side):
    """Closed solid swept through cross-sections (lists of 4 starboard corners), mirrored to side -1 for port."""
    bm = bmesh.new()
    rings = [[bm.verts.new(U((p[0], side * p[1], p[2]))) for p in sec] for sec in sections]
    for r0, r1 in zip(rings[:-1], rings[1:]):
        for k in range(4):
            f = bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
            f.material_index = mat_faces[k]
    for r in (rings[0], rings[-1]):
        f = bm.faces.new(r)
        f.material_index = mat_faces[1]
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return mesh_object(name, bm, materials)


def bulwark_spans():
    """X ranges of the bulwark on each side: aft and forward of the boarding gap."""
    # The ends reach (almost) the stem and stern posts, so both sides meet there.
    end = HALF_LENGTH - 1e-4
    xs = [x for x in stations(64) if abs(x) < HALF_LENGTH - 0.01]
    aft = [-end] + [x for x in xs if x <= -GATE_HALF] + [-GATE_HALF]
    fore = [GATE_HALF] + [x for x in xs if x >= GATE_HALF] + [end]
    return [sorted(set(round(x, 5) for x in aft)), sorted(set(round(x, 5) for x in fore))]


def bulwarks():
    parts = []
    for side in (-1, 1):
        for span in bulwark_spans():
            # Faces: outer (hull paint), top (under the cap rail), inner (cream), bottom.
            parts.append(sweep("bulwark", [bulwark_section(x) for x in span], [0, 1, 1, 1], ["MI_Paint_Hull", "MI_Paint_Cream"], side))
            caps = []
            for x in span:
                bs, zs = half_beam(x), sheer_z(x)
                yi = max(bs - BULWARK_T - 0.03, 0.0)
                top = zs + BULWARK_H
                caps.append([(x, bs + 0.03, top - 0.005), (x, bs + 0.03, top + 0.05), (x, yi, top + 0.05), (x, yi, top - 0.005)])
            parts.append(sweep("caprail", caps, [0, 0, 0, 0], ["MI_Wood_Varnish"], side))
    return parts


def keel():
    xs = [KEEL_AFT_X + (KEEL_FORE_X - KEEL_AFT_X) * i / 24 for i in range(25)]
    secs = []
    for x in xs:
        top = bottom_z(x) + 0.1
        bot = min(keel_z(x), top - 0.02)
        t = KEEL_HALF_T * (0.55 + 0.45 * clamp((KEEL_FORE_X - x) / 1.2))     # finer towards the forefoot
        secs.append([(x, t, bot), (x, t, top), (x, -t, top), (x, -t, bot)])
    return sweep("keel", secs, [0, 0, 0, 0], ["MI_Paint_Boottop"], 1)


def sternpost():
    """Deadwood between the curved stern profile (and the keel's aft end) and the raked rudder stock, so the rudder
    hangs on the hull instead of behind a gap. Bottom paint below the boot top, hull paint above."""
    (hx, hz), (tx, tz) = RUDDER_HEEL, RUDDER_HEAD

    def fore(z):
        if z <= bottom_z(KEEL_AFT_X):
            return KEEL_AFT_X
        return -(ENDS_START_X + (HALF_LENGTH - ENDS_START_X) * ((z + DRAFT) / (DRAFT + sheer_z(HALF_LENGTH))) ** (1.0 / 3.0))

    def aft(z):
        return hx + (tx - hx) * (z - hz) / (tz - hz) + 0.04

    parts = []
    for z0, z1, mat in ((hz, BOOTTOP, "MI_Paint_Boottop"), (BOOTTOP, 0.92, "MI_Paint_Hull")):
        secs = []
        for k in range(9):
            z = z0 + (z1 - z0) * k / 8
            xf, xa = fore(z) + 0.02, aft(z)
            xa = min(xa, xf - 0.01)
            secs.append([(xa, 0.06, z), (xf, 0.08, z), (xf, -0.08, z), (xa, -0.06, z)])
        parts.append(sweep("sternpost", secs, [0, 0, 0, 0], [mat], 1))
    return parts


def rod(name, a, b, radius, mat, segments=8):
    """Cylinder between two Unreal-space points."""
    a, b = U(a), U(b)
    d = b - a
    obj = sb.add_cylinder(name, radius, d.length, (0, 0, 0), "Z", segments, M[mat])
    rot = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_matrix().to_4x4()
    obj.data.transform(Matrix.Translation((a + b) / 2) @ rot)
    return obj


def box(name, size, center, mat):
    return sb.add_box(name, size, U(center), M[mat])


def rig_and_fittings():
    parts = []
    deck_m = sheer_z(MAST_X)
    parts.append(sb.add_cylinder("mast", 0.1, MAST_H, U((MAST_X, 0, deck_m + MAST_H / 2 - 0.1)), "Z", 16, M["MI_Wood_Varnish"], radius_top=0.065))
    parts.append(sb.add_cylinder("truck", 0.085, 0.08, U((MAST_X, 0, deck_m + MAST_H - 0.06)), "Z", 16, M["MI_Wood_Varnish"]))
    parts.append(sb.add_cylinder("partners", 0.17, 0.08, U((MAST_X, 0, deck_m + 0.08)), "Z", 16, M["MI_Wood_Varnish"]))
    parts.append(sb.add_cylinder("gooseneck", 0.105, 0.08, U((MAST_X, 0, deck_m + BOOM_H)), "Z", 16, M["MI_Metal_Iron"]))
    # Shrouds forward of the mast (the boom swings aft of them) and a forestay to the stem head.
    hounds = (MAST_X, 0, deck_m + THROAT_H + 0.45)
    for side in (-1, 1):
        for dx in (0.15, 0.5):
            x = MAST_X + dx
            # Chainplates outside the hull, the shroud's lower end (deadeye) above the rail.
            bs, zs = half_beam(x), sheer_z(x)
            plate = (x, side * (bs + 0.08), zs + 0.62)
            parts.append(rod("shroud", hounds, plate, 0.014, "MI_Metal_Iron"))
            parts.append(sb.add_cylinder("deadeye", 0.05, 0.04, U(plate), "Y", 12, M["MI_Wood_Varnish"]))
            parts.append(box("chainplate", (0.05, 0.02, 0.8), (x, side * (bs + 0.07), zs + 0.22), "MI_Metal_Iron"))
    parts.append(rod("forestay", (MAST_X, 0, deck_m + THROAT_H + 0.8), (HALF_LENGTH - 0.08, 0, sheer_z(HALF_LENGTH) + 0.25), 0.014, "MI_Metal_Iron"))
    # Wheel pedestal and bitts for the mooring lines.
    hub_z = sheer_z(WHEEL_X) + WHEEL_H
    parts.append(box("pedestal", (0.2, 0.26, hub_z - 0.1 - sheer_z(WHEEL_X)), (WHEEL_X, 0, (hub_z - 0.1 + sheer_z(WHEEL_X)) / 2), "MI_Wood_Varnish"))
    parts.append(box("pedestal_head", (0.26, 0.16, 0.16), (WHEEL_X, 0, hub_z - 0.04), "MI_Wood_Varnish"))
    for x in (4.15, -4.25):
        for y in (-0.22, 0.22):
            parts.append(box("bitt", (0.14, 0.14, 0.38), (x, y, deck_z(x, y) + 0.17), "MI_Wood_Varnish"))
        parts.append(box("bitt_bar", (0.08, 0.6, 0.08), (x, 0, deck_z(x, 0) + 0.26), "MI_Wood_Varnish"))
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


def hull_collision(mesh_name):
    cols = []
    n = 0

    def add(points):
        nonlocal n
        n += 1
        cols.append(convex("UCX_%s_%02d" % (mesh_name, n), points))

    # Hull body in slices: section and deck points at three stations each (the deck is the walkable top).
    bounds = [-5.0, -3.75, -2.5, -1.25, 0.0, 1.25, 2.5, 3.75, 5.0]
    for x0, x1 in zip(bounds[:-1], bounds[1:]):
        pts = []
        for x in (x0, (x0 + x1) / 2, x1):
            x = clamp(x, -HALF_LENGTH + 0.02, HALF_LENGTH - 0.02)
            for t in section_thetas(x):
                pts += [section_point(x, t, 1), section_point(x, t, -1)]
            bs = half_beam(x)
            pts += [(x, y, deck_z(x, y)) for y in (-0.6 * bs, -0.3 * bs, 0.0, 0.3 * bs, 0.6 * bs)]
        add(pts)
    # Bulwarks in pieces along the sheer (both sides, both spans).
    for side in (-1, 1):
        for span in bulwark_spans():
            k = max(1, len(span) // 4)
            cuts = span[::k] + ([span[-1]] if span[-1] not in span[::k] else [])
            for a, b in zip(cuts[:-1], cuts[1:]):
                pts = []
                for x in (a, (a + b) / 2, b):
                    pts += [(p[0], side * p[1], p[2]) for p in bulwark_section(x, 0.05)]
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
    add([(MAST_X + 0.11 * math.cos(a), 0.11 * math.sin(a), z) for a in [i * math.pi / 4 for i in range(8)] for z in (deck_m - 0.1, deck_m + 2.6)])
    hub_z = sheer_z(WHEEL_X) + WHEEL_H
    add([(WHEEL_X + dx, dy, z) for dx in (-0.13, 0.13) for dy in (-0.14, 0.14) for z in (sheer_z(WHEEL_X) - 0.05, hub_z + 0.04)])
    for x in (4.15, -4.25):
        add([(x + dx, dy, z) for dx in (-0.07, 0.07) for dy in (-0.3, 0.3) for z in (deck_z(x, 0) - 0.05, deck_z(x, 0) + 0.36)])
    return cols


def finish(parts, name, waterline=None):
    for p in parts:
        sb.world_box_uv(p, 1.0)
    obj = sb.join(parts, name)
    sb.bake_vertex_masks(obj, wear=0.5, waterline=waterline, wet_height=0.15, seed=7)
    return obj


def export(obj, cols=()):
    sb.save_blend(obj.name)
    sb.export_fbx([obj] + list(cols), os.path.join("Boat", obj.name + ".fbx"))


def build_hull():
    parts = [hull_body(), keel()] + sternpost() + bulwarks() + rig_and_fittings()
    obj = finish(parts, "SM_Boat_Hull", waterline=0.0)
    cols = hull_collision("SM_Boat_Hull")
    print("SB_BOAT_HULL faces", len(obj.data.polygons), "collision pieces", len(cols))
    export(obj, cols)


# --- moving parts -------------------------------------------------------------------------------------------
def build_spar(name, length, radius, end_radius):
    parts = [sb.add_cylinder("spar", radius, length, (0, 0, 0), "Z", 12, M["MI_Wood_Varnish"], radius_top=end_radius)]
    # Along -X from the pivot (Blender -X == Unreal -X).
    sb.transform_obj(parts[0], (0, -math.pi / 2, 0))
    for v in parts[0].data.vertices:
        v.co.x -= length / 2
    jaw = sb.add_box("jaw", (0.22, 0.05, 0.16), (-0.11, 0.0, 0.0), M["MI_Wood_Varnish"])
    parts.append(jaw)
    obj = finish(parts, name)
    export(obj)


def build_sail():
    """Gaff mainsail: rows from the foot (boom) to the head (gaff), each from the luff (mast) to the leech, with
    a belly of 8 % of the chord towards +Y (Unreal), deepest at 40 % from the luff. Given a thickness so both
    faces render."""
    g = math.radians(GAFF_ANGLE)
    rise = THROAT_H - BOOM_H
    tack, throat = Vector((0.0, 0.0)), Vector((0.0, rise))
    peak = Vector((GAFF_L * math.cos(g), rise + GAFF_L * math.sin(g)))
    clew = Vector((BOOM_L - 0.15, 0.0))
    nu, nv = 14, 16
    bm = bmesh.new()
    grid = []
    for j in range(nv + 1):
        v = j / nv
        luff = tack.lerp(throat, v)
        leech = clew.lerp(peak, v)
        chord = (leech - luff).length
        row = []
        for i in range(nu + 1):
            u = i / nu
            p = luff.lerp(leech, u)
            belly = 0.08 * chord * (u / 0.4 if u < 0.4 else (1.0 - u) / 0.6) ** 0.8 * (1.0 - 0.3 * v)
            # (aft distance, height) -> Unreal (-aft, belly, height)
            row.append(bm.verts.new(U((-p.x, belly, p.y))))
        grid.append(row)
    for j in range(nv):
        for i in range(nu):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    obj = mesh_object("SM_Boat_Sail", bm, ["MI_Canvas"])
    solid = obj.modifiers.new("Solidify", "SOLIDIFY")
    solid.thickness = 0.012
    solid.offset = 0.0
    sb.apply_modifiers(obj)
    obj.data.shade_smooth()
    sb.world_box_uv(obj, 1.0)
    sb.bake_vertex_masks(obj, wear=0.0, seed=3)
    export(obj)


def build_rudder():
    hx, hz = RUDDER_HEEL
    tx, tz = RUDDER_HEAD
    stock_len = math.hypot(tx - hx, tz - hz)
    rake = math.atan2(hx - tx, tz - hz)
    blade_top = (0.0 - hz) / math.cos(rake)          # where the stock meets the waterline
    secs = []
    for k in range(9):
        s = blade_top * k / 8
        chord = RUDDER_CHORD * (0.85 + 0.15 * math.sin(math.pi * k / 8))
        secs.append([(-chord, 0.025, s), (0.0, 0.045, s), (0.0, -0.045, s), (-chord, -0.025, s)])
    blade = sweep("blade", [[(p[0], p[1], p[2]) for p in sec] for sec in secs], [0, 0, 0, 0], ["MI_Paint_Boottop"], 1)
    # sweep() builds along the station axis given in p[0]; here the sections vary in z, which works the same.
    head = sb.add_box("rudder_head", (0.16, 0.09, stock_len - blade_top), U((-0.06, 0, (stock_len + blade_top) / 2)), M["MI_Paint_Hull"])
    obj = finish([blade, head], "SM_Boat_Rudder")
    export(obj)


def build_wheel():
    parts = []
    # Wheel in the Y/Z plane, axle along X.
    parts.append(sb.add_torus("rim", 0.4, 0.028, (0, 0, 0), (0, math.pi / 2, 0), 32, 8, M["MI_Wood_Varnish"]))
    parts.append(sb.add_cylinder("hub", 0.07, 0.12, (0, 0, 0), "X", 16, M["MI_Metal_Brass"]))
    parts.append(sb.add_cylinder("axle", 0.03, 0.2, (0.1, 0, 0), "X", 10, M["MI_Metal_Iron"]))
    for k in range(8):
        a = k * math.pi / 4
        d = Vector((0.0, math.cos(a), math.sin(a)))
        spoke = rod("spoke", (0, 0, 0), tuple(d * 0.4), 0.017, "MI_Wood_Varnish")
        handle = rod("handle", tuple(d * 0.42), tuple(d * 0.56), 0.022, "MI_Wood_Varnish")
        parts += [spoke, handle]
    obj = finish(parts, "SM_Boat_Wheel")
    export(obj)


def main():
    sb.reset_scene()
    mats()
    args = sb.script_args() or ["hull", "spars", "sail", "rudder", "wheel"]
    if "hull" in args:
        build_hull()
    if "spars" in args:
        sb.reset_scene()
        mats()
        build_spar("SM_Boat_Boom", BOOM_L, 0.07, 0.055)
        sb.reset_scene()
        mats()
        build_spar("SM_Boat_Gaff", GAFF_L, 0.06, 0.045)
    for part, fn in (("sail", build_sail), ("rudder", build_rudder), ("wheel", build_wheel)):
        if part in args:
            sb.reset_scene()
            mats()
            fn()


main()
