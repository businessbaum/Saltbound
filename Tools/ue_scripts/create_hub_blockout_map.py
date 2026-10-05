"""The hub ship's review map (D61; phase 1 of Docs/Design/HubShip/HubShip_Agent_Prompt.md): /Game/Saltbound/Maps/L_HubBlockout.

Script-owned, recreated on each run, never L_VisualTest (the owner plays that). The ocean, sky and lighting rig as in the
visual test map; the blockout (SM_HubBlk_*, build_hub_blockout.py) at her berth `SaltboundRoads` (hub.berth); the calm
zones round her and in her basin; the player's boat moored at the fitting berth (its start position is the mooring, its
lines and gangway reach the starboard quay); the player on the starboard quay; text labels at the NPC spots and the gates;
lanterns where the data hangs them (and on the Well's gallery corners and in the shaft, its note).

    python Tools/ue_exec.py Tools/ue_scripts/create_hub_blockout_map.py [art]

With `art` it builds the art pass's review map instead, /Game/Saltbound/Maps/L_HubReview (D62, D63): the ship as the game
streams her in (hub_place.py, shared with L_Hub: the art over the blockout as hidden collision, the sea gate, the gates, the E
points, the hoist, the ladders, the moorings), no labels or markers.
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

import importlib  # noqa: E402
import hub_place  # noqa: E402
importlib.reload(hub_place)        # the editor keeps modules (and hub_place reloads the data)
H = hub_place.H

ART = "art" in sys.argv[1:]
MAP = "/Game/Saltbound/Maps/L_HubReview" if ART else "/Game/Saltbound/Maps/L_HubBlockout"

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
ocean.set_editor_property("calm_zones", hub_place.calm_zones(local=False, berth=((BX, BY, BZ), YAW, BERTH_ID)))
print("SB_HUB_ZONES ship", [round(c) for c in world(H.PIVOT_X, 0, 0)[:2]], "hall", [round(c) for c in world(0.5 * sum(hub_place.water_x()), 0, 0)[:2]])
# The far floor, 60 m down under her (the visual test map's floor; it is not under her whole hull there).
floor = spawn(unreal.StaticMeshActor, (BX, BY, -6000), label="Seabed")
floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
floor.set_actor_scale3d(unreal.Vector(1200, 1200, 1))
floor.static_mesh_component.set_material(0, unreal.load_asset("/Game/Saltbound/Materials/Instances/MI_Sand"))
floor.static_mesh_component.set_editor_property("visible_in_ray_tracing", False)

# --- the ship ----------------------------------------------------------------------------------------------------------------
placer = hub_place.Placer(eas, (BX, BY, BZ), YAW, "HubShip")
counts = hub_place.everything(placer, under_art=ART)
print("SB_HUB_PLACED", counts)


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
for name, fl in ({} if ART else FLOORS).items():
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


for gm in ([] if ART else hub_place.PL.all_gate_marks()):
    zl = H.level_z(gm["level"])
    label("%s %s" % (gm["g"], gm["zone"]), gm["x"], gm["y"], zl + 2.6, open_side(gm), unreal.Color(r=255, g=70, b=50, a=255), size=28.0)

# --- the player's boat at the fitting berth, the player on the forward quay beside her ------------------------------------
_bid, bx, by, bh, breach = hub_place.berth_spots()[0]
boat = spawn(unreal.SaltBoat, world(bx, by, 0.0), (0, YAW + bh, 0), "Boat")
boat.set_editor_property("mooring_reach", breach * 100.0)                           # to the forward quay's edge, on her starboard
boat.set_editor_property("mooring_pier_height", H.Z["quay"] * 100.0)
spawn(unreal.PlayerStart, world(bx + breach + 1.5, by, H.Z["quay"] + 0.95), (0, YAW + 180.0, 0), "PlayerStart")
rig.set_editor_property("start_region", "ShoresOfPlenty")
rig.set_editor_property("start_weather", "Clear")
rig.set_editor_property("start_sea_state", "Rough")
rig.apply_preset("Day")
les.save_current_level()
print("SB_HUB_MAP_OK", MAP, "berth", BERTH_ID, (BX, BY), "yaw", YAW)
