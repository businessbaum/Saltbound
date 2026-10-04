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
    """Handle of the launched game's top-level window (PowerShell). Matches the process id, so an open
    editor window ("Saltbound - Unreal Editor") is never picked up."""
    deadline = time.time() + timeout
    ids = ",".join(str(p) for p in pids)
    while time.time() < deadline:
        # The launched process or its children (the packaged Saltbound.exe starts the real game process).
        ps = ("$ids = @(%s) + @(Get-CimInstance Win32_Process | Where-Object { @(%s) -contains $_.ParentProcessId } | "
              "ForEach-Object { $_.ProcessId }); Get-Process -Id $ids -ErrorAction SilentlyContinue | Where-Object { "
              "$_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -notmatch 'Unreal Editor' } | "
              "Select-Object -First 1 -ExpandProperty MainWindowHandle" % (ids, ids))
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
    # Topmost: an open editor can keep the foreground; desktop duplication records whatever is on top.
    user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0040)   # HWND_TOPMOST, NOSIZE|NOMOVE|SHOWWINDOW
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
                    help="ride the waves at this height (cm) above the water instead of the shot's fixed Z "
                         "(0 = use the shot's Z as the height above the water)")
    ap.add_argument("--fov", type=float, default=80.0, help="horizontal field of view (deg)")
    ap.add_argument("--freeze", action="store_true",
                    help="stop the ocean clock at --time (identical stills; the floating camera still settles)")
    ap.add_argument("--dump", action="store_true",
                    help="render-side capture (sb.Dev.DumpFrames) at a fixed time step of 1/--fps: every frame is the "
                         "same ocean time in every run and other windows cannot cover the image; --warmup is game time")
    ap.add_argument("--keep-frames", default=None, help="with --dump: also copy the PNG frames into this folder")
    ns = ap.parse_args()
    if ns.dump:
        return record_dump(ns)
    shot = {n: (loc, rot) for n, loc, rot in capture_shots.SHOTS["visualtest"]}[ns.shot]
    (x, y, z), (pitch, yaw, _roll) = shot
    rx, ry = ns.res.split("x")
    if ns.float_height == 0:
        ns.float_height = z
    # Commands run in order after the world is up; the ocean clock is set last, so every clip starts at
    # the same sea state regardless of loading time.
    cmds = ["r.VSync 0", "t.MaxFPS %d" % ns.fps, "sb.Lighting %s" % ns.preset] + list(ns.exec) + [
        ("sb.Dev.Camera %g %g %g %g %g %g" % (x, y, z, pitch, yaw, ns.fov)) if ns.float_height is None else
        ("sb.Dev.Camera %g %g %g %g %g %g 1" % (x, y, ns.float_height, pitch, yaw, ns.fov)),
    ]
    delayed = ["sb.Dev.Delayed %g sb.Ocean.Time %g" % (ns.warmup - 1.0, ns.time)]
    if ns.freeze:
        delayed.append("sb.Dev.Delayed %g sb.Ocean.Freeze 1" % (ns.warmup - 0.95))
    exec_cmds = ",".join(cmds + delayed)
    if ns.packaged:
        args = [PACKAGED]
    else:
        args = [EDITOR, PROJECT, "/Game/Saltbound/Maps/L_VisualTest", "-game"]
    # Pinned at the top-left corner, no log console (it would cover the frame); the log file is still written.
    args += ["-windowed", "-resx=%s" % rx, "-resy=%s" % ry, "-WinX=0", "-WinY=0", "-nosplash", "-SaltNewGame", '-ExecCmds=%s' % exec_cmds]
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


def shot_commands(ns):
    shot = {n: (loc, rot) for n, loc, rot in capture_shots.SHOTS["visualtest"]}[ns.shot]
    (x, y, z), (pitch, yaw, _roll) = shot
    if ns.float_height == 0:
        ns.float_height = z
    return ["sb.Lighting %s" % ns.preset] + list(ns.exec) + [
        ("sb.Dev.Camera %g %g %g %g %g %g" % (x, y, z, pitch, yaw, ns.fov)) if ns.float_height is None else
        ("sb.Dev.Camera %g %g %g %g %g %g 1" % (x, y, ns.float_height, pitch, yaw, ns.fov))]


def record_dump(ns):
    """Fixed-time-step render capture: the game advances exactly 1/fps per frame, the renderer writes each
    frame as a PNG (MovieFrame*), ffmpeg assembles them. Deterministic and independent of the desktop."""
    import glob
    shots_dir = os.path.join(ROOT, "Saltbound", "Saved", "Screenshots")
    for f in glob.glob(os.path.join(shots_dir, "**", "MovieFrame*"), recursive=True):
        os.remove(f)
    rx, ry = ns.res.split("x")
    frames = max(1, int(round(ns.seconds * ns.fps)))
    w = ns.warmup
    delayed = ["sb.Dev.Delayed %g sb.Ocean.Time %g" % (w - 1.0, ns.time)]
    if ns.freeze:
        delayed.append("sb.Dev.Delayed %g sb.Ocean.Freeze 1" % (w - 0.95))
    delayed += ["sb.Dev.Delayed %g sb.Dev.DumpFrames %d" % (w, frames),
                "sb.Dev.Delayed %g quit" % (w + ns.seconds + 0.5)]
    exec_cmds = ",".join(["r.VSync 0"] + shot_commands(ns) + delayed)
    args = ([PACKAGED] if ns.packaged else [EDITOR, PROJECT, "/Game/Saltbound/Maps/L_VisualTest", "-game"]) + [
        "-windowed", "-resx=%s" % rx, "-resy=%s" % ry, "-WinX=0", "-WinY=0", "-nosplash",
        "-UseFixedTimeStep", "-FPS=%d" % ns.fps, "-SaltNewGame", "-ExecCmds=%s" % exec_cmds]
    proc = subprocess.Popen(args)
    try:
        proc.wait(timeout=900)
    except subprocess.TimeoutExpired:
        proc.kill()
        raise SystemExit("game did not quit")
    files = sorted(glob.glob(os.path.join(shots_dir, "**", "MovieFrame*.png"), recursive=True))
    if not files:
        raise SystemExit("no frames dumped")
    tmp = os.path.join(os.path.dirname(os.path.abspath(ns.out)), "_frames_%d" % os.getpid())
    os.makedirs(tmp, exist_ok=True)
    for i, f in enumerate(files):
        shutil.move(f, os.path.join(tmp, "%05d.png" % i))
    subprocess.run([shutil.which("ffmpeg"), "-y", "-loglevel", "error", "-framerate", str(ns.fps), "-i",
                    os.path.join(tmp, "%05d.png"), "-c:v", "libx264", "-preset", "veryfast", "-crf", "16",
                    "-pix_fmt", "yuv420p", ns.out], check=True)
    if ns.keep_frames:
        os.makedirs(ns.keep_frames, exist_ok=True)
        for f in sorted(os.listdir(tmp)):
            shutil.move(os.path.join(tmp, f), os.path.join(ns.keep_frames, f))
    shutil.rmtree(tmp, ignore_errors=True)
    print("SB_CLIP", ns.out, "frames", len(files))


if __name__ == "__main__":
    main()
