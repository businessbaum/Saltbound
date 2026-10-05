"""Hand-made timber for the hub ship (D62): boards, beams and posts generated straight into one mesh.

The hub is about 30 times the player's boat, with tens of thousands of boards, so they are not built as objects with
modifiers (sb_timber.rough_plank, about 3 000 vertices a board): each board is a ring of 8-11 points (a chamfered
section, the top slightly cupped and grooved) swept along its frame and written into the part's bmesh. The look keeps
the boat's rules (D38, D52): every board its own seed, its width drifting, its long edges wobbling, gently bent and
twisted, a few millimetres proud or sunk, broad worn chamfers (rounded on some boards, angular on others), ends cut a
little askew; its own shade (the caller picks among a material's variants) and wear.

Vertex colours as M_SB_Surface reads them (sb_blender.bake_vertex_masks): R edge wear (on the chamfers, broken along
the board), G grime and wet (a callable of the position: corners, the waterline), B a tone per board.
UVs: U along the board in metres (the grain follows it), V round its section in metres; the end grain across.

Coordinates are ship metres (x forward, y starboard, z up). `Part.finish()` moves them to Blender (pivot at the ship's
x 80, Blender Y = -y) and adds UCX hulls for the collision boxes registered with the part.
"""
import math
import random

import bmesh
import bpy
from mathutils import Vector

import sb_blender as sb

PIVOT_X = 80.0


class Wave:
    """Smooth 1D noise on s in [0, 1]: a few sines with random phases."""

    def __init__(self, rng, octaves=2, base=1.0):
        self.terms = [(rng.uniform(0.6, 1.4) * base * (2 ** o), rng.uniform(0.0, 2.0 * math.pi), 0.5 ** o) for o in range(octaves)]
        self.norm = sum(t[2] for t in self.terms)

    def __call__(self, s):
        return sum(a * math.sin(2.0 * math.pi * f * s + p) for f, p, a in self.terms) / self.norm


def straight(a, b, across, out):
    """A frame for a straight board from a to b: width along `across`, thickness along `out` (ship metres)."""
    a, b = Vector(a), Vector(b)
    across, out = Vector(across).normalized(), Vector(out).normalized()

    def f(s):
        return a + (b - a) * s, across, out, 1.0
    return f


def arc(centre, u, v, radius, a0, a1, out=None):
    """A frame along a circular arc in the plane (u, v) round centre, from angle a0 to a1 (radians): width along the
    plane's normal (u x v), thickness outward from the centre (or along `out`)."""
    c, u, v = Vector(centre), Vector(u).normalized(), Vector(v).normalized()
    w = u.cross(v)

    def f(s):
        a = a0 + (a1 - a0) * s
        r = u * math.cos(a) + v * math.sin(a)
        return c + r * radius, w, (Vector(out) if out is not None else r), 1.0
    return f


def polyline(points, across, out=None):
    """A frame along a polyline (ship metres), width along `across` (or a function of the point), thickness along
    `out` (or across x direction)."""
    pts = [Vector(p) for p in points]
    seg = [(b - a).length for a, b in zip(pts[:-1], pts[1:])]
    total = sum(seg)

    def f(s):
        d = s * total
        for a, b, ln in zip(pts[:-1], pts[1:], seg):
            if d <= ln or b is pts[-1]:
                t = min(d / max(ln, 1e-9), 1.0)
                p = a + (b - a) * t
                acr = Vector(across(p) if callable(across) else across).normalized()
                o = Vector(out(p) if callable(out) else out) if out is not None else (b - a).normalized().cross(acr)
                return p, acr, o.normalized(), 1.0
            d -= ln
        return pts[-1], Vector(across), Vector(out or (0, 0, 1)), 1.0
    return f


def length_of(frame, n=24):
    pts = [frame(i / n)[0] for i in range(n + 1)]
    return sum((q - p).length for p, q in zip(pts[:-1], pts[1:]))


class Part:
    """One exported mesh: the faces of many boards, their material slots and the part's collision hulls."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.col = self.bm.loops.layers.float_color.new("Col")
        self.slots = []
        self.ucx = []
        self.count = 0

    def slot(self, mat):
        if mat not in self.slots:
            self.slots.append(mat)
        return self.slots.index(mat)

    # --- faces -------------------------------------------------------------------------------------------------------
    def face(self, verts, mat, uvs=None, cols=None, smooth=False):
        try:
            f = self.bm.faces.new(verts)
        except ValueError:
            return None
        f.material_index = self.slot(mat)
        f.smooth = smooth
        for k, loop in enumerate(f.loops):
            if uvs:
                loop[self.uv].uv = uvs[k]
            loop[self.col] = cols[k] if cols else (0.0, 0.0, 0.5, 1.0)
        return f

    def polygon(self, pts, mat, tone=0.5, grime=None, uv_scale=1.0, smooth=False, facing=None):
        """A flat face through points (ship metres), UVs projected on its plane; `facing` turns it to face that way."""
        pts = [tuple(p) for p in pts]
        n = Vector((0.0, 0.0, 0.0))
        for p, q in zip(pts, pts[1:] + pts[:1]):
            p, q = Vector(p), Vector(q)
            n += Vector(((p.y - q.y) * (p.z + q.z), (p.z - q.z) * (p.x + q.x), (p.x - q.x) * (p.y + q.y)))
        if facing is not None and n.dot(Vector(facing)) < 0.0:
            pts.reverse()
            n = -n
        vs = [self.bm.verts.new(Vector(p)) for p in pts]
        n = n.normalized() if n.length > 1e-9 else Vector((0.0, 0.0, 1.0))
        u = n.cross(Vector((0.0, 0.0, 1.0)) if abs(n.z) < 0.9 else Vector((1.0, 0.0, 0.0))).normalized()
        v = n.cross(u)
        uvs = [(Vector(p).dot(u) * uv_scale, Vector(p).dot(v) * uv_scale) for p in pts]
        cols = [(0.0, grime(Vector(p)) if grime else 0.0, tone, 1.0) for p in pts]
        return self.face(vs, mat, uvs, cols, smooth)

    # --- swept sections ----------------------------------------------------------------------------------------------
    def sweep(self, frame, length, section, mat, seed, seg=0.5, wear=1.0, tone=None, grime=None, bend=0.0, twist=0.0,
              drift=0.04, wobble=0.01, slant=0.02, caps=(True, True), smooth_top=None, uniform=False):
        """Sweep a closed section along frame(s) -> (P, A, N, width_scale), s in [0, 1] from end to end.

        section: [(y, z, kind)] counter-clockwise in (A, N) (A to the right, N up), kind 'c' marks a chamfer point
        (edge wear and smooth shading), 't' a top point, '' a plain corner."""
        rng = random.Random(seed)
        n = max(2, int(math.ceil(length / seg)))
        tone = rng.uniform(0.0, 1.0) if tone is None else tone
        w_drift, edge_a, edge_b, t_drift, bend_w = Wave(rng), Wave(rng, 3, 2.0), Wave(rng, 3, 2.0), Wave(rng), Wave(rng, 2, 0.5)
        wear_w = Wave(rng, 3, 3.0)
        b_n, b_a = rng.uniform(-1.0, 1.0) * bend, rng.uniform(-1.0, 1.0) * bend * 0.6
        tw = math.radians(rng.uniform(-1.0, 1.0) * twist)
        sl0, sl1 = rng.uniform(-slant, slant), rng.uniform(-slant, slant)
        hw = max(abs(p[0]) for p in section)
        rings, ss = [], []
        for i in range(n + 1):
            s = i / n
            p, a, nn, ws = frame(s)
            p, a, nn = Vector(p), Vector(a), Vector(nn)
            t = (Vector(frame(min(s + 0.01, 1.0))[0]) - Vector(frame(max(s - 0.01, 0.0))[0]))
            if t.length > 1e-9 and t.cross(a).dot(nn) < 0.0:
                a = -a                                         # keep (T, A, N) right-handed: faces stay outward
            sc = ws * (1.0 + drift * w_drift(s))
            ang = tw * (s - 0.5)
            ca, sa = math.cos(ang), math.sin(ang)
            off = nn * (b_n * (1.0 - 4.0 * (s - 0.5) ** 2) + 0.004 * t_drift(s)) + a * (b_a * (1.0 - 4.0 * (s - 0.5) ** 2))
            ring = []
            for y, z, kind in section:
                yy = y * sc
                if abs(y) >= 0.6 * hw:                                     # the long edges wobble
                    yy += (edge_a(s) if y > 0 else edge_b(s)) * wobble
                zz = z * sc if uniform else z
                yr, zr = yy * ca - zz * sa, yy * sa + zz * ca
                q = p + off + a * yr + nn * zr
                if i == 0:
                    q += (frame(0.02)[0] - frame(0.0)[0]).normalized() * (sl0 * yy / max(hw, 1e-3)) if length > 0.1 else Vector()
                elif i == n:
                    q += (frame(1.0)[0] - frame(0.98)[0]).normalized() * (sl1 * yy / max(hw, 1e-3)) if length > 0.1 else Vector()
                ring.append(q)
            rings.append(ring)
            ss.append(s * length)
        verts = [[self.bm.verts.new(q) for q in ring] for ring in rings]
        m = len(section)
        # Perimeter coordinate for V.
        per = [0.0]
        for k in range(m):
            (y0, z0, _), (y1, z1, _) = section[k], section[(k + 1) % m]
            per.append(per[-1] + math.hypot(y1 - y0, z1 - z0))
        du, dv = rng.uniform(0.0, 4.0), rng.uniform(0.0, 4.0)
        smooth_top = (rng.random() < 0.55) if smooth_top is None else smooth_top
        for i in range(n):
            for k in range(m):
                k1 = (k + 1) % m
                ka, kb = section[k][2], section[k1][2]
                dy, dz = section[k1][0] - section[k][0], section[k1][1] - section[k][1]
                diagonal = abs(dy) > 1e-5 and abs(dz) > 1e-5                     # only the slanted facets are worn edges
                chamfer = diagonal and (ka == "c" and kb == "c" or (ka == "c" and kb == "t") or (ka == "t" and kb == "c"))
                top = ka in "tc" and kb in "tc" and ka != "" and kb != ""
                quad = [verts[i][k], verts[i][k1], verts[i + 1][k1], verts[i + 1][k]]
                uvs = [(ss[i] + du, per[k] + dv), (ss[i] + du, per[k + 1] + dv), (ss[i + 1] + du, per[k + 1] + dv), (ss[i + 1] + du, per[k] + dv)]
                cols = []
                for vi, si in ((verts[i][k], i), (verts[i][k1], i), (verts[i + 1][k1], i + 1), (verts[i + 1][k], i + 1)):
                    r = 0.0
                    if chamfer:
                        r = wear * min(max(0.55 + 0.9 * wear_w(si / n), 0.0), 1.0)
                    g = grime(vi.co) if grime else 0.0
                    cols.append((r, g, tone, 1.0))
                self.face(quad, mat, uvs, cols, smooth=(chamfer or (top and smooth_top)))
        for end, ring in ((0, verts[0]), (1, verts[-1])):
            if not caps[end]:
                continue
            vs = list(reversed(ring)) if end == 0 else ring
            sec = list(reversed(section)) if end == 0 else section
            uvs = [(y + du, z + dv) for y, z, _ in sec]
            cols = [(0.3 * wear, grime(v.co) if grime else 0.0, tone, 1.0) for v in vs]
            self.face(vs, mat, uvs, cols)
        self.count += 1
        return tone

    def board(self, frame, length, width, thick, mat, seed, chamfer=None, cup=0.006, groove=0.004, **kw):
        """A board lying on its back (thickness along N): chamfered top edges, the top a little cupped and grooved."""
        rng = random.Random(seed * 7 + 3)
        hw, ht = width * 0.5, thick * 0.5
        c = min(chamfer if chamfer is not None else thick * rng.uniform(0.18, 0.32), ht * 0.9, hw * 0.4)
        rounded = rng.random() < 0.55
        k = 0.29 if rounded else 0.5
        cupd = rng.uniform(0.0, cup)
        g = rng.uniform(0.0, groove)
        gy = rng.uniform(-0.3, 0.3) * hw
        sec = [(-hw, -ht, ""), (hw, -ht, ""), (hw, ht - c, "c"), (hw - c * k, ht - c * k, "c"), (hw - c, ht, "t"),
               (0.45 * hw, ht - cupd * 0.6, "t"), (gy, ht - cupd - g, "t"), (-0.45 * hw, ht - cupd * 0.6, "t"),
               (-hw + c, ht, "t"), (-hw + c * k, ht - c * k, "c"), (-hw, ht - c, "c")]
        kw.setdefault("wobble", min(0.012, width * 0.025))
        return self.sweep(frame, length, sec, mat, seed, **kw)

    def beam(self, frame, length, width, thick, mat, seed, chamfer=None, **kw):
        """A squared timber (beam, post, rail): all four long edges chamfered."""
        rng = random.Random(seed * 5 + 1)
        hw, ht = width * 0.5, thick * 0.5
        c = min(chamfer if chamfer is not None else min(width, thick) * rng.uniform(0.12, 0.22), hw * 0.45, ht * 0.45)
        k = 0.29 if rng.random() < 0.6 else 0.5
        sec = [(-hw + c, -ht, "c"), (hw - c, -ht, "c"), (hw - c * k, -ht + c * k, "c"), (hw, -ht + c, "c"),
               (hw, ht - c, "c"), (hw - c * k, ht - c * k, "c"), (hw - c, ht, "c"), (-hw + c, ht, "c"),
               (-hw + c * k, ht - c * k, "c"), (-hw, ht - c, "c"), (-hw, -ht + c, "c"), (-hw + c * k, -ht + c * k, "c")]
        kw.setdefault("wobble", min(0.01, width * 0.02))
        kw.setdefault("smooth_top", True)
        return self.sweep(frame, length, sec, mat, seed, **kw)

    def round(self, frame, length, radius, mat, seed, sides=8, **kw):
        """A round section (rope, rail, spar, bolt): `radius` may be a number or a function of s."""
        rad = radius if callable(radius) else (lambda s, r=radius: r)

        def fr(s):
            p, a, nn, w = frame(s)
            return p, a, nn, rad(s) / max(rad(0.5), 1e-6)
        r0 = rad(0.5)
        sec = [(r0 * math.cos(2 * math.pi * k / sides), r0 * math.sin(2 * math.pi * k / sides), "c") for k in range(sides)]
        kw.setdefault("wobble", 0.0)
        kw.setdefault("drift", 0.0)
        kw.setdefault("smooth_top", True)
        kw.setdefault("uniform", True)
        return self.sweep(fr, length, sec, mat, seed, **kw)

    def block(self, x0, x1, y0, y1, z0, z1, mat, seed, axis="x", collide=False, **kw):
        """A chamfered timber block between the bounds (ship metres), its grain along `axis`."""
        c = Vector((0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1)))
        size = {"x": x1 - x0, "y": y1 - y0, "z": z1 - z0}
        X, Y, Zv = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
        if axis == "x":
            a, b, across, out, w, t = c - X * size["x"] / 2, c + X * size["x"] / 2, Y, Zv, size["y"], size["z"]
        elif axis == "y":
            a, b, across, out, w, t = c - Y * size["y"] / 2, c + Y * size["y"] / 2, -X, Zv, size["x"], size["z"]
        else:
            a, b, across, out, w, t = c - Zv * size["z"] / 2, c + Zv * size["z"] / 2, X, Y, size["x"], size["y"]
        self.beam(straight(a, b, across, out), (b - a).length, w, t, mat, seed, **kw)
        if collide:
            self.collide_box(x0, x1, y0, y1, z0, z1)

    def lathe(self, profile, centre, axis, mat, seed=0, sides=16, wear=0.6, tone=None, grime=None, smooth=True, caps=True):
        """A turned or carved round shape: profile [(r, h)] from the base along `axis` (finials, domes, balusters, the
        brass drop, hubs). r = 0 ends close to a point."""
        rng = random.Random(seed)
        tone = rng.uniform(0.0, 1.0) if tone is None else tone
        ax = Vector(axis).normalized()
        u = ax.cross(Vector((0.0, 0.0, 1.0)) if abs(ax.z) < 0.9 else Vector((1.0, 0.0, 0.0))).normalized()
        v = ax.cross(u)
        c = Vector(centre)
        rows = []
        for r, h in profile:
            rows.append([self.bm.verts.new(c + ax * h + (u * math.cos(2 * math.pi * k / sides) + v * math.sin(2 * math.pi * k / sides)) * r)
                         for k in range(sides)])
        hs = [h for _, h in profile]
        for i in range(len(rows) - 1):
            for k in range(sides):
                k1 = (k + 1) % sides
                quad = [rows[i][k], rows[i][k1], rows[i + 1][k1], rows[i + 1][k]]
                uvs = [(k / sides * 2.0, hs[i]), (k1 / sides * 2.0, hs[i]), (k1 / sides * 2.0, hs[i + 1]), (k / sides * 2.0, hs[i + 1])]
                edge = abs(profile[i + 1][0] - profile[i][0]) > 0.6 * abs(hs[i + 1] - hs[i]) + 1e-4
                cols = [((wear if edge else 0.0), grime(q.co) if grime else 0.0, tone, 1.0) for q in quad]
                self.face(quad, mat, uvs, cols, smooth)
        if caps:
            if profile[0][0] > 1e-4:
                self.face(list(reversed(rows[0])), mat, [(0.0, 0.0)] * sides, [(0.0, 0.0, tone, 1.0)] * sides)
            if profile[-1][0] > 1e-4:
                self.face(rows[-1], mat, [(0.0, 0.0)] * sides, [(0.0, 0.0, tone, 1.0)] * sides)

    # --- collision ---------------------------------------------------------------------------------------------------
    def collide_box(self, x0, x1, y0, y1, z0, z1):
        self.ucx.append([Vector((x, y, z)) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)])

    def collide_points(self, pts):
        self.ucx.append([Vector(p) for p in pts])

    # --- output ------------------------------------------------------------------------------------------------------
    def finish(self, materials, ucx=True, pivot=(PIVOT_X, 0.0, 0.0)):
        """Move to Blender (the mesh's pivot at `pivot`, ship metres; the ship's x 80 by default), reverse the windings
        (the Y mirror flips them), make the object and its UCX hulls."""
        px, py, pz = pivot
        for v in self.bm.verts:
            v.co = Vector((v.co.x - px, -(v.co.y - py), v.co.z - pz))
        bmesh.ops.reverse_faces(self.bm, faces=self.bm.faces, flip_multires=False)
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me)
        self.bm.free()
        obj = sb.new_object(self.name, me)
        for s in self.slots:
            obj.data.materials.append(materials(s))
        if "Col" in me.color_attributes:
            me.color_attributes.active_color = me.color_attributes["Col"]
        objs = [obj]
        if ucx:
            for k, pts in enumerate(self.ucx):
                hb = bmesh.new()
                for p in pts:
                    hb.verts.new(Vector((p.x - px, -(p.y - py), p.z - pz)))
                bmesh.ops.convex_hull(hb, input=list(hb.verts))
                hm = bpy.data.meshes.new("UCX_%s_%03d" % (self.name, k))
                hb.to_mesh(hm)
                hb.free()
                ho = sb.new_object("UCX_%s_%03d" % (self.name, k), hm)
                ho.display_type = "WIRE"
                objs.append(ho)
        print("SB_PART", self.name, "boards", self.count, "verts", len(me.vertices), "faces", len(me.polygons), "ucx", len(objs) - 1)
        return objs
