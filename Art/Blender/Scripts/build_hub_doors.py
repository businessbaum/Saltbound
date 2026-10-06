"""The hub ship's hidden side doors (D64; hub.doors in the data): the hull itself opens inwards like a double door, in both
sides, into the harbour's basins (D70: a station at each, x 32..40 and 104..112); boats come in with their masts up.

Each side has two leaves, 4 m wide, from the strake seam at z0 (-2.5 at the doors' middle) to the one under z1 (13.0 at the
aft jamb), 0.35 m thick and curved like the hull. Shut, the strakes, the waterline belt, the wales and the port lids run on
across them (build_hub_hull.py's own generator builds them into the leaves), so only the seams and the iron hinge straps show
(owner: "not a big obvious gate").

How they swing (worked out so nothing runs through anything; the build checks it):
  - A leaf curved like this hull (its outer face 2.5 m further out at mid-height than at its foot) cannot turn about an axis on
    the hull's skin: whatever lies inside the axis swings aft and out through the fixed planking. So each leaf turns about a
    vertical axis inboard of all of it (PIVOT_IN inside its innermost point), and 1.4 m aft of the aft jamb (forward of the
    fore jamb): the hinge arms reach from a hinge post standing in the hall to the leaf. Open, the leaves stand in the water
    along x 32 and x 40 as the data says (aft leaf x 30.6..33.4), which leaves the opening clear (6.6 m at the boats' deck
    height, 5 m at mid-height).
  - Where the leaves meet, their outer corners would swing into each other: the seam is bevelled through the thickness and the
    aft leaf (the master) opens first and shuts last; the fore leaf follows it by LAG degrees (printed by the build).

Meshes (/Game/Saltbound/Hub/Art; Nanite; UCX):
  SM_Hub_DoorStbdAft / SM_Hub_DoorStbdFore / SM_Hub_DoorPortAft / SM_Hub_DoorPortFore   (the aft station)
  SM_Hub_Door2StbdAft / ... / SM_Hub_Door2PortFore                                      (D70: the fore station)
                       one leaf each, built where it stands shut; pivot on its hinge axis at z0 (see PIVOTS, printed and in
                       Art/Generated/hub_doors.json with the rope eyes, the blocks and the swing); UCX boxes along its height
  SM_Hub_DoorFrame     what stays, at both stations: the hinge posts (UCX), the jambs, head and sill inside the openings, the
                       capstans' bases, the rope blocks under the gun deck, the fairleads on the basins' aft edges, the ropes
  SM_Hub_Capstan       a capstan's turning part (drum, whelps, bars, the rope's turns); pivot on the deck at its axis (as D62)
  SM_Hub_GateRope      one rope, 27 m hanging down from its pivot (the top); scale it to the length shown (as D62)
Run: Tools/Build-BlenderAsset.ps1 build_hub_doors [leaves] [frame] [capstan] [rope]
"""
import json
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_hub_hull as HH  # noqa: E402
import hub_layout as H  # noqa: E402
import hub_timber as T  # noqa: E402
import sb_blender as sb  # noqa: E402

HUB, Z = H.HUB, H.Z
DO = HUB["doors"]
X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
nseed = HH.nseed
PT = HH.PLANK_T
X0, XM, X1 = HH.DOOR_X                     # the station being built (use_station)
STATION = 0
OFFSET = 1.4                 # the hinge axis aft of the aft jamb (forward of the fore one)
PIVOT_IN = 0.15              # the hinge axis this far inboard of the leaf's innermost point
INNER = -0.21                # the leaf's inner face (its planks are 0..+0.14 outside the hull's surface: 0.35 thick)
BATTEN = 0.12                # the ledges and braces on its inner face
SEAM = 0.008                 # half the gap at the seams
ROPE_R = 0.11
ROPE_LEN = 27.0
CAP_BAR_R = 1.3              # capstan bars' reach from the axis
CAP_BAR_Z = 0.95             # bars at the 1.4 m player's waist
HINGE_Z = (-1.7, 2.0, 5.0, 8.0, 11.0)      # in the free bands between the wales and the port lids (the straps cross neither)
DECK_UNDER = Z["gun"] - HUB["slab"]        # the gun deck's underside: the blocks hang from it


def leaf_z(x):
    """The leaf's bottom and top at x: the seams at the doors' lines."""
    return HH.line_z(HH.LINES[HH.DOOR_J[0]], x), HH.line_z(HH.LINES[HH.DOOR_J[1]], x)


def y_surf(z, x=None):
    return HH.hw(XM if x is None else x, z)


def pivot_y():
    """The hinge axis' |y|: inboard of every point of the leaves (their inner battens at the innermost height)."""
    zb, zt = leaf_z(X0)
    inner = min(y_surf(zb + (zt - zb) * k / 60.0, x) for k in range(61) for x in (X0, X1))
    return inner + INNER - BATTEN - PIVOT_IN


PIVOT_Y = pivot_y()
PIVOT_Z = float(DO["z0"])


def use_station(k):
    """D70: build the doors of station k (hub.doors.stations, aft first)."""
    global STATION, X0, XM, X1, PIVOT_Y
    STATION = k
    X0, XM, X1 = HH.DOOR_XS[k]
    PIVOT_Y = pivot_y()


def station_name(k):
    return "" if k == 0 else str(k + 1)


def capstan_at(k, side):
    """Station k's capstan on its side (hub.doors.capstans: starboard, port for each station in turn)."""
    return DO["capstans"][2 * k + (0 if side > 0 else 1)]


def pivots():
    """(key, mesh name, pivot (ship m), opening yaw in degrees (Unreal / ship frame: + turns +X towards +Y), x range) of the
    current station."""
    out = []
    for side, sname in ((1, "Stbd"), (-1, "Port")):
        pre = "SM_Hub_Door" + station_name(STATION)
        out.append(((side, "A"), pre + "%sAft" % sname, (X0 - OFFSET, side * PIVOT_Y, PIVOT_Z), -90.0 * side, (X0, XM)))
        out.append(((side, "F"), pre + "%sFore" % sname, (X1 + OFFSET, side * PIVOT_Y, PIVOT_Z), 90.0 * side, (XM, X1)))
    return out


def seam_x(y, z):
    """The meeting seam at (|y|, z): x 36 at the planks' outer face, leaning forward inboard (and aft outboard, over the wales
    and lids) steeply enough that the aft leaf's points slide clear of the fore leaf as it opens."""
    yo = y_surf(z) + PT
    r = XM - (X0 - OFFSET)
    s = (yo + 0.45 - PIVOT_Y) / r
    return XM + (yo - abs(y)) * s


def grime_low(q):
    return min(max((0.8 - q.z) / 1.6, 0.0), 1.0)


# --- one leaf --------------------------------------------------------------------------------------------------------------
def frame_at(x, z, side):
    return HH.hull_frame(x, z, side)


def surf_pt(x, z, side, d):
    p, t, a, n = frame_at(x, z, side)
    return p + n * d


def vertical(part, x, side, d, width, thick, mat, z0=None, z1=None, kind="beam", step=0.75, **kw):
    """A timber up the leaf at x, following the hull's section, its middle at depth d."""
    zb, zt = leaf_z(x)
    z0 = zb if z0 is None else z0
    z1 = zt if z1 is None else z1
    n = max(2, int(math.ceil((z1 - z0) / step)))
    pts = [surf_pt(x, z0 + (z1 - z0) * k / n, side, d) for k in range(n + 1)]
    fr = T.polyline(pts, X, lambda q, s=side: frame_at(x, q.z, s)[3])
    if kind == "board":
        part.board(fr, T.length_of(fr), width, thick, mat, nseed(), seg=0.8, wear=0.6, bend=0.01, twist=0.4, **kw)
    else:
        part.beam(fr, T.length_of(fr), width, thick, mat, nseed(), seg=0.8, wear=0.8, **kw)


def across(part, xa, xb, z, side, d, width, thick, mat, **kw):
    """A timber across the leaf (along x) at height z, its middle at depth d."""
    pa, pb = surf_pt(xa, z, side, d), surf_pt(xb, z, side, d)
    n = frame_at(0.5 * (xa + xb), z, side)[3]
    part.beam(T.straight(pa, pb, n.cross(X).normalized(), n), (pb - pa).length, width, thick, mat, nseed(), seg=1.0, **kw)


def build_leaf_back(part, key, piv, xr):
    """The leaf's inside and its ironwork: an inner lining of upright boards, stiles and rails round its edges (they close
    its thickness), ledges and braces on its inner face, the hinge straps outside, the hinge arms to the axis, the rope's eye."""
    side, which = key
    xa, xb = xr[0] + SEAM, xr[1] - SEAM
    hinge_x, free_x = (xa, xb) if which == "A" else (xb, xa)
    toward = 1.0 if which == "A" else -1.0                       # from the hinge edge towards the free edge
    rng = random.Random(7 + (side > 0) * 2 + (which == "A"))
    mid_d = INNER + 0.05
    # Inner lining: upright boards edge to edge.
    nb = int(round((xb - xa) / 0.4))
    for k in range(nb):
        x = xa + (xb - xa) * (k + 0.5) / nb
        vertical(part, x, side, mid_d, (xb - xa) / nb - 0.02, 0.1, rng.choice(("MI_Wood_TeakDark", "MI_Hub_HullTarDark", "MI_Wood_TeakDark")),
                 kind="board")
    # Stiles and rails round the edges, closing the thickness between the lining and the planks.
    for x in (xa + 0.15, xb - 0.15):
        vertical(part, x, side, INNER * 0.5, 0.3, -INNER, "MI_Hub_HullTarDark")
    zb, zt = leaf_z(0.5 * (xa + xb))
    for z in (zb + 0.15, zt - 0.15):
        across(part, xa + 0.3, xb - 0.3, z, side, INNER * 0.5, 0.3, -INNER, "MI_Hub_HullTarDark")
    # Ledges on the inner face, braces between them rising from the hinge side.
    ledges = [zh + HH.sheer(hinge_x) * HH.sheer_k(zh) for zh in HINGE_Z]          # the hinges are bolted through them
    for z in ledges:
        across(part, xa + 0.05, xb - 0.05, z, side, INNER - BATTEN * 0.5, 0.34, BATTEN, "MI_Wood_TeakDark", wear=0.9)
    for z_lo, z_hi in zip(ledges[:-1], ledges[1:]):
        pa = surf_pt(hinge_x + toward * 0.25, z_lo + 0.25, side, INNER - BATTEN * 0.5)
        pb = surf_pt(free_x - toward * 0.25, z_hi - 0.25, side, INNER - BATTEN * 0.5)
        n = frame_at(0.5 * (xa + xb), 0.5 * (z_lo + z_hi), side)[3]
        d = (pb - pa).normalized()
        part.beam(T.straight(pa, pb, n.cross(d), n), (pb - pa).length, 0.28, BATTEN * 0.9, "MI_Wood_TeakDark", nseed(), seg=1.0, wear=0.9)
    # The hinges: straps on the outer face (what shows outside), arms inside to the axis, a ring round the hinge post.
    pv = Vector(piv)
    for zh in HINGE_Z:
        z = zh + HH.sheer(hinge_x) * HH.sheer_k(zh)
        ln = 2.1
        xs = [hinge_x + toward * (0.04 + ln * k / 6) for k in range(7)]
        pts = [surf_pt(xx, z, side, PT + 0.025) for xx in xs]
        fr = T.polyline(pts, lambda q, s=side: frame_at(q.x, q.z, s)[2], lambda q, s=side: frame_at(q.x, q.z, s)[3])
        part.beam(fr, T.length_of(fr), 0.17, 0.035, "MI_Metal_Iron", nseed(), seg=0.4, wear=0.5, grime=grime_low)
        tip = surf_pt(xs[-1] + toward * 0.12, z, side, PT + 0.02)
        part.lathe([(0.0, 0.0), (0.11, 0.0), (0.11, 0.03), (0.0, 0.03)], tip, frame_at(xs[-1], z, side)[3], "MI_Metal_Iron", sides=8)
        for xx in xs[1::2]:                                                 # bolt heads
            part.lathe([(0.0, 0.0), (0.035, 0.0), (0.03, 0.03), (0.0, 0.035)], surf_pt(xx, z, side, PT + 0.06), frame_at(xx, z, side)[3],
                       "MI_Metal_Iron", sides=6, wear=0.8)
        # Round the hinge edge (in the seam) and on inside along the stile.
        e_out, e_in = surf_pt(hinge_x + toward * 0.03, z, side, PT + 0.02), surf_pt(hinge_x + toward * 0.03, z, side, INNER - 0.02)
        part.beam(T.straight(e_out, e_in, UP, X * toward), (e_out - e_in).length, 0.17, 0.035, "MI_Metal_Iron", nseed(), seg=1.0)
        # The arm: an iron-strapped oak knee from the stile to a ring round the hinge post.
        c = Vector((pv.x, pv.y, z))
        root = surf_pt(hinge_x + toward * 0.25, z, side, INNER - 0.1)
        d = (root - c)
        d.z = 0.0
        ln_ = d.length
        d.normalize()
        a0 = c + d * 0.36
        part.beam(T.straight(a0, root, UP, d.cross(UP)), (root - a0).length, 0.36, 0.26, "MI_Hub_HullTarDark", nseed(), seg=0.8, wear=0.9)
        for zz in (z - 0.2, z + 0.2):
            q0, q1 = a0 + UP * (zz - z), root + UP * (zz - z) - d * 0.1
            part.beam(T.straight(q0, q1, d.cross(UP), UP), (q1 - q0).length, 0.14, 0.04, "MI_Metal_Iron", nseed(), seg=1.0)
        part.lathe([(0.3, -0.24), (0.36, -0.2), (0.36, 0.2), (0.3, 0.24)], c, UP, "MI_Metal_Iron", sides=16, caps=False)
        part.lathe([(0.3, -0.24), (0.3, 0.24)], c, UP, "MI_Metal_Iron", sides=16, caps=False)
    # The rope's eye at the head, near the free edge on the inner face.
    eye = eye_point(side, which)
    n = frame_at(free_x, eye.z, side)[3]
    plate = eye + n * 0.1
    part.beam(T.straight(plate - UP * 0.28, plate + UP * 0.28, X, n), 0.56, 0.3, 0.04, "MI_Metal_Iron", nseed(), seg=1.0)
    part.beam(T.arc(eye - n * 0.05, UP, -n, 0.13, -math.pi * 0.5, math.pi * 0.5), math.pi * 0.13, 0.05, 0.05, "MI_Metal_Iron", nseed(), seg=0.08)
    return eye


def eye_point(side, which):
    """Where the rope takes hold: the eye at the leaf's head near its free edge, on the inner face (ship m, shut)."""
    free_x = (XM - 0.6) if which == "A" else (XM + 0.6)
    z = leaf_z(free_x)[1] - 0.7
    return surf_pt(free_x, z, side, INNER - BATTEN - 0.18)


def bevel(part, key):
    """Cut the meeting seam along seam_x: the aft leaf's points beyond it come back to it, the fore leaf's points before it
    go forward to it (only the end rings of the boards there move)."""
    side, which = key
    for v in part.bm.verts:
        x, y, z = v.co
        if abs(x - XM) > 0.7:
            continue
        s = seam_x(y, z)
        if which == "A" and x > s - SEAM:
            v.co.x = s - SEAM
        elif which == "F" and x < s + SEAM:
            v.co.x = s + SEAM


def leaf_ucx(part, xr, side):
    """Collision: boxes up the leaf (it is flat along x, curved up its height), from its inner battens to its outer face."""
    zb, zt = leaf_z(XM)
    n = 6
    for k in range(n):
        z0, z1 = zb + (zt - zb) * k / n, zb + (zt - zb) * (k + 1) / n
        pts = []
        for x in (xr[0] + SEAM, xr[1] - SEAM):
            for z in (z0, z1):
                pts += [surf_pt(x, z, side, PT + 0.05), surf_pt(x, z, side, INNER - BATTEN)]
        part.collide_points(pts)


# --- the swing check -----------------------------------------------------------------------------------------------------
def rot(p, c, deg):
    a = math.radians(deg)
    dx, dy = p[0] - c[0], p[1] - c[1]
    return (c[0] + dx * math.cos(a) - dy * math.sin(a), c[1] + dx * math.sin(a) + dy * math.cos(a), p[2])


def in_fore_leaf(p):
    """Is a point (starboard, ship m) inside the shut fore leaf (between its seam and the fore jamb, through its thickness)?"""
    x, y, z = p
    zb, zt = leaf_z(XM)
    if not (zb < z < zt) or x > X1 - 0.05:
        return False
    ys = y_surf(z)
    if not (ys + INNER - BATTEN < y < ys + PT + 0.4):
        return False
    return x > seam_x(y, z) + 0.002


def in_fixed_hull(p):
    """Is a point inside the fixed hull aft of the aft jamb (its planks and lining, the door's height)?"""
    x, y, z = p
    if x > X0 - 0.01 or x < X0 - 6.0:
        return False
    ys = HH.hw(x, z)
    return ys - 0.35 < y < ys + PT + 0.3


def swing_check(verts_a, verts_f, piv_a, piv_f):
    """The aft leaf's points (starboard, ship m) swung by up to 90 degrees against the fore leaf swung `lag` degrees behind
    it, and against the fixed hull. Returns (smallest lag with no contact, contacts with the hull)."""
    sample = verts_a[::7]
    hull_hits = 0
    for th in range(1, 91, 3):
        for p in sample:
            if in_fixed_hull(rot(p, piv_a, -th)):
                hull_hits += 1
    for lag in range(0, 91, 2):
        ok = True
        for th in range(1, 91 + lag, 2):
            ta, tf = min(th, 90), min(max(th - lag, 0), 90)
            for p in sample:
                q = rot(p, piv_a, -ta)
                q = rot(q, piv_f, -tf)                               # into the fore leaf's own (shut) frame: undo its +tf
                if in_fore_leaf(q):
                    ok = False
                    break
            if not ok:
                break
        if ok:
            return lag, hull_hits
    return None, hull_hits


# --- builds ------------------------------------------------------------------------------------------------------------------
def build_leaves():
    rows, stations = [], []
    for k in range(len(HH.DOOR_XS)):
        use_station(k)
        sb.reset_scene()
        stations.append(build_station_leaves(rows))
    use_station(0)
    out = os.path.join(H.ROOT, "Art", "Generated", "hub_doors.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump({"_comment": "D64, D70 the side doors (build_hub_doors.py), ship metres, a pair in each side at each station (aft "
                   "first). Each leaf mesh's pivot is its hinge axis at z0; open = yaw open_yaw (Unreal, + turns +X to +Y). The aft "
                   "leaf opens first and shuts last; the fore leaf follows its station's lag_deg behind it. eye: where the leaf's "
                   "rope takes hold; block: the fixed block it runs to (then down to the capstan, in SM_Hub_DoorFrame).",
                   "stations": stations, "lag_deg": max(s["lag_deg"] or 0 for s in stations),
                   "z_bottom_mid": stations[0]["z_bottom_mid"], "z_top_mid": stations[0]["z_top_mid"],
                   "capstans": [[c[0], c[1], Z["quay"]] for c in DO["capstans"]], "leaves": rows}, fh, indent=1)
    print("SB_DOORS_JSON", out)


def build_station_leaves(rows):
    """The current station's four leaves: built, bevelled, checked, exported; their rows appended. Returns the station's row."""
    piv = pivots()
    leaves = {key: T.Part(name) for key, name, _, _, _ in piv}
    route = HH.Router(None, doors=leaves, x_range=(X0 - 1.0, X1 + 1.0))
    fits = HH.fittings()
    HH.skin(route, fits)
    eyes = {}
    for key, name, pv, yaw, xr in piv:
        eyes[key] = build_leaf_back(leaves[key], key, pv, xr)
        bevel(leaves[key], key)
        leaf_ucx(leaves[key], xr, key[0])
    # Check the swing on the starboard pair (the port pair is its mirror).
    va = [tuple(v.co) for v in leaves[(1, "A")].bm.verts]
    vf = [tuple(v.co) for v in leaves[(1, "F")].bm.verts]
    pa = next(p for k, _, p, _, _ in piv if k == (1, "A"))
    pf = next(p for k, _, p, _, _ in piv if k == (1, "F"))
    lag, hull_hits = swing_check(va, vf, pa, pf)
    print("SB_DOOR_SWING station", STATION, "lag_deg", lag, "hull_contacts", hull_hits)
    for key, name, pv, yaw, xr in piv:
        e = eyes[key]
        rows.append({"mesh": name, "station": STATION, "side": "starboard" if key[0] > 0 else "port", "leaf": "aft" if key[1] == "A" else "fore",
                     "pivot": [round(c, 3) for c in pv], "open_yaw": yaw, "x": list(xr),
                     "eye_shut": [round(e.x, 3), round(e.y, 3), round(e.z, 3)],
                     "eye_open": [round(c, 3) for c in rot((e.x, e.y, e.z), pv, yaw)],
                     "block": [round(c, 3) for c in block_point(key)],
                     "capstan": [capstan_at(STATION, key[0])[0], capstan_at(STATION, key[0])[1], Z["quay"]]})
        HH.export(leaves[key], pivot=pv, ucx=True)
    zb, zt = leaf_z(XM)
    return {"x0": X0, "x1": X1, "lag_deg": lag, "z_bottom_mid": round(zb, 3), "z_top_mid": round(zt, 3),
            "leaf_z_at_x": {str(x): [round(v, 3) for v in leaf_z(x)] for x in (X0, XM, X1)}}


def block_point(key):
    """The block each leaf's rope runs to: inboard of its open eye, hung under the gun deck (aft leaves: over the aft quay's
    edge, where the rope goes down to the capstan; fore leaves: forward of the fore hinge post, then aft under the deck)."""
    side, which = key
    x = X0 - OFFSET - 0.15 if which == "A" else X1 + OFFSET + 0.2
    return Vector((x, side * 15.0, DECK_UNDER - 0.27))


def build_frame():
    """What stays: hinge posts, the opening's frame inside the hull, the capstans' bases, blocks, fairleads and the ropes (at
    every station, one mesh in the ship's frame)."""
    p = T.Part("SM_Hub_DoorFrame")
    for k in range(len(HH.DOOR_XS)):
        use_station(k)
        build_station_frame(p)
    use_station(0)
    HH.export(p, pivot=(H.PIVOT_X, 0.0, 0.0), ucx=True)


def build_station_frame(p):
    floor = HUB["dock"]["floor"]
    for side in (1, -1):
        # Hinge posts: round oak with iron collars over and under each hinge ring, a shoe on the floor, a cleat under the deck.
        for xp in (X0 - OFFSET, X1 + OFFSET):
            c = Vector((xp, side * PIVOT_Y, 0.0))
            p.round(T.straight(c + UP * floor, c + UP * (DECK_UNDER - 0.05), X, Y), DECK_UNDER - 0.05 - floor, 0.29, "MI_Hub_HullTarDark",
                    nseed(), sides=12, seg=1.0, wear=0.8, grime=grime_low)
            for zh in HINGE_Z:
                z = zh + HH.sheer(X0) * HH.sheer_k(zh)
                for zz in (z - 0.3, z + 0.3):
                    p.lathe([(0.29, -0.05), (0.34, -0.04), (0.34, 0.04), (0.29, 0.05)], c + UP * zz, UP, "MI_Metal_Iron", sides=14, caps=False)
            p.lathe([(0.0, 0.0), (0.42, 0.0), (0.42, 0.3), (0.31, 0.42), (0.0, 0.42)], c + UP * floor, UP, "MI_Metal_Iron", sides=12)
            p.block(xp - 0.45, xp + 0.45, c.y - 0.45, c.y + 0.45, DECK_UNDER - 0.35, DECK_UNDER, "MI_Hub_HullTarDark", nseed(), axis="x")
            p.collide_box(xp - 0.32, xp + 0.32, c.y - 0.32, c.y + 0.32, floor, DECK_UNDER)
        # The opening's frame inside the hull: jambs aft and forward, the head over it, the sill under it (behind the seams).
        for xj, sgn in ((X0, -1.0), (X1, 1.0)):
            x = xj + sgn * 0.22
            vertical(p, x, side, -0.19, 0.4, 0.3, "MI_Hub_HullTarDark", z0=leaf_z(xj)[0] - 0.4, z1=leaf_z(xj)[1] + 0.4)
        for edge, sgn in ((1, 1.0), (0, -1.0)):
            pts, ns = [], []
            for k in range(9):
                x = X0 - 0.45 + (X1 - X0 + 0.9) * k / 8
                z = leaf_z(x)[edge] + sgn * 0.22
                pts.append(surf_pt(x, z, side, -0.19))
            fr = T.polyline(pts, lambda q, s=side: frame_at(q.x, q.z, s)[2], lambda q, s=side: frame_at(q.x, q.z, s)[3])
            p.beam(fr, T.length_of(fr), 0.4, 0.3, "MI_Hub_HullTarDark", nseed(), seg=1.0, wear=0.8)
        # The capstan's base on the aft quay: a cross of timbers, an iron pawl ring.
        cx, cy = capstan_at(STATION, side)
        c = Vector((cx, cy, Z["quay"]))
        for k in range(4):
            a = math.pi * k / 4
            d = Vector((math.cos(a), math.sin(a), 0.0))
            p.beam(T.straight(c - d * 1.0 + UP * 0.06, c + d * 1.0 + UP * 0.06, d.cross(UP), UP), 2.0, 0.3, 0.12, "MI_Wood_TeakDark", nseed(), seg=0.5)
        p.lathe([(0.66, 0.0), (0.78, 0.0), (0.78, 0.12), (0.66, 0.12)], c + UP * 0.1, UP, "MI_Metal_Iron", sides=16, caps=False)
        # Blocks under the gun deck for both leaves' ropes; a fairlead post at the aft quay's edge; the ropes between them.
        ba, bf = block_point((side, "A")), block_point((side, "F"))
        for b in (ba, bf):
            block(p, b, side)
        # The fairlead: two sheaves between oak cheeks on the aft quay's edge; the ropes come down over the water (clear of the
        # galleries over the quay), wrap under them and run aft over the quay to the capstan's drum.
        qx = HUB["dock"]["basins"][STATION]["x0"]
        sr = 0.26
        sx = ba.x + 0.0 - sr                                             # the ropes come down at the sheaves' forward tangent
        for k, dy in enumerate((-0.17, 0.17)):
            sc = Vector((sx, side * 15.0 + dy, Z["quay"] + 0.5))
            p.lathe([(0.0, -0.07), (sr, -0.07), (sr + 0.03, -0.035), (sr - 0.04, 0.0), (sr + 0.03, 0.035), (sr, 0.07), (0.0, 0.07)], sc, Y,
                    "MI_Wood_Varnish", sides=14)
        for dy in (-0.33, 0.0, 0.33):
            p.block(qx - 0.75, sx + 0.38, side * 15.0 + dy - 0.06, side * 15.0 + dy + 0.06, Z["quay"], Z["quay"] + 0.82, "MI_Hub_HullTarDark",
                    nseed(), axis="x")
        p.round(T.straight(Vector((sx, side * 15.0 - 0.42, Z["quay"] + 0.5)), Vector((sx, side * 15.0 + 0.42, Z["quay"] + 0.5)), X, UP), 0.84, 0.05,
                "MI_Metal_Iron", nseed(), sides=8)
        p.block(qx - 0.8, qx - 0.1, side * 15.0 - 0.5, side * 15.0 + 0.5, Z["quay"], Z["quay"] + 0.14, "MI_Wood_TeakDark", nseed(), axis="y")
        rope_run(p, [bf + Vector((0.0, 0.0, 0.17)), ba + Vector((0.35, 0.0, 0.17))], 0.08)      # the fore leaf's rope, aft under the deck
        drum = c + UP * 0.53
        for k, dy in enumerate((-0.17, 0.17)):
            yy = side * 15.0 + dy
            top_ = Vector((ba.x, yy, ba.z - 0.05))
            down = Vector((sx + sr, yy, Z["quay"] + 0.5))
            rope_run(p, [top_, down], 0.08)
            arc = [Vector((sx + sr * math.cos(-0.5 * math.pi * j / 4), yy, Z["quay"] + 0.5 + sr * math.sin(-0.5 * math.pi * j / 4))) for j in range(5)]
            arc += [Vector((sx - sr * math.sin(0.5 * math.pi * j / 4), yy, Z["quay"] + 0.5 - sr * math.cos(0.5 * math.pi * j / 4))) for j in range(1, 3)]
            rope_run(p, arc, 0.08)
            to = drum + (Vector((sx, yy, drum.z)) - drum).normalized() * 0.5 + UP * (0.06 + 0.1 * k)
            rope_run(p, [arc[-1], to], 0.08)


def block(p, b, side):
    """A rope block hung under the deck from an iron strop and eyebolt: a varnished shell with an iron-bound sheave."""
    top = Vector((b.x, b.y, DECK_UNDER))
    p.lathe([(0.0, 0.0), (0.12, 0.0), (0.12, 0.04), (0.0, 0.04)], top - UP * 0.04, UP, "MI_Metal_Iron", sides=8)
    p.beam(T.arc(top - UP * 0.12, X, UP, 0.08, math.pi, 2 * math.pi), math.pi * 0.08, 0.04, 0.04, "MI_Metal_Iron", nseed(), seg=0.05)
    p.block(b.x - 0.32, b.x + 0.32, b.y - 0.16, b.y + 0.16, b.z - 0.45, b.z + 0.1, "MI_Wood_Varnish", nseed(), axis="z")
    p.beam(T.straight(Vector((b.x, b.y, b.z + 0.1)), top - UP * 0.15, X, Y), (top.z - 0.15 - b.z - 0.1), 0.08, 0.06, "MI_Metal_Iron", nseed())
    p.lathe([(0.0, -0.17), (0.26, -0.17), (0.26, 0.17), (0.0, 0.17)], b - UP * 0.17, Y, "MI_Metal_Iron", sides=14)


def rope_run(p, pts, r):
    pts = [Vector(q) for q in pts]
    d = (pts[-1] - pts[0]).normalized()
    acr = d.cross(UP) if abs(d.z) < 0.95 else d.cross(X)
    p.round(T.polyline(pts, acr.normalized(), acr.normalized().cross(d)), T.length_of(T.polyline(pts, acr.normalized())), r,
            "MI_Rope_Hemp", nseed(), sides=8, seg=1.0, wear=0.0)


def build_capstan():
    """The turning part, built at the origin (pivot on the deck at its axis)."""
    p = T.Part("SM_Hub_Capstan")
    o = Vector((0.0, 0.0, 0.0))
    # Drum: a waisted barrel with whelps (upright ribs the rope rides on), a head with bar holes, a cap.
    p.lathe([(0.0, 0.12), (0.62, 0.12), (0.6, 0.22), (0.46, 0.4), (0.42, 0.62), (0.46, 0.78), (0.58, 0.82), (0.62, 0.86),
             (0.62, 1.12), (0.55, 1.18), (0.3, 1.24), (0.0, 1.26)], o, UP, "MI_Wood_Varnish", sides=16, wear=1.0)
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        d = Vector((math.cos(a), math.sin(a), 0.0))
        pts = [o + d * 0.5 + UP * 0.24, o + d * 0.47 + UP * 0.5, o + d * 0.5 + UP * 0.78]
        f = T.polyline(pts, lambda q, d=d: d.cross(UP), lambda q, d=d: d)
        p.beam(f, T.length_of(f), 0.1, 0.1, "MI_Wood_TeakDark", nseed(), seg=0.3)
    for k in range(6):                                                      # the bars, worn bright where hands go
        a = 2 * math.pi * k / 6
        d = Vector((math.cos(a), math.sin(a), 0.0))
        p.beam(T.straight(o + d * 0.4 + UP * CAP_BAR_Z, o + d * CAP_BAR_R + UP * CAP_BAR_Z, UP, d.cross(UP)), CAP_BAR_R - 0.4, 0.11, 0.11,
               "MI_Wood_TeakLight", nseed(), seg=0.5, wear=1.0)
    for k in range(3):                                                      # the rope's turns on the drum
        z = 0.42 + 0.11 * k
        p.beam(T.arc(o + UP * z, X, Y, 0.47, 0.0, 2 * math.pi), 2 * math.pi * 0.47, 0.1, 0.1, "MI_Rope_Hemp", nseed(), seg=0.2, wear=0.0)
    p.lathe([(0.0, 0.0), (0.1, 0.0), (0.1, 0.08), (0.0, 0.08)], o + UP * 1.26, UP, "MI_Metal_Brass", sides=10)
    p.collide_box(-0.62, 0.62, -0.62, 0.62, 0.0, 1.26)
    p.collide_points([Vector((math.cos(2 * math.pi * k / 8) * CAP_BAR_R, math.sin(2 * math.pi * k / 8) * CAP_BAR_R, z))
                      for k in range(8) for z in (CAP_BAR_Z - 0.1, CAP_BAR_Z + 0.1)])
    HH.export(p, pivot=(0.0, 0.0, 0.0), ucx=True)


def build_rope():
    p = T.Part("SM_Hub_GateRope")
    p.round(T.straight((0.0, 0.0, 0.0), (0.0, 0.0, -ROPE_LEN), X, Y), ROPE_LEN, ROPE_R, "MI_Rope_Hemp", nseed(), sides=8, seg=1.0, wear=0.0)
    HH.export(p, pivot=(0.0, 0.0, 0.0))


BUILDERS = {"leaves": build_leaves, "frame": build_frame, "capstan": build_capstan, "rope": build_rope}

if __name__ == "__main__":
    random.seed(64)
    wanted = sb.script_args() or list(BUILDERS)
    for k_ in range(len(HH.DOOR_XS)):
        use_station(k_)
        print("SB_DOOR_PIVOTS", [(name, [round(c, 3) for c in pv], yaw) for _, name, pv, yaw, _ in pivots()])
    use_station(0)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBDOORS_OK", wanted)
