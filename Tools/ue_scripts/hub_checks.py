"""Phase 1 checks of the hub ship's blockout (D61), run inside the editor on L_HubBlockout (driven by Tools/hub_blockout_checks.py).

  headroom()         editor world: on every open floor (0.5 m grid) the clear height up to the next solid; logs SB_HEADROOM
  walk(route)        PIE: steers the player along a route (simulated input, the control yaw towards each waypoint), logs
                     SB_WALK lines (time, length, stuck) and SB_WALK DONE
  boat(run)          PIE: the boat sails into the dock under the arch, turns on the spot in the basin, sails out (a simple
                     autopilot on the rudder; the sail set or not); logs SB_HUBBOAT lines and SB_HUBBOAT DONE
  water(seconds)     PIE: the sea's height (the CPU's long waves) under the hull and in the basin; logs SB_HUBWATER
Every place comes from HubShip_Data.json (hub_layout.py); the routes' corners are chosen in the open floor between props.
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "Art", "Blender", "Scripts"))
import hub_layout as H  # noqa: E402
import importlib  # noqa: E402
importlib.reload(H)                # the editor keeps modules: always read the current data

Z = H.Z


def _log(msg):
    unreal.log("SB_" + msg)


def _game():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def _editor():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _cmd(c):
    unreal.SystemLibrary.execute_console_command(_game(), c)


def to_ship(loc):
    """World cm -> ship metres (x, y, z)."""
    (bx, by, bz), yaw, _ = H.berth_world()
    dx, dy = loc.x - bx, loc.y - by
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    lx, ly = dx * c + dy * s, -dx * s + dy * c
    return lx / 100.0 + H.PIVOT_X, ly / 100.0, (loc.z - bz) / 100.0


def yaw_world(ship_heading_deg):
    return H.berth_world()[1] + ship_heading_deg


# --- routes ------------------------------------------------------------------------------------------------------------
def _e(space, eid):
    e = next(e for e in H.space(space)["e"] if e["id"] == eid)
    return e["x"], e["y"]


def _flight(frm, to, down=False):
    """The flight's two ends (0.6 m beyond its rectangle), bottom first unless down: [(x, y, floor z), ...]."""
    f = next(f for f in H.flights() if f["from"] == frm and f["to"] == to)
    xm, ym = 0.5 * (f["x0"] + f["x1"]), 0.5 * (f["y0"] + f["y1"])
    lo, hi = Z[frm], Z[to]
    ends = {"-x": ((f["x1"] + 0.6, ym, lo), (f["x0"] - 0.6, ym, hi)), "+x": ((f["x0"] - 0.6, ym, lo), (f["x1"] + 0.6, ym, hi)),
            "-y": ((xm, f["y1"] + 0.6, lo), (xm, f["y0"] - 0.6, hi)), "+y": ((xm, f["y0"] - 0.6, lo), (xm, f["y1"] + 0.6, hi))}[f["up"]]
    return list(reversed(ends)) if down else list(ends)


def routes():
    q, lw, m = Z["quay"], Z["lower"], Z["main"]
    berth = _e("slip", "fitting_berth") + (q,)
    land = next(r for zid, r in H.open_rects("lower") if zid == "tally" and r[1] - r[0] < 5.0)
    st = H.R(H.HUB["stair"])
    sy = 0.5 * (st[2] + st[3])
    foot = land[0] - 22 * 0.30
    to_tally = [berth, (foot - 2.4, H.HUB["quayY"] - 1.5, q), (foot - 1.6, sy, q), (foot - 0.4, sy, q), (land[0] + 0.6, sy, lw),
                (land[1] + 1.5, sy, lw)]
    tally = to_tally + [_e("tally", "pay_hatch") + (lw,)]
    lane = H.R(H.HUB["lane"])
    to_well = to_tally + [(lane[0] - 5.0, sy, lw), (lane[0] - 3.0, 0.5 * (lane[2] + lane[3]) + 1.5, lw),       # round the table (lane[0] + 0.8, 0.5 * (lane[2] + lane[3]) + 1.0, lw),
                          (lane[1] - 0.5, 0.5 * (lane[2] + lane[3]) + 0.5, lw)]
    down = _flight("quay", "lower", down=True)
    keel = to_well + [(down[0][0] - 0.2, down[0][1], lw)] + down + [(down[1][0] + 1.0, 6.8, q), (82.0, 6.6, q), (81.6, 2.4, q), (77.0, -1.0, q), _e("keelgate", "talk") + (q,)]
    sh = H.R(H.HUB["shaft"])
    f_lm, f_mu, f_um = _flight("lower", "middle"), _flight("middle", "upper"), _flight("upper", "main")
    chand = to_well + [(sh[0] - 4.0, sh[3] + 1.0, lw), (f_lm[0][0], sh[3] + 1.0, lw)] + f_lm + \
        [(f_mu[0][0] + 1.5, f_mu[0][1], Z["middle"])] + f_mu + [(f_um[0][0] + 1.5, f_mu[1][1], Z["upper"]), (f_um[0][0] + 1.5, f_um[0][1] - 1.0, Z["upper"])] + \
        [f_um[0], f_um[1]] + [(sh[0] - 1.0, sh[3] + 2.5, m), (sh[1] + 2.0, sh[3] + 1.5, m), (sh[1] + 6.0, -5.0, m), (sh[1] + 10.5, -5.0, m), _e("chandlery", "counter") + (m,)]
    slip = [berth, (H.HUB["basin"]["x1"] + 0.5, H.HUB["quayY"] - 2.0, q), _e("slip", "counter") + (q,)]
    return {"slip": slip, "tally": tally, "keelgate": keel, "chandlery": chand}


def _safe(state, fn):
    """A tick callback that unregisters itself when it fails (an error every frame would flood the log)."""
    def tick(dt):
        try:
            fn(dt)
        except Exception as exc:  # noqa: BLE001
            unreal.unregister_slate_post_tick_callback(state["handle"])
            _log("CHECK FAILED %s: %s" % (fn.__name__, exc))
    return tick


# --- walking -----------------------------------------------------------------------------------------------------------
WALK = {}


def walk(name, sprint=False):
    pts = routes()[name]
    x0, y0, z0 = pts[0]
    wx, wy, wz = H.ship_to_world(x0, y0, z0 + 0.05)
    head = math.degrees(math.atan2(pts[1][1] - y0, pts[1][0] - x0))
    _cmd("sb.Dev.Teleport %.1f %.1f %.1f %.1f 0" % (wx, wy, wz, yaw_world(head)))
    length = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts[:-1], pts[1:]))
    WALK.clear()
    WALK.update(name=name, pts=pts, i=1, t=0.0, started=False, best=1e9, best_t=0.0, length=length, sprint=sprint)
    WALK["handle"] = unreal.register_slate_post_tick_callback(_safe(WALK, _walk_tick))
    _log("WALK START %s points=%d length=%.1fm" % (name, len(pts), length))


def _walk_end(ok, why=""):
    _cmd("sb.Dev.Input 0 0")
    unreal.unregister_slate_post_tick_callback(WALK["handle"])
    t = WALK["t"] - WALK.get("t0", 0.0)
    _log("WALK DONE %s ok=%d time=%.1fs length=%.1fm %s" % (WALK["name"], 1 if ok else 0, t, WALK["length"], why))


def _walk_tick(dt):
    s = WALK
    s["t"] += dt
    w = _game()
    if w is None:
        _walk_end(False, "no game world")
        return
    if s["t"] < 0.8:
        return
    if not s["started"]:
        _cmd("sb.Dev.Input 1 0")
        s["started"] = True
        s["t0"] = s["t"]
        s["best_t"] = s["t"]
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    loc = pawn.get_actor_location()
    feet = to_ship(loc)
    feet = (feet[0], feet[1], feet[2] - 0.7)
    tx, ty, tz = s["pts"][s["i"]]
    d = math.hypot(tx - feet[0], ty - feet[1])
    if d < 0.55 and abs(feet[2] - tz) < 1.2:
        _log("WALK AT %s %d (%.1f, %.1f, %.1f) after %.1fs" % (s["name"], s["i"], feet[0], feet[1], feet[2], s["t"] - s["t0"]))
        s["i"] += 1
        s["best"] = 1e9
        if s["i"] >= len(s["pts"]):
            _walk_end(True)
            return
        tx, ty, tz = s["pts"][s["i"]]
        d = math.hypot(tx - feet[0], ty - feet[1])
    if d < s["best"] - 0.25:
        s["best"], s["best_t"] = d, s["t"]
    elif s["t"] - s["best_t"] > 3.0:
        _walk_end(False, "STUCK going to %d (%.1f, %.1f, %.1f) at (%.1f, %.1f, %.1f)" % (s["i"], tx, ty, tz, feet[0], feet[1], feet[2]))
        return
    if s["t"] - s["t0"] > 150.0:
        _walk_end(False, "timeout")
        return
    head = math.degrees(math.atan2(ty - feet[1], tx - feet[0]))
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw_world(head)))


# --- headroom -----------------------------------------------------------------------------------------------------------
def headroom(step=0.5):
    w = _editor()
    # Ceilings only: props, gates, masts, lanterns, labels and the boat are not what the head meets walking.
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    skip = [a for a in eas.get_all_level_actors() if a.get_actor_label() in ("Hub_Props", "Hub_Gates", "Hub_Masts", "Hub_Markers")
            or isinstance(a, (unreal.SaltLantern, unreal.TextRenderActor, unreal.SaltBoat))]
    areas = []
    for lvl in ("quay", "lower", "middle", "upper", "main"):
        for zid, r in H.open_rects(lvl):
            areas.append((lvl, zid, r))
    for o in H.OPEN:
        if o.get("inside"):
            areas += [(o["level"], o["id"], H.R(r)) for r in o["rects"]]
    worst = {}
    low = []
    n = 0
    for lvl, zid, (x0, x1, y0, y1) in areas:
        zf = H.level_z(lvl)
        x = x0 + step / 2
        while x < x1:
            y = y0 + step / 2
            while y < y1:
                top = H.ship_to_world(x, y, zf + 1.0)
                bot = H.ship_to_world(x, y, zf - 1.0)
                hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(*top), unreal.Vector(*bot), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                             True, skip, unreal.DrawDebugTrace.NONE, True)
                if hit is not None:
                    fz = hit.to_tuple()[4].z
                    if abs(fz - H.ship_to_world(x, y, zf)[2]) < 30.0:            # the floor itself (not a prop's top or a step)
                        start = unreal.Vector(top[0], top[1], fz + 5.0)
                        end = unreal.Vector(top[0], top[1], fz + 1500.0)
                        up = unreal.SystemLibrary.line_trace_single(w, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, skip,
                                                                    unreal.DrawDebugTrace.NONE, True)
                        clear = ((up.to_tuple()[4].z if up is not None else 1e9) - fz) / 100.0
                        n += 1
                        key = "%s/%s" % (lvl, zid)
                        if key not in worst or clear < worst[key][0]:
                            worst[key] = (clear, x, y)
                        if clear < 2.2:
                            low.append((lvl, zid, round(x, 2), round(y, 2), round(clear, 2), up.to_tuple()[9].get_actor_label() if up else "-"))
                y += step
            x += step
    for key, (clear, x, y) in sorted(worst.items()):
        _log("HEADROOM %s min=%.2fm at (%.1f, %.1f)" % (key, clear, x, y))
    for item in low[:60]:
        _log("HEADROOM_LOW %s" % (item,))
    _log("HEADROOM DONE samples=%d under_2.2m=%d" % (n, len(low)))


# --- the boat -------------------------------------------------------------------------------------------------------------
BOAT = {}


def _boat():
    return unreal.GameplayStatics.get_all_actors_of_class(_game(), unreal.SaltBoat)[0]


def boat(run="in"):
    """in: from 40 m astern, heading in under the arch, sail set; cut at the dock's middle, steered along the centreline.
    turn: at the basin's middle, at rest, the oar hard over (sculling) for 40 s.  out: from the basin's middle heading out."""
    b = H.HUB["basin"]
    mid_x = 0.5 * (b["x0"] + b["x1"])
    if run == "in":
        x, head, speed = -40.0, 0.0, 2.5
    elif run == "turn":
        x, head, speed = mid_x, 0.0, 0.0
    else:
        x, head, speed = mid_x, 180.0, 0.5
    wx, wy, _ = H.ship_to_world(x, 0.0, 0.0)
    _cmd("sb.Wind 200 7")
    _cmd("sb.Boat.Place %.0f %.0f %.1f %.1f" % (wx, wy, yaw_world(head), speed))
    _cmd("sb.Boat.Control 0 %d" % (0 if run == "turn" else 1))
    BOAT.clear()
    BOAT.update(run=run, t=0.0, head=head, last=-1.0, max_off=0.0, min_gap=99.0, contacts=0, yaw0=None, turned=0.0, prev_yaw=None,
                sail=run != "turn", cut_x=mid_x - 4.0)
    BOAT["handle"] = unreal.register_slate_post_tick_callback(_safe(BOAT, _boat_tick))
    _log("HUBBOAT START %s" % run)


def _boat_tick(dt):
    s = BOAT
    s["t"] += dt
    bt = _boat()
    loc = bt.get_actor_location()
    x, y, z = to_ship(loc)
    yaw = bt.get_actor_rotation().yaw - H.berth_world()[1]
    yaw = (yaw + 180.0) % 360.0 - 180.0
    info_contacts = 0
    half = H.HUB["basin"]["y1"]
    beam = 1.7                                                         # the first boat's half-beam at the sheer (D36)
    inside = H.HUB["basin"]["x0"] - 2.0 < x < H.HUB["basin"]["x1"]
    if inside:
        s["min_gap"] = min(s["min_gap"], half - abs(y) - beam)
    if s["run"] == "turn":
        _cmd("sb.Boat.Control 1 0")
        if s["prev_yaw"] is not None:
            s["turned"] += ((yaw - s["prev_yaw"] + 180.0) % 360.0) - 180.0
        s["prev_yaw"] = yaw
        s["max_off"] = max(s["max_off"], math.hypot(x - 22.0, y))
        if abs(s["turned"]) >= 180.0 and "t180" not in s:
            s["t180"] = s["t"]
        done = s["t"] > 40.0 or abs(s["turned"]) >= 360.0
    else:
        # Steer along the centreline: heading error and the offset from it, the oar's sign + = to starboard.
        want = s["head"] + (-12.0 * y if s["head"] == 0.0 else 12.0 * y)
        err = ((want - yaw + 180.0) % 360.0) - 180.0
        rudder = max(-1.0, min(1.0, err / 25.0))
        sail = 1 if s["sail"] else 0
        if s["run"] == "in" and x > s["cut_x"]:
            s["sail"] = False
            sail = 0
        _cmd("sb.Boat.Control %.2f %d" % (rudder, sail))
        s["max_off"] = max(s["max_off"], abs(y)) if inside else s["max_off"]
        done = s["t"] > 45.0 or (s["run"] == "out" and x < -30.0)
    if s["t"] - s["last"] >= 1.0:
        s["last"] = s["t"]
        _cmd("sb.Boat.Info")
        _log("HUBBOAT %s t=%.1f ship=(%.1f, %.1f) heading=%.1f" % (s["run"], s["t"], x, y, yaw))
    if done:
        _cmd("sb.Boat.Control off")
        unreal.unregister_slate_post_tick_callback(s["handle"])
        _log("HUBBOAT DONE %s t=%.1f end=(%.1f, %.1f) heading=%.1f max_offset=%.2fm min_gap_to_quay=%.2fm turned=%.0fdeg t180=%s" % (
            s["run"], s["t"], x, y, yaw, s["max_off"], s["min_gap"], s["turned"], ("%.1f" % s["t180"]) if "t180" in s else "-"))


# --- the sea --------------------------------------------------------------------------------------------------------------
WATER = {}


def water(seconds=60.0):
    pts = []
    for x in range(12, 152, 14):
        for fy in (-0.6, 0.0, 0.6):
            pts.append(("hull", x, fy * H.half_width(x, 0.0)))
    b = H.HUB["basin"]
    for x in (4.0, 12.0, 20.0, 28.0, 36.0):
        for y in (-6.0, 0.0, 6.0):
            pts.append(("basin", x, y))
    WATER.clear()
    WATER.update(t=0.0, seconds=seconds, pts=pts, max={"hull": -99.0, "basin": -99.0}, at={})
    WATER["handle"] = unreal.register_slate_post_tick_callback(_safe(WATER, _water_tick))
    _log("HUBWATER START points=%d" % len(pts))


def _water_tick(dt):
    s = WATER
    s["t"] += dt
    ocean = unreal.GameplayStatics.get_all_actors_of_class(_game(), unreal.SaltOcean)[0]
    for kind, x, y in s["pts"]:
        wx, wy, _ = H.ship_to_world(x, y, 0.0)
        h = ocean.sample_surface(unreal.Vector2D(wx, wy)).location.z / 100.0
        if h > s["max"][kind]:
            s["max"][kind] = h
            s["at"][kind] = (x, round(y, 1))
    if s["t"] >= s["seconds"]:
        unreal.unregister_slate_post_tick_callback(s["handle"])
        _log("HUBWATER DONE %.0fs hull_max=%.2fm at %s basin_max=%.2fm at %s" % (s["t"], s["max"]["hull"], s["at"].get("hull"),
                                                                                s["max"]["basin"], s["at"].get("basin")))
