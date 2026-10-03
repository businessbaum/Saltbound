"""Record a short video from a fixed camera with the real game renderer (no editor viewport effects).

Launches the uncooked game (UnrealEditor.exe <project> -game) or the packaged build, places the
development camera on a named shot (capture_shots.py), sets the ocean time, waits for the scene to
settle and records the window with ffmpeg (gdigrab). The editor should be closed (it competes for the GPU).

  python Tools/record_clip.py out.mp4 --shot 15_ref_low [--seconds 8] [--time 100] [--preset Day]
         [--exec "sb.Region ShoresOfPlenty" ...] [--packaged] [--res 1920x1080] [--fps 60]
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import capture_shots  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PROJECT = os.path.join(ROOT, "Saltbound", "Saltbound.uproject")
ENGINE = os.environ.get("SALTBOUND_ENGINE", r"C:\Program Files\Epic Games\UE_5.8")
EDITOR = os.path.join(ENGINE, r"Engine\Binaries\Win64\UnrealEditor.exe")
PACKAGED = os.path.join(ROOT, r"Builds\Development\Windows\Saltbound.exe")


def find_window_handle(pids, timeout=120):
    """Handle of the game's top-level window (PowerShell)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        ps = ("Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -match 'Saltbound' } | "
              "Select-Object -First 1 -ExpandProperty MainWindowHandle")
        out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout.strip()
        if out:
            return out
        time.sleep(2)
    raise SystemExit("game window not found")


def client_rect_on_screen(hwnd):
    """Brings the window to the front and returns its client area (x, y, w, h) in screen pixels."""
    import ctypes
    from ctypes import wintypes
    user32 = ctypes.windll.user32
    user32.SetProcessDPIAware()
    user32.ShowWindow(hwnd, 9)          # SW_RESTORE
    user32.SetForegroundWindow(hwnd)
    rect = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    pt = wintypes.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    return pt.x, pt.y, w - (w % 2), h - (h % 2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--shot", required=True)
    ap.add_argument("--seconds", type=float, default=8.0)
    ap.add_argument("--time", type=float, default=100.0)
    ap.add_argument("--preset", default="Day")
    ap.add_argument("--exec", nargs="*", default=[])
    ap.add_argument("--packaged", action="store_true")
    ap.add_argument("--res", default="1920x1080")
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--warmup", type=float, default=14.0, help="seconds after launch before recording")
    ap.add_argument("--float", dest="float_height", type=float, default=None,
                    help="ride the waves at this height (cm) above the water instead of the shot's fixed Z")
    ns = ap.parse_args()
    shot = {n: (loc, rot) for n, loc, rot in capture_shots.SHOTS["visualtest"]}[ns.shot]
    (x, y, z), (pitch, yaw, _roll) = shot
    rx, ry = ns.res.split("x")
    # Commands run in order after the world is up; the ocean clock is set last, so every clip starts at
    # the same sea state regardless of loading time.
    cmds = ["r.VSync 0", "t.MaxFPS %d" % ns.fps, "sb.Lighting %s" % ns.preset] + list(ns.exec) + [
        ("sb.Dev.Camera %g %g %g %g %g 80" % (x, y, z, pitch, yaw)) if ns.float_height is None else
        ("sb.Dev.Camera %g %g %g %g %g 80 1" % (x, y, ns.float_height, pitch, yaw)),
    ]
    delayed = ["sb.Dev.Delayed %g sb.Ocean.Time %g" % (ns.warmup - 1.0, ns.time)]
    exec_cmds = ",".join(cmds + delayed)
    if ns.packaged:
        args = [PACKAGED]
    else:
        args = [EDITOR, PROJECT, "/Game/Saltbound/Maps/L_VisualTest", "-game"]
    args += ["-windowed", "-resx=%s" % rx, "-resy=%s" % ry, "-nosplash", "-log", '-ExecCmds=%s' % exec_cmds]
    proc = subprocess.Popen(args)
    try:
        hwnd = int(find_window_handle([proc.pid]))
        time.sleep(max(0.0, ns.warmup - 2.0))
        ffmpeg = shutil.which("ffmpeg")
        if not ffmpeg:
            raise SystemExit("ffmpeg not found")
        x, y, w, h = client_rect_on_screen(hwnd)
        time.sleep(1.0)
        # Desktop Duplication (ddagrab) reads the composed GPU output; gdigrab returns stale frames for
        # DirectX flip-model windows.
        src = "ddagrab=output_idx=0:framerate=%d:draw_mouse=0:offset_x=%d:offset_y=%d:video_size=%dx%d" % (ns.fps, x, y, w, h)
        rec = [ffmpeg, "-y", "-loglevel", "error", "-f", "lavfi", "-i", src, "-t", str(ns.seconds),
               "-vf", "hwdownload,format=bgra", "-c:v", "libx264", "-preset", "veryfast",
               "-crf", "16", "-pix_fmt", "yuv420p", ns.out]
        subprocess.run(rec, check=True)
        print("SB_CLIP", ns.out)
    finally:
        proc.terminate()
        try:
            proc.wait(20)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    main()
