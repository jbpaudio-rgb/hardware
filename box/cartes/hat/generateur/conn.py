"""Connectivity check: every non-plane net must be one connected piece (tracks + vias + pads)."""
import json, math, collections
from pcb import pads_abs, PLANES, pad_dist, seg_point_dist
R = json.load(open("build/routes.json"))
segs, vias = R["segs"], R["vias"]
par = {}
def f(x):
    while par.setdefault(x, x) != x:
        par[x] = par[par[x]]; x = par[x]
    return x
def u(a, b): par[f(a)] = f(b)
nodes = collections.defaultdict(list)
for i, q in enumerate(pads_abs):
    if q["net"] and q["kind"] != "np":
        nodes[q["net"]].append(("p", i, q))
for i, s in enumerate(segs):
    nodes[s["net"]].append(("s", i, s))
for i, v in enumerate(vias):
    nodes[v["net"]].append(("v", i, v))
def layers(n):
    t, i, o = n
    if t == "s": return {o["layer"]}
    if t == "v": return {"F.Cu", "B.Cu"}
    return {"F.Cu", "B.Cu"} if o["kind"] == "th" else {"F.Cu"}
def touch(a, b):
    if not (layers(a) & layers(b)): return False
    ta, _, A = a; tb, _, B = b
    if ta == "s" and tb == "s":
        e = (A["w"] + B["w"]) / 2 + 0.01
        return min(seg_point_dist(A["a"], A["b"], B["a"]), seg_point_dist(A["a"], A["b"], B["b"]),
                   seg_point_dist(B["a"], B["b"], A["a"]), seg_point_dist(B["a"], B["b"], A["b"])) < e
    if ta == "s": a, b, ta, tb, A, B = b, a, tb, ta, B, A
    if tb == "s":
        if ta == "v": return min(seg_point_dist(B["a"], B["b"], (A["x"], A["y"])), 9) < 0.3 + B["w"] / 2
        return min(pad_dist(A, B["a"]), pad_dist(A, B["b"])) < B["w"] / 2 + 0.01 or pad_dist(A, ((B["a"][0]+B["b"][0])/2, (B["a"][1]+B["b"][1])/2)) < 0.01
    if ta == "v" and tb == "v": return math.hypot(A["x"] - B["x"], A["y"] - B["y"]) < 0.6
    if ta == "p" and tb == "v": A, B = B, A
    if "drill" in B or tb == "p" and ta == "v": return pad_dist(B, (A["x"], A["y"])) < 0.3
    return pad_dist(A, (B["x"], B["y"])) < 0.01 or pad_dist(B, (A["x"], A["y"])) < 0.01
bad = []
for net, ns in nodes.items():
    if net in PLANES: continue
    for k, a in enumerate(ns):
        for b in ns[k + 1:]:
            if touch(a, b): u((net,) + a[:2], (net,) + b[:2])
    groups = collections.defaultdict(list)
    for n in ns:
        if n[0] == "p": groups[f((net,) + n[:2])].append(f'{n[2]["ref"]}.{n[2]["name"]}')
    if len(groups) > 1:
        bad.append((net, [sorted(g) for g in groups.values()]))
print("nets with breaks:", len(bad))
for b in bad: print(" ", b[0], b[1])
bad_groups = bad
