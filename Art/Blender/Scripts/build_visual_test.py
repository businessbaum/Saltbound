"""Milestone 3 visual test assets (style benchmark, not final content).

  SM_TestDeckSection  4 m slice of the starter boat (3.4 m beam): teak deck, bulwark + cap rail,
                      oxblood hull, cream deckhouse with portholes and door, hatch, cleat, rope coil.
                      Pivot: deck surface, midships. Design waterline 0.75 m below the deck.
  SM_Lantern          hanging deck lantern (pivot: bottom centre; flame at z = 0.11 m)
  SM_DockSection      8 m harbour pier section (pivot: water level, centre; deck top at +1.10 m)
  SM_Rock_Coast_A/B/C chunky coastal rocks (pivot: water level)
  SM_Landmark_Spire   distant sea stack with a ruined tower and an ancient light band

Run: Tools/Build-BlenderAsset.ps1 build_visual_test [asset ...]
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402

M = {}


def mats():
    names = {
        "MI_Wood_Teak": (0.55, 0.35, 0.18), "MI_Wood_Varnish": (0.4, 0.2, 0.08), "MI_Caulk_Dark": (0.05, 0.05, 0.05),
        "MI_Paint_Cream": (0.9, 0.85, 0.7), "MI_Paint_Hull": (0.45, 0.12, 0.08), "MI_Paint_Boottop": (0.1, 0.1, 0.1),
        "MI_Metal_Brass": (0.7, 0.55, 0.25), "MI_Metal_Iron": (0.1, 0.1, 0.1), "MI_Canvas": (0.85, 0.8, 0.7),
        "MI_Rope_Hemp": (0.6, 0.5, 0.3), "MI_Glass_Dark": (0.1, 0.15, 0.18), "MI_LanternGlass": (1.0, 0.6, 0.2),
        "MI_CandleFlame": (1.0, 0.8, 0.3), "MI_Wood_DockWeathered": (0.5, 0.45, 0.4), "MI_Wood_Piling": (0.15, 0.12, 0.1),
        "MI_Rock_Coast": (0.4, 0.42, 0.42), "MI_Stone_Ruin": (0.55, 0.52, 0.48), "MI_AncientGlow": (0.4, 1.0, 0.9),
    }
    for n, c in names.items():
        M[n] = sb.get_material(n, (*c, 1))


def plank(parts, name, size, center, mat, rot=(0, 0, 0), bevel=0.004, segments=2):
    p = sb.add_box(name, size, center, M[mat])
    if any(rot):
        sb.transform_obj(p, rot, center)
    if bevel > 0:
        sb.soften(p, bevel, segments)
    parts.append(p)
    return p


def finish(parts, name, seed, waterline=None, wet=0.4, ucx=(), wear=1.0, jitter=0.0008):
    for p in parts:
        if not p.data.uv_layers or p.name.startswith(("deck", "plank", "caulk", "margin", "stanchion", "bulwark", "caprail",
                                                       "strake", "endcap", "cabin", "corner", "door", "trim", "roof",
                                                       "handrail", "coaming", "hatch", "hinge", "cleat_base", "joist",
                                                       "stringer", "brace", "ladder", "glass", "porthole_glass")):
            sb.world_box_uv(p, 1.0)
    rng = random.Random(seed * 101)
    for p in parts:
        sb.offset_uv(p, rng.uniform(0, 1), rng.uniform(0, 1))
    obj = sb.join(parts, name)
    if jitter:
        sb.jitter_vertices(obj, jitter, seed)
    sb.bake_vertex_masks(obj, wear=wear, waterline=waterline, wet_height=wet, seed=seed)
    cols = []
    for i, (size, center) in enumerate(ucx):
        cols.append(sb.add_ucx_box(name, i + 1, size, center))
    return obj, cols


def export(name, obj, cols, folder="VisualTest"):
    sb.export_fbx([obj] + list(cols), os.path.join(folder, name + ".fbx"))


# --------------------------------------------------------------------------------------------------
def deck_section():
    rng = random.Random(3)
    parts = []
    L = 4.0
    x0, x1 = -L / 2, L / 2
    # Deck planking (teak) over a dark caulking bed: narrow planks, butt joints at random positions.
    pw, seam, th = 0.08, 0.007, 0.03
    width = 2.90
    n = int(width / (pw + seam))
    ys = -(n * (pw + seam)) / 2 + pw / 2
    for i in range(n):
        y = ys + i * (pw + seam)
        split = rng.uniform(-1.3, 1.3)
        for xa, xb in ((x0, split), (split, x1)):
            plank(parts, "deck", (xb - xa - 0.004, pw, th), ((xa + xb) / 2, y, -th / 2 + rng.uniform(-0.0012, 0.0012)), "MI_Wood_Teak", bevel=0.003)
    plank(parts, "caulk", (L, width + 0.02, 0.02), (0, 0, -0.012), "MI_Caulk_Dark", bevel=0.0, segments=1)
    for s in (-1, 1):
        # Margin boards (varnished) and covering board.
        plank(parts, "margin", (L, 0.20, 0.035), (0, s * 1.555, -0.0175 + 0.003), "MI_Wood_Varnish", bevel=0.004)
        # Stanchions every 0.6 m, bulwark lining (cream inside), hull-paint skin outside, varnished cap rail.
        for k in range(7):
            x = x0 + 0.2 + k * 0.6
            plank(parts, "stanchion", (0.06, 0.06, 0.50), (x, s * 1.665, 0.25), "MI_Wood_Varnish", bevel=0.005)
        gangway = (0.25, 1.05) if s > 0 else None
        for z0, h in ((0.02, 0.22), (0.26, 0.22)):
            spans = [(x0, x1)] if gangway is None else [(x0, gangway[0]), (gangway[1], x1)]
            for xa, xb in spans:
                plank(parts, "bulwark_in", (xb - xa, 0.025, h - 0.006), ((xa + xb) / 2, s * 1.685, z0 + h / 2), "MI_Paint_Cream", bevel=0.003)
                plank(parts, "bulwark_out", (xb - xa, 0.012, h + 0.012), ((xa + xb) / 2, s * 1.704, z0 + h / 2), "MI_Paint_Hull", bevel=0.002)
        spans = [(x0, x1)] if gangway is None else [(x0, gangway[0] + 0.03), (gangway[1] - 0.03, x1)]
        for xa, xb in spans:
            plank(parts, "caprail", (xb - xa, 0.13, 0.045), ((xa + xb) / 2, s * 1.68, 0.5225), "MI_Wood_Varnish", bevel=0.008)
        # Hull strakes below the deck: convex section y = 1.70 - 0.55 t^2, z = -1.5 t.
        steps = 8
        for k in range(steps):
            t0, t1 = k / steps, (k + 1) / steps
            ya, za = 1.70 - 0.55 * t0 ** 2, -1.5 * t0
            yb, zb = 1.70 - 0.55 * t1 ** 2, -1.5 * t1
            zc = (za + zb) / 2
            mat = "MI_Paint_Cream" if k == 0 else ("MI_Paint_Boottop" if abs(zc + 0.75) < 0.1 else "MI_Paint_Hull")
            ln = math.hypot(yb - ya, zb - za)
            ang = math.atan2(ya - yb, za - zb)  # strake tilt from vertical
            p = plank(parts, "strake", (L, 0.035, ln - 0.004), (0, s * (ya + yb) / 2, zc), mat, bevel=0.004)
            sb.transform_obj(p, (s * ang, 0, 0), (0, s * (ya + yb) / 2, zc))
        # Section end caps (temporary: this is a slice of the boat).
    for xe in (x0 - 0.01, x1 + 0.01):
        plank(parts, "endcap", (0.02, 3.38, 1.55), (xe, 0, -0.75), "MI_Paint_Boottop", bevel=0.0, segments=1)

    # Deckhouse (aft part of the section): cream walls, varnished corner posts and trim, canvas roof.
    cx0, cx1, hw, wh = -1.9, -0.3, 0.72, 0.95
    for s in (-1, 1):
        plank(parts, "cabin_side", (cx1 - cx0, 0.04, wh), ((cx0 + cx1) / 2, s * hw, wh / 2), "MI_Paint_Cream", bevel=0.005)
        for x in (-1.5, -0.8):
            ring = sb.add_torus("porthole", 0.085, 0.016, (x, s * (hw + 0.02), 0.58), (math.pi / 2, 0, 0), 24, 8, M["MI_Metal_Brass"])
            parts.append(ring)
            plank(parts, "porthole_glass", (0.15, 0.01, 0.15), (x, s * (hw + 0.012), 0.58), "MI_Glass_Dark", bevel=0.0, segments=1)
        for x in (cx0, cx1):
            plank(parts, "corner_post", (0.07, 0.07, wh + 0.02), (x, s * hw, (wh + 0.02) / 2), "MI_Wood_Varnish", bevel=0.008)
    plank(parts, "cabin_back", (0.04, 2 * hw, wh), (cx0, 0, wh / 2), "MI_Paint_Cream", bevel=0.005)
    # Front wall with a door opening (0.62 wide, 0.86 high) and varnished frame + door.
    dw, dh = 0.62, 0.86
    side_w = hw - dw / 2
    for s in (-1, 1):
        plank(parts, "cabin_front", (0.04, side_w, wh), (cx1, s * (dw / 2 + side_w / 2), wh / 2), "MI_Paint_Cream", bevel=0.005)
        plank(parts, "door_jamb", (0.06, 0.05, dh), (cx1 + 0.01, s * (dw / 2 - 0.025), dh / 2), "MI_Wood_Varnish", bevel=0.006)
    plank(parts, "cabin_header", (0.04, dw, wh - dh), (cx1, 0, dh + (wh - dh) / 2), "MI_Paint_Cream", bevel=0.004)
    plank(parts, "door_head", (0.06, dw, 0.05), (cx1 + 0.01, 0, dh - 0.025), "MI_Wood_Varnish", bevel=0.006)
    for k in range(5):
        plank(parts, "door_board", (0.03, (dw - 0.1) / 5 - 0.004, dh - 0.07), (cx1 - 0.01, -dw / 2 + 0.05 + (k + 0.5) * (dw - 0.1) / 5, (dh - 0.07) / 2 + 0.01), "MI_Wood_Varnish", bevel=0.003)
    plank(parts, "door_sill", (0.12, dw + 0.06, 0.06), (cx1 + 0.03, 0, 0.03), "MI_Wood_Varnish", bevel=0.008)
    # Trim band and cambered canvas roof with handrails.
    for s in (-1, 1):
        plank(parts, "trim", (cx1 - cx0 + 0.1, 0.05, 0.07), ((cx0 + cx1) / 2, s * (hw + 0.005), wh - 0.035), "MI_Wood_Varnish", bevel=0.006)
    roof = sb.add_box("roof", (cx1 - cx0 + 0.16, 2 * hw + 0.16, 0.04), ((cx0 + cx1) / 2, 0, wh + 0.02), M["MI_Canvas"])
    for v in roof.data.vertices:
        v.co.z += 0.05 * (1.0 - (v.co.y / (hw + 0.08)) ** 2)
    sb.soften(roof, 0.006, 2)
    parts.append(roof)
    for s in (-1, 1):
        plank(parts, "handrail", (1.3, 0.035, 0.045), ((cx0 + cx1) / 2, s * 0.55, wh + 0.13), "MI_Wood_Varnish", bevel=0.008)
        for x in (-1.6, -1.1, -0.6):
            plank(parts, "handrail_foot", (0.06, 0.035, 0.07), (x, s * 0.55, wh + 0.075), "MI_Wood_Varnish", bevel=0.006)

    # Foredeck hatch: varnished coaming, teak lid with brass hinges.
    hx, hs = 1.2, 0.62
    for s in (-1, 1):
        plank(parts, "coaming", (hs, 0.05, 0.11), (hx, s * (hs / 2 - 0.025), 0.055), "MI_Wood_Varnish", bevel=0.006)
        plank(parts, "coaming", (0.05, hs - 0.1, 0.11), (hx + s * (hs / 2 - 0.025), 0, 0.055), "MI_Wood_Varnish", bevel=0.006)
    for k in range(6):
        plank(parts, "hatch_lid", (hs + 0.04, (hs + 0.04) / 6 - 0.006, 0.03), (hx, -(hs + 0.04) / 2 + (k + 0.5) * (hs + 0.04) / 6, 0.125), "MI_Wood_Teak", bevel=0.003)
    for y in (-0.18, 0.18):
        plank(parts, "hinge", (0.09, 0.05, 0.008), (hx - hs / 2 - 0.03, y, 0.143), "MI_Metal_Brass", bevel=0.002, segments=1)

    # Brass cleat on the starboard covering board and a coiled rope next to it.
    cl_x, cl_y = 1.65, -1.52
    plank(parts, "cleat_base", (0.09, 0.04, 0.04), (cl_x, cl_y, 0.02), "MI_Metal_Brass", bevel=0.008)
    parts.append(sb.add_cylinder("cleat_horn", 0.014, 0.24, (cl_x, cl_y, 0.05), "X", 10, M["MI_Metal_Brass"], radius_top=0.010))
    for k in range(4):
        r = 0.16 - k * 0.025
        parts.append(sb.add_torus("rope", r, 0.012, (1.2, -1.15, 0.013 + k * 0.004), (0, 0, k * 0.4), 32, 6, M["MI_Rope_Hemp"]))
    for k in range(3):
        parts.append(sb.add_torus("rope", 0.15, 0.012, (1.2, -1.15, 0.036 + k * 0.022), (0, 0, 1.0 + k), 32, 6, M["MI_Rope_Hemp"]))

    ucx = [
        ((L, 3.38, 0.06), (0, 0, -0.03)),                        # deck
        ((L, 0.10, 0.55), (0, -1.68, 0.275)),                    # starboard bulwark
        ((1.25, 0.10, 0.55), (x0 + 0.625, 1.68, 0.275)),         # port bulwark aft of the gangway
        ((x1 - 1.05, 0.10, 0.55), ((1.05 + x1) / 2, 1.68, 0.275)),
        ((cx1 - cx0 + 0.08, 2 * hw + 0.08, wh + 0.1), ((cx0 + cx1) / 2, 0, (wh + 0.1) / 2)),  # deckhouse
        ((hs + 0.04, hs + 0.04, 0.14), (hx, 0, 0.07)),            # hatch
    ]
    obj, cols = finish(parts, "SM_TestDeckSection", 3, waterline=-0.75, wet=0.12, ucx=ucx, wear=0.9)
    sb.save_blend("SM_TestDeckSection")
    export("SM_TestDeckSection", obj, cols)


def lantern():
    parts = []
    parts.append(sb.add_cylinder("base", 0.075, 0.025, (0, 0, 0.0125), "Z", 24, M["MI_Metal_Brass"]))
    parts.append(sb.add_cylinder("base_plate", 0.066, 0.012, (0, 0, 0.031), "Z", 24, M["MI_Metal_Iron"]))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(sb.add_cylinder("post", 0.0065, 0.2, (sx * 0.05, sy * 0.05, 0.135), "Z", 8, M["MI_Metal_Iron"]))
    for ax, s in (("x", 1), ("x", -1), ("y", 1), ("y", -1)):
        size = (0.004, 0.092, 0.188) if ax == "x" else (0.092, 0.004, 0.188)
        c = (s * 0.049, 0, 0.133) if ax == "x" else (0, s * 0.049, 0.133)
        plank(parts, "glass", size, c, "MI_LanternGlass", bevel=0.0, segments=1)
    parts.append(sb.add_cylinder("top_ring", 0.07, 0.014, (0, 0, 0.238), "Z", 24, M["MI_Metal_Brass"]))
    parts.append(sb.add_cylinder("cap", 0.07, 0.06, (0, 0, 0.275), "Z", 24, M["MI_Metal_Brass"], radius_top=0.028))
    parts.append(sb.add_cylinder("chimney", 0.02, 0.035, (0, 0, 0.32), "Z", 12, M["MI_Metal_Iron"]))
    parts.append(sb.add_torus("handle", 0.035, 0.005, (0, 0, 0.37), (math.pi / 2, 0, 0), 20, 6, M["MI_Metal_Iron"]))
    parts.append(sb.add_cylinder("candle", 0.017, 0.06, (0, 0, 0.067), "Z", 12, M["MI_Paint_Cream"]))
    parts.append(sb.add_cylinder("flame", 0.007, 0.028, (0, 0, 0.111), "Z", 8, M["MI_CandleFlame"], radius_top=0.0005))
    for p in parts:
        if p.name.startswith(("base", "top_ring", "cap")):
            sb.soften(p, 0.002, 1)
    obj, cols = finish(parts, "SM_Lantern", 5, ucx=[((0.15, 0.15, 0.36), (0, 0, 0.18))], jitter=0.0)
    sb.save_blend("SM_Lantern")
    export("SM_Lantern", obj, cols)


def dock_section():
    """8 m harbour pier section: weathered, hand-made timber (D20). Same footprint and deck height as
    before; every board, beam and post is an individual sb_timber piece with its own seed."""
    import sb_timber as tb
    rng = random.Random(9)
    parts = []
    L, W, top, T = 8.0, 2.4, 1.10, 0.065
    wood, pile = M["MI_Wood_DockWeathered"], M["MI_Wood_Piling"]
    piles_x = (-3.6, -1.2, 1.2, 3.6)
    post_y = 1.3                                   # posts stand just outside the deck edge
    seed = 100
    half_pi = math.pi / 2
    for x in piles_x:
        for y in (-post_y, post_y):
            seed += 1
            parts.append(tb.rough_post("post", 0.15, 3.85, (x + rng.uniform(-0.02, 0.02), y, top + 0.27 - 3.85),
                                       taper=0.88, material=pile, seed=seed))
        seed += 1
        # Joist across the dock, carried by the two posts.
        parts.append(tb.rough_plank("beam", W + 0.75, 0.22, 0.14, (x, 0, top - T - 0.2 - 0.11),
                                    (half_pi, 0, half_pi), wood, seed, wear=0.6, split_chance=0.1))
    for y in (-1.0, 0.0, 1.0):
        seed += 1
        parts.append(tb.rough_plank("beam", L, 0.2, 0.14, (0, y, top - T - 0.1), (half_pi, 0, 0), wood, seed,
                                    wear=0.6, split_chance=0.1))
    # Deck boards: chunky, varied width, length and placement; grain along each board.
    x = -L / 2
    while True:
        w = rng.uniform(0.2, 0.27)
        if x + w > L / 2 + 0.01:
            break
        seed += 1
        ln = W + rng.uniform(-0.1, 0.14)
        parts.append(tb.rough_plank("board", ln, w, T, (x + w / 2, rng.uniform(-0.05, 0.05), top - T / 2 + rng.uniform(-0.004, 0.002)),
                                    (math.radians(rng.uniform(-0.6, 0.6)), math.radians(rng.uniform(-0.4, 0.4)),
                                     half_pi + math.radians(rng.uniform(-1.2, 1.2))), wood, seed))
        x += w + rng.uniform(0.012, 0.026)
    # X-bracing on both long sides between the posts (boards on edge).
    for y in (-post_y - 0.12, post_y + 0.12):
        for a, b in zip(piles_x[:-1], piles_x[1:]):
            span = math.hypot(b - a, 1.2)
            ang = math.atan2(1.2, b - a)
            for sgn in (-1, 1):
                seed += 1
                parts.append(tb.rough_plank("beam", span, 0.15, 0.05, ((a + b) / 2, y + sgn * 0.03, -0.1),
                                            (half_pi, sgn * ang, 0), wood, seed, wear=0.7, split_chance=0.15))
    # Ladder on the +Y side.
    lx = -2.4
    for dx in (-0.22, 0.22):
        seed += 1
        parts.append(tb.rough_plank("beam", 2.2, 0.07, 0.07, (lx + dx, post_y + 0.08, 0.05), (0, -half_pi, 0), wood, seed, wear=0.5))
    for k in range(6):
        seed += 1
        parts.append(tb.rough_plank("beam", 0.5, 0.06, 0.045, (lx, post_y + 0.08, -0.8 + k * 0.32), (half_pi, 0, 0), wood, seed, wear=0.4))
    # Iron bollard near the end.
    parts.append(sb.add_cylinder("bollard", 0.1, 0.30, (3.4, 0.0, top + 0.15), "Z", 16, M["MI_Metal_Iron"]))
    parts.append(sb.add_cylinder("bollard_cap", 0.14, 0.06, (3.4, 0.0, top + 0.32), "Z", 16, M["MI_Metal_Iron"], radius_top=0.12))
    for p in parts[-2:]:
        sb.world_box_uv(p, 1.0)
    ucx = [((L + 0.1, W + 0.1, 0.12), (0, 0, top - 0.06)),
           ((0.4, 0.4, 0.4), (3.4, 0.0, top + 0.2))]
    for x in piles_x:
        for y in (-post_y, post_y):
            ucx.append(((0.34, 0.34, 3.8), (x, y, top + 0.27 - 1.9)))
    obj, cols = finish(parts, "SM_DockSection", 9, waterline=0.0, wet=0.55, ucx=ucx, wear=0.6, jitter=0.0)
    print("SB_DOCK faces", len(obj.data.polygons), "parts", len(parts))
    sb.save_blend("SM_DockSection")
    export("SM_DockSection", obj, cols)


ROCKS = (
    # name, size (m), seed, chunks, planar cuts, face budget
    ("SM_Rock_Coast_A", (9.0, 6.5, 6.0), 11, 6, 16, 40000),
    ("SM_Rock_Coast_B", (4.0, 3.2, 2.6), 12, 4, 13, 24000),
    ("SM_Rock_Coast_C", (1.6, 1.3, 1.0), 13, 3, 10, 12000),
    ("SM_Rock_Scatter_A", (0.7, 0.55, 0.38), 14, 2, 8, 4000),
    ("SM_Rock_Scatter_B", (0.38, 0.3, 0.22), 15, 2, 7, 2000),
)


def rocks():
    import sb_rock
    for name, size, seed, chunks, cuts, faces in ROCKS:
        sb.reset_scene()
        mats()
        bottom = -2.5 if size[0] > 1.0 else -0.3
        r = sb_rock.stylised_rock(name, size, seed, chunks=chunks, cuts=cuts, bottom=bottom, target_faces=faces,
                                  material=M["MI_Rock_Coast"])
        print("SB_ROCK", name, len(r.data.polygons))
        sb.save_blend(name)
        sb.export_fbx([r], os.path.join("VisualTest", name + ".fbx"))


def landmark():
    """Sea stack (soft-faceted rock) crowned by a ruined round tower built from bevelled stone blocks:
    courses offset by half a block, a jagged broken crown, one window opening and the ancient glow band."""
    import sb_rock
    sb.reset_scene()
    mats()
    stack = sb_rock.stylised_rock("SM_Landmark_Spire", (14.0, 11.0, 30.0), 21, chunks=5, cuts=14, bottom=-6.0,
                                  target_faces=40000, material=M["MI_Rock_Coast"])
    rng = random.Random(22)
    cx, cy, radius = 0.4, 0.2, 2.2
    near = [v.co.z for v in stack.data.vertices if math.hypot(v.co.x - cx, v.co.y - cy) < radius + 0.4]
    base = (max(near) if near else max(v.co.z for v in stack.data.vertices)) - 0.9
    course_h, depth, per_ring, rings = 0.5, 0.45, 16, 13
    blocks = []
    for k in range(rings):
        z = base + k * course_h + course_h / 2
        offset = (0.5 if k % 2 else 0.0) * 2 * math.pi / per_ring
        for j in range(per_ring):
            a = offset + j * 2 * math.pi / per_ring
            # Broken crown: each column survives up to a jagged height.
            crown = 7.5 + 3.5 * (0.5 + 0.5 * math.sin(3 * a + 1.0)) + rng.uniform(-1.2, 1.2)
            if k > crown:
                continue
            if 3 <= k <= 5 and abs(math.atan2(math.sin(a - 0.6), math.cos(a - 0.6))) < 0.3:
                continue  # window opening, facing the cove
            arc = 2 * math.pi * radius / per_ring - 0.03
            r = radius + rng.uniform(-0.03, 0.03)
            b = sb.add_box("block", (depth, arc, course_h - 0.03), (r, 0, 0), M["MI_Stone_Ruin"])
            sb.transform_obj(b, (rng.uniform(-0.02, 0.02), rng.uniform(-0.02, 0.02), a + rng.uniform(-0.015, 0.015)))
            for v in b.data.vertices:
                v.co.x += cx
                v.co.y += cy
                v.co.z += z
            sb.soften(b, 0.035, 2)
            blocks.append(b)
    # Inner core closes the shell (seen through the window as a dark recess).
    core = sb.add_cylinder("core", radius - depth / 2 - 0.05, 5 * course_h, (cx, cy, base + 2.5 * course_h), "Z", 24, M["MI_Stone_Ruin"])
    blocks.append(core)
    blocks.append(sb.add_torus("ancient_band", radius + 0.27, 0.09, (cx, cy, base + 2 * course_h), (0, 0, 0), 64, 6, M["MI_AncientGlow"]))
    for b in blocks:
        sb.world_box_uv(b, 1.0)
    tower = sb.join(blocks, "tower")
    sb.bake_vertex_masks(tower, wear=1.0, seed=23, bevel_max=0.06)
    obj = sb.join([stack, tower], "SM_Landmark_Spire")
    print("SB_LANDMARK faces", len(obj.data.polygons))
    sb.save_blend("SM_Landmark_Spire")
    sb.export_fbx([obj], os.path.join("VisualTest", "SM_Landmark_Spire.fbx"))


BUILDERS = {"deck": deck_section, "lantern": lantern, "dock": dock_section, "rocks": rocks, "landmark": landmark}

if __name__ == "__main__":
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        if key in ("deck", "lantern", "dock"):
            sb.reset_scene()
            mats()
        BUILDERS[key]()
    print("SB_VISUAL_TEST_ASSETS_OK", wanted)
