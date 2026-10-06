"""Frames of the gulls in flight (D76): PIE in the open map, a camera following one gull (sb.Dev.BirdCam), a frame written by
the renderer every so often (sb.Dev.DumpFrames), tiled into a contact sheet with the time under each.

  python Tools/bird_shot.py <out.png> [--views "0 400 160 12; 3 300 90 5; 5 350 20 -25"] [--frames 6] [--every 0.35]
                            [--pre "slomo 0.08; sb.Birds.Flap 1"]   (a slow wingbeat to look at)
  each view: bird index, distance (cm), yaw round from its heading (180 = behind), pitch (deg above it)
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
        time.sleep(0.05)
        new = frames() - before
    if not new:
        return False
    time.sleep(0.2)
    shutil.move(sorted(new)[-1], path)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--views", default="0 420 160 10; 2 320 95 4; 4 380 25 -22")
    ap.add_argument("--frames", type=int, default=6)
    ap.add_argument("--every", type=float, default=0.35)
    ap.add_argument("--preset", default="Day")
    ap.add_argument("--settle", type=float, default=1.5)
    ap.add_argument("--pre", default="", help='console commands first, e.g. "slomo 0.08; sb.Birds.Flap 1"')
    ns = ap.parse_args()
    views = [v.split() for v in ns.views.split(";") if v.strip()]
    R.start_pie()
    shots = []
    try:
        time.sleep(2.0)
        R.pie_console("sb.Lighting " + ns.preset)
        for cmd in [c.strip() for c in ns.pre.split(";") if c.strip()]:
            R.pie_console(cmd)
        for vi, v in enumerate(views):
            R.pie_console("sb.Dev.BirdCam " + " ".join(v))
            time.sleep(ns.settle)
            t0 = time.time()
            for k in range(ns.frames):
                path = ns.out.replace(".png", "_%d_%02d.png" % (vi, k))
                if grab(path):
                    shots.append((path, "bird %s view %d +%.2fs" % (v[0], vi, time.time() - t0)))
                    print("SB_FRAME", path)
                time.sleep(ns.every)
        R.pie_console("sb.Dev.BirdCam off")
    finally:
        R.stop_pie()
    if shots:
        from PIL import Image, ImageDraw
        ims = [Image.open(p).convert("RGB") for p, _ in shots]
        w = 640
        h = int(w * ims[0].size[1] / ims[0].size[0])
        cols = ns.frames
        rows = (len(ims) + cols - 1) // cols
        sheet = Image.new("RGB", (w * cols, (h + 20) * rows), (20, 20, 20))
        d = ImageDraw.Draw(sheet)
        for i, (im, (_, label)) in enumerate(zip(ims, shots)):
            x, y = (i % cols) * w, (i // cols) * (h + 20)
            sheet.paste(im.resize((w, h)), (x, y))
            d.text((x + 6, y + h + 3), label, fill=(230, 230, 230))
        sheet.save(ns.out)
        print("SB_SHEET", ns.out)


if __name__ == "__main__":
    main()
