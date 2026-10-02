"""Minimal client for the Unreal Editor MCP server (ModelContextProtocol plugin, http://127.0.0.1:8000/mcp).

Usage:
  python Tools/ue_mcp.py toolsets
  python Tools/ue_mcp.py describe EditorToolset.EditorAppToolset
  python Tools/ue_mcp.py call EditorToolset.EditorAppToolset CaptureViewport '{"bShowUI": false}'
  python Tools/ue_mcp.py capture out.png [x y z pitch yaw roll]
"""
import base64, json, os, sys, urllib.error, urllib.request

URL = os.environ.get("SALTBOUND_MCP_URL", "http://127.0.0.1:8000/mcp")
_sid = None


def _post(payload, sid=None, timeout=600):
    req = urllib.request.Request(URL, data=json.dumps(payload).encode(), method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Accept", "application/json, text/event-stream")
    if sid:
        req.add_header("Mcp-Session-Id", sid)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        body = r.read().decode("utf-8", "replace")
        new_sid = r.headers.get("Mcp-Session-Id")
        ctype = r.headers.get("Content-Type", "")
    if "event-stream" in ctype:
        msgs = [json.loads(l[5:].strip()) for l in body.splitlines() if l.startswith("data:")]
        return (msgs[-1] if msgs else None), new_sid
    return (json.loads(body) if body.strip() else None), new_sid


def _session():
    global _sid
    if _sid is None:
        _, _sid = _post({"jsonrpc": "2.0", "id": 0, "method": "initialize",
                         "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                                    "clientInfo": {"name": "saltbound-tools", "version": "1"}}})
        _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, _sid)
    return _sid


def rpc(method, params=None):
    res, _ = _post({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}, _session())
    if res and "error" in res:
        raise RuntimeError(res["error"])
    return res["result"]


def call_tool(toolset, tool, args=None):
    params = {"tool_name": tool, "arguments": args or {}}
    if toolset:
        params["toolset_name"] = toolset
    return rpc("tools/call", {"name": "call_tool", "arguments": params})


def text_of(result):
    return "\n".join(c.get("text", "") for c in result.get("content", []) if c.get("type") == "text")


def capture(path, pose=None, show_ui=False):
    if pose:
        x, y, z, p, yw, r = pose
        xf = {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": p, "yaw": yw, "roll": r},
              "scale": {"x": 1, "y": 1, "z": 1}}
    else:
        xf = json.loads(text_of(call_tool("EditorToolset.EditorAppToolset", "GetCameraTransform")))["returnValue"]
    args = {"bShowUI": show_ui, "captureTransform": xf,
            "annotations": {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0, "maxLabelDistance": 0,
                            "classFilter": {"refPath": ""}, "maxLabels": 0}}
    res = call_tool("EditorToolset.EditorAppToolset", "CaptureViewport", args)
    data = None
    for c in res.get("content", []):
        if c.get("type") == "image":
            data = c["data"]
    if data is None:
        sc = res.get("structuredContent") or {}
        rv = sc.get("returnValue", sc)
        data = (rv.get("image") or {}).get("data")
        if data is None:
            try:
                rv = json.loads(text_of(res)).get("returnValue", {})
                data = rv.get("image", {}).get("data")
            except Exception:
                pass
    if not data:
        raise RuntimeError("No image in capture result: " + json.dumps(res)[:1000])
    with open(path, "wb") as f:
        f.write(base64.b64decode(data))
    return path


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "toolsets":
        print(text_of(rpc("tools/call", {"name": "list_toolsets", "arguments": {}})))
    elif cmd == "describe":
        print(text_of(rpc("tools/call", {"name": "describe_toolset", "arguments": {"toolset_name": sys.argv[2]}})))
    elif cmd == "call":
        args = json.loads(sys.argv[4]) if len(sys.argv) > 4 else {}
        res = call_tool(sys.argv[2], sys.argv[3], args)
        out = json.dumps(res, indent=1, ensure_ascii=False)
        print(out[:int(os.environ.get("MAXOUT", "20000"))])
    elif cmd == "capture":
        pose = [float(v) for v in sys.argv[3:9]] if len(sys.argv) >= 9 else None
        print(capture(sys.argv[2], pose))
