"""Phase 1 checks of the hub ship's blockout (D61), run inside the editor on L_HubBlockout (driven by Tools/hub_blockout_checks.py).

  headroom()         editor world: on every open floor (0.5 m grid) the clear height up to the next solid; logs SB_HEADROOM
  walk(route)        PIE: steers the player along a route (simulated input, the control yaw towards each waypoint), logs
                     SB_WALK lines (time, length, stuck) and SB_WALK DONE
  water(seconds)     PIE: the sea's height (the CPU's long waves) under the hull and in the harbour; logs SB_HUBWATER
  (D70: the D61 stern dock's boat run is gone; the boat's runs through the side doors are sb.Dev.RunHubTests boatdock)
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


def _to_chandlery(m):
    """D69: from the Well's last flight forward along the waist (between the cargo hatches and the starboard boat) to the
    Chandlery's door in the forecastle's front, in and to its counter."""
    import hub_plan as PL
    door = next(d for d in PL.plan("main").doors if d["kind"] == "link" and {d["a"][1], d["b"][1]} == {"deck", "chandlery"})
    hatch_y = max(b[3] for b in H.prop("main_deck", "cargo_hatches")["boxes"])
    boat_y = min(b[2] for b in H.prop("main_deck", "ships_boats")["boxes"] if b[2] > 0)
    lane = 0.5 * (hatch_y + boat_y)
    return [(112.0, lane, m), (door["x"] - 1.5, door["y"], m), (door["x"] + 1.2, door["y"], m), _e("chandlery", "counter") + (m,)]


def routes():
    """The walks from the fitting berth's board (D70: on the starboard quay at the aft docking station) to each service: in at
    the Slip's open front to its counter; up the dock stair along the shop's aft wall to the Tally House's pay hatch; along the
    covered quay (between the pillars and the shipwright's work) to the Well's foot, up its five flights to the main deck and
    forward to the Chandlery. Corners in the open floor, clear of the props and the stairs' rails."""
    q, lw, md, up, gn, m = Z["quay"], Z["lower"], Z["middle"], Z["upper"], Z["gun"], Z["main"]
    berth = _e("slip", "fitting_berth") + (q,)
    st = H.R(H.HUB["stair"])
    sx = 0.5 * (st[0] + st[1])
    sh = H.R(H.HUB["shaft"])
    pb = H.prop("tally", "pay_booth")
    slip = [berth, (st[1] + 0.9, st[2] - 1.2, q), (st[1] + 0.9, st[2] + 1.0, q), _e("slip", "counter") + (q,)]
    to_pay = [berth, (sx, st[2] - 1.2, q), (sx, st[2] - 0.6, q), (sx, st[3] + 0.7, lw), (st[1] + 1.0, st[3] + 1.1, lw),
              (pb["x0"] - 0.6, pb["y0"] - 1.1, lw), _e("tally", "pay_hatch") + (lw,)]
    pil = H.prop("harbour", "pillars")
    qw = H.prop("slip", "quay_work")
    walk_y = 0.5 * (pil["at"][0][1] + pil["size"] * 0.5 + qw["y0"])               # the covered quay's free strip
    f_ql = _flight("quay", "lower")
    foot = f_ql[0]
    quay = [berth, (st[1] + 2.5, walk_y, q), (sh[0] - 7.0, walk_y, q), (foot[0] + 1.2, walk_y - 0.2, q), (foot[0] + 1.2, foot[1], q)] + f_ql
    f_lm, f_mu, f_ug, f_gm = _flight("lower", "middle"), _flight("middle", "upper"), _flight("upper", "gun"), _flight("gun", "main")
    chand = quay + [(f_ql[1][0] - 0.8, sh[3] + 1.0, lw), (f_lm[0][0] - 2.5, sh[3] + 1.0, lw), (f_lm[0][0], f_lm[0][1] + 3.0, lw)] + f_lm + \
        [(f_mu[0][0] + 3.0, f_mu[0][1] + 1.5, md)] + f_mu + [(f_ug[0][0] + 1.0, f_mu[1][1] + 2.5, up)] + f_ug + \
        [(f_gm[0][0] - 3.0, f_gm[0][1] - 1.5, gn)] + f_gm + \
        [(f_gm[1][0] + 5.4, sh[3] - 2.0, m)] + _to_chandlery(m)
    return {"slip": slip, "tally": to_pay, "chandlery": chand}


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


# --- the sea --------------------------------------------------------------------------------------------------------------
WATER = {}


def water(seconds=60.0):
    pts = []
    for x in range(12, 152, 14):
        for fy in (-0.6, 0.0, 0.6):
            pts.append(("hull", x, fy * H.half_width(x, 0.0)))
    for x0, x1, _, _ in H.basins():                                    # D70: the harbour's basins and canal
        for x in (x0 + 1.5, 0.5 * (x0 + x1), x1 - 1.5):
            for y in (-12.0, 0.0, 12.0):
                pts.append(("basin", x, y))
    for x in range(16, 148, 12):
        pts.append(("basin", float(x), 0.0))
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
