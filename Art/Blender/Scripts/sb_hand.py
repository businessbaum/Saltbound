"""A comic arm and hand (D80 the player's arms, D81 the Harbour Keeper's): the upper arm, the elbow, one loft from the elbow over
the wrist to the knuckles (a strong forearm narrowing to the wrist, a broad palm with a fleshy thumb pad and the knuckles standing
up in a ridge), a thick thumb and four fingers that taper to fat rounded tips with pale nails. Its bones: upperarm, lowerarm,
lowerarm_twist, hand, thumb_01-03, index/middle/ring/pinky _01-03 (suffixed with the side's tag), weights from the part each
vertex belongs to (sb_char). Built for the right side in Unreal's frame; the caller mirrors it for the left.
"""
import math

from mathutils import Matrix

import sb_char as C
from sb_char import V, smooth

# Fingers: (name, knuckle (along the hand, across towards the little finger), spread degrees, lengths, base and tip radius), metres
# for a hand of scale 1 (palm 11.2 cm).
FINGERS = [
    ("index", (0.110, -0.035), -5.0, (0.045, 0.028, 0.024), 0.0122, 0.0104),
    ("middle", (0.113, -0.0117), -1.0, (0.050, 0.031, 0.025), 0.0127, 0.0108),
    ("ring", (0.110, 0.0117), 3.0, (0.047, 0.029, 0.024), 0.0120, 0.0101),
    ("pinky", (0.102, 0.034), 8.0, (0.037, 0.023, 0.020), 0.0104, 0.0088),
]
PALM_LEN = 0.112


def gauss(x, c, w):
    return math.exp(-((x - c) / w) ** 2)


def rot(v, axis, deg):
    return Matrix.Rotation(math.radians(deg), 3, V(axis).normalized()) @ V(v)


class ArmSpec:
    """Where the arm lies in its rest pose and how it is shaped."""

    def __init__(self, shoulder, elbow, wrist, hand_dir, back_of_hand, parent, tag="r", scale=1.0, fore_r=None, upper_r=0.043,
                 curl=(8.0, 12.0, 9.0), mats=(0, 1, 2)):
        self.shoulder, self.elbow, self.wrist = V(shoulder), V(elbow), V(wrist)
        self.F = V(hand_dir).normalized()
        u = V(back_of_hand)
        self.UP = (u - self.F * u.dot(self.F)).normalized()
        self.S = self.UP.cross(self.F).normalized()   # towards the little finger
        self.parent, self.tag, self.scale = parent, tag, scale
        self.fore_r = fore_r or default_fore_r
        self.upper_r = upper_r
        self.curl = curl
        self.skin, self.shade, self.nail = mats

    def n(self, bone):
        return "%s_%s" % (bone, self.tag)

    def H(self, lf, ls, lu):
        """A point in the hand's frame (along, across towards the little finger, up from the back of the hand), scaled."""
        s = self.scale
        return self.wrist + self.F * (lf * s) + self.S * (ls * s) + self.UP * (lu * s)


def default_fore_r(t):
    """The forearm's half width and half height at t (0 the elbow .. 1 the wrist): chunky below the elbow, slim at the wrist."""
    side = 0.047 + 0.008 * gauss(t, 0.2, 0.17) - 0.017 * smooth(t, 0.25, 0.97)
    up = 0.041 + 0.006 * gauss(t, 0.22, 0.17) - 0.020 * smooth(t, 0.25, 0.97)
    return side, up


def finger_joints(knuckle, lengths, curl, base_dir, bend_axis):
    d = base_dir
    js, ds = [V(knuckle)], []
    for L, c in zip(lengths, curl):
        d = rot(d, bend_axis, c)
        ds.append(d)
        js.append(js[-1] + d * L)
    return js, ds


class Arm:
    """What build_arm made: the forearm's surface, its weights and frames, for sleeves and things worn on it."""

    def __init__(self, spec, surface, weight, frames, fore_pts, fore_len, fore_dir, up_dir):
        self.spec, self.surface, self.weight, self.frames, self.fore_pts = spec, surface, weight, frames, fore_pts
        self.fore_len, self.fore_dir, self.up_dir = fore_len, fore_dir, up_dir


def build_arm(B, spec, bones):
    """The arm into B; its bones appended to bones (name, parent, head, tail). Returns an Arm."""
    sp = spec
    s = sp.scale
    n = sp.n
    F, S, UP = sp.F, sp.S, sp.UP
    SHOULDER, ELBOW, WRIST = sp.shoulder, sp.elbow, sp.wrist
    palm_len = PALM_LEN * s
    K = WRIST + F * palm_len
    mid = ELBOW.lerp(WRIST, 0.5)
    bones += [(n("upperarm"), sp.parent, tuple(SHOULDER), tuple(ELBOW)), (n("lowerarm"), n("upperarm"), tuple(ELBOW), tuple(WRIST)),
              (n("lowerarm_twist"), n("lowerarm"), tuple(mid), tuple(WRIST)), (n("hand"), n("lowerarm_twist"), tuple(WRIST), tuple(K))]
    up_dir = (ELBOW - SHOULDER).normalized()
    fore_dir = (WRIST - ELBOW).normalized()
    fore_len = (WRIST - ELBOW).length
    fore_r = sp.fore_r

    def upper_w(p):
        t = (V(p) - SHOULDER).dot(up_dir) / (ELBOW - SHOULDER).length
        k = 0.5 * smooth(t, 0.86, 1.1)
        return {n("upperarm"): 1.0 - k, n("lowerarm"): k}

    C.tube(B, C.resample([SHOULDER, ELBOW], 0.03), lambda k, t, a: (sp.upper_r, sp.upper_r * 0.95, 2.0), sp.skin, upper_w, n=16,
           up_hint=tuple(UP))
    r0 = fore_r(0.0)
    B.blob(ELBOW, (r0[0] * 1.0, r0[0] * 0.96, r0[1] * 1.05), (up_dir, UP.cross(up_dir).normalized(), UP), sp.skin, upper_w)

    def arm_w(p):
        p = V(p)
        lf = (p - WRIST).dot(F)
        if lf > 0.0:
            h = 0.45 + 0.55 * smooth(lf, 0.0, 0.02 * s)
            return {n("hand"): h, n("lowerarm_twist"): 1.0 - h}
        t = (p - ELBOW).dot(fore_dir) / fore_len
        elbow = 0.5 * (1.0 - smooth(t, -0.06, 0.06))
        twist = smooth(t, 0.3, 0.68)
        hand = 0.45 * smooth(t, 0.86, 1.0)
        return {n("lowerarm"): (1.0 - twist) * (1.0 - elbow), n("upperarm"): elbow * (1.0 - twist), n("lowerarm_twist"): twist * (1.0 - hand),
                n("hand"): twist * hand}

    fore_pts = C.resample([ELBOW - fore_dir * 0.012, WRIST], 0.008)
    palm_pts = C.resample([WRIST, WRIST + F * (palm_len + 0.016 * s)], 0.004)[1:]
    path = fore_pts + palm_pts
    fr = C.frames(path, tuple(UP))
    d_wrist = (WRIST - (ELBOW - fore_dir * 0.012)).length
    knuckles = [(ks * s, r * s) for _, (kf, ks), _, _, r, _ in FINGERS]
    wr = fore_r(1.0)

    def arm_xy(k, d, a):
        if d <= d_wrist:
            t = (d - 0.012) / fore_len
            rs, ru = fore_r(t)
            x, y = C.soft_box(rs, ru, a, 2.15)
            # The flexors' swell on the inner, palm side below the elbow.
            if y < 0 and x < 0:
                g = gauss(t, 0.3, 0.2) * (-math.sin(a)) * (-math.cos(a))
                x -= 0.004 * g
                y -= 0.004 * g
            return x, y
        lf = (d - d_wrist) / s
        w = wr[0] / s + (0.050 - wr[0] / s) * smooth(lf, 0.0, 0.08)
        top = wr[1] / s - 0.0055 * smooth(lf, 0.0, 0.05)
        bot = wr[1] / s - 0.003 * smooth(lf, 0.02, 0.06) - 0.003 * smooth(lf, 0.08, 0.11)
        end = 1.0
        if lf > PALM_LEN - 0.012:
            q = (lf - (PALM_LEN - 0.012)) / 0.028
            end = math.sqrt(max(1.0 - q * q, 0.0))
        x, y = C.soft_box(w, top if math.sin(a) > 0 else bot, a, 2.6)
        y -= 0.0015
        sn, cs = math.sin(a), math.cos(a)
        # The thumb's pad on the palm side towards the thumb, the little finger's pad opposite.
        if sn < 0 and cs < 0:
            g = smooth(lf, -0.01, 0.015) * (1.0 - smooth(lf, 0.05, 0.085)) * (-sn) * (-cs)
            y -= 0.010 * g
            x -= 0.005 * g
        if sn < 0 and cs > 0:
            g = smooth(lf, 0.0, 0.03) * (1.0 - smooth(lf, 0.075, 0.1)) * (-sn) * cs
            y -= 0.005 * g
        # The knuckles standing up in a ridge across the back, tendons fanning faintly towards them.
        if sn > 0:
            ridge = smooth(lf, PALM_LEN - 0.03, PALM_LEN - 0.012) * (1.0 - smooth(lf, PALM_LEN + 0.004, PALM_LEN + 0.014))
            bump = sum(gauss(x * s, ks, r * 0.62) for ks, r in knuckles)
            y += (0.0052 * bump + 0.0015) * ridge * sn
            ten = smooth(lf, 0.03, 0.06) * (1.0 - smooth(lf, PALM_LEN - 0.025, PALM_LEN - 0.012))
            y += 0.0008 * ten * sum(gauss(x * s, ks * (0.55 + 0.45 * smooth(lf, 0.03, 0.09)), 0.0035 * s) for ks, _ in knuckles) * sn
        return x * end * s, y * end * s

    C.tube_xy(B, path, arm_xy, sp.skin, arm_w, n=40, frames_=fr, cap=(True, False))

    def surface(t, angle, lift=0.0):
        """A point on the forearm's skin at t (0 elbow .. 1 wrist), angle round it (0 = the little finger's side, 90 = the back of
        the hand's side), lifted off it by lift."""
        d = 0.012 + t * fore_len
        i = min(range(len(fore_pts)), key=lambda j: abs((V(fore_pts[j]) - V(fore_pts[0])).length - d))
        p = V(fore_pts[i])
        _, ss, u = fr[i]
        rs, ru = fore_r(t)
        x, y = C.soft_box(rs + lift, ru + lift, math.radians(angle), 2.15)
        return p + ss * x + u * y

    def digit(js, ds, r0, r1, up_vec, chain):
        """A finger (or the thumb): thick, tapering, knuckles swelling on the back, pads underneath, a fat rounded tip, a nail."""
        start = js[0] - ds[0] * 0.016 * s
        pts = C.resample([start] + js, 0.003 * s)
        cum = [0.0]
        for a_, b_ in zip(pts, pts[1:]):
            cum.append(cum[-1] + (V(b_) - V(a_)).length)
        total = cum[-1]
        joints = [0.016 * s + sum((js[i + 1] - js[i]).length for i in range(k)) for k in (1, 2)]
        tip_len = r1 * 1.25

        def xy(k, d, a):
            x_ = cum[k]
            r = r0 + (r1 - r0) * smooth(x_, 0.016 * s, total * 0.8)
            r *= 1.0 + 0.06 * gauss(x_, total - tip_len * 1.2, tip_len * 0.8)
            end = 1.0
            if x_ > total - tip_len:
                q = (x_ - (total - tip_len)) / tip_len
                end = math.sqrt(max(1.0 - q * q, 0.0))
            xx, yy = C.soft_box(r, r * 0.88, a, 2.3)
            if math.sin(a) > 0:
                for j in joints:
                    yy += r * 0.16 * gauss(x_, j, 0.007 * s) * math.sin(a)
            else:
                for c in (joints[0] - 0.02 * s, joints[1] - 0.012 * s, total - tip_len * 1.3):
                    yy -= r * 0.07 * gauss(x_, c, 0.008 * s) * -math.sin(a)
            return xx * max(end, 0.0), yy * max(end, 0.0)

        fr_ = C.frames(pts, tuple(up_vec))
        C.tube_xy(B, pts, xy, sp.skin, C.chain_weight(chain, 0.008 * s), n=18, frames_=fr_, cap=(True, False))
        d = ds[-1]
        upn = (up_vec - d * up_vec.dot(d)).normalized()
        side = d.cross(upn).normalized()
        L = (js[3] - js[2]).length
        centre = js[2] + d * (L * 0.58) + upn * (r1 * 0.8)
        B.blob(centre, (L * 0.44, r1 * 0.7, r1 * 0.17), (d, side, upn), sp.nail, C.chain_weight(chain[-1:], 0.0), segs=(14, 8))

    def finger_bones(name, js):
        return [("%s_%02d_%s" % (name, i + 1, sp.tag), None, tuple(js[i]), tuple(js[i + 1])) for i in range(3)]

    thumb_c = sp.H(0.012, -0.024, -0.014)
    thumb_d0 = (F * 0.72 + S * -0.6 + UP * -0.22).normalized()
    thumb_up = UP * math.cos(math.radians(55)) + (-S) * math.sin(math.radians(55))
    thumb_up = (thumb_up - thumb_d0 * thumb_up.dot(thumb_d0)).normalized()
    tj, td = finger_joints(thumb_c, tuple(x * s for x in (0.046, 0.035, 0.029)), (0.0, 10.0, 9.0), thumb_d0,
                           thumb_up.cross(thumb_d0).normalized())
    tb = finger_bones("thumb", tj)
    bones += [(b[0], n("hand") if i == 0 else tb[i - 1][0], b[2], b[3]) for i, b in enumerate(tb)]
    digit(tj, td, 0.0175 * s, 0.0132 * s, thumb_up, [(n("hand"), tuple(WRIST), tuple(tj[0]))] + [(b[0], b[2], b[3]) for b in tb])

    for name, (kf, ks), spread, lengths, r0, r1 in FINGERS:
        d0 = rot(F, UP, spread)
        js, ds = finger_joints(sp.H(kf, ks, 0.001), tuple(x * s for x in lengths), sp.curl, d0, UP.cross(d0).normalized())
        fb = finger_bones(name, js)
        bones += [(b[0], n("hand") if i == 0 else fb[i - 1][0], b[2], b[3]) for i, b in enumerate(fb)]
        digit(js, ds, r0 * s, r1 * s, UP, [(n("hand"), tuple(WRIST), tuple(js[0]))] + [(b[0], b[2], b[3]) for b in fb])

    return Arm(sp, surface, arm_w, fr, fore_pts, fore_len, fore_dir, up_dir)


def mirror_into(src, dst):
    """Copies a builder's geometry mirrored to -y, with _r weights renamed _l."""
    from mathutils import Vector
    vmap = {}
    for v in src.bm.verts:
        p = V(v.co)
        nv = dst.bm.verts.new(Vector((p.x, -p.y, p.z)))   # v.co is in Blender's frame: flipping its y mirrors Unreal's y too
        dst.weights[nv] = {k[:-2] + "_l" if k.endswith("_r") else k: w for k, w in src.weights[v].items()}
        vmap[v] = nv
    for f in src.bm.faces:
        nf = dst.bm.faces.new([vmap[v] for v in reversed(f.verts)])
        nf.material_index = f.material_index


def mirror_bones(bones):
    """Adds the _l twins of the _r bones (y mirrored)."""
    for name, parent, head, tail in list(bones):
        if name.endswith("_r"):
            bones.append((name[:-2] + "_l", (parent[:-2] + "_l") if parent and parent.endswith("_r") else parent,
                          (head[0], -head[1], head[2]), (tail[0], -tail[1], tail[2])))
