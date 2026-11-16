"""Copper objects of the routed board, vectorised distance helpers (numpy)."""
import json, math
import numpy as np
from pcb import pads_abs, PLANES, OX, OY
import design as D

TRACK_CL = 0.13   # design clearance (JLC 4-layer minimum is 0.09 mm)


def objects(routes):
    """Every copper item on the outer layers: dict(net, layers, kind, ...)."""
    obs = []
    for q in pads_abs:
        lay = {"F.Cu"} if q["kind"] == "smd" else {"F.Cu", "B.Cu"}
        obs.append(dict(net=(q["net"] or f"#nc{q['ref']}.{q['name']}") if q["kind"] != "np" else "#hole", layers=lay, kind="pad", pd=q))
    for s in routes["segs"]:
        obs.append(dict(net=s["net"], layers={s["layer"]}, kind="seg", a=tuple(s["a"]), b=tuple(s["b"]), r=s["w"] / 2))
    for v in routes["vias"]:
        obs.append(dict(net=v["net"], layers={"F.Cu", "B.Cu"}, kind="circ", c=(v["x"], v["y"]), r=0.3))
    return obs


def bbox(o):
    if o["kind"] == "pad":
        q = o["pd"]; e = max(q["w"], q["h"], q.get("drill") or 0) / 2 + 0.05
        return q["x"] - e, q["y"] - e, q["x"] + e, q["y"] + e
    if o["kind"] == "seg":
        return min(o["a"][0], o["b"][0]) - o["r"], min(o["a"][1], o["b"][1]) - o["r"], \
            max(o["a"][0], o["b"][0]) + o["r"], max(o["a"][1], o["b"][1]) + o["r"]
    return o["c"][0] - o["r"], o["c"][1] - o["r"], o["c"][0] + o["r"], o["c"][1] + o["r"]


def dist_field(o, X, Y):
    """Distance from grid points to the object's copper edge (<=0 inside)."""
    if o["kind"] == "circ":
        return np.hypot(X - o["c"][0], Y - o["c"][1]) - o["r"]
    if o["kind"] == "seg":
        ax, ay = o["a"]; bx, by = o["b"]
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        t = np.zeros_like(X) if L2 == 0 else np.clip(((X - ax) * dx + (Y - ay) * dy) / L2, 0, 1)
        return np.hypot(ax + t * dx - X, ay + t * dy - Y) - o["r"]
    q = o["pd"]
    if q["kind"] == "np":
        return np.hypot(X - q["x"], Y - q["y"]) - (q["drill"] or q["w"]) / 2
    if q["shape"] == "circle":
        return np.hypot(X - q["x"], Y - q["y"]) - q["w"] / 2
    a = math.radians(q["angle"])
    # same convention as pcb.rot(x, y, -angle)
    c, s = math.cos(-a), math.sin(-a)
    px, py = X - q["x"], Y - q["y"]
    lx = px * c + py * s
    ly = -px * s + py * c
    if q["shape"] == "oval":     # stadium
        r = min(q["w"], q["h"]) / 2
        hx, hy = max(q["w"] / 2 - r, 0), max(q["h"] / 2 - r, 0)
        return np.hypot(np.maximum(np.abs(lx) - hx, 0), np.maximum(np.abs(ly) - hy, 0)) - r
    ddx = np.abs(lx) - q["w"] / 2
    ddy = np.abs(ly) - q["h"] / 2
    out = np.hypot(np.maximum(ddx, 0), np.maximum(ddy, 0))
    ins = np.minimum(np.maximum(ddx, ddy), 0)
    return out + ins
