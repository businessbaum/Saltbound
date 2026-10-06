"""Frames of the hub's harbour passages as the player sees them (D70): PIE in the open map (L_VisualTest), a passage run with
sb.Hub.Passage, a frame written by the renderer (sb.Dev.DumpFrames) every few seconds, tiled into a contact sheet.

  python Tools/hub_passage_shot.py <out.png> [--run "arrive 0 1 long"] [--every 2.0] [--count 12] [--preset Day]
  --then "depart long"   a second passage straight after the first (its frames follow on the sheet)
Each frame is <out>_<k>.png; the sheet <out> has three columns, the time under each frame.
"""
import argparse
import glob
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import run_pie_test as R  # noqa: E402

SHOTS = os.path.join(os.path.dirname(__file__), "..", "Saltbound", "Saved", "Screenshots")


def frames():
    return set(glob.glob(os.path.join(SHOTS, "**", "MovieFrame*.png"), recursive=True))


def grab(path):
    before = frames()
    R.pie_console("sb.Dev.DumpFrames 1")
    new = set()
    t0 = time.time()
    while not new and time.time() - t0 < 10.0:
        time.sleep(0.2)
        new = frames() - before
    if not new:
        return False
    time.sleep(0.3)
    shutil.move(sorted(new)[-1], path)
    return True


def run(cmd, every, count, out, k0, stamps):
    R.pie_console("sb.Hub.Passage " + cmd)
    t0 = time.time()
    k = k0
    for i in range(count):
        target = t0 + 0.4 + i * every
        while time.time() < target:
            time.sleep(0.05)
        path = out.replace(".png", "_%02d.png" % k)
        if grab(path):
            stamps.append((path, "%s +%.1fs" % (cmd.split()[0], time.time() - t0)))
            print("SB_FRAME", path)
        k += 1
    return k


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--run", default="arrive 0 1 long")
    ap.add_argument("--then", default="")
    ap.add_argument("--every", type=float, default=2.0)
    ap.add_argument("--count", type=int, default=12)
    ap.add_argument("--then-count", type=int, default=0)
    ap.add_argument("--preset", default="Day")
    ns = ap.parse_args()
    R.start_pie()
    stamps = []
    try:
        time.sleep(2.0)
        R.pie_console("sb.Lighting " + ns.preset)
        R.pie_console("sb.Hub.Show 1")
        time.sleep(1.0)
        k = run(ns.run, ns.every, ns.count, ns.out, 0, stamps)
        if ns.then:
            time.sleep(2.0)
            run(ns.then, ns.every, ns.then_count or ns.count, ns.out, k, stamps)
    finally:
        R.stop_pie()
    if stamps:
        from PIL import Image, ImageDraw
        ims = [Image.open(p).convert("RGB") for p, _ in stamps]
        w = 960
        h = int(w * ims[0].size[1] / ims[0].size[0])
        cols = 3
        rows = (len(ims) + cols - 1) // cols
        sheet = Image.new("RGB", (w * cols, (h + 24) * rows), (20, 20, 20))
        draw = ImageDraw.Draw(sheet)
        for i, (im, (_, label)) in enumerate(zip(ims, stamps)):
            x, y = (i % cols) * w, (i // cols) * (h + 24)
            sheet.paste(im.resize((w, h)), (x, y))
            draw.text((x + 8, y + h + 4), label, fill=(230, 230, 230))
        sheet.save(ns.out)
        print("SB_SHEET", ns.out)


if __name__ == "__main__":
    main()
