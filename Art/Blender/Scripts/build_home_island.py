"""The home island's ground (D60): the ring of land round the lagoon, its beaches, reef and channel, and where its plants and
rocks stand. Shape: island_shape.py.

  SM_Home_Sand   the visible sand and sea floor (Nanite): 0.5 m cells wherever the ground comes within 4.5 m of the
                 water (the beaches, the land under the grass, the lagoon's rim, the reef flat, the channel), 2 m cells
                 in the deep lagoon and down the drop-off (overlapping the fine part's border 6 cm lower, so no seam
                 shows). Vertex colours as the cove's sand (MI_Sand): R lit ripple crests, G shaded troughs, B tone,
                 A 1 = sea-floor sand / 0 = beach sand; the dry beach has its wind ripples and swash ridges (D37).
  SM_Home_Turf   the grass over the inland (Nanite, MI_Turf): 0.5 m cells, 6 cm over the sand, its ragged edge rolling
                 down under the sand (a chunky lip, no blend). Vertex colours: R lit rises, G hollows, B tone, A 1 =
                 grass / 0 = forest floor (MI_Turf's second texture set, under the trees).
  SM_Home_Col    the broad shape for collision (hidden in the map): 1 m cells where it comes within 3 m of the water,
                 2 m cells deeper (overlapping 3 cm lower).
  Art/Generated/home_island.json   where the plants, the rocks and the lagoon's sea grass go, read by
                 Tools/ue_scripts/create_visual_test_map.py: plants {mesh: [[x, y, z, yaw, pitch, roll, scale], ...]} and
                 seagrass {A|B|C: [...]} in Unreal cm and degrees; rocks [[mesh, x, y, z, yaw, scale], ...].

Origin = the world origin at the water level (placed at 0, 0, 0). Unreal metres in the shape (Blender Y = -Unreal Y).
Run: Tools/Build-BlenderAsset.ps1 build_home_island [sand] [turf] [col] [scatter]
"""
import json
import math
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import island_shape as S  # noqa: E402

try:
    import bpy
    import sb_blender as sb
except ImportError:          # the scatter also runs in the host Python
    bpy = None
    sb = None

FINE, COARSE, BLOCK = 0.5, 2.0, 2.0
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SCATTER_JSON = os.path.join(REPO, "Art", "Generated", "home_island.json")


def lattice(cell):
    n = int(round(2 * S.HALF / cell))
    xs = S.CENTRE[0] - S.HALF + np.arange(n + 1) * cell
    ys = S.CENTRE[1] - S.HALF + np.arange(n + 1) * cell
    X, Y = np.meshgrid(xs, ys, indexing="ij")          # [i, j]: i along X (north), j along Y (east)
    return X, Y, n


def block_mask(z_fine, threshold, dilate=1):
    """Per 2 m block: True where the fine heights come within reach of the water."""
    k = int(round(BLOCK / FINE))
    n = (z_fine.shape[0] - 1) // k
    zmax = np.full((n, n), -1e9)
    for a in range(k + 1):
        for b in range(k + 1):
            zmax = np.maximum(zmax, z_fine[a:a + n * k:k, b:b + n * k:k][:n, :n])
    m = zmax > threshold
    for _ in range(dilate):
        g = m.copy()
        g[1:, :] |= m[:-1, :]
        g[:-1, :] |= m[1:, :]
        g[:, 1:] |= m[:, :-1]
        g[:, :-1] |= m[:, 1:]
        m = g
    return m


def grow(m, steps=1):
    for _ in range(steps):
        g = m.copy()
        g[1:, :] |= m[:-1, :]
        g[:-1, :] |= m[1:, :]
        g[:, 1:] |= m[:, :-1]
        g[:, :-1] |= m[:, 1:]
        m = g
    return m


def cells_to_mesh(name, X, Y, Z, cell_keep, cols=None, material="MI_Sand"):
    """A grid mesh of the kept cells (cell_keep[i, j] = the quad from vertex (i, j) to (i+1, j+1)). Returns the object,
    or the raw arrays without Blender."""
    ni, nj = cell_keep.shape
    used = np.zeros((ni + 1, nj + 1), dtype=bool)
    ci, cj = np.nonzero(cell_keep)
    for a in (0, 1):
        for b in (0, 1):
            used[ci + a, cj + b] = True
    index = -np.ones(used.shape, dtype=np.int64)
    index[used] = np.arange(used.sum())
    verts = np.stack([X[used], -Y[used], Z[used]], -1)          # Blender Y = -Unreal Y
    # Winding as the cove's seabed (faces up after the Y mirror).
    faces = np.stack([index[ci, cj], index[ci, cj + 1], index[ci + 1, cj + 1], index[ci + 1, cj]], -1)
    vcols = cols[used] if cols is not None else None
    return verts, faces, vcols


def mesh_object(name, parts, material):
    verts = np.concatenate([p[0] for p in parts])
    faces, off = [], 0
    for p in parts:
        faces.append(p[1] + off)
        off += len(p[0])
    faces = np.concatenate(faces)
    cols = None
    if all(p[2] is not None for p in parts):
        cols = np.concatenate([p[2] for p in parts])
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(verts))
    me.vertices.foreach_set("co", verts.astype(np.float32).ravel())
    me.loops.add(len(faces) * 4)
    me.loops.foreach_set("vertex_index", faces.astype(np.int32).ravel())
    me.polygons.add(len(faces))
    me.polygons.foreach_set("loop_start", (np.arange(len(faces)) * 4).astype(np.int32))
    me.update(calc_edges=True)
    me.validate()
    me.shade_smooth()
    obj = sb.new_object(name, me)
    obj.data.materials.append(sb.get_material(material, (0.76, 0.68, 0.52, 1)))
    vi = np.empty(len(me.loops), dtype=np.int32)
    me.loops.foreach_get("vertex_index", vi)
    uv = me.uv_layers.new(name="UVMap")
    uv.data.foreach_set("uv", verts[vi, :2].astype(np.float32).ravel())   # top projection (tangents only)
    if cols is not None:
        attr = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
        attr.data.foreach_set("color", cols.astype(np.float32).ravel())
        me.color_attributes.active_color = attr
    print("SB_HOME", name, "verts", len(verts), "faces", len(faces))
    return obj


def fine_fields():
    t = time.time()
    X, Y, n = lattice(FINE)
    F = S.fields(X, Y)
    print("SB_HOME fields", X.shape, round(time.time() - t, 1), "s")
    return X, Y, F


def sand(X, Y, F):
    z = F["z"]
    dz, crest, trough, tone = S.sand_detail(X, Y, z)
    w = S.beach_weight(z)
    cols = np.stack([crest, trough, 0.5 + (tone - 0.5) * w, 1.0 - w], -1)
    fine_blocks = block_mask(z, -4.5)
    k = int(round(BLOCK / FINE))
    fine_cells = np.repeat(np.repeat(fine_blocks, k, 0), k, 1)
    parts = [cells_to_mesh("fine", X, Y, z + dz, fine_cells, cols)]
    Xc, Yc, nc = lattice(COARSE)
    zc = S.base(Xc, Yc) - 0.06
    coarse_cells = grow(~fine_blocks, 1)
    colc = np.zeros(Xc.shape + (4,))
    colc[..., 2] = 0.5
    colc[..., 3] = 1.0 - S.beach_weight(zc)
    parts.append(cells_to_mesh("coarse", Xc, Yc, zc, coarse_cells, colc))
    sb.reset_scene()
    obj = mesh_object("SM_Home_Sand", parts, "MI_Sand")
    sb.save_blend("SM_Home_Sand")
    sb.export_fbx([obj], os.path.join("Islands", "SM_Home_Sand.fbx"))


def turf_layer(X, Y, F):
    """Height and colours of the grass: (z, cols, t)."""
    z = F["z"]
    t = S.turf(F, X, Y)
    # Little grass hummocks, only where the turf is whole.
    hum = 0.05 * np.maximum(S.noise(X, Y, 1.3, 61) + 0.4 * S.noise(X, Y, 0.55, 62), 0.0)
    zt = z + 0.06 * t - 0.28 * (1.0 - t) + hum * t
    # Lit rises and shaded hollows from the curvature at about 2 m.
    lap = (np.roll(z, 4, 0) + np.roll(z, -4, 0) + np.roll(z, 4, 1) + np.roll(z, -4, 1) - 4 * z) / 4.0
    lit = np.clip(-lap * 4.0, 0.0, 1.0)
    hollow = np.clip(lap * 4.0, 0.0, 1.0)
    tone = 0.5 + 0.5 * S.noise(X, Y, 6.0, 63)
    fo = S.forest(F, X, Y)
    cols = np.stack([lit, hollow, tone, 1.0 - 0.9 * fo], -1)
    return zt, cols, t


def turf(X, Y, F):
    zt, cols, t = turf_layer(X, Y, F)
    tmax = np.maximum(np.maximum(t[:-1, :-1], t[1:, :-1]), np.maximum(t[:-1, 1:], t[1:, 1:]))
    keep = tmax > 0.35
    sb.reset_scene()
    obj = mesh_object("SM_Home_Turf", [cells_to_mesh("turf", X, Y, zt, keep, cols)], "MI_Turf")
    sb.save_blend("SM_Home_Turf")
    sb.export_fbx([obj], os.path.join("Islands", "SM_Home_Turf.fbx"))


def collision():
    Xf, Yf, nf = lattice(1.0)
    zf = S.base(Xf, Yf)
    k = int(round(BLOCK / 1.0))
    fine_blocks = block_mask_cell(zf, -3.0, k)
    fine_cells = np.repeat(np.repeat(fine_blocks, k, 0), k, 1)
    parts = [cells_to_mesh("fine", Xf, Yf, zf, fine_cells)]
    Xc, Yc, nc = lattice(COARSE)
    zc = S.base(Xc, Yc) - 0.03
    parts.append(cells_to_mesh("coarse", Xc, Yc, zc, grow(~fine_blocks, 1)))
    sb.reset_scene()
    obj = mesh_object("SM_Home_Col", parts, "MI_Sand")
    sb.save_blend("SM_Home_Col")
    sb.export_fbx([obj], os.path.join("Islands", "SM_Home_Col.fbx"))


def block_mask_cell(z, threshold, k):
    n = (z.shape[0] - 1) // k
    zmax = np.full((n, n), -1e9)
    for a in range(k + 1):
        for b in range(k + 1):
            zmax = np.maximum(zmax, z[a:a + n * k:k, b:b + n * k:k][:n, :n])
    return grow(zmax > threshold, 1)


# --- where things stand -----------------------------------------------------------------------------------------
class Placer:
    """Jittered candidates, kept by a density and a spacing to everything already placed (a spatial hash)."""

    def __init__(self, seed):
        self.rng = np.random.default_rng(seed)
        self.items = []          # (x, y, radius)
        self.hash = {}

    def free(self, x, y, radius):
        cx, cy = int(math.floor(x / 10.0)), int(math.floor(y / 10.0))
        for a in range(cx - 1, cx + 2):
            for b in range(cy - 1, cy + 2):
                for (px, py, pr) in self.hash.get((a, b), ()):
                    if (px - x) ** 2 + (py - y) ** 2 < (max(pr, radius)) ** 2:
                        return False
        return True

    def add(self, x, y, radius):
        self.hash.setdefault((int(math.floor(x / 10.0)), int(math.floor(y / 10.0))), []).append((x, y, radius))

    def candidates(self, spacing):
        n = int(2 * S.HALF / spacing)
        g = (np.arange(n) + 0.5) * spacing - S.HALF
        X, Y = np.meshgrid(S.CENTRE[0] + g, S.CENTRE[1] + g, indexing="ij")
        X = X + self.rng.uniform(-0.5, 0.5, X.shape) * spacing
        Y = Y + self.rng.uniform(-0.5, 0.5, Y.shape) * spacing
        order = self.rng.permutation(X.size)
        return X.ravel()[order], Y.ravel()[order]


def ground_z(x, y):
    """The visible surface: the turf where it covers, else the sand."""
    F = S.fields(x, y)
    z = F["z"]
    t = S.turf(F, x, y)
    dz = S.sand_detail(x, y, z)[0]
    return np.where(t > 0.6, z + 0.06, z + dz), F, t


def scatter():
    t0 = time.time()
    P = Placer(60)
    out = {"lagoon": [S.CENTRE[0] * 100.0, S.CENTRE[1] * 100.0], "plants": {}, "rocks": [], "seagrass": {"A": [], "B": [], "C": []}}

    def put(mesh, x, y, z, yaw, scale, pitch=0.0, roll=0.0):
        out["plants"].setdefault(mesh, []).append([round(x * 100, 1), round(y * 100, 1), round(z * 100, 1), round(yaw, 1),
                                                   round(pitch, 1), round(roll, 1), round(scale, 3)])

    landing = (-17.0, 0.0)

    def kind(spacing, density, radius, meshes, sink=0.05, scale=(0.85, 1.15), yaw_fn=None, tilt=0.0, keep_out=0.0):
        xs, ys = P.candidates(spacing)
        z, F, t = ground_z(xs, ys)
        p = density(F, xs, ys, z, t)
        roll = P.rng.uniform(0, 1, xs.shape)
        placed = 0
        for k in np.nonzero(roll < p)[0]:
            x, y = float(xs[k]), float(ys[k])
            if math.hypot(x - landing[0], y - landing[1]) < keep_out:
                continue
            if not P.free(x, y, radius):
                continue
            P.add(x, y, radius)
            mesh = meshes[int(P.rng.integers(len(meshes)))]
            yaw = yaw_fn(F, k, x, y) if yaw_fn else float(P.rng.uniform(0, 360))
            put(mesh, x, y, float(z[k]) - sink, yaw, float(P.rng.uniform(*scale)),
                float(P.rng.uniform(-tilt, tilt)), float(P.rng.uniform(-tilt, tilt)))
            placed += 1
        return placed

    def away(F):
        return (1.0 - F["gap"]) * S.smooth((F["path"] - 2.4) / 0.8)

    # Trees where the forest stands, a few alone inland.
    n_tree = kind(3.0, lambda F, x, y, z, t: (0.55 * S.forest(F, x, y) + 0.03 * F["inland"]) * (t > 0.7) * away(F),
                  7.5, ["SM_Tree_A", "SM_Tree_B"], sink=0.15, scale=(0.85, 1.2), keep_out=14.0)

    # Palms along both shores' backs, leaning to the nearer water; a few inland and on the hill.
    def palm_density(F, x, y, z, t):
        # In groves along the shores, with open stretches of beach between them (not a fence of palms).
        shore = np.exp(-((F["e"] - 11.0) / 9.0) ** 2) + np.exp(-((F["f"] - 13.0) / 10.0) ** 2)
        grove = S.smooth((S.noise(x, y, 11.0, 71) + 0.45 * S.noise(x, y, 4.0, 72) + 0.05) / 0.4)
        return (0.5 * np.minimum(shore, 1.0) * grove + 0.012 * F["inland"]) * (z > 0.45) * away(F)

    def palm_yaw(F, k, x, y):
        r, th = S.polar(x, y)
        out_deg = math.degrees(th)
        toward = out_deg if F["f"][k] < F["e"][k] else out_deg + 180.0
        return toward + float(P.rng.uniform(-40, 40))

    n_palm = kind(2.0, palm_density, 3.8, ["SM_Palm_A", "SM_Palm_A", "SM_Palm_B", "SM_Palm_C", "SM_Palm_C"], sink=0.1,
                  scale=(0.7, 1.25), yaw_fn=palm_yaw, keep_out=5.0)
    # The islet's palms.
    ix, iy, ir = S.ISLET
    for k, (dx, dy, mesh) in enumerate(((-2.0, 1.5, "SM_Palm_B"), (2.5, -1.0, "SM_Palm_A"), (0.5, 3.5, "SM_Palm_C"))):
        x, y = ix + dx, iy + dy
        z, F, t = ground_z(np.array([x]), np.array([y]))
        P.add(x, y, 3.0)
        put(mesh, x, y, float(z[0]) - 0.1, math.degrees(math.atan2(dy, dx)) + 10.0 * k, 1.0 - 0.08 * k)
    # Big-leaf plants at the forest's edges, bushes over the grass, flowering bushes along the path, ferns under the trees.
    n_big = kind(1.6, lambda F, x, y, z, t: (0.28 * S.forest(F, x, y) + 0.05) * (t > 0.7) * away(F),
                 2.6, ["SM_BigLeaf_A"], sink=0.05, keep_out=6.0)
    n_bush = kind(1.4, lambda F, x, y, z, t: (0.1 + 0.16 * F["inland"]) * (t > 0.7) * away(F),
                  2.4, ["SM_Bush_A", "SM_Bush_B"], sink=0.1, scale=(0.75, 1.25), keep_out=6.0)
    n_flower = kind(1.4, lambda F, x, y, z, t: (0.05 + 0.5 * np.exp(-((F["path"] - 3.4) / 1.2) ** 2)) * (t > 0.6)
                    * (1.0 - F["gap"]) * (F["path"] > 2.6),
                    2.2, ["SM_Bush_Flower"], sink=0.1, scale=(0.8, 1.15), keep_out=4.0)
    n_fern = kind(1.0, lambda F, x, y, z, t: (0.3 * S.forest(F, x, y) + 0.05 * F["inland"]) * (t > 0.6) * away(F),
                  1.5, ["SM_Fern_A"], sink=0.04, scale=(0.8, 1.3))
    # Beach grass on the dunes and along the grass's edge.
    def grass_density(F, x, y, z, t):
        dune = (z > 0.35) * (z < 2.2) * (np.exp(-((F["e"] - 8.0) / 4.0) ** 2) + np.exp(-((F["f"] - 10.0) / 5.0) ** 2))
        edge = np.exp(-((t - 0.65) / 0.25) ** 2)
        return np.minimum(0.55 * dune + 0.4 * edge * (t > 0.1) + 0.06 * t, 0.85) * (z > 0.3) * (1.0 - F["gap"]) * (F["path"] > PATH_CLEAR)

    PATH_CLEAR = S.PATH_HALF + 0.4
    n_grass = kind(0.8, grass_density, 0.7, ["SM_GrassTuft_A", "SM_GrassTuft_B"], sink=0.03, scale=(0.75, 1.3),
                   tilt=6.0, keep_out=2.5)

    # Rocks: the hilltop's crown, both headlands at the channel, a few in the surf on the windward (west) coast, coral
    # heads in the lagoon. (mesh, x, y, yaw, scale, sink as a share of the scale in metres)
    rocks = []
    b = math.radians(-90.0)
    rr = S.inner_radius(b) + 0.52 * S.band_width(b)
    hx, hy = S.CENTRE[0] + rr * math.cos(b), S.CENTRE[1] + rr * math.sin(b)
    rocks += [("A", hx + 6.0, hy - 3.0, 40, 1.9, 0.55), ("B", hx - 5.0, hy + 4.5, 200, 1.6, 0.5), ("C", hx + 1.0, hy + 8.0, 120, 2.0, 0.6)]
    for side in (-1.0, 1.0):
        for k, (along, outwards, mesh, sc) in enumerate(((2.0, 0.0, "A", 2.2), (7.0, 14.0, "B", 1.8), (4.0, -12.0, "C", 1.6),
                                                          (11.0, 30.0, "A", 1.5))):
            r_mid = S.inner_radius(S.GAP_ANGLE) + 0.5 * S.band_width(S.GAP_ANGLE) + outwards
            a = S.GAP_ANGLE + side * (S.GAP_HALF + along) / r_mid
            rocks.append((mesh, S.CENTRE[0] + r_mid * math.cos(a), S.CENTRE[1] + r_mid * math.sin(a), 70 * k + 90 * side, sc, 0.45))
    for k, deg in enumerate((-120.0, -100.0, -72.0, -48.0, 160.0)):
        a = math.radians(deg)
        r = S.outer_radius(a) + 3.0 + 2.0 * (k % 2)
        rocks.append(("ABC"[k % 3], S.CENTRE[0] + r * math.cos(a), S.CENTRE[1] + r * math.sin(a), 50 * k, 1.8 + 0.25 * (k % 3), 0.45))
    for k, (x, y) in enumerate(((60.0, 40.0), (120.0, 60.0), (70.0, -50.0), (150.0, 10.0))):
        rocks.append(("C", x, y, 80 * k, 0.9 + 0.15 * k, 0.4))
    for mesh, x, y, yaw, sc, sink in rocks:
        z = float(S.base(np.array([x]), np.array([y]))[0])
        out["rocks"].append(["SM_Rock_Coast_" + mesh, round(x * 100, 1), round(y * 100, 1), round((z - sink * sc) * 100, 1), yaw, sc])

    # Sea grass in the lagoon, 1.5-5.5 m down: patches, never under the pier or in the channel.
    rng = np.random.default_rng(61)
    patches = 0
    while patches < 46:
        cx, cy = rng.uniform(-20.0, 210.0), rng.uniform(-110.0, 120.0)
        F = S.fields(np.array([cx]), np.array([cy]))
        if not (-5.5 < F["z"][0] < -1.5) or F["e"][0] > -6.0 or F["gap"][0] > 0.0:
            continue
        patches += 1
        radius = rng.uniform(2.0, 5.0)
        for _ in range(int(rng.integers(6, 20))):
            rr_ = radius * math.sqrt(rng.random())
            a = rng.uniform(0.0, 2.0 * math.pi)
            x, y = cx + rr_ * math.cos(a), cy + rr_ * math.sin(a)
            if -4.5 < x < 17.0 and abs(y) < 3.5:
                continue
            z = float(S.base(np.array([x]), np.array([y]))[0])
            if not -6.0 < z < -1.2:
                continue
            centre = 1.0 - rr_ / radius
            k = "A" if rng.random() < 0.25 + 0.5 * centre else ("B" if rng.random() < 0.6 else "C")
            sc = rng.uniform(0.8, 1.25) * (1.0 + min(-z / 14.0, 1.0) * 0.4)
            out["seagrass"][k].append([round(x * 100, 1), round(y * 100, 1), round(z * 100 - 3.0, 1),
                                       round(rng.uniform(0, 360), 1), round(rng.uniform(-6, 6), 1), round(rng.uniform(-6, 6), 1), round(sc, 3)])
    os.makedirs(os.path.dirname(SCATTER_JSON), exist_ok=True)
    with open(SCATTER_JSON, "w", encoding="utf-8") as fh:
        json.dump(out, fh, separators=(",", ":"))
    counts = {k: len(v) for k, v in out["plants"].items()}
    print("SB_HOME scatter", counts, "rocks", len(out["rocks"]), "seagrass", {k: len(v) for k, v in out["seagrass"].items()},
          "trees", n_tree, "palms", n_palm, "big", n_big, "bush", n_bush, "flower", n_flower, "fern", n_fern, "grass", n_grass,
          round(time.time() - t0, 1), "s")


if __name__ == "__main__":
    args = (sb.script_args() if sb else sys.argv[1:]) or ["sand", "turf", "col", "scatter"]
    if "scatter" in args:
        scatter()
    if sb and ("sand" in args or "turf" in args):
        X, Y, F = fine_fields()
        if "sand" in args:
            sand(X, Y, F)
        if "turf" in args:
            turf(X, Y, F)
    if sb and "col" in args:
        collision()
    print("SB_HOME_OK", args)
