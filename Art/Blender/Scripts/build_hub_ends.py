"""The hub ship's ends and castles from outside (D62, reworked for D64): a galleon's stern, its quarter galleries, the castles'
decks, rails and fronts, and the head. Hand-made like the boat; every number from HubShip_Data.json through hub_layout.py
(outside.stern, outside.bow, hub.form, hub.galleries, hub.rudder, hub.helm, hub.cables, hub.beakhead). Visual only.

The stern (D64, owner: "rounder, wider and bigger at the back top like a real galleon"):
  - Below the counter (z 8.5) the hull narrows to the sternpost: a flat tuck at the post (x 1.6), planked across, its edges in
    fashion pieces, the one rudder hung on the post, the deadwood and the keel's sole under it.
  - The counter: a heavy wing transom across the tuck's head, the soffit under the stern's overhang, a moulded counter rail.
  - Above it the stern face is round in plan and leans aft to the taffrail: planked in rows across, a moulded band at every
    deck, rows of tall cabin windows on every storey, open galleries at sc1 and sc2 (balustraded, on brackets, the upper one
    roofed) running round the corners into the quarter galleries, her name in brass under the taffrail, three great lanterns.

Meshes (pivot at the ship's x 80 on the waterline; /Game/Saltbound/Hub/Art; Nanite):
  SM_Hub_Stern      the tuck, sternpost, deadwood, counter, the stern face with its windows, bands and galleries, the frieze
                    with her name, the taffrail and its lanterns, the rudder, the stern moorings
  SM_Hub_Galleries  the two quarter galleries (three storeys of windows, the brass drop, the roof and dome)
  SM_Hub_Castles    the poop, quarterdeck and forecastle decks, their bulwarks' inner sides, balustrades, the castles' fronts,
                    the lashed great wheel
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
ROW = 0.5                       # stern face and wall board width
DECK_W, DECK_T = 0.32, 0.1      # deck boards on the castles
POOP_X, QD_X, FC_X = HUB["poopX"], HUB["qdX"], HUB["fcX"]
COUNTER, POST_X, ROUND = HH.COUNTER, HH.POST_X, HH.ROUND
GAL = HUB["galleries"]
QG = GAL["quarter"]
TEAK = ("MI_Wood_Teak", "MI_Wood_Teak", "MI_Wood_TeakDark", "MI_Wood_TeakLight")
TAR = ("MI_Hub_HullTar", "MI_Hub_HullTar", "MI_Hub_HullTarFaded", "MI_Hub_HullTarDark")
STERN_TOP = H.profile_top(POST_X) - 0.05      # the stern face's top (the taffrail's), level across
FRIEZE = (Z["poop"] + 1.05, Z["poop"] + 2.05)  # the name's panel under the taffrail


def hw(x, z):
    return HH.hw(x, z)


def plane(p, t, a, n):
    return (Vector(p), Vector(t).normalized(), Vector(a).normalized(), Vector(n).normalized())


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


def wall_boards(part, F, u0, u1, v0, v1, keep=None, mats=("MI_Hub_HullTar", "MI_Hub_HullTarFaded", "MI_Hub_HullTarDark")):
    """A boarded wall on plane F (boards along t, rows up a), skipping where keep(u, v) is False (openings)."""
    v = v0
    while v < v1 - 0.05:
        vt = min(v + ROW, v1)
        rngs = intervals(lambda u: keep is None or (keep(u, v + 0.03) and keep(u, vt - 0.03)), u0, u1, 0.05)
        board_runs(part, (HH.lpt(F, 0.0, 0.5 * (v + vt), 0.0), F[1], F[2], F[3]), rngs, -(vt - v) * 0.5, (vt - v) * 0.5, 0.0, mats,
                   lmin=2.5, lmax=6.0)
        v = vt


def window_unit(part, F, hw_, hh, d, kind, frame_mat, pointed=0.0, panes=(1, 2), hood=True):
    """A window on plane F centred at its origin: frame, mullions and transoms, glass (dark or lit), or oxblood shutters."""
    glass = "MI_LanternGlass" if kind in ("lit", "glow") else "MI_Glass_Dark"
    p, t, a, n = F
    outline = [(-hw_, -hh), (hw_, -hh), (hw_, hh - pointed)]
    if pointed > 0.0:
        for k in range(1, 8):
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


def baluster_row(part, pts, z_of, height=0.9, posts=2.4, mat="MI_Wood_Varnish", baluster="MI_Paint_Cream", bottom=True):
    """A galleon's balustrade along a polyline of plan points (ship m) on a floor at z_of(p): posts, a bottom rail, turned
    balusters, a cap rail."""
    pts = [Vector((p[0], p[1], 0.0)) for p in pts]
    pts = [Vector((p.x, p.y, z_of(p))) for p in pts]
    seglen = [(b - a).length for a, b in zip(pts[:-1], pts[1:])]
    L = sum(seglen)
    if L < 0.3:
        return

    def at(s):
        d = s * L
        for a, b, ln in zip(pts[:-1], pts[1:], seglen):
            if d <= ln or b is pts[-1]:
                return a + (b - a) * min(d / max(ln, 1e-9), 1.0), (b - a).normalized()
            d -= ln
        return pts[-1], (pts[-1] - pts[-2]).normalized()
    for zz, w, h in ([(0.1, 0.24, 0.16)] if bottom else []) + [(height - 0.08, 0.34, 0.16)]:
        rail = [q + UP * zz for q in pts]
        f = T.polyline(rail, lambda q, rr=rail: _across_h(rr, q), lambda q: UP)
        part.beam(f, T.length_of(f, 40), w, h, mat, nseed(), seg=0.8)
    n_post = max(1, int(round(L / posts)))
    for k in range(n_post + 1):
        c, d = at(k / n_post)
        part.beam(T.straight(c, c + UP * (height - 0.16), d, UP.cross(d)), height - 0.16, 0.26, 0.26, mat, nseed(), seg=1.0)
    n_bal = int(L / 0.36)
    prof = [(0.07, 0.0), (0.07, 0.06), (0.05, 0.1), (0.09, 0.28), (0.1, 0.36), (0.06, 0.5), (0.045, 0.56), (0.06, 0.58), (0.06, 0.62)]
    h = height - 0.34
    for k in range(1, n_bal):
        s = k / n_bal
        if any(abs(s * L - L * j / n_post) < 0.2 for j in range(n_post + 1)):
            continue
        c, _ = at(s)
        part.lathe([(r, hh * h / 0.62) for r, hh in prof], c + UP * 0.18, UP, baluster, sides=8, wear=0.7)


def _across_h(rail, q):
    """The horizontal across direction of a polyline at the point nearest q."""
    best, d = None, 1e9
    for a, b in zip(rail[:-1], rail[1:]):
        m = (a + b) * 0.5
        if (m - q).length < d:
            d, best = (m - q).length, (b - a)
    best = Vector((best.x, best.y, 0.0)).normalized()
    return UP.cross(best)


def balustrade(part, a, b, z, height=0.9, side_out=None, posts=2.4, mat="MI_Wood_Varnish", baluster="MI_Paint_Cream"):
    """A straight balustrade from a to b (plan points) on a deck at z."""
    baluster_row(part, [a, b], lambda p: z, height=height, posts=posts, mat=mat, baluster=baluster)


# --- the stern face (above the counter) -------------------------------------------------------------------------------------
def corner_w(z):
    """The stern face's half-width at its corners at height z."""
    return hw(ROUND - H.stern_lean(z) + 1e-3, z)


def stern_x(y, z):
    """x of the stern face at (|y|, z) above the counter (the data's form is round in plan: solve hw(x, z) = |y|)."""
    lean = H.stern_lean(z)
    xa, xb = -lean, ROUND - lean
    y = abs(y)
    if y >= hw(xb, z):
        return xb
    for _ in range(28):
        m = 0.5 * (xa + xb)
        if hw(m, z) < y:
            xa = m
        else:
            xb = m
    return 0.5 * (xa + xb)


class Face:
    """The stern face at one height: plan points from port corner to starboard corner with their arc length u (0 at the
    middle), tangents and outward normals; points off it at depth d along the normal."""

    def __init__(self, z, n=90):
        self.z = z
        w = corner_w(z)
        ys = [w * math.sin(0.5 * math.pi * (-1.0 + 2.0 * k / n)) for k in range(n + 1)]       # denser at the corners
        self.p = [Vector((stern_x(y, z), y, z)) for y in ys]
        self.u = [0.0]
        for a, b in zip(self.p[:-1], self.p[1:]):
            self.u.append(self.u[-1] + (b - a).length)
        mid = self.u[n // 2]
        self.u = [u - mid for u in self.u]
        self.w = w

    def at(self, u, d=0.0):
        """(point, tangent along +y, outward normal) at arc length u, d metres out from the face."""
        us, ps = self.u, self.p
        u = min(max(u, us[0]), us[-1])
        for k in range(len(us) - 1):
            if u <= us[k + 1] or k == len(us) - 2:
                t = (u - us[k]) / max(us[k + 1] - us[k], 1e-9)
                p = ps[k].lerp(ps[k + 1], min(max(t, 0.0), 1.0))
                tg = (ps[k + 1] - ps[k]).normalized()
                break
        n = Vector((-tg.y, tg.x, 0.0))
        if n.x > 0.0:
            n = -n
        return p + n * d, tg, n

    @property
    def half(self):
        return self.u[-1]


_FACES = {}


def face(z):
    key = round(z, 3)
    if key not in _FACES:
        _FACES[key] = Face(z)
    return _FACES[key]


def face_frame(u, z, d=0.0):
    """A local plane on the stern face at (u, z): t along it (to starboard), a up it (with the lean), n outward."""
    p, tg, n = face(z).at(u, d)
    p2, _, _ = face(z + 0.5).at(u, d)
    a = (p2 - p).normalized()
    n2 = tg.cross(a)
    if n2.dot(n) < 0.0:
        n2 = -n2
    return (p, tg, a, n2)


def face_boards(part, z0, z1, keep, mats=TAR, thick=0.13, d=0.0):
    """One row of boards across the stern face between z0 and z1, broken where keep(u) is False."""
    zm = 0.5 * (z0 + z1)
    f = face(zm)
    rngs = intervals(keep, -f.half + 0.35, f.half - 0.35, 0.05)
    rng = random.Random(nseed())
    for ua, ub in rngs:
        if ub - ua < 0.2:
            continue
        # The dark core behind the row.
        n = max(1, int((ub - ua) / 1.0))
        for k in range(n):
            u0, u1 = ua + (ub - ua) * k / n, ua + (ub - ua) * (k + 1) / n
            part.polygon([face(z0).at(u0, d)[0], face(z0).at(u1, d)[0], face(z1).at(u1, d)[0], face(z1).at(u0, d)[0]], "MI_Caulk_Dark",
                         tone=0.2, facing=face(zm).at(0.5 * (u0 + u1))[2])
        u = ua
        first = True
        while u < ub - 1e-6:
            ue = min(u + (rng.uniform(0.6, 5.5) if first else rng.uniform(2.5, 5.5)), ub)
            first = False
            if ub - ue < 0.8:
                ue = ub
            k = max(2, int((ue - u) / 0.6))
            pts = [f.at(u + 0.005 + (ue - u - 0.01) * j / k, d + thick * 0.5)[0] for j in range(k + 1)]
            fr = T.polyline(pts, lambda q, uu=0.5 * (u + ue): face_frame(uu, zm)[2], lambda q, uu=0.5 * (u + ue): face_frame(uu, zm)[3])
            part.board(fr, T.length_of(fr), (z1 - z0) - 0.025, thick, rng.choice(mats), nseed(), seg=0.7, wear=rng.uniform(0.6, 1.0),
                       bend=0.01, twist=0.4, slant=0.02)
            u = ue


def face_moulding(part, z, h, proud, mat, u0=None, u1=None, d=0.0):
    """A moulded band across the stern face at height z (its middle)."""
    f = face(z)
    u0 = -f.half if u0 is None else u0
    u1 = f.half if u1 is None else u1
    k = max(4, int((u1 - u0) / 0.8))
    pts = [f.at(u0 + (u1 - u0) * j / k, d)[0] for j in range(k + 1)]
    fr = T.polyline(pts, lambda q: face_frame(0.0, z)[2], lambda q: _face_normal_near(f, q))
    part.sweep(fr, T.length_of(fr), HH.moulding_section(h, proud), mat, nseed(), seg=0.8, wear=0.9, drift=0.02, wobble=0.005, slant=0.0)


def _face_normal_near(f, q):
    best, bd = 0, 1e9
    for k, p in enumerate(f.p):
        dd = (p.x - q.x) ** 2 + (p.y - q.y) ** 2
        if dd < bd:
            bd, best = dd, k
    return f.at(f.u[best])[2]


# --- the stern: storeys, windows, galleries ----------------------------------------------------------------------------------
GAL_OUT = float(GAL["out"])
GAL_LEVELS = [Z[k] for k in GAL["levels"]]                  # the open galleries' floors (sc1, sc2)
GAL_ROOF = Z["sc3"] - 0.05                                  # the upper gallery's roof, under the sc3 band
STOREYS = [(Z["upper"], "small"), (Z["gun"], "small"), (Z["main"], "tall"), (Z["sc1"], "door"), (Z["sc2"], "door"), (Z["sc3"], "tall")]
BANDS = [(Z["upper"], 0.36, 0.2, "MI_Wood_Tarred"), (Z["gun"], 0.4, 0.22, "MI_Hub_Ochre"), (Z["main"], 0.7, 0.28, "MI_Hub_Ochre"),
         (Z["sc3"] + 0.12, 0.42, 0.24, "MI_Hub_Ochre"), (Z["poop"], 0.6, 0.26, "MI_Hub_Ochre")]


def stern_openings():
    """Every window and door on the stern face: (u, z0, z1, half width, kind)."""
    rng = random.Random(640)
    out = []
    for zf, style in STOREYS:
        f = face(zf + 1.5)
        if style == "small":
            w, z0, z1, pitch = 0.55, zf + 1.0, zf + 2.3, 3.0
        elif style == "tall":
            w, z0, z1, pitch = 0.62, zf + 0.75, zf + 2.6, 2.6
        else:
            w, z0, z1, pitch = 0.62, zf + 0.12, zf + 2.6, 2.6
        n = int((2 * (f.half - 1.6)) / pitch)
        for k in range(n + 1):
            u = (k - n * 0.5) * pitch
            if abs(u) > f.half - 1.3:
                continue
            kind = "door" if style == "door" and k % 3 == 1 else rng.choice(("glass", "glass", "glow", "shutter") if style != "small" else ("glow", "shutter", "glass", "shutter"))
            out.append((u, z0, z1, w, kind))
    return out


def stern_face(p):
    """The planked stern face from the counter to the frieze, with its windows, bands and corner pieces."""
    opens = stern_openings()
    z = COUNTER + 0.5
    while z < FRIEZE[0] - 0.02:
        zt = min(z + ROW, FRIEZE[0])
        zm = 0.5 * (z + zt)

        def keep(u, z=z, zt=zt):
            for ou, oz0, oz1, w, kind in opens:
                if abs(u - ou) < w + 0.16 and oz0 - 0.18 < zt - 0.02 and z + 0.02 < oz1 + 0.18:
                    return False
            return True
        face_boards(p, z, zt, keep)
        z = zt
    # Windows and doors (their frames on the face's local plane).
    rng = random.Random(641)
    for u, z0, z1, w, kind in opens:
        zc = 0.5 * (z0 + z1)
        F = face_frame(u, zc)
        frame = "MI_Paint_Cream" if rng.random() < 0.7 else "MI_Paint_CreamAged"
        if kind == "door":
            window_unit(p, F, w, 0.5 * (z1 - z0), -0.12, "glass", frame, panes=(1, 4), hood=False)
        else:
            window_unit(p, F, w, 0.5 * (z1 - z0), -0.12, kind, frame, panes=(1, 3), hood=True)
    # Bands at the decks; the counter rail at its foot.
    for zb, h, proud, mat in BANDS:
        face_moulding(p, zb, h, proud, mat)
    face_moulding(p, COUNTER + 0.28, 0.56, 0.42, "MI_Wood_Tarred")
    # Pilasters between the windows of the cabin storeys: oxblood with ochre capitals, from band to band.
    for zf0, zf1 in ((Z["main"] + 0.35, Z["sc1"] - 0.05), (Z["sc3"] + 0.33, Z["poop"] - 0.3)):
        f = face(0.5 * (zf0 + zf1))
        pitch = 2.6
        n = int((2 * (f.half - 1.6)) / pitch)
        for k in range(n + 2):
            u = (k - (n + 1) * 0.5) * pitch
            if abs(u) > f.half - 0.9:
                continue
            for zz0, zz1, mat, dd, ww in ((zf0, zf1, "MI_Paint_Hull", 0.13, 0.42), (zf1 - 0.32, zf1, "MI_Hub_Ochre", 0.19, 0.6)):
                pa, _, n0 = face(zz0).at(u, PT + dd * 0.5)
                pb, _, _ = face(zz1).at(u, PT + dd * 0.5)
                tg = face(zz0).at(u)[1]
                p.beam(T.straight(pa, pb, tg, n0), (pb - pa).length, ww, dd, mat, nseed(), seg=1.0, wear=1.0)
    # The corner pieces: a heavy rounded post up each corner of the stern face, from the counter to the taffrail.
    for side in (1, -1):
        pts = []
        for k in range(24):
            zz = COUNTER + (STERN_TOP + 0.4 - COUNTER) * k / 23
            w = corner_w(zz)
            pts.append(Vector((ROUND - H.stern_lean(zz) - 0.12, side * (w + 0.05), zz)))
        f = T.polyline(pts, lambda q, s=side: Vector((0.6, s, 0.0)).normalized(), None)
        p.beam(f, T.length_of(f, 40), 0.85, 0.8, "MI_Hub_HullTarDark", nseed(), seg=0.8, wear=1.0)
        for zc in (Z["main"], Z["sc3"] + 0.12, Z["poop"]):
            w = corner_w(zc)
            c = Vector((ROUND - H.stern_lean(zc) - 0.15, side * (w + 0.05), zc))
            p.block(c.x - 0.6, c.x + 0.45, c.y - 0.6, c.y + 0.6, zc - 0.4, zc + 0.4, "MI_Hub_Ochre", nseed(), axis="z")


def gallery_line(z, d):
    """The open galleries' line at height z, d metres out: across the stern face, round its corners (an arc about each), and
    on along the side to the quarter gallery's aft face. Plan points from port to starboard."""
    f = face(z)
    mid = []
    k = 60
    for j in range(k + 1):
        u = -f.half + 2.0 * f.half * j / k
        mid.append(f.at(u, d)[0])
    out = []
    for side in (-1, 1):
        cp = f.at(side * f.half)[0]                 # the corner
        n0 = f.at(side * f.half)[2]
        a0 = math.atan2(n0.y, n0.x)
        a1 = math.atan2(side, 0.0)
        arc = []
        for j in range(1, 7):
            a = a0 + (a1 - a0) * j / 6
            arc.append(Vector((cp.x + math.cos(a) * d, cp.y + math.sin(a) * d, z)))
        xq = QG["x0"] + 0.6 * d / QG["out"]
        if xq > arc[-1].x + 0.05:
            arc.append(Vector((xq, cp.y + side * d, z)))
        out.append(arc if side > 0 else list(reversed(arc)))
    return out[0] + mid + out[1]


def stern_galleries(p):
    """Open galleries at sc1 and sc2 across the stern and round its corners: plank floors on carved brackets, an ochre fascia,
    a balustrade, turned columns carrying the gallery over it, and a roof over the upper one."""
    rng = random.Random(77)
    for gi, zf in enumerate(GAL_LEVELS):
        # Floor boards following the line, 0.3 m rows from the face out.
        rows = int(GAL_OUT / 0.3)
        for r in range(rows):
            d = 0.12 + (GAL_OUT - 0.1) * (r + 0.5) / rows
            line = gallery_line(zf - 0.06, d)
            L = len(line)
            j = 0
            while j < L - 1:
                je = min(j + rng.randint(5, 9), L - 1)
                pts = line[j:je + 1]
                fr = T.polyline(pts, lambda q, ln=line: _across_h(ln, q), lambda q: UP)
                p.board(fr, T.length_of(fr), (GAL_OUT - 0.1) / rows - 0.02, 0.1, rng.choice(TEAK), nseed(), seg=0.8, wear=0.8, bend=0.005, twist=0.3)
                j = je
        # Under the floor: a dark soffit (the gallery below sees it as its ceiling) and the edge beam.
        inner, outer = gallery_line(zf - 0.32, 0.0), gallery_line(zf - 0.32, GAL_OUT + 0.1)
        for a0, a1, b0, b1 in zip(inner[:-1], inner[1:], outer[:-1], outer[1:]):
            p.polygon([a0, b0, b1, a1], "MI_Hub_HullTarDark", tone=0.3, facing=(0.0, 0.0, -1.0))
        fl = gallery_line(zf - 0.2, GAL_OUT + 0.05)
        fr = T.polyline(fl, lambda q, ln=fl: _across_h(ln, q), lambda q: UP)
        p.sweep(fr, T.length_of(fr, 60), [(-0.06, -0.24, ""), (0.12, -0.24, "c"), (0.2, -0.12, "c"), (0.2, 0.12, "t"), (0.12, 0.2, "c"), (-0.06, 0.2, "")],
                "MI_Hub_Ochre", nseed(), seg=0.8, wear=1.0, wobble=0.004)
        # Brackets under it: carved knees from the face, every 2.6 m along it.
        f = face(zf)
        nb = int(2 * f.half / 2.6)
        for k in range(nb + 1):
            u = -f.half + 2 * f.half * (k + 0.0) / nb
            if abs(abs(u) - f.half) < 0.3:
                u = (f.half - 0.6) * (1 if u > 0 else -1)
            p0, tg, n0 = face(zf - 1.7).at(u, 0.0)
            p1, _, n1 = face(zf - 0.3).at(u, GAL_OUT - 0.3)
            p2, _, _ = face(zf - 0.3).at(u, 0.0)
            ctrl = p2 + (p1 - p2) * 0.15 + (p0 - p2) * 0.15
            pts = [p0 * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + p1 * t * t for t in (j / 8 for j in range(9))]
            fr = T.polyline(pts, lambda q, tg=tg: tg, None)
            p.beam(fr, T.length_of(fr), 0.32, 0.36, "MI_Hub_HullTarDark", nseed(), seg=0.4, wear=1.0)
            p.lathe([(0.0, 0.0), (0.17, 0.0), (0.2, 0.1), (0.12, 0.2), (0.15, 0.32), (0.0, 0.42)], p0 - UP * 0.02, -UP, "MI_Hub_Ochre", sides=8)
        # The balustrade along the edge, and the columns at every second post, up to the next floor or the roof.
        bl = gallery_line(zf, GAL_OUT - 0.18)
        baluster_row(p, [(q.x, q.y) for q in bl], lambda q: zf, height=1.0, posts=2.6)
        top = (GAL_LEVELS[gi + 1] - 0.34) if gi + 1 < len(GAL_LEVELS) else GAL_ROOF - 0.05
        Ltot = sum((b - a).length for a, b in zip(bl[:-1], bl[1:]))
        ncol = max(2, int(Ltot / 5.2))
        acc = [0.0]
        for a, b in zip(bl[:-1], bl[1:]):
            acc.append(acc[-1] + (b - a).length)
        for k in range(ncol + 1):
            s = Ltot * k / ncol
            j = next((i for i in range(len(acc) - 1) if acc[i + 1] >= s), len(acc) - 2)
            t = (s - acc[j]) / max(acc[j + 1] - acc[j], 1e-9)
            c = bl[j].lerp(bl[j + 1], t)
            c = Vector((c.x, c.y, zf))
            hgt = top - c.z
            prof = [(0.17, 0.0), (0.2, 0.08), (0.14, 0.2), (0.15, 0.35), (0.12, 0.6), (0.13, 0.85), (0.11, 0.92), (0.19, 0.97), (0.22, 1.0)]
            p.lathe([(r, h * hgt) for r, h in prof], c, UP, "MI_Paint_Cream", sides=10, wear=0.8)
    # The roof over the upper gallery: boards falling outward, a fascia and a cornice.
    zr = GAL_ROOF
    rows = 8
    for r in range(rows):
        d0, d1 = 0.05 + (GAL_OUT + 0.35) * r / rows, 0.05 + (GAL_OUT + 0.35) * (r + 1) / rows
        dm = 0.5 * (d0 + d1)
        line = gallery_line(zr - 0.3 * dm / GAL_OUT, dm)
        fr = T.polyline(line, lambda q, ln=line: _across_h(ln, q), lambda q: UP)
        p.board(fr, T.length_of(fr, 60), (d1 - d0) + 0.03, 0.09, "MI_Hub_HullTarDark", nseed(), seg=0.8, wear=0.9)
    fl = gallery_line(zr - 0.42, GAL_OUT + 0.38)
    fr = T.polyline(fl, lambda q, ln=fl: _across_h(ln, q), lambda q: UP)
    p.beam(fr, T.length_of(fr, 60), 0.36, 0.16, "MI_Hub_Ochre", nseed(), seg=0.8, wear=1.0)


def taffrail(p):
    """Her name on a dark frieze under the taffrail, brass borders, the cap moulding, a balustrade along the top, a crest."""
    zm = 0.5 * (FRIEZE[0] + FRIEZE[1])
    f = face(zm)
    # The frieze: dark boards behind the letters, across the whole face.
    face_boards(p, FRIEZE[0], FRIEZE[1], lambda u: True, mats=("MI_Paint_HullDark",), thick=0.12)
    for zb in (FRIEZE[0] + 0.06, FRIEZE[1] - 0.06):
        face_moulding(p, zb, 0.1, 0.12, "MI_Metal_Brass", u0=-f.half * 0.62, u1=f.half * 0.62, d=0.12)
    letters(p, HUB["name"].upper(), FRIEZE[0] + 0.2, FRIEZE[1] - 0.2)
    face_moulding(p, FRIEZE[1] + 0.12, 0.26, 0.3, "MI_Hub_Ochre")
    # Over it, the taffrail: turned balusters between the cap moulding and the rail, up to the stern's top.
    z0, z1 = FRIEZE[1] + 0.25, STERN_TOP
    fr = face(z1)
    n = int(2 * fr.half / 0.4)
    for k in range(1, n):
        u = -fr.half + 2 * fr.half * k / n
        c, _, nn = face(z0).at(u, 0.15)
        hgt = z1 - z0 - 0.1
        prof = [(0.09, 0.0), (0.09, 0.06), (0.06, 0.12), (0.12, 0.4), (0.13, 0.5), (0.07, 0.7), (0.05, 0.78), (0.08, 0.82), (0.08, 0.88)]
        p.lathe([(r, h * hgt / 0.88) for r, h in prof], c, UP, "MI_Paint_Cream", sides=8, wear=0.7)
    k = 40
    pts = [fr.at(-fr.half + 2 * fr.half * j / k, 0.12)[0] + UP * 0.1 for j in range(k + 1)]
    f2 = T.polyline(pts, lambda q, ln=pts: _across_h(ln, q), lambda q: UP)
    p.beam(f2, T.length_of(f2, 60), 0.62, 0.26, "MI_Wood_Varnish", nseed(), seg=0.6, wear=1.0)
    # The crest at the middle: a carved shell of ochre over a brass boss, on a plinth.
    c, _, nn = face(STERN_TOP).at(0.0, 0.2)
    p.block(c.x - 0.3, c.x + 0.3, -1.1, 1.1, STERN_TOP + 0.2, STERN_TOP + 0.55, "MI_Hub_Ochre", nseed(), axis="y")
    for j in range(7):
        a = math.pi * (j + 0.5) / 7
        d = Vector((0.0, math.cos(a), math.sin(a)))
        q0 = c + UP * 0.55 + d * 0.15
        q1 = c + UP * 0.55 + d * 0.95
        p.beam(T.straight(q0, q1, X.cross(d), X), 0.8, 0.22, 0.16, "MI_Hub_Ochre", nseed(), seg=0.5, wear=1.0)
    p.lathe([(0.0, -0.08), (0.36, -0.08), (0.42, 0.0), (0.36, 0.1), (0.0, 0.14)], c + UP * 0.55 - X * 0.2, -X, "MI_Metal_Brass", sides=16, wear=0.8)


LETTERS = {
    "H": [((0, 0), (0, 1)), ((1, 0), (1, 1)), ((0, 0.52), (1, 0.52))],
    "O": "ring",
    "L": [((0, 1), (0, 0)), ((0, 0), (0.9, 0))],
    "W": [((0, 1), (0.24, 0)), ((0.24, 0), (0.5, 0.62)), ((0.5, 0.62), (0.76, 0)), ((0.76, 0), (1, 1))],
    "K": [((0, 0), (0, 1)), ((0.95, 1), (0.06, 0.42)), ((0.32, 0.6), (0.97, 0))],
    "E": [((0, 0), (0, 1)), ((0, 1), (0.88, 1)), ((0, 0.52), (0.7, 0.52)), ((0, 0), (0.9, 0))],
}


def letters(p, text, z0, z1):
    """Her name in chunky brass strokes with serifs, centred on the frieze, following its curve (read from astern: port to
    starboard)."""
    h = z1 - z0
    w, gap = h * 0.62, h * 0.34
    total = len(text) * w + (len(text) - 1) * gap
    u = -total * 0.5
    for ch in text:
        strokes = LETTERS.get(ch)
        if strokes == "ring":
            pts = [(0.5 + 0.5 * math.cos(2 * math.pi * k / 14), 0.5 + 0.5 * math.sin(2 * math.pi * k / 14)) for k in range(15)]
            strokes = list(zip(pts[:-1], pts[1:]))
        F = face_frame(u + w * 0.5, 0.5 * (z0 + z1), 0.13)
        for (u0, v0), (u1, v1) in strokes or []:
            a = HH.lpt(F, (u0 - 0.5) * w, z0 + v0 * h - F[0].z, 0.05)
            b = HH.lpt(F, (u1 - 0.5) * w, z0 + v1 * h - F[0].z, 0.05)
            d = (b - a).normalized()
            p.beam(T.straight(a - d * 0.04, b + d * 0.04, F[3].cross(d), F[3]), (b - a).length + 0.08, h * 0.15, 0.09,
                   "MI_Metal_Brass", nseed(), seg=2.0, wear=0.9)
            if abs(u0 - u1) < 1e-3:
                for vv in (v0, v1):
                    c = HH.lpt(F, (u0 - 0.5) * w, z0 + vv * h - F[0].z, 0.06)
                    p.beam(T.straight(c - F[1] * w * 0.17, c + F[1] * w * 0.17, F[2], F[3]), w * 0.34, h * 0.08, 0.1, "MI_Metal_Brass", nseed(), seg=2.0)
        u += w + gap


def lantern(p, base, mount=None):
    """A great stern lantern: brass frame, six glass sides, a dome, a finial; on an iron bracket from `mount` if given."""
    if mount is not None:
        f = T.polyline([mount, mount + (base - mount) * 0.5 + UP * 0.35, base + Vector((0.0, 0.0, -0.15))], lambda q: Y, lambda q: X)
        p.beam(f, T.length_of(f), 0.16, 0.12, "MI_Metal_Iron", nseed(), seg=0.4)
    p.lathe([(0.0, -0.25), (0.15, -0.25), (0.35, 0.0), (0.55, 0.15), (0.62, 0.3), (0.0, 0.3)], base, UP, "MI_Metal_Brass", sides=6, wear=0.8)
    p.lathe([(0.56, 0.3), (0.6, 1.0), (0.64, 1.6), (0.0, 1.6)], base, UP, "MI_LanternGlass", sides=6, caps=False)
    for k in range(6):
        a = 2 * math.pi * k / 6
        dirv = Vector((math.cos(a), math.sin(a), 0.0))
        p.beam(T.straight(base + dirv * 0.6 + UP * 0.3, base + dirv * 0.66 + UP * 1.6, dirv.cross(UP), dirv), 1.3, 0.07, 0.06,
               "MI_Metal_Brass", nseed(), seg=2.0)
    p.lathe([(0.7, 1.6), (0.7, 1.7), (0.55, 1.85), (0.35, 2.05), (0.15, 2.2), (0.06, 2.3), (0.12, 2.38), (0.0, 2.5)], base, UP,
            "MI_Metal_Brass", sides=12, wear=0.8)


# --- below the counter: tuck, sternpost, deadwood, rudder -------------------------------------------------------------------------
def tuck_w(z):
    """The tuck's half-width at z (the hull's at the sternpost, below the counter)."""
    return hw(POST_X + 0.02, z)


def heel_x(z):
    """Where the hull's bottom rises through height z near the stern (the heel's curve, profile_bottom)."""
    a, b = POST_X, 12.0
    for _ in range(30):
        m = 0.5 * (a + b)
        if H.profile_bottom(m) > z:
            a = m
        else:
            b = m
    return 0.5 * (a + b)


POST_HW = 0.62


def tuck_and_post(p):
    """The flat tuck at the sternpost planked across, its fashion pieces, the sternpost, the deadwood, the keel's sole."""
    rng = random.Random(90)
    # The tuck: rows across at x POST_X (facing aft), between the post and the fashion pieces.
    z = 1.8
    F0 = (Vector((POST_X, 0.0, 0.0)), Y, UP, -X)
    while z < COUNTER - 0.62:
        zt = min(z + ROW, COUNTER - 0.62)
        zm = 0.5 * (z + zt)
        w = min(tuck_w(z + 0.05), tuck_w(zt - 0.05)) - 0.3
        # The dark core behind the row follows the tuck's true outline (it flares fast under the counter, faster than a row
        # of square-ended boards can follow): no see-through beside the fashion pieces.
        for sgn in (-1.0, 1.0):
            pts = [(POST_X, sgn * POST_HW, z), (POST_X, sgn * (tuck_w(z) + 0.05), z), (POST_X, sgn * (tuck_w(zt) + 0.05), zt), (POST_X, sgn * POST_HW, zt)]
            p.polygon(pts, "MI_Caulk_Dark", tone=0.2, facing=(-1.0, 0.0, 0.0))
        if w > POST_HW + 0.3:
            for sgn in (-1.0, 1.0):
                rngs = [(POST_HW + 0.02, w)] if sgn > 0 else [(-w, -POST_HW - 0.02)]
                board_runs(p, (Vector((POST_X, 0.0, zm)), Y, UP, -X), rngs, -(zt - z) * 0.5, (zt - z) * 0.5, 0.0, TAR, lmin=2.0, lmax=5.0)
        z = zt
    # Fashion pieces: heavy timbers along the tuck's edges, from the post's foot up to the wing transom.
    for side in (1, -1):
        pts = []
        for k in range(20):
            zz = 0.0 + (COUNTER - 0.3) * k / 19
            pts.append(Vector((POST_X - 0.25, side * (tuck_w(zz) - 0.1), zz)))
        f = T.polyline(pts, X, None)
        p.beam(f, T.length_of(f, 40), 0.7, 0.6, "MI_Hub_HullTarDark", nseed(), seg=0.6, wear=1.0)
    # The wing transom across the tuck's head, with the helm port where the rudder's head goes up through it.
    w = tuck_w(COUNTER - 0.05) - 0.05
    for ya, yb in ((-w, -0.75), (0.75, w)):
        a, b = Vector((POST_X - 0.13, ya, COUNTER - 0.4)), Vector((POST_X - 0.13, yb, COUNTER - 0.4))
        p.sweep(T.straight(a, b, UP, -X), yb - ya, HH.moulding_section(0.8, 0.62), "MI_Wood_Tarred", nseed(), seg=0.8, wear=0.9, wobble=0.005)
    # The soffit: the stern's overhang seen from below, planked, from the wing transom aft to the counter rail.
    # Where the face overhangs the tuck the soffit closes it from below; at the corners, where the tuck's head stands out aft
    # of the face, a cap closes the shelf from above.
    f = face(COUNTER + 0.02)
    k = 60
    for j in range(k):
        u0, u1 = -f.half + 2 * f.half * j / k, -f.half + 2 * f.half * (j + 1) / k
        a0, a1 = f.at(u0)[0], f.at(u1)[0]
        b0, b1 = Vector((POST_X + 0.05, a0.y, a0.z)), Vector((POST_X + 0.05, a1.y, a1.z))
        if min(a0.x, a1.x) < POST_X:
            s0, s1 = Vector((min(a0.x, POST_X), a0.y, a0.z)), Vector((min(a1.x, POST_X), a1.y, a1.z))
            p.polygon([s0, s1, b1, b0], "MI_Hub_HullTarDark", tone=0.35, facing=(0.0, 0.0, -1.0))
        if max(a0.x, a1.x) > POST_X - 0.2:
            c0, c1 = Vector((POST_X - 0.2, a0.y, COUNTER - 0.01)), Vector((POST_X - 0.2, a1.y, COUNTER - 0.01))
            p.polygon([Vector((max(a0.x, POST_X - 0.1), a0.y, COUNTER - 0.01)), Vector((max(a1.x, POST_X - 0.1), a1.y, COUNTER - 0.01)), c1, c0],
                      "MI_Hub_HullTarDark", tone=0.35, facing=(0.0, 0.0, 1.0))
    for y in [-17.0 + 0.5 * j for j in range(69)]:
        xa = stern_x(y, COUNTER + 0.02)
        if xa > POST_X - 0.9 or abs(y) < 0.8:
            continue
        a, b = Vector((xa + 0.05, y, COUNTER - 0.06)), Vector((POST_X - 0.62, y, COUNTER - 0.06))
        p.board(T.straight(a, b, Y, -UP), (b - a).length, 0.47, 0.08, rng.choice(TAR), nseed(), seg=0.8, wear=0.5)
    # The sternpost: from the keel's sole up under the wing transom, as wide as the hull there.
    pts = []
    for k in range(14):
        zz = HUB["keel"] - 0.5 + (COUNTER - 0.5 - HUB["keel"] + 0.5) * k / 13
        pts.append(Vector((POST_X + 0.45, 0.0, zz)))
    f = T.polyline(pts, Y, -X)
    p.beam(f, T.length_of(f), 2 * POST_HW + 0.2, 1.1, "MI_Hub_HullTarDark", nseed(), seg=0.8, wear=1.0)
    # The deadwood between the post and the heel, and the keel's sole running aft under the rudder's heel.
    zz = HUB["keel"] - 0.5
    while zz < -4.2:
        zt = min(zz + 0.62, -4.0)
        xh = heel_x(zt) + 0.8
        p.block(POST_X + 0.9, max(xh, POST_X + 1.5), -POST_HW + 0.05, POST_HW - 0.05, zz, zt - 0.02, rng.choice(("MI_Hub_HullTarDark", "MI_Hub_HullTar")),
                nseed(), axis="x")
        zz = zt
    ru = HUB["rudder"]
    p.block(ru["x0"] + 1.2, 6.0, -POST_HW - 0.02, POST_HW + 0.02, HUB["keel"] - 0.56, ru["z0"] - 0.08, "MI_Hub_HullTarDark", nseed(), axis="x")
    # The heel: the keel's run up the heel's curve to the post, as wide as the narrowing hull there (it closes the fin's
    # underside and takes the strakes' ends).
    xs_ = [POST_X + 0.05 + 0.25 * k for k in range(19)]

    def heel_frame(s):
        x = xs_[0] + (xs_[-1] - xs_[0]) * s
        z = H.profile_bottom(x)
        dz = (H.profile_bottom(x + 0.05) - H.profile_bottom(x - 0.05)) / 0.1
        tg = Vector((1.0, 0.0, dz)).normalized()
        n = Vector((tg.z, 0.0, -tg.x))
        if n.z > 0.0:
            n = -n
        w = 2.0 * (hw(x, z + 0.55) + PT + 0.18)
        return Vector((x, 0.0, z)) + n * 0.0, Y, n, max(w, 1.3) / 2.0
    p.beam(heel_frame, T.length_of(heel_frame, 30), 2.0, 1.1, "MI_Hub_HullTarDark", nseed(), seg=0.3, wear=1.0, drift=0.0)
    for zb in (-10.0, -7.0, -4.0, -1.0, 2.0, 5.0):                            # iron straps round the post (the gudgeons)
        for sgn in (-1.0, 1.0):
            a, b = Vector((POST_X - 0.05, sgn * (POST_HW + 0.12), zb)), Vector((POST_X + 2.2, sgn * (POST_HW + 0.12), zb))
            p.beam(T.straight(a, b, UP, Y * sgn), 2.25, 0.2, 0.05, "MI_Metal_Iron", nseed(), seg=1.0, wear=0.6)


def rudder(p):
    """The one rudder on the sternpost (hub.rudder): oak timbers bolted together, its fore edge round about the pintles, iron
    straps; its head goes up under the counter into a canvas coat at the helm port, where the tiller comes in."""
    ru = HUB["rudder"]
    x_le = ru["x1"]
    n = 4

    def chord(z):
        return (ru["x1"] - ru["x0"]) + (ru["head"] - (ru["x1"] - ru["x0"])) * (z - ru["z0"]) / (ru["z1"] - ru["z0"])
    top = ru["z1"] - 0.12
    for k in range(n):
        def fr(s, k=k):
            z = ru["z0"] + (top - ru["z0"]) * s
            c = chord(z) - 0.25
            x = x_le - 0.25 - c * (k + 0.5) / n
            return Vector((x, 0.0, z)), X, Y, (c / n) / 0.8
        p.beam(fr, top - ru["z0"], 0.8, 0.82 - 0.1 * k, "MI_Hub_HullTarDark" if k % 2 else "MI_Hub_HullTar", nseed(), seg=0.8, wear=0.9,
               grime=lambda q: min(max((1.2 - q.z) / 1.5, 0.0), 1.0))
    # The main piece: round at the fore edge (the pintles' axis).
    p.round(T.straight((x_le - 0.3, 0.0, ru["z0"]), (x_le - 0.3, 0.0, ru["z1"] + 0.3), Y, X), ru["z1"] + 0.3 - ru["z0"], 0.45, "MI_Hub_HullTar",
            nseed(), sides=12, seg=1.0, wear=0.7)
    for zs in (-10.0, -7.0, -4.0, -1.0, 2.0, 5.0):
        c = chord(zs)
        for sgn in (-1.0, 1.0):
            a = Vector((x_le - 0.1, sgn * 0.47, zs + 0.25))
            b = Vector((x_le - c * 0.8, sgn * 0.42, zs + 0.25))
            p.beam(T.straight(a, b, UP, Vector((0.0, sgn, 0.0))), (b - a).length, 0.22, 0.05, "MI_Metal_Iron", nseed(), seg=1.0, wear=0.6)
    # The coat round its head at the helm port.
    c = Vector((x_le - 0.45, 0.0, COUNTER - 0.75))
    p.lathe([(0.62, 0.0), (0.72, 0.1), (0.66, 0.35), (0.7, 0.55), (0.6, 0.8), (0.5, 0.8)], c, UP, "MI_Hub_SailFurled", sides=12, caps=False)


def cable(p, cb, r=0.2):
    """A mooring cable from an iron-rimmed fairlead in the hull down into the water, sagging a little, and on under it."""
    a, b = Vector(cb["from"]), Vector(cb["to"])
    side = 1.0 if a.y > 0 else -1.0
    x = a.x
    a = Vector((x, side * (hw(x, a.z) + PT + 0.1), a.z))
    d = (b - a)
    pts = [a + d * (k / 12) - UP * (0.6 * math.sin(math.pi * k / 12)) for k in range(13)]
    pts.append(b + d.normalized() * 4.0)
    f = T.polyline(pts, lambda q: UP.cross(d.normalized()).normalized(), lambda q: UP)
    p.round(f, T.length_of(f, 60), r, "MI_Rope_Hemp", nseed(), sides=8, seg=1.2, wear=0.0, grime=lambda q: min(max((0.6 - q.z) / 0.8, 0.0), 1.0))
    n = HH.hull_frame(x, a.z, side)[3]
    p.lathe([(r + 0.12, -0.2), (r + 0.28, -0.12), (r + 0.3, 0.0), (r + 0.28, 0.12), (r + 0.12, 0.2), (r + 0.06, 0.2), (r + 0.06, -0.2)],
            a - n * 0.15, n, "MI_Metal_Iron", sides=12, caps=False)


def build_stern():
    p = T.Part("SM_Hub_Stern")
    tuck_and_post(p)
    rudder(p)
    stern_face(p)
    stern_galleries(p)
    taffrail(p)
    # The three great lanterns: on the crest in the middle and on the corner pieces' heads.
    c, _, _ = face(STERN_TOP).at(0.0, 0.2)
    lantern(p, c + UP * 1.85 - X * 0.1)
    for side in (1, -1):
        w = corner_w(STERN_TOP)
        top = Vector((ROUND - H.stern_lean(STERN_TOP) - 0.12, side * (w + 0.05), STERN_TOP + 0.4))
        p.lathe([(0.0, 0.0), (0.5, 0.0), (0.5, 0.18), (0.38, 0.3), (0.0, 0.3)], top, UP, "MI_Hub_Ochre", sides=10)
        lantern(p, top + UP * 0.55)
    # The inside of the stern above the poop deck (seen from the poop): boards on the face's back.
    zi = Z["poop"]
    while zi < STERN_TOP - 0.05:
        zt = min(zi + ROW, STERN_TOP - 0.05)
        f = face(0.5 * (zi + zt))
        face_boards(p, zi, zt, lambda u: True, mats=("MI_Wood_TeakDark", "MI_Hub_HullTarDark"), thick=0.1, d=-0.62)
        zi = zt
    for cb in HUB["cables"]:
        if cb["id"] == "stern":
            cable(p, cb)
    HH.export(p)


# --- the quarter galleries ----------------------------------------------------------------------------------------------
def gal_pt(x, d, z, side):
    return Vector((x, side * (hw(x, z) + PT + d), z))


def build_galleries():
    """The quarter galleries (hub.galleries.quarter): enclosed bays standing out from the quarters over three storeys, on
    brackets with a brass drop under them, windows on every face, a roof leaning back and a brass dome."""
    p = T.Part("SM_Hub_Galleries")
    g = QG
    x0, x1, out, z0, z1 = g["x0"], g["x1"], g["out"], g["z0"], g["z1"]
    outline = [(x0, 0.0), (x0 + 0.6, out), (x1 - 1.4, out), (x1, 0.0)]
    tiers = [(z0 + 0.4, z0 + 2.75), (z0 + 3.4, z0 + 5.75), (z0 + 6.4, z1 - 0.3)]
    for side in (1, -1):
        faces = list(zip(outline[:-1], outline[1:]))
        # The floor and its edge, brackets under it, the brass drop.
        floor_pts = [gal_pt(x, d, z0 - 0.3, side) for x, d in outline]
        p.polygon(floor_pts, "MI_Hub_HullTarDark", tone=0.3, facing=-UP)
        for fa, fb in faces:
            pa, pb = gal_pt(fa[0], fa[1] + 0.1, z0 - 0.15, side), gal_pt(fb[0], fb[1] + 0.1, z0 - 0.15, side)
            t = (pb - pa).normalized()
            n = t.cross(UP) if side > 0 else UP.cross(t)
            p.sweep(T.straight(pa - t * 0.15, pb + t * 0.15, UP, n), (pb - pa).length + 0.3, HH.moulding_section(0.5, 0.32), "MI_Hub_Ochre", nseed(), seg=1.0)
        for x in (x0 + 1.2, 0.5 * (x0 + x1), x1 - 1.6):
            if abs(x - 5.0) < 1.0:
                x += 1.4                                      # clear of the stern mooring's fairlead
            a, b = gal_pt(x, 0.05, z0 - 2.4, side), gal_pt(x, out - 0.5, z0 - 0.35, side)
            ctrl = gal_pt(x, 0.3, z0 - 0.5, side)
            pts = [a * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + b * t * t for t in (j / 8 for j in range(9))]
            p.beam(T.polyline(pts, X, None), T.length_of(T.polyline(pts, X)), 0.36, 0.34, "MI_Hub_HullTarDark", nseed(), seg=0.4, wear=1.0)
        drop = gal_pt(0.5 * (x0 + x1) + 0.4, out * 0.5, z0 - 0.3, side)
        p.lathe([(0.0, 0.0), (0.95, 0.0), (1.05, 0.25), (0.8, 0.6), (0.9, 1.0), (0.55, 1.6), (0.62, 2.0), (0.3, 2.6),
                 (0.36, 2.9), (0.12, 3.3), (0.0, 3.6)], drop, -UP, "MI_Metal_Brass", sides=12, wear=1.0)
        # The faces: oxblood boards between the windows, an ochre moulding at each storey, cream above; corner posts.
        for (x, d) in outline[1:-1]:
            a, b = gal_pt(x, d, z0, side), gal_pt(x, d, z1 + 0.4, side)
            p.beam(T.straight(a, b, X, Vector((0.0, side, 0.0))), (b - a).length, 0.36, 0.36, "MI_Paint_Hull", nseed(), seg=1.0)
        for fi, (fa, fb) in enumerate(faces):
            P0, P1 = gal_pt(fa[0], fa[1], z0, side), gal_pt(fb[0], fb[1], z0, side)
            t = (P1 - P0).normalized()
            L = (P1 - P0).length
            lean = gal_pt(fa[0], fa[1], z1 + 0.4, side) - P0
            Hf = lean.length
            a = lean.normalized()
            n = t.cross(a)
            if n.y * side < 0.0:
                n = -n
            F = (P0, t, a, n)
            nwin = max(1, int(L / 2.2))
            wins = []
            for (tz0, tz1) in tiers:
                for k in range(nwin):
                    door = fi == 0 and tz0 < z0 + 6.0                     # the aft faces open onto the stern galleries
                    wh = 0.5 * (tz1 - tz0) - (0.05 if door else 0.25)
                    wins.append((L * (k + 0.5) / nwin, 0.5 * (tz0 + tz1) - z0 - (0.25 if door else 0.0), min(0.72, L / nwin * 0.5 - 0.3), wh, door))

            def keep(u, v, wins=wins):
                return all(not (abs(u - wu) < whw + 0.14 and wv - whh - 0.16 < v < wv + whh + 0.16) for wu, wv, whw, whh, _ in wins)
            wall_boards(p, F, 0.02, L - 0.02, 0.0, Hf - 0.05, keep,
                        mats=("MI_Paint_Hull", "MI_Paint_Hull", "MI_Paint_HullFaded", "MI_Paint_HullDark"))
            for j, (wu, wv, whw, whh, door) in enumerate(wins):
                window_unit(p, (HH.lpt(F, wu, wv, 0.0), t, a, n), whw, whh, 0.0, "glass" if door else ("glow" if (j + side) % 3 == 0 else "glass"),
                            "MI_Paint_Cream", panes=(1, 3 if door else 2), hood=not door)
            for zm, h, mat in ((z0 + 3.1, 0.45, "MI_Hub_Ochre"), (z0 + 6.1, 0.45, "MI_Hub_Ochre"), (z1 - 0.05, 0.5, "MI_Paint_Cream"), (z1 + 0.35, 0.4, "MI_Hub_Ochre")):
                v = zm - z0
                pa, pb = HH.lpt(F, -0.2, v, 0.15), HH.lpt(F, L + 0.2, v, 0.15)
                p.sweep(T.straight(pa, pb, a, n), L + 0.4, HH.moulding_section(h, 0.3), mat, nseed(), seg=1.0)
        # The roof leaning back to the hull, boarded, and the dome on top with its finial.
        rows = 6
        zr = z1 + 0.55
        for k in range(rows):
            t0, t1 = k / rows, (k + 1) / rows
            za, zb = zr + 1.6 * t0, zr + 1.6 * t1
            da, db = out * (1 - t0) + 0.25, out * (1 - t1) + 0.25
            xa0 = x0 + 0.6 * (1 - t0) * 0.6
            xa1 = x1 - 1.4 * (1 - t0) * 0.6
            pa, pb = gal_pt(xa0, 0.5 * (da + db), 0.5 * (za + zb), side), gal_pt(xa1, 0.5 * (da + db), 0.5 * (za + zb), side)
            acr = (gal_pt(0.5 * (xa0 + xa1), db, zb, side) - gal_pt(0.5 * (xa0 + xa1), da, za, side))
            w = acr.length
            acr.normalize()
            nn = (pb - pa).normalized().cross(acr)
            if nn.z < 0:
                nn = -nn
            p.board(T.straight(pa, pb, acr, nn), (pb - pa).length, w + 0.04, 0.1, "MI_Hub_HullTarDark", nseed(), seg=1.0, wear=0.9)
        dome = gal_pt(0.5 * (x0 + x1) - 0.3, out * 0.45, zr + 0.4, side)
        p.lathe([(0.0, 0.0), (1.2, 0.0), (1.2, 0.35), (1.08, 0.45), (1.02, 0.9), (0.86, 1.35), (0.6, 1.75), (0.3, 2.0), (0.0, 2.08)],
                dome, UP, "MI_Metal_Brass", sides=16, wear=0.9)
        p.lathe([(0.0, 0.0), (0.12, 0.0), (0.18, 0.2), (0.1, 0.4), (0.2, 0.6), (0.0, 0.85)], dome + UP * 2.05, UP, "MI_Metal_Brass", sides=8)
    HH.export(p)


# --- the castles: decks, rails, fronts ----------------------------------------------------------------------------------------
def deck(part, z, region, x0, x1, y0, y1):
    """Deck boards fore and aft: region(x, y) says where the deck is; a dark core under the seams."""
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


def bulwark_inside(part, z, xa, xb):
    """The inner side of a castle deck's bulwark: stanchions on the side's frames, boards between, a waterway at the foot."""
    rng = random.Random(nseed())
    for side in (1, -1):
        x = xa
        while x < xb - 0.2:
            xe = min(x + rng.uniform(4.0, 6.5), xb)
            for v0 in (0.0, 0.45):
                pts = []
                for k in range(4):
                    xx = x + (xe - x) * k / 3
                    zt = min(z + v0 + 0.45, H.profile_top(xx) - 0.05)
                    zz = 0.5 * (z + v0 + zt)
                    pts.append(Vector((xx, side * (hw(xx, zz) - 0.12), zz)))
                f = T.polyline(pts, UP, lambda q, s=side: Vector((0.0, -s, 0.0)))
                part.board(f, T.length_of(f), 0.42, 0.08, rng.choice(("MI_Wood_TeakDark", "MI_Hub_HullTarDark")), nseed(), seg=0.8, wear=0.6)
            x = xe
        x = xa + 0.6
        while x < xb - 0.3:
            zt = H.profile_top(x) - 0.03
            a, b = Vector((x, side * (hw(x, z) - 0.25), z)), Vector((x, side * (hw(x, zt) - 0.25), zt))
            part.beam(T.straight(a, b, X, Vector((0.0, -side, 0.0))), (b - a).length, 0.22, 0.2, "MI_Hub_HullTarDark", nseed(), seg=1.0)
            x += 1.8
        pts = [Vector((xx, side * (hw(xx, z + 0.1) - 0.3), z + 0.08)) for xx in [xa + (xb - xa) * k / 8 for k in range(9)]]
        part.beam(T.polyline(pts, lambda q, s=side: Vector((0.0, s, 0.0)), UP), xb - xa, 0.3, 0.16, "MI_Hub_HullTarDark", nseed(), seg=1.0)


def front(part, xf, z0, z1, facing, wins, doors=(), closed_doors=()):
    """A castle's front across the deck at x xf from z0 to z1: boards, windows, door openings (framed, for the gates) and shut
    doors; facing = +1 forward, -1 aft."""
    half = lambda z: hw(xf, z) - 0.05                                                    # noqa: E731
    F = (Vector((xf, 0.0, 0.0)), Y * facing * -1.0 if facing < 0 else Y, UP, X * facing)
    t = F[1]

    def keep(u, v):
        y = u if facing > 0 else -u
        if abs(y) > half(v):
            return False
        for wy, wz, ww, wh in wins:
            if abs(y - wy) < ww + 0.14 and wz - 0.2 < v < wz + wh + 0.2:
                return False
        for dy, dw, dh in list(doors) + list(closed_doors):
            if abs(y - dy) < dw * 0.5 + 0.2 and v < z0 + dh + 0.25:
                return False
        return True
    wall_boards(part, F, -half(z0) - 0.1, half(z0) + 0.1, z0, z1, keep)
    rng = random.Random(int(xf * 10))
    for wy, wz, ww, wh in wins:
        u = wy if facing > 0 else -wy
        window_unit(part, (HH.lpt(F, u, wz + wh * 0.5, 0.0), F[1], F[2], F[3]), ww, wh * 0.5, 0.0,
                    rng.choice(("glass", "glow", "shutter", "glass")), "MI_Paint_Cream")
    for dy, dw, dh in list(doors) + list(closed_doors):
        u = dy if facing > 0 else -dy
        Fd = (HH.lpt(F, u, z0, 0.0), F[1], F[2], F[3])
        for sgn in (-1.0, 1.0):
            HH.lbeam(part, Fd, sgn * (dw * 0.5 + 0.12), 0.0, sgn * (dw * 0.5 + 0.12), dh + 0.12, 0.26, 0.24, 0.0, "MI_Hub_HullTarDark")
        HH.lbeam(part, Fd, -dw * 0.5 - 0.35, dh + 0.17, dw * 0.5 + 0.35, dh + 0.17, 0.3, 0.3, 0.0, "MI_Hub_Ochre")
    for dy, dw, dh in closed_doors:
        u = dy if facing > 0 else -dy
        Fd = (HH.lpt(F, u, z0, 0.0), F[1], F[2], F[3])
        nb = 5
        for k in range(nb):
            uu = -dw * 0.5 + dw * (k + 0.5) / nb
            HH.lbeam(part, Fd, uu, 0.02, uu, dh, dw / nb - 0.02, 0.08, -0.06, rng.choice(("MI_Paint_Hull", "MI_Paint_HullFaded")), wear=1.0)
        for v in (0.4, dh - 0.4):
            HH.lbeam(part, Fd, -dw * 0.5 + 0.05, v, dw * 0.5 - 0.1, v, 0.12, 0.03, 0.02, "MI_Metal_Iron", wear=0.5)
        part.lathe([(0.0, 0.0), (0.08, 0.0), (0.08, 0.05), (0.0, 0.05)], HH.lpt(Fd, dw * 0.3, dh * 0.5, 0.03), F[3], "MI_Metal_Brass", sides=8)
    # The bands across the front continue the sides': an ochre one at the storey's foot, a moulding at the deck line above.
    for zb, h, mat in ((z0 + 0.3, 0.42, "MI_Hub_Ochre"), (z1 - 0.15, 0.3, "MI_Hub_Ochre")):
        gaps = sorted([((dy if facing > 0 else -dy) - dw * 0.5 - 0.3, (dy if facing > 0 else -dy) + dw * 0.5 + 0.3)
                       for dy, dw, dh in list(doors) + list(closed_doors) if zb < z0 + dh + 0.3])
        u0 = -half(zb) + 0.1
        segs = []
        for g0, g1 in gaps:
            if g0 > u0:
                segs.append((u0, g0))
            u0 = max(u0, g1)
        segs.append((u0, half(zb) - 0.1))
        for s0, s1 in segs:
            if s1 - s0 > 0.3:
                pa, pb = HH.lpt(F, s0, zb, 0.0), HH.lpt(F, s1, zb, 0.0)
                part.sweep(T.straight(pa, pb, UP, F[3]), s1 - s0, HH.moulding_section(h, 0.22), mat, nseed(), seg=1.0)


def build_castles():
    p = T.Part("SM_Hub_Castles")
    zp, zq, zf = Z["poop"], Z["sc2"], Z["fcTop"]
    # The poop deck (x 0..poopX) inside the stern face, the quarterdeck (poopX..qdX), the forecastle deck (fcX..stem).
    deck(p, zp, lambda x, y: abs(y) < hw(x, zp) - 0.3 and x > stern_x(y, zp + 0.5) + 0.75, -1.0, POOP_X + 0.05, -20.0, 20.0)
    deck(p, zq, lambda x, y: abs(y) < hw(x, zq) - 0.3, POOP_X + 0.3, QD_X - 0.05, -20.0, 20.0)
    fc_end = HH.end_x(zf) - 0.7
    deck(p, zf, lambda x, y: abs(y) < hw(x, zf) - 0.3, FC_X + 0.3, fc_end, -20.0, 20.0)
    bulwark_inside(p, zp, 1.2, POOP_X - 0.2)
    bulwark_inside(p, zq, POOP_X + 0.5, QD_X - 0.2)
    bulwark_inside(p, zf, FC_X + 0.5, fc_end - 2.0)
    # Balustrades along the decks' open edges: the poop's over the quarterdeck, the quarterdeck's over the waist, the
    # forecastle's over the waist.
    for xb, zb in ((POOP_X - 0.15, zp), (QD_X - 0.15, zq), (FC_X + 0.25, zf)):
        w = hw(xb, zb) - 0.4
        balustrade(p, (xb, -w), (xb, w), zb)
    # The fronts: the poop's (over the quarterdeck), the quarterdeck's (over the waist, with the two door openings where the
    # gates stand), the forecastle's (facing aft, two shut doors; the chained ladders at |y| 15 are the gates').
    win = lambda ys, zs, w=0.62, h=1.9: [(y, z, w, h) for y in ys for z in zs]                       # noqa: E731
    front(p, POOP_X + 0.02, zq, zp - 0.02, 1, win((-12.5, -8.5, -4.0, 4.0, 8.5, 12.5), (zq + 0.75, Z["sc3"] + 0.75)),
          closed_doors=[(0.0, 1.6, 2.4)])
    gates = [(g["y"], g["w"], 2.6) for g in _plan_gates() if abs(g["x"] - QD_X) < 0.5]
    front(p, QD_X + 0.02, Z["main"], zq - 0.02, 1, win((-15.5, -12.0, -3.0, 3.0, 12.0, 15.5), (Z["main"] + 0.75,)) +
          win((-15.0, -11.0, -7.5, -3.0, 3.0, 7.5, 11.0, 15.0), (Z["sc1"] + 0.75,)), doors=gates)
    front(p, FC_X - 0.02, Z["main"], zf - 0.02, -1, win((-11.5, -3.0, 3.0, 11.5), (Z["main"] + 0.75,)) +
          win((-11.5, -7.0, -3.0, 3.0, 7.0, 11.5), (Z["fc1"] + 0.75,)), closed_doors=[(-6.5, 1.5, 2.3), (6.5, 1.5, 2.3)])
    wheel(p)
    HH.export(p)


def _plan_gates():
    try:
        import hub_plan as P
        return P.plan("main").gates
    except Exception as e:                                   # the plan is the main session's; fall back to the data's doors
        print("SB_WARN no hub_plan gates:", e)
        return [{"x": QD_X, "y": -7.5, "w": 2.0}, {"x": QD_X, "y": 7.5, "w": 2.0}]


def wheel(p):
    """The great wheel on the quarterdeck (hub.helm), lashed with a rope cross; it never turns. Its ropes go down through the
    deck to the tiller."""
    hx, r = HUB["helm"]["x"], HUB["helm"]["r"]
    zd = Z["sc2"]
    c = Vector((hx, HUB["helm"]["y"], zd + r + 0.45))
    p.beam(T.arc(c, Y, UP, r - 0.12, 0.0, 2.0 * math.pi), 2.0 * math.pi * (r - 0.12), 0.24, 0.22, "MI_Wood_Varnish", nseed(), seg=0.3, wear=1.0)
    p.lathe([(0.0, -0.3), (0.36, -0.3), (0.42, -0.2), (0.42, 0.2), (0.36, 0.3), (0.0, 0.3)], c, X, "MI_Metal_Brass", sides=14)
    for k in range(10):
        a = 2 * math.pi * k / 10
        dirv = Y * math.cos(a) + UP * math.sin(a)
        p.beam(T.straight(c + dirv * 0.35, c + dirv * (r - 0.2), dirv.cross(X), X), r - 0.55, 0.12, 0.12, "MI_Wood_Varnish", nseed(), seg=1.0)
        p.lathe([(0.07, 0.0), (0.09, 0.08), (0.06, 0.2), (0.08, 0.38), (0.05, 0.55), (0.0, 0.6)], c + dirv * (r - 0.02), dirv, "MI_Wood_Varnish", sides=8)
    for dx in (-0.45, 0.45):
        base = Vector((hx + dx, 0.0, zd))
        for dy in (-0.5, 0.5):
            p.beam(T.straight(base + Y * dy, c + X * dx + Y * dy * 0.25 - UP * 0.1, X, Y), (c - base).length, 0.24, 0.22, "MI_Wood_Varnish", nseed())
    p.round(T.straight(c - X * 0.7, c + X * 0.7, Y, UP), 1.4, 0.13, "MI_Metal_Iron", nseed(), sides=10)
    for sgn in (-1.0, 1.0):
        a = Vector((hx - 0.3, sgn * 2.2, zd + 0.05))
        b = c - X * 0.25 + (Y * -sgn + UP).normalized() * (r - 0.05)
        f = T.polyline([a, c - X * 0.32 + (Y * sgn - UP).normalized() * 0.4, b], lambda q: UP, lambda q: -X)
        p.round(f, T.length_of(f), 0.05, "MI_Rope_Hemp", nseed(), sides=6, seg=0.6, wear=0.0)
        p.lathe([(0.0, 0.0), (0.12, 0.0), (0.12, 0.05), (0.0, 0.05)], Vector((hx - 0.3, sgn * 2.2, zd)), UP, "MI_Metal_Iron", sides=8)
        # The wheel ropes from the barrel down through the deck (to the tiller, aft below).
        a = c + Y * sgn * 0.5
        b = Vector((hx - 0.6, sgn * 0.5, zd + 0.02))
        p.round(T.straight(a, b, X, Y), (b - a).length, 0.05, "MI_Rope_Hemp", nseed(), sides=6, seg=1.0, wear=0.0)
        p.lathe([(0.12, 0.0), (0.2, 0.0), (0.2, 0.05), (0.12, 0.05)], b - UP * 0.02, UP, "MI_Metal_Brass", sides=10, caps=False)


# --- the head ----------------------------------------------------------------------------------------------------------
def build_head():
    p = T.Part("SM_Hub_Head")
    bh = HUB["beakhead"]
    # The stem: a great curved timber up the bow from the keel's fore end to above the forecastle's rail.
    zs = [HUB["keel"] - 0.4 + k * 0.8 for k in range(60)]
    zs = [z for z in zs if z < Z["fcTop"] + 1.5] + [Z["fcTop"] + 1.8]
    pts = [Vector((H.stem_x(min(z, Z["fcTop"] + 0.9)) + 0.25, 0.0, z)) for z in zs]
    pts[0] = Vector((131.0, 0.0, HUB["keel"] - 0.4))
    f = T.polyline(pts, lambda q: Y, None)
    p.beam(f, T.length_of(f, 80), 1.3, 0.8, "MI_Hub_HullTarDark", nseed(), seg=0.6, wear=1.0)
    # The knee of the head: from the stem sweeping out and up to the figurehead, trailboards on its sides.
    s0 = Vector((H.stem_x(6.0) + 0.3, 0.0, 6.0))
    fig = Vector(bh["figure"])
    ctrl = Vector((s0.x + 4.0, 0.0, 13.5))
    knee = [s0 * (1 - t) ** 2 + ctrl * 2 * t * (1 - t) + fig * t * t for t in (k / 16 for k in range(17))]
    f = T.polyline(knee, lambda q: Y, None)
    p.beam(f, T.length_of(f, 60), 0.9, 1.2, "MI_Hub_HullTarDark", nseed(), seg=0.5, wear=1.0)
    for side in (1, -1):
        tb = [q + Vector((0.0, side * 0.47, 0.0)) for q in knee[3:15]]
        f = T.polyline(tb, lambda q: UP, lambda q, s=side: Vector((0.0, s, 0.0)))
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
            xc = x0 - 1.5
            c = Vector((xc, side * (hw(xc, zfc) + 0.25), zfc))
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
    f = T.polyline(pts, lambda q: Y, None)
    p.beam(f, T.length_of(f, 80), 0.75, 0.32, "MI_Hub_Ochre", nseed(), seg=0.2, wear=1.0)
    p.lathe([(0.0, 0.0), (0.42, 0.0), (0.5, 0.18), (0.36, 0.3), (0.44, 0.5), (0.2, 0.6), (0.0, 0.72)], c + Vector((0.1, 0.0, 2.05)), UP,
            "MI_Metal_Brass", sides=10, wear=1.0)
    # The bowsprit (outside.bow), hewn, with iron bands; its heel in the forecastle's bow; the bobstay down to the stem.
    sp = H.prop("main_deck", "bowsprit")
    a, b = Vector(sp["from"]), Vector(sp["to"])
    d = (b - a).normalized()
    p.round(T.straight(a, b, Y, d.cross(Y)), (b - a).length, lambda s: 0.9 + (0.4 - 0.9) * s, "MI_Wood_Varnish", nseed(), sides=12, seg=1.0, wear=0.6)
    for s in (0.25, 0.35, 0.55, 0.8):
        q = a + (b - a) * s
        r = 0.9 + (0.4 - 0.9) * s + 0.04
        p.lathe([(r, -0.1), (r + 0.03, 0.0), (r, 0.1)], q, d, "MI_Metal_Iron", sides=14, caps=False)
    # Its heel held on the forecastle deck: the bitts either side of it, a cross cap over it, a chock under it.
    zd = Z["fcTop"]
    for sgn in (-1.0, 1.0):
        p.block(a.x + 0.6, a.x + 1.4, sgn * 1.35 - 0.4, sgn * 1.35 + 0.4, zd, a.z + 1.7, "MI_Hub_HullTarDark", nseed(), axis="z")
        p.lathe([(0.0, 0.0), (0.34, 0.0), (0.38, 0.12), (0.2, 0.2), (0.0, 0.22)], Vector((a.x + 1.0, sgn * 1.35, a.z + 1.7)), UP, "MI_Hub_Ochre", sides=8)
    p.block(a.x + 0.5, a.x + 1.5, -1.95, 1.95, a.z + 1.15, a.z + 1.5, "MI_Hub_HullTarDark", nseed(), axis="y")
    p.block(a.x - 0.4, a.x + 2.2, -0.75, 0.75, zd, a.z - 0.75, "MI_Wood_TeakDark", nseed(), axis="x")
    bs = H.prop("main_deck", "bobstay")
    a, b = Vector(bs["from"]), Vector(bs["to"])
    p.round(T.straight(a, b, Y, (b - a).normalized().cross(Y)), (b - a).length, 0.12, "MI_Rope_Hemp", nseed(), sides=8, seg=2.0, wear=0.0)
    # Catheads out from the forecastle's bow, and the hawse holes with the bow cables to the anchors ahead.
    for side in (1, -1):
        xc = 150.0
        zc = H.profile_top(xc) - 0.6
        q = Vector((xc, side * (hw(xc, zc) - 0.2), zc))
        out = Vector((0.35, side, 0.0)).normalized()
        p.beam(T.straight(q, q + out * 3.0, UP, out.cross(UP) * side), 3.0, 0.6, 0.6, "MI_Hub_HullTarDark", nseed(), seg=1.0)
        p.lathe([(0.0, 0.0), (0.36, 0.0), (0.36, 0.08), (0.0, 0.08)], q + out * 3.0, out, "MI_Metal_Brass", sides=12)
        p.beam(T.straight(q + out * 1.0 - UP * 0.3, q + out * 0.2 - UP * 1.6, out, out.cross(UP)), 1.5, 0.3, 0.3, "MI_Hub_HullTarDark", nseed())
    for cb in HUB["cables"]:
        if cb["id"] != "bow":
            continue
        a0 = Vector(cb["from"])
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
