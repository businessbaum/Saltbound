"""Course keeping in waves (D56): how far the boat's heading wanders with the rudder amidships.

  python Tools/boat_course.py [SeaState ...] [--seconds 60] [--yaw 0] [--sail 1] [--pre "sb.Boat.RigTurn 1"]

For each sea state: starts PIE, places the boat in open water (2.5 km north, 800 m west of the cove) heading `--yaw` at 2 m/s, sets the sail
(or not) with the rudder amidships, and logs its heading every 5 s. Prints the heading change and the largest swing
(SB_COURSE lines). The owner's complaint was the boat circling in big waves.
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import run_pie_test as pie  # noqa: E402


def heading_track(sea, seconds, yaw, sail, pre=()):
    pie.stop_pie()
    pie.start_pie()
    offset = pie.log_size()
    cmds = list(pre) + ["sb.SeaState %s 0" % sea, "sb.Boat.Place 250000 -80000 %g 2" % yaw, "sb.Boat.Control 0 %d" % sail]
    for cmd in cmds:
        pie.pie_console(cmd)
    for t in range(0, seconds + 1, 5):
        pie.pie_console("sb.Dev.Delayed %d sb.Boat.Info" % max(t, 1))
    time.sleep(seconds + 4)
    text = pie.read_from(offset)
    yaws = [float(m) for m in re.findall(r"SB_BOAT .*?yaw=(-?[\d.]+)", text)]
    pie.stop_pie()
    return yaws


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seas", nargs="*", default=["Calm", "Rough"])
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--yaw", type=float, default=0.0)
    ap.add_argument("--sail", type=int, default=1)
    ap.add_argument("--pre", action="append", default=[], help="console commands before the run (e.g. sb.Boat.RigTurn 1)")

    ns = ap.parse_args()
    for sea in ns.seas:
        yaws = heading_track(sea, ns.seconds, ns.yaw, ns.sail, ns.pre)
        if not yaws:
            print("SB_COURSE %s no data" % sea)
            continue
        unwrapped = [yaws[0]]
        for y in yaws[1:]:
            d = (y - unwrapped[-1] + 180.0) % 360.0 - 180.0
            unwrapped.append(unwrapped[-1] + d)
        change = unwrapped[-1] - unwrapped[0]
        swing = max(unwrapped) - min(unwrapped)
        print("SB_COURSE %s heading change %.1f deg over %d s, range %.1f deg, track %s" % (
            sea, change, ns.seconds, swing, " ".join("%.0f" % y for y in unwrapped)))


if __name__ == "__main__":
    main()
