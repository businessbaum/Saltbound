"""The hub ship's layout (D61): one loader for every hub script (Blender and the editor's Python).

Every number comes from Docs/Design/HubShip/HubShip_Data.json (generated from hub_data.js by
`python Tools/design/render_hub_ship.py --data`); nothing here is typed in from the sheets. Pure Python (no numpy: the
editor's Python has none).

Ship frame, metres: x forward from the stern face (0) to the stem (160 at the main deck), y to starboard, z up from the
design waterline. Unreal: cm, +X forward, +Y starboard, +Z up, the ship's pivot at x 80, y 0, z 0 (`to_unreal`).
Blender: metres, the same pivot, Blender Y = -Unreal Y (`to_blender`; sb_blender.export_fbx turns the axes).
"""
import json
import math
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
DATA_PATH = os.path.join(ROOT, "Docs", "Design", "HubShip", "HubShip_Data.json")

with open(DATA_PATH, encoding="utf-8") as _fh:
    D = json.load(_fh)

HUB = D["hub"]
Z = HUB["Z"]
PIVOT_X = 80.0                      # frame.unreal: X_cm = (x - 80) * 100
SPACES = D["spaces"]
LEVELS = {lv["id"]: lv for lv in D["levels"]}
OPEN = D["open"]
LOCKED = D["locked"]
GATES = D["gates"]
GATE_MARKS = D["gate_marks"]


# --- frames --------------------------------------------------------------------------------------------------------
def to_unreal(x, y, z):
    """Ship metres -> the ship actor's local Unreal cm."""
    return ((x - PIVOT_X) * 100.0, y * 100.0, z * 100.0)


def to_blender(x, y, z):
    """Ship metres -> Blender metres (pivot at x 80; Blender Y = -Unreal Y)."""
    return (x - PIVOT_X, -y, z)


def face_to_yaw(face):
    """frame.face: degrees in plan, 0 = bow, 90 = starboard; equals Unreal yaw in the ship's frame."""
    return float(face)


def berth_world():
    """The berth (hub.berth): world centre in cm and the actor's yaw (bowDeg is a compass bearing: north +X = yaw 0,
    east +Y = yaw 90, so the yaw is the bearing, wrapped to -180..180)."""
    b = HUB["berth"]
    yaw = (b["bowDeg"] + 180.0) % 360.0 - 180.0
    return (b["centreM"][0] * 100.0, b["centreM"][1] * 100.0, 0.0), yaw, b["id"]


def ship_to_world(x, y, z, berth=None):
    """Ship metres -> world cm at the berth (for checks and labels)."""
    (bx, by, bz), yaw, _ = berth or berth_world()
    lx, ly, lz = to_unreal(x, y, z)
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return (bx + lx * c - ly * s, by + lx * s + ly * c, bz + lz)


# --- the hull --------------------------------------------------------------------------------------------------------
def _interp(pts, x):
    if x <= pts[0][0]:
        return pts[0][1]
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        if x <= x1:
            return y0 if x1 == x0 else y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return pts[-1][1]


_STBD = []
for _x, _y in HUB["outline_deck"]:
    if _y >= 0 and (not _STBD or _x > _STBD[-1][0]):
        _STBD.append((float(_x), float(_y)))


def hb(x):
    """Half-beam of the deck outline at x (outline_deck, the starboard side)."""
    if x < 0 or x > _STBD[-1][0]:
        return 0.0
    return _interp(_STBD, x)


SECTION = [(float(z), float(w)) for z, w in HUB["section"]]
SECTION_MID = _interp(SECTION, Z["main"])            # the section's half-width at the main deck amidships


def stem_x(z):
    """How far forward the stem reaches at height z (the profile's bow edge, from the forecastle rail to the keel)."""
    prof = HUB["profile"]
    i = next(k for k, p in enumerate(prof) if p[0] >= 161.0)       # (161.5, 26.7), the forecastle rail's fore end
    bow = []
    for px, pz in prof[i:]:
        bow.append((float(pz), float(px)))
        if pz <= HUB["keel"]:                                       # down to the keel's fore end, (132, -12)
            break
    return _interp(sorted(bow), z)


def half_width(x, z):
    """The hull's half-width at (x, z): the amidships section scaled by the deck outline. Towards the bow the outline
    is stretched so the stem at height z (stem_x) is where the deck outline's stem is."""
    xs = stem_x(z)
    deck_stem = _STBD[-1][0]
    if x > 100.0:
        x = 100.0 + (x - 100.0) * (deck_stem - 100.0) / max(xs - 100.0, 1.0)
    return hb(x) * _interp(SECTION, z) / 20.0


def profile_top(x):
    """The side profile's top edge at x: the sterncastle rail, the main deck rail, the forecastle rail."""
    prof = HUB["profile"]
    top = []
    for px, pz in prof:
        top.append((float(px), float(pz)))
        if px >= 161.0:
            break
    return _interp(top, x)


def profile_bottom(x):
    """The hull's lowest point at x: the keel, rising to the stern face and up the stem."""
    prof = HUB["profile"]
    i = next(k for k, p in enumerate(prof) if p[0] >= 161.0)
    bottom = sorted((float(px), float(pz)) for px, pz in prof[i:])
    return _interp(bottom, x)


# --- floors, rectangles ------------------------------------------------------------------------------------------------
def R(r):
    """A data rectangle as a tuple (x0, x1, y0, y1)."""
    return (float(r["x0"]), float(r["x1"]), float(r["y0"]), float(r["y1"]))


def level_z(level_id):
    return float(LEVELS[level_id]["z"])


def open_rects(level_id):
    """[(zone id, rect)] of the open zones on a level (the Chandlery stands on the deck: it is listed apart)."""
    out = []
    for o in OPEN:
        if o["level"] == level_id and not o.get("inside"):
            out += [(o["id"], R(r)) for r in o["rects"]]
    return out


def voids(level_id):
    return [R(r) for r in LEVELS[level_id].get("voids", [])]


def flights():
    """The Well's four flights: dicts with from, to, the rectangle and the way up (+x, -x, +y, -y)."""
    return HUB["flights"]


def space(name):
    return SPACES[name]


def prop(space_name, prop_id):
    return next(p for p in SPACES[space_name]["props"] if p["id"] == prop_id)


def inside(r, x, y):
    return r[0] <= x < r[1] and r[2] <= y < r[3]


# --- gates -----------------------------------------------------------------------------------------------------------------
# The blockout's gate boxes (build_hub_blockout.py draws them, build_hub_inside.py builds the art on them, ASaltHubGate blocks
# there): width and height (m) per gate type; doors 2.2 x 1.2 m (prompt section 3).
GATE_SIZE = {"G1": (1.2, 2.2), "G2": (2.4, 1.0), "G3": (0.9, 3.0), "G4": (1.6, 1.4), "G6": (3.0, 2.5)}
GATE_WALL = 0.3          # the blockout's wall thickness (centred on the zone edge)


def gate_axis(gm):
    """Which wall line the gate stands on: ('x', line) spans y, ('y', line) spans x, or None (on the deck)."""
    lines = []
    for zid, r in open_rects(gm["level"]):
        lines += [("x", r[0], r[2], r[3]), ("x", r[1], r[2], r[3]), ("y", r[2], r[0], r[1]), ("y", r[3], r[0], r[1])]
    if gm["level"] == "main":
        lines += [("x", HUB["fcX"], -20.0, 20.0)]
    best = None
    for ax, ln, a0, a1 in lines:
        along, across = (gm["y"], gm["x"]) if ax == "x" else (gm["x"], gm["y"])
        if a0 - 0.3 <= along <= a1 + 0.3 and abs(across - ln) < 1.0:
            if best is None or abs(across - ln) < best[2]:
                best = (ax, ln, abs(across - ln), a0, a1)
    return best


def gate_box(gm):
    """Where a gate mark blocks, as (kind, (x0, x1, y0, y1, z0, z1)) in ship metres. kind: 'seal' (G5, the Keelgate: no box),
    'cargo' (the main deck's cargo rope, the deck's whole width), 'plate' (shrouds whose ratlines start out of reach: a chained
    plate at the bulwark), 'hatch' (a padlocked hatch on the deck) or 'wall' (in a wall line or a rail)."""
    z = level_z(gm["level"])
    g, x, y = gm["g"], gm["x"], gm["y"]
    if g == "G5":
        return "seal", None
    if g == "G2" and gm["level"] == "main":
        w = half_width(x, z) - 0.1
        return "cargo", (x - 0.05, x + 0.05, -w, w, z, z + 1.0)
    found = gate_axis(gm)
    wdt, hgt = GATE_SIZE[g]
    z0 = z + (1.0 if g == "G4" else 0.0)
    if found is None and gm["level"] == "main" and abs(y) > 15.0:
        return "plate", (x - 0.75, x + 0.75, y - 0.1, y + 0.1, z, z + 2.5)
    if found is None:
        return "hatch", (x - 0.8, x + 0.8, y - 0.8, y + 0.8, z, z + 0.3)
    ax, ln, _, a0, a1 = found
    along = y if ax == "x" else x
    if a1 - a0 >= wdt:
        along = min(max(along, a0 + wdt / 2), a1 - wdt / 2)
    t = GATE_WALL / 2 + 0.08
    if ax == "x":
        return "wall", (ln - t, ln + t, along - wdt / 2, along + wdt / 2, z0, z0 + hgt)
    return "wall", (along - wdt / 2, along + wdt / 2, ln - t, ln + t, z0, z0 + hgt)
