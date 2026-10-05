"""Swimming check in PIE (D28, D48): enter the sea beside the pier and settle, dive, hover at depth without input
(no drifting up), rise and climb out onto the pier, walk off the pier end into the sea.

Usage (editor running, L_VisualTest open):  python Tools/swim_test.py [--keep-pie]
Prints the SB_WATER state after each step and PASS / FAIL per expectation; exit code 0 when all pass.
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import run_pie_test as pie  # noqa: E402

STATE = re.compile(r"SB_WATER mode=(\S+) inWater=(\d) immersion=([\d.]+) surface=(-?[\d.]+) feet=\((-?[\d.]+), (-?[\d.]+), (-?[\d.]+)\)"
                   r" speed=(-?[\d.]+) velZ=(-?[\d.]+) cam=(-?[\d.]+) camAboveWater=(-?[\d.]+)")


def state():
    offset = pie.log_size()
    pie.pie_console("sb.Dev.WaterState")
    for _ in range(20):
        time.sleep(0.25)
        m = STATE.search(pie.read_from(offset))
        if m:
            g = m.groups()
            return {"mode": g[0], "in_water": g[1] == "1", "immersion": float(g[2]), "surface": float(g[3]),
                    "feet": (float(g[4]), float(g[5]), float(g[6])), "vz": float(g[8]), "cam_above": float(g[10])}
    raise RuntimeError("no SB_WATER line")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep-pie", action="store_true")
    ns = ap.parse_args()
    results = []

    def check(name, ok, s):
        results.append(ok)
        print("%s %s  mode=%s immersion=%.2f feet=(%.0f, %.0f, %.0f) surface=%.0f camAboveWater=%.0f" % (
            "PASS" if ok else "FAIL", name, s["mode"], s["immersion"], s["feet"][0], s["feet"][1], s["feet"][2], s["surface"], s["cam_above"]))

    pie.start_pie(warmup=3.0)
    try:
        time.sleep(2.0)
        # 1. Drop into the water beside the pier (feet 60 cm below the still water level, sand ~2.3 m down).
        pie.pie_console("sb.Dev.Teleport 440 190 -60 -90 0")
        time.sleep(5.0)
        s = state()
        check("enter_and_settle", s["mode"] == "Swimming" and abs(s["vz"]) < 15.0, s)
        # 2. Dive for 1.5 s: fully submerged, camera under water.
        pie.pie_console("sb.Dev.Input 0 0 0 1")
        time.sleep(1.5)
        s = state()
        check("dive", s["mode"] == "Swimming" and s["immersion"] > 0.99 and s["cam_above"] < -20.0, s)
        # 3. Release: the swimmer hovers where they stopped (neutral under water, D48): no rising, no sinking.
        pie.pie_console("sb.Dev.Input 0 0 0 0")
        time.sleep(1.5)
        s0 = state()
        time.sleep(3.0)
        s = state()
        drift = s["feet"][2] - s0["feet"][2]
        print("   hover drift over 3 s: %.1f cm" % drift)
        check("hover_at_depth", s["mode"] == "Swimming" and s["immersion"] > 0.99 and abs(drift) < 10.0, s)
        # 4. Hold Up facing the pier: rise to the surface, but no climbing out on to the pier (D59, owner: "remove the
        #    climbing system for objects when you run against them"; out of the sea by the beach or the boat's ladders).
        pie.pie_console("sb.Dev.Input 0 0 1 0")
        time.sleep(3.0)
        pie.pie_console("sb.Dev.Input 0 0 0 0")
        time.sleep(1.0)
        s = state()
        check("no_climb_out", s["mode"] == "Swimming" and s["in_water"] and s["feet"][2] < 95.0, s)
        # 5. Walk off the pier end into the sea again.
        pie.pie_console("sb.Dev.Teleport 1350 0 112 0 0")
        pie.pie_console("sb.Dev.Input 1 0 0 0")
        time.sleep(2.5)
        pie.pie_console("sb.Dev.Input 0 0 0 0")
        time.sleep(3.0)
        s = state()
        check("walk_off_into_sea", s["mode"] == "Swimming", s)
    finally:
        if not ns.keep_pie:
            pie.stop_pie()
    print("RESULT: %s (%d/%d)" % ("PASS" if all(results) else "FAIL", sum(results), len(results)))
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
