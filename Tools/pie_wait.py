"""Waits that end as soon as their condition holds (owner, 2026-10-07: "only wait as long as you need"). Every editor
second a script spends is a second the other sessions wait for the lock, so use these instead of fixed time.sleep:

    wait_log(marker, offset, timeout=60)      the log text from offset once it contains marker (polled every 0.1 s)
    frames()                                  the MovieFrame PNGs in Saved/Screenshots now
    new_frame(before, timeout=20)             the first new MovieFrame PNG, once fully written (its size holds)
    settled_shot(path, max_s=6)               shoots (sb.Dev.DumpFrames 1) until the image has settled (exposure, Lumen,
                                              streaming), keeps the last frame as the shot; replaces "@wait N" + "@shot"

A settled image: its mean brightness changed by less than 1.5 % since the frame before, or it went up and down in the last
two frames (flicker, waves) instead of drifting one way (exposure still adapting). Never earlier than the third frame.
"""
import glob
import os
import shutil
import time

import run_pie_test as R

SHOTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Saltbound", "Saved", "Screenshots")


def wait_log(marker, offset, timeout=60.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        text = R.read_from(offset)
        if marker in text:
            return text
        time.sleep(0.1)
    return None


def frames():
    return set(glob.glob(os.path.join(SHOTS, "**", "MovieFrame*.png"), recursive=True))


def new_frame(before, timeout=20.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        new = frames() - before
        if new:
            path = max(new, key=os.path.getmtime)
            size = -1
            while time.time() - t0 < timeout:
                s = os.path.getsize(path)
                if s > 0 and s == size:
                    return path
                size = s
                time.sleep(0.05)
        time.sleep(0.05)
    return None


def _brightness(path):
    from PIL import Image, ImageStat
    with Image.open(path) as im:
        return ImageStat.Stat(im.convert("L").resize((64, 36))).mean[0]


def settled_shot(path, max_s=6.0, tol=0.015):
    t0 = time.time()
    last, steps, keep = None, [], None
    while True:
        before = frames()
        R.pie_console("sb.Dev.DumpFrames 1")
        f = new_frame(before)
        if f is None:
            print("SB_SHOT_FAIL", path)
            return None
        b = _brightness(f)
        if keep:
            os.remove(keep)
        keep = f
        if last is not None:
            steps.append((b - last) / max(last, 1.0))
        last = b
        settled = len(steps) >= 2 and (abs(steps[-1]) < tol or (steps[-1] * steps[-2] < 0 and abs(steps[-1]) < 4 * tol))
        if settled or time.time() - t0 > max_s:
            break
    shutil.move(keep, path)
    print("SB_SHOT %s (%.1f s, %d frames%s)" % (path, time.time() - t0, len(steps) + 1, "" if settled else ", NOT SETTLED"))
    return path
