"""The hub ship's ends and castles from outside (D62; phase 2 of the hub prompt): the galleon's stern, its quarter
galleries, the castles' decks, rails and fronts, the courtyard walls over the dock, and the head. Hand-made like the boat;
every number from HubShip_Data.json through hub_layout.py (outside.stern, outside.bow, hub.arch, hub.helm, hub.rudders,
hub.cables, hub.beakhead). Visual only (the blockout stays in the map as collision).

Meshes (pivot at the ship's x 80 on the waterline; /Game/Saltbound/Hub/Art; Nanite):
  SM_Hub_Stern      the transom with the dock mouth and its arch, the seven great windows, the frieze with her name, the
                    taffrail, the stern lanterns, the lashed great wheel, the twin rudders, the stern moorings
  SM_Hub_Galleries  the two quarter galleries (two tiers of windows, the brass drop, the dome)
  SM_Hub_Castles    the poop, quarterdeck and forecastle decks, their rails and balustrades, the castles' fronts, the
                    courtyard walls over the dock, the arch's vault and forward face
  SM_Hub_Head       the stem, the knee of the head, the beakhead with its headrails and figurehead, the bowsprit, the
                    catheads, the hawse holes and bow cables, the bobstay
Run: Tools/Build-BlenderAsset.ps1 build_hub_ends [stern] [galleries] [castles] [head]
"""
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
X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
nseed = HH.nseed
PT = HH.PLANK_T
ROW = 0.5                       # transom and wall board width
DECK_W, DECK_T = 0.32, 0.1      # deck boards on the castles
POOP_X = HUB["poopX"]
FRONT_X = HUB["basin"]["x1"] + 0.3          # the quarterdeck's front over the main deck (as the blockout's)
MOUTH, QUAY_Y = HUB["basin"]["y1"], HUB["quayY"]
ARCH = HUB["arch"]
TEAK = ("MI_Wood_Teak", "MI_Wood_Teak", "MI_Wood_TeakDark", "MI_Wood_TeakLight")


def arch_z(y):
    a = HUB["arch"]
    top, spring, half = a["z0"], Z["sc1"], MOUTH
    rise = top - spring
    r = (half * half + rise * rise) / (2.0 * rise)
    zc = top - r
    return zc + math.sqrt(max(r * r - y * y, 0.0)) if abs(y) <= half else spring


def arch_centre():
    a = HUB["arch"]
    rise = a["z0"] - Z["sc1"]
    r = (MOUTH * MOUTH + rise * rise) / (2.0 * rise)
    return a["z0"] - r, r


def hw(x, z):
    return H.half_width(x, z)


def plane(p, t, a, n):
    return (Vector(p), Vector(t).normalized(), Vector(a).normalized(), Vector(n).normalized())


def board_runs(part, F, u_ranges, v0, v1, d, mats, wear=(0.6, 1.0), seg=0.6, lmin=3.0, lmax=6.5, thick=0.12, grime=None):
    """Boards along the plane's t between v0 and v1 for each (u0, u1) range, with a dark core behind them."""
    rng = random.Random(nseed())
    p, t, a, n = F
    w = v1 - v0
    for u0, u1 in u_ranges:
        if u1 - u0 < 0.15:
            continue
        part.polygon([HH.lpt(F, u0, v0, 0.0), HH.lpt(F, u1, v0, 0.0), HH.lpt(F, u1, v1, 0.0), HH.lpt(F, u0, v1, 0.0)],
                     "MI_Caulk_Dark", tone=0.2, facing=n)
        u = u0
        first = True
        while u < u1 - 1e-6:
            ue = min(u + (rng.uniform(0.6, lmax) if first else rng.uniform(lmin, lmax)), u1)
            first = False
            if u1 - ue < 0.8:
                ue = u1
            pa, pb = HH.lpt(F, u + 0.005, 0.5 * (v0 + v1), d + thick * 0.5), HH.lpt(F, ue - 0.005, 0.5 * (v0 + v1), d + thick * 0.5)
            part.board(T.straight(pa, pb, a, n), (pb - pa).length, w - 0.025, thick, rng.choice(mats), nseed(), seg=seg,
                       wear=rng.uniform(*wear), grime=grime, bend=0.01, twist=0.5, slant=0.02)
            u = ue


def intervals(keep, lo, hi, step=0.05):
    out, start = [], None
    y = lo
    while y <= hi + 1e-9:
        k = keep(y)
        if k and start is None:
            start = y
        if not k and start is not None:
            out.append((start, y - step))
            start = None
        y += step
    if start is not None:
        out.append((start, hi))
    return out


def window_unit(part, F, hw_, hh, d, kind, frame_mat, pointed=0.0, panes=(1, 2), hood=True):
    """A window on plane F centred at its origin: frame, mullions and transoms, glass (dark or lit), or oxblood shutters."""
    glass = "MI_LanternGlass" if kind == "lit" else "MI_Glass_Dark"
    p, t, a, n = F
    outline = [(-hw_, -hh), (hw_, -hh), (hw_, hh - pointed)]
    if pointed > 0.0:
        for k in range(1, 8):                                    # a pointed (two-centred) head
            s = k / 8.0
            outline.append((hw_ * (1.0 - s), hh - pointed + pointed * math.sin(s * math.pi * 0.5)))
        outline.append((0.0, hh))
        for k in range(7, 0, -1):
            s = k / 8.0
            outline.append((-hw_ * (1.0 - s), hh - pointed + pointed * math.sin(s * math.pi * 0.5)))
    outline.append((-hw_, hh - pointed))
    part.polygon([HH.lpt(F, u, v, d) for u, v in outline], glass, tone=0.5, facing=n)
    pts = [HH.lpt(F, u, v, d + 0.11) for u, v in outline] + [HH.lpt(F, outline[0][0], outline[0][1], d + 0.11)]
    for pa, pb in zip(pts[:-1], pts[1:]):
        if (pb - pa).length < 1e-3:
            continue
        part.beam(T.straight(pa, pb, n.cross((pb - pa).normalized()), n), (pb - pa).length + 0.12, 0.16, 0.22, frame_mat, nseed(), seg=1.0, wear=0.8)
    nv, nh = panes
    for k in range(1, nv + 1):
        u = -hw_ + 2 * hw_ * k / (nv + 1)
        HH.lbeam(part, F, u, -hh, u, hh - pointed * 0.3, 0.08, 0.08, d, frame_mat, wear=0.6)
    for k in range(1, nh + 1):
        v = -hh + (2 * hh - pointed) * k / (nh + 1)
        HH.lbeam(part, F, -hw_, v, hw_, v, 0.07, 0.08, d, frame_mat, wear=0.6)
    HH.lbeam(part, F, -hw_ - 0.22, -hh - 0.1, hw_ + 0.22, -hh - 0.1, 0.2, 0.26, d + 0.05, frame_mat)            # the sill
    if hood:
        HH.lbeam(part, F, -hw_ - 0.28, hh + 0.2, hw_ + 0.28, hh + 0.2, 0.16, 0.26, d + 0.08, frame_mat)
    if kind == "shutter":
        for sgn in (-1.0, 1.0):
            for k in range(2):
                u = sgn * (hw_ * (k + 0.5) / 2)
                HH.lbeam(part, F, u, -hh + 0.03, u, hh - 0.03 - pointed, hw_ / 2 - 0.015, 0.06, d + 0.12,
                         "MI_Paint_Hull" if random.random() < 0.7 else "MI_Paint_HullFaded", wear=1.0)


def balustrade(part, a, b, z, height=0.9, side_out=None, posts=2.4, mat="MI_Wood_Varnish", baluster="MI_Paint_Cream"):
    """A galleon's balustrade from a to b (plan points) on a deck at z: posts, a bottom rail, turned balusters, a cap rail."""
    a, b = Vector((a[0], a[1], z)), Vector((b[0], b[1], z))
    d = (b - a)
    L = d.length
    if L < 0.3:
        return
    d.normalize()
    across = UP.cross(d)
    part.beam(T.straight(a + UP * 0.1, b + UP * 0.1, across, UP), L, 0.24, 0.16, mat, nseed(), seg=1.0)
    part.beam(T.straight(a + UP * (height - 0.08), b + UP * (height - 0.08), across, UP), L + 0.1, 0.34, 0.16, mat, nseed(), seg=1.0)
    n_post = max(1, int(round(L / posts)))
    for k in range(n_post + 1):
        c = a + d * (L * k / n_post)
        part.beam(T.straight(c, c + UP * (height - 0.16), d, across), height - 0.16, 0.26, 0.26, mat, nseed(), seg=1.0)
    n_bal = int(L / 0.36)
    prof = [(0.07, 0.0), (0.07, 0.06), (0.05, 0.1), (0.09, 0.28), (0.1, 0.36), (0.06, 0.5), (0.045, 0.56), (0.06, 0.58), (0.06, 0.62)]
    h = height - 0.34
    for k in range(1, n_bal):
        c = a + d * (L * k / n_bal) + UP * 0.18
        if any(abs(L * k / n_bal - L * j / n_post) < 0.2 for j in range(n_post + 1)):
            continue
        part.lathe([(r, hh * h / 0.62) for r, hh in prof], c, UP, baluster, sides=8, wear=0.7)


# --- the stern ----------------------------------------------------------------------------------------------------------
GREAT_WIN = dict(z0=26.2, z1=31.5, w=1.8, pitch=2.8, n=7)     # outside.stern
FRIEZE = (32.0, 33.3)


def stern_blocked(y, z):
    """True where the transom has no boards: the dock mouth, the quays' mouths, the great windows' band."""
    if abs(y) < MOUTH and z < arch_z(y) + 0.35:
        return True
    if abs(y) < QUAY_Y + 0.3 and Z["quay"] - 0.3 < z < Z["lower"] + 0.25:
        return True
    if abs(y) < MOUTH + 0.3 and GREAT_WIN["z0"] - 0.35 < z < FRIEZE[1] + 0.1:
        return True
    return False


WING_WINDOWS = [(Z["main"] + 0.8, "glass"), (Z["sc1"] + 0.8, "lit"), (Z["sc2"] + 0.8, "glass"), (Z["sc3"] + 0.8, "shutter")]
WING_PORTS = [Z["lower"] + 1.0, Z["middle"] + 1.0, Z["upper"] + 1.0]
WING_Y = 13.4


def stern_openings():
    """The wings' stern windows and ports (y, z0, z1, w, kind)."""
    out = []
    for side in (1, -1):
        for z0, kind in WING_WINDOWS:
            out.append((side * WING_Y, z0, z0 + 2.0, 1.3, kind if side > 0 else ("lit" if kind == "glass" else kind)))
        for z0 in WING_PORTS:
            out.append((side * WING_Y, z0, z0 + 1.0, 1.0, "shut"))
    return out


def build_stern():
    p = T.Part("SM_Hub_Stern")
    opens = stern_openings()
    # Transom rows from the ancient band to the poop rail.
    z = HH.STONE
    rows = []
    while z < H.profile_top(0.0) - 0.05:
        zt = min(z + ROW, H.profile_top(0.0) - 0.02)
        rows.append((z, zt))
        z = zt
    F0 = plane((0.0, 0.0, 0.0), Y, UP, -X)
    for z0, z1 in rows:
        zm = 0.5 * (z0 + z1)

        def keep(y):
            if abs(y) > hw(0.3, zm) - 0.3:
                return False
            if stern_blocked(y, z0 + 0.03) or stern_blocked(y, z1 - 0.03):
                return False
            for oy, oz0, oz1, w, kind in opens:
                if kind == "shut":
                    continue
                if abs(y - oy) < w * 0.5 + 0.12 and oz0 - 0.2 < zm < oz1 + 0.2:
                    return False
            return True
        rngs = intervals(keep, -hw(0.3, zm), hw(0.3, zm))
        board_runs(p, (Vector((0.0, 0.0, zm)), Y, UP, -X), rngs, -(z1 - z0) * 0.5, (z1 - z0) * 0.5, 0.0,
                   ("MI_Hub_HullTar", "MI_Hub_HullTar", "MI_Hub_HullTarFaded", "MI_Hub_HullTarDark"), lmin=2.5, lmax=6.0)
    # The wings' windows and ports on the transom.
    for oy, oz0, oz1, w, kind in opens:
        F = (Vector((0.0, oy, 0.5 * (oz0 + oz1))), Y, UP, -X)
        if kind == "shut":
            f = dict(side=1, x0=oy - w * 0.5, x1=oy + w * 0.5, z0=oz0, z1=oz1, kind="shut", hole=False)
            HH.fitting(p, f, F=F)
        else:
            window_unit(p, F, w * 0.5, 1.0, -0.05, kind, "MI_Paint_Cream")
    # Corner posts up the stern's edges, carried above the rail for the lanterns.
    for side in (1, -1):
        pts = [(-0.25, side * (hw(0.3, z) - 0.2), z) for z in [HH.STONE - 0.3] + [4.0 + 2.0 * k for k in range(16)] if z < 34.4] + \
              [(-0.25, side * (hw(0.3, 34.4) - 0.2), 34.4), (-0.25, side * (hw(0.3, 34.4) - 0.2), 36.3)]
        f = T.polyline(pts, X, lambda q, s=side: Vector((0.0, s, 0.0)))
        p.beam(f, T.length_of(f, 60), 0.7, 0.75, "MI_Hub_HullTarDark", nseed(), seg=0.8, wear=1.0)
        for zc in (Z["lower"], Z["main"], Z["poop"]):                                     # ochre capitals at the bands
            c = Vector((-0.3, side * (hw(0.3, zc) - 0.2), zc))
            p.block(c.x - 0.45, c.x + 0.25, c.y - 0.45, c.y + 0.45, zc - 0.45, zc + 0.45, "MI_Hub_Ochre", nseed(), axis="z")
    # The arch: a heavy rib over the mouth (narrower at the crown, under the windows' sill), ochre springers and keystone.
    zc, r = arch_centre()
    ys = [-MOUTH - 0.4 + (2 * MOUTH + 0.8) * k / 40 for k in range(41)]

    def rib_w(y):
        return 0.42 + 0.4 * (min(abs(y), MOUTH) / MOUTH) ** 2

    pts = [Vector((-0.35, y, arch_z(max(min(y, MOUTH), -MOUTH)) + rib_w(y) * 0.5)) for y in ys]

    def rib_frame(s):
        k = min(int(s * 40), 39)
        tt = s * 40 - k
        q = pts[k].lerp(pts[k + 1], tt)
        y = q.y
        nrm = Vector((0.0, -y, zc - arch_z(max(min(y, MOUTH), -MOUTH)))).normalized()
        return q, -nrm, -X, rib_w(y) / 0.7
    p.beam(rib_frame, T.length_of(rib_frame, 80), 0.7, 0.7, "MI_Hub_HullTarDark", nseed(), seg=0.5, wear=1.0)
    for side in (1, -1):
        p.block(-0.75, 0.05, side * MOUTH - 0.6, side * MOUTH + 0.6, Z["sc1"] - 0.6, Z["sc1"] + 0.5, "MI_Hub_Ochre", nseed(), axis="z")
        # The mouth's side posts down into the water (the sea gate runs in grooves beside them).
        p.block(-0.65, 0.1, side * (MOUTH + 0.5) - 0.5, side * (MOUTH + 0.5) + 0.5, -2.0, Z["sc1"] - 0.6, "MI_Hub_HullTarDark", nseed(), axis="z")
        # Over the quay's mouth: a lintel and the jamb outboard of it.
        p.block(-0.55, 0.0, side * MOUTH, side * (QUAY_Y + 0.6), Z["lower"] - 0.05, Z["lower"] + 0.45, "MI_Hub_HullTarDark", nseed(), axis="y")
        p.block(-0.55, 0.0, side * (QUAY_Y + 0.3) - 0.3, side * (QUAY_Y + 0.3) + 0.3, Z["quay"] - 0.2, Z["lower"] - 0.05, "MI_Hub_HullTarDark", nseed(), axis="z")
    p.block(-0.8, 0.0, -0.45, 0.45, Z["sc2"] - 0.25, Z["sc2"] + 0.5, "MI_Hub_Ochre", nseed(), axis="z")     # keystone
    # The great windows: pointed tall lights between oxblood pilasters, a sill moulding and a cornice.
    gw = GREAT_WIN
    p.polygon([(0.35, -MOUTH - 0.3, gw["z0"] - 0.35), (0.35, MOUTH + 0.3, gw["z0"] - 0.35), (0.35, MOUTH + 0.3, FRIEZE[1] + 0.25),
               (0.35, -MOUTH - 0.3, FRIEZE[1] + 0.25)], "MI_Paint_HullDark", tone=0.3, facing=-X)
    for k in range(gw["n"]):
        yc = (k - (gw["n"] - 1) * 0.5) * gw["pitch"]
        F = (Vector((0.0, yc, 0.5 * (gw["z0"] + gw["z1"]))), Y, UP, -X)
        window_unit(p, F, gw["w"] * 0.5, 0.5 * (gw["z1"] - gw["z0"]), -0.12, "lit" if abs(k - 3) <= 1 else "glass",
                    "MI_Paint_Cream", pointed=0.9, panes=(1, 3), hood=False)
    for k in range(gw["n"] + 1):
        yc = (k - gw["n"] * 0.5) * gw["pitch"]
        yc = max(min(yc, MOUTH - 0.2), -MOUTH + 0.2) if abs(yc) > MOUTH - 0.2 else yc
        p.block(-0.45, -0.02, yc - 0.5, yc + 0.5, gw["z0"] - 0.3, FRIEZE[0] - 0.2, "MI_Paint_Hull", nseed(), axis="z")
        p.block(-0.6, -0.02, yc - 0.6, yc + 0.6, FRIEZE[0] - 0.55, FRIEZE[0] - 0.2, "MI_Hub_Ochre", nseed(), axis="y")
        p.block(-0.6, -0.02, yc - 0.6, yc + 0.6, gw["z0"] - 0.35, gw["z0"] - 0.05, "MI_Hub_Ochre", nseed(), axis="y")
    p.block(-0.75, -0.02, -MOUTH - 0.6, MOUTH + 0.6, gw["z0"] - 0.78, gw["z0"] - 0.3, "MI_Paint_Cream", nseed(), axis="y")
    p.block(-0.7, -0.02, -MOUTH - 0.6, MOUTH + 0.6, FRIEZE[0] - 0.2, FRIEZE[0], "MI_Paint_Cream", nseed(), axis="y")
    # The frieze with her name in brass letters, a brass border, a cornice over it.
    p.block(-0.35, -0.02, -MOUTH - 0.3, MOUTH + 0.3, FRIEZE[0], FRIEZE[1], "MI_Paint_HullDark", nseed(), axis="y")
    for zb in (FRIEZE[0] + 0.06, FRIEZE[1] - 0.06):
        p.block(-0.47, -0.3, -MOUTH - 0.2, MOUTH + 0.2, zb - 0.06, zb + 0.06, "MI_Metal_Brass", nseed(), axis="y", wear=0.4)
    p.block(-0.8, -0.02, -MOUTH - 0.6, MOUTH + 0.6, FRIEZE[1], FRIEZE[1] + 0.3, "MI_Hub_Ochre", nseed(), axis="y")
    letters(p, HUB["name"].upper(), FRIEZE[0] + 0.22, FRIEZE[1] - 0.22, -0.36)
    taffrail(p)
    for side in (1, -1):
        lantern(p, Vector((-0.6, side * (hw(0.3, 34.4) - 0.6), 37.0)), side)
    wheel(p)
    rudders(p)
    for cb in HUB["cables"]:
        if cb["id"] == "stern":
            cable(p, cb)
    HH.export(p)


LETTERS = {
    "H": [((0, 0), (0, 1)), ((1, 0), (1, 1)), ((0, 0.52), (1, 0.52))],
    "O": "ring",
    "L": [((0, 1), (0, 0)), ((0, 0), (0.9, 0))],
    "W": [((0, 1), (0.24, 0)), ((0.24, 0), (0.5, 0.62)), ((0.5, 0.62), (0.76, 0)), ((0.76, 0), (1, 1))],
    "K": [((0, 0), (0, 1)), ((0.95, 1), (0.06, 0.42)), ((0.32, 0.6), (0.97, 0))],
    "E": [((0, 0), (0, 1)), ((0, 1), (0.88, 1)), ((0, 0.52), (0.7, 0.52)), ((0, 0), (0.9, 0))],
}


def letters(p, text, z0, z1, x):
    """Her name carved in chunky brass strokes with serifs, centred on the frieze (read from astern: port to starboard)."""
    h = z1 - z0
    w, gap = h * 0.62, h * 0.32
    total = len(text) * w + (len(text) - 1) * gap
    y = -total * 0.5
    for ch in text:
        strokes = LETTERS.get(ch)
        if strokes == "ring":
            pts = [(0.5 + 0.5 * math.cos(2 * math.pi * k / 14), 0.5 + 0.5 * math.sin(2 * math.pi * k / 14)) for k in range(15)]
            strokes = list(zip(pts[:-1], pts[1:]))
        for (u0, v0), (u1, v1) in strokes or []:
            a = Vector((x, y + u0 * w, z0 + v0 * h))
            b = Vector((x, y + u1 * w, z0 + v1 * h))
            d = (b - a).normalized()
            p.beam(T.straight(a - d * 0.04, b + d * 0.04, (-X).cross(d), -X), (b - a).length + 0.08, h * 0.15, 0.09,
                   "MI_Metal_Brass", nseed(), seg=2.0, wear=0.9)
            if abs(u0 - u1) < 1e-3:                                          # serifs on the uprights
                for vv in (v0, v1):
                    c = Vector((x, y + u0 * w, z0 + vv * h))
                    p.beam(T.straight(c - Y * w * 0.17, c + Y * w * 0.17, UP, -X), w * 0.34, h * 0.08, 0.1, "MI_Metal_Brass", nseed(), seg=2.0)
        y += w + gap


def taffrail(p):
    """The taffrail across the stern's top, rising 1.0 m in the middle (outside.stern), on turned balusters, a brass crest."""
    half = hw(0.3, 34.4) - 0.25

    def zt(y):
        return 34.4 + 1.0 * max(math.cos(0.5 * math.pi * min(abs(y) / half, 1.0)), 0.0) ** 1.4

    ys = [-half + 2 * half * k / 30 for k in range(31)]
    f = T.polyline([(-0.25, y, zt(y) + 0.12) for y in ys], lambda q: X, lambda q: UP)
    p.beam(f, T.length_of(f, 60), 0.5, 0.24, "MI_Wood_Varnish", nseed(), seg=0.6, wear=1.0)
    prof = [(0.09, 0.0), (0.09, 0.06), (0.06, 0.12), (0.12, 0.4), (0.13, 0.5), (0.07, 0.7), (0.05, 0.78), (0.08, 0.82), (0.08, 0.88)]
    y = -half + 0.4
    while y < half - 0.3:
        h = zt(y) - 34.4
        if h > 0.25:
            p.lathe([(r, k * h / 0.88) for r, k in prof], Vector((-0.25, y, 34.4)), UP, "MI_Paint_Cream", sides=8, wear=0.7)
        y += 0.42
    p.lathe([(0.0, -0.08), (0.45, -0.08), (0.62, 0.0), (0.62, 0.1), (0.45, 0.18), (0.0, 0.18)], Vector((-0.35, 0.0, 36.15)), -X,
            "MI_Metal_Brass", sides=20, wear=0.8)
    p.lathe([(0.0, 0.0), (0.5, 0.0), (0.5, 0.06), (0.0, 0.06)], Vector((-0.2, 0.0, 36.15)), -X, "MI_Paint_HullDark", sides=20)


def lantern(p, base, side):
    """A great stern lantern on an iron bracket from the corner post: brass frame, six glass sides, a dome, a finial."""
    post_top = Vector((-0.25, side * (hw(0.3, 34.4) - 0.2), 36.3))
    f = T.polyline([post_top, post_top + Vector((-0.2, 0.0, 0.25)), base + Vector((0.0, 0.0, -0.15))], lambda q: Y, lambda q: X)
    p.beam(f, T.length_of(f), 0.16, 0.12, "MI_Metal_Iron", nseed(), seg=0.4)
    p.lathe([(0.0, 0.0), (0.35, 0.0), (0.55, 0.15), (0.62, 0.3), (0.0, 0.3)], base, UP, "MI_Metal_Brass", sides=6, wear=0.8)
    p.lathe([(0.56, 0.3), (0.6, 1.0), (0.64, 1.6), (0.0, 1.6)], base, UP, "MI_LanternGlass", sides=6, caps=False)
    for k in range(6):
        a = 2 * math.pi * (k + 0.0) / 6
        dirv = Vector((math.cos(a), math.sin(a), 0.0))
        p.beam(T.straight(base + dirv * 0.6 + UP * 0.3, base + dirv * 0.66 + UP * 1.6, dirv.cross(UP), dirv), 1.3, 0.07, 0.06,
               "MI_Metal_Brass", nseed(), seg=2.0)
    p.lathe([(0.7, 1.6), (0.7, 1.7), (0.55, 1.85), (0.35, 2.05), (0.15, 2.2), (0.06, 2.3), (0.12, 2.38), (0.0, 2.5)], base, UP,
            "MI_Metal_Brass", sides=12, wear=0.8)


def wheel(p):
    """The great wheel on the poop over the arch (hub.helm), lashed with a rope cross; it never turns."""
    hx, r = HUB["helm"]["x"], HUB["helm"]["r"]
    c = Vector((hx, HUB["helm"]["y"], Z["poop"] + r + 0.45))
    p.beam(T.arc(c, Y, UP, r - 0.12, 0.0, 2.0 * math.pi), 2.0 * math.pi * (r - 0.12), 0.24, 0.22, "MI_Wood_Varnish", nseed(), seg=0.3, wear=1.0)
    p.lathe([(0.0, -0.3), (0.36, -0.3), (0.42, -0.2), (0.42, 0.2), (0.36, 0.3), (0.0, 0.3)], c, X, "MI_Metal_Brass", sides=14)
    for k in range(10):
        a = 2 * math.pi * k / 10
        dirv = Y * math.cos(a) + UP * math.sin(a)
        p.beam(T.straight(c + dirv * 0.35, c + dirv * (r - 0.2), dirv.cross(X), X), r - 0.55, 0.12, 0.12, "MI_Wood_Varnish", nseed(), seg=1.0)
        p.lathe([(0.07, 0.0), (0.09, 0.08), (0.06, 0.2), (0.08, 0.38), (0.05, 0.55), (0.0, 0.6)], c + dirv * (r - 0.02), dirv,
                "MI_Wood_Varnish", sides=8)
    for dx in (-0.45, 0.45):                                                # the pedestal: two posts and a crosspiece
        base = Vector((hx + dx, 0.0, Z["poop"]))
        for dy in (-0.5, 0.5):
            p.beam(T.straight(base + Y * dy, c + X * dx + Y * dy * 0.25 - UP * 0.1, X, Y), (c - base).length, 0.24, 0.22, "MI_Wood_Varnish", nseed())
    p.round(T.straight(c - X * 0.7, c + X * 0.7, Y, UP), 1.4, 0.13, "MI_Metal_Iron", nseed(), sides=10)
    for sgn in (-1.0, 1.0):                                                 # the lashing: two ropes crossed over it
        a = Vector((hx - 0.3, sgn * 2.2, Z["poop"] + 0.05))
        b = c - X * 0.25 + (Y * -sgn + UP).normalized() * (r - 0.05)
        e = Vector((hx - 0.3, -sgn * 2.2, Z["poop"] + 0.05))
        for q0, q1 in ((a, b), (b, b + (e - b) * 0.0 + UP * 0.0)):
            pass
        f = T.polyline([a, c - X * 0.32 + (Y * sgn - UP).normalized() * 0.4, b], lambda q: UP, lambda q: -X)
        p.round(f, T.length_of(f), 0.05, "MI_Rope_Hemp", nseed(), sides=6, seg=0.6, wear=0.0)
        for ry in (-1.0, 1.0):                                              # the ring bolts in the deck
            pass
    for sgn in (-1.0, 1.0):
        p.lathe([(0.0, 0.0), (0.12, 0.0), (0.12, 0.05), (0.0, 0.05)], Vector((hx - 0.3, sgn * 2.2, Z["poop"])), UP, "MI_Metal_Iron", sides=8)


def rudders(p):
    """Twin rudders on the wings' sternposts (hub.rudders): oak timbers bolted together, iron pintles and straps, the stock
    up into a hood on the transom."""
    ru = HUB["rudders"]
    n = 4
    for side in (1, -1):
        y = side * ru["y"]

        def chord(z):
            return ru["x1"] - ru["x0"] + (ru["head"] - (ru["x1"] - ru["x0"])) * (z - ru["z0"]) / (ru["z1"] - ru["z0"])
        for k in range(n):
            def fr(s, k=k):
                z = ru["z0"] + (ru["z1"] - ru["z0"]) * s
                c = chord(z)
                x = ru["x1"] - c * (k + 0.5) / n - 0.02
                return Vector((x, y, z)), X, Vector((0.0, side, 0.0)), (c / n) / 0.8
            p.beam(fr, ru["z1"] - ru["z0"], 0.8, 0.6 - 0.08 * k, "MI_Hub_HullTarDark" if k else "MI_Hub_HullTar", nseed(), seg=0.8, wear=0.9,
                   grime=lambda q: min(max((1.2 - q.z) / 1.5, 0.0), 1.0))
        for zs in (-4.5, -2.0, 0.5, 3.0, 5.3):
            c = chord(zs)
            for sgn in (-1.0, 1.0):
                a = Vector((0.0, y + sgn * 0.33, zs))
                b = Vector((-c * 0.85, y + sgn * 0.33, zs))
                p.beam(T.straight(a, b, UP, Vector((0.0, sgn, 0.0))), (b - a).length, 0.2, 0.05, "MI_Metal_Iron", nseed(), seg=1.0)
        p.block(-ru["head"] * 0.7, 0.0, y - 0.35, y + 0.35, ru["z1"] - 0.1, ru["z1"] + 2.2, "MI_Hub_HullTarDark", nseed(), axis="z")
        p.block(-1.3, 0.0, y - 0.7, y + 0.7, ru["z1"] + 1.6, ru["z1"] + 2.8, "MI_Hub_HullTar", nseed(), axis="y")    # the hood


def cable(p, cb, r=0.2):
    """A mooring cable from a fairlead in the hull down into the water, sagging a little, and on under the surface."""
    a, b = Vector(cb["from"]), Vector(cb["to"])
    side = 1.0 if a.y > 0 else -1.0
    x = a.x
    a = Vector((x, side * (hw(x, a.z) + PT + 0.1), a.z))
    d = (b - a)
    pts = [a + d * (k / 12) - UP * (0.6 * math.sin(math.pi * k / 12)) for k in range(13)]
    pts.append(b + d.normalized() * 4.0)
    f = T.polyline(pts, lambda q: UP.cross(d.normalized()).normalized(), lambda q: UP)
    p.round(f, T.length_of(f, 60), r, "MI_Rope_Hemp", nseed(), sides=8, seg=1.2, wear=0.0, grime=lambda q: min(max((0.6 - q.z) / 0.8, 0.0), 1.0))
    p.lathe([(r + 0.12, -0.2), (r + 0.28, -0.12), (r + 0.3, 0.0), (r + 0.28, 0.12), (r + 0.12, 0.2), (r + 0.06, 0.2), (r + 0.06, -0.2)],
            a - d.normalized() * 0.15, d.normalized(), "MI_Metal_Iron", sides=12, caps=False)


# --- the quarter galleries ----------------------------------------------------------------------------------------------
GAL = dict(x0=3.0, x1=13.0, out=2.4, z0=21.5, z1=32.0, inset=1.6)


def gal_pt(x, d, z, side):
    return Vector((x, side * (hw(x, z) + PT + d), z))


def build_galleries():
    p = T.Part("SM_Hub_Galleries")
    g = GAL
    for side in (1, -1):
        outline = [(g["x0"], 0.0), (g["x0"] + g["inset"], g["out"]), (g["x1"] - g["inset"], g["out"]), (g["x1"], 0.0)]
        faces = list(zip(outline[:-1], outline[1:]))
        tiers = [(22.1, 24.9), (25.7, 28.5)]

        def face_frame(fa, fb, z, side=side):
            pa, pb = gal_pt(fa[0], fa[1], z, side), gal_pt(fb[0], fb[1], z, side)
            t = (pb - pa).normalized()
            n = t.cross(UP) if side > 0 else UP.cross(t)
            if n.y * side < 0:
                n = -n
            return pa, pb, t, n
        # The floor and its edge beam, the brackets under the ends, the brass drop.
        floor_pts = [gal_pt(x, d, g["z0"], side) for x, d in outline]
        p.polygon(floor_pts, "MI_Hub_HullTarDark", tone=0.3, facing=-UP)
        p.polygon([q + UP * 0.3 for q in floor_pts], "MI_Hub_HullTarDark", tone=0.3, facing=UP)
        for fa, fb in faces:
            pa, pb, t, n = face_frame(fa, fb, g["z0"])
            p.beam(T.straight(pa + n * 0.05, pb + n * 0.05, UP, n), (pb - pa).length + 0.3, 0.5, 0.28, "MI_Hub_Ochre", nseed(), seg=1.0)
        for x in (g["x0"] + 0.9, g["x1"] - 0.9):
            a, b = gal_pt(x, 0.05, g["z0"] - 2.4, side), gal_pt(x, 1.7, g["z0"] - 0.1, side)
            p.beam(T.straight(a, b, X, (b - a).normalized().cross(X)), (b - a).length, 0.36, 0.32, "MI_Hub_HullTarDark", nseed())
        drop = gal_pt(0.5 * (g["x0"] + g["x1"]), 1.25, g["z0"] - 0.05, side)
        p.lathe([(0.0, 0.0), (0.95, 0.0), (1.05, 0.25), (0.8, 0.6), (0.9, 1.0), (0.55, 1.6), (0.62, 2.0), (0.3, 2.6),
                 (0.36, 2.9), (0.12, 3.3), (0.0, 3.6)], drop, -UP, "MI_Metal_Brass", sides=12, wear=1.0)
        # Posts at the corners, the tiers' windows between oxblood panels, mouldings, the cornice.
        for (x, d) in outline[1:-1]:
            a, b = gal_pt(x, d, g["z0"] + 0.3, side), gal_pt(x, d, 29.4, side)
            p.beam(T.straight(a, b, X, Vector((0.0, side, 0.0))), (b - a).length, 0.34, 0.34, "MI_Paint_Hull", nseed(), seg=1.0)
        for fa, fb in faces:
            # Each face a plane leaning with the hull: oxblood boards between the windows, ochre and cream mouldings.
            zb = g["z0"] + 0.3
            P0, P1 = gal_pt(fa[0], fa[1], zb, side), gal_pt(fb[0], fb[1], zb, side)
            t = (P1 - P0).normalized()
            L = (P1 - P0).length
            lean = gal_pt(fa[0], fa[1], 29.5, side) - P0
            Hf = lean.length
            a = lean.normalized()
            n = t.cross(a)
            if n.y * side < 0.0:
                n = -n
            F = (P0, t, a, n)
            nwin = max(1, int(L / 2.3))
            wins = []
            for (z0, z1) in tiers:
                for k in range(nwin):
                    wins.append((L * (k + 0.5) / nwin, 0.5 * (z0 + z1) - zb, min(0.75, L / nwin * 0.5 - 0.3), 0.5 * (z1 - z0) - 0.25))

            def keep(u, v, wins=wins):
                return all(not (abs(u - wu) < whw + 0.12 and wv - whh - 0.15 < v < wv + whh + 0.15) for wu, wv, whw, whh in wins)
            wall_boards(p, F, 0.02, L - 0.02, 0.0, Hf - 0.05, keep,
                        mats=("MI_Paint_Hull", "MI_Paint_Hull", "MI_Paint_HullFaded", "MI_Paint_HullDark"))
            for j, (wu, wv, whw, whh) in enumerate(wins):
                window_unit(p, (HH.lpt(F, wu, wv, 0.0), t, a, n), whw, whh, 0.0, "lit" if (j + side) % 3 == 0 else "glass",
                            "MI_Paint_Cream", panes=(1, 2), hood=False)
            for zm, h, mat in ((25.3, 0.5, "MI_Hub_Ochre"), (28.9, 0.5, "MI_Paint_Cream"), (29.35, 0.4, "MI_Hub_Ochre")):
                v = zm - zb
                pa, pb = HH.lpt(F, -0.2, v, 0.15), HH.lpt(F, L + 0.2, v, 0.15)
                p.beam(T.straight(pa, pb, a, n), L + 0.4, h, 0.3, mat, nseed(), seg=1.0)
        # The roof leaning back to the hull, boarded, and the small dome on top with its finial.
        rows = 6
        for k in range(rows):
            t0, t1 = k / rows, (k + 1) / rows
            za, zb = 29.6 + 2.2 * t0, 29.6 + 2.2 * t1
            da, db = g["out"] * (1 - t0) + 0.2, g["out"] * (1 - t1) + 0.2
            xa0 = g["x0"] + g["inset"] * (1 - t0) * 0.6
            xa1 = g["x1"] - g["inset"] * (1 - t0) * 0.6
            pa, pb = gal_pt(xa0, 0.5 * (da + db), 0.5 * (za + zb), side), gal_pt(xa1, 0.5 * (da + db), 0.5 * (za + zb), side)
            acr = (gal_pt(0.5 * (xa0 + xa1), db, zb, side) - gal_pt(0.5 * (xa0 + xa1), da, za, side))
            w = acr.length
            acr.normalize()
            n = (pb - pa).normalized().cross(acr)
            if n.z < 0:
                n = -n
            p.board(T.straight(pa, pb, acr, n), (pb - pa).length, w + 0.04, 0.1, "MI_Hub_HullTarDark", nseed(), seg=1.0, wear=0.9)
        dome = gal_pt(0.5 * (g["x0"] + g["x1"]), 1.35, 29.8, side)
        p.lathe([(0.0, 0.0), (1.15, 0.0), (1.15, 0.35), (1.05, 0.45), (1.0, 0.9), (0.85, 1.35), (0.6, 1.75), (0.3, 2.0), (0.0, 2.08)],
                dome, UP, "MI_Metal_Brass", sides=16, wear=0.9)
        p.lathe([(0.0, 0.0), (0.12, 0.0), (0.18, 0.2), (0.1, 0.4), (0.2, 0.6), (0.0, 0.85)], dome + UP * 2.05, UP, "MI_Metal_Brass", sides=8)
    HH.export(p)


# --- the castles: decks, rails, fronts, the courtyard walls, the arch's vault ------------------------------------------------
def deck(part, z, region, x0, x1, y0, y1):
    """Deck boards fore and aft on the castles' tops: region(x, y) says where the deck is; a dark core under the seams."""
    rng = random.Random(nseed())
    y = y0 + DECK_W * 0.5
    while y < y1:
        xs = intervals(lambda x: region(x, y - DECK_W * 0.45) and region(x, y + DECK_W * 0.45), x0, x1, 0.1)
        for a, b in xs:
            part.polygon([(a, y - DECK_W * 0.5, z - DECK_T), (b, y - DECK_W * 0.5, z - DECK_T), (b, y + DECK_W * 0.5, z - DECK_T),
                          (a, y + DECK_W * 0.5, z - DECK_T)], "MI_Caulk_Dark", tone=0.2, facing=UP)
            xx = a + rng.uniform(0.0, 3.0)
            ends = [a]
            while xx < b - 1.0:
                ends.append(xx)
                xx += rng.uniform(3.5, 7.0)
            ends.append(b)
            for xa, xb in zip(ends[:-1], ends[1:]):
                if xb - xa < 0.2:
                    continue
                pa, pb = Vector((xa + 0.005, y, z - DECK_T * 0.5)), Vector((xb - 0.005, y, z - DECK_T * 0.5))
                part.board(T.straight(pa, pb, Y, UP), xb - xa - 0.01, DECK_W - 0.02, DECK_T, rng.choice(TEAK), nseed(), seg=0.7,
                           wear=rng.uniform(0.5, 1.0), bend=0.006, twist=0.4)
        y += DECK_W


def wall_boards(part, F, u0, u1, v0, v1, keep=None, mats=("MI_Hub_HullTar", "MI_Hub_HullTarFaded", "MI_Hub_HullTarDark")):
    """A boarded wall on plane F (boards along t, rows up a), skipping where keep(u, v) is False (openings)."""
    v = v0
    while v < v1 - 0.05:
        vt = min(v + ROW, v1)
        rngs = intervals(lambda u: keep is None or (keep(u, v + 0.03) and keep(u, vt - 0.03)), u0, u1, 0.05)
        board_runs(part, (HH.lpt(F, 0.0, 0.5 * (v + vt), 0.0), F[1], F[2], F[3]), rngs, -(vt - v) * 0.5, (vt - v) * 0.5, 0.0, mats,
                   lmin=2.5, lmax=6.0)
        v = vt


def build_castles():
    p = T.Part("SM_Hub_Castles")
    zp, zq = Z["poop"], Z["sc2"]
    # The poop: over the arch (x 0..10) and the wings to its front; the quarterdeck over the wings forward of it.
    deck(p, zp, lambda x, y: abs(y) < hw(x, zp) - 0.3 and (x < ARCH["x1"] or abs(y) > MOUTH + 0.05), 0.35, POOP_X + 0.05, -16.0, 16.0)
    deck(p, zq, lambda x, y: MOUTH + 0.05 < abs(y) < hw(x, zq) - 0.3, POOP_X + 0.45, FRONT_X - 0.05, -20.0, 20.0)
    fc_end = HH.end_x(Z["fcTop"]) - 0.7
    deck(p, Z["fcTop"], lambda x, y: abs(y) < hw(x, Z["fcTop"]) - 0.3, HUB["fcX"] + 0.85, fc_end, -20.0, 20.0)
    # Balustrades round the courtyard (the dock open to the sky between the wings) and across the castles' fronts.
    for side in (1, -1):
        balustrade(p, (ARCH["x1"], side * MOUTH), (POOP_X + 0.05, side * MOUTH), zp)
        balustrade(p, (POOP_X + 0.45, side * MOUTH), (FRONT_X - 0.05, side * MOUTH), zq)
        balustrade(p, (POOP_X + 0.15, side * MOUTH), (POOP_X + 0.15, side * (hw(POOP_X, zp) - 0.3)), zp)
        balustrade(p, (FRONT_X - 0.15, side * MOUTH), (FRONT_X - 0.15, side * (hw(FRONT_X, zq) - 0.3)), zq)
    balustrade(p, (ARCH["x1"] - 0.15, -MOUTH), (ARCH["x1"] - 0.15, MOUTH), zp)
    balustrade(p, (HUB["fcX"] + 0.75, -hw(HUB["fcX"], Z["fcTop"]) + 0.3), (HUB["fcX"] + 0.75, hw(HUB["fcX"], Z["fcTop"]) - 0.3), Z["fcTop"])
    # The poop's front (x POOP_X, over the quarterdeck) and the quarterdeck's front (x FRONT_X, over the main deck), boarded
    # with windows; the gates (doors and ladders) are the blockout's for now.
    gates = [(g["x"], g["y"]) for g in H.GATE_MARKS if g["level"] == "main"]
    for xf, zb, zt in ((POOP_X + 0.3, zq, zp), (FRONT_X, Z["main"], zq)):
        for side in (1, -1):
            F = (Vector((xf, 0.0, 0.0)), Y * side, UP, X)
            wins = []
            for wy in (MOUTH + 2.0, hw(xf, zt) - 2.2):
                if all(abs(gy * side - wy) > 1.6 or abs(gx - xf) > 2.0 for gx, gy in gates):
                    wins.append((wy, zb + 0.9 if zt - zb < 6 else zb + 1.2))

            def keep(u, v, wins=wins):
                for wy, wz in wins:
                    if abs(u - wy) < 0.85 and wz - 0.25 < v < wz + 2.25:
                        return False
                return True
            wall_boards(p, F, MOUTH + 0.05, hw(xf, zt) - 0.35, zb, zt - 0.02, keep)
            for wy, wz in wins:
                Fw = (Vector((xf, side * wy, wz + 1.0)), Y * side, UP, X)
                window_unit(p, Fw, 0.65, 1.0, 0.0, random.choice(("glass", "lit", "shutter")), "MI_Paint_Cream")
    # The forecastle's aft face over the main deck, its doors and ladders the blockout's gates.
    xf = HUB["fcX"] + 0.6
    F = (Vector((xf, 0.0, 0.0)), -Y, UP, -X)
    fwins = [(4.0, 22.3), (-4.0, 22.3), (12.0, 22.3), (-12.0, 22.3)]

    def fkeep(u, v):
        return all(not (abs(-u - wy) < 0.85 and wz - 0.25 < v < wz + 2.25) for wy, wz in fwins)
    wall_boards(p, F, -hw(xf, Z["fcTop"]) + 0.35, hw(xf, Z["fcTop"]) - 0.35, Z["main"], Z["fcTop"] - 0.02, fkeep)
    for wy, wz in fwins:
        window_unit(p, (Vector((xf, wy, wz + 1.0)), -Y, UP, -X), 0.65, 1.0, 0.0, random.choice(("glass", "shutter")), "MI_Paint_Cream")
    # The courtyard walls: the wings' inner faces over the dock, rows of shuttered windows (L2: wing quarters).
    for side in (1, -1):
        F = (Vector((0.0, side * MOUTH, 0.0)), X, UP, Vector((0.0, -side, 0.0)))
        wins = []
        for z0 in (Z["lower"] + 1.0, Z["middle"] + 1.0, Z["upper"] + 1.0, Z["main"] + 1.0, Z["sc1"] + 1.0, Z["sc2"] + 1.0, Z["sc3"] + 0.9):
            for x in (14.0, 18.0, 26.0, 30.0, 34.0, 38.5):
                if z0 + 2.0 < H.profile_top(x) - 1.2:
                    wins.append((x, z0))

        def wkeep(u, v, wins=wins):
            if v > H.profile_top(u) - 0.02 or (ARCH["x1"] > u):
                return False
            return all(not (abs(u - wx) < 0.85 and wz - 0.25 < v < wz + 2.25) for wx, wz in wins)
        wall_boards(p, F, ARCH["x1"] + 0.05, FRONT_X - 0.35, Z["lower"], zp + 0.88, wkeep)
        rng = random.Random(70 + side)
        for wx, wz in wins:
            window_unit(p, (Vector((wx, side * MOUTH, wz + 1.0)), X, UP, Vector((0.0, -side, 0.0))), 0.65, 1.0, 0.0,
                        "shutter" if rng.random() < 0.55 else rng.choice(("glass", "lit")), "MI_Paint_Cream")
        # A post at the tier's step, and the cap rails along the walls' tops.
        p.block(POOP_X, POOP_X + 0.6, side * MOUTH - 0.35, side * MOUTH + 0.35, Z["lower"], zp + 0.9, "MI_Hub_HullTarDark", nseed(), axis="z")
    # The arch: its vault over the mouth (boards fore and aft, three ribs) and its forward face over the courtyard.
    zc, r = arch_centre()
    y = -MOUTH
    while y < MOUTH - 1e-6:
        yb = min(y + ROW, MOUTH)
        ym = 0.5 * (y + yb)
        nrm = Vector((0.0, -ym, zc - arch_z(ym))).normalized()           # towards the arch's centre: down into the mouth
        tng = Vector((0.0, 1.0, (arch_z(yb) - arch_z(y)) / max(yb - y, 1e-6))).normalized()
        pa, pb = Vector((0.4, ym, arch_z(ym) + 0.06)), Vector((ARCH["x1"] - 0.05, ym, arch_z(ym) + 0.06))
        p.board(T.straight(pa, pb, tng, nrm), pb.x - pa.x, (Vector((0, yb, arch_z(yb))) - Vector((0, y, arch_z(y)))).length - 0.02, 0.12,
                random.choice(("MI_Hub_HullTar", "MI_Hub_HullTarFaded")), nseed(), seg=1.0, wear=0.8)
        y = yb
    for xr in (2.5, 6.0, ARCH["x1"] - 0.3):
        ys = [-MOUTH + 2 * MOUTH * k / 30 for k in range(31)]
        f = T.polyline([(xr, yy, arch_z(yy) - 0.18) for yy in ys], lambda q: X, lambda q: Vector((0.0, -q.y, zc - q.z)).normalized())
        p.beam(f, T.length_of(f, 60), 0.4, 0.36, "MI_Hub_HullTarDark", nseed(), seg=0.6)
    F = (Vector((ARCH["x1"], 0.0, 0.0)), -Y, UP, X)
    awins = [(-5.0, 27.0), (0.0, 27.0), (5.0, 27.0)]

    def akeep(u, v):
        if v < arch_z(-u) + 0.3:
            return False
        return all(not (abs(-u - wy) < 0.85 and wz - 0.25 < v < wz + 2.25) for wy, wz in awins)
    wall_boards(p, F, -MOUTH, MOUTH, Z["sc1"], zp - 0.02, akeep)
    for wy, wz in awins:
        window_unit(p, (Vector((ARCH["x1"], wy, wz + 1.0)), -Y, UP, X), 0.65, 1.0, 0.0, "lit" if wy == 0 else "glass", "MI_Paint_Cream")
    HH.export(p)


# --- the head ----------------------------------------------------------------------------------------------------------
def build_head():
    p = T.Part("SM_Hub_Head")
    bh = HUB["beakhead"]
    # The stem: a great curved timber up the bow from the ancient foot to above the forecastle's rail.
    zs = [HH.stone_top(160.0) - 0.3 + k * 1.0 for k in range(26)]
    zs = [z for z in zs if z < Z["fcTop"] + 1.6] + [Z["fcTop"] + 1.8]
    pts = [Vector((H.stem_x(min(z, 26.3)) + 0.2, 0.0, z)) for z in zs]
    f = T.polyline(pts, lambda q: Y, lambda q: X)
    p.beam(f, T.length_of(f, 60), 1.0, 0.7, "MI_Hub_HullTarDark", nseed(), seg=0.6, wear=1.0)
    # The knee of the head: from the stem sweeping out and up to the figurehead, trailboards on its sides.
    s0 = Vector((H.stem_x(8.0) + 0.3, 0.0, 8.0))
    fig = Vector(bh["figure"])
    ctrl = Vector((s0.x + 3.0, 0.0, 15.5))
    knee = [s0 * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + fig * t * t for t in (k / 16 for k in range(17))]
    f = T.polyline(knee, lambda q: Y, lambda q: Vector((1.0, 0.0, -0.4)).normalized())
    p.beam(f, T.length_of(f, 60), 0.9, 1.1, "MI_Hub_HullTarDark", nseed(), seg=0.5, wear=1.0)
    for side in (1, -1):
        tb = [q + Vector((0.0, side * 0.47, 0.0)) for q in knee[3:15]]
        f = T.polyline(tb, lambda q, s=side: UP.cross(X) * 0 + Vector((0.0, 0.0, 1.0)), lambda q, s=side: Vector((0.0, s, 0.0)))
        p.beam(f, T.length_of(f, 40), 0.55, 0.08, "MI_Hub_Ochre", nseed(), seg=0.5, wear=1.0)
    # The beakhead's platform: two head timbers, cross beams and gratings, out from the stem to the figure.
    x0 = H.stem_x(bh["z"]) - 0.2
    zt = bh["z"]

    def half(x):
        return bh["halfW"] * (1.0 - 0.65 * (x - x0) / (bh["x1"] - x0))
    for side in (1, -1):
        f = T.polyline([(x, side * half(x), zt - 0.2) for x in (x0, 0.5 * (x0 + bh["x1"]), bh["x1"])], lambda q: UP, lambda q, s=side: Vector((0, s, 0)))
        p.beam(f, bh["x1"] - x0, 0.45, 0.3, "MI_Hub_HullTarDark", nseed(), seg=1.0)
    x = x0 + 0.4
    while x < bh["x1"] - 0.3:
        w = half(x) - 0.1
        p.beam(T.straight((x, -w, zt - 0.05), (x, w, zt - 0.05), X, UP), 2 * w, 0.12, 0.1, "MI_Wood_TeakDark", nseed(), seg=1.0, wear=0.8)
        x += 0.35
    for yy in (-1.2, -0.6, 0.0, 0.6, 1.2):
        xe = x0 + (bh["x1"] - x0) * min(1.0, (1.0 - abs(yy) / bh["halfW"]) / 0.65) - 0.3
        if xe > x0 + 0.5:
            p.beam(T.straight((x0 + 0.1, yy, zt - 0.14), (xe, yy, zt - 0.14), Y, UP), xe - x0, 0.1, 0.12, "MI_Wood_TeakDark", nseed(), seg=1.0)
    # Headrails: two curved rails a side from the figure's back up to the forecastle's bow, on carved head timbers.
    rail_top = bh["rails_to"]
    for side in (1, -1):
        for k, (zh, zfc) in enumerate(((zt + 0.9, rail_top - 0.4), (zt + 1.9, rail_top + 0.4))):
            a = Vector((bh["x1"] - 0.6, side * 0.5, zh))
            c = Vector((x0 - 1.5, side * (hw(x0 - 1.5, zfc) + 0.25), zfc))
            mid = Vector((0.5 * (a.x + c.x), side * (bh["halfW"] + 0.3), zh + 0.4 + k * 0.3))
            rail = [a * (1 - t) ** 2 + mid * 2 * t * (1 - t) + c * t * t for t in (j / 14 for j in range(15))]
            f = T.polyline(rail, lambda q: UP, lambda q, s=side: Vector((0.0, s, 0.0)))
            p.beam(f, T.length_of(f, 40), 0.3, 0.26, "MI_Paint_Hull" if k else "MI_Hub_Ochre", nseed(), seg=0.5, wear=1.0)
        for t in (0.25, 0.5, 0.75):
            xx = x0 + (bh["x1"] - x0) * t
            a = Vector((xx, side * (half(xx) - 0.05), zt))
            b = Vector((xx - 0.5, side * (half(xx) + 0.25), zt + 2.0))
            p.beam(T.straight(a, b, X, Vector((0.0, side, 0.0))), (b - a).length, 0.22, 0.2, "MI_Hub_HullTarDark", nseed())
    # The figurehead: a carved scroll in ochre and brass at the knee's tip, a crown on it.
    c = fig + Vector((0.3, 0.0, 0.2))
    pts = []
    for k in range(40):
        a = 2.4 * math.pi * k / 39
        r = 1.05 * (1.0 - 0.75 * k / 39)
        pts.append(c + Vector((math.cos(a + 0.4) * r, 0.0, math.sin(a + 0.4) * r)) + Vector((0.0, 0.0, 0.9)))
    f = T.polyline(pts, lambda q: Y, lambda q: (q - (c + Vector((0, 0, 0.9)))).normalized() if (q - c).length > 0.05 else X)
    p.beam(f, T.length_of(f, 80), 0.75, 0.32, "MI_Hub_Ochre", nseed(), seg=0.2, wear=1.0)
    p.lathe([(0.0, 0.0), (0.42, 0.0), (0.5, 0.18), (0.36, 0.3), (0.44, 0.5), (0.2, 0.6), (0.0, 0.72)], c + Vector((0.1, 0.0, 2.05)), UP,
            "MI_Metal_Brass", sides=10, wear=1.0)
    # The bowsprit (outside.bow), hewn, with iron bands; the bobstay down to the stem.
    sp = H.prop("main_deck", "bowsprit")
    a, b = Vector(sp["from"]), Vector(sp["to"])
    d = (b - a).normalized()
    p.round(T.straight(a, b, Y, d.cross(Y)), (b - a).length, lambda s: 0.9 + (0.4 - 0.9) * s, "MI_Wood_Varnish", nseed(), sides=12, seg=1.0, wear=0.6)
    for s in (0.25, 0.35, 0.55, 0.8):
        q = a + (b - a) * s
        r = 0.9 + (0.4 - 0.9) * s + 0.04
        p.lathe([(r, -0.1), (r + 0.03, 0.0), (r, 0.1)], q, d, "MI_Metal_Iron", sides=14, caps=False)
    bs = H.prop("main_deck", "bobstay")
    a, b = Vector(bs["from"]), Vector(bs["to"])
    p.round(T.straight(a, b, Y, (b - a).normalized().cross(Y)), (b - a).length, 0.12, "MI_Rope_Hemp", nseed(), sides=8, seg=2.0, wear=0.0)
    # Catheads out from the forecastle's bow, and the hawse holes with the bow cables to the anchors ahead.
    for side in (1, -1):
        xc = 150.0
        q = Vector((xc, side * (hw(xc, 24.6) - 0.2), 24.6))
        out = Vector((0.35, side, 0.0)).normalized()
        p.beam(T.straight(q, q + out * 3.0, UP, out.cross(UP) * side), 3.0, 0.6, 0.6, "MI_Hub_HullTarDark", nseed(), seg=1.0)
        p.lathe([(0.0, 0.0), (0.36, 0.0), (0.36, 0.08), (0.0, 0.08)], q + out * 3.0, out, "MI_Metal_Brass", sides=12)
    for cb in HUB["cables"]:
        if cb["id"] != "bow":
            continue
        a0, b0 = Vector(cb["from"]), Vector(cb["to"])
        side = 1.0 if a0.y > 0 else -1.0
        hole = Vector((a0.x, side * (hw(a0.x, a0.z) + PT), a0.z))
        n = HH.hull_frame(a0.x, a0.z, side)[3]
        p.lathe([(0.75, -0.1), (0.8, 0.08), (0.62, 0.22), (0.45, 0.22), (0.45, -0.1)], hole - n * 0.12, n, "MI_Metal_Iron", sides=16, caps=False)
        p.polygon([hole + (n.cross(UP).normalized() * math.cos(2 * math.pi * k / 12) + UP * math.sin(2 * math.pi * k / 12)) * 0.46 - n * 0.05
                   for k in range(12)], "MI_Hub_Void", tone=0.1, facing=n)
        cable(p, dict(id="bow", **{"from": [a0.x, a0.y, a0.z], "to": cb["to"]}), r=0.24)
    HH.export(p)


BUILDERS = {"stern": build_stern, "galleries": build_galleries, "castles": build_castles, "head": build_head}

if __name__ == "__main__":
    random.seed(62)
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBENDS_OK", wanted)
