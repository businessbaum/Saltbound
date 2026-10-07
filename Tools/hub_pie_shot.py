"""Captures of the hub ship in play (D69): PIE in the open map (L_HubReview), the side doors shut, a fixed camera posed in the
ship's frame, each frame written by the renderer (sb.Dev.DumpFrames), so the doors, the exposure, the fog and the ropes are as
in the game.

  python Tools/hub_pie_shot.py <out.png> [--preset Day|Evening|Dusk] [--max 6] x y z pitch yaw [x y z pitch yaw ...]
  ship metres (x from the stern face, +y starboard, z up from the waterline); yaw in the ship's frame (0 = to the bow).
  Several poses write <out>_<k>.png and a two-column sheet <out>.
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "Art", "Blender", "Scripts"))
sys.path.insert(0, os.path.dirname(__file__))
import hub_layout as H  # noqa: E402
import pie_wait as W  # noqa: E402
import run_pie_test as R  # noqa: E402



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("vals", nargs="+", type=float)
    ap.add_argument("--preset", default="Day")
    ap.add_argument("--max", type=float, default=6.0)
    ap.add_argument("--keep-pie", action="store_true")
    ns = ap.parse_args()
    poses = [ns.vals[k:k + 5] for k in range(0, len(ns.vals), 5)]
    _, byaw, _ = H.berth_world()
    R.start_pie()
    paths = []
    try:
        R.pie_console("sb.Lighting " + ns.preset)
        R.pie_console("sb.Hub.Doors snapclosed")
        for k, (x, y, z, pitch, yaw) in enumerate(poses):
            wx, wy, wz = H.ship_to_world(x, y, z)
            R.pie_console("sb.Dev.Camera %.1f %.1f %.1f %.1f %.1f 80" % (wx, wy, wz, pitch, byaw + yaw))
            path = ns.out if len(poses) == 1 else ns.out.replace(".png", "_%d.png" % k)
            if W.settled_shot(path, max_s=ns.max):          # shoots until exposure, Lumen and streaming have settled
                paths.append(path)
    finally:
        if not ns.keep_pie:
            R.stop_pie()
    if len(paths) > 1:
        from PIL import Image
        ims = [Image.open(p).convert("RGB") for p in paths]
        w, h = 1280, int(1280 * ims[0].size[1] / ims[0].size[0])
        rows = int(math.ceil(len(ims) / 2))
        S = Image.new("RGB", (w * 2, h * rows), (20, 20, 20))
        for k, im in enumerate(ims):
            S.paste(im.resize((w, h)), ((k % 2) * w, (k // 2) * h))
        S.save(ns.out)
        print("SB_SHEET", ns.out)


if __name__ == "__main__":
    main()
