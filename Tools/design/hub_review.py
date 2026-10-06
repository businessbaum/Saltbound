"""The hub ship's layout for review (D64): plans of every floor, the side, a section through the Water Hall, the stern.

    python Tools/design/hub_review.py            -> Docs/Design/HubShip/HubShip_D64_*.png

Drawn from the data the build uses (HubShip_Data.json via hub_layout.py, the rooms via hub_plan.py), so what the owner reviews is
what gets built. A review drawing, not game content.
"""
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.patches import Polygon, Rectangle  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Art", "Blender", "Scripts"))
import hub_layout as H  # noqa: E402
import hub_plan as P  # noqa: E402

OUT = os.path.join(ROOT, "Docs", "Design", "HubShip")
BG = "#f4efe4"
INK = "#3b2f25"
OPEN_COLOURS = {"dock": "#9fd0c0", "dock_g": "#9fd0c0", "slip": "#e6c27a", "keelgate": "#b9b0e0", "tally": "#e7a98f", "lane": "#e7c3b3",
                "well_l": "#c8d9a6", "well_m": "#c8d9a6", "well_u": "#c8d9a6", "well_g": "#c8d9a6", "deck": "#d9cfae", "chandlery": "#e5b3c8"}
WATER = "#5f9fc0"
CORR = "#d5ccbb"
AIR = "#e9e2cc"
VOID = "#ffffff"
ROOM_A, ROOM_B = "#efe7d6", "#e6dcc6"
NAMES = {o["id"]: o["name"] for o in H.OPEN}
LOCKED_NAMES = {z["id"]: z["name"] for z in H.LOCKED}


def rgb(h):
    h = h.lstrip("#")
    return [int(h[k:k + 2], 16) / 255.0 for k in (0, 2, 4)]


def floor_image(p):
    img = np.ones((P.NY, P.NX, 3))
    img[:, :] = rgb(BG)
    for (i, j) in p.void:
        img[j, i] = rgb(VOID)
    for (i, j), l in p.lab.items():
        if l[0] == "water":
            c = WATER
        elif l[0] == "open":
            c = OPEN_COLOURS.get(l[1], "#cccccc")
        elif l[0] == "corr":
            c = CORR
        elif l[0] == "air":
            c = AIR
        else:
            c = ROOM_A if l[1] % 2 else ROOM_B
        img[j, i] = rgb(c)
    return img


def draw_floor(ax, level_id, title):
    p = P.plan(level_id)
    ax.imshow(floor_image(p), origin="lower", extent=(P.X0, P.X1, P.Y0, P.Y1), interpolation="nearest")
    # The hull at this floor.
    xs = np.linspace(-2, 162, 400)
    hw = [H.half_width(x, p.z + 0.1) for x in xs]
    ax.plot(xs, hw, color=INK, lw=1.4)
    ax.plot(xs, [-w for w in hw], color=INK, lw=1.4)
    for w in p.walls:
        kind = w["kind"]
        style = {"wall": dict(color=INK, lw=0.9), "hull": dict(color=INK, lw=1.6), "rail": dict(color="#7a5a3a", lw=1.0, ls=(0, (2, 1))),
                 "bulwark": dict(color="#7a5a3a", lw=1.6), "edge": dict(color=INK, lw=0.9)}[kind]
        if w["axis"] == "x":
            ax.plot([w["line"], w["line"]], [w["a0"], w["a1"]], solid_capstyle="butt", **style)
        else:
            ax.plot([w["a0"], w["a1"]], [w["line"], w["line"]], solid_capstyle="butt", **style)
    for d in p.doors:
        hw_ = d["w"] / 2
        col = "#2e7d32" if d["kind"] == "link" else "#fbf7ee"
        if d["axis"] == "x":
            ax.plot([d["x"], d["x"]], [d["y"] - hw_, d["y"] + hw_], color=col, lw=2.2, solid_capstyle="butt")
        else:
            ax.plot([d["x"] - hw_, d["x"] + hw_], [d["y"], d["y"]], color=col, lw=2.2, solid_capstyle="butt")
    gates = [g for g in P.all_gate_marks() if g["level"] == level_id]
    for g in gates:
        ax.plot(g["x"], g["y"], marker="v", ms=6, color="#c62828", mec="white", mew=0.6)
        ax.text(g["x"], g["y"] + 0.9, g["g"], fontsize=5.5, color="#c62828", ha="center", va="bottom")
    # Names: open spaces and locked zones at their middle.
    seen = {}
    for (i, j), l in p.lab.items():
        if l[0] in ("open", "corr", "air") or l[0] == "room":
            key = ("open", l[1]) if l[0] == "open" else ("zone", p.rooms[l[1]].zone if l[0] == "room" else l[1])
            seen.setdefault(key, []).append((P.cx(i), P.cy(j)))
    for (kind, k), pts in seen.items():
        if len(pts) < 20:
            continue
        x = sum(q[0] for q in pts) / len(pts)
        y = sum(q[1] for q in pts) / len(pts)
        if kind == "open":
            ax.text(x, y, NAMES.get(k, k), fontsize=7, ha="center", va="center", color="#1d1d1d", weight="bold",
                    bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none", alpha=0.7))
        else:
            ax.text(x, y, "%s %s" % (k, LOCKED_NAMES.get(k, "")), fontsize=6, ha="center", va="center", color="#6d5d4b", style="italic")
    if level_id == "quay":
        dr = H.HUB["doors"]
        for s in (-1, 1):
            ax.add_patch(Rectangle((dr["x0"], s * 19.3 - 0.4), dr["x1"] - dr["x0"], 0.8, fc="#2e7d32", ec="none"))
            for hx in (dr["x0"], dr["x1"]):                                   # the leaves open
                ax.plot([hx, hx], [s * 19.0, s * 15.0], color="#2e7d32", lw=2.4)
        ax.text(36, 20.6, "hidden doors (both sides)", fontsize=6.5, color="#2e7d32", ha="center")
        for b in [H.prop("dock", "fitting_berth")] + [dict(bb, heading=-90) for bb in H.prop("dock", "fleet_berths")["berths"]]:
            boat(ax, b["x"], 0.5 * (b["y0"] + b["y1"]), b.get("heading", -90), 10.0 if "10" in b.get("boat", "10 m") else 14.0)
    ax.set_xlim(-4, 164)
    ax.set_ylim(-22, 22)
    ax.set_aspect("equal")
    ax.set_xticks(range(0, 161, 10))
    ax.set_yticks(range(-20, 21, 10))
    ax.tick_params(labelsize=6)
    rooms = len(p.rooms)
    ax.set_title("%s (z %+.1f)   rooms %d   gates %d" % (title, p.z, rooms, len(gates)), fontsize=9, loc="left", color=INK)


def boat(ax, x, y, heading, length):
    """A boat in plan: a pointed hull length x 3.4 m."""
    a = math.radians(heading)
    fx, fy = math.cos(a), math.sin(a)
    rx, ry = -fy, fx
    hl, hb = length / 2, 1.7
    pts = [(x + fx * hl, y + fy * hl), (x + fx * hl * 0.4 + rx * hb, y + fy * hl * 0.4 + ry * hb), (x - fx * hl * 0.8 + rx * hb * 0.9, y - fy * hl * 0.8 + ry * hb * 0.9),
           (x - fx * hl, y - fy * hl), (x - fx * hl * 0.8 - rx * hb * 0.9, y - fy * hl * 0.8 - ry * hb * 0.9), (x + fx * hl * 0.4 - rx * hb, y + fy * hl * 0.4 - ry * hb)]
    ax.add_patch(Polygon(pts, closed=True, fc="#8a3324", ec=INK, lw=0.6))


def plans():
    groups = [("HubShip_D64_plans_low", [("deep", "The Deep"), ("hold", "Hold"), ("quay", "Dock deck: the Water Hall"), ("lower", "Lower deck")]),
              ("HubShip_D64_plans_mid", [("middle", "Middle deck"), ("upper", "Upper deck"), ("gun", "Gun deck"), ("main", "Main deck")]),
              ("HubShip_D64_plans_castles", [("sc1", "Sterncastle, first storey (and forecastle)"), ("sc2", "Sterncastle, second storey; quarterdeck"),
                                              ("sc3", "Sterncastle, third storey"), ("poop", "Poop deck")])]
    for name, floors in groups:
        fig, axes = plt.subplots(len(floors), 1, figsize=(17, 4.1 * len(floors)), facecolor="white")
        for ax, (lv, title) in zip(axes, floors):
            draw_floor(ax, lv, title)
            if lv == "sc1":
                p = P.plan("fc1")
                ax.imshow(np.where(np.array([[[1 if (i, j) in p.lab else 0] for i in range(P.NX)] for j in range(P.NY)]) > 0,
                                   floor_image(p), np.nan), origin="lower", extent=(P.X0, P.X1, P.Y0, P.Y1), interpolation="nearest")
        fig.suptitle("Hollowkeel, D64 layout: corridors and rooms on every floor (sand), open spaces coloured, water blue, gates red", fontsize=11,
                     color=INK, x=0.01, ha="left")
        fig.tight_layout(rect=(0, 0, 1, 0.97))
        path = os.path.join(OUT, name + ".png")
        fig.savefig(path, dpi=110)
        plt.close(fig)
        print("SB_REVIEW", path)


def side_and_sections():
    fig = plt.figure(figsize=(17, 13), facecolor="white")
    # Side profile.
    ax = fig.add_axes((0.04, 0.55, 0.92, 0.42))
    prof = H.HUB["profile"]
    ax.add_patch(Polygon(prof, closed=True, fc="#4a3426", ec=INK, lw=1.2))
    ax.axhline(0, color=WATER, lw=1.5)
    ax.fill_between([-30, 200], [-14, -14], [0, 0], color=WATER, alpha=0.18)
    for lv in H.LEVELS:
        z = H.level_z(lv)
        xs = [x for x in np.arange(0, 162, 0.5) if H.half_width(x, z + 0.1) > 1.0 and H.profile_bottom(x) < z < H.profile_top(x) + 0.01]
        only = [H.R(r) for r in H.LEVELS[lv].get("only", [])]
        if only:
            xs = [x for x in xs if any(r[0] <= x < r[1] for r in only)]
        if xs:
            ax.plot([min(xs), max(xs)], [z, z], color="#e8d7b0", lw=0.8)
            ax.text(max(xs) + 0.5 if lv not in ("sc1", "sc2", "sc3", "poop") else min(xs) - 0.5, z + 0.3, lv, fontsize=6, color="#e8d7b0",
                    ha="left" if lv not in ("sc1", "sc2", "sc3", "poop") else "right")
    dr = H.HUB["doors"]
    ax.add_patch(Rectangle((dr["x0"], dr["z0"]), dr["x1"] - dr["x0"], dr["z1"] - dr["z0"], fc="none", ec="#7fd18a", lw=1.2, ls="--"))
    ax.text(36, dr["z1"] + 0.6, "hidden doors (shut: only a seam)", fontsize=7, color="#7fd18a", ha="center")
    dk = H.HUB["dock"]
    ax.add_patch(Rectangle((dk["x0"], dk["floor"]), dk["x1"] - dk["x0"], H.Z["gun"] - 0.3 - dk["floor"], fc="none", ec="#9fd0c0", lw=0.8, ls=":"))
    ax.text(38, -3.2, "Water Hall", fontsize=7, color="#9fd0c0", ha="center")
    sh = H.HUB["shaft"]
    ax.add_patch(Rectangle((sh["x0"], H.Z["quay"]), sh["x1"] - sh["x0"], H.Z["main"] - H.Z["quay"], fc="none", ec="#c8d9a6", lw=0.8, ls=":"))
    ax.text(84, 2.2, "Well", fontsize=7, color="#c8d9a6", ha="center")
    for m in H.HUB["masts"]:
        ax.plot([m["x"], m["x"]], [H.Z["main"], H.Z["main"] + m["h"] * 0.42], color="#6b4f37", lw=3)
    # The rudder, and a boat at the doors with her mast up.
    rd = H.HUB["rudder"]
    ax.add_patch(Polygon([(rd["x0"], rd["z0"]), (rd["x1"], rd["z0"]), (rd["x1"], rd["z1"]), (rd["x1"] - rd["head"], rd["z1"])], fc="#5a4030", ec=INK, lw=0.8))
    ax.set_xlim(-28, 192)
    ax.set_ylim(-14, 50)
    ax.set_aspect("equal")
    ax.tick_params(labelsize=6)
    ax.set_title("Side (starboard): 3 m decks, the sterncastle in two tiers, the hidden doors at x 32..40 (masts cut off at 42 %)", fontsize=9, loc="left")
    # Section through the Water Hall at x 36, looking forward.
    ax = fig.add_axes((0.04, 0.05, 0.44, 0.45))
    zs = np.linspace(-12, H.profile_top(36), 120)
    hw = [H.half_width(36, z) for z in zs]
    pts = [(w, z) for w, z in zip(hw, zs)] + [(-w, z) for w, z in zip(reversed(hw), reversed(zs))]
    ax.add_patch(Polygon(pts, closed=True, fc="#4a3426", ec=INK, lw=1.2))
    inner = [(max(w - 0.6, 0), z) for w, z in zip(hw, zs)] + [(-max(w - 0.6, 0), z) for w, z in zip(reversed(hw), reversed(zs))]
    ax.add_patch(Polygon(inner, closed=True, fc="#e9e2cc", ec="none"))
    ax.add_patch(Rectangle((-21, -4), 42, 4, fc=WATER, ec="none", alpha=0.85))
    for s in (-1, 1):
        ax.add_patch(Rectangle((s * 18.4 - 1.2, dr["z0"]), 2.4, dr["z1"] - dr["z0"], fc=BG, ec="#2e7d32", lw=1.2))
    for lv in ("gun", "main"):
        z = H.level_z(lv)
        w = H.floor_half(lv, 36)
        ax.add_patch(Rectangle((-w, z - 0.3), 2 * w, 0.3, fc="#8a6a4a", ec="none"))
    for lv in ("lower", "middle", "upper"):                                     # the galleries behind (x 26..30, 46..50)
        z = H.level_z(lv)
        w = H.floor_half(lv, 36)
        ax.plot([-w, w], [z, z], color="#8a6a4a", lw=0.8, ls="--")
    ax.text(0, H.Z["upper"] + 0.4, "galleries over the quays (behind)", fontsize=6.5, ha="center", color="#6d5d4b")
    # The first boat end-on, mast up: 3.4 m beam, sheer 0.75 m over the water, mast top 10.9 m.
    ax.add_patch(Polygon([(-1.7, 0.75), (1.7, 0.75), (1.2, -0.3), (0, -1.4), (-1.2, -0.3)], fc="#8a3324", ec=INK, lw=0.8))
    ax.plot([0, 0], [0.75, 10.9], color="#6b4f37", lw=2)
    ax.plot([-3.2, 3.2], [8.2, 8.2], color="#6b4f37", lw=1.5)
    ax.text(3.6, 10.9, "the first boat's mast top 10.9 m", fontsize=6.5, va="center")
    ax.axhline(H.Z["gun"] - 0.3, color="#c62828", lw=0.6, ls=":")
    ax.text(-20.5, H.Z["gun"] + 0.1, "clear to 13.2 m", fontsize=6.5, color="#c62828")
    ax.set_xlim(-24, 24)
    ax.set_ylim(-13, 21)
    ax.set_aspect("equal")
    ax.tick_params(labelsize=6)
    ax.set_title("Section through the Water Hall at x 36 (doors open in both sides)", fontsize=9, loc="left")
    # The stern from aft: the widest the hull is at each height over its last 6 m.
    ax = fig.add_axes((0.53, 0.05, 0.43, 0.45))
    zs = np.linspace(-12, H.profile_top(0.5), 160)
    hw = [max(H.half_width(x, z) for x in np.linspace(0.0, 6.0, 25)) for z in zs]
    pts = [(w, z) for w, z in zip(hw, zs)] + [(-w, z) for w, z in zip(reversed(hw), reversed(zs))]
    ax.add_patch(Polygon(pts, closed=True, fc="#4a3426", ec=INK, lw=1.2))
    gl = H.HUB["galleries"]
    for lv in gl["levels"]:
        z = H.level_z(lv)
        w = H.half_width(4, z)
        ax.add_patch(Rectangle((-w, z - 0.25), 2 * w, 0.25, fc="#b07a3a", ec="none"))
        ax.plot([-w, w], [z + 1.0, z + 1.0], color="#e8d7b0", lw=1.0)
        for k in range(-int(w), int(w) + 1, 2):
            ax.plot([k, k], [z, z + 1.0], color="#e8d7b0", lw=0.5)
    q = gl.get("quarter")                       # D69: the quarter galleries are gone (the galleries run along the sides)
    for s in ((-1, 1) if q else ()):
        w = H.half_width(4, q["z0"])
        ax.add_patch(Rectangle((s * w if s > 0 else -w - q["out"], q["z0"]), q["out"], q["z1"] - q["z0"], fc="#5b4030", ec="#b07a3a", lw=1.0))
    for lv in ("sc1", "sc2", "sc3"):
        z = H.level_z(lv)
        w = H.half_width(4, z) - 2.0
        for k in np.arange(-w, w + 0.1, 2.8):
            ax.add_patch(Rectangle((k - 0.7, z + 0.6), 1.4, 1.9, fc="#f2d39b", ec="#e8d7b0", lw=0.5))
    rd = H.HUB["rudder"]
    ax.add_patch(Rectangle((-0.35, rd["z0"]), 0.7, rd["z1"] - rd["z0"], fc="#5a4030", ec=INK, lw=0.6))
    ax.axhline(0, color=WATER, lw=1.5)
    ax.set_xlim(-24, 24)
    ax.set_ylim(-13, 33)
    ax.set_aspect("equal")
    ax.tick_params(labelsize=6)
    ax.set_title("The stern from aft: narrow to the rudder below the counter (8.5 m), full and round above, the galleries", fontsize=8.5, loc="left")
    path = os.path.join(OUT, "HubShip_D64_side_sections.png")
    fig.savefig(path, dpi=110)
    plt.close(fig)
    print("SB_REVIEW", path)


if __name__ == "__main__":
    plans()
    side_and_sections()
