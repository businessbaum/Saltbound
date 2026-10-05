"""Home waters (D58): the waves round Saltbound Cove a bit lower than on the open sea.

Owner (2026-10-05): "Make the waves in the start area a bit more calm." A third calm zone on the ocean: within 700 m of
the cove the waves (the swells and the wind sea, on the GPU and for buoyancy) are 70 % as high, back to full height by
1.5 km (Ruin Isle, 1.7 km out, keeps the open sea). It scales whatever sea the rig sets (a flat sea stays flat, a storm
is milder near home); the cove's own calm zone (15 % within 25 m) still rules inside it. A sea-state zone was tried first
and dropped: it blends the whole sea towards its state, so it made a flat sea rougher.

Runs inside the editor on the open map (stop PIE first):
    python Tools/ue_exec.py Tools/ue_scripts/place_home_waters.py
create_visual_test_map.py calls add_zone() as well, so a rebuilt map keeps it.
"""
import unreal

CENTRE = (500.0, 300.0)            # the cove's calm zone centre (cm)
INNER, OUTER, FACTOR = 70000.0, 150000.0, 0.7


def home_zone():
    z = unreal.SaltCalmZone()
    z.set_editor_property("center", unreal.Vector2D(*CENTRE))
    z.set_editor_property("inner_radius", INNER)
    z.set_editor_property("outer_radius", OUTER)
    z.set_editor_property("min_factor", FACTOR)
    return z


def add_zone(zones):
    """The calm zones with the home waters added (or replaced: the zone at the cove's centre with the wide radius)."""
    keep = [z for z in zones if not (z.get_editor_property("inner_radius") >= 50000.0
                                     and abs(z.get_editor_property("center").x - CENTRE[0]) < 1.0
                                     and abs(z.get_editor_property("center").y - CENTRE[1]) < 1.0)]
    return keep + [home_zone()]


def place():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()
    for a in actors:
        if a.get_actor_label() == "SeaStateZone_Home":
            eas.destroy_actor(a)
    ocean = next(a for a in actors if a.get_class().get_name() == "SaltOcean")
    zones = add_zone(list(ocean.get_editor_property("calm_zones")))
    ocean.set_editor_property("calm_zones", zones)
    print("SB_HOME_WATERS calm zones:", [(z.get_editor_property("inner_radius"), z.get_editor_property("min_factor")) for z in zones])


if __name__ == "__main__":
    place()
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
