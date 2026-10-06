"""The hub ship's placement in a level (D63, D64), shared by the art review map (create_hub_blockout_map.py art: at her berth
in a world of its own) and her streamed level L_Hub (build_hub_level.py: the ship's x 80 at the origin, bow to +X; the game
streams it in at a berth, ASaltHubShip).

Every number comes from the data (hub_layout.py reads Docs/Design/HubShip/HubShip_Data.json; hub_plan.py lays out the rooms
and gates from it) or from the art scripts' own frames (Art/Generated: hub_doors.json the side doors' hinges, rope eyes and
blocks; hub_lamps.json where the lanterns hang; hub_rig.json the pennant; build_hub_inside.py hoist_pivots). What goes in:
  the art meshes (SM_Hub_*, visual only), the blockout parts as hidden collision (one per floor, D64), the pennant, the side
  doors (ASaltHubDoors, one per station (D70): the leaves, their ropes and capstans), the lifting bridges' leaves, the gates
  (ASaltHubGate), the E points (ASaltHubPoint), the hoist (ASaltHubHoist), the quay ladders (ASaltHubLadder), the moorings
  (ASaltHubMooring: the docking stations and the fleet's berths), the lanterns' lights and the exposure volumes below decks.
"""
import importlib
import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Art", "Blender", "Scripts"))
import hub_layout as H  # noqa: E402  (pure Python: every number from HubShip_Data.json)
importlib.reload(H)                # the editor keeps modules: always read the current data
import hub_plan as PL  # noqa: E402  (pure Python: the rooms and the gates, D64)
importlib.reload(PL)

BLK = "/Game/Saltbound/Hub/Blockout/"
MI_DIR = "/Game/Saltbound/Materials/Instances/"
ART_DIR = "/Game/Saltbound/Hub/Art/"
GEN_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "Art", "Generated")
LEVELS_UP = sorted(H.LEVELS, key=H.level_z)
OUTSIDE_FLOORS = {"poop", "fcTop"}         # the castles' weather decks are the outside's (build_hub_ends.py)
# The outside (build_hub_hull/ends/doors/rig.py), the designed spaces (build_hub_inside.py), every floor (build_hub_rooms.py).
# D70: her outside and her inside are two levels (L_Hub, L_HubInside): the inside streams in only near her (ASaltHubShip).
ART_OUTSIDE = ["HullStern", "HullWaistS", "HullWaistP", "HullBow", "Stern", "Castles", "Command", "Head", "DoorFrame",
               "MastMizzen", "MastMain", "MastFore", "Spritsail"]
ART_INSIDE = (["Dock", "Slip", "Tally", "Well", "MainDeck", "Chandlery", "Gates"]
              + ["Lvl_" + lv for lv in LEVELS_UP if lv not in OUTSIDE_FLOORS])
ART_PARTS = ART_OUTSIDE + ART_INSIDE
BLK_OUTSIDE = ["ShellStern", "ShellWaistS", "ShellWaistP", "ShellBow", "Masts"]
BLK_INSIDE = ["Dock"] + ["Deck_" + lv for lv in LEVELS_UP] + ["Stairs", "Gates", "Props", "Markers"]
BLK_PARTS = BLK_OUTSIDE + BLK_INSIDE
# Under the art the blockout only collides; its gates are ASaltHubGate's blockers, its markers are for the plain review.
BLK_SKIP_UNDER_ART = {"Gates", "Markers"}
# D69: the hidden shell and decks still cast shadows (bCastHiddenShadow). They are watertight, the art's boards are not: the
# sun came in through the planking's seams and the hull's art (the hall's water and walls in sunlight, slits of light along the
# ceilings). The stairs, props and masts stay shadowless (the art's own shapes cast theirs).
BLK_HIDDEN_SHADOW = ("Shell", "Deck_", "Dock")
# Calm zones (prompt section 4; not in the data): round the ship within 120 m (full by 300 m), and her harbour. Measured in the
# Storm sea (D61, hub_checks.water): the prompt's 30 % and 10 % let the sea reach +1.56 m under the hull and +0.51 m in the
# basin; 20 % and 7 % keep it under +1.2 m and +0.4 m. D70: the harbour runs her whole length, so its 7 % reaches 75 m round
# her middle (her ends at 80 m; the boat's last approach to a door is in it too). (inner cm, outer cm, the sea's share left)
SHIP_ZONE = (12000.0, 30000.0, 0.2)
HALL_ZONE = (7500.0, 9000.0, 0.07)
# D69 (owner: "the light from the lanterns are not good. Create strong and warm lightsources"): 60 cd at 2100 K made thin, pale
# pools that the daylight in the doorways and the Well washed out. Now 240 cd at 1900 K (an oil flame), a little flicker
# (ASaltHubRopes), and soft shadows from a 4 cm flame.
LANTERN_CD = 240.0
LANTERN_K = 1900.0
# How far a lantern reaches (cm): with 16 m each of the Well's 14 lanterns lit the whole shaft, and their shadows cost 4 ms at
# the Keelgate in the packaged build (D63). 240 cd still gives 2 lux at 11 m, where the window fades it out.
LANTERN_RADIUS = 1100.0
# D69: inside the hull the sea's haze stays outside (ASaltInteriorZone; ship metres x0, x1, y0, y1, z0, z1): the fog starts beyond
# the ship's length and is half as dense. The hold up to the gun deck, the sterncastle's storeys under the quarterdeck and under
# the poop (the quarterdeck itself is open deck, D69), the forecastle's.
INTERIOR_ZONES = {"Hull": (-2.0, 162.0, -21.0, 21.0, -6.0, 16.3), "Sterncastle": (-2.0, 48.0, -20.0, 20.0, 16.3, 22.4),
                  "Poop": (-2.0, 24.0, -20.0, 20.0, 22.4, 28.4), "Forecastle": (134.0, 162.0, -20.0, 20.0, 16.3, 22.4)}
# D69: her water hall is sheltered (no sun or sky glow in its water's volume): a box round its water, fading over its last 1.2 m.
# D70: the harbour's water is the canal and the two basins: a box each, 2.5 m in under the quays round them (the faces' foot
# must be fully sheltered), the basins out to the hull's outer face.
SHELTER_EDGE = 120.0
SHELTER_UNDER = 2.5
INTERIOR_MIN_EV = 3.0      # how far the eye opens up below decks (the Day preset stops at EV 11, Dusk at 5)
INTERIOR_MAX_EV = 7.5      # and how far it may close there (the shaft's daylight and the doorways would black the rooms out)
# The enclosed spaces (ship metres x0, x1, y0, y1, z0, z1): each gets its own exposure range (D70: the harbour aft and forward
# of the Well, shut by its doors).
INTERIORS = {"HarbourAft": (9.0, 72.0, -20.0, 20.0, 1.5, 13.2), "HarbourFore": (96.0, 151.0, -20.0, 20.0, 1.5, 13.2),
             "Slip": (50.0, 68.0, 10.0, 20.0, 1.5, 4.5), "TallyLane": (50.0, 73.0, 6.0, 20.0, 4.5, 7.5),
             "Well": (72.0, 96.0, -12.0, 12.0, 1.5, 16.5),
             "Chandlery": (134.0, 143.0, -1.0, 12.0, 16.5, 22.4)}       # D69: in the forecastle's front, two storeys high
# The hoist (well.hoist, counterweight, gantry): the cage's floor centre at the quay stop is the actor (build_hub_inside.py
# hoist_pivots); the gantry stands 7.5 m over the main deck, its sheaves 0.25 m under its beam; the cage's yoke 3.2 m over
# its floor; the counterweight's eye 0.2 m over its top.
GANTRY_H = 7.5
SHEAVE_DROP = 0.25
YOKE_H = 3.2
WEIGHT_EYE = 0.2
# The E points: the box the player aims at, round the spot at chest height (cm).
POINT_EXTENT = (45.0, 45.0, 60.0)
POINT_Z = 1.1


class Placer:
    """Spawns into the open level in the ship's frame: `origin` (cm) is where her x 80 lies, `yaw` her heading."""

    def __init__(self, eas, origin=(0.0, 0.0, 0.0), yaw=0.0, folder="HubShip"):
        self.eas = eas
        self.berth = (tuple(origin), float(yaw), None)
        self.origin = tuple(origin)
        self.yaw = float(yaw)
        self.folder = folder

    def world(self, x, y, z):
        return H.ship_to_world(x, y, z, self.berth)

    def spawn(self, cls, loc, yaw=0.0, label=None, folder=None):
        """`loc` in world cm; `yaw` in the ship's frame (0 bow, 90 starboard)."""
        a = self.eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=self.yaw + yaw))
        if label:
            a.set_actor_label(label)
        a.set_folder_path(self.folder + ("/" + folder if folder else ""))
        return a

    def mesh(self, path, label, folder, loc=None, yaw=0.0, collide=True, visible=True, movable=False):
        m = unreal.load_asset(path)
        if m is None:
            return None
        a = self.spawn(unreal.StaticMeshActor, loc or self.origin, yaw, label, folder)
        c = a.static_mesh_component
        if movable:
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_static_mesh(m)
        if not collide:
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        if not visible:
            c.set_visibility(False, False)
        return a


def _generated(name):
    path = os.path.join(GEN_DIR, name)
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def calm_zones(local=True, berth=None):
    """Her calm zones: round her pivot, and over her harbour (D70: its whole length); centres in her frame (cm, the berth
    actor's) or in the world."""
    out = []
    for (x, y), (inner, outer, factor) in (((H.PIVOT_X, 0.0), SHIP_ZONE), ((H.PIVOT_X, 0.0), HALL_ZONE)):
        c = H.to_unreal(x, y, 0.0) if local else H.ship_to_world(x, y, 0.0, berth)
        z = unreal.SaltCalmZone()
        z.set_editor_property("center", unreal.Vector2D(c[0], c[1]))
        z.set_editor_property("inner_radius", inner)
        z.set_editor_property("outer_radius", outer)
        z.set_editor_property("min_factor", factor)
        out.append(z)
    return out


def shelter_rects():
    """D70: the harbour's water to shelter (ship metres x0, x1, y0, y1): the canal and each basin, SHELTER_UNDER in under the
    quays round them (the canal's ends stop short of the hull's narrowing ends); the basins out to the hull's outer face."""
    cn = H.HUB["dock"]["canal"]
    out = [(cn["x0"] - 2.0, cn["x1"] + 1.5, -cn["half"] - SHELTER_UNDER, cn["half"] + SHELTER_UNDER)]
    for x0, x1, _, _ in H.basins():
        hy = max(H.half_width(x, 0.0) for x in (x0, 0.5 * (x0 + x1), x1)) + 0.3
        out.append((x0 - SHELTER_UNDER, x1 + SHELTER_UNDER, -hy, hy))
    return out


def shelter_boxes(local=True, berth=None):
    """The harbour's water as SaltShelterBoxes, in her frame (the berth actor's, cm) or in the world."""
    out = []
    for x0, x1, y0, y1 in shelter_rects():
        xc, yc = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
        c = H.to_unreal(xc, yc, 0.0) if local else H.ship_to_world(xc, yc, 0.0, berth)
        b = unreal.SaltShelterBox()
        b.set_editor_property("center", unreal.Vector2D(c[0], c[1]))
        b.set_editor_property("half_extent", unreal.Vector2D((x1 - x0) * 50.0, (y1 - y0) * 50.0))
        b.set_editor_property("yaw", 0.0 if local else (berth or H.berth_world())[1])
        b.set_editor_property("edge_cm", SHELTER_EDGE)
        b.set_editor_property("amount", 1.0)
        out.append(b)
    return out


def shelter_box(local=True, berth=None):
    """The canal's box (the berth's single Shelter until the ocean takes several)."""
    return shelter_boxes(local, berth)[0]


HUB_LEVEL = "/Game/Saltbound/Maps/L_Hub"
HUB_INSIDE_LEVEL = "/Game/Saltbound/Maps/L_HubInside"


def berth_and_ship(eas, folder="HubShip"):
    """The game's side (D63): her berth in the Saltbound Roads (hub.berth, her calm zones) and ASaltHubShip, which streams
    L_Hub in there (hidden until the first landfall; sb.Hub.Show 1)."""
    (bx, by, bz), yaw, bid = H.berth_world()
    p = Placer(eas, (bx, by, bz), yaw, folder)
    b = p.spawn(unreal.SaltHubBerth, (bx, by, bz), 0.0, "HubBerth_" + bid)
    b.set_editor_property("berth_id", bid)
    b.set_editor_property("default", True)
    b.set_editor_property("calm_zones", calm_zones(local=True))
    b.set_editor_property("shelter", shelter_box(local=True))
    if "shelters" in [str(n) for n in dir(b)]:
        b.set_editor_property("shelters", shelter_boxes(local=True))
    s = p.spawn(unreal.SaltHubShip, (bx, by, bz), 0.0, "HubShip")
    s.set_editor_property("hub_level", unreal.load_asset(HUB_LEVEL))
    set_inside_level(s)
    return b, s


def set_inside_level(ship):
    """D70: her inside streams apart (when L_HubInside exists and the class knows it)."""
    if "inside_level" in [str(n) for n in dir(ship)] and unreal.EditorAssetLibrary.does_asset_exist(HUB_INSIDE_LEVEL):
        ship.set_editor_property("inside_level", unreal.load_asset(HUB_INSIDE_LEVEL))


# --- the ship --------------------------------------------------------------------------------------------------------------
def blockout(p, under_art, parts=None):
    """The blockout parts (all, or those of one level). Under the art: hidden, collision only, without the gates and the
    markers."""
    n = 0
    for part in parts or BLK_PARTS:
        if under_art and part in BLK_SKIP_UNDER_ART:
            continue
        a = p.mesh(BLK + "SM_HubBlk_" + part, "Hub_" + part, "Collision" if under_art else None, visible=not under_art)
        if a is None:
            continue
        n += 1
        a.set_editor_property("tags", ["HubBlk_" + part])                  # the hub tests tell props from ceilings by these
        if under_art and part.startswith(BLK_HIDDEN_SHADOW):
            a.static_mesh_component.set_editor_property("cast_hidden_shadow", True)
        if part == "Markers":
            a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            a.static_mesh_component.set_editor_property("cast_shadow", False)
    return n


def art(p, parts=None):
    """The art meshes (all, or those of one level): visual only (the blockout collides), with the pennant at the main truck on
    the outside (build_hub_rig.py writes where: it streams aft, she lies head to wind on her anchors)."""
    n = 0
    parts = parts or ART_PARTS
    for part in parts:
        if p.mesh(ART_DIR + "SM_Hub_" + part, "HubArt_" + part, "Art", collide=False) is not None:
            n += 1
    rig = _generated("hub_rig.json") if "MastMain" in parts else None
    if rig:
        px, py, pz = rig["pennant"]
        if p.mesh(ART_DIR + "SM_Hub_Pennant", "HubArt_Pennant", "Art", loc=p.world(px, py, pz), yaw=180.0, collide=False, movable=True):
            n += 1
    return n


def doors_origin(k=0):
    """Station k's ASaltHubDoors stands at its doors' middle on the waterline (ship metres)."""
    x0, x1 = H.door_stations()[k]
    return 0.5 * (x0 + x1), 0.0, 0.0


def doors(p):
    """The hidden side doors (hub.doors; build_hub_doors.py's hinges, rope eyes, blocks and swing in hub_doors.json): an
    ASaltHubDoors per station (D70: one at each basin) with its four leaves, their ropes up to the blocks and its two
    capstans on the basin's aft edge, in its own frame (cm)."""
    dj = _generated("hub_doors.json")
    if dj is None or not hasattr(unreal, "SaltHubDoors"):
        return []
    do = H.HUB["doors"]
    out = []
    for k in range(len(H.door_stations())):
        ox, oy, oz = doors_origin(k)

        def loc(q):
            return unreal.Vector((q[0] - ox) * 100.0, (q[1] - oy) * 100.0, (q[2] - oz) * 100.0)
        rows = [lf for lf in dj["leaves"] if lf.get("station", 0) == k]
        if not rows:
            continue
        a = p.spawn(unreal.SaltHubDoors, p.world(ox, oy, oz), 0.0, "HubDoors" if k == 0 else "HubDoors%d" % (k + 1), "Art")
        leaves = []
        for lf in rows:
            leaf = unreal.SaltHubDoorLeaf()
            leaf.set_editor_property("mesh", lf["mesh"].replace("SM_Hub_", ""))
            leaf.set_editor_property("pivot", loc(lf["pivot"]))
            leaf.set_editor_property("open_yaw", float(lf["open_yaw"]))
            leaf.set_editor_property("eye", loc(lf["eye_shut"]))
            leaf.set_editor_property("block", loc(lf["block"]))
            leaf.set_editor_property("follows", lf["leaf"] == "fore")   # the aft leaf leads (the seam is bevelled)
            leaves.append(leaf)
        st = (dj.get("stations") or [dj])[min(k, len(dj.get("stations") or [dj]) - 1)]
        a.set_editor_property("leaves", leaves)
        a.set_editor_property("lag_degrees", float(st.get("lag_deg") or dj["lag_deg"]))
        a.set_editor_property("travel_seconds", float(do["seconds"]))
        a.set_editor_property("capstan_spots", [loc(lf["capstan"]) for lf in rows if lf["leaf"] == "aft"])
        a.set_editor_property("watch_open_cm", float(do["open_m"]) * 100.0)
        a.set_editor_property("hull_half_cm", H.half_width(ox, 0.0) * 100.0)
        # D70: its station, the docking station its passages moor at and the boat's pose there (local cm).
        dock = next((d for d in H.HUB["docking"] if d["basin"] == k), None)
        if dock is not None and "station" in [str(n) for n in dir(a)]:
            x, y, hd = H.docking_pose(dock)
            a.set_editor_property("station", k)
            a.set_editor_property("mooring_id", MOORING_IDS.get(dock["id"], "HubDock" + dock["id"].title()))
            a.set_editor_property("dock_pose", unreal.Transform(location=unreal.Vector((x - ox) * 100.0, (y - oy) * 100.0, 0.0),
                                                                rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=hd)))
        out.append(a)
    return out


def doors_clear_x(k=0):
    """The clear way between station k's open leaves along x (ship metres): each leaf mesh's box (pivoted on its hinge) turned
    open."""
    dj = _generated("hub_doors.json")
    lo, hi = -1.0e9, 1.0e9
    for lf in (dj or {}).get("leaves", []):
        if lf.get("station", 0) != k:
            continue
        m = unreal.load_asset(ART_DIR + lf["mesh"])
        if m is None:
            continue
        box = m.get_bounding_box()
        a = math.radians(lf["open_yaw"])
        xs = [(x * math.cos(a) - y * math.sin(a)) / 100.0 for x in (box.min.x, box.max.x) for y in (box.min.y, box.max.y)]
        if lf["leaf"] == "aft":
            lo = max(lo, lf["pivot"][0] + max(xs))
        else:
            hi = min(hi, lf["pivot"][0] + min(xs))
    return lo, hi


def bridges(p):
    """D70: the lifting bridges (hub.bridges.lift): an ASaltHubBridge at each, its two leaves (SM_Hub_BridgeLeaf, pivoted on their
    hinges at the quay's deck, their own collision) lowered; the passages raise them."""
    n = 0
    zq = H.Z["quay"]
    lb = H.HUB["bridges"]["lift"]
    if hasattr(unreal, "SaltHubBridge"):
        for (k, yb), (x0, x1, y0, y1) in zip(lb["at"], H.lift_bridges()):
            xc, yc = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
            a = p.spawn(unreal.SaltHubBridge, p.world(xc, yc, zq), 0.0, "HubBridge_%d_%s" % (k, "S" if yb > 0 else "P"), "Bridges")
            a.set_editor_property("station", k)
            a.set_editor_property("side", 1 if yb > 0 else -1)
            a.set_editor_property("half_span_cm", (x1 - x0) * 50.0)
            a.set_editor_property("open_degrees", float(lb["open"]))
            n += 1
        return n
    for k, (x0, x1, y0, y1) in enumerate(H.lift_bridges()):
        yc = 0.5 * (y0 + y1)
        for end, (xh, yaw) in enumerate(((x0, 0.0), (x1, 180.0))):
            if p.mesh(ART_DIR + "SM_Hub_BridgeLeaf", "HubBridge_%d_%s" % (k, "AF"[end]), "Bridges", loc=p.world(xh, yc, zq), yaw=yaw,
                      collide=True, movable=True) is not None:
                n += 1
    return n


# --- light ---------------------------------------------------------------------------------------------------------------
def lantern_spots(parts=None):
    """Where the art hangs its lit lanterns (build_hub_inside.py writes hub_lamps.json: each lantern's glass, ship metres), of
    all the art or of some parts."""
    lamps = []
    for part, pts in sorted((_generated("hub_lamps.json") or {}).items()):
        if parts is None or part.replace("SM_Hub_", "") in parts:
            lamps += [tuple(q) for q in pts]
    return lamps


def lanterns(p, parts=None):
    """Below decks the lanterns burn by day as well (the rig puts ASaltLantern out by day): plain warm lights at the lanterns'
    spots (D61 review: with the game's outdoor exposure and the lanterns out, the Tally House measured black by day)."""
    lamps = lantern_spots(parts)
    for k, (x, y, z) in enumerate(lamps):
        lamp = p.spawn(unreal.PointLight, p.world(x, y, z), 0.0, "Lantern_%02d" % k, "Lanterns")
        lamp.set_editor_property("tags", ["HubLantern"])                     # ASaltHubRopes flickers them
        lc = lamp.get_component_by_class(unreal.PointLightComponent)
        lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        lc.set_editor_property("intensity", LANTERN_CD)
        lc.set_editor_property("use_temperature", True)
        lc.set_editor_property("temperature", LANTERN_K)
        lc.set_editor_property("attenuation_radius", LANTERN_RADIUS)
        lc.set_editor_property("source_radius", 4.0)
        lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    return len(lamps)


def interiors(p):
    """D69: the enclosed spaces' fog (ASaltInteriorZone): while the camera is inside, the sea's haze stays outside."""
    n = 0
    for name, (x0, x1, y0, y1, z0, z1) in INTERIOR_ZONES.items():
        a = p.spawn(unreal.SaltInteriorZone, p.world(0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1)), 0.0, "Interior_" + name, "Exposure")
        a.set_editor_property("extent", unreal.Vector((x1 - x0) * 50.0, (y1 - y0) * 50.0, (z1 - z0) * 50.0))
        a.set_editor_property("fog_start_cm", 16500.0)
        a.set_editor_property("fog_density_scale", 0.5)
        a.set_editor_property("blend_cm", 250.0)
        n += 1
    return n


def ropes(p, spans_too=True):
    """D69 (owner: "loose ends and no physics in the ropes"): the running rigging that hangs (build_hub_rig.py writes the spans
    to hub_ropes.json, ship metres): real cables in ASaltHubRopes at her pivot, blown aft (she lies head to wind). It also
    flickers the lanterns of its own level: D70's inside gets one without spans for its lanterns."""
    data = _generated("hub_ropes.json")
    if not data or not hasattr(unreal, "SaltHubRopes"):
        return 0
    a = p.spawn(unreal.SaltHubRopes, p.world(H.PIVOT_X, 0.0, 0.0), 0.0, "HubRopes" if spans_too else "HubLanternFlicker", "Art")
    spans = []
    for r in (data["spans"] + data.get("hull_spans", [])) if spans_too else []:
        sp = unreal.SaltRopeSpan()
        sp.set_editor_property("start", unreal.Vector(*H.to_unreal(*r["a"])))
        sp.set_editor_property("end", unreal.Vector(*H.to_unreal(*r["b"])))
        sp.set_editor_property("slack", float(r["slack"]))
        sp.set_editor_property("width", float(r["width"]) * 100.0)
        sp.set_editor_property("material", 1 if r.get("tar") else 0)
        sp.set_editor_property("wind", float(r.get("wind", 1.0)))
        spans.append(sp)
    a.set_editor_property("spans", spans)
    a.set_editor_property("materials", [unreal.load_asset(MI_DIR + "MI_Rope_Hemp"), unreal.load_asset(MI_DIR + "MI_Hub_RopeTar")])
    return len(spans)


def exposure(p):
    """The eye adapts below decks: each enclosed space gets its own exposure range (the rig's presets clamp it for the open
    sea), and it does not close again for the bright doorways and the Well's daylight (D62: the lane read black)."""
    n = 0
    for name, (x0, x1, y0, y1, z0, z1) in INTERIORS.items():
        v = p.spawn(unreal.PostProcessVolume, p.world(0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1)), 0.0, "Exposure_" + name, "Exposure")
        v.set_actor_scale3d(unreal.Vector((x1 - x0) * 0.5, (y1 - y0) * 0.5, (z1 - z0) * 0.5))      # the brush is 2 m (200 cm) across
        v.set_editor_property("unbound", False)
        v.set_editor_property("priority", 1.0)
        v.set_editor_property("blend_radius", 300.0)
        ps = v.get_editor_property("settings")
        ps.set_editor_property("override_auto_exposure_min_brightness", True)
        ps.set_editor_property("auto_exposure_min_brightness", INTERIOR_MIN_EV)
        ps.set_editor_property("override_auto_exposure_max_brightness", True)
        ps.set_editor_property("auto_exposure_max_brightness", INTERIOR_MAX_EV)
        # D69: screen traces back on inside (the rig turns them off for the open sea's waves, D23): the hall's flat water
        # mirrors the walls and doors it sees on screen, where ray tracing alone missed thin boards and showed the sky.
        ps.set_editor_property("override_lumen_reflections_screen_traces", True)
        ps.set_editor_property("lumen_reflections_screen_traces", True)
        v.set_editor_property("settings", ps)
        n += 1
    return n


# --- what the player uses ----------------------------------------------------------------------------------------------------
def gates(p):
    """A gate at every gate but the Keelgate (hub_plan.all_gate_marks: the corridors' mouths and the data's own marks; G5 blocks
    nothing, the main quest opens it): its blocker where the blockout's gate box stands (hub_layout.gate_box), E with the gate
    type's prompt and line (or the mark's own line)."""
    n = 0
    for k, gm in enumerate(PL.all_gate_marks()):
        kind, box = H.gate_box(gm)
        if kind == "seal":
            continue
        x0, x1, y0, y1, z0, z1 = box
        g = p.spawn(unreal.SaltHubGate, p.world(0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1)), 0.0,
                    "Gate_%02d_%s_%s" % (k, gm["g"], gm["zone"]), "Gates")
        gt = H.GATES[gm["g"]]
        g.set_editor_property("zone_id", gm["zone"])
        g.set_editor_property("gate_type", gm["g"])
        g.set_editor_property("prompt", gt["prompt"])
        g.set_editor_property("line", gm.get("line", gt["line"]))
        g.set_editor_property("block_extent", unreal.Vector((x1 - x0) * 50.0, (y1 - y0) * 50.0, (z1 - z0) * 50.0))
        ax, ay, az, reachable = gate_approach(gm, kind, box)
        g.set_editor_property("approach", unreal.Vector((ax - 0.5 * (x0 + x1)) * 100.0, (ay - 0.5 * (y0 + y1)) * 100.0, (az - 0.5 * (z0 + z1)) * 100.0))
        g.set_editor_property("try_walk", kind != "hatch" and reachable)
        n += 1
    return n


GATE_APPROACH = 1.2        # where a player meets a gate: this far (m) from its blocker's face, on the open floor beside it


def gate_approach(gm, kind, box):
    """The spot on the open floor beside a gate (ship metres, the floor's height): across its blocker from the locked side;
    a plate at the bulwark is met from inboard. Whether that spot is open floor (the room layout's label there): the
    forecastle's ladders (L14) and the cargo hatch stand behind the cargo rope (L15), nobody reaches them yet."""
    x0, x1, y0, y1, _, _ = box
    cx, cy = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
    pl = PL.plan(gm["level"])
    if kind == "plate":
        sides = [(0.0, -1.0 if cy > 0 else 1.0)]
    elif x1 - x0 < y1 - y0:
        sides = [(1.0, 0.0), (-1.0, 0.0)]
    else:
        sides = [(0.0, 1.0), (0.0, -1.0)]
    for nx, ny in sides:
        d = (abs(nx) * (x1 - x0) + abs(ny) * (y1 - y0)) * 0.5 + GATE_APPROACH
        px, py = cx + nx * d, cy + ny * d
        lab = pl.lab.get(PL.cell_of(px, py))
        if lab and lab[0] == "open":
            return px, py, H.level_z(gm["level"]), True
    nx, ny = sides[-1]
    d = (abs(nx) * (x1 - x0) + abs(ny) * (y1 - y0)) * 0.5 + GATE_APPROACH
    return cx + nx * d, cy + ny * d, H.level_z(gm["level"]), False


POINT_FLOORS = {"slip": "quay", "tally": "lower", "chandlery": "main"}
POINT_SKIP = set()


def points(p):
    """An interaction point at every E of the spaces (their prompts; for now they log what they would do)."""
    n = 0
    for space, level in POINT_FLOORS.items():
        for e in H.space(space).get("e", []):
            if (space, e["id"]) in POINT_SKIP:
                continue
            a = p.spawn(unreal.SaltHubPoint, p.world(e["x"], e["y"], H.Z[level] + POINT_Z), 0.0, "E_%s_%s" % (space, e["id"]), "Points")
            a.set_editor_property("space", space)
            a.set_editor_property("point_id", e["id"])
            a.set_editor_property("prompt", e["prompt"])
            a.set_editor_property("use_extent", unreal.Vector(*POINT_EXTENT))
            n += 1
    return n


def hoist_frame():
    """The hoist's numbers in the actor's frame (cm): its pivot (ship metres), its stops, the sheaves, the counterweight's
    low spot, the rail gaps at its stops."""
    hs = H.R(H.HUB["hoist"])
    cw = H.prop("well", "counterweight")
    sh = H.R(H.HUB["shaft"])
    zq = H.Z["quay"]
    hx, hy = 0.5 * (hs[0] + hs[1]), 0.5 * (hs[2] + hs[3])
    cwx, cwy = 0.5 * (cw["x0"] + cw["x1"]), 0.5 * (cw["y0"] + cw["y1"])
    stops = H.prop("well", "hoist")["stops"]
    sheave_z = (H.Z["main"] + GANTRY_H - SHEAVE_DROP - zq) * 100.0
    return {
        "pivot": (hx, hy, zq),
        "stops": [(H.Z[s] - zq) * 100.0 for s in stops],
        "cage_sheave": (0.0, 0.0, sheave_z),
        "weight_sheave": ((cwx - hx) * 100.0, (cwy - hy) * 100.0, sheave_z),
        "weight_low": ((cwx - hx) * 100.0, (cwy - hy) * 100.0, 0.0),
        "yoke": YOKE_H * 100.0,
        "weight_h": (cw["h"] + WEIGHT_EYE) * 100.0,
        "gap_centre": ((sh[0] - hx) * 100.0, 0.0, 50.0),             # the cage's aft side (its door) opens on the galleries
        "gap_extent": (6.0, (hs[3] - hs[2]) * 50.0, 50.0),
    }


def hoist(p):
    f = hoist_frame()
    a = p.spawn(unreal.SaltHubHoist, p.world(*f["pivot"]), 0.0, "Hoist", "Hoist")
    a.set_editor_property("stop_heights", f["stops"])
    a.set_editor_property("cage_sheave", unreal.Vector(*f["cage_sheave"]))
    a.set_editor_property("weight_sheave", unreal.Vector(*f["weight_sheave"]))
    a.set_editor_property("weight_low", unreal.Vector(*f["weight_low"]))
    a.set_editor_property("yoke_height", f["yoke"])
    a.set_editor_property("weight_height", f["weight_h"])
    a.set_editor_property("gap_centre", unreal.Vector(*f["gap_centre"]))
    a.set_editor_property("gap_extent", unreal.Vector(*f["gap_extent"]))
    return a


def ladders(p):
    """The rung ladders up the quays from the water (harbour.rung_ladders [x, y on the quay's face, yaw from the water to the
    quay], the D54 ladder): the actor on the edge, its +Y out over the water."""
    n = 0
    for lx, ly, yaw in H.prop("harbour", "rung_ladders")["at"]:
        p.spawn(unreal.SaltHubLadder, p.world(lx, ly, H.Z["quay"]), yaw + 90.0, "Ladder_%g_%g" % (lx, ly), "Ladders")
        n += 1
    return n


MOORING_IDS = {"aft": "HubFitting", "fore": "HubDockFore"}     # the aft station is the Slip's fitting berth (D61's id kept)


def berth_spots():
    """The boat's berths in the harbour (D70: hub.docking, harbour.fleet_berths along the canal): (id, x, y, heading, reach) in
    ship metres and degrees; reach is how far the quay's edge lies to her starboard (- to port)."""
    out = []
    for d in H.HUB["docking"]:
        x, y, hd = H.docking_pose(d)
        out.append((MOORING_IDS.get(d["id"], "HubDock" + d["id"].title()), x, y, hd, d["side"] * float(d.get("reach", 2.1))))
    fl = H.prop("harbour", "fleet_berths")
    for k, b in enumerate(fl["berths"]):
        x, y, hd = H.docking_pose(dict(b, heading=fl["heading"]))
        out.append(("HubFleet%d" % (k + 1), x, y, hd, b["side"] * float(b.get("reach", 2.1))))
    return out


def moorings(p):
    """Where the player's boat moors to her: the boat's pose on the berth's line, her bow the berth's heading; the quay's edge
    its Reach away (+ starboard); the quay deck its PierHeight over the water."""
    spots = berth_spots()
    for mid, x, y, heading, reach in spots:
        a = p.spawn(unreal.SaltHubMooring, p.world(x, y, 0.0), heading, "Mooring_" + mid, "Moorings")
        pt = a.get_component_by_class(unreal.SaltMooringPointComponent)
        pt.set_editor_property("mooring_id", mid)
        pt.set_editor_property("reach", reach * 100.0)
        pt.set_editor_property("pier_height", H.Z["quay"] * 100.0)
    return len(spots)


# --- what the tests measure against ------------------------------------------------------------------------------------------
def _area(aid, r, floor):
    a = unreal.SaltHubArea()
    a.set_editor_property("id", aid)
    a.set_editor_property("min", unreal.Vector2D((r[0] - H.PIVOT_X) * 100.0, r[2] * 100.0))
    a.set_editor_property("max", unreal.Vector2D((r[1] - H.PIVOT_X) * 100.0, r[3] * 100.0))
    a.set_editor_property("floor", floor * 100.0)
    return a


def _v(x, y, z):
    return unreal.Vector(*H.to_unreal(x, y, z))


ROUTE_SECONDS = 40.0       # prompt section 4: walking from the fitting berth to each service, under 40 s
BOAT_IN_OUT = 40.0         # the side-entry run starts this far (m) out from the hull, abreast of the doors


def test_data(p):
    """ASaltHubTestData at her pivot: the open floors (the open zones' rectangles), the walks from the fitting berth to each
    service (hub_checks.routes), the sea's sample points under the hull and in the harbour, 40 points under the keel, the aft
    station's doors' clear way and the boat's run through them. (D70: no slipway.)"""
    import hub_checks
    importlib.reload(hub_checks)
    d = p.spawn(unreal.SaltHubTestData, p.world(H.PIVOT_X, 0.0, 0.0), 0.0, "HubTestData", "Tests")
    # The open floors as the room layout has them (inside the hull, round the stair holes and voids), each zone's own level.
    lv_of = {o["id"]: o["level"] for o in H.OPEN}
    d.set_editor_property("open_floors", [_area(zid, r, H.Z[lv]) for lv in sorted(set(lv_of.values())) for zid, r in PL.open_floor_rects(lv)])
    d.set_editor_property("low_floor_exempt", [])
    routes = []
    for name, pts in hub_checks.routes().items():
        r = unreal.SaltHubRoute()
        r.set_editor_property("name", name)
        r.set_editor_property("points", [_v(x, y, z) for x, y, z in pts])
        r.set_editor_property("target_seconds", ROUTE_SECONDS)
        routes.append(r)
    d.set_editor_property("routes", routes)
    d.set_editor_property("hull_water", [_v(x, fy * H.half_width(x, 0.0), 0.0) for x in range(12, 152, 14) for fy in (-0.6, 0.0, 0.6)])
    pts = [(x, y) for x0, x1, _, _ in H.basins() for x in (x0 + 1.5, 0.5 * (x0 + x1), x1 - 1.5) for y in (-12.0, -6.0, 0.0, 6.0, 12.0)]
    pts += [(x, 0.0) for x in range(16, 148, 12)]                          # D70: along the canal
    d.set_editor_property("basin_water", [_v(x, y, 0.0) for x, y in pts])
    keel = H.HUB["keel"]
    xs = [6.0 + k * (150.0 - 6.0) / 19.0 for k in range(20)]
    d.set_editor_property("keel_points", [_v(x, s * 0.5 * H.half_width(x, keel), 0.0) for x in xs for s in (-1.0, 1.0)])
    d.set_editor_property("keel_z", keel * 100.0)
    ox, _, _ = doors_origin()
    lo, hi = doors_clear_x()
    half = H.half_width(ox, 0.0)
    d.set_editor_property("doors_x", unreal.Vector2D((lo - H.PIVOT_X) * 100.0, (hi - H.PIVOT_X) * 100.0))
    d.set_editor_property("doors_hull_half", half * 100.0)
    d.set_editor_property("boat_in_start", _v(0.5 * (lo + hi), half + BOAT_IN_OUT, 0.0))      # off the starboard pair, heading in
    d.set_editor_property("hall_middle", _v(0.5 * (lo + hi), 0.0, 0.0))
    unreal.log("SB_HUB_DOORS_CLEAR x %.2f..%.2f (%.2f m) hull half %.2f" % (lo, hi, hi - lo, half))
    return d


def everything(p, under_art=True):
    """The whole ship in one level: the art review map (and the blockout map without the art)."""
    counts = {"blockout": blockout(p, under_art)}
    if under_art:
        counts.update({"art": art(p), "doors": len(doors(p)), "bridges": bridges(p), "gates": gates(p), "points": points(p),
                       "hoist": 1 if hoist(p) else 0, "ladders": ladders(p), "moorings": moorings(p),
                       "test_data": 1 if test_data(p) else 0})
    counts["lanterns"] = lanterns(p)
    counts["exposure"] = exposure(p)
    counts["interiors"] = interiors(p)
    if under_art:
        counts["ropes"] = ropes(p)
    return counts


def outside(p):
    """D70: her outside (L_Hub, always there while she is shown): the hull, the castles, the rig and its ropes, the doors, the
    shell's and masts' collision, the lanterns on her outside."""
    return {"blockout": blockout(p, True, BLK_OUTSIDE), "art": art(p, ART_OUTSIDE), "doors": len(doors(p)), "ropes": ropes(p),
            "lanterns": lanterns(p, ART_OUTSIDE)}


def inside(p):
    """D70: her inside (L_HubInside, streamed in near her): every floor and room, the harbour, the bridges, the gates, the E
    points, the hoist, the ladders, the moorings, the test data, the lanterns below decks and what flickers them, the exposure
    and the interiors' fog."""
    return {"blockout": blockout(p, True, BLK_INSIDE), "art": art(p, ART_INSIDE), "bridges": bridges(p), "gates": gates(p),
            "points": points(p), "hoist": 1 if hoist(p) else 0, "ladders": ladders(p), "moorings": moorings(p),
            "test_data": 1 if test_data(p) else 0, "lanterns": lanterns(p, ART_INSIDE), "flicker": ropes(p, spans_too=False),
            "exposure": exposure(p), "interiors": interiors(p)}
