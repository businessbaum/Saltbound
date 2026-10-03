"""Capture named camera shots of the open editor level for each lighting preset; writes PNGs + a contact sheet.

Usage: python Tools/capture_shots.py <shot-set> [--presets Day Evening] [--out Docs/Evidence/M3]
Shot sets are defined below (camera location cm, rotation pitch/yaw/roll deg).
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import ue_exec  # noqa: E402
import ue_mcp  # noqa: E402

SHOTS = {
    "visualtest": [
        ("01_deck_forward", (880, 390, 240), (-16, 2, 0)),
        ("02_lantern_close", (962, 398, 222), (-24, -163, 0)),
        ("03_from_dock", (500, -20, 275), (-14, 40.8, 0)),
        ("04_rail_detail", (960, 395, 220), (-36, 42, 0)),
        ("05_cove_wide", (2600, -900, 600), (-13, 152, 0)),
        ("06_landmark", (1350, 0, 275), (1.0, 19.5, 0)),
        ("07_water_rocks", (250, -500, 80), (-8, 160, 0)),
        ("08_dock_end", (1250, -150, 275), (-18, 120, 0)),
        ("09_open_sea", (22000, -6000, 260), (-3, 35, 0)),
        ("10_open_sea_sun", (22000, -6000, 260), (-3, 120, 0)),
        # Owner reference comparisons (session 2): first person on the pier, low over the open sea.
        ("11_dock_fp", (-160, 45, 285), (-16, 6, 0)),
        ("12_dock_fp_boat", (650, -60, 285), (-14, 40, 0)),
        ("13_sea_low", (22000, -6000, 330), (-6, 30, 0)),
        ("14_sea_low_cross", (22000, -6000, 330), (-6, 105, 0)),
        # Reference-like low view (owner's Sea of Thieves screenshot): 1.6 m above the water, looking
        # along the swell with the sun ahead-right.
        ("15_ref_low", (22000, -6000, 160), (-7, 95, 0)),
    ],
}


def apply_preset(name):
    ue_exec.run(
        "import unreal\n"
        "eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
        "for a in eas.get_all_level_actors():\n"
        "    if a.get_class().get_name() == 'SaltLightingRig':\n"
        "        a.apply_preset(%r)\n" % name)


def contact_sheet(paths, out, cols=4, w=640):
    from PIL import Image, ImageDraw
    ims = [Image.open(p).convert("RGB") for p in paths]
    h = int(ims[0].height * w / ims[0].width)
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * (h + 22)), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for i, (im, p) in enumerate(zip(ims, paths)):
        x, y = (i % cols) * w, (i // cols) * (h + 22)
        sheet.paste(im.resize((w, h)), (x, y + 22))
        d.text((x + 6, y + 4), os.path.splitext(os.path.basename(p))[0], fill=(230, 230, 230))
    sheet.save(out)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("set")
    ap.add_argument("--presets", nargs="*", default=["Day", "Evening"])
    ap.add_argument("--out", default=os.path.join(os.environ.get("TEMP", "."), "saltbound_shots"))
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--settle", type=float, default=2.5)
    ns = ap.parse_args()
    os.makedirs(ns.out, exist_ok=True)
    # Game view hides editor sprites (light icons, volumes) in the captures.
    ue_exec.run("import unreal\n"
                "unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)\n")
    for preset in ns.presets:
        apply_preset(preset)
        time.sleep(4.0)  # let exposure, sky capture and Lumen settle
        paths = []
        for name, loc, rot in SHOTS[ns.set]:
            if ns.only and name not in ns.only:
                continue
            path = os.path.join(ns.out, "%s_%s.png" % (preset, name))
            ue_mcp.capture(path, (*loc, *rot), settle=ns.settle)
            paths.append(path)
            print("SHOT", path)
        if paths:
            print("SHEET", contact_sheet(paths, os.path.join(ns.out, "sheet_%s.png" % preset)))


if __name__ == "__main__":
    main()
