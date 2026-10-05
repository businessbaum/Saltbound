"""The hub ship's review map (D61; phase 1 of Docs/Design/HubShip/HubShip_Agent_Prompt.md): /Game/Saltbound/Maps/L_HubBlockout.

Script-owned, recreated on each run, never L_VisualTest (the owner plays that). The ocean, sky and lighting rig as in the
visual test map; the blockout (SM_HubBlk_*, build_hub_blockout.py) at her berth `SaltboundRoads` (hub.berth); the calm
zones round her and in her basin; the player's boat moored at the fitting berth (its start position is the mooring, its
lines and gangway reach the starboard quay); the player on the starboard quay; text labels at the NPC spots and the gates;
lanterns where the data hangs them (and on the Well's gallery corners and in the shaft, its note).

    python Tools/ue_exec.py Tools/ue_scripts/create_hub_blockout_map.py
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Art", "Blender", "Scripts"))
import hub_layout as H  # noqa: E402  (pure Python: every number from HubShip_Data.json)
import importlib  # noqa: E402
importlib.reload(H)                # the editor keeps modules: always read the current data

MAP = "/Game/Saltbound/Maps/L_HubBlockout"
BLK = "/Game/Saltbound/Hub/Blockout/"
PARTS = ["ShellStern", "ShellWaistS", "ShellWaistP", "ShellBow", "Basin", "Quay", "Lower", "Middle", "Upper", "Main", "Stairs",
         "Gates", "Props", "Masts", "Markers"]
# Calm zones (prompt section 4; not in the data): round the ship within 120 m (full by 300 m), in the basin. Measured in the
# Storm sea (D61, hub_checks.water): the prompt's 30 % and 10 % let the sea reach +1.56 m under the hull and +0.51 m in
# the basin; 20 % and 7 % keep it under +1.2 m and +0.4 m.
SHIP_ZONE = (12000.0, 30000.0, 0.2)
LANTERN_CD = 60.0          # a lantern below decks (the game's ASaltLantern is 28 cd)
INTERIOR_MIN_EV = 3.0      # how far the eye opens up below decks (the Day preset stops at EV 11, Dusk at 5)
BASIN_ZONE = (2500.0, 4000.0, 0.07)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if sb_ue.eal.does_asset_exist(MAP):
    les.load_level(MAP)
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, unreal.WorldSettings) and actor.get_class() != unreal.Brush.static_class():
            eas.destroy_actor(actor)
else:
    les.new_level(MAP)

(BX, BY, BZ), YAW, BERTH_ID = H.berth_world()


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None, folder=None):
    """rot = (pitch, yaw, roll) in degrees."""
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def world(x, y, z):
    return H.ship_to_world(x, y, z)


# --- sky, lighting, the sea ----------------------------------------------------------------------------------------------
sun = spawn(unreal.DirectionalLight, (0, 0, 2000), (-38, -60, 0), "Sun")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_editor_property("atmosphere_sun_light", True)
sc.set_editor_property("intensity", 90000.0)
spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 800), label="SkyLight")
skc = sky.get_component_by_class(unreal.SkyLightComponent)
skc.set_mobility(unreal.ComponentMobility.MOVABLE)
skc.set_editor_property("real_time_capture", True)
spawn(unreal.ExponentialHeightFog, (0, 0, -200), label="HeightFog")
pp = spawn(unreal.PostProcessVolume, label="GlobalPostProcess")
pp.set_editor_property("unbound", True)
rig = spawn(unreal.SaltLightingRig, (0, 0, 300), label="LightingRig")

ocean = spawn(unreal.SaltOcean, (0, 0, 0), label="Ocean")
zones = []
for centre, (inner, outer, factor) in ((world(H.PIVOT_X, 0.0, 0.0), SHIP_ZONE),
                                       (world(0.5 * (H.HUB["basin"]["x0"] + H.HUB["basin"]["x1"]), 0.0, 0.0), BASIN_ZONE)):
    z = unreal.SaltCalmZone()
    z.set_editor_property("center", unreal.Vector2D(centre[0], centre[1]))
    z.set_editor_property("inner_radius", inner)
    z.set_editor_property("outer_radius", outer)
    z.set_editor_property("min_factor", factor)
    zones.append(z)
ocean.set_editor_property("calm_zones", zones)
print("SB_HUB_ZONES ship", [round(c) for c in world(H.PIVOT_X, 0, 0)[:2]], "basin", [round(c) for c in world(22.0, 0, 0)[:2]])
# The far floor, 60 m down under her (the visual test map's floor; it is not under her whole hull there).
floor = spawn(unreal.StaticMeshActor, (BX, BY, -6000), label="Seabed")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
floor.set_actor_scale3d(unreal.Vector(1200, 1200, 1))
floor.static_mesh_component.set_material(0, unreal.load_asset("/Game/Saltbound/Materials/Instances/MI_Sand"))
floor.static_mesh_component.set_editor_property("visible_in_ray_tracing", False)

# --- the ship ----------------------------------------------------------------------------------------------------------------
for part in PARTS:
    a = spawn(unreal.StaticMeshActor, (BX, BY, BZ), (0, YAW, 0), "Hub_" + part, "HubShip")
    a.static_mesh_component.set_static_mesh(unreal.load_asset(BLK + "SM_HubBlk_" + part))
    if part == "Markers":
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        a.static_mesh_component.set_editor_property("cast_shadow", False)


def label(text, x, y, z, face_deg, colour, size=34.0):
    """A text label in the ship's frame, readable from the side it faces (face: 0 bow, 90 starboard)."""
    t = spawn(unreal.TextRenderActor, world(x, y, z), (0, YAW + face_deg, 0), "Label_" + text.split(" ")[0], "HubShip/Labels")
    c = t.get_editor_property("text_render")
    c.set_editor_property("text", text)
    c.set_editor_property("world_size", size)
    c.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    c.set_editor_property("text_render_color", colour)
    return t


FLOORS = {"slip": H.Z["quay"], "tally": H.Z["lower"], "keelgate": H.Z["quay"], "chandlery": H.Z["main"]}
SVC = {o["id"]: o.get("svc", "") for o in H.OPEN}
for name, fl in FLOORS.items():
    sp = H.space(name)
    npc = sp["npc"]
    label("%s %s NPC" % (sp["svc"], sp["name"]), npc["x"], npc["y"], fl + 2.1, npc["face"], unreal.Color(r=255, g=80, b=230, a=255))
    for e in sp["e"]:
        # Readable where the player stands: facing the way the NPC faces, the fitting berth's from the quay.
        face = 90.0 if e["id"] == "fitting_berth" else npc["face"]
        label("E %s" % e["prompt"], e["x"], e["y"], fl + 1.35, face, unreal.Color(r=60, g=220, b=255, a=255), size=22.0)


def open_side(gm):
    """Which way a gate's label should face: towards the open floor next to it (sampled 1.5 m to each side)."""
    rects = [r for _, r in H.open_rects(gm["level"])]
    best = 0.0
    for face in (0.0, 90.0, 180.0, 270.0):
        f = math.radians(face)
        px, py = gm["x"] + 1.5 * math.cos(f), gm["y"] + 1.5 * math.sin(f)
        if any(H.inside(r, px, py) for r in rects):
            return face
    return best


for gm in H.GATE_MARKS:
    zl = H.level_z(gm["level"])
    label("%s %s" % (gm["g"], gm["zone"]), gm["x"], gm["y"], zl + 2.6, open_side(gm), unreal.Color(r=255, g=70, b=50, a=255), size=28.0)

# --- light: lanterns where the data hangs them ----------------------------------------------------------------------------
lamps = []
for name, fl in (("slip", H.Z["quay"]), ("tally", H.Z["lower"]), ("keelgate", H.Z["quay"])):
    pr = H.prop(name, "lanterns")
    lamps += [(x, y, fl + z - 0.4) for x, y, z in pr["at"]]
ring = H.R(H.HUB["ring"])
for lvl in ("lower", "middle", "upper"):                                      # the Well's gallery corners (its note)
    lamps += [(x, y, H.Z[lvl] + 3.0) for x in (ring[0] + 0.6, ring[1] - 0.6) for y in (ring[2] + 0.6, ring[3] - 0.6)]
lamps.append((0.5 * (H.HUB["shaft"]["x0"] + H.HUB["shaft"]["x1"]), 0.0, H.Z["middle"] + 2.0))   # the ring lantern in the shaft
beams = H.prop("chandlery", "beams")
lamps += [(bx, -13.5, H.Z["main"] + beams["z"] - 0.5) for bx in beams["at_x"][::2]]                # stock lanterns from the beams
# Below decks the lanterns burn by day as well (the rig puts ASaltLantern out by day): plain warm lights at the lanterns' spots
# (D61 review: with the game's outdoor exposure and the lanterns out, the Tally House measured black by day).
for k, (x, y, z) in enumerate(lamps):
    lamp = spawn(unreal.PointLight, world(x, y, z), (0, YAW, 0), "Lantern_%02d" % k, "HubShip/Lanterns")
    lc = lamp.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", LANTERN_CD)
    lc.set_editor_property("use_temperature", True)
    lc.set_editor_property("temperature", 2100.0)
    lc.set_editor_property("attenuation_radius", 1600.0)
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
print("SB_HUB_LANTERNS", len(lamps))

# The eye adapts below decks: each enclosed space gets its own exposure range (the rig's presets clamp it for the open sea).
INTERIORS = {"Slip": (44.0, 64.0, -9.0, 19.0, 1.5, 13.5), "TallyStair": (30.0, 73.0, 6.0, 20.0, 1.5, 9.5),
             "KeelgateWell": (72.0, 96.0, -12.0, 12.0, 1.5, 17.5)}
for name, (x0, x1, y0, y1, z0, z1) in INTERIORS.items():
    v = spawn(unreal.PostProcessVolume, world(0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1)), (0, YAW, 0), "Exposure_" + name, "HubShip")
    v.set_actor_scale3d(unreal.Vector((x1 - x0) * 0.5, (y1 - y0) * 0.5, (z1 - z0) * 0.5))      # the brush is 2 m (200 cm) across
    v.set_editor_property("unbound", False)
    v.set_editor_property("priority", 1.0)
    v.set_editor_property("blend_radius", 300.0)
    ps = v.get_editor_property("settings")
    ps.set_editor_property("override_auto_exposure_min_brightness", True)
    ps.set_editor_property("auto_exposure_min_brightness", INTERIOR_MIN_EV)
    v.set_editor_property("settings", ps)

# --- the player's boat at the fitting berth, the player on the starboard quay --------------------------------------------
fb = H.prop("dock", "fitting_berth")
bx, by = 0.5 * (fb["x0"] + fb["x1"]), fb["y"]
boat = spawn(unreal.SaltBoat, world(bx, by, 0.0), (0, YAW, 0), "Boat")
boat.set_editor_property("mooring_reach", (H.HUB["basin"]["y1"] - by) * 100.0)     # to the starboard quay's edge
boat.set_editor_property("mooring_pier_height", H.Z["quay"] * 100.0)
spawn(unreal.PlayerStart, world(bx - 1.0, H.HUB["basin"]["y1"] + 2.0, H.Z["quay"] + 0.95), (0, YAW, 0), "PlayerStart")
rig.set_editor_property("start_region", "ShoresOfPlenty")
rig.set_editor_property("start_weather", "Clear")
rig.set_editor_property("start_sea_state", "Rough")
rig.apply_preset("Day")
les.save_current_level()
print("SB_HUB_MAP_OK", MAP, "berth", BERTH_ID, (BX, BY), "yaw", YAW)
