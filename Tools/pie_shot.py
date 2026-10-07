"""Frames from play (D79): starts PIE in the open map, runs a script of console commands, waits and shots, and writes each shot
as the renderer drew it (sb.Dev.DumpFrames, no UI), so held tools, exposure, the underwater pass and the clock are as in the game.

  python Tools/pie_shot.py <out_dir> "<step>; <step>; ..." [--sheet name.jpg] [--keep-pie] [--settle 0]
  steps: a console command (sb.Lighting Day, sb.Dev.Camera ..., sb.Dev.Tool Spyglass, ...), "@wait <s>" (only for things that take
         time in the game, not for the image: shots wait until it has settled), "@shot <name>" (settled), "@now <name>" (the next
         frame as it is, for moments in motion), "@use 1|0" (the left mouse button pressed or let go, on the player).

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
import pie_wait  # noqa: E402
import run_pie_test as R  # noqa: E402
import ue_exec  # noqa: E402

SHOTS = os.path.join(os.path.dirname(__file__), "..", "Saltbound", "Saved", "Screenshots")


def frames():
    return set(glob.glob(os.path.join(SHOTS, "**", "MovieFrame*.png"), recursive=True))


def shot(path, settled=True):
    """A frame from play; settled (the default) dumps frames until the image has settled (exposure, Lumen, streaming:
    pie_wait.settled_shot), else takes the next frame as it is (moments in motion)."""
    if settled:
        ok = pie_wait.settled_shot(path)
        print("SB_SHOT" if ok else "SB_SHOT_FAIL", path)
        return path if ok else None
    before = frames()
    R.pie_console("sb.Dev.DumpFrames 1")
    new = pie_wait.new_frame(before)
    if not new:
        print("SB_SHOT_FAIL", path)
        return None
    shutil.move(new, path)
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
    ap.add_argument("--settle", type=float, default=0.0)
    ns = ap.parse_args()
    os.makedirs(ns.out_dir, exist_ok=True)
    R.start_pie()
    paths = []
    try:
        time.sleep(ns.settle)
        for step in [s.strip() for s in ns.script.split(";") if s.strip()]:
            if step.startswith("@wait"):
                time.sleep(float(step.split()[1]))
            elif step.startswith("@shot") or step.startswith("@now"):
                p = shot(os.path.join(ns.out_dir, step.split()[1] + ".png"), settled=step.startswith("@shot"))
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
