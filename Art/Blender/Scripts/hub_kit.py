"""The hub's interior kit (D64): helpers the inside builders share, copied from the outside scripts as they were at D63
(build_hub_hull.py: colours, materials, seeds, timbers on a plane, export; build_hub_ends.py: boarded walls, windows,
balustrades, letters), so the inside does not change when the outside is rebuilt.
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import hub_timber as T  # noqa: E402
import sb_blender as sb  # noqa: E402

X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
ROW = 0.5                       # wall board width (build_hub_ends.ROW)


COLOURS = {
    "MI_Hub_HullTar": (0.16, 0.11, 0.08), "MI_Hub_HullTarFaded": (0.24, 0.18, 0.13), "MI_Hub_HullTarDark": (0.1, 0.07, 0.05),
    "MI_Hub_Ochre": (0.62, 0.43, 0.16), "MI_Hub_OchreFaded": (0.7, 0.55, 0.3), "MI_Hub_AncientStone": (0.84, 0.82, 0.74),
    "MI_Hub_GlassClear": (0.62, 0.76, 0.76), "MI_Hub_AncientGlowLow": (0.3, 0.75, 0.68),
    "MI_AncientGlow": (0.35, 0.95, 0.85), "MI_Caulk_Dark": (0.04, 0.035, 0.03), "MI_Wood_Tarred": (0.06, 0.05, 0.04),
    "MI_Paint_Cream": (0.86, 0.8, 0.64), "MI_Paint_CreamAged": (0.76, 0.68, 0.5), "MI_Metal_Iron": (0.08, 0.08, 0.08),
    "MI_Metal_Brass": (0.6, 0.45, 0.2), "MI_Glass_Dark": (0.12, 0.16, 0.17), "MI_LanternGlass": (1.0, 0.7, 0.35),
    "MI_Wood_Varnish": (0.36, 0.2, 0.08), "MI_Paint_Hull": (0.42, 0.11, 0.07), "MI_Paint_HullFaded": (0.5, 0.2, 0.13),
    "MI_Wood_Teak": (0.5, 0.33, 0.17), "MI_Wood_TeakDark": (0.38, 0.24, 0.12), "MI_Wood_TeakLight": (0.6, 0.42, 0.22),
    "MI_Rope_Hemp": (0.55, 0.45, 0.28), "MI_Hub_Void": (0.01, 0.01, 0.01),
}


def material(name):
    return sb.get_material(name, (*COLOURS.get(name, (0.5, 0.5, 0.5)), 1.0))


SEED = [9100]


def nseed():
    SEED[0] += 1
    return SEED[0]


def lpt(F, u, v, d):
    p, t, a, n = F
    return p + t * u + a * v + n * d


def lbeam(part, F, u0, v0, u1, v1, width, thick, d, mat, wear=0.8, **kw):
    """A timber on the hull's local plane F from (u0, v0) to (u1, v1), its back at depth d (outward from the surface)."""
    p, t, a, n = F
    pa, pb = lpt(F, u0, v0, d + thick * 0.5), lpt(F, u1, v1, d + thick * 0.5)
    if (pb - pa).length < 1e-3:
        return
    across = n.cross((pb - pa).normalized())
    part.beam(T.straight(pa, pb, across, n), (pb - pa).length, width, thick, mat, nseed(), wear=wear, **kw)


def export(part):
    objs = part.finish(material, ucx=False)
    sb.save_blend(part.name)
    sb.export_fbx(objs, os.path.join("Hub", part.name + ".fbx"))


def board_runs(part, F, u_ranges, v0, v1, d, mats, wear=(0.6, 1.0), seg=0.6, lmin=3.0, lmax=6.5, thick=0.12, grime=None):
    """Boards along the plane's t between v0 and v1 for each (u0, u1) range, with a dark core behind them."""
    rng = random.Random(nseed())
    p, t, a, n = F
    w = v1 - v0
    for u0, u1 in u_ranges:
        if u1 - u0 < 0.15:
            continue
        part.polygon([lpt(F, u0, v0, 0.0), lpt(F, u1, v0, 0.0), lpt(F, u1, v1, 0.0), lpt(F, u0, v1, 0.0)],
                     "MI_Caulk_Dark", tone=0.2, facing=n)
        u = u0
        first = True
        while u < u1 - 1e-6:
            ue = min(u + (rng.uniform(0.6, lmax) if first else rng.uniform(lmin, lmax)), u1)
            first = False
            if u1 - ue < 0.8:
                ue = u1
            pa, pb = lpt(F, u + 0.005, 0.5 * (v0 + v1), d + thick * 0.5), lpt(F, ue - 0.005, 0.5 * (v0 + v1), d + thick * 0.5)
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
    part.polygon([lpt(F, u, v, d) for u, v in outline], glass, tone=0.5, facing=n)
    pts = [lpt(F, u, v, d + 0.11) for u, v in outline] + [lpt(F, outline[0][0], outline[0][1], d + 0.11)]
    for pa, pb in zip(pts[:-1], pts[1:]):
        if (pb - pa).length < 1e-3:
            continue
        part.beam(T.straight(pa, pb, n.cross((pb - pa).normalized()), n), (pb - pa).length + 0.12, 0.16, 0.22, frame_mat, nseed(), seg=1.0, wear=0.8)
    nv, nh = panes
    for k in range(1, nv + 1):
        u = -hw_ + 2 * hw_ * k / (nv + 1)
        lbeam(part, F, u, -hh, u, hh - pointed * 0.3, 0.08, 0.08, d, frame_mat, wear=0.6)
    for k in range(1, nh + 1):
        v = -hh + (2 * hh - pointed) * k / (nh + 1)
        lbeam(part, F, -hw_, v, hw_, v, 0.07, 0.08, d, frame_mat, wear=0.6)
    lbeam(part, F, -hw_ - 0.22, -hh - 0.1, hw_ + 0.22, -hh - 0.1, 0.2, 0.26, d + 0.05, frame_mat)            # the sill
    if hood:
        lbeam(part, F, -hw_ - 0.28, hh + 0.2, hw_ + 0.28, hh + 0.2, 0.16, 0.26, d + 0.08, frame_mat)
    if kind == "shutter":
        for sgn in (-1.0, 1.0):
            for k in range(2):
                u = sgn * (hw_ * (k + 0.5) / 2)
                lbeam(part, F, u, -hh + 0.03, u, hh - 0.03 - pointed, hw_ / 2 - 0.015, 0.06, d + 0.12,
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



LETTERS = {
    "H": [((0, 0), (0, 1)), ((1, 0), (1, 1)), ((0, 0.52), (1, 0.52))],
    "O": "ring",
    "L": [((0, 1), (0, 0)), ((0, 0), (0.9, 0))],
    "W": [((0, 1), (0.24, 0)), ((0.24, 0), (0.5, 0.62)), ((0.5, 0.62), (0.76, 0)), ((0.76, 0), (1, 1))],
    "K": [((0, 0), (0, 1)), ((0.95, 1), (0.06, 0.42)), ((0.32, 0.6), (0.97, 0))],
    "E": [((0, 0), (0, 1)), ((0, 1), (0.88, 1)), ((0, 0.52), (0.7, 0.52)), ((0, 0), (0.9, 0))],
}


def wall_boards(part, F, u0, u1, v0, v1, keep=None, mats=("MI_Hub_HullTar", "MI_Hub_HullTarFaded", "MI_Hub_HullTarDark")):
    """A boarded wall on plane F (boards along t, rows up a), skipping where keep(u, v) is False (openings)."""
    v = v0
    while v < v1 - 0.05:
        vt = min(v + ROW, v1)
        rngs = intervals(lambda u: keep is None or (keep(u, v + 0.03) and keep(u, vt - 0.03)), u0, u1, 0.05)
        board_runs(part, (lpt(F, 0.0, 0.5 * (v + vt), 0.0), F[1], F[2], F[3]), rngs, -(vt - v) * 0.5, (vt - v) * 0.5, 0.0, mats,
                   lmin=2.5, lmax=6.0)
        v = vt
