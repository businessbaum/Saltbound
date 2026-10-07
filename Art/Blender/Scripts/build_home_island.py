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
  SM_Home_Ground (D71) the whole visible ground in one Nanite mesh (replaces SM_Home_Sand and SM_Home_Turf in the map):
                 0.25 m cells on the land and the beaches, the relief of island_shape.relief() on the broad shape (berm,
                 tussocks, the forest's humps, ruts, mounds and root ridges round the trees), material M_SB_IslandGround
                 (MI_HomeGround). Vertex colours: R grass, G litter, B earth, A tone; UV1 (the sand's ripples 0.5 flat,
                 1 crest, 0 trough; the curvature 0.5 flat). The plants' heights in home_island.json follow it (rez_plants), and the
                 places (island_shape.PLACES) are kept clear.

Run: Tools/Build-BlenderAsset.ps1 build_home_island [scatter] [ground] [col] [sand] [turf]
(the scatter also runs in the host Python: python Art/Blender/Scripts/build_home_island.py scatter)
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


# --- D71: one visible ground (sand, earth, grass, litter) ------------------------------------------------------------
GROUND_FINE = 0.25            # cell (m) on the land, the beaches and the first 0.6 m of water
GROUND_SINK = {"SM_Tree": 0.15, "SM_Palm": 0.1, "SM_BigLeaf": 0.05, "SM_Bush": 0.1, "SM_Fern": 0.04, "SM_GrassTuft": 0.03,
               "SM_Runner": 0.0}


def scatter_plants(kinds=("Tree", "Palm")):
    """The placed plants of some kinds from home_island.json as rows (mesh, x, y, yaw, scale) in metres."""
    with open(SCATTER_JSON, encoding="utf-8") as fh:
        H = json.load(fh)
    rows = []
    for mesh, items in H["plants"].items():
        if any(k in mesh for k in kinds):
            rows += [(mesh, r[0] / 100.0, r[1] / 100.0, r[3], r[6]) for r in items]
    return rows


def surface(x, y, plants, F=None):
    """The visible ground at the points (m): z with its relief, the cover (grass, litter, earth), the sand's crests and
    troughs and a broad tone."""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    F = F if F is not None else S.fields(x, y)
    g, lit, earth = S.cover(F, x, y)
    pdz, plit = S.plant_relief(x, y, plants)
    lit = np.maximum(lit, plit * np.clip(1.4 * g - 0.05, 0.0, 1.0)) * (1.0 - earth)
    # The Keeper's hut: the sand trodden hard in front of the counter and round the door, no grass under the floor.
    hl, hr = S.place_local(x, y, "keeper_hut")
    yard = S.smooth((1.0 - np.maximum(np.abs(hl - 2.6) / 3.6, np.abs(hr) / 4.6)) / 0.35)
    yard = yard * (0.55 + 0.45 * S.smooth((S.vnoise(x, y, 0.6, 81) + 0.2) / 0.5))
    earth = np.maximum(earth, 0.8 * yard)
    g = g * (1.0 - S.smooth((1.0 - np.maximum(np.abs(hl + 0.2) / 3.4, np.abs(hr) / 3.4)) / 0.3))
    # The dig spot: a low mound of loose, darker sand.
    dl, dr = S.place_local(x, y, "dig_spot")
    dd = np.hypot(dl, dr * 1.2)
    dig = np.exp(-(dd / 0.55) ** 2)
    earth = np.maximum(earth, 0.45 * S.smooth((0.75 - dd) / 0.25) * (0.6 + 0.4 * S.vnoise(x, y, 0.2, 82)))
    # The spring (D71): its pool's rim levelled into the slope and a shallow bowl under the water, wet earth round it.
    sl, sr = S.place_local(x, y, "spring")
    sd = np.hypot(sl / 1.25, sr / 1.05)
    rim = S.smooth((1.45 - sd) / 0.35)
    g = g * (1.0 - S.smooth((1.2 - sd) / 0.3))
    lit = np.maximum(lit, 0.75 * S.smooth((1.9 - sd) / 0.6))
    dz, crest, trough = S.relief(F, x, y, g, lit, earth)
    dz = dz + pdz + 0.06 * dig * (1.0 + 0.3 * S.vnoise(x, y, 0.18, 83))
    sp = S.PLACES["spring"]
    zs = float(S.base(np.array([sp["x"]]), np.array([sp["y"]]))[0])
    target = zs + 0.02 - 0.18 * S.smooth((0.95 - sd) / 0.4)
    z_all = F["z"] + dz
    dz = dz + (np.minimum(z_all, target) - z_all) * rim
    tone = 0.5 + 0.35 * S.noise(x, y, 6.0, 63) + 0.15 * S.fbm(x, y, 2.5, 64)
    return {"z": F["z"] + dz, "g": g, "lit": lit, "earth": earth, "crest": crest, "trough": trough, "tone": tone, "F": F}


def curvature(z, step, gain):
    """0.5 on flat ground, more on rises, less in hollows (at about 0.5 and 1.5 m), from a lattice of heights."""
    k1, k2 = max(1, int(round(0.5 / step))), max(1, int(round(1.5 / step)))
    lap = 0.0
    for k, w in ((k1, 0.6), (k2, 0.4)):
        lap = lap + w * (np.roll(z, k, 0) + np.roll(z, -k, 0) + np.roll(z, k, 1) + np.roll(z, -k, 1) - 4 * z) / (4.0 * (k * step) ** 2)
    return np.clip(0.5 - lap * gain, 0.0, 1.0)


def ground_part(X, Y, keep, plants, offset=0.0, curv_gain=1.2):
    """A grid part of the ground: (verts, faces, cols, uv1) for the kept cells."""
    t = time.time()
    ni, nj = keep.shape
    used = np.zeros((ni + 1, nj + 1), dtype=bool)
    ci, cj = np.nonzero(keep)
    for a in (0, 1):
        for b in (0, 1):
            used[ci + a, cj + b] = True
    # Evaluate only the rows and columns in use (a box), so the curvature has neighbours.
    ii, jj = np.nonzero(used)
    i0, i1, j0, j1 = max(ii.min() - 8, 0), min(ii.max() + 9, ni + 1), max(jj.min() - 8, 0), min(jj.max() + 9, nj + 1)
    Xb, Yb = X[i0:i1, j0:j1], Y[i0:i1, j0:j1]
    G = surface(Xb, Yb, plants)
    step = float(X[1, 0] - X[0, 0])
    curv = curvature(G["z"], step, curv_gain)
    Z = np.full(X.shape, np.nan)
    Z[i0:i1, j0:j1] = G["z"] + offset

    def full(a):
        out = np.zeros(X.shape)
        out[i0:i1, j0:j1] = a
        return out

    index = -np.ones(used.shape, dtype=np.int64)
    index[used] = np.arange(used.sum())
    verts = np.stack([X[used], -Y[used], Z[used]], -1)
    faces = np.stack([index[ci, cj], index[ci, cj + 1], index[ci + 1, cj + 1], index[ci + 1, cj]], -1)
    cols = np.stack([full(G["g"])[used], full(G["lit"])[used], full(G["earth"])[used], np.clip(full(G["tone"])[used], 0, 1)], -1)
    ripple = 0.5 + 0.5 * (full(G["crest"])[used] - full(G["trough"])[used])
    uv1 = np.stack([np.clip(ripple, 0, 1), full(curv)[used]], -1)
    print("SB_HOME ground part", step, "m: verts", len(verts), "faces", len(faces), round(time.time() - t, 1), "s")
    return verts, faces, cols, uv1


def ground_object(name, parts, material):
    verts = np.concatenate([p[0] for p in parts])
    faces, off = [], 0
    for p in parts:
        faces.append(p[1] + off)
        off += len(p[0])
    faces = np.concatenate(faces)
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
    # UV0: top projection (tangents); UV1: (the sand's ripples: 0.5 flat, 1 a lit crest, 0 a trough; the curvature: 0.5
    # flat, more on rises) as data for M_SB_IslandGround.
    for layer, data in (("UVMap", verts[:, :2]), ("Data", np.concatenate([p[3] for p in parts]))):
        uv = me.uv_layers.new(name=layer)
        uv.data.foreach_set("uv", data[vi].astype(np.float32).ravel())
    attr = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    cols = np.concatenate([p[2] for p in parts])
    attr.data.foreach_set("color", cols.astype(np.float32).ravel())
    me.color_attributes.active_color = attr
    print("SB_HOME", name, "verts", len(verts), "faces", len(faces))
    return obj


def ground():
    """SM_Home_Ground (D71): the whole visible ground, sand to forest, in one Nanite mesh (replaces SM_Home_Sand and
    SM_Home_Turf in the map). 0.25 m cells where it comes within 0.6 m of the water, 0.5 m down to 4.5 m (3 cm lower,
    overlapping one block under the fine part), 2 m deeper (6 cm lower)."""
    t0 = time.time()
    plants = scatter_plants()
    X5, Y5, n5 = lattice(FINE)
    z5 = S.base(X5, Y5)
    r1 = block_mask(z5, -0.6, dilate=1)
    r2 = block_mask(z5, -4.5, dilate=1)
    r2 = (r2 & ~r1) | (grow(r2 & ~r1, 1) & r1)
    k1 = int(round(BLOCK / GROUND_FINE))
    k2 = int(round(BLOCK / FINE))
    X1, Y1, n1 = lattice(GROUND_FINE)
    parts = [ground_part(X1, Y1, np.repeat(np.repeat(r1, k1, 0), k1, 1), plants)]
    parts.append(ground_part(X5, Y5, np.repeat(np.repeat(r2, k2, 0), k2, 1), plants, offset=-0.03))
    Xc, Yc, nc = lattice(COARSE)
    zc = S.base(Xc, Yc) - 0.06
    coarse_cells = grow(~(r1 | r2), 1)
    vc_, fc_, _ = cells_to_mesh("coarse", Xc, Yc, zc, coarse_cells)
    n = len(vc_)
    colc = np.zeros((n, 4))
    colc[:, 3] = 0.5
    parts.append((vc_, fc_, colc, np.full((n, 2), 0.5)))
    sb.reset_scene()
    obj = ground_object("SM_Home_Ground", parts, "MI_HomeGround")
    sb.save_blend("SM_Home_Ground")
    sb.export_fbx([obj], os.path.join("Islands", "SM_Home_Ground.fbx"))
    print("SB_HOME ground", round(time.time() - t0, 1), "s")


def details(out):
    """D71: small things that make the ground's edges (owner: "no good transition between grass and sand"): the beach creeper
    running from the grass out over the sand. Its own placer and seed, so the plants above keep their places."""
    trunks = [(m, r[0] / 100.0, r[1] / 100.0, r[3], r[6]) for m, rows in out["plants"].items() if ("Tree" in m or "Palm" in m)
              for r in rows]
    P = Placer(71)
    for m, x, y, yaw, sc in trunks:
        P.add(x, y, 0.8)
    xs, ys = P.candidates(1.1)
    G = surface(xs, ys, trunks)
    F = G["F"]
    edge = (G["g"] > 0.02) & (G["g"] < 0.55) & (F["z"] > 0.42) & (F["path"] > S.PATH_HALF + 0.9) & (F["gap"] < 0.3)
    outer = S.smooth((F["e"] - F["f"]) / 30.0)
    p = edge * (0.3 + 0.35 * outer) * S.smooth((S.noise(xs, ys, 6.0, 91) + 0.3) / 0.5)
    roll = P.rng.uniform(0, 1, xs.shape)
    rows = {"SM_Runner_A": [], "SM_Runner_B": []}
    for k in np.nonzero(roll < p)[0]:
        x, y = float(xs[k]), float(ys[k])
        if not P.free(x, y, 1.0):
            continue
        P.add(x, y, 1.0)
        # They run away from the grass, out over the sand: face down the slope of the grass's weight (roughly seaward).
        r, th = S.polar(x, y)
        seaward = math.degrees(th) if F["f"][k] < F["e"][k] else math.degrees(th) + 180.0
        mesh = "SM_Runner_A" if P.rng.random() < 0.6 else "SM_Runner_B"
        rows[mesh].append([round(x * 100, 1), round(y * 100, 1), 0.0, round(seaward + float(P.rng.uniform(-60, 60)), 1), 0.0, 0.0,
                           round(float(P.rng.uniform(0.8, 1.3)), 3)])
    out["plants"].update(rows)
    print("SB_HOME details: runners", {k: len(v) for k, v in rows.items()})
    # Things the sea and the trees leave on the ground (build_home_props.py), instanced: shells and seaweed along the
    # wrack line (the waves' reach, about 0.3-0.5 m up), pebbles on the outer beaches and at the path's sides, husks and
    # dead fronds under the palms, twigs under the trees, driftwood sticks, crab burrows on the upper beach; more of
    # them where the grass meets the sand.
    xs, ys = P.candidates(0.7)
    G = surface(xs, ys, trunks)
    F = G["F"]
    outer = S.smooth((F["e"] - F["f"]) / 30.0)
    sand = (G["g"] < 0.15) & (G["earth"] < 0.3) & (F["gap"] < 0.3)
    wrack = np.exp(-((F["z"] - 0.4) / 0.12) ** 2)
    edge = np.exp(-((G["g"] - 0.35) / 0.25) ** 2) * (F["z"] > 0.4)
    grove = S.smooth((S.noise(xs, ys, 9.0, 92) + 0.2) / 0.5)
    near_palm = np.zeros(xs.shape)
    near_tree = np.zeros(xs.shape)
    for m, px, py, yaw, sc in trunks:
        box = (np.abs(xs - px) < 4.0) & (np.abs(ys - py) < 4.0)
        if not box.any():
            continue
        d = np.hypot(xs[box] - px, ys[box] - py)
        if "Palm" in m:
            near_palm[box] = np.maximum(near_palm[box], S.smooth((3.0 - d) / 1.5) * (d > 0.5))
        else:
            near_tree[box] = np.maximum(near_tree[box], S.smooth((3.5 - d) / 1.5) * (d > 0.7))
    pathside = np.exp(-((F["path"] - S.PATH_HALF - 0.25) / 0.3) ** 2) * (F["inland"] > 0.2)
    kinds = {
        # mesh: (density, radius, sink, scale range, lay on the slope)
        "SM_Home_Shell_A": (sand * (0.012 + 0.32 * wrack * (0.4 + 0.6 * outer)) * (F["z"] > 0.05), 0.15, 0.005, (0.8, 1.3)),
        "SM_Home_Shell_B": (sand * (0.006 + 0.15 * wrack) * (F["z"] > 0.05), 0.15, 0.005, (0.8, 1.3)),
        "SM_Home_Shell_C": (sand * (0.005 + 0.12 * wrack) * (F["z"] > 0.05), 0.15, 0.01, (0.8, 1.4)),
        "SM_Home_ClumpSeaweed": (sand * 0.45 * wrack * (0.3 + 0.7 * outer) * grove, 0.5, 0.0, (0.8, 1.5)),
        "SM_Home_Pebble_A": ((sand * (0.25 * outer * (F["z"] < 0.9) * (F["z"] > -0.2) * grove) + 0.25 * pathside + 0.06 * edge), 0.3, 0.01, (0.8, 1.4)),
        "SM_Home_Pebble_B": ((sand * 0.1 * outer * (F["z"] < 0.9) * (F["z"] > 0.0) * grove) + 0.12 * pathside, 0.4, 0.02, (0.8, 1.5)),
        "SM_Home_DebrisHusk": (0.12 * near_palm + 0.03 * edge, 0.6, 0.03, (0.85, 1.2)),
        "SM_Home_DebrisFrond": (0.06 * near_palm + 0.04 * edge * (F["z"] > 0.5), 1.4, 0.0, (0.8, 1.25)),
        "SM_Home_DebrisTwigs": (0.08 * near_tree + 0.04 * edge + 0.03 * G["lit"], 0.6, 0.0, (0.8, 1.3)),
        "SM_Home_DebrisStick": (sand * 0.14 * wrack * (0.3 + 0.7 * outer) + 0.02 * edge, 1.2, 0.01, (0.8, 1.3)),
        "SM_Home_Burrow": (sand * 0.035 * (F["z"] > 0.55) * (F["z"] < 1.6) * (1.0 - wrack), 0.5, 0.01, (0.8, 1.2)),
    }
    roll = P.rng.uniform(0, 1, xs.shape)
    det = {}
    for mesh, (dens, radius, sink, sc_range) in kinds.items():
        pick = P.rng.uniform(0, 1, xs.shape) < np.clip(dens, 0.0, 0.9)
        rows_ = []
        for k in np.nonzero(pick & (roll > 0.0))[0]:
            x, y = float(xs[k]), float(ys[k])
            if not P.free(x, y, radius):
                continue
            P.add(x, y, radius)
            rows_.append([round(x * 100, 1), round(y * 100, 1), 0.0, round(float(P.rng.uniform(0, 360)), 1), 0.0, 0.0,
                          round(float(P.rng.uniform(*sc_range)), 3), sink])
        det[mesh] = rows_
    # The lawn (owner: "just create real visual grass small and a lot so it blends in"): square-metre patches of short blades
    # wherever the grass covers (overlapping, each turned and scaled its own way), thinner patches at the grass's edge, none on
    # the footpath, in the places or where the litter takes over under the trees. Its own placer and seed.
    PL = Placer(72)
    xs, ys = PL.candidates(0.72)                                # D78: denser (about 1.9 patches a square metre)
    G = surface(xs, ys, trunks)
    F = G["F"]
    g = G["g"]
    # The grass's own edge (and the sandy fingers in it) gets a fringe: sparser patches down to a thin cover.
    ok = ((g > 0.28) | (PL.rng.uniform(0, 1, xs.shape) < (g - 0.12) / 0.16)) & (g > 0.12)
    ok &= (F["path"] > S.PATH_HALF + 0.45) & (G["earth"] < 0.35)
    ok &= PL.rng.uniform(0, 1, xs.shape) > np.clip((G["lit"] - 0.3) / 0.4, 0, 1)
    for name, p in S.PLACES.items():
        if "clear" in p:
            hl, hr = S.place_local(xs, ys, name)
            l0, l1, r0, r1_ = p["clear"]
            ok &= ~((hl > l0 - 0.5) & (hl < l1 + 0.5) & (hr > r0 - 0.5) & (hr < r1_ + 0.5))
    lawn = {"SM_Grass_PatchA": [], "SM_Grass_PatchB": [], "SM_Grass_PatchEdge": []}
    pick = PL.rng.uniform(0, 1, xs.shape)
    # D86: the thin edge patches only at the grass's real edges; inside the meadow (grass all round) the full ones (the thin ones
    # showed the darker floor as squares there). The places are kept: only the mesh changes.
    inner = np.mean([surface(xs + 2.5 * np.cos(a), ys + 2.5 * np.sin(a), trunks)["g"]
                     for a in np.linspace(0.0, 2.0 * np.pi, 8, endpoint=False)], axis=0) > 0.62
    for k in np.nonzero(ok)[0]:
        thin = g[k] < 0.7 and not inner[k]
        mesh = "SM_Grass_PatchEdge" if thin else ("SM_Grass_PatchA" if pick[k] < 0.6 else "SM_Grass_PatchB")
        # Combed with the wind (towards about 20 degrees, from the game's 200), the direction wandering slowly over the island.
        yaw = 20.0 + 40.0 * S.vnoise(xs[k], ys[k], 9.0, 74) + PL.rng.normal(0.0, 22.0)
        # D86: a quarter wider than tall (the 0.72 m jittered grid of turned squares left 4.5 % of the meadow uncovered: square holes
        # where the darker floor showed); the blades' height as before. Rows: ..., scale across, sink, scale up.
        sc = float(PL.rng.uniform(0.85, 1.15))
        lawn[mesh].append([round(float(xs[k]) * 100, 1), round(float(ys[k]) * 100, 1), 0.0, round(float(yaw), 1), 0.0, 0.0,
                           round(sc * LAWN_WIDEN, 3), 0.0, round(sc, 3)])
    det.update(lawn)
    # D86: the bare spots in the meadow's middle (the sand fingers far from its edges read as mange) get the thin patches too; the
    # grass's own edges and the beaches keep D78's fringe. Its own placer and seed (every earlier place is kept).
    PB = Placer(75)
    xs, ys = PB.candidates(0.72)
    G = surface(xs, ys, trunks)
    F = G["F"]
    g = G["g"]
    around = np.mean([surface(xs + 2.5 * np.cos(a), ys + 2.5 * np.sin(a), trunks)["g"]
                      for a in np.linspace(0.0, 2.0 * np.pi, 8, endpoint=False)], axis=0)
    # (Seen in play: the holes the main pass leaves over worn earth (ruts, bare tussocks) and the thinned litter's edge show the
    # darkened floor as square brown holes; they are filled too, away from the footpath.)
    holes = (g <= 0.28) | (G["earth"] >= 0.35) | ((G["lit"] > 0.3) & (G["lit"] < 0.55))
    ok = holes & (around > 0.62) & (F["path"] > S.PATH_HALF + 0.45) & (G["lit"] < 0.55)
    for name, p in S.PLACES.items():
        if "clear" in p:
            hl, hr = S.place_local(xs, ys, name)
            l0, l1, r0, r1_ = p["clear"]
            ok &= ~((hl > l0 - 0.5) & (hl < l1 + 0.5) & (hr > r0 - 0.5) & (hr < r1_ + 0.5))
    fill = 0
    for k in np.nonzero(ok)[0]:
        yaw = 20.0 + 40.0 * S.vnoise(xs[k], ys[k], 9.0, 74) + PB.rng.normal(0.0, 22.0)
        sc = float(PB.rng.uniform(0.85, 1.15))
        det["SM_Grass_PatchA"].append([round(float(xs[k]) * 100, 1), round(float(ys[k]) * 100, 1), 0.0, round(float(yaw), 1), 0.0,
                                       0.0, round(sc * LAWN_WIDEN, 3), 0.0, round(sc, 3)])
        fill += 1
    # D86 (the meadow "has no flowers", D78's open point): small patches of daisies, buttercups and violets in the open meadow,
    # in groups with open stretches between (a grove noise), each kind keeping to its own drifts.
    PM = Placer(76)
    xs, ys = PM.candidates(1.3)
    G = surface(xs, ys, trunks)
    F = G["F"]
    g = G["g"]
    group = S.vnoise(xs, ys, 7.0, 86)
    kind = S.vnoise(xs, ys, 13.0, 87)
    ok = (g > 0.75) & (F["path"] > S.PATH_HALF + 0.8) & (G["earth"] < 0.2) & (G["lit"] < 0.15)
    ok &= PM.rng.uniform(0, 1, xs.shape) < 0.6 * np.clip((group - 0.2) / 0.35, 0.0, 1.0)
    for name, p in S.PLACES.items():
        if "clear" in p:
            hl, hr = S.place_local(xs, ys, name)
            l0, l1, r0, r1_ = p["clear"]
            ok &= ~((hl > l0 - 1.0) & (hl < l1 + 1.0) & (hr > r0 - 1.0) & (hr < r1_ + 1.0))
    flowers = {"SM_Meadow_Daisy": [], "SM_Meadow_Buttercup": [], "SM_Meadow_Violet": []}
    for k in np.nonzero(ok)[0]:
        x, y = float(xs[k]), float(ys[k])
        if not PM.free(x, y, 0.45):
            continue
        PM.add(x, y, 0.45)
        mesh = "SM_Meadow_Daisy" if kind[k] > -0.1 else ("SM_Meadow_Buttercup" if kind[k] > -0.55 else "SM_Meadow_Violet")
        flowers[mesh].append([round(x * 100, 1), round(y * 100, 1), 0.0, round(float(PM.rng.uniform(0, 360)), 1), 0.0, 0.0,
                              round(float(PM.rng.uniform(0.85, 1.2)), 3), 0.0])
    det.update(flowers)
    print("SB_HOME meadow: fill", fill, "flowers", {k: len(v) for k, v in flowers.items()})
    # D78 (owner: "real leaves for the braun part"): patches of fallen leaves over the litter (thick under the crowns, thinning
    # out at their edges, where they lie on the grass), a few on the footpath where it passes under trees. Its own placer.
    PF = Placer(73)
    xs, ys = PF.candidates(0.9)
    G = surface(xs, ys, trunks)
    lit = G["lit"]
    near = S.plant_relief(xs, ys, [t for t in trunks if "Tree" in t[0]])[1]
    on_path = (G["earth"] > 0.4) & (near > 0.3)
    leaves = {"SM_Litter_PatchA": [], "SM_Litter_PatchB": [], "SM_Litter_Sparse": []}
    pick = PF.rng.uniform(0, 1, xs.shape)
    for k in np.nonzero((lit > 0.15) | on_path)[0]:
        if on_path[k] or lit[k] < 0.35:
            if pick[k] > 0.75:
                continue
            mesh = "SM_Litter_Sparse"
        else:
            mesh = "SM_Litter_PatchA" if lit[k] > 0.6 else "SM_Litter_PatchB"
        leaves[mesh].append([round(float(xs[k]) * 100, 1), round(float(ys[k]) * 100, 1), 0.0, round(float(PF.rng.uniform(0, 360)), 1), 0.0,
                             0.0, round(float(PF.rng.uniform(0.85, 1.15)), 3), 0.0])
    det.update(leaves)
    out["details"] = det
    print("SB_HOME details:", {k: len(v) for k, v in det.items()})


def lay_details(out, trunks):
    """The details' heights and tilts on the visible ground (each lies on its slope)."""
    for mesh, rows in out.get("details", {}).items():
        if not rows:
            continue
        a = np.array(rows, dtype=float)
        x, y = a[:, 0] / 100.0, a[:, 1] / 100.0
        e = 0.15
        z = surface(x, y, trunks)["z"]
        gx = (surface(x + e, y, trunks)["z"] - surface(x - e, y, trunks)["z"]) / (2 * e)
        gy = (surface(x, y + e, trunks)["z"] - surface(x, y - e, trunks)["z"]) / (2 * e)
        yaw = np.radians(a[:, 3])
        sx = gx * np.cos(yaw) + gy * np.sin(yaw)
        sy = -gx * np.sin(yaw) + gy * np.cos(yaw)
        pitch, roll = np.degrees(np.arctan(sx)), -np.degrees(np.arctan(sy))
        out["details"][mesh] = [[r[0], r[1], round(float(zz - r[7]) * 100.0, 1), r[3], round(float(p), 1), round(float(q), 1), r[6]]
                                + r[8:9] for r, zz, p, q in zip(rows, z, pitch, roll)]


def rez_plants(out):
    """D71: the plants stand on the new ground (its relief, the trees' mounds); the places keep clear of them."""
    trees = [(m, r[0] / 100.0, r[1] / 100.0, r[3], r[6]) for m, rows in out["plants"].items() if ("Tree" in m or "Palm" in m)
             for r in rows]
    removed = {}
    for mesh, rows in out["plants"].items():
        a = np.array(rows, dtype=float)
        x, y = a[:, 0] / 100.0, a[:, 1] / 100.0
        G = surface(x, y, trees)
        sink = next(v for k, v in GROUND_SINK.items() if mesh.startswith(k))
        z = G["z"] - sink
        keep = np.ones(len(rows), dtype=bool)
        for name, p in S.PLACES.items():
            if "clear" not in p:
                continue
            l0, l1, r0, r1_ = p["clear"]
            hl, hr = S.place_local(x, y, name)
            keep &= ~((hl > l0) & (hl < l1) & (hr > r0) & (hr < r1_))
        removed[mesh] = int((~keep).sum())
        out["plants"][mesh] = [[r[0], r[1], round(float(zz) * 100.0, 1)] + list(r[3:]) for r, zz, k in zip(rows, z, keep) if k]
    for mesh, rows in out.get("details", {}).items():
        keep = []
        for r in rows:
            inside = False
            for name, p in S.PLACES.items():
                if "clear" in p:
                    hl, hr = S.place_local(r[0] / 100.0, r[1] / 100.0, name)
                    l0, l1, r0, r1_ = p["clear"]
                    inside = inside or (l0 < hl < l1 and r0 < hr < r1_)
            if not inside:
                keep.append(r)
        out["details"][mesh] = keep
    lay_details(out, trees)
    print("SB_HOME rez plants: removed for the places", {k: v for k, v in removed.items() if v})


def block_mask_cell(z, threshold, k):
    n = (z.shape[0] - 1) // k
    zmax = np.full((n, n), -1e9)
    for a in range(k + 1):
        for b in range(k + 1):
            zmax = np.maximum(zmax, z[a:a + n * k:k, b:b + n * k:k][:n, :n])
    return grow(zmax > threshold, 1)


# --- where things stand -----------------------------------------------------------------------------------------
LAWN_WIDEN = 1.25   # D86: the lawn's patches wider than tall (see details())


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
    # D71: three in ten broadleaf trees become the third kind (leaning, four limbs); every tree keeps its place.
    c_rows = []
    for mesh in ("SM_Tree_A", "SM_Tree_B"):
        kept = []
        for r in out["plants"][mesh]:
            (c_rows if int(abs(r[0]) * 7.0 + abs(r[1]) * 13.0) % 10 < 3 else kept).append(r)
        out["plants"][mesh] = kept
    out["plants"]["SM_Tree_C"] = c_rows
    details(out)
    rez_plants(out)
    os.makedirs(os.path.dirname(SCATTER_JSON), exist_ok=True)
    with open(SCATTER_JSON, "w", encoding="utf-8") as fh:
        json.dump(out, fh, separators=(",", ":"))
    counts = {k: len(v) for k, v in out["plants"].items()}
    print("SB_HOME scatter", counts, "rocks", len(out["rocks"]), "seagrass", {k: len(v) for k, v in out["seagrass"].items()},
          "trees", n_tree, "palms", n_palm, "big", n_big, "bush", n_bush, "flower", n_flower, "fern", n_fern, "grass", n_grass,
          round(time.time() - t0, 1), "s")


if __name__ == "__main__":
    args = (sb.script_args() if sb else sys.argv[1:]) or ["scatter", "ground", "col"]
    if "scatter" in args:
        scatter()
    if sb and "ground" in args:
        ground()
    if sb and ("sand" in args or "turf" in args):
        X, Y, F = fine_fields()
        if "sand" in args:
            sand(X, Y, F)
        if "turf" in args:
            turf(X, Y, F)
    if sb and "col" in args:
        collision()
    print("SB_HOME_OK", args)
