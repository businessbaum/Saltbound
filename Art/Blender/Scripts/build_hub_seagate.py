"""The hub ship's sea gate (D62; owner: "open and close it with ropes like a Castel Gate"; hub.seagate in the data).

A portcullis of tarred oak in grooves just inside the stern face: posts with iron-shod points, rails on both faces bolted at
the crossings, a heavy head beam with four iron eyes. Four ropes run from the eyes up into a slot in the gatehouse over the
arch, over sheaves, and down through the wings to a capstan on each quay by the mouth. ASaltSeaGate moves the gate, pays
the ropes in and out and turns the capstans; E at a capstan raises or lowers it.

Meshes (/Game/Saltbound/Hub/Art):
  SM_Hub_SeaGate       the portcullis; pivot at its bottom centre (its spikes' tips), +X forward; one UCX box
  SM_Hub_GateRope      one rope, 20 m hanging down from its pivot (the top); the actor scales it to the length shown
  SM_Hub_Capstan       a capstan's turning part (drum, whelps, bars, the rope's turns); pivot on the deck at its axis
  SM_Hub_SeaGateFrame  what stays: the grooves' timbers on the mouth's walls, the sill on the basin's floor, the gatehouse
                       slot, the capstans' bases and pawl rings, the blocks under the arcade roof and the ropes up to them;
                       ship pivot (x 80), UCX boxes for the grooves (the boat must not pass through them)
Run: Tools/Build-BlenderAsset.ps1 build_hub_seagate
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import build_hub_ends as HE  # noqa: E402
import build_hub_hull as HH  # noqa: E402
import hub_layout as H  # noqa: E402
import hub_timber as T  # noqa: E402
import sb_blender as sb  # noqa: E402

HUB, Z = H.HUB, H.Z
SG = HUB["seagate"]
X, Y, UP = Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0))
nseed = HH.nseed
ROPE_R = 0.11
ROPE_LEN = 27.0           # from under its sheave (z 30.4) to the closed gate's head (z 4)
CAP_BAR_R = 1.3            # capstan bars' reach from the axis
CAP_BAR_Z = 0.95           # bars at the 1.4 m player's waist
CAP_SPOT_Z = Z["quay"]


def export(part, pivot):
    objs = part.finish(HH.material, ucx=bool(part.ucx), pivot=pivot)
    sb.save_blend(part.name)
    sb.export_fbx(objs, os.path.join("Hub", part.name + ".fbx"))


def grime_low(z_water):
    return lambda q: min(max((z_water + 0.8 - q.z) / 1.6, 0.0), 1.0)


def build_gate():
    """The portcullis built where it stands closed (ship metres); exported with its pivot at the bottom centre."""
    p = T.Part("SM_Hub_SeaGate")
    x, hw, z0, h = SG["x"], SG["halfW"], SG["z0"], SG["h"]
    t = SG["thick"]
    g = grime_low(0.0)
    n_posts = 15
    ys = [-hw + 0.25 + (2 * hw - 0.5) * k / (n_posts - 1) for k in range(n_posts)]
    for k, y in enumerate(ys):
        # Each post: oak, its foot shod with an iron point.
        p.beam(T.straight((x, y, z0 + 0.62), (x, y, z0 + h - 0.6), X, Y), h - 1.22, 0.38, 0.38,
               "MI_Hub_HullTarDark" if k % 3 else "MI_Hub_HullTar", nseed(), seg=0.8, wear=1.0, grime=g)
        p.lathe([(0.0, 0.0), (0.06, 0.08), (0.2, 0.42), (0.22, 0.62), (0.2, 0.72), (0.0, 0.72)], Vector((x, y, z0)), UP,
                "MI_Metal_Iron", sides=4, wear=0.8, grime=g)
    rails = [z0 + 1.3, z0 + 2.8, z0 + 4.3, z0 + 5.8, z0 + 7.3]
    for j, zr in enumerate(rails):
        for face in (-1.0, 1.0) if j % 2 == 0 else (1.0,):
            xx = x + face * (t * 0.5 - 0.13) + (0.0 if j % 2 == 0 else -t * 0.5 + 0.13)
            p.beam(T.straight((xx, -hw + 0.05, zr), (xx, hw - 0.05, zr), UP, X * face), 2 * hw - 0.1, 0.3, 0.2,
                   "MI_Hub_HullTar", nseed(), seg=1.0, wear=1.0, grime=g)
        for y in ys if j % 2 == 0 else ():                               # iron plates and bolts at the crossings
            for face in (-1.0, 1.0):
                c = Vector((x + face * 0.29, y, zr))
                p.block(c.x - 0.02, c.x + 0.02, y - 0.17, y + 0.17, zr - 0.17, zr + 0.17, "MI_Metal_Iron", nseed(), axis="y", grime=g)
    # The head beam with its iron eyes for the ropes; the side stiles that run in the grooves.
    p.beam(T.straight((x, -hw, z0 + h - 0.35), (x, hw, z0 + h - 0.35), UP, X), 2 * hw, 0.7, t, "MI_Hub_HullTarDark", nseed(), seg=1.0, wear=1.0)
    for y in SG["ropes"]:
        c = Vector((x, y, z0 + h + 0.1))
        p.beam(T.arc(c, Y, UP, 0.22, 0.0, math.pi), 0.7, 0.07, 0.08, "MI_Metal_Iron", nseed(), seg=0.1)
        p.block(x - 0.12, x + 0.12, y - 0.3, y + 0.3, z0 + h - 0.05, z0 + h + 0.1, "MI_Metal_Iron", nseed(), axis="y")
    for sgn in (-1.0, 1.0):
        p.beam(T.straight((x, sgn * (hw - 0.12), z0 + 0.4), (x, sgn * (hw - 0.12), z0 + h), X, Y), h - 0.4, t, 0.26,
               "MI_Hub_HullTarDark", nseed(), seg=0.8, grime=g)
    p.collide_box(x - t * 0.5, x + t * 0.5, -hw, hw, z0, z0 + h)
    export(p, (x, 0.0, z0))


def build_rope():
    p = T.Part("SM_Hub_GateRope")
    p.round(T.straight((0.0, 0.0, 0.0), (0.0, 0.0, -ROPE_LEN), X, Y), ROPE_LEN, ROPE_R, "MI_Rope_Hemp", nseed(), sides=8, seg=1.0, wear=0.0)
    export(p, (0.0, 0.0, 0.0))


def build_capstan():
    """The turning part, built at the origin (ship frame without offset; exported with pivot at the origin)."""
    p = T.Part("SM_Hub_Capstan")
    o = Vector((0.0, 0.0, 0.0))
    # Drum: a waisted barrel with whelps (vertical ribs the rope rides on), a head with bar holes, a cap.
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
    # Collision: the drum, and the bars' sweep (so nobody walks through them).
    p.collide_box(-0.62, 0.62, -0.62, 0.62, 0.0, 1.26)
    p.collide_points([Vector((math.cos(2 * math.pi * k / 8) * CAP_BAR_R, math.sin(2 * math.pi * k / 8) * CAP_BAR_R, z))
                      for k in range(8) for z in (CAP_BAR_Z - 0.1, CAP_BAR_Z + 0.1)])
    export(p, (0.0, 0.0, 0.0))


def build_frame():
    p = T.Part("SM_Hub_SeaGateFrame")
    x, hw, z0 = SG["x"], SG["halfW"], SG["z0"]
    po = SG["posts"]
    top = Z["sc1"]
    g = grime_low(0.0)
    for side in (1.0, -1.0):
        # The groove: two heavy timbers on the mouth's wall either side of the gate, iron-faced where it runs.
        for xa, xb in ((po["x0"], x - SG["thick"] * 0.5 - 0.04), (x + SG["thick"] * 0.5 + 0.04, po["x1"])):
            ya, yb = sorted((side * po["y"], side * (HUB["basin"]["y1"] + 0.3)))
            p.block(xa, xb, ya, yb, z0 - 0.3, top, "MI_Hub_HullTarDark", nseed(), axis="z", grime=g, collide=True)
            xf = xb if xa < x else xa
            p.block(xf - 0.03, xf + 0.03, ya + 0.05, yb - 0.05, z0, top - 0.3, "MI_Metal_Iron", nseed(), axis="z", grime=g)
        ya, yb = sorted((side * (hw - 0.02), side * (HUB["basin"]["y1"] + 0.3)))
        p.polygon([(x - 0.33, ya, z0), (x + 0.33, ya, z0), (x + 0.33, ya, top), (x - 0.33, ya, top)][::1], "MI_Hub_Void",
                  tone=0.1, facing=(0.0, -side, 0.0))
        # A capstan's base: a timber pad with an iron pawl ring; the block under the arcade roof and the rope up to it.
        cx, cy = SG["capstans"][0][0], side * abs(SG["capstans"][0][1])
        c = Vector((cx, cy, CAP_SPOT_Z))
        for k in range(4):
            a = math.pi * k / 4
            d = Vector((math.cos(a), math.sin(a), 0.0))
            p.beam(T.straight(c - d * 1.0 + UP * 0.06, c + d * 1.0 + UP * 0.06, d.cross(UP), UP), 2.0, 0.3, 0.12, "MI_Wood_TeakDark", nseed(), seg=0.5)
        p.lathe([(0.66, 0.0), (0.78, 0.0), (0.78, 0.12), (0.66, 0.12)], c + UP * 0.1, UP, "MI_Metal_Iron", sides=16, caps=False)
        blk = Vector((cx + 0.2, cy, Z["lower"] - 0.45))
        p.block(blk.x - 0.3, blk.x + 0.3, blk.y - 0.18, blk.y + 0.18, blk.z - 0.35, blk.z + 0.35, "MI_Wood_Varnish", nseed(), axis="z")
        p.beam(T.straight(blk + UP * 0.35, blk + UP * 0.8, X, Y), 0.45, 0.12, 0.12, "MI_Metal_Iron", nseed())
        a = c + Vector((0.0, -side * 0.47, 0.6))
        p.round(T.polyline([a, blk - UP * 0.3 + Vector((0.0, -side * 0.05, 0.0))], X, Y), (blk - a).length, ROPE_R * 0.8, "MI_Rope_Hemp", nseed(),
                sides=8, seg=1.0, wear=0.0)
    # The sill on the basin's floor: a stone-shod timber with a slot for the points.
    for xa, xb in ((x - 1.0, x - 0.34), (x + 0.34, x + 1.0)):
        p.block(xa, xb, -hw - 0.3, hw + 0.3, z0 - 0.15, z0 + 0.35, "MI_Hub_HullTarDark", nseed(), axis="y", grime=g)
    # The gatehouse's slot over the vault: dark lining up to its head, the sheaves the ropes go over.
    for xx in (x - SG["thick"] * 0.5 - 0.12, x + SG["thick"] * 0.5 + 0.12):
        ys = [-MOUTH_W + 0.5 * k for k in range(int(2 * MOUTH_W / 0.5) + 1)]
        for ya, yb in zip(ys[:-1], ys[1:]):                                   # from the vault's line up to the slot's head
            pts = [(xx, ya, HE.arch_z(ya) + 0.05), (xx, yb, HE.arch_z(yb) + 0.05), (xx, yb, 30.9), (xx, ya, 30.9)]
            p.polygon(pts, "MI_Hub_Void", tone=0.1, facing=(x - xx, 0.0, 0.0))
    p.polygon([(x - 0.5, -MOUTH_W, 30.9), (x + 0.5, -MOUTH_W, 30.9), (x + 0.5, MOUTH_W, 30.9), (x - 0.5, MOUTH_W, 30.9)], "MI_Hub_Void",
              tone=0.1, facing=(0.0, 0.0, -1.0))
    for y in SG["ropes"]:
        c = Vector((x, y, 30.5))
        p.lathe([(0.0, -0.14), (0.42, -0.14), (0.45, -0.06), (0.36, 0.0), (0.45, 0.06), (0.42, 0.14), (0.0, 0.14)], c, Y,
                "MI_Wood_Varnish", sides=14)
    export(p, (H.PIVOT_X, 0.0, 0.0))


MOUTH_W = HUB["basin"]["y1"]

BUILDERS = {"gate": build_gate, "rope": build_rope, "capstan": build_capstan, "frame": build_frame}

if __name__ == "__main__":
    random.seed(62)
    wanted = sb.script_args() or list(BUILDERS)
    for key in wanted:
        sb.reset_scene()
        BUILDERS[key]()
    print("SB_HUBGATE_OK", wanted)
