"""Milestone 3 visual benchmark: /Game/Saltbound/Maps/L_VisualTest (script-owned, recreated on each run).

A small cove: coastal rocks, a two-section pier, the boat deck section moored alongside (riding the waves),
lanterns, a distant landmark in the fog, the ocean with a calm zone, and a lighting rig (Day / Evening).
Under the water (D28): a sculpted sand seabed (beach, pier ridge, drop-off, sandbank) with sea-grass patches.
The sailing boat (M4, D36) lies moored on the other side of the pier.
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(__file__))
import sb_ue  # noqa: E402

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Art", "Blender", "Scripts"))
import seabed_shape  # noqa: E402  (pure Python: the seabed's height function)

MAP = "/Game/Saltbound/Maps/L_VisualTest"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if sb_ue.eal.does_asset_exist(MAP):
    les.load_level(MAP)
    for actor in eas.get_all_level_actors():
        if not isinstance(actor, unreal.WorldSettings) and actor.get_class() != unreal.Brush.static_class():
            eas.destroy_actor(actor)
else:
    les.new_level(MAP)


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
    """rot = (pitch, yaw, roll) in degrees."""
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    if label:
        a.set_actor_label(label)
    return a


def mesh(label, path, loc, rot=(0, 0, 0), scale=1.0, mat=None):
    a = spawn(unreal.StaticMeshActor, loc, rot, label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    s = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
    a.set_actor_scale3d(unreal.Vector(*s))
    if mat:
        a.static_mesh_component.set_material(0, unreal.load_asset(mat))
    return a


# --- sky & lighting ---------------------------------------------------------------------------------
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
# Volumetric clouds belong to the environment rig (transient component, weather/region driven; D25).
pp = spawn(unreal.PostProcessVolume, label="GlobalPostProcess")
pp.set_editor_property("unbound", True)
rig = spawn(unreal.SaltLightingRig, (0, 0, 300), label="LightingRig")

# --- ocean -------------------------------------------------------------------------------------------
ocean = spawn(unreal.SaltOcean, (0, 0, 0), label="Ocean")
ocean.set_editor_property("underwater_particle_count", 380)   # D31: more visible particles
zone = unreal.SaltCalmZone()
zone.set_editor_property("center", unreal.Vector2D(500, 300))
zone.set_editor_property("inner_radius", 2500.0)
zone.set_editor_property("outer_radius", 15000.0)
zone.set_editor_property("min_factor", 0.15)  # sheltered cove; the open sea keeps the full swell
# Ruin Isle (D47): the swell settles around the island, so its beach is not overrun.
ISLE = (160000, 60000)
isle_zone = unreal.SaltCalmZone()
isle_zone.set_editor_property("center", unreal.Vector2D(*ISLE))
isle_zone.set_editor_property("inner_radius", 8000.0)
isle_zone.set_editor_property("outer_radius", 30000.0)
isle_zone.set_editor_property("min_factor", 0.3)
# Home waters (D58, owner: "make the waves in the start area a bit more calm"): 70 % within 700 m (place_home_waters.py).
import place_home_waters  # noqa: E402
ocean.set_editor_property("calm_zones", place_home_waters.add_zone([zone, isle_zone]))
# Wave height comes from the rig's sea state (StartSeaState below), not from the ocean actor.

# --- seabed --------------------------------------------------------------------------------------------
CUBE = "/Engine/BasicShapes/Cube"
UW = "/Game/Saltbound/Environment/Underwater/"
# Far floor 60 m down: with the clearer water (D29) a shallower floor would tint the open sea.
seabed = mesh("Seabed", CUBE, (0, 0, -6000), scale=(1200, 1200, 1), mat="/Game/Saltbound/Materials/Instances/MI_Sand")
# Sculpted cove floor (build_seabed.py): beach behind the rocks, about 2.3 m under the pier, a drop-off to the
# 17.5 m floor, and the shallow sandbank east of the pier (light ground: how each region's water reads).
# SM_Seabed (1 m cells, broad shape) only collides and is hidden; SM_Seabed_Sand (Nanite, 0.2 m) is the visible
# sand with sculpted ripples (D37).
cove_floor = mesh("Seabed_Cove", UW + "SM_Seabed", (0, 0, 0))
cove_floor.static_mesh_component.set_editor_property("visible", False)
cove_floor.set_actor_hidden_in_game(True)
sand = mesh("Seabed_Sand", UW + "SM_Seabed_Sand", (0, 0, 0))
sand.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
# Steep wave faces reflect rays downward; hardware-traced Lumen reflections would show the sand as beige
# streaks on the open sea. The seabed is only seen through the water, never in reflections.
for floor in (seabed, cove_floor, sand):
    floor.static_mesh_component.set_editor_property("visible_in_ray_tracing", False)
    floor.static_mesh_component.set_editor_property("affect_distance_field_lighting", False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground(x, y, top=800.0):
    """Highest surface below (x, y): (z, actor) or (None, None)."""
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, top), unreal.Vector(x, y, -2500.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None, None
    t = hit.to_tuple()
    return t[4].z, t[9]
# A distant region zone (2 km east): sailing there blends into the Ancient Isles palette.
region_zone = spawn(unreal.SaltRegionZone, (200000, 0, 0), label="RegionZone_AncientIsles")
region_zone.set_editor_property("region", "AncientIsles")
region_zone.set_editor_property("inner_radius", 30000.0)
region_zone.set_editor_property("outer_radius", 90000.0)
# Storm area (D30): 3 km south of the cove the sea blends into the Storm sea state (much bigger, longer waves)
# within 1.6 km and is fully stormy within 600 m. Fly or sail there to compare with the cove's Rough sea.
storm_zone = spawn(unreal.SaltSeaStateZone, (0, -300000, 0), label="SeaStateZone_Storm")
storm_zone.set_editor_property("sea_state", "Storm")
storm_zone.set_editor_property("inner_radius", 60000.0)
storm_zone.set_editor_property("outer_radius", 160000.0)

# --- coast -------------------------------------------------------------------------------------------
R = "/Game/Saltbound/Environment/Rocks/"
rocks = [
    ("Rock_A1", "SM_Rock_Coast_A", (-750, 150, -20), 30, 1.25),
    ("Rock_A2", "SM_Rock_Coast_A", (-550, -750, -40), 160, 0.95),
    ("Rock_A3", "SM_Rock_Coast_A", (-1100, -250, 60), 250, 1.5),
    ("Rock_B1", "SM_Rock_Coast_B", (-260, 520, -50), 70, 1.0),
    ("Rock_B2", "SM_Rock_Coast_B", (-220, -380, -70), 10, 1.1),
    ("Rock_B3", "SM_Rock_Coast_B", (-600, 850, -30), 200, 1.3),
    ("Rock_C1", "SM_Rock_Coast_C", (-90, 230, -60), 15, 1.0),
    ("Rock_C2", "SM_Rock_Coast_C", (-40, -190, -70), 120, 0.9),
    ("Rock_C3", "SM_Rock_Coast_C", (160, 640, -80), 300, 1.2),
    ("Rock_C4", "SM_Rock_Coast_C", (-380, 820, -40), 60, 1.4),
    ("Rock_C5", "SM_Rock_Coast_C", (320, -660, -90), 210, 1.1),
    ("Headland_A", "SM_Rock_Coast_A", (900, 3900, -150), 80, 2.6),
    ("Headland_B", "SM_Rock_Coast_B", (2000, 3300, -100), 140, 2.2),
    ("Headland_C", "SM_Rock_Coast_A", (3400, 4300, -250), 20, 2.0),
]
for label, m, loc, yaw, s in rocks:
    mesh(label, R + m, loc, (0, yaw, 0), s)

# No small scatter rocks around the big ones (owner: "we dont need those", D30).
import random  # noqa: E402
mesh("Landmark_Spire", R + "SM_Landmark_Spire", (42000, 15000, -600), (0, 35, 0), 2.0)

# --- Ruin Isle (D47): the first destination, 1.7 km out towards the Ancient Isles, and island masses on the horizon -
I = "/Game/Saltbound/Environment/Islands/"
isle_col = mesh("RuinIsle_Collision", I + "SM_Island_Ruin_Col", (ISLE[0], ISLE[1], 0))
isle_col.static_mesh_component.set_editor_property("visible", False)
isle_col.set_actor_hidden_in_game(True)
isle = mesh("RuinIsle", I + "SM_Island_Ruin", (ISLE[0], ISLE[1], 0))
isle.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def isle_coast(a):
    """Coast radius (cm) of Ruin Isle at the angle a: build_islands.coast_radius."""
    return 5500.0 * (1.0 + 0.16 * math.sin(3 * a + 0.7) + 0.09 * math.sin(5 * a + 2.1) + 0.05 * math.sin(9 * a + 4.0))


# The broken gateway on the knoll faces the cove; a wall corner beside it; rocks along the windward shore.
kx, ky = ISLE[0] + 1400, ISLE[1] - 800
kz, _ = ground(kx, ky, top=2500.0)
mesh("RuinIsle_Arch", I + "SM_Ruin_Arch", (kx, ky, (kz or 800.0) - 15.0), (0, 200, 0))
wz, _ = ground(kx + 700, ky + 900, top=2500.0)
mesh("RuinIsle_Wall", I + "SM_Ruin_Wall", (kx + 700, ky + 900, (wz or 700.0) - 10.0), (0, 30, 0))
for k, (a, rock, scale, yaw) in enumerate(((0.9, "A", 2.4, 30), (2.6, "B", 2.0, 110), (4.4, "C", 2.8, 250), (5.6, "A", 1.8, 300))):
    r = isle_coast(a) * 0.97
    mesh("RuinIsle_Rock%d" % k, R + "SM_Rock_Coast_" + rock, (ISLE[0] + r * math.cos(a), ISLE[1] + r * math.sin(a), -120), (0, yaw, 0), scale)
gulls_isle = spawn(unreal.SaltBirds, (ISLE[0], ISLE[1], 0), label="Gulls_RuinIsle")
gulls_isle.set_editor_property("count", 6)
gulls_isle.set_editor_property("seed", 33)
gulls_isle.set_editor_property("area_radius", 6000.0)
# --- Pickups (D50): driftwood, shells and a glass float on the cove's beach, more on Ruin Isle, and the ancient shard in
# front of the gateway. Each lies on the sand (traced); its mesh is set here too (the editor does not re-run the
# actor's construction after its item is set).
P = "/Game/Saltbound/Props/Pickups/SM_Pickup_"
dx, dy = math.cos(math.radians(200)), math.sin(math.radians(200))
PICKUPS = [
    ("Cove_Driftwood_1", "Driftwood", (-2400, -900), 30), ("Cove_Driftwood_2", "Driftwood", (-2700, 1500), 110),
    ("Cove_Seashell_1", "Seashell", (-1900, -300), 10), ("Cove_Seashell_2", "Seashell", (-2100, 600), 140),
    ("Cove_Seashell_3", "Seashell", (-2600, -1700), 250), ("Cove_GlassFloat_1", "GlassFloat", (-2000, 2200), 0),
    ("Isle_Driftwood_1", "Driftwood", (ISLE[0] - 3900, ISLE[1] + 900), 75), ("Isle_Driftwood_2", "Driftwood", (ISLE[0] + 2000, ISLE[1] + 4300), 160),
    ("Isle_Seashell_1", "Seashell", (ISLE[0] - 4100, ISLE[1] - 500), 40), ("Isle_Seashell_2", "Seashell", (ISLE[0] - 3600, ISLE[1] + 2200), 200),
    ("Isle_Seashell_3", "Seashell", (ISLE[0] + 1500, ISLE[1] - 4200), 300), ("Isle_RopeCoil_1", "RopeCoil", (ISLE[0] - 3300, ISLE[1] - 1800), 20),
    ("Isle_AncientShard", "AncientShard", (kx + dx * 160, ky + dy * 160), 200),
]
for pid, item, (px, py), yaw in PICKUPS:
    pz, _ = ground(px, py, top=2500.0)
    if pz is None or pz < 20.0:
        print("SB_PICKUP_SKIPPED", pid, pz)
        continue
    pick = spawn(unreal.SaltPickup, (px, py, pz - 2.0), (0, yaw, 0), "Pickup_" + pid)
    pick.set_editor_property("item_id", item)
    pick.set_editor_property("pickup_id", pid)
    pick.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(unreal.load_asset(P + item))
# Island masses 4-7 km out: the haze fades them into the sky.
for label, name, loc, yaw in (("FarIsland_A", "SM_Island_Far_A", (550000, 250000), 20),
                              ("FarIsland_B", "SM_Island_Far_B", (700000, -150000), 140),
                              ("FarIsland_C", "SM_Island_Far_C", (420000, -480000), 75)):
    mesh(label, I + name, (loc[0], loc[1], -300), (0, yaw, 0))

# --- pier --------------------------------------------------------------------------------------------
H = "/Game/Saltbound/Environment/Harbor/"
mesh("Dock_1", H + "SM_DockSection", (200, 0, 0))
mesh("Dock_2", H + "SM_DockSection", (1000, 0, 0), (0, 0, 0))

# --- moored boat section (rides the waves; walkable) ----------------------------------------------
boat = spawn(unreal.SaltDevFloater, (900, 330, 75), (0, 0, 0), "BoatSection")
boat.mesh.set_static_mesh(unreal.load_asset("/Game/Saltbound/Dev/VisualTest/SM_TestDeckSection"))


def attach(actor, parent):
    actor.attach_to_actor(parent, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, False)


def local_to_world(parent_loc, local):
    return (parent_loc[0] + local[0], parent_loc[1] + local[1], parent_loc[2] + local[2])


bl = (900, 330, 75)
lantern_boat = spawn(unreal.SaltLantern, local_to_world(bl, (-42, 42, 103)), (0, 20, 0), "Lantern_Boat")
attach(lantern_boat, boat)
crate = mesh("Crate_Boat", "/Game/Saltbound/Props/SM_Crate_Small", local_to_world(bl, (60, 95, 0)), (0, 8, 0))
crate.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
attach(crate, boat)
spawn(unreal.SaltLantern, (1310, -70, 110), (0, -30, 0), "Lantern_Dock")

# --- the sailing boat (M4, D36): greybox, moored on the pier's south side with the bow to the sea --------------
spawn(unreal.SaltBoat, (900, -350, 0), (0, 0, 0), "Boat")
# The boat lays its own gangway to the pier while moored (D43), so it is not a separate actor.

# --- seagulls (ambient): a flock circling over the cove and a few birds out at sea -----------------------------
spawn(unreal.SaltBirds, (1500, 0, 0), label="Gulls_Cove")
gulls_sea = spawn(unreal.SaltBirds, (16000, -7000, 0), label="Gulls_Sea")
gulls_sea.set_editor_property("count", 5)
gulls_sea.set_editor_property("seed", 21)
gulls_sea.set_editor_property("area_radius", 9000.0)
mesh("Crate_Dock", "/Game/Saltbound/Props/SM_Crate_Small", (1180, -60, 110), (0, 25, 0))
mesh("Crate_Dock2", "/Game/Saltbound/Props/SM_Crate_Small", (1185, -62, 150.5), (0, 12, 0), 0.85)

# --- sea grass (D28): a few patches on the sand, 1.2-14 m deep, never under the pier or on rocks -----------
grng = random.Random(11)
grass = {"A": [], "B": [], "C": []}
patches = 0
while patches < 34:
    cx, cy = grng.uniform(-1200.0, 6500.0), grng.uniform(-4200.0, 4200.0)
    cz, actor = ground(cx, cy)
    if cz is None or actor != cove_floor or not -1300.0 < cz < -150.0:
        continue
    patches += 1
    radius = grng.uniform(180.0, 480.0)
    for _ in range(grng.randint(6, 20)):
        r = radius * math.sqrt(grng.random())
        a = grng.uniform(0.0, 2.0 * math.pi)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        if -450.0 < x < 1700.0 and abs(y) < 320.0:
            continue
        z, actor = ground(x, y)
        if z is None or actor != cove_floor or not -1400.0 < z < -120.0:
            continue
        # Taller clumps in the patch centre and in deeper water; short ones at the rim.
        centre = 1.0 - r / radius
        kind = "A" if grng.random() < 0.25 + 0.5 * centre else ("B" if grng.random() < 0.6 else "C")
        sc = grng.uniform(0.8, 1.25) * (1.0 + min(-z / 1400.0, 1.0) * 0.4)
        z = seabed_shape.height(x / 100.0, y / 100.0) * 100.0      # the visible, rippled surface
        grass[kind].append(unreal.Transform(unreal.Vector(x, y, z - 3.0),
                                            unreal.Rotator(roll=grng.uniform(-6, 6), pitch=grng.uniform(-6, 6), yaw=grng.uniform(0, 360)),
                                            unreal.Vector(sc, sc, sc)))
for kind, transforms in grass.items():
    scatter = spawn(unreal.SaltInstanceScatter, (0, 0, 0), label="SeaGrass_" + kind)
    scatter.set_instances(unreal.load_asset(UW + "SM_SeaGrass_" + kind),
                          unreal.load_asset("/Game/Saltbound/Materials/Instances/MI_SeaGrass"), transforms, 5000.0, 7000.0, False)
print("SB_SEAGRASS patches=%d clumps=%s" % (patches, {k: len(v) for k, v in grass.items()}))

spawn(unreal.PlayerStart, (380, -10, 110 + 92), (0, 0, 0), "PlayerStart")
rig.set_editor_property("start_region", "ShoresOfPlenty")
rig.set_editor_property("start_weather", "Clear")
rig.set_editor_property("start_sea_state", "Rough")
rig.apply_preset("Day")
les.save_current_level()
print("SB_MAP_OK", MAP)
