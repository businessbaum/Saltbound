"""The hub ship's rig (D62, reworked D69; outside.masts and hub.rig in HubShip_Data.json): a galleon's three masts, every sail
furled.

Each mast in three pieces as a real one is stepped: the lower mast with rope wooldings up to its round fighting top, the
topmast fidded forward of the lower masthead (a cap joins them) up to the crosstrees, the topgallant mast to the truck.
The square yards hang from slings with lifts to the mastheads and footropes under them; the spritsail yard hangs under the
bowsprit. Standing rigging is tarred dark: the shrouds from the channels' deadeyes (pairs of deadeyes with lanyards) up to the
mastheads with ratlines across, futtock shrouds under the tops, topmast and topgallant shrouds, backstays, the stays forward.
The pennant at the main truck is its own mesh on the flag material (it streams aft: she lies to her anchors, head to wind).

D69 (owner's review):
  - "make the sail normal": the mizzen's lateen yard is gone, its lowest yard is square like every other; each furled sail is
    gathered canvas (a lumpy roll on top of the yard, the bunt hanging fat in front of its middle, pinched by the gaskets that
    go round the sail and the yard, tapering to the yardarms), no longer a smooth tube.
  - "these nets dont look like ropes": the ratlines are hemp lines, not tarred bars: round, sagging a little between the
    shrouds, seized to each shroud with a knot, every fifth one running out to the aftmost shroud; the shrouds are thicker and
    rounder.
  - "loose ends and no physics in the ropes": every rope ends at something (a block on the yardarm, the masthead, a pin, a
    deadeye, an eyebolt in the rail): check_ends() measures each end against the spars, masts, blocks and decks and lists the
    loose ones (SB_RIG_LOOSE). Ropes sag by their length. The running rigging that hangs in the wind (braces, clewlines,
    halyard falls) is not in these meshes: its spans go to Art/Generated/hub_ropes.json and ASaltHubRopes makes them real
    cables that swing (hub_place.ropes); here only their blocks and eyebolts.

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
HEMP = "MI_Rope_Hemp"                # running rigging, ratlines, gaskets, lanyards
SPAR = "MI_Wood_Varnish"
HEADS = "MI_Wood_Tarred"
CLOTH = "MI_Hub_SailFurled"
HH.COLOURS.update({TAR_ROPE: (0.12, 0.1, 0.08), CLOTH: (0.78, 0.74, 0.64), "MI_Hub_Pennant": (0.45, 0.1, 0.07),
                   "MI_Hub_PennantCream": (0.85, 0.78, 0.6)})
GEN = os.path.join(H.ROOT, "Art", "Generated")
ROPES_JSON = os.path.join(GEN, "hub_ropes.json")

# --- what the ropes end on (check_ends) --------------------------------------------------------------------------------------
SPARS = []          # (a, b, radius): masts, yards, tops' rims, the bowsprit
ANCHORS = []        # points a rope may end at: blocks, deadeyes, pins, eyebolts
ENDS = []           # (point, what) of every rope end
SPANS = []          # the hanging ropes for ASaltHubRopes (ship metres)


def deck_z(x):
    """The weather deck at x: the forecastle's over its length, the quarterdeck's and the poop's aft, the main deck between."""
    if x >= HUB["fcX"]:
        return Z["fcTop"]
    if x < HUB["poopX"]:
        return Z["poop"]
    if x < HUB["qdX"]:
        return Z["sc2"]
    return ZM


def mast_geom(m):
    """Heights and radii of one mast: (x, h, top z, cross z, scale)."""
    h = m["h"]
    k = h / 76.0
    g = dict(x=m["x"], h=h, k=k, ztop=ZM + RIG["top"]["at"] * h, zcross=ZM + RIG["cross"]["at"] * h, ztruck=ZM + h,
             r_deck=RIG["thick"][0] * 0.5 * (0.85 + 0.15 * k), r_truck=RIG["thick"][1] * 0.5,
             top_r=RIG["top"]["w"] * 0.5 * k, cross_w=RIG["cross"]["w"] * k, scale=RIG["yards"]["scale"][m["id"]], id=m["id"])
    # The three pieces (D69: yards, slings and lifts hang on the piece standing at their height, not on the lower mast's axis):
    # the lower mast up to zl1, the topmast fidded forward of it (xt, rt), the topgallant forward of that (xg, rg).
    g["zl1"] = g["ztop"] + 2.2
    g["rt"] = mast_r(g, g["ztop"]) * 0.7
    g["xt"] = g["x"] + mast_r(g, g["zl1"]) + g["rt"] + 0.1
    g["rg"] = g["rt"] * 0.62
    g["xg"] = g["xt"] + g["rt"] * 0.8 + g["rg"] + 0.05
    return g


def piece(g, z):
    """The mast piece standing at height z: (its axis' x, its radius there)."""
    if z <= g["zl1"]:
        return g["x"], mast_r(g, z)
    z0, z1 = g["ztop"] - 1.8, g["zcross"] + 1.6
    if z <= z1:
        t = (z - z0) / (z1 - z0)
        return g["xt"], g["rt"] + (g["rt"] * 0.8 - g["rt"]) * t
    z0 = g["zcross"] - 1.4
    t = min((z - z0) / (g["ztruck"] - z0), 1.0)
    return g["xg"], g["rg"] + (g["r_truck"] * 0.8 - g["rg"]) * t


def rope(p, a, b, r, mat=TAR_ROPE, sag=None, sides=8, what="rope"):
    """A rope from a to b, sagging by `sag` metres in the middle (by default a little with its length: ropes are never rods)."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    if L < 0.05:
        return
    if sag is None:
        sag = min(0.004 * L, 0.35) if mat == TAR_ROPE else min(0.01 * L, 0.8)
    ENDS.append((a, what))
    ENDS.append((b, what))
    d = (b - a) / L
    acr = d.cross(UP) if abs(d.z) < 0.95 else d.cross(X)
    acr.normalize()
    if sag > 0.003:
        n = max(6, min(24, int(L / 1.5)))
        pts = [a + (b - a) * (k / n) - UP * (sag * 4.0 * (k / n) * (1.0 - k / n)) for k in range(n + 1)]
        f = T.polyline(pts, acr, acr.cross(d))
        p.round(f, T.length_of(f), r, mat, nseed(), sides=sides, seg=L / n + 0.01, wear=0.0)
    else:
        p.round(T.straight(a, b, acr, acr.cross(d)), L, r, mat, nseed(), sides=sides, seg=L + 1.0, wear=0.0)


def spar(p, a, b, r0, r1, mat=SPAR, sides=12, rmid=None):
    """A hewn, tapered spar from a (r0) to b (r1); with rmid it swells to rmid at the middle (a yard)."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    d = (b - a) / L
    SPARS.append((a, b, max(r0, r1, rmid or 0.0)))
    acr = d.cross(UP) if abs(d.z) < 0.95 else d.cross(X)
    acr.normalize()
    if rmid is None:
        rad = (lambda s: r0 + (r1 - r0) * s)
    else:
        rad = (lambda s: rmid - (rmid - r0) * (2.0 * abs(s - 0.5)) ** 1.6)
    p.round(T.straight(a, b, acr, acr.cross(d)), L, rad, mat, nseed(), sides=sides, seg=1.2, wear=0.6, drift=0.02)


def band(p, c, axis, r, h=0.18, mat="MI_Metal_Iron"):
    p.lathe([(r, -h * 0.5), (r + 0.04, 0.0), (r, h * 0.5)], c, axis, mat, sides=14, caps=False)


def block(p, c, axis, size=0.35, mat=HEADS):
    """A rope block: an oval wooden shell along `axis` with an iron strop round it and a hook eye at its head."""
    ax = Vector(axis).normalized()
    s = size
    p.lathe([(0.0, -0.5 * s), (0.22 * s, -0.45 * s), (0.3 * s, -0.2 * s), (0.32 * s, 0.0), (0.3 * s, 0.2 * s), (0.22 * s, 0.45 * s),
             (0.0, 0.5 * s)], c, ax, mat, sides=10, wear=0.8)
    band(p, c, ax.cross(UP).normalized() if abs(ax.z) < 0.95 else X, 0.33 * s, h=0.06 * s * 2)
    ANCHORS.append(Vector(c))


def eyebolt(p, c, n):
    """An iron eyebolt in a rail or a deck, its ring standing out along n."""
    n = Vector(n).normalized()
    p.lathe([(0.0, 0.0), (0.06, 0.0), (0.06, 0.04), (0.0, 0.04)], c, n, "MI_Metal_Iron", sides=8)
    u = n.cross(UP) if abs(n.z) < 0.9 else n.cross(X)
    u.normalize()
    p.beam(T.arc(c + n * 0.12, u, n, 0.08, 0.0, 2.0 * math.pi), 0.5, 0.03, 0.03, "MI_Metal_Iron", nseed(), seg=0.05, wear=0.4)
    ANCHORS.append(Vector(c) + n * 0.12)


# --- furled sails ------------------------------------------------------------------------------------------------------------
def _hull2(pts):
    """The convex hull of 2D points (monotone chain), counter-clockwise."""
    pts = sorted(set((round(x, 5), round(y, 5)) for x, y in pts))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, hi = [], []
    for q in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], q) <= 0:
            lo.pop()
        lo.append(q)
    for q in reversed(pts):
        while len(hi) >= 2 and cross(hi[-2], hi[-1], q) <= 0:
            hi.pop()
        hi.append(q)
    return lo[:-1] + hi[:-1]


def furled(p, a, b, r, up, fwd, yard_r=0.0):
    """A sail furled on top of a yard from a to b (the yard's axis, ship metres): gathered canvas, not a tube.

    The roll sits on the yard and a little forward of it; it is fattest at the bunt (the middle), where the gathered cloth hangs
    in a bag in front of the yard, and tapers to the yardarms. Its section is lumpy (folds that wander along it), and the gaskets
    pinch it every 1.6-2 m: each one a rope round the sail and the yard together."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    d = (b - a) / L
    up = Vector(up).normalized()
    fwd = Vector(fwd).normalized()
    up = (up - d * up.dot(d)).normalized()
    fwd = (fwd - d * fwd.dot(d) - up * fwd.dot(up)).normalized()
    rng = random.Random(nseed())
    n_g = max(3, int(L / 1.8))
    gaskets = [(k + rng.uniform(-0.12, 0.12)) / n_g for k in range(1, n_g)]
    folds = [(rng.uniform(1.0, 3.0), rng.uniform(0.6, 1.6) / max(L, 1.0) * 10.0, rng.uniform(0.0, 6.28), rng.uniform(0.04, 0.09))
             for _ in range(5)]
    K = 14

    def body(s):
        t = min(max(s, 0.0), 1.0)
        taper = math.sin(math.pi * t) ** 0.45
        bunt = math.exp(-((t - 0.5) / 0.1) ** 2)
        pinch = 1.0 - 0.2 * max(math.exp(-((t - g) * L / 0.22) ** 2) for g in gaskets)
        return r * (0.38 + 0.62 * taper + 0.25 * bunt) * pinch, bunt

    def ring(s):
        rr, bunt = body(s)
        c = a + (b - a) * s + up * (yard_r + rr * 0.78) + fwd * (rr * 0.22)
        pts = []
        for k in range(K):
            th = 2.0 * math.pi * k / K
            lump = 1.0 + sum(amp * math.sin(m * th + f * s * L + ph) for m, f, ph, amp in folds)
            ca, sa = math.cos(th), math.sin(th)
            v = fwd * (ca * rr * 1.06) + up * (sa * rr * 0.86)
            # The bunt's bag: the cloth gathered at the middle hangs in front of and below the yard.
            hang = max(0.0, ca * 0.6 - sa * 0.8)
            v += (fwd * 0.55 - up * 0.75) * (bunt * rr * 0.9 * hang ** 1.5)
            pts.append(c + v * lump)
        return pts, c, rr
    n = max(8, int(L / 0.25))
    rings = [ring(i / n) for i in range(n + 1)]
    bm = p.bm
    verts = [[bm.verts.new(q) for q in pts] for pts, _, _ in rings]
    tone = rng.uniform(0.2, 0.8)
    for i in range(n):
        for k in range(K):
            k1 = (k + 1) % K
            quad = [verts[i][k], verts[i][k1], verts[i + 1][k1], verts[i + 1][k]]
            per = 2.0 * math.pi * rings[i][2]
            uvs = [(L * i / n, per * k / K), (L * i / n, per * (k + 1) / K), (L * (i + 1) / n, per * (k + 1) / K), (L * (i + 1) / n, per * k / K)]
            cols = [(0.0, 0.25, tone, 1.0)] * 4
            p.face(quad, CLOTH, uvs, cols, smooth=True)
    for end, rv in ((0, verts[0]), (1, verts[-1])):
        c = rings[0][1] if end == 0 else rings[-1][1]
        cv = bm.verts.new(c + (d * (-0.08 if end == 0 else 0.08)))
        for k in range(K):
            k1 = (k + 1) % K
            tri = [rv[k1], rv[k], cv] if end == 0 else [rv[k], rv[k1], cv]
            p.face(tri, CLOTH, [(0.0, 0.0)] * 3, [(0.0, 0.25, tone, 1.0)] * 3, smooth=True)
    # The gaskets: each round the pinched sail and the yard (a loop in the section's plane, hugging both).
    for g in gaskets:
        pts, c, rr = ring(g)
        y0 = a + (b - a) * g
        local = []
        for q in pts:
            v = q - y0
            local.append((v.dot(fwd), v.dot(up)))
        for k in range(12):
            th = 2.0 * math.pi * k / 12
            local.append((math.cos(th) * (yard_r + 0.02), math.sin(th) * (yard_r + 0.02)))
        hull = _hull2(local)
        loop = [y0 + fwd * (u * 1.03) + up * (v * 1.03) for u, v in hull]
        loop.append(loop[0])
        f = T.polyline(loop, d, None)
        p.round(f, T.length_of(f, 60), 0.035, HEMP, nseed(), sides=5, seg=0.15, wear=0.0)


# --- yards -------------------------------------------------------------------------------------------------------------------
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


def pins(mx):
    """The belaying pins of the fife rail round the mast at x mx (build_hub_inside.fife_rail: 4 x 4 m, five pins a side), with
    the deck it stands on: [(x, y, z)] at the pin's waist."""
    zb = Z["fcTop"] if mx >= HUB["fcX"] else ZM
    out = []
    for k in range(1, 6):
        t = -2.0 + 4.0 * k / 6.0
        for sy in (-1.0, 1.0):
            out.append(Vector((mx + t, sy * 1.92, zb + 1.02)))
        for sx in (-1.0, 1.0):
            out.append(Vector((mx + sx * 1.92, t, zb + 1.02)))
    return out


def pin_for(mx, y, x_bias):
    """The free pin on the fife rail nearest a rope coming down on side y, a little towards x_bias."""
    best, bd = None, 1e9
    for q in pins(mx):
        if (q.y > 0.0) != (y > 0.0) or abs(q.y) < 1.0:
            continue
        key = (round(q.x, 2), round(q.y, 2))
        if key in USED_PINS:
            continue
        dd = abs(q.x - (mx + x_bias))
        if dd < bd:
            bd, best = dd, q
    if best is None:
        best = pins(mx)[0]
    USED_PINS.add((round(best.x, 2), round(best.y, 2)))
    ANCHORS.append(best)
    return best


USED_PINS = set()


def rail_point(x, side):
    """On the bulwark's cap rail at x (its top, inboard edge): where a brace is belayed."""
    zt = H.profile_top(x)
    w = HH.hw(x, zt - 0.1) - 0.2
    return Vector((x, side * w, zt + 0.12))


def square_yard(p, g, k, frac, half_frac):
    """A square yard at frac of the mast with its furled sail, sling, lifts and footropes; blocks at its yardarms for the
    braces, and the hanging ropes (braces aft to the rail, clewlines of the course and the halyard's fall down to the pins)."""
    x = g["x"]
    hl = half_frac * HUB["B"] * g["scale"]
    z = ZM + frac * g["h"]
    xm, r_mast = piece(g, z)
    rmid = 0.12 + 0.022 * hl
    xc = xm + r_mast + rmid + 0.12
    spar(p, (xc, -hl, z), (xc, hl, z), rmid * 0.5, rmid * 0.5, rmid=rmid)
    for s in (-1.0, 1.0):
        p.lathe([(0.0, 0.0), (rmid * 0.6, 0.0), (rmid * 0.6, 0.35), (rmid * 0.3, 0.45), (0.0, 0.5)], Vector((xc, s * hl, z)), Y * s, SPAR, sides=8)
        band(p, Vector((xc, s * hl * 0.97, z)), Y, rmid * 0.5 + 0.01, h=0.14)
    furled(p, (xc, -hl * 0.93, z), (xc, hl * 0.93, z), 0.22 + 0.016 * hl, UP, X, yard_r=rmid * 0.85)
    # Sling and parrel, lifts from the yardarms to blocks under the masthead above, footropes sagging under the yard.
    head = next_head(g, z)
    xh, rh = piece(g, head)
    rope(p, (xc, 0.0, z + rmid), (xh + rh, 0.0, head), 0.08, HEMP, sag=0.0, what="sling")
    p.beam(T.arc(Vector((xm, 0.0, z)), X, Y, r_mast + 0.08, 0.0, 2 * math.pi), 2 * math.pi * (r_mast + 0.08), 0.12, 0.1, HEMP, nseed(), seg=0.3)
    rope(p, (xm + r_mast + 0.06, 0.0, z), (xc - rmid * 0.9, 0.0, z), 0.06, HEMP, sag=0.0, what="parrel")
    for s in (-1.0, 1.0):
        arm = Vector((xc, s * hl * 0.97, z + rmid * 0.5 + 0.02))
        hb = Vector((xh + rh * 0.3, s * (rh + 0.35), head - 0.35))
        block(p, hb, (arm - hb).normalized(), size=0.42)
        rope(p, arm, hb - (hb - arm).normalized() * 0.2, 0.05, HEMP, what="lift")
        rope(p, (xc - 0.1, s * 0.6, z - rmid - 0.05), (xc - 0.1, s * hl * 0.95, z - rmid - 0.05), 0.04, HEMP, sag=1.1, what="footrope")
        for t in (0.35, 0.7):                                                       # stirrups holding the footrope up
            q = Vector((xc - 0.1, s * (0.6 + (hl * 0.95 - 0.6) * t), z))
            low = q - UP * (rmid + 0.05 + 1.1 * 4.0 * t * (1.0 - t))
            rope(p, q, low, 0.03, HEMP, sag=0.0, what="stirrup")
            ANCHORS.append(low)
        # The brace's block at the yardarm, on a pendant; the brace leads aft and down to the rail (a cable).
        bb = Vector((xc - 0.35, s * (hl + 0.25), z - 0.15))
        block(p, bb, X, size=0.4)
        rope(p, Vector((xc, s * hl * 0.98, z)), bb + X * 0.18, 0.04, HEMP, sag=0.0, what="pendant")
        xr = max(x - 9.0 - 5.0 * k, HUB["poopX"] + 3.0)
        rp = rail_point(xr, s)
        eyebolt(p, rp, UP)
        SPANS.append(dict(a=list(bb - X * 0.18), b=list(rp + UP * 0.12), slack=1.006 + 0.002 * k, width=0.07, wind=0.6, what="brace"))
    # The course (the lowest yard) is worked from the deck: its clewlines hang from the sail's quarters to the fife rail.
    if k == 0:
        for s in (-1.0, 1.0):
            q = Vector((xc + 0.15, s * hl * 0.55, z - rmid * 0.6))
            block(p, q, UP, size=0.3)
            pin = pin_for(x, s, 1.4)
            SPANS.append(dict(a=list(q - UP * 0.16), b=list(pin), slack=1.015, width=0.05, wind=1.2, what="clewline"))


# --- tops, crosstrees, deadeyes ----------------------------------------------------------------------------------------------
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
    for k in range(48):                                                     # the rim is somewhere a rope may end
        a = 2 * math.pi * k / 48
        ANCHORS.append(Vector((x + math.cos(a) * R, math.sin(a) * R, zt - 0.15)))


def crosstrees(p, g):
    x, zc, w = g["x"], g["zcross"], g["cross_w"]
    rm = mast_r(g, zc)
    for s in (-1.0, 1.0):
        p.beam(T.straight((x - 1.6, s * (rm + 0.15), zc - 0.3), (x + 2.0, s * (rm + 0.15), zc - 0.3), Y, UP), 3.6, 0.22, 0.3, HEADS, nseed())
    for dx in (-0.8, 1.2):
        p.beam(T.straight((x + dx, -w * 0.5, zc - 0.15), (x + dx, w * 0.5, zc - 0.15), X, UP), w, 0.24, 0.26, HEADS, nseed())
        SPARS.append((Vector((x + dx, -w * 0.5, zc - 0.15)), Vector((x + dx, w * 0.5, zc - 0.15)), 0.2))


def deadeye(p, c, n, up_dir):
    p.lathe([(0.0, -0.1), (0.3, -0.1), (0.36, -0.04), (0.36, 0.04), (0.3, 0.1), (0.0, 0.1)], c, n, "MI_Wood_Tarred", sides=12)
    ANCHORS.append(Vector(c))


def shroud_points(g, side):
    """The lower shrouds' feet (the upper deadeyes over the channel's) and heads (round the masthead)."""
    mid = g["id"]
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


def ratlines(p, feet, heads, z0, z1, step=0.42, r=0.03):
    """Ratlines: hemp lines across the shrouds, each seized to every shroud it crosses with a knot and sagging a little
    between them; four in five stop at the aftmost but one shroud (feet[0] is the aftmost), every fifth runs out to it."""
    def at(a, b, z):
        t = (z - a.z) / (b.z - a.z)
        return a + (b - a) * t
    rng = random.Random(nseed())
    z = z0
    j = 0
    while z < z1:
        zz = z + rng.uniform(-0.02, 0.02)
        pts = [at(f, h, zz) for f, h in zip(feet, heads)]
        span = pts if j % 5 == 0 else pts[1:]
        for a, b in zip(span[:-1], span[1:]):
            rope(p, a, b, r, HEMP, sag=min(0.012 + 0.012 * (b - a).length, 0.06), sides=6, what="ratline")
        for q, f, h in zip(pts, feet, heads):
            if j % 5 != 0 and f is feet[0]:
                continue
            ax = (h - f).normalized()
            p.lathe([(0.075, -0.06), (0.095, -0.03), (0.1, 0.0), (0.095, 0.03), (0.075, 0.06)], q, ax, HEMP, sides=8, caps=False, wear=0.0)
        z += step
        j += 1


# --- a mast ------------------------------------------------------------------------------------------------------------------
def build_mast(m):
    name = "SM_Hub_Mast" + m["id"].capitalize()
    p = T.Part(name)
    g = mast_geom(m)
    x = g["x"]
    # Lower mast: from under the deck to above the top, with rope wooldings and iron hoops at the head.
    zl0, zl1 = ZM - 0.6, g["zl1"]
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
    rt, xt = g["rt"], g["xt"]
    p.block(x - mast_r(g, zl1) - 0.2, xt + rt + 0.25, -mast_r(g, zl1) - 0.15, mast_r(g, zl1) + 0.15, zl1, zl1 + 0.5, HEADS, nseed(), axis="x")
    spar(p, (xt, 0.0, g["ztop"] - 1.8), (xt, 0.0, g["zcross"] + 1.6), rt, rt * 0.8, sides=12)
    crosstrees(p, g)
    rg, xg = g["rg"], g["xg"]
    p.block(xt - rt - 0.15, xg + rg + 0.2, -rt - 0.1, rt + 0.1, g["zcross"] + 1.6, g["zcross"] + 1.95, HEADS, nseed(), axis="x")
    spar(p, (xg, 0.0, g["zcross"] - 1.4), (xg, 0.0, g["ztruck"]), rg, g["r_truck"] * 0.8, sides=10)
    if m["id"] == "main":                                                   # where the pennant flies (the map script reads it)
        with open(os.path.join(GEN, "hub_rig.json"), "w", encoding="utf-8") as fh:
            json.dump({"pennant": [xg, 0.0, g["ztruck"] + 0.3]}, fh)
    p.lathe([(0.0, 0.0), (g["r_truck"] + 0.25, 0.0), (g["r_truck"] + 0.3, 0.12), (g["r_truck"] + 0.2, 0.3), (0.0, 0.34)],
            Vector((xg, 0.0, g["ztruck"])), UP, SPAR, sides=12)                                     # the truck
    # Yards: square, every one (D69: the mizzen's lateen yard is gone).
    for k, (frac, half) in enumerate(zip(RIG["yards"]["at"], RIG["yards"]["half"])):
        square_yard(p, g, k, frac, half)
    # Standing rigging: lower shrouds with deadeyes, lanyards and ratlines; futtock, topmast and topgallant shrouds;
    # backstays; the stays forward.
    for side in (1.0, -1.0):
        sp = shroud_points(g, side)
        for lower, upper, head, n in sp:
            deadeye(p, upper, n, (head - upper).normalized())
            ANCHORS.append(lower)                                               # the channel's deadeye (build_hub_hull.channels)
            for dy in (-0.15, 0.0, 0.15):
                off = n.cross((upper - lower).normalized()).normalized() * dy
                rope(p, lower + off + (upper - lower).normalized() * 0.2, upper + off - (upper - lower).normalized() * 0.2, 0.025, HEMP,
                     sides=5, sag=0.0, what="lanyard")
            rope(p, upper, head, 0.095, TAR_ROPE, sides=10, what="shroud")
            SPARS.append((upper, head, 0.1))
            ANCHORS.append(head)
        feet = [u + (h - u).normalized() * 0.5 for _, u, h, _ in sp]
        heads = [h for _, _, h, _ in sp]
        ratlines(p, feet, heads, max(f.z for f in feet) + 0.4, g["ztop"] - 3.2)
        # Futtock shrouds from the top's rim down to the mast; topmast shrouds from the rim up to the topmast head.
        n_t = 4
        rim = [Vector((x - g["top_r"] * 0.5 + g["top_r"] * 0.8 * k / (n_t - 1), side * g["top_r"] * 0.85, g["ztop"] - 0.2)) for k in range(n_t)]
        thead = [Vector((xt, side * (rt + 0.15), g["zcross"] - 0.5)) for _ in range(n_t)]
        for r_, th in zip(rim, thead):
            band(p, Vector((x, 0.0, g["ztop"] - 3.0)), UP, mast_r(g, g["ztop"] - 3.0) + 0.04, h=0.25)       # the futtock band
            rope(p, r_, Vector((x, side * (mast_r(g, g["ztop"] - 3.0) + 0.1), g["ztop"] - 3.0)), 0.06, sag=0.0, what="futtock")
            rope(p, r_, th, 0.07, sides=8, what="topmast shroud")
            SPARS.append((r_, th, 0.08))
            ANCHORS.append(th)
        ratlines(p, rim, thead, g["ztop"] + 0.6, g["zcross"] - 2.2, r=0.026)
        for dx in (-0.8, 1.2):                                              # from the crosstrees' ends
            ct = Vector((x + dx, side * g["cross_w"] * 0.5, g["zcross"] - 0.15))
            tg = Vector((xg, side * (rg + 0.1), g["ztruck"] - 1.2))
            rope(p, ct, tg, 0.05, what="topgallant shroud")
            ANCHORS.append(tg)
        # Backstays from the topmast and topgallant heads down to the channel's aft end, set up with deadeyes there.
        mid = m["id"]
        xa, xb = HH.channel_span(mid)
        F = HH.hull_frame(xa - 1.2, HH.channel_z(mid), side)
        foot = HH.lpt(F, 0.0, 0.2, HH.PLANK_T + 1.6)
        for kb, (top_pt, r_b) in enumerate((((xt - 0.2, side * (rt + 0.1), g["zcross"] - 0.3), 0.08),
                                            ((xg - 0.1, side * (rg + 0.08), g["ztruck"] - 1.3), 0.06))):
            ft = foot + X * (1.0 * kb)
            de = ft + (Vector(top_pt) - ft).normalized() * 1.1
            deadeye(p, de, F[3], (Vector(top_pt) - de).normalized())
            rope(p, ft, de, 0.03, HEMP, sag=0.0, sides=5, what="backstay lanyard")
            ANCHORS.append(ft)
            rope(p, de, top_pt, r_b, sides=8, what="backstay")
            ANCHORS.append(Vector(top_pt))
    stays(p, m, g, xt, xg)
    check_ends(name)
    HH.export(p)


def stays(p, m, g, xt, xg):
    """The stays forward: from each masthead to the next mast forward (or the bowsprit)."""
    masts = {mm["id"]: mast_geom(mm) for mm in HUB["masts"]}
    sp = H.prop("main_deck", "bowsprit")
    a, b = Vector(sp["from"]), Vector(sp["to"])

    def on_sprit(t):
        return a + (b - a) * t + UP * (0.95 - 0.5 * t)
    x = g["x"]
    if m["id"] == "mizzen":
        mn = masts["main"]
        rope(p, (x + 0.8, 0.0, g["ztop"] + 1.0), (mn["x"] - mast_r(mn, ZM + 6.0), 0.0, ZM + 6.0), 0.1, what="stay")
        rope(p, (xt + 0.4, 0.0, g["zcross"] + 0.5), (mn["x"] - 0.5, 0.0, mn["ztop"] + 0.3), 0.07, what="stay")
        rope(p, (xg + 0.3, 0.0, g["ztruck"] - 1.0), (mn["x"], 0.0, mn["zcross"] + 0.2), 0.05, what="stay")
        ANCHORS.extend([Vector((mn["x"] - mast_r(mn, ZM + 6.0), 0.0, ZM + 6.0)), Vector((mn["x"] - 0.5, 0.0, mn["ztop"] + 0.3)),
                        Vector((mn["x"], 0.0, mn["zcross"] + 0.2))])
    elif m["id"] == "main":
        fo = masts["fore"]
        deck = Z["fcTop"]
        for dy in (-0.35, 0.35):                                              # the main stay, doubled
            rope(p, (x + 0.8, dy, g["ztop"] + 1.0), (fo["x"] - mast_r(fo, deck + 1.0) - 0.3, dy, deck + 1.0), 0.16, what="stay")
            ANCHORS.append(Vector((fo["x"] - mast_r(fo, deck + 1.0) - 0.3, dy, deck + 1.0)))
        rope(p, (xt + 0.4, 0.0, g["zcross"] + 0.5), (fo["x"] - 0.4, 0.0, fo["ztop"] + 0.3), 0.09, what="stay")
        rope(p, (xg + 0.3, 0.0, g["ztruck"] - 1.0), (fo["x"], 0.0, fo["zcross"] + 0.2), 0.06, what="stay")
        ANCHORS.extend([Vector((fo["x"] - 0.4, 0.0, fo["ztop"] + 0.3)), Vector((fo["x"], 0.0, fo["zcross"] + 0.2))])
    else:
        for dy in (-0.3, 0.3):
            rope(p, (x + 0.8, dy, g["ztop"] + 1.0), on_sprit(0.45) + Y * dy, 0.14, what="stay")
            ANCHORS.append(on_sprit(0.45) + Y * dy)
        rope(p, (xt + 0.4, 0.0, g["zcross"] + 0.5), on_sprit(0.97), 0.08, what="stay")
        rope(p, (xg + 0.3, 0.0, g["ztruck"] - 1.0), on_sprit(1.0) + UP * 0.2, 0.05, what="stay")
        ANCHORS.extend([on_sprit(0.97), on_sprit(1.0) + UP * 0.2])
    # Every stay starts at a masthead of this mast.
    ANCHORS.extend([Vector((x + 0.8, dy, g["ztop"] + 1.0)) for dy in (-0.35, -0.3, 0.0, 0.3, 0.35)])
    ANCHORS.extend([Vector((xt + 0.4, 0.0, g["zcross"] + 0.5)), Vector((xg + 0.3, 0.0, g["ztruck"] - 1.0))])


def check_ends(name, tol=0.3):
    """Every rope end must touch something: a spar or mast (within its radius + tol), a block, deadeye, pin or eyebolt, a deck
    or the hull's side. Prints each loose end (SB_RIG_LOOSE) and the count; the build goes on (the list is the work to do)."""
    def seg_dist(q, a, b):
        ab = b - a
        t = min(max((q - a).dot(ab) / max(ab.length_squared, 1e-9), 0.0), 1.0)
        return (a + ab * t - q).length
    loose = 0
    for q, what in ENDS:
        ok = any(seg_dist(q, a, b) < r + tol for a, b, r in SPARS) or any((q - c).length < tol + 0.25 for c in ANCHORS)
        if not ok and abs(q.y) > HH.hw(q.x, q.z) - 0.5 and q.z < H.profile_top(q.x) + 0.3:
            ok = True                                                           # on the hull's side or its channels
        if not ok and abs(q.z - deck_z(q.x)) < 0.5:
            ok = True                                                           # on a deck
        if not ok:
            loose += 1
            print("SB_RIG_LOOSE %s %s at (%.2f, %.2f, %.2f)" % (name, what, q.x, q.y, q.z))
    print("SB_RIG_ENDS %s %d ends, %d loose" % (name, len(ENDS), loose))
    ENDS.clear()
    SPARS.clear()
    ANCHORS.clear()


def build_spritsail():
    p = T.Part("SM_Hub_Spritsail")
    sp = H.prop("main_deck", "bowsprit")
    a, b = Vector(sp["from"]), Vector(sp["to"])
    SPARS.append((a, b, 0.9))
    ss = RIG["spritsail"]
    c = a + (b - a) * ss["at"] - UP * 1.6
    hl = ss["half"]
    spar(p, c - Y * hl, c + Y * hl, 0.15, 0.15, rmid=0.32)
    furled(p, c - Y * hl * 0.92, c + Y * hl * 0.92, 0.36, UP, X, yard_r=0.28)
    rope(p, c + UP * 0.3, a + (b - a) * ss["at"] - UP * 0.5, 0.08, HEMP, sag=0.0, what="sling")
    ANCHORS.append(c + UP * 0.3)
    for s in (-1.0, 1.0):
        arm = c + Y * s * hl * 0.97
        rope(p, arm + UP * 0.1, a + (b - a) * (ss["at"] + 0.18) - UP * 0.4, 0.04, HEMP, what="lift")
        # Its braces lead aft to the forecastle's rail (cables), from blocks at the yardarms.
        bb = arm + Y * s * 0.25 - X * 0.3
        block(p, bb, X, size=0.32)
        rp = rail_point(152.0, s)
        eyebolt(p, rp, UP)
        SPANS.append(dict(a=list(bb - X * 0.15), b=list(rp + UP * 0.12), slack=1.01, width=0.05, wind=0.8, what="spritsail brace"))
    check_ends("SM_Hub_Spritsail")
    HH.export(p)


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
    rope(p, (0.03, 0.0, 0.0), (0.03, 0.0, -hoist), 0.03, HEMP, sag=0.0)
    ENDS.clear()
    objs = p.finish(HH.material, ucx=False, pivot=(0.0, 0.0, 0.0))
    sb.save_blend(p.name)
    sb.export_fbx(objs, os.path.join("Hub", p.name + ".fbx"))


def write_spans():
    with open(ROPES_JSON, "w", encoding="utf-8") as fh:
        json.dump({"_comment": "D69 the hub's hanging ropes (build_hub_rig.py): a and b in ship metres, slack = length / distance, "
                   "width in metres, wind = how much it moves; ASaltHubRopes makes each a cable (hub_place.ropes).",
                   "spans": [dict(s, a=[round(v, 3) for v in s["a"]], b=[round(v, 3) for v in s["b"]]) for s in SPANS]}, fh, indent=1)
    print("SB_RIG_SPANS", len(SPANS), ROPES_JSON)


BUILDERS = {"masts": lambda: [build_mast(m) or sb.reset_scene() for m in HUB["masts"]], "spritsail": build_spritsail,
            "pennant": build_pennant}

if __name__ == "__main__":
    random.seed(62)
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    if "masts" in wanted and "spritsail" in wanted:
        write_spans()
    print("SB_HUBRIG_OK", wanted)
