"""The player's arms (D80; owner: "Every object now will first be taken into your hand. with left klick can you use it. This means
the Design of our charakter begins: Make it in a comic style, be detailed but not realistic").

  SK_Arms   both arms of a young sailor seen from his own eyes, one skinned mesh: big comic hands (sb_hand: broad palms with a
            fleshy thumb pad, knuckles standing up in a ridge, thick tapering fingers with fat tips and pale nails), strong
            forearms narrowing to the wrist, the shirt's off-white linen sleeves rolled up past the elbows in two thick rolls; a
            leather wrap tied round the left wrist, a twisted hemp bracelet with a cockle shell on the right. USaltArmsComponent
            poses the bones every frame (reaching for what the hands hold, the fingers closing round it): no animation asset.
The skeleton (SK_Arms_Rig): arms_root at the eyes; for each side upperarm, lowerarm, lowerarm_twist (takes half the hand's
roll so the wrist does not wring), hand, thumb_01-03 and index, middle, ring, pinky _01-03. Built in Unreal's frame relative to
the eyes (x forward, y right, z up, metres), the right arm built and mirrored for the left.
Run: Tools/Build-BlenderAsset.ps1 build_arms
"""
import math
import os
import sys

from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sb_blender as sb  # noqa: E402
import sb_char as C  # noqa: E402
import sb_hand as HD  # noqa: E402
from sb_char import V, smooth  # noqa: E402

MATS = [("MI_Char_Skin", (0.86, 0.6, 0.45)), ("MI_Char_SkinShade", (0.78, 0.5, 0.4)), ("MI_Char_Nail", (0.95, 0.8, 0.72)),
        ("MI_Char_Linen", (0.9, 0.87, 0.78)), ("MI_Char_Leather", (0.38, 0.23, 0.13)), ("MI_Rope_Hemp", (0.66, 0.53, 0.33)),
        ("MI_Char_Shell", (0.96, 0.88, 0.76))]
SKIN, SHADE, NAIL, LINEN, LEATHER, ROPE, SHELL = range(7)

# The right arm's rest pose (relative to the eyes): reaching forward, the hand level, palm down, thumb in (towards -y).
SPEC = dict(shoulder=(-0.10, 0.20, -0.30), elbow=(0.15, 0.24, -0.40), wrist=(0.39, 0.20, -0.335), hand_dir=(1.0, -0.05, 0.03),
            back_of_hand=(0, 0, 1), parent="arms_root", mats=(SKIN, SHADE, NAIL))


def sleeve(B, arm):
    """Off-white linen to the elbow, rolled up in two thick rolls."""
    sp = arm.spec
    n = sp.n
    S0, E0 = sp.shoulder, sp.elbow
    up_dir, fore_dir, fore_len = arm.up_dir, arm.fore_dir, arm.fore_len
    path = C.resample([S0 - up_dir * 0.05, E0, E0 + fore_dir * 0.03], 0.01)

    def sleeve_w(p):
        p = V(p)
        s = (p - S0).dot(up_dir) / (E0 - S0).length
        beyond = (p - E0).dot(fore_dir)
        k = 0.6 * smooth(beyond, -0.02, 0.025) if s > 0.8 else 0.0
        return {n("upperarm"): 1.0 - k, n("lowerarm"): k}

    def sleeve_section(k, t, a):
        r = 0.062 + 0.004 * math.sin(3 * t * math.pi)
        r *= 1.0 + 0.045 * math.sin(5 * a + 9 * t) + 0.03 * math.sin(11 * a - 7 * t) + 0.02 * math.sin(40 * t + a)
        return r, r * 0.95, 2.0
    C.tube(B, path, sleeve_section, LINEN, sleeve_w, n=28, cap=(False, False))

    def roll_w(p):
        return {n("lowerarm"): 0.65, n("upperarm"): 0.35}

    _, fs, fu = C.frames([E0, sp.wrist], (0, 0, 1))[0]
    for i, (dd, tube_r) in enumerate(((0.03, 0.0155), (0.054, 0.0132))):
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


def bracelet(B, arm):
    """The right wrist's twisted hemp bracelet and a cockle shell hanging from it."""
    surface, weight, fr, fore_pts = arm.surface, arm.weight, arm.frames, arm.fore_pts
    t0 = 0.9
    m = 64
    for strand in range(2):
        pts = []
        for j in range(m + 1):
            a = 360.0 * j / m
            tw = math.radians(a * 7 + strand * 180)
            c0 = surface(t0, a, 0.0045)
            c1 = surface(t0, a, 0.0)
            radial = (c0 - c1).normalized()
            along = (surface(t0 + 0.02, a) - c1).normalized()
            pts.append(c0 + radial * (0.0021 * math.cos(tw)) + along * (0.0021 * math.sin(tw)))
        C.tube(B, pts, lambda k, t, a: (0.0026, 0.0026), ROPE, weight, n=8, cap=(False, False))
    hang = surface(t0, -100, 0.012)
    i = min(range(len(fore_pts)), key=lambda j: (V(fore_pts[j]) - hang).length)
    tdir, s, u = fr[i]

    def ribs(l):
        ang = math.atan2(l.y, l.x + 1e-6)
        return Vector((l.x, l.y, l.z + 0.0008 * abs(math.sin(ang * 9)) * (1 if l.z > 0 else 0.3)))
    B.blob(hang - u * 0.004, (0.0105, 0.0095, 0.004), (tdir, s, u * -1.0), SHELL, weight, segs=(18, 8), bend=ribs)
    loop = [surface(t0, -100 + d, 0.005) for d in (-14, 0, 14)]
    C.tube(B, [loop[0], hang - u * 0.001, loop[2]], lambda k, t, a: (0.0014, 0.0014), ROPE, weight, n=6)


def wrap(B, arm):
    """The left wrist's leather wrap: a band with rolled edges, tied on top with a knot of thong."""
    surface, weight, fr, fore_pts = arm.surface, arm.weight, arm.frames, arm.fore_pts
    rings = []
    m = 32
    for k in range(10):
        t = 0.72 + 0.22 * k / 9
        rings.append([tuple(surface(t, 360.0 * i / m, 0.0032 + 0.0005 * math.sin(i * 1.7 + k))) for i in range(m)])
    B.rings(rings, LEATHER, weight, cap=(False, False))
    for t in (0.72, 0.94):
        edge = [surface(t, 360.0 * i / 40, 0.0042) for i in range(41)]
        C.tube(B, edge, lambda k, tt, a: (0.0026, 0.002), LEATHER, weight, n=8, cap=(False, False))
    knot = surface(0.83, 95, 0.008)
    i = min(range(len(fore_pts)), key=lambda j: (V(fore_pts[j]) - knot).length)
    tdir, s, u = fr[i]
    B.blob(knot, (0.006, 0.0045, 0.0035), (tdir, s, u), ROPE, weight, segs=(10, 6))
    for sgn in (1, -1):
        end = [knot, knot + tdir * (0.012 * sgn) + s * 0.004 - u * 0.001, knot + tdir * (0.02 * sgn) + s * 0.011 - u * 0.004]
        C.tube(B, C.resample(end, 0.003), lambda k, t, a: (0.0016 * (1 - 0.4 * t), 0.0012), ROPE, weight, n=6)


def main():
    sb.reset_scene()
    bones = [("arms_root", None, (0.0, 0.0, 0.0), (0.05, 0.0, 0.0))]
    both = C.Builder()
    arm = HD.build_arm(both, HD.ArmSpec(**SPEC), bones)
    sleeve(both, arm)
    bracelet(both, arm)
    left = C.Builder()
    arm_l = HD.build_arm(left, HD.ArmSpec(**SPEC), [])
    sleeve(left, arm_l)
    wrap(left, arm_l)
    HD.mirror_into(left, both)
    left.bm.free()
    HD.mirror_bones(bones)
    C.finish(both, "SK_Arms", MATS, bones, "Characters", uv_metres=0.08)


main()
