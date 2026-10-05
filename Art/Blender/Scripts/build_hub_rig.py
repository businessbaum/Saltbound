"""The hub ship's rig (D62; outside.masts and hub.rig in HubShip_Data.json): a galleon's three masts, every sail furled.

Each mast in three pieces as a real one is stepped: the lower mast with rope wooldings up to its round fighting top, the
topmast fidded forward of the lower masthead (a cap joins them) up to the crosstrees, the topgallant mast to the truck.
The square yards hang from slings with lifts to the mastheads and footropes under them, their sails furled on top in rolls
bound with gaskets; the mizzen's lowest yard is a galleon's lateen yard slung fore and aft; the spritsail yard hangs under
the bowsprit. Standing rigging is tarred dark: the shrouds from the channels' deadeyes (pairs of deadeyes with lanyards)
up to the mastheads with ratlines across, futtock shrouds under the tops, topmast and topgallant shrouds, backstays, the
stays forward. The pennant at the main truck is its own mesh on the flag material (it streams aft: she lies to her anchors,
head to wind).

Meshes (/Game/Saltbound/Hub/Art; Nanite):
  SM_Hub_MastMizzen / SM_Hub_MastMain / SM_Hub_MastFore   each mast with its tops, yards, sails, shrouds, ratlines, stays
  SM_Hub_Spritsail     the spritsail yard and its furled sail under the bowsprit
  SM_Hub_Pennant       the main truck's pennant: +X along the cloth from the pivot at the top of its hoist
Run: Tools/Build-BlenderAsset.ps1 build_hub_rig
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
RIG = HUB["rig"]
X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
nseed = HH.nseed
ZM = Z["main"]
TAR_ROPE = "MI_Hub_RopeTar"          # standing rigging
HEMP = "MI_Rope_Hemp"                # running rigging, gaskets, lanyards
SPAR = "MI_Wood_Varnish"
HEADS = "MI_Wood_Tarred"
CLOTH = "MI_Hub_SailFurled"
HH.COLOURS.update({TAR_ROPE: (0.12, 0.1, 0.08), CLOTH: (0.78, 0.74, 0.64), "MI_Hub_Pennant": (0.45, 0.1, 0.07),
                   "MI_Hub_PennantCream": (0.85, 0.78, 0.6)})


def mast_geom(m):
    """Heights and radii of one mast: (x, h, top z, cross z, scale)."""
    h = m["h"]
    k = h / 76.0
    return dict(x=m["x"], h=h, k=k, ztop=ZM + RIG["top"]["at"] * h, zcross=ZM + RIG["cross"]["at"] * h, ztruck=ZM + h,
                r_deck=RIG["thick"][0] * 0.5 * (0.85 + 0.15 * k), r_truck=RIG["thick"][1] * 0.5,
                top_r=RIG["top"]["w"] * 0.5 * k, cross_w=RIG["cross"]["w"] * k, scale=RIG["yards"]["scale"][m["id"]])


def rope(p, a, b, r, mat=TAR_ROPE, sag=0.0, sides=6):
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    if L < 0.05:
        return
    d = (b - a) / L
    acr = d.cross(UP) if abs(d.z) < 0.95 else d.cross(X)
    acr.normalize()
    if sag > 0.0:
        pts = [a + (b - a) * (k / 6) - UP * (sag * math.sin(math.pi * k / 6)) for k in range(7)]
        f = T.polyline(pts, acr, acr.cross(d))
        p.round(f, T.length_of(f), r, mat, nseed(), sides=sides, seg=L / 6 + 0.01, wear=0.0)
    else:
        p.round(T.straight(a, b, acr, acr.cross(d)), L, r, mat, nseed(), sides=sides, seg=L + 1.0, wear=0.0)


def spar(p, a, b, r0, r1, mat=SPAR, sides=12, rmid=None):
    """A hewn, tapered spar from a (r0) to b (r1); with rmid it swells to rmid at the middle (a yard)."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    d = (b - a) / L
    acr = d.cross(UP) if abs(d.z) < 0.95 else d.cross(X)
    acr.normalize()
    if rmid is None:
        rad = (lambda s: r0 + (r1 - r0) * s)
    else:
        rad = (lambda s: rmid - (rmid - r0) * (2.0 * abs(s - 0.5)) ** 1.6)
    p.round(T.straight(a, b, acr, acr.cross(d)), L, rad, mat, nseed(), sides=sides, seg=1.2, wear=0.6, drift=0.02)


def band(p, c, axis, r, h=0.18, mat="MI_Metal_Iron"):
    p.lathe([(r, -h * 0.5), (r + 0.04, 0.0), (r, h * 0.5)], c, axis, mat, sides=14, caps=False)


def furled(p, a, b, r, dirv_up):
    """A sail furled on a yard: a canvas roll from a to b, fat in the middle, bulging between the gaskets, bound by them."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    n_g = max(3, int(L / 2.0))

    def rad(s):
        body = r * (0.55 + 0.45 * math.sin(math.pi * min(max(s, 0.0), 1.0)) ** 0.6)
        bulge = 1.0 + 0.12 * abs(math.sin(math.pi * s * n_g))
        return body * bulge
    d = (b - a) / L
    up = Vector(dirv_up).normalized()
    acr = d.cross(up).normalized()
    p.round(T.straight(a, b, acr, up), L, rad, CLOTH, nseed(), sides=10, seg=0.5, wear=0.0, drift=0.05, wobble=0.0)
    for k in range(1, n_g):
        s = k / n_g
        c = a + (b - a) * s
        p.beam(T.arc(c, acr, up, rad(s) + 0.02, 0.0, 2 * math.pi), 2 * math.pi * rad(s), 0.07, 0.05, HEMP, nseed(), seg=0.25, wear=0.0)


def square_yard(p, g, frac, half_frac):
    """A square yard at frac of the mast with its furled sail, sling, lifts and footropes."""
    x = g["x"]
    hl = half_frac * HUB["B"] * g["scale"]
    z = ZM + frac * g["h"]
    r_mast = mast_r(g, z)
    xc = x + r_mast + 0.45
    rmid = 0.12 + 0.022 * hl
    spar(p, (xc, -hl, z), (xc, hl, z), rmid * 0.5, rmid * 0.5, rmid=rmid)
    for s in (-1.0, 1.0):
        p.lathe([(0.0, 0.0), (rmid * 0.6, 0.0), (rmid * 0.6, 0.35), (rmid * 0.3, 0.45), (0.0, 0.5)], Vector((xc, s * hl, z)), Y * s, SPAR, sides=8)
    furled(p, (xc + 0.1, -hl * 0.93, z + rmid + 0.25 * (hl / 16.8) + 0.05), (xc + 0.1, hl * 0.93, z + rmid + 0.25 * (hl / 16.8) + 0.05),
           0.32 + 0.028 * hl, UP)
    # Sling and parrel, lifts from the arms to the masthead above, footropes sagging under the yard.
    head = next_head(g, z)
    rope(p, (xc, 0.0, z + rmid), (x + mast_r(g, head), 0.0, head), 0.08, HEMP)
    p.beam(T.arc(Vector((x, 0.0, z)), X, Y, r_mast + 0.08, 0.0, 2 * math.pi), 2 * math.pi * (r_mast + 0.08), 0.12, 0.1, HEMP, nseed(), seg=0.3)
    for s in (-1.0, 1.0):
        rope(p, (xc, s * hl * 0.97, z + 0.1), (x + 0.3, s * (mast_r(g, head) + 0.1), head - 0.3), 0.05, HEMP)
        rope(p, (xc - 0.1, s * 0.6, z - rmid - 0.05), (xc - 0.1, s * hl * 0.95, z - rmid - 0.05), 0.04, HEMP, sag=1.1)


def mast_r(g, z):
    """The mast's radius at height z (the piece standing there)."""
    t = (z - ZM) / g["h"]
    return g["r_deck"] + (g["r_truck"] - g["r_deck"]) * min(max(t, 0.0), 1.0) ** 0.9


def next_head(g, z):
    """The masthead above height z (where lifts and slings go): the top's, the crosstrees' or the truck's."""
    for zz in (g["ztop"] + 1.2, g["zcross"] + 0.8, g["ztruck"] - 0.6):
        if zz > z + 1.0:
            return zz
    return g["ztruck"]


def round_top(p, g):
    """The fighting top: a round platform of boards on trestle and cross trees, a rim, a low rail, the lubber's hole."""
    x, zt, R = g["x"], g["ztop"], g["top_r"]
    rm = mast_r(g, zt)
    for s in (-1.0, 1.0):                                                   # trestle trees fore and aft, cross trees across
        p.beam(T.straight((x - R * 0.8, s * (rm + 0.25), zt - 0.45), (x + R * 0.8, s * (rm + 0.25), zt - 0.45), Y, UP), R * 1.6, 0.4, 0.5, HEADS, nseed())
    for dx in (-R * 0.5, -0.2 - rm, rm + 0.2, R * 0.5):
        w = math.sqrt(max(R * R - dx * dx, 0.0)) * 0.95
        p.beam(T.straight((x + dx, -w, zt - 0.3), (x + dx, w, zt - 0.3), X, UP), 2 * w, 0.35, 0.3, HEADS, nseed())
    y = -R + 0.2
    while y < R - 0.2:
        w = math.sqrt(max(R * R - y * y, 0.0)) - 0.15
        if abs(y) < rm + 0.7:                                               # the lubber's hole round the mast
            for xa, xb in ((x - w, x - rm - 0.7), (x + rm + 0.7, x + w)):
                if xb - xa > 0.2:
                    p.board(T.straight((xa, y, zt - 0.08), (xb, y, zt - 0.08), Y, UP), xb - xa, 0.38, 0.12, "MI_Wood_TeakDark", nseed(), seg=0.8)
        elif w > 0.3:
            p.board(T.straight((x - w, y, zt - 0.08), (x + w, y, zt - 0.08), Y, UP), 2 * w, 0.38, 0.12,
                    random.choice(("MI_Wood_TeakDark", "MI_Wood_Teak")), nseed(), seg=0.8)
        y += 0.4
    p.beam(T.arc(Vector((x, 0.0, zt - 0.1)), X, Y, R, 0.0, 2 * math.pi, out=None), 2 * math.pi * R, 0.3, 0.35, "MI_Hub_Ochre", nseed(), seg=0.6, wear=1.0)
    n = max(10, int(2 * math.pi * R / 1.4))
    for k in range(n):                                                      # a low rail round the rim, open forward
        a = 2 * math.pi * (k + 0.5) / n
        if math.cos(a) > 0.75:
            continue
        c = Vector((x + math.cos(a) * (R - 0.15), math.sin(a) * (R - 0.15), zt))
        p.beam(T.straight(c, c + UP * 0.95, X, Y), 0.95, 0.16, 0.16, HEADS, nseed(), seg=1.0)
    a0 = math.acos(0.75)
    p.beam(T.arc(Vector((x, 0.0, zt + 0.95)), X, Y, R - 0.15, a0, 2 * math.pi - a0), (2 * math.pi - 2 * a0) * R, 0.2, 0.14, SPAR, nseed(), seg=0.5)


def crosstrees(p, g):
    x, zc, w = g["x"], g["zcross"], g["cross_w"]
    rm = mast_r(g, zc)
    for s in (-1.0, 1.0):
        p.beam(T.straight((x - 1.6, s * (rm + 0.15), zc - 0.3), (x + 2.0, s * (rm + 0.15), zc - 0.3), Y, UP), 3.6, 0.22, 0.3, HEADS, nseed())
    for dx in (-0.8, 1.2):
        p.beam(T.straight((x + dx, -w * 0.5, zc - 0.15), (x + dx, w * 0.5, zc - 0.15), X, UP), w, 0.24, 0.26, HEADS, nseed())


def deadeye(p, c, n, up_dir):
    p.lathe([(0.0, -0.1), (0.3, -0.1), (0.36, -0.04), (0.36, 0.04), (0.3, 0.1), (0.0, 0.1)], c, n, "MI_Wood_Tarred", sides=12)


def shroud_points(g, side):
    """The lower shrouds' feet (the upper deadeyes over the channel's) and heads (round the masthead)."""
    mid = next(m for m in HUB["masts"] if m["x"] == g["x"])["id"]
    xa, xb = HH.channel_span(mid)
    zc = HH.channel_z(mid)
    n_de = HH.CHANNELS[mid][1]
    out = []
    for k in range(n_de):
        x = xa + 0.8 + (xb - xa - 1.6) * k / max(n_de - 1, 1)
        F = HH.hull_frame(x, zc, side)
        lower = HH.lpt(F, 0.0, 0.32, HH.PLANK_T + 1.75) + F[2] * 0.2
        head = Vector((g["x"] + (k - (n_de - 1) * 0.5) * 0.25, side * (mast_r(g, g["ztop"]) + 0.2), g["ztop"] - 0.6))
        d = (head - lower).normalized()
        upper = lower + d * 1.3
        out.append((lower, upper, head, F[3]))
    return out


def ratlines(p, feet, heads, z0, z1, step=0.5, r=0.035):
    """Horizontal ratlines between neighbouring shrouds every `step` metres up."""
    def at(a, b, z):
        t = (z - a.z) / (b.z - a.z)
        return a + (b - a) * t
    z = z0
    while z < z1:
        pts = [at(f, h, z) for f, h in zip(feet, heads)]
        for a, b in zip(pts[:-1], pts[1:]):
            rope(p, a, b, r, TAR_ROPE, sides=4)
        z += step


def build_mast(m):
    name = "SM_Hub_Mast" + m["id"].capitalize()
    p = T.Part(name)
    g = mast_geom(m)
    x = g["x"]
    # Lower mast: from under the deck to above the top, with rope wooldings and iron hoops at the head.
    zl0, zl1 = ZM - 0.6, g["ztop"] + 2.2
    spar(p, (x, 0.0, zl0), (x, 0.0, zl1), g["r_deck"], mast_r(g, zl1), sides=14)
    z = ZM + 2.5
    while z < g["ztop"] - 2.0:
        p.lathe([(mast_r(g, z) + 0.02, -0.25), (mast_r(g, z) + 0.06, -0.2), (mast_r(g, z) + 0.06, 0.2), (mast_r(g, z) + 0.02, 0.25)],
                Vector((x, 0.0, z)), UP, HEMP, sides=14, caps=False)
        z += 2.6
    for zz in (g["ztop"] + 0.5, g["ztop"] + 1.6):
        band(p, Vector((x, 0.0, zz)), UP, mast_r(g, zz) + 0.02)
    round_top(p, g)
    # The cap joining the lower masthead and the topmast fidded forward of it; the topmast up to the crosstrees.
    rt = mast_r(g, g["ztop"]) * 0.7
    xt = x + mast_r(g, zl1) + rt + 0.1
    p.block(x - mast_r(g, zl1) - 0.2, xt + rt + 0.25, -mast_r(g, zl1) - 0.15, mast_r(g, zl1) + 0.15, zl1, zl1 + 0.5, HEADS, nseed(), axis="x")
    spar(p, (xt, 0.0, g["ztop"] - 1.8), (xt, 0.0, g["zcross"] + 1.6), rt, rt * 0.8, sides=12)
    crosstrees(p, g)
    rg = rt * 0.62
    xg = xt + rt * 0.8 + rg + 0.05
    p.block(xt - rt - 0.15, xg + rg + 0.2, -rt - 0.1, rt + 0.1, g["zcross"] + 1.6, g["zcross"] + 1.95, HEADS, nseed(), axis="x")
    spar(p, (xg, 0.0, g["zcross"] - 1.4), (xg, 0.0, g["ztruck"]), rg, g["r_truck"] * 0.8, sides=10)
    if m["id"] == "main":                                                   # where the pennant flies (the map script reads it)
        out = os.path.join(H.ROOT, "Art", "Generated", "hub_rig.json")
        with open(out, "w", encoding="utf-8") as fh:
            json.dump({"pennant": [xg, 0.0, g["ztruck"] + 0.3]}, fh)
    p.lathe([(0.0, 0.0), (g["r_truck"] + 0.25, 0.0), (g["r_truck"] + 0.3, 0.12), (g["r_truck"] + 0.2, 0.3), (0.0, 0.34)],
            Vector((xg, 0.0, g["ztruck"])), UP, SPAR, sides=12)                                     # the truck
    # Yards: square ones; on the mizzen the lowest is the lateen yard.
    for k, (frac, half) in enumerate(zip(RIG["yards"]["at"], RIG["yards"]["half"])):
        if k == 0 and m["id"] == RIG["lateen"]["mast"]:
            lateen(p, g)
            continue
        square_yard(p, g, frac, half)
    # Standing rigging: lower shrouds with deadeyes, lanyards and ratlines; futtock, topmast and topgallant shrouds;
    # backstays; the stays forward.
    for side in (1.0, -1.0):
        sp = shroud_points(g, side)
        for lower, upper, head, n in sp:
            deadeye(p, upper, n, (head - upper).normalized())
            for dy in (-0.15, 0.0, 0.15):
                off = n.cross((upper - lower).normalized()).normalized() * dy
                rope(p, lower + off + (upper - lower).normalized() * 0.2, upper + off - (upper - lower).normalized() * 0.2, 0.025, HEMP, sides=4)
            rope(p, upper, head, 0.08, TAR_ROPE)
        feet = [u + (h - u).normalized() * 0.5 for _, u, h, _ in sp]
        heads = [h for _, _, h, _ in sp]
        ratlines(p, feet, heads, max(f.z for f in feet) + 0.4, g["ztop"] - 3.2)
        # Futtock shrouds from the top's rim down to the mast; topmast shrouds from the rim up to the topmast head.
        n_t = 4
        rim = [Vector((x - g["top_r"] * 0.5 + g["top_r"] * 0.8 * k / (n_t - 1), side * g["top_r"] * 0.85, g["ztop"] - 0.2)) for k in range(n_t)]
        thead = [Vector((xt, side * (rt + 0.15), g["zcross"] - 0.5)) for _ in range(n_t)]
        for r_, th in zip(rim, thead):
            rope(p, r_, Vector((x, side * (mast_r(g, g["ztop"] - 3.0) + 0.1), g["ztop"] - 3.0)), 0.05)
            rope(p, r_, th, 0.06)
        ratlines(p, rim, thead, g["ztop"] + 0.6, g["zcross"] - 2.2)
        for dx in (-0.6, 0.6):
            rope(p, (x + dx, side * g["cross_w"] * 0.5, g["zcross"] - 0.2), (xg, side * (rg + 0.1), g["ztruck"] - 1.2), 0.045)
        # Backstays from the topmast and topgallant heads down to the channel's aft end.
        mid = m["id"]
        xa, xb = HH.channel_span(mid)
        F = HH.hull_frame(xa - 1.2, HH.channel_z(mid), side)
        foot = HH.lpt(F, 0.0, 0.2, HH.PLANK_T + 1.6)
        rope(p, (xt - 0.2, side * (rt + 0.1), g["zcross"] - 0.3), foot, 0.07)
        rope(p, (xg - 0.1, side * (rg + 0.08), g["ztruck"] - 1.3), foot + X * 1.0, 0.055)
    stays(p, m, g, xt, xg)
    export(p, name)


def stays(p, m, g, xt, xg):
    """The stays forward: from each masthead to the next mast forward (or the bowsprit)."""
    masts = {mm["id"]: mast_geom(mm) for mm in HUB["masts"]}
    sp = H.prop("main_deck", "bowsprit")
    a, b = Vector(sp["from"]), Vector(sp["to"])

    def on_sprit(t):
        return a + (b - a) * t + UP * 0.6
    x = g["x"]
    if m["id"] == "mizzen":
        mn = masts["main"]
        rope(p, (x + 0.8, 0.0, g["ztop"] + 1.0), (mn["x"] - mast_r(mn, ZM + 6.0), 0.0, ZM + 6.0), 0.1)
        rope(p, (xt + 0.4, 0.0, g["zcross"] + 0.5), (mn["x"] - 0.5, 0.0, mn["ztop"] + 0.3), 0.07)
        rope(p, (xg + 0.3, 0.0, g["ztruck"] - 1.0), (mn["x"], 0.0, mn["zcross"] + 0.2), 0.05)
    elif m["id"] == "main":
        fo = masts["fore"]
        deck = Z["fcTop"]
        for dy in (-0.35, 0.35):                                              # the main stay, doubled
            rope(p, (x + 0.8, dy, g["ztop"] + 1.0), (fo["x"] - mast_r(fo, deck + 1.0) - 0.3, dy, deck + 1.0), 0.16)
        rope(p, (xt + 0.4, 0.0, g["zcross"] + 0.5), (fo["x"] - 0.4, 0.0, fo["ztop"] + 0.3), 0.09)
        rope(p, (xg + 0.3, 0.0, g["ztruck"] - 1.0), (fo["x"], 0.0, fo["zcross"] + 0.2), 0.06)
    else:
        for dy in (-0.3, 0.3):
            rope(p, (x + 0.8, dy, g["ztop"] + 1.0), on_sprit(0.45) + Y * dy, 0.14)
        rope(p, (xt + 0.4, 0.0, g["zcross"] + 0.5), on_sprit(0.97), 0.08)
        rope(p, (xg + 0.3, 0.0, g["ztruck"] - 1.0), on_sprit(1.0) + UP * 0.2, 0.05)


def lateen(p, g):
    """The galleon's lateen yard on the mizzen: a long yard slung fore and aft across the mast, its fore end down, the sail
    furled along it; the halyard to the masthead, a vang to its upper end."""
    la = RIG["lateen"]
    x = g["x"]
    z = ZM + RIG["yards"]["at"][0] * g["h"]
    half = la["len"] * 0.5
    a = math.radians(la["rake"])
    c = Vector((x, -(mast_r(g, z) + 0.5), z))
    d = Vector((math.cos(a), 0.0, -math.sin(a)))
    fore, aft = c + d * half, c - d * half
    spar(p, fore, aft, 0.2, 0.2, rmid=0.48)
    up = d.cross(Y)
    if up.z < 0:
        up = -up
    furled(p, fore + d * -1.0 + up * 0.75, aft + d * 1.0 + up * 0.75, 1.0, up)
    rope(p, c + up * 0.5, (x, -(mast_r(g, g["ztop"]) + 0.1), g["ztop"] + 1.0), 0.1, HEMP)
    rope(p, fore, (fore.x, fore.y, ZM + 0.9), 0.06, HEMP)                       # the tack down to the deck
    rope(p, aft, (x - 4.0, 0.0, g["ztop"] - 1.0), 0.05, HEMP)


def build_spritsail():
    p = T.Part("SM_Hub_Spritsail")
    sp = H.prop("main_deck", "bowsprit")
    a, b = Vector(sp["from"]), Vector(sp["to"])
    ss = RIG["spritsail"]
    c = a + (b - a) * ss["at"] - UP * 1.6
    hl = ss["half"]
    spar(p, c - Y * hl, c + Y * hl, 0.15, 0.15, rmid=0.32)
    furled(p, c - Y * hl * 0.92 + UP * 0.45, c + Y * hl * 0.92 + UP * 0.45, 0.55, UP)
    rope(p, c + UP * 0.3, a + (b - a) * ss["at"] - UP * 0.5, 0.08, HEMP)
    for s in (-1.0, 1.0):
        rope(p, c + Y * s * hl * 0.97, a + (b - a) * (ss["at"] + 0.18) - UP * 0.4, 0.04, HEMP)
    export(p, "SM_Hub_Spritsail")


def build_pennant():
    """The pennant: a long cloth along +X from its hoist (the pivot at the hoist's top), oxblood over a cream stripe (two
    materials on the flag master with the same Length, so they stream together)."""
    pn = RIG["pennant"]
    L, hoist = pn["len"], pn["hoist"]
    p = T.Part("SM_Hub_Pennant")
    nu = 60
    for j, (v0, v1, mat) in enumerate(((0.0, 0.62, "MI_Hub_Pennant"), (0.62, 1.0, "MI_Hub_PennantCream"))):
        for i in range(nu):
            u0, u1 = i / nu, (i + 1) / nu
            quad = []
            for u, v in ((u0, v0), (u1, v0), (u1, v1), (u0, v1)):
                half = 0.5 * (hoist * (1.0 - u) ** 0.85 + 0.04 * u)
                zc = -0.5 * hoist
                y = 0.03 * math.sin(u * 9.0 + v * 2.0) * u
                quad.append((0.05 + (L - 0.05) * u, y, zc + half * (1.0 - 2.0 * v)))
            for pts in (quad, quad[::-1]):                                   # two-sided cloth
                p.polygon([Vector(q) for q in pts], mat, tone=0.5, smooth=True)
    rope(p, (0.03, 0.0, 0.0), (0.03, 0.0, -hoist), 0.03, HEMP)
    objs = p.finish(HH.material, ucx=False, pivot=(0.0, 0.0, 0.0))
    sb.save_blend(p.name)
    sb.export_fbx(objs, os.path.join("Hub", p.name + ".fbx"))


def export(p, name):
    HH.export(p)


BUILDERS = {"masts": lambda: [build_mast(m) or sb.reset_scene() for m in HUB["masts"]], "spritsail": build_spritsail,
            "pennant": build_pennant}

if __name__ == "__main__":
    random.seed(62)
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBRIG_OK", wanted)
