"""Builds the KiCad board for the front panel, exports a Specctra DSN, routes it with Freerouting,
imports the routes and stitches GND pads to the ground planes."""
import math, re, subprocess, sys, uuid, json, copy
from sexpr import parse, dump, Q, find, first
import design as D

OX, OY = D.OFFSET
NS = uuid.UUID("6f6c6176-696f-4f63-7461-76696f000001")


def uid(*k):
    return str(uuid.uuid5(NS, "/".join(map(str, k))))


def rot(x, y, deg):
    """KiCad footprint rotation (deg CCW on screen, y down)."""
    a = math.radians(deg)
    return x * math.cos(a) + y * math.sin(a), -x * math.sin(a) + y * math.cos(a)


# ---------------- footprints ----------------
_cache = {}


def load_fp(key):
    if key not in _cache:
        t = parse(open(D.FP[key]).read())
        _cache[key] = t
    return copy.deepcopy(_cache[key])


nets = ["GND"] + sorted({n for p in D.parts for n in p["pads"].values()} - {"GND"})
NETNUM = {n: i + 1 for i, n in enumerate(nets)}
PLANES = ("GND", "+5V_LED")   # GND on In1.Cu, +5V_LED on In2.Cu
STITCH_S, STITCH_V = [], []
PREWIRED = set()

pads_abs = []      # dicts: ref, name, net, x, y, kind(smd/th/np), shape, w, h, angle(deg CCW), drill, layers
footprints = []

for p in D.parts:
    fp = load_fp(p["fp"])
    x, y, r = p["x"] + OX, p["y"] + OY, p["rot"]
    body = [e for e in fp[2:] if not (isinstance(e, list) and e and e[0] in
                                       ("version", "generator", "tedit", "tstamp", "layer", "at", "path"))]
    out = ["footprint", Q(D.FPNAME[p["fp"]]), ["layer", Q("F.Cu")], ["tstamp", uid("fp", p["ref"])],
           ["at", round(x, 4), round(y, 4)] + ([round(r, 3)] if r else [])]
    if p["lcsc"]:
        out.append(["property", Q("LCSC"), Q(p["lcsc"].rstrip("?"))])
    out.append(["path", Q("/" + uid("sym", p["ref"]))])
    for e in body:
        if isinstance(e, list) and e and e[0] == "fp_text":
            if e[1] == "reference":
                e[2] = Q(p["ref"])
            elif e[1] == "value":
                e[2] = Q(p["value"])
            at = first(e, "at")
            if at is not None and r:
                ang = float(at[3]) if len(at) > 3 else 0.0
                del at[3:]
                at.append(round(ang + r, 3))
        if isinstance(e, list) and e and e[0] == "pad":
            name = str(e[1])
            kind = e[2]
            shape = e[3]
            at = first(e, "at")
            px, py = float(at[1]), float(at[2])
            pang = float(at[3]) if len(at) > 3 else 0.0
            del at[3:]
            if r or pang:
                at.append(round(pang + r, 3))
            size = first(e, "size")
            w, h = float(size[1]), float(size[2])
            drill = first(e, "drill")
            dval = None
            if drill is not None:
                vals = [float(v) for v in drill[1:] if not isinstance(v, list) and v != "oval"]
                dval = vals[0] if vals else None
            net = p["pads"].get(name) if name else None
            # drop any net from the library footprint, add ours
            e[:] = [v for v in e if not (isinstance(v, list) and v and v[0] == "net")]
            if net:
                e.append(["net", NETNUM[net], Q(net)])
            dx, dy = rot(px, py, r)
            lays = first(e, "layers")
            if kind == "smd" and lays is not None and not any("Cu" in str(v) for v in lays[1:]):
                out.append(e)      # paste-only aperture: no copper
                continue
            pads_abs.append(dict(ref=p["ref"], name=name, net=net, x=x + dx, y=y + dy,
                                 kind={"smd": "smd", "thru_hole": "th", "np_thru_hole": "np"}.get(kind, kind),
                                 shape=shape, w=w, h=h, angle=pang + r, drill=dval))
        out.append(e)
    footprints.append(out)


def pad_poly(pd, grow=0.0):
    """Corner points of a (rotated) rect pad, in board mm."""
    w, h = pd["w"] / 2 + grow, pd["h"] / 2 + grow
    pts = []
    for cx, cy in ((-w, -h), (w, -h), (w, h), (-w, h)):
        dx, dy = rot(cx, cy, pd["angle"])
        pts.append((pd["x"] + dx, pd["y"] + dy))
    return pts


# ---------------- DSN export ----------------
def um(v):
    return int(round(v * 1000))


def write_dsn(path):
    L = []
    L.append('(pcb front.dsn\n (parser (string_quote ")\n  (space_in_quoted_tokens on)\n  (host_cad "KiCad\'s Pcbnew")\n  (host_version "8"))')
    L.append(" (resolution um 10)\n (unit um)")
    L.append(" (structure\n  (layer F.Cu (type signal) (property (index 0)))\n  (layer B.Cu (type signal) (property (index 1)))")
    pts = " ".join(f"{um(x + OX)} {-um(y + OY)}" for x, y in D.OUTLINE + [D.OUTLINE[0]])
    L.append(f"  (boundary (path pcb 0 {pts}))")
    # plane stitches (via + stub) are placed before routing and kept clear here
    for st in STITCH_S:
        ax, ay = st["a"]; bx, by = st["b"]
        L_ = math.hypot(bx - ax, by - ay) or 1e-6
        nx, ny = -(by - ay) / L_ * (st["w"] / 2 + 0.05), (bx - ax) / L_ * (st["w"] / 2 + 0.05)
        poly = [(ax + nx, ay + ny), (bx + nx, by + ny), (bx - nx, by - ny), (ax - nx, ay - ny), (ax + nx, ay + ny)]
        L.append(f'  (keepout "" (polygon F.Cu 0 {" ".join(f"{um(x)} {-um(y)}" for x, y in poly)}))')
    for v in STITCH_V:
        rr = 0.35
        poly = " ".join(f"{um(v['x'] + rr * math.cos(t))} {-um(v['y'] + rr * math.sin(t))}"
                        for t in [k * math.pi / 4 for k in range(9)])
        L.append(f'  (keepout "" (polygon signal 0 {poly}))')
    for pd in pads_abs:
        if pd["kind"] == "np":
            rr = (pd["drill"] or pd["w"]) / 2 + 0.2
            poly = " ".join(f"{um(pd['x'] + rr * math.cos(t))} {-um(pd['y'] + rr * math.sin(t))}"
                            for t in [k * math.pi / 4 for k in range(9)])
            L.append(f'  (keepout "" (polygon signal 0 {poly}))')
    L.append('  (via "Via[0-1]_600:300_um")')
    L.append("  (rule (width 150) (clearance 130) (clearance 130 (type default_smd)) (clearance 50 (type smd_smd)))\n )")
    L.append(" (placement")
    for p in D.parts:
        L.append(f'  (component IMG_{p["ref"]} (place {p["ref"]} 0 0 front 0))')
    L.append(" )\n (library")
    pins_of = {}
    stacks = []
    for p in D.parts:
        lines = [f"  (image IMG_{p['ref']}"]
        seen = {}
        for k, pd in enumerate([q for q in pads_abs if q["ref"] == p["ref"]]):
            if pd["kind"] == "np":
                continue
            nm = pd["name"] or f"X{k}"
            seen[nm] = seen.get(nm, 0) + 1
            pid = nm if seen[nm] == 1 else f"{nm}@{seen[nm]}"
            ps = f"PS_{p['ref']}_{k}"
            if pd["shape"] == "circle" or (pd["shape"] == "oval" and abs(pd["w"] - pd["h"]) < 1e-6):
                lay = ["F.Cu", "B.Cu"] if pd["kind"] == "th" else ["F.Cu"]
                shp = " ".join(f"(shape (circle {l} {um(pd['w'])}))" for l in lay)
            else:
                poly = pad_poly(dict(pd, x=0, y=0))
                coords = " ".join(f"{um(x)} {-um(y)}" for x, y in poly + [poly[0]])
                lay = ["F.Cu", "B.Cu"] if pd["kind"] == "th" else ["F.Cu"]
                shp = " ".join(f"(shape (polygon {l} 0 {coords}))" for l in lay)
            stacks.append(f"  (padstack {ps} {shp} (attach off))")
            lines.append(f"   (pin {ps} {pid} {um(pd['x'])} {-um(pd['y'])})")
            if pd["net"] and pd["net"] not in PLANES and pd["net"] not in PREWIRED:
                pins_of.setdefault(pd["net"], []).append(f"{p['ref']}-{pid}")
        lines.append("  )")
        L.extend(lines)
    L.extend(stacks)
    L.append('  (padstack "Via[0-1]_600:300_um" (shape (circle F.Cu 600)) (shape (circle B.Cu 600)) (attach off))')
    L.append(" )\n (network")
    for n, pins in pins_of.items():
        L.append(f'  (net "{n}" (pins {" ".join(pins)}))')
    power = [n for n in pins_of if n in ("+5V", "VBUS_C", "VBUS_J")]
    mid = [n for n in pins_of if n in ("+3V3", "+1V1")]
    sig = [n for n in pins_of if n not in power and n not in mid ]
    def cls(name, ns, w):
        return (f'  (class {name} {" ".join(chr(34) + n + chr(34) for n in ns)}\n'
                f'   (circuit (use_via "Via[0-1]_600:300_um")) (rule (width {w}) (clearance 130)))')
    L.append(cls("signal", sig, 150))
    L.append(cls("p3v3", mid, 250))
    L.append(cls("power", power, 600))
    L.append(" )\n (wiring)\n)")
    open(path, "w").write("\n".join(L))


def read_ses(path):
    t = parse(open(path).read())
    routes = first(t, "routes")
    res = first(routes, "resolution")
    scale = 1000.0 * float(res[2])   # units per mm
    segs, vias = [], []
    for net in find(first(routes, "network_out"), "net"):
        name = str(net[1])
        for w in find(net, "wire"):
            path = first(w, "path")
            lay, width = str(path[1]), float(path[2]) / scale
            c = [float(v) / scale for v in path[3:] if not isinstance(v, list)]
            pts = [(c[i], -c[i + 1]) for i in range(0, len(c) - 1, 2)]
            for a, b in zip(pts, pts[1:]):
                segs.append(dict(net=name, layer=lay, w=width, a=a, b=b))
        for v in find(net, "via"):
            vias.append(dict(net=name, x=float(v[2]) / scale, y=-float(v[3]) / scale))
    return segs, vias


# ---------------- geometry helpers for GND stitching ----------------
def seg_point_dist(a, b, p):
    ax, ay = a; bx, by = b; px, py = p
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = 0 if L2 == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L2))
    return math.hypot(ax + t * dx - px, ay + t * dy - py)


def pad_dist(pd, p):
    """Distance from point to pad (approximate: rect via local coords, circle exact)."""
    if pd["shape"] in ("circle",) or pd["kind"] == "np":
        r = (pd["drill"] if pd["kind"] == "np" else pd["w"]) / 2
        return max(0.0, math.hypot(p[0] - pd["x"], p[1] - pd["y"]) - r)
    lx, ly = rot(p[0] - pd["x"], p[1] - pd["y"], -pd["angle"])
    dx = max(0.0, abs(lx) - pd["w"] / 2)
    dy = max(0.0, abs(ly) - pd["h"] / 2)
    return math.hypot(dx, dy)


def inside_board(p, margin=0.6):
    x, y = p[0] - OX, p[1] - OY
    poly = D.OUTLINE
    ins = False
    for i in range(len(poly)):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % len(poly)]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            ins = not ins
        if seg_point_dist((x1, y1), (x2, y2), (x, y)) < margin:
            return False
    return ins


def stub_ok(pd, c, segs, CL, TW):
    for s in segs:
        if s["net"] == pd["net"]:
            continue
        if s["layer"] == "F.Cu" and seg_seg_dist(s["a"], s["b"], (pd["x"], pd["y"]), c) < CL + (s["w"] + TW) / 2:
            return False
    for q in pads_abs:
        if q is pd or q["net"] == pd["net"] or q["kind"] == "np":
            continue
        if abs(q["x"] - c[0]) > 6 or abs(q["y"] - c[1]) > 6:
            continue
        for t in (0.1, 0.25, 0.4, 0.55, 0.7, 0.85, 1.0):
            m = (pd["x"] + t * (c[0] - pd["x"]), pd["y"] + t * (c[1] - pd["y"]))
            if pad_dist(q, m) < CL + TW / 2:
                return False
    return True


ENC_CENTERS = [(cx + OX, cy + OY) for cx, cy in D.ENC_C]
U2C = next((p["x"] + OX, p["y"] + OY) for p in D.parts if p["ref"] == "U2")


def prewire():
    """Straight F.Cu links for the LED data chain (DO -> next DI) when the straight line is clear."""
    out = []
    for net in sorted({q["net"] for q in pads_abs if (q["net"] or "").startswith("LED_DIN_") and q["net"] != "LED_DIN_0"}):
        ps = [q for q in pads_abs if q["net"] == net]
        if len(ps) != 2:
            continue
        a, b = ps
        if math.hypot(a["x"] - b["x"], a["y"] - b["y"]) > 8:
            continue
        mx, my = (a["x"] + b["x"]) / 2, (a["y"] + b["y"]) / 2
        enc = min(ENC_CENTERS, key=lambda e: math.hypot(e[0] - mx, e[1] - my))
        L_ = math.hypot(mx - enc[0], my - enc[1]) or 1
        ux, uy = (mx - enc[0]) / L_, (my - enc[1]) / L_
        for d in (0, -0.6, 0.6, -1.2, 1.2, -1.8, 1.8, -2.4):
            m = (mx + d * ux, my + d * uy)
            fake = dict(a, x=m[0], y=m[1])
            if stub_ok(a, m, out, 0.2, 0.2) and stub_ok(fake, (b["x"], b["y"]), out, 0.2, 0.2):
                out.append(dict(net=net, layer="F.Cu", w=0.2, a=(a["x"], a["y"]), b=m))
                out.append(dict(net=net, layer="F.Cu", w=0.2, a=m, b=(b["x"], b["y"])))
                break
    return out


def stitch_planes(segs, vias):
    """Add a via + short track next to each SMD GND pad so it reaches the bottom plane."""
    added_v, added_s = [], []
    VR, CL, TW = 0.3, 0.2, 0.3
    for pd in pads_abs:
        if pd["net"] not in PLANES or pd["kind"] != "smd":
            continue
        ok = False
        # 1) reuse a nearby via of the same net if a straight stub can reach it
        for v in sorted([v for v in added_v if v["net"] == pd["net"]], key=lambda v: math.hypot(v["x"] - pd["x"], v["y"] - pd["y"])):
            c = (v["x"], v["y"])
            if math.hypot(c[0] - pd["x"], c[1] - pd["y"]) > 2.2:
                break
            if stub_ok(pd, c, segs + added_s, CL, TW):
                added_s.append(dict(net=pd["net"], layer="F.Cu", w=TW, a=(pd["x"], pd["y"]), b=c))
                ok = True
                break
        if ok:
            continue
        # 2) new via, preferring the direction away from the nearest encoder (outside the LED rings)
        enc = min(ENC_CENTERS, key=lambda e: math.hypot(e[0] - pd["x"], e[1] - pd["y"]))
        out = math.atan2(pd["y"] - enc[1], pd["x"] - enc[0]) if math.hypot(enc[0] - pd["x"], enc[1] - pd["y"]) < 16 else 0.0
        if math.hypot(U2C[0] - pd["x"], U2C[1] - pd["y"]) < 10:      # keep the RP2040 fan-out area free
            out = math.atan2(pd["y"] - U2C[1], pd["x"] - U2C[0])
        angs = sorted([k * math.pi / 8 for k in range(16)], key=lambda a: abs(math.remainder(a - out, 2 * math.pi)))
        for dist in (0.75, 0.95, 1.2, 1.5):
            for a in angs:
                ex = max(pd["w"], pd["h"]) / 2
                c = (pd["x"] + (ex + dist) * math.cos(a), pd["y"] + (ex + dist) * math.sin(a))
                if not inside_board(c):
                    continue
                bad = False
                for q in pads_abs:
                    if abs(q["x"] - c[0]) > 6 or abs(q["y"] - c[1]) > 6:
                        continue
                    if q is pd:
                        continue
                    need = VR + CL if q["net"] != pd["net"] or q["kind"] == "np" else VR + 0.05
                    if pad_dist(q, c) < need:
                        bad = True; break
                if bad:
                    continue
                for s in segs + added_s:
                    if s["net"] == pd["net"]:
                        continue
                    if seg_point_dist(s["a"], s["b"], c) < VR + CL + s["w"] / 2:
                        bad = True; break
                    # the stub from pad to via must not cross other-net tracks on F.Cu
                    if s["layer"] == "F.Cu" and seg_seg_dist(s["a"], s["b"], (pd["x"], pd["y"]), c) < CL + (s["w"] + TW) / 2:
                        bad = True; break
                if bad:
                    continue
                for v in vias + added_v:
                    if math.hypot(v["x"] - c[0], v["y"] - c[1]) < 2 * VR + CL:
                        bad = True; break
                if bad:
                    continue
                # stub must not pass over other-net pads
                for q in pads_abs:
                    if q is pd or q["net"] == pd["net"] or q["kind"] == "np":
                        continue
                    if abs(q["x"] - c[0]) > 6 or abs(q["y"] - c[1]) > 6:
                        continue
                    for t in (0.25, 0.5, 0.75):
                        m = (pd["x"] + t * (c[0] - pd["x"]), pd["y"] + t * (c[1] - pd["y"]))
                        if pad_dist(q, m) < CL + TW / 2:
                            bad = True; break
                    if bad:
                        break
                if bad:
                    continue
                added_v.append(dict(net=pd["net"], x=c[0], y=c[1]))
                added_s.append(dict(net=pd["net"], layer="F.Cu", w=TW, a=(pd["x"], pd["y"]), b=c))
                ok = True
                break
            if ok:
                break
        if not ok and pd["ref"] == "U2" and pd["name"] == "57":   # RP2040 exposed pad: vias in pad
            ok = True   # the footprint already has 9 thermal vias in the pad
        if not ok and pd["ref"] == "U2":   # other GND pin (TESTEN): short track into the exposed pad
            ep = next(q for q in pads_abs if q["ref"] == "U2" and q["name"] == "57" and q["kind"] == "smd")
            added_s.append(dict(net="GND", layer="F.Cu", w=0.2, a=(pd["x"], pd["y"]),
                                b=(min(max(pd["x"], ep["x"] - 1.2), ep["x"] + 1.2), min(max(pd["y"], ep["y"] - 1.2), ep["y"] + 1.2))))
            ok = True
        if not ok:
            print("NO STITCH for", pd["ref"], pd["name"], pd["net"])
    return added_s, added_v


def seg_seg_dist(a, b, c, d):
    def ccw(p, q, r):
        return (r[1] - p[1]) * (q[0] - p[0]) > (q[1] - p[1]) * (r[0] - p[0])
    if ccw(a, c, d) != ccw(b, c, d) and ccw(a, b, c) != ccw(a, b, d):
        return 0.0
    return min(seg_point_dist(a, b, c), seg_point_dist(a, b, d), seg_point_dist(c, d, a), seg_point_dist(c, d, b))


# ---------------- board file ----------------
LAYERS = """(layers (0 "F.Cu" signal) (1 "In1.Cu" power) (2 "In2.Cu" power) (31 "B.Cu" signal) (32 "B.Adhes" user "B.Adhesive") (33 "F.Adhes" user "F.Adhesive")
 (34 "B.Paste" user) (35 "F.Paste" user) (36 "B.SilkS" user "B.Silkscreen") (37 "F.SilkS" user "F.Silkscreen")
 (38 "B.Mask" user) (39 "F.Mask" user) (40 "Dwgs.User" user "User.Drawings") (41 "Cmts.User" user "User.Comments")
 (42 "Eco1.User" user "User.Eco1") (43 "Eco2.User" user "User.Eco2") (44 "Edge.Cuts" user) (45 "Margin" user)
 (46 "B.CrtYd" user "B.Courtyard") (47 "F.CrtYd" user "F.Courtyard") (48 "B.Fab" user) (49 "F.Fab" user))"""


def write_board(path, segs, vias):
    out = ["(kicad_pcb (version 20211014) (generator octavio)", " (general (thickness 1.6))", ' (paper "A3")',
           " " + LAYERS,
           " (setup (pad_to_mask_clearance 0) (pcbplotparams (layerselection 0x00010fc_ffffffff) (outputformat 1)"
           " (outputdirectory \"gerbers/\")))",
           ' (net 0 "")']
    for n, k in NETNUM.items():
        out.append(f' (net {k} "{n}")')
    for f in footprints:
        out.append(" " + dump(f, 1))
    pts = [(x + OX, y + OY) for x, y in D.OUTLINE]
    for a, b in zip(pts, pts[1:] + pts[:1]):
        out.append(f' (gr_line (start {a[0]} {a[1]}) (end {b[0]} {b[1]}) (layer "Edge.Cuts") (width 0.1))')
    out.append(f' (gr_text "Octavio Synth - facade v1.1 - RP2040" (at {OX + 230} {OY + 145}) (layer "B.SilkS")'
               ' (effects (font (size 1.5 1.5) (thickness 0.2)) (justify mirror)))')
    for s in segs:
        out.append(f' (segment (start {s["a"][0]:.4f} {s["a"][1]:.4f}) (end {s["b"][0]:.4f} {s["b"][1]:.4f})'
                   f' (width {s["w"]:.3f}) (layer "{s["layer"]}") (net {NETNUM[s["net"]]}))')
    for v in vias:
        out.append(f' (via (at {v["x"]:.4f} {v["y"]:.4f}) (size 0.6) (drill 0.3) (layers "F.Cu" "B.Cu")'
                   f' (net {NETNUM[v["net"]]}))')
    poly = " ".join(f"(xy {x} {y})" for x, y in pts)
    for lay, zn in (("F.Cu", "GND"), ("In1.Cu", "GND"), ("In2.Cu", "+5V_LED"), ("B.Cu", "GND")):
        out.append(f' (zone (net {NETNUM[zn]}) (net_name "{zn}") (layer "{lay}") (tstamp {uid("zone", lay)}) (hatch edge 0.508)'
                   ' (connect_pads (clearance 0.25)) (min_thickness 0.2) (filled_areas_thickness no)'
                   ' (fill (thermal_gap 0.4) (thermal_bridge_width 0.4)) (polygon (pts ' + poly + ')))')
    out.append(")")
    open(path, "w").write("\n".join(out) + "\n")


if __name__ == "__main__":
    step = sys.argv[1] if len(sys.argv) > 1 else "dsn"
    if step == "dsn":
        pw = prewire()
        a, b = stitch_planes(pw, [])
        a = pw + a
        PREWIRED.update(s_["net"] for s_ in pw)
        print("prewired LED links", len(PREWIRED))
        STITCH_S.extend(a); STITCH_V.extend(b)
        json.dump(dict(s=a, v=b), open("build/stitch.json", "w"))
        print("plane stitches", len(b), "of", sum(1 for q in pads_abs if q["net"] in PLANES and q["kind"] == "smd"))
        write_dsn("build/front.dsn")
        write_board("build/front_unrouted.kicad_pcb", [], [])
        json.dump(pads_abs, open("build/pads.json", "w"))
        print("pads", len(pads_abs), "nets", len(nets))
    elif step == "import":
        segs, vias = read_ses("build/front.ses")
        st = json.load(open("build/stitch.json"))
        s2 = [dict(x, a=tuple(x["a"]), b=tuple(x["b"])) for x in st["s"]]; v2 = st["v"]
        print("routes", len(segs), "vias", len(vias), "plane stitches", len(v2))
        write_board("build/octavio-facade.kicad_pcb", segs + s2, vias + v2)
        json.dump(dict(segs=segs + s2, vias=vias + v2), open("build/routes.json", "w"))
