"""Frames from play (D79): starts PIE in the open map, runs a script of console commands, waits and shots, and writes each shot
as the renderer drew it (sb.Dev.DumpFrames, no UI), so held tools, exposure, the underwater pass and the clock are as in the game.

  python Tools/pie_shot.py <out_dir> "<step>; <step>; ..." [--sheet name.jpg] [--keep-pie] [--settle 2]
  steps: a console command (sb.Lighting Day, sb.Dev.Camera ..., sb.Dev.Tool Spyglass, ...), "@wait <s>", "@shot <name>",
         "@use 1|0" (the left mouse button pressed or let go, on the player).

Take the editor lock first (python Tools/editor_lock.py take <owner> "<what>").
"""
import argparse
import glob
import math
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import run_pie_test as R  # noqa: E402
import ue_exec  # noqa: E402

SHOTS = os.path.join(os.path.dirname(__file__), "..", "Saltbound", "Saved", "Screenshots")


def frames():
    return set(glob.glob(os.path.join(SHOTS, "**", "MovieFrame*.png"), recursive=True))


def shot(path):
    before = frames()
    R.pie_console("sb.Dev.DumpFrames 1")
    new = set()
    t0 = time.time()
    while not new and time.time() - t0 < 20.0:
        time.sleep(0.4)
        new = frames() - before
    if not new:
        print("SB_SHOT_FAIL", path)
        return None
    time.sleep(0.5)
    shutil.move(sorted(new)[-1], path)
    print("SB_SHOT", path)
    return path


def use(down):
    ue_exec.run(
        "import unreal\n"
        "w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()\n"
        "unreal.SystemLibrary.execute_console_command(w, 'sb.Dev.Use %d')\n" % (1 if down else 0))


def sheet(paths, out, width=960):
    from PIL import Image, ImageDraw
    ims = [Image.open(p).convert("RGB") for p in paths]
    h = int(width * ims[0].size[1] / ims[0].size[0])
    cols = 2 if len(ims) > 1 else 1
    rows = int(math.ceil(len(ims) / cols))
    S = Image.new("RGB", (width * cols, (h + 24) * rows), (20, 20, 20))
    d = ImageDraw.Draw(S)
    for k, (im, p) in enumerate(zip(ims, paths)):
        x, y = (k % cols) * width, (k // cols) * (h + 24)
        S.paste(im.resize((width, h)), (x, y + 24))
        d.text((x + 8, y + 5), os.path.splitext(os.path.basename(p))[0], fill=(230, 230, 230))
    S.save(out, quality=88)
    print("SB_SHEET", out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("script")
    ap.add_argument("--sheet", default="")
    ap.add_argument("--keep-pie", action="store_true")
    ap.add_argument("--settle", type=float, default=2.0)
    ns = ap.parse_args()
    os.makedirs(ns.out_dir, exist_ok=True)
    R.start_pie()
    paths = []
    try:
        time.sleep(ns.settle)
        for step in [s.strip() for s in ns.script.split(";") if s.strip()]:
            if step.startswith("@wait"):
                time.sleep(float(step.split()[1]))
            elif step.startswith("@shot"):
                p = shot(os.path.join(ns.out_dir, step.split()[1] + ".png"))
                if p:
                    paths.append(p)
            elif step.startswith("@use"):
                use(step.split()[1] == "1")
            else:
                R.pie_console(step)
    finally:
        if not ns.keep_pie:
            R.stop_pie()
    if ns.sheet and paths:
        sheet(paths, os.path.join(ns.out_dir, ns.sheet))


if __name__ == "__main__":
    main()
