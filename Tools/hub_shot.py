"""Editor viewport captures of the hub ship in her review map, posed in the ship's frame (D69).

  python Tools/hub_shot.py <out.png> x y z pitch yaw [x y z pitch yaw ...] [--sheet]
  x, y, z in ship metres (x from the stern face, +y starboard, z up from the waterline); yaw in the ship's frame
  (0 = looking at the bow, 90 = to starboard, 180 = aft). Several poses write <out>_<k>.png; --sheet also tiles them
  into <out> (two columns).
Needs L_HubReview open (her berth from the data, hub_layout.berth_world).
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "Art", "Blender", "Scripts"))
sys.path.insert(0, os.path.dirname(__file__))
import hub_layout as H  # noqa: E402
import ue_mcp  # noqa: E402


def pose_world(x, y, z, pitch, yaw):
    wx, wy, wz = H.ship_to_world(x, y, z)
    _, byaw, _ = H.berth_world()
    return (wx, wy, wz, pitch, byaw + yaw, 0.0)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    sheet = "--sheet" in sys.argv
    out = args[0]
    vals = [float(v) for v in args[1:]]
    poses = [vals[k:k + 5] for k in range(0, len(vals), 5)]
    paths = []
    for k, p in enumerate(poses):
        path = out if len(poses) == 1 else out.replace(".png", "_%d.png" % k)
        ue_mcp.capture(path, pose_world(*p), settle=2.0)
        paths.append(path)
        print("SB_SHOT", path)
    if sheet and len(paths) > 1:
        from PIL import Image
        ims = [Image.open(p).convert("RGB") for p in paths]
        w, h = ims[0].size
        cols = 2
        rows = int(math.ceil(len(ims) / cols))
        S = Image.new("RGB", (w * cols, h * rows), (20, 20, 20))
        for k, im in enumerate(ims):
            S.paste(im.resize((w, h)), ((k % cols) * w, (k // cols) * h))
        S.save(out)
        print("SB_SHEET", out)


if __name__ == "__main__":
    main()
