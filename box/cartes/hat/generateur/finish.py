"""Finish the connections the autorouter left open: grid maze router (2 layers + vias)
over the routed board, then rewrite the board files."""
import json, math, heapq, collections, sys
import numpy as np
import pcb
from pcb import pads_abs, PLANES, inside_board, write_board
from geom import objects, bbox, dist_field, TRACK_CL

G = 0.05          # grid step (mm)
TW = 0.15         # track width
VIA_R = 0.3
VIA_COST = 30.0   # in grid steps
MARGIN = 5.0
LAYERS = ("F.Cu", "B.Cu")


def groups_of(routes):
    """Connectivity groups (lists of pad indices) per broken net, using conn.py logic."""
    import conn  # noqa: runs the check on build/routes.json, exposes bad list
    return conn.bad_groups


def route_net(net, groups, routes):
    obs = objects(routes)
    # terminal pads of the two groups to join (closest pair of groups)
    A, B = groups[0], groups[1]
    pa = [q for q in pads_abs if q["net"] == net and f'{q["ref"]}.{q["name"]}' in A]
    pb = [q for q in pads_abs if q["net"] == net and f'{q["ref"]}.{q["name"]}' in B]
    best = min(((a, b) for a in pa for b in pb), key=lambda ab: math.hypot(ab[0]["x"] - ab[1]["x"], ab[0]["y"] - ab[1]["y"]))
    a, b = best
    x0 = min(a["x"], b["x"]) - MARGIN; x1 = max(a["x"], b["x"]) + MARGIN
    y0 = min(a["y"], b["y"]) - MARGIN; y1 = max(a["y"], b["y"]) + MARGIN
    nx, ny = int((x1 - x0) / G) + 1, int((y1 - y0) / G) + 1
    xs = x0 + np.arange(nx) * G; ys = y0 + np.arange(ny) * G
    X, Y = np.meshgrid(xs, ys, indexing="xy")          # shape (ny, nx)
    dmin = {l: np.full(X.shape, 99.0) for l in LAYERS}
    for o in obs:
        if o["net"] == net:
            continue
        bx0, by0, bx1, by1 = bbox(o)
        if bx1 < x0 - 1 or bx0 > x1 + 1 or by1 < y0 - 1 or by0 > y1 + 1:
            continue
        i0 = max(0, int((by0 - 1 - y0) / G)); i1 = min(ny, int((by1 + 1 - y0) / G) + 2)
        j0 = max(0, int((bx0 - 1 - x0) / G)); j1 = min(nx, int((bx1 + 1 - x0) / G) + 2)
        if i0 >= i1 or j0 >= j1:
            continue
        d = dist_field(o, X[i0:i1, j0:j1], Y[i0:i1, j0:j1])
        for l in o["layers"] & set(LAYERS):
            np.minimum(dmin[l][i0:i1, j0:j1], d, out=dmin[l][i0:i1, j0:j1])
    inb = np.vectorize(lambda x, y: inside_board((x, y), 0.5))(X, Y)
    need_t = TW / 2 + TRACK_CL + 0.03
    need_v = VIA_R + TRACK_CL + 0.03
    free = {l: (dmin[l] >= need_t) & inb for l in LAYERS}
    via_ok = (dmin["F.Cu"] >= need_v) & (dmin["B.Cu"] >= need_v) & inb
    # also keep vias off the plane-stitch vias' neighbourhood is covered by dmin (they are objects)

    def cells_in(q):
        d = dist_field(dict(kind="pad", pd=q), X, Y)
        lays = ("F.Cu",) if q["kind"] == "smd" else LAYERS
        return [(l, i, j) for l in lays for i, j in zip(*np.nonzero(d <= -0.02)) if free[l][i, j] or True]

    src = cells_in(a); dst = set(cells_in(b))
    if not src or not dst:
        return None
    for l, i, j in list(src) + list(dst):
        free[l][i, j] = True     # inside our own pad is always fine
    li = {l: k for k, l in enumerate(LAYERS)}
    dist = {}
    prev = {}
    h = []
    tx = np.mean([j for _, _, j in dst]); ty = np.mean([i for _, i, _ in dst])
    for c in src:
        dist[c] = 0.0
        heapq.heappush(h, (math.hypot(c[2] - tx, c[1] - ty), 0.0, c))
    steps = [(-1, 0, 1), (1, 0, 1), (0, -1, 1), (0, 1, 1), (-1, -1, 1.414), (-1, 1, 1.414), (1, -1, 1.414), (1, 1, 1.414)]
    end = None
    n = 0
    while h:
        f, g, c = heapq.heappop(h)
        if g > dist.get(c, 1e18):
            continue
        if c in dst:
            end = c; break
        n += 1
        if n > 3_000_000:
            break
        l, i, j = c
        for di, dj, w in steps:
            ii, jj = i + di, j + dj
            if 0 <= ii < ny and 0 <= jj < nx and free[l][ii, jj]:
                if di and dj and not (free[l][i, jj] and free[l][ii, j]):
                    continue
                nc = (l, ii, jj); ng = g + w + (0.2 if di and dj else 0)
                if ng < dist.get(nc, 1e18):
                    dist[nc] = ng; prev[nc] = c
                    heapq.heappush(h, (ng + math.hypot(jj - tx, ii - ty), ng, nc))
        if via_ok[i, j]:
            ol = LAYERS[1 - li[l]]
            nc = (ol, i, j); ng = g + VIA_COST
            if free[ol][i, j] and ng < dist.get(nc, 1e18):
                dist[nc] = ng; prev[nc] = c
                heapq.heappush(h, (ng + math.hypot(j - tx, i - ty), ng, nc))
    if end is None:
        return None
    path = [end]
    while path[-1] in prev:
        path.append(prev[path[-1]])
    path.reverse()
    pt = lambda c: (round(x0 + c[2] * G, 4), round(y0 + c[1] * G, 4))
    segs, vias = [], []
    # snap ends to pad centres
    run = [path[0]]
    for c in path[1:]:
        if c[0] != run[-1][0]:
            segs += simplify(run, pt, net)
            vias.append(dict(net=net, x=pt(c)[0], y=pt(c)[1]))
            run = [c]
        else:
            run.append(c)
    segs += simplify(run, pt, net)
    if segs:
        segs[0]["a"] = (a["x"], a["y"]) if segs[0]["layer"] == "F.Cu" or a["kind"] != "smd" else segs[0]["a"]
        segs[-1]["b"] = (b["x"], b["y"]) if segs[-1]["layer"] == "F.Cu" or b["kind"] != "smd" else segs[-1]["b"]
    return segs, vias


def simplify(run, pt, net):
    pts = [pt(c) for c in run]
    if len(pts) < 2:
        return []
    out = [pts[0]]
    for k in range(1, len(pts) - 1):
        ax, ay = out[-1]; bx, by = pts[k]; cx, cy = pts[k + 1]
        if abs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax)) > 1e-6:
            out.append(pts[k])
    out.append(pts[-1])
    return [dict(net=net, layer=run[0][0], w=TW, a=p, b=q) for p, q in zip(out, out[1:]) if p != q]


FAILED = set()
if __name__ == "__main__":
    for it in range(20):
        import importlib, conn
        importlib.reload(conn)
        if not conn.bad_groups:
            break
        R = json.load(open("build/routes.json"))
        todo = [b for b in conn.bad_groups if b[0] not in FAILED]
        if not todo:
            break
        net, groups = todo[0]
        res = route_net(net, groups, R)
        if res is None:
            print("FAILED", net); FAILED.add(net); continue
        s, v = res
        print("routed", net, len(s), "segments", len(v), "vias")
        R["segs"] += s; R["vias"] += v
        json.dump(R, open("build/routes.json", "w"))
    R = json.load(open("build/routes.json"))
    segs = [dict(x, a=tuple(x["a"]), b=tuple(x["b"])) for x in R["segs"]]
    write_board("build/octavio-hat.kicad_pcb", segs, R["vias"])
