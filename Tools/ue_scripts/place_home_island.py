"""The home island's actors in L_VisualTest (D71): its ground, rocks, plants, sea grass, the small things on the ground, the
places to explore and the Harbour Keeper's hut. Replaces only these actors in the open map, so it runs without rebuilding the
map (the hub agent's berth and everything else stay); create_visual_test_map.py calls place() too, so a rebuilt map keeps it.

    python Tools/ue_exec.py Tools/ue_scripts/place_home_island.py        (stop PIE first; saves the map)

Data: Art/Generated/home_island.json (build_home_island.py: plants, rocks, sea grass, the ground's details) and
Art/Generated/home_places.json (build_home_props.py: the props of the places and the hut, the Keeper's spot).
The collision (Home_Collision, SM_Home_Col) and the far sea floor stay with create_visual_test_map.py.
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, "..", "..", "Art", "Generated")
I = "/Game/Saltbound/Environment/Islands/"
R = "/Game/Saltbound/Environment/Rocks/"
VEG = "/Game/Saltbound/Environment/Vegetation/"
UW = "/Game/Saltbound/Environment/Underwater/"
PROPS = "/Game/Saltbound/Environment/HomeIsland/"
FOLDER = "HomeIsland"

# Labels this script owns (prefixes); everything else in the map is left alone.
OWNED = ("Home_Sand", "Home_Turf", "Home_Ground", "Island_Rock_", "Plants_", "SeaGrass_", "Detail_", "Place_", "Keeper_")

# (cull start, cull end in cm (0 = never), casts shadows, collides) by the mesh's kind (D60; D71 adds the details).
KINDS = {"Grass_Patch": (5000.0, 7000.0, False, False), "Litter_": (4000.0, 5500.0, False, False),
         "Palm": (0.0, 0.0, True, True), "Tree": (0.0, 0.0, True, True), "Bush": (25000.0, 30000.0, True, False),
         "BigLeaf": (25000.0, 30000.0, True, False), "Fern": (12000.0, 15000.0, True, False),
         "GrassTuft": (6000.0, 8000.0, False, False), "Runner": (5000.0, 7000.0, False, False),
         "Debris": (4000.0, 6000.0, True, False), "Shell": (2500.0, 3500.0, False, False), "Pebble": (3500.0, 5000.0, True, False),
         "Burrow": (2500.0, 3500.0, False, False), "Clump": (5000.0, 7000.0, False, False)}


def load(name):
    path = os.path.join(GEN, name)
    if not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def transforms(rows):
    return [unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw), unreal.Vector(sc, sc, sc))
            for x, y, z, yaw, pitch, roll, sc in rows]


def clear(eas):
    n = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith(OWNED):
            eas.destroy_actor(a)
            n += 1
    return n


def spawn(eas, cls, loc, rot=(0.0, 0.0, 0.0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    if label:
        a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    return a


def mesh(eas, label, path, loc, rot=(0.0, 0.0, 0.0), scale=1.0, collide=True, shadow=True):
    a = spawn(eas, unreal.StaticMeshActor, loc, rot, label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    s = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
    a.set_actor_scale3d(unreal.Vector(*s))
    if not collide:
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    a.static_mesh_component.set_cast_shadow(shadow)
    return a


def scatter(eas, label, mesh_path, rows, kind, material=None):
    c0, c1, shadow, collide = KINDS[kind]
    s = spawn(eas, unreal.SaltInstanceScatter, (0, 0, 0), label=label)
    s.set_instances(unreal.load_asset(mesh_path), unreal.load_asset(material) if material else None, transforms(rows), c0, c1, shadow, collide)
    return s


def place(eas=None):
    eas = eas or unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = clear(eas)
    home = load("home_island.json")
    # The ground (D71): one visible surface; the hidden SM_Home_Col (create_visual_test_map.py) collides.
    ground = mesh(eas, "Home_Ground", I + "SM_Home_Ground", (0, 0, 0), collide=False)
    # The island's rocks (D60).
    for k, (m, x, y, z, yaw, s) in enumerate(home.get("rocks", [])):
        mesh(eas, "Island_Rock_%02d" % k, R + m, (x, y, z), (0, yaw, 0), s)
    # Plants (D60, D71) and the small things on the ground (D71): instanced.
    counts = {}
    for group, path in (("plants", VEG), ("details", PROPS)):
        for name, rows in sorted(home.get(group, {}).items()):
            kind = next((k for k in KINDS if k in name), None)
            if kind is None or not rows:
                print("SB_HOME_ISLAND skipped", name)
                continue
            mesh_path = (PROPS if name.startswith("SM_Home_") else VEG) + name
            s = scatter(eas, ("Plants_" if group == "plants" else "Detail_") + name[3:], mesh_path, rows, kind)
            # D78: the lawn's blades sway only within 25 m, the fallen leaves never (cheaper for Nanite's rasteriser).
            wpo_off = {"Grass_Patch": 2500, "Litter_": 1}.get(kind)
            if wpo_off:
                s.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent).set_editor_property(
                    "world_position_offset_disable_distance", wpo_off)
            counts[name] = len(rows)
    for kind, rows in home.get("seagrass", {}).items():
        scatter(eas, "SeaGrass_" + kind, UW + "SM_SeaGrass_" + kind, rows, "GrassTuft", "/Game/Saltbound/Materials/Instances/MI_SeaGrass")
    # The places and the Keeper's hut (build_home_props.py).
    places = load("home_places.json")
    for k, p in enumerate(places.get("meshes", [])):
        a = mesh(eas, "Place_%s" % p["label"], p.get("path", PROPS) + p["mesh"], p["loc"], p.get("rot", (0, 0, 0)), p.get("scale", 1.0),
                 collide=p.get("collide", True), shadow=p.get("shadow", True))
        if p["mesh"] == "SM_Home_Smoke":
            a.static_mesh_component.set_editor_property("visible_in_ray_tracing", False)
    for p in places.get("lights", []):
        light = spawn(eas, unreal.PointLight, p["loc"], (0, 0, 0), "Place_Light_%s" % p["label"])
        lc = light.point_light_component
        lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        lc.set_editor_property("intensity", p.get("intensity", 5.0))
        lc.set_editor_property("light_color", unreal.Color(*[int(255 * c) for c in p.get("color", (1.0, 0.5, 0.2))], 255))
        lc.set_editor_property("attenuation_radius", p.get("radius", 600.0))
        lc.set_editor_property("cast_shadows", False)
    for k, p in enumerate(places.get("lanterns", [])):
        lantern = spawn(eas, unreal.SaltLantern, p["loc"], p.get("rot", (0, 0, 0)), "Keeper_Lantern_%d" % k)
        for prop, val in p.get("props", {}).items():
            lantern.set_editor_property(prop, val)
    keeper = places.get("keeper")
    if keeper:
        pt = spawn(eas, unreal.SaltHubPoint, keeper["point"], (0, keeper["yaw"], 0), "Keeper_Point")
        pt.set_editor_property("space", "home")
        pt.set_editor_property("point_id", "keeper")
        pt.set_editor_property("prompt", unreal.Text("Talk to the Harbour Keeper"))
        pt.set_editor_property("use_extent", unreal.Vector(*keeper.get("extent", (45.0, 90.0, 60.0))))
        spot = spawn(eas, unreal.TargetPoint, keeper["spot"], (0, keeper["yaw"], 0), "Keeper_Spot")
        spot.tags = [unreal.Name("KeeperSpot")]
        if keeper.get("figure"):
            mesh(eas, "Keeper_Figure", PROPS + keeper["figure"], keeper["spot"], (0, keeper["yaw"], 0), collide=False)
    print("SB_HOME_ISLAND placed: removed", removed, "plants/details", counts, "places", len(places.get("meshes", [])))
    return ground


if __name__ == "__main__":
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise SystemExit("SB_HOME_ISLAND stop PIE first")
    place()
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
