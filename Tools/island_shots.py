"""Named views of the home island (D71) from the editor viewport, with a contact sheet.

Usage (take the editor lock first: python Tools/editor_lock.py take island "<what>"):
  python Tools/island_shots.py <out_dir> [--only name ...] [--preset Day] [--sheet name.jpg] [--no-load]

Every shot stands at a point of the island (metres, X north, Y east) at a height above the ground (island_shape.py) and
looks at a target (metres; its z is above the ground too, unless given as absolute with 'abs'). Loads L_VisualTest when
another map is open (the hub agent works in L_HubReview and L_Hub).
"""
import argparse
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "Art", "Blender", "Scripts"))
import island_shape as S  # noqa: E402
import ue_exec  # noqa: E402
import ue_mcp  # noqa: E402

MAP = "/Game/Saltbound/Maps/L_VisualTest"

# name: (eye x, y, height above ground), (target x, y, height above ground)  [metres]
SHOTS = {
    "pier_to_beach": ((6.0, 0.4, None, 2.9), (-26.0, 4.0, 1.5)),
    "beach_line": ((-18.5, -9.0, 1.7), (-31.0, -24.0, 0.3)),
    "grass_edge_close": ((-24.0, 20.0, 1.6), (-29.5, 17.0, 0.0)),
    "path_inland": ((-20.0, -0.5, 1.7), (-31.0, -20.0, 0.8)),
    "palm_close": ((-21.5, 1.5, 1.6), (-25.7, 4.8, 3.2)),
    "tree_close": ((-27.0, 12.5, 1.6), (-32.5, 16.5, 2.2)),
    "forest_floor": ((-38.0, -24.0, 1.7), (-43.0, -28.5, 0.0)),
    "outer_beach": ((-70.0, 25.0, 1.7), (-74.0, -10.0, 0.5)),
    "hill_crown": ((80.0, -128.0, 1.7), (95.0, -145.0, 2.0)),
    "keeper_hut": ((-20.0, 4.0, 1.7), (-28.5, 12.5, 1.6)),
    "island_wide": ((-40.0, 60.0, 30.0), (40.0, -40.0, 0.0)),
    # D71: the Keeper's hut and the places to explore (island_shape.PLACES).
    "hut_from_beach": ((-15.0, 2.5, 1.7), (-23.5, 13.0, 2.0)),
    "keeper_counter": ((-19.1, 11.5, 1.65), (-22.4, 12.1, 1.3)),
    "wreck": ((-63.8, -100.0, 1.7), (-68.3, -108.0, 0.4)),
    "tide_pools": ((45.0, -186.0, 1.8), (50.5, -188.8, 0.0)),
    "lookout": ((90.5, -148.0, 1.7), (95.5, -143.5, 1.0)),
    "spring": ((71.8, -170.4, 1.7), (73.9, -166.2, 0.3)),
    "fire_pit": ((-30.3, -38.3, 1.7), (-34.0, -41.0, 0.2)),
    "dig_spot": ((-20.0, 3.5, 1.7), (-23.3, 4.9, 0.0)),
    # The lawn (the owner: "real visual grass small and a lot so it blends in").
    "lawn_close": ((-33.0, 20.0, 1.65), (-36.5, 22.5, 0.0)),
    "lawn_field": ((-36.0, 0.0, 1.7), (-50.0, 10.0, 0.6)),
    # D78: the owner's view (close, from above), the meadow at eye height, the forest floor's leaves close.
    "grass_top": ((-40.0, 14.0, 0.9), (-40.3, 14.2, 0.0)),
    "meadow_eye": ((-30.0, 30.0, 1.6), (-48.0, 22.0, 0.4)),
    "meadow_far": ((-25.0, -10.0, 2.2), (-60.0, 20.0, 1.0)),
    "litter_close": ((-41.0, -26.0, 1.4), (-43.3, -28.4, 0.0)),
    "litter_top": ((-44.6, -27.0, 0.9), (-44.8, -27.2, 0.0)),
}


def ground(x, y):
    import numpy as np
    return float(S.base(np.array([x]), np.array([y]))[0])


def pose(eye, target):
    ex, ey = eye[0], eye[1]
    ez = eye[3] if len(eye) > 3 and eye[2] is None else max(ground(ex, ey), 0.0) + eye[2]
    tx, ty, th = target
    tz = max(ground(tx, ty), 0.0) + th
    dx, dy, dz = tx - ex, ty - ey, tz - ez
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return (ex * 100.0, ey * 100.0, ez * 100.0, pitch, yaw, 0.0)


def contact_sheet(paths, out, cols=3, w=800):
    from PIL import Image, ImageDraw
    ims = [Image.open(p).convert("RGB") for p in paths]
    h = int(ims[0].height * w / ims[0].width)
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * (h + 24)), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for i, (im, p) in enumerate(zip(ims, paths)):
        x, y = (i % cols) * w, (i // cols) * (h + 24)
        sheet.paste(im.resize((w, h)), (x, y + 24))
        d.text((x + 6, y + 5), os.path.splitext(os.path.basename(p))[0], fill=(230, 230, 230))
    sheet.save(out, quality=88)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--preset", default="Day")
    ap.add_argument("--sheet", default=None)
    ap.add_argument("--settle", type=float, default=2.0)
    ap.add_argument("--no-load", action="store_true")
    ns = ap.parse_args()
    os.makedirs(ns.out, exist_ok=True)
    code = ("import unreal\n"
            "w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()\n"
            "les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
            "if %r and not w.get_path_name().startswith(%r):\n"
            "    les.load_level(%r)\n"
            "les.editor_set_game_view(True)\n"
            "for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():\n"
            "    if a.get_class().get_name() == 'SaltLightingRig':\n"
            "        a.apply_preset(%r)\n" % (not ns.no_load, MAP, MAP, ns.preset))
    ue_exec.run(code)
    time.sleep(4.0)
    paths = []
    for name, (eye, target) in SHOTS.items():
        if ns.only and name not in ns.only:
            continue
        path = os.path.join(ns.out, name + ".png")
        ue_mcp.capture(path, pose(eye, target), settle=ns.settle)
        paths.append(path)
        print("SHOT", path)
    if paths and ns.sheet:
        print("SHEET", contact_sheet(paths, os.path.join(ns.out, ns.sheet)))


if __name__ == "__main__":
    main()
