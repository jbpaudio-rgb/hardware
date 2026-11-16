"""Copper clearance check on the outer layers: tracks/vias vs everything of another net."""
import json, sys, collections
import numpy as np
from geom import objects, bbox, dist_field

LIMIT = float(sys.argv[1]) if len(sys.argv) > 1 else 0.12
R = json.load(open("build/routes.json"))
obs = objects(R)
grid = collections.defaultdict(list)
for i, o in enumerate(obs):
    x0, y0, x1, y1 = bbox(o)
    for gx in range(int(x0 // 2), int(x1 // 2) + 1):
        for gy in range(int(y0 // 2), int(y1 // 2) + 1):
            grid[gx, gy].append(i)


def samples(o):
    if o["kind"] == "circ":
        a = np.linspace(0, 2 * np.pi, 24, endpoint=False)
        return o["c"][0] + o["r"] * np.cos(a), o["c"][1] + o["r"] * np.sin(a)
    (ax, ay), (bx, by), r = o["a"], o["b"], o["r"]
    L = max(np.hypot(bx - ax, by - ay), 1e-6)
    t = np.linspace(0, 1, max(2, int(L / 0.03) + 1))
    nx, ny = -(by - ay) / L * r, (bx - ax) / L * r
    xs = np.concatenate([ax + t * (bx - ax) + nx, ax + t * (bx - ax) - nx, [ax - (bx - ax) / L * r, bx + (bx - ax) / L * r]])
    ys = np.concatenate([ay + t * (by - ay) + ny, ay + t * (by - ay) - ny, [ay - (by - ay) / L * r, by + (by - ay) / L * r]])
    return xs, ys


bad = {}
for i, o in enumerate(obs):
    if o["kind"] == "pad":
        continue
    X, Y = samples(o)
    x0, y0, x1, y1 = bbox(o)
    cand = set()
    for gx in range(int(x0 // 2) - 1, int(x1 // 2) + 2):
        for gy in range(int(y0 // 2) - 1, int(y1 // 2) + 2):
            cand.update(grid[gx, gy])
    for j in cand:
        p = obs[j]
        if j == i or p["net"] == o["net"] or not (p["layers"] & o["layers"]):
            continue
        d = float(dist_field(p, X, Y).min())
        if d < LIMIT:
            key = tuple(sorted([(o["net"], o["kind"]), (p["net"], p["kind"])]))
            if key not in bad or d < bad[key][0]:
                where = (round(float(X.mean()), 2), round(float(Y.mean()), 2))
                bad[key] = (round(d, 3), where, sorted(o["layers"] & p["layers"]))
print("clearance problems:", len(bad))
for k, v in sorted(bad.items(), key=lambda kv: kv[1][0])[:60]:
    print(" ", v[0], k, v[1], v[2])
