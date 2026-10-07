"""The Harbour Keeper (D83; owner, D77: "simple stylised figures behind counters"; GameConcept 4.4: the first face, at his place by
the pier's beach).

  SK_Keeper   an old salt, 1.74 m, stylised like the rest of the world (chunky, readable at a glance, a little comic): a big head
              under a knitted watch cap with a turned-up brim, a bulbous nose, small dark eyes under bushy white brows, a great
              white beard and moustache, a clay pipe in the corner of his mouth; a barrel chest and a belly, a dark wool waistcoat
              with brass buttons open over an off-white linen shirt, its sleeves rolled up over strong forearms; big hands (the
              player's, sb_hand, a size up) resting on his counter; a belt with a brass buckle, canvas trousers, boots (behind the
              counter, seldom seen).
The skeleton: root; pelvis, spine_01, spine_02 (the chest), neck, head; for each arm upperarm (from the chest), lowerarm,
lowerarm_twist, hand and the fingers (sb_hand). The legs go with the pelvis. ASaltPerson poses the bones every frame (breathing,
the head turning to whoever comes to the counter, a gesture while he talks): no animation asset.
Built in Unreal's frame (x where he faces, y his right, z up from his feet), metres; the rest pose leans on a counter 0.93 m
above his feet (build_home_props.py: COUNTER - FLOOR), its inner edge 0.16 m in front of him.
Run: Tools/Build-BlenderAsset.ps1 build_keeper
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402
import sb_char as C  # noqa: E402
import sb_hand as HD  # noqa: E402
from sb_char import V, smooth  # noqa: E402

MATS = [("MI_Char_Skin", (0.86, 0.6, 0.45)), ("MI_Char_SkinShade", (0.78, 0.5, 0.4)), ("MI_Char_Nail", (0.95, 0.8, 0.72)),
        ("MI_Char_Linen", (0.9, 0.87, 0.78)), ("MI_Char_Leather", (0.38, 0.23, 0.13)), ("MI_Char_Vest", (0.2, 0.22, 0.24)),
        ("MI_Char_Wool", (0.45, 0.14, 0.1)), ("MI_Char_Beard", (0.85, 0.83, 0.8)), ("MI_Char_Eye", (0.05, 0.04, 0.04)),
        ("MI_Char_Trousers", (0.22, 0.26, 0.32)), ("MI_Wood_TeakDark", (0.42, 0.26, 0.13)), ("MI_Metal_Brass", (0.6, 0.45, 0.2))]
SKIN, SHADE, NAIL, LINEN, LEATHER, VEST, WOOL, BEARD, EYE, TROUSERS, PIPE, BRASS = range(12)

# The body's joints (his rest pose).
PELVIS, SPINE1, SPINE2, NECK, HEAD, HEAD_TOP = (0.0, 0.0, 0.93), (0.0, 0.0, 1.06), (0.0, 0.0, 1.24), (0.0, 0.0, 1.47), \
    (0.025, 0.0, 1.56), (0.025, 0.0, 1.8)
BODY_CHAIN = [("pelvis", PELVIS, SPINE1), ("spine_01", SPINE1, SPINE2), ("spine_02", SPINE2, NECK), ("neck", NECK, HEAD),
              ("head", HEAD, HEAD_TOP)]
# The right arm leaning on the counter: hands 0.93 m up, a forearm's length in front of him.
ARM = dict(shoulder=(-0.005, 0.2, 1.41), elbow=(0.07, 0.29, 1.13), wrist=(0.31, 0.215, 0.975), hand_dir=(0.93, -0.33, -0.12),
           back_of_hand=(0.05, 0.1, 1.0), parent="spine_02", scale=1.16, upper_r=0.052, curl=(14.0, 18.0, 12.0),
           mats=(SKIN, SHADE, NAIL))


def fore_r(t):
    """His forearms: thick and strong (a sailor's), slimmer at the wrist."""
    side, up = HD.default_fore_r(t)
    return side * 1.12, up * 1.1


def torso_ring(z):
    """Half depth (x), half width (y) and the middle's forward shift of the torso at height z: hips, a round belly, a barrel
    chest, broad sloping shoulders, the neck."""
    keys = [(0.86, 0.125, 0.165, 0.0), (0.95, 0.15, 0.175, 0.01), (1.06, 0.185, 0.185, 0.035), (1.14, 0.19, 0.19, 0.04),
            (1.24, 0.17, 0.198, 0.025), (1.33, 0.155, 0.208, 0.015), (1.40, 0.135, 0.212, 0.0), (1.45, 0.11, 0.17, -0.005),
            (1.49, 0.075, 0.1, 0.0), (1.53, 0.058, 0.062, 0.012), (1.57, 0.055, 0.058, 0.018)]
    for (z0, a0, b0, c0), (z1, a1, b1, c1) in zip(keys, keys[1:]):
        if z <= z1:
            t = (z - z0) / (z1 - z0)
            s = t * t * (3 - 2 * t)
            return a0 + (a1 - a0) * s, b0 + (b1 - b0) * s, c0 + (c1 - c0) * s
    return keys[-1][1:]


def body(B):
    weight = C.chain_weight(BODY_CHAIN, 0.05)
    zs = [0.86 + 0.71 * i / 56 for i in range(57)]
    # The torso as rings round z (the ring's "side" is +y, its "up" is +x: forward).
    rings = []
    for z in zs:
        rx, ry, cx = torso_ring(z)
        ring = []
        for i in range(72):
            a = 2 * math.pi * i / 72
            c, s = math.cos(a), math.sin(a)
            e = 2.0 / 2.4
            x = rx * math.copysign(abs(s) ** e, s)
            y = ry * math.copysign(abs(c) ** e, c)
            ring.append((cx + x, y, z))
        rings.append(ring)

    def mat(k, i):
        # The waistcoat round the back and sides, open down the front over the shirt; the shirt's collar above it; the
        # trousers below the belt.
        z = zs[k]
        a = 2 * math.pi * (i + 0.5) / 72
        front = math.sin(a)                 # 1 = straight ahead
        if z < 0.97:
            return TROUSERS
        if z > 1.475:
            return LINEN if z < 1.52 else SKIN
        gap = 0.04 + 0.22 * smooth(z, 1.22, 1.47)   # a narrow opening, widening to the collar
        return LINEN if front > 1.0 - gap else VEST
    B.rings(rings, VEST, weight, cap=(True, False), mat_fn=mat)
    # The belt and its buckle, brass buttons down the waistcoat's edges.
    belt = []
    for i in range(48):
        a = 2 * math.pi * i / 48
        rx, ry, cx = torso_ring(0.975)
        belt.append((cx + (rx + 0.008) * math.sin(a), (ry + 0.008) * math.cos(a), 0.975))
    C.tube(B, belt + belt[:1], lambda k, t, a: (0.006, 0.024, 3.0), LEATHER, weight, up_hint=(0, 0, 1), n=10, cap=(False, False))
    rx, ry, cx = torso_ring(0.975)
    B.blob((cx + rx + 0.016, 0.0, 0.975), (0.008, 0.03, 0.026), ((1, 0, 0), (0, 1, 0), (0, 0, 1)), BRASS, weight, segs=(12, 8))
    for z in (1.08, 1.17, 1.26, 1.35):
        rx, ry, cx = torso_ring(z)
        gap = 0.04 + 0.22 * smooth(z, 1.22, 1.47)
        a = math.asin(1.0 - gap)
        for sgn in (1, -1):
            px = cx + (rx + 0.004) * abs(math.sin(a)) ** (2.0 / 2.4)
            py = sgn * (ry + 0.004) * abs(math.cos(a)) ** (2.0 / 2.4) * 1.02
            B.blob((px, py, z), (0.006, 0.012, 0.012), ((1, 0, 0), (0, 1, 0), (0, 0, 1)), BRASS, weight, segs=(10, 6))
    # The shirt's collar standing open round the neck.
    ring = [(0.018 + 0.074 * math.cos(math.radians(a)), 0.074 * math.sin(math.radians(a)), 1.49 + 0.012 * math.cos(math.radians(a)))
            for a in range(-140, 141, 10)]
    C.tube(B, ring, lambda k, t, a: (0.007, 0.018, 2.5), LINEN, weight, up_hint=(0, 0, 1), n=10)
    # Deltoids rounding the shoulders into the arms.
    for sgn in (1, -1):
        B.blob((0.0, 0.2 * sgn, 1.405), (0.066, 0.06, 0.062), ((1, 0, 0), (0, 1, 0), (0, 0, 1)), LINEN,
               lambda p: {"spine_02": 1.0}, segs=(16, 10))


def legs(B):
    w = lambda p: {"pelvis": 1.0}   # noqa: E731  (the legs stand still behind the counter)
    for sgn in (1, -1):
        path = C.resample([(0.0, 0.085 * sgn, 0.9), (0.02, 0.1 * sgn, 0.5), (0.0, 0.105 * sgn, 0.12)], 0.04)
        C.tube(B, path, lambda k, t, a: (0.085 - 0.03 * t, 0.08 - 0.025 * t), TROUSERS, w, up_hint=(1, 0, 0), n=16, cap=(True, True))
        B.blob((0.045, 0.105 * sgn, 0.06), (0.135, 0.062, 0.065), ((1, 0, 0), (0, 1, 0), (0, 0, 1)), LEATHER, w, segs=(16, 10))


def sleeve(B, arm):
    """Linen to the elbow, rolled up in two thick rolls (as the player's, build_arms.py)."""
    sp = arm.spec
    n = sp.n
    S0, E0 = sp.shoulder, sp.elbow
    up_dir, fore_dir, fore_len = arm.up_dir, arm.fore_dir, arm.fore_len
    path = C.resample([S0 - up_dir * 0.03, E0, E0 + fore_dir * 0.03], 0.012)

    def sleeve_w(p):
        p = V(p)
        s = (p - S0).dot(up_dir) / (E0 - S0).length
        beyond = (p - E0).dot(fore_dir)
        k = 0.6 * smooth(beyond, -0.02, 0.025) if s > 0.8 else 0.0
        return {n("upperarm"): 1.0 - k, n("lowerarm"): k}

    def sleeve_section(k, t, a):
        r = 0.07 + 0.006 * math.sin(3 * t * math.pi)
        r *= 1.0 + 0.045 * math.sin(5 * a + 9 * t) + 0.03 * math.sin(11 * a - 7 * t) + 0.02 * math.sin(40 * t + a)
        return r, r * 0.95, 2.0
    C.tube(B, path, sleeve_section, LINEN, sleeve_w, n=28, cap=(False, False))

    def roll_w(p):
        return {n("lowerarm"): 0.65, n("upperarm"): 0.35}
    _, fs, fu = C.frames([E0, sp.wrist], (0, 0, 1))[0]
    for i, (dd, tube_r) in enumerate(((0.03, 0.018), (0.058, 0.015))):
        rs, ru = sp.fore_r(dd / fore_len)
        centre = E0 + fore_dir * dd
        ring = []
        m = 40
        for j in range(m + 1):
            a = 2 * math.pi * j / m
            wob = 1.0 + 0.07 * math.sin(3 * a + i) + 0.035 * math.sin(7 * a + 2 * i)
            rr = rs + tube_r * 0.75 + 0.002 * i
            ru2 = ru + tube_r * 0.75 + 0.002 * i
            ring.append(centre + fs * (math.cos(a) * rr * wob) + fu * (math.sin(a) * ru2 * wob) + fore_dir * (0.004 * math.sin(2 * a + i)))
        C.tube(B, ring, lambda k, t_, a_, tr=tube_r: (tr * (1 + 0.15 * math.sin(6 * t_ * math.pi)), tr * 0.8, 2.0), LINEN, roll_w,
               up_hint=tuple(fore_dir), n=12, cap=(False, False))


def head(B):
    hw = lambda p: {"head": 1.0}                          # noqa: E731
    neckw = lambda p: {"head": 0.6, "neck": 0.4}          # noqa: E731
    X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    # The skull and face: a big rounded head, cheeks, a heavy brow.
    B.blob((0.03, 0.0, 1.655), (0.112, 0.103, 0.128), (X, Y, Z), SKIN, hw, segs=(28, 18))
    for sgn in (1, -1):
        B.blob((0.095, 0.052 * sgn, 1.63), (0.04, 0.035, 0.032), (X, Y, Z), SKIN, hw, segs=(14, 10))     # cheeks
        B.blob((0.008, 0.106 * sgn, 1.65), (0.022, 0.013, 0.036), (X, Y, Z), SKIN, hw, segs=(12, 8))     # ears
    B.blob((0.115, 0.0, 1.697), (0.03, 0.09, 0.02), (X, Y, Z), SKIN, hw, segs=(18, 8))                  # brow
    # The nose: a bridge and a big bulb.
    B.blob((0.13, 0.0, 1.665), (0.022, 0.018, 0.035), ((0.94, 0, -0.34), Y, (0.34, 0, 0.94)), SKIN, hw, segs=(12, 10))
    B.blob((0.152, 0.0, 1.632), (0.034, 0.03, 0.03), (X, Y, Z), SHADE, hw, segs=(16, 12))
    for sgn in (1, -1):
        B.blob((0.142, 0.022 * sgn, 1.622), (0.016, 0.014, 0.012), (X, Y, Z), SHADE, hw, segs=(10, 8))  # nostril wings
        # Small dark eyes under bushy white brows.
        B.blob((0.119, 0.043 * sgn, 1.672), (0.011, 0.014, 0.015), (X, Y, Z), EYE, hw, segs=(12, 10))
        B.blob((0.122, 0.046 * sgn, 1.704), (0.018, 0.036, 0.014), ((1, 0, 0), (0, 0.97, 0.26 * sgn), (0, -0.26 * sgn, 0.97)), BEARD, hw,
               segs=(12, 8), bend=lambda l: Vector((l.x, l.y, l.z + 0.006 * math.sin(l.y * 140))))
    # The beard: a great lumpy mass from the cheeks over the jaw and chin down the chest, combed in thick locks.
    rng = random.Random(83)

    def locks(l):
        a = math.atan2(l.y, l.x)
        bump = 0.006 * math.sin(a * 9 + l.z * 30) + 0.004 * math.sin(a * 17 - l.z * 50)
        r = l.length or 1.0
        return l * (1.0 + bump / r)
    B.blob((0.085, 0.0, 1.565), (0.075, 0.105, 0.105), (X, Y, Z), BEARD, neckw, segs=(26, 18), bend=locks)
    B.blob((0.115, 0.0, 1.5), (0.045, 0.07, 0.07), ((0.96, 0, 0.28), Y, (-0.28, 0, 0.96)), BEARD, neckw, segs=(18, 12), bend=locks)
    for k in range(9):
        a = -1.1 + 2.2 * k / 8
        p = (0.09 + 0.06 * math.cos(a), 0.1 * math.sin(a), 1.53 + rng.uniform(-0.03, 0.02))
        B.blob(p, (0.03, 0.028, 0.05), (X, Y, Z), BEARD, neckw, segs=(10, 8), bend=locks)
    # The moustache: two thick drooping locks from under the nose.
    for sgn in (1, -1):
        pts = [(0.162, 0.006 * sgn, 1.605), (0.162, 0.03 * sgn, 1.6), (0.152, 0.058 * sgn, 1.585), (0.135, 0.075 * sgn, 1.56)]
        C.tube(B, C.resample(pts, 0.008), lambda k, t, a: (0.016 * (1 - 0.6 * t) + 0.004, 0.011 * (1 - 0.5 * t) + 0.003), BEARD, hw,
               up_hint=(0, 0, 1), n=12)
    # The knitted watch cap: ribbed, pulled down to the ears, its brim turned up in a thick roll.
    rings = []
    for k in range(13):
        t = k / 12
        z = 1.722 + 0.11 * math.sin(t * math.pi / 2)
        r = 0.118 * math.cos(t * math.pi / 2) ** 0.62 + 0.004
        ring = []
        for i in range(48):
            a = 2 * math.pi * i / 48
            rib = 1.0 + 0.025 * math.cos(a * 24) * (1 - t)
            ring.append((0.026 + r * rib * math.cos(a) * 1.04, r * rib * math.sin(a) * 0.95, z + 0.012 * math.cos(a)))
        rings.append(ring)
    B.rings(rings, WOOL, hw, cap=(False, True))
    brim = []
    for i in range(49):
        a = 2 * math.pi * i / 48
        brim.append((0.026 + 0.124 * math.cos(a) * 1.04, 0.124 * math.sin(a) * 0.95, 1.724 - 0.008 * math.cos(a)))
    C.tube(B, brim, lambda k, t, a: (0.016, 0.028, 2.0), WOOL, hw, up_hint=(0, 0, 1), n=12, cap=(False, False))
    # White hair from ear to ear round the back, under the cap.
    for k in range(11):
        a = math.radians(70 + 220 * k / 10)
        p = (0.03 + 0.1 * math.cos(a), 0.1 * math.sin(a), 1.69 + 0.008 * math.sin(k * 1.7))
        B.blob(p, (0.03, 0.03, 0.04), (X, Y, Z), BEARD, hw, segs=(10, 8), bend=locks)
    # The clay pipe in the left corner of his mouth (as he faces): a curved stem and a bowl.
    stem = [(0.15, -0.03, 1.582), (0.19, -0.05, 1.566), (0.225, -0.062, 1.558), (0.245, -0.066, 1.562)]
    C.tube(B, C.resample(stem, 0.008), lambda k, t, a: (0.0055 - 0.001 * t, 0.0055 - 0.001 * t), PIPE, hw, up_hint=(0, 0, 1), n=10)
    bowl = []
    for k in range(9):
        t = k / 8
        r = 0.013 + 0.004 * math.sin(t * math.pi) - 0.002 * t
        bowl.append([(0.252 + r * math.cos(2 * math.pi * i / 16), -0.066 + r * math.sin(2 * math.pi * i / 16), 1.556 + 0.045 * t)
                     for i in range(16)])
    B.rings(bowl, PIPE, hw, cap=(True, True))


def main():
    sb.reset_scene()
    bones = [("root", None, (0.0, 0.0, 0.0), (0.0, 0.0, 0.1))]
    parent = "root"
    for name, h, t in BODY_CHAIN:
        bones.append((name, parent, h, t))
        parent = name
    B = C.Builder()
    body(B)
    legs(B)
    head(B)
    right = C.Builder()
    arm = HD.build_arm(right, HD.ArmSpec(fore_r=fore_r, **ARM), bones)
    sleeve(right, arm)
    HD.mirror_into(right, B)     # the left arm
    right_bm = right
    # The right arm itself: copy the right builder's geometry unmirrored.
    vmap = {}
    for v in right_bm.bm.verts:
        nv = B.bm.verts.new(v.co.copy())
        B.weights[nv] = dict(right_bm.weights[v])
        vmap[v] = nv
    for f in right_bm.bm.faces:
        nf = B.bm.faces.new([vmap[v] for v in f.verts])
        nf.material_index = f.material_index
    right_bm.bm.free()
    HD.mirror_bones(bones)
    C.finish(B, "SK_Keeper", MATS, bones, "Characters", uv_metres=0.1)


main()
