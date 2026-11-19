#!/usr/bin/env python3
"""White labels, as polygons for enclosure.cpp (outer outlines counter-clockwise, holes clockwise).
  inscriptions.txt  labels inlaid in the panel, case XY (X = x, Y = -y)
  capuchons.txt     one section per keycap ("# NAME"), in the cap's own XY (centre 0,0, Y = towards the back)
Needs matplotlib. Font: DejaVu Sans Condensed Bold."""
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties
import numpy as np, os, re

# Octavio logo (web/octavio-app/logo-only.svg, straight segments only) on the rear wall, in the middle of the free part
# left of the connectors (seen from the front; on the right seen from behind). On the top, where the logo was (between the
# case edge and the glass), SINECRAFT in small letters.
LOGO_SVG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logo-octavio.svg")
REAR_FREE = (2.0, 166.0)                        # case x: past the corner post .. before the USB-C's left screw (171)
LOGO_CX, LOGO_W = sum(REAR_FREE) / 2, 70.0
LOGO_CZ = 24.0                                  # logo centre height: middle of the wall (3 .. 45)

FONT = FontProperties(fname="/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf")
# panel: (text, centre x, baseline y, cap height) in panel mm, y towards the player
PANEL = [
    ("VOLUME",    272, 149.6, 3.0),
]
# keycaps, left to right (same order as PAGE in fpb/design.py and Panel.h); top face 16.3 x 15.3 mm
PAGES = ["ENGINE", "VOICE", "+", "FILTER", "ENV", "MOD", "SEQ", "FX", "MIXER", "KEYS"]   # KEYS: a little keyboard drawn, no text
PARTS = ["1", "2", "3", "4"]
CAP_TEXT_W = 12.0          # widest label on a cap
CAP_H_PAGE, CAP_H_PART = 3.0, 6.0

def area(p):
    x, y = p[:, 0], p[:, 1]
    return 0.5 * np.sum(x * np.roll(y, -1) - np.roll(x, -1) * y)

def inside(pt, poly):
    x, y = pt; c = False
    for (x1, y1), (x2, y2) in zip(poly, np.roll(poly, -1, axis=0)):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1): c = not c
    return c

CAP_REF = TextPath((0, 0), "H", size=1, prop=FONT).get_extents().height
def width(text, cap):
    return TextPath((0, 0), text, size=1, prop=FONT).get_extents().width * cap / CAP_REF

def label(text, cx, base_y, cap):
    """Contours of `text`, centred on cx, baseline at Y = base_y (Y up), cap height `cap`."""
    tp = TextPath((0, 0), text, size=1, prop=FONT); s = cap / CAP_REF; ext = tp.get_extents()
    dx = cx - (ext.x0 + ext.width / 2) * s
    loops = [np.column_stack([np.array(l)[:, 0] * s + dx, np.array(l)[:, 1] * s + base_y])
             for l in tp.to_polygons(closed_only=True) if len(l) >= 3]
    out = []
    for i, l in enumerate(loops):
        if np.allclose(l[0], l[-1]): l = l[:-1]
        depth = sum(inside(l[0], m) for j, m in enumerate(loops) if j != i)
        if (depth % 2 == 0) != (area(l) > 0): l = l[::-1]
        out.append(l)
    return out

# SINECRAFT on the top, in a blocky pixel lettering (our own 5 x 7 grid, chunky pixels so it prints with a 0.4 mm nozzle)
BRAND, BRAND_CX, BRAND_CY, BRAND_PX = "SINECRAFT", 83.75, 5.9, 0.7   # centred over the glass, middle of the band above it
PIXEL_FONT = {
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "I": ["###", ".#.", ".#.", ".#.", ".#.", ".#.", "###"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "C": [".####", "#....", "#....", "#....", "#....", "#....", ".####"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
}

def pixel_grid(text):
    rows = [""] * 7
    for i, ch in enumerate(text):
        g = PIXEL_FONT[ch]
        for r in range(7): rows[r] += ("." if i else "") + g[r]
    return rows

# KEYS: a little piano keyboard (5 white keys, 3 black), 26 x 13 squares of 0.5 mm = 13 x 6.5 mm
def keyboard_grid():
    W, H = 26, 13
    g = [["#" if r in (0, H - 1) or c in (0, W - 1) else "." for c in range(W)] for r in range(H)]
    for sep in (5, 10, 15, 20):                                  # the lines between the white keys
        for r in range(H): g[r][sep] = "#"
    for sep in (5, 10, 20):                                      # the black keys, over the top 7 rows
        for r in range(1, 8):
            for c in (sep - 1, sep, sep + 1): g[r][c] = "#"
    return ["".join(row) for row in g]
KEYS_PX = 0.5

def pixel_label(text, cx, cy, px, rows=None):
    """Outline of the pixel lettering, centred on (cx, cy) in Y-up coordinates: outer loops counter-clockwise, holes
    clockwise (boundary edges with the ink on their left, chained; at a corner where two pixels only touch, turn left)."""
    rows = rows or pixel_grid(text); H, W = len(rows), len(rows[0])
    ink = lambda c, r: 0 <= r < H and 0 <= c < W and rows[H - 1 - r][c] == "#"     # r counted upwards
    nxt = {}
    for r in range(H):
        for c in range(W):
            if not ink(c, r): continue
            if not ink(c, r - 1): nxt.setdefault((c, r), []).append((c + 1, r))
            if not ink(c + 1, r): nxt.setdefault((c + 1, r), []).append((c + 1, r + 1))
            if not ink(c, r + 1): nxt.setdefault((c + 1, r + 1), []).append((c, r + 1))
            if not ink(c - 1, r): nxt.setdefault((c, r + 1), []).append((c, r))
    loops = []
    while nxt:
        start = next(iter(nxt)); loop = [start]; cur = start; prev = None
        while True:
            outs = nxt[cur]
            if len(outs) > 1 and prev is not None:          # pick the left turn
                d = (cur[0] - prev[0], cur[1] - prev[1]); left = (-d[1], d[0])
                o = next((o for o in outs if (o[0] - cur[0], o[1] - cur[1]) == left), outs[0])
            else: o = outs[0]
            outs.remove(o)
            if not outs: del nxt[cur]
            prev, cur = cur, o
            if cur == start and (start not in nxt): break
            if cur == start: break
            loop.append(cur)
        pts = []                                             # drop the points in the middle of straight runs
        for i, p in enumerate(loop):
            a, b = loop[i - 1], loop[(i + 1) % len(loop)]
            if (p[0] - a[0]) * (b[1] - p[1]) - (p[1] - a[1]) * (b[0] - p[0]) != 0: pts.append(p)
        loops.append(np.array([[cx + (x - W / 2) * px, cy + (y - H / 2) * px] for x, y in pts]))
    return loops

def dump(f, loops):
    for l in loops: f.write(f"{len(l)}\n" + "".join(f"{x:.4f} {y:.4f}\n" for x, y in l))

def logo():
    d = re.search(r'\bd="([^"]+)"', open(LOGO_SVG).read()).group(1)
    loops = [np.array([[float(a), float(b)] for a, b in re.findall(r"[ML]\s*([-\d.]+)[ ,]([-\d.]+)", sub)])
             for sub in d.split("Z") if sub.strip()]
    allp = np.vstack(loops); lo, hi = allp.min(0), allp.max(0)
    s = LOGO_W / (hi[0] - lo[0]); c = (lo + hi) / 2
    loops = [np.column_stack([LOGO_CX + (l[:, 0] - c[0]) * s, LOGO_CZ - (l[:, 1] - c[1]) * s]) for l in loops]   # x, z up
    out = []
    for i, l in enumerate(loops):
        depth = sum(inside(l[0], m) for j, m in enumerate(loops) if j != i)
        if (depth % 2 == 0) != (area(l) > 0): l = l[::-1]
        out.append(l)
    h = (hi[1] - lo[1]) * s
    print(f"logo (arriere): {LOGO_W:.0f} x {h:.1f} mm, centre x {LOGO_CX:.1f}, z {LOGO_CZ:.1f}")
    return out, h

with open("inscriptions.txt", "w") as f:
    dump(f, pixel_label(BRAND, BRAND_CX, -BRAND_CY, BRAND_PX))
    print(f"facade  {BRAND} (pixels de {BRAND_PX} mm): {len(pixel_grid(BRAND)[0]) * BRAND_PX:.1f} x {7 * BRAND_PX:.1f} mm")
    for text, cx, by, cap in PANEL:
        dump(f, label(text, cx, -by, cap))
        print(f"facade  {text}: {width(text, cap):.1f} mm")

# rear wall: what each connector is, above it (x along the case, z height; read from behind, so mirrored in x)
REAR = [
    ("MIDI IN",    285.9, 29.6, 2.0), ("MIDI OUT", 272.9, 29.6, 2.0), ("LINE IN", 259.9, 29.6, 2.0), ("PHONES", 242.6, 29.6, 2.0),
    ("OUT L",      205.0, 28.6, 2.4), ("OUT R", 222.0, 28.6, 2.4),   # LINE OUT L, R (from behind, L is on the right)
    ("POWER",      180.0, 20.8, 2.4),
]
mirror = lambda loops, cx: [np.column_stack([2 * cx - l[:, 0], l[:, 1]])[::-1] for l in loops]
with open("arriere.txt", "w") as f:
    lg, lh = logo()
    dump(f, mirror(lg, LOGO_CX))
    for text, cx, bz, cap in REAR:
        dump(f, mirror(label(text, cx, bz, cap), cx)); print(f"arriere {text}: {width(text, cap):.1f} mm")

# one cap height for all the page names, so the row looks even
hp = min(CAP_H_PAGE, min(CAP_TEXT_W / width(t, 1) for t in PAGES))
CAP_H_PLUS = 6.0           # the PAGE 2 key just shows a big "+"
with open("capuchons.txt", "w") as f:
    for t in PAGES:
        if t == "KEYS":
            f.write("# KEYS\n"); dump(f, pixel_label("", 0, 0, KEYS_PX, keyboard_grid())); continue
        h = CAP_H_PLUS if t == "+" else hp
        f.write("# " + ("PLUS" if t == "+" else t.replace(" ", "_")) + "\n"); dump(f, label(t, 0, -h / 2, h))
    for t in PARTS:
        f.write(f"# PART{t}\n"); dump(f, label(t, 0, -CAP_H_PART / 2, CAP_H_PART))
print(f"capuchons de page: lettres de {hp:.2f} mm, le plus large {max(width(t, hp) for t in PAGES if t not in ('+', 'KEYS')):.1f} mm")
