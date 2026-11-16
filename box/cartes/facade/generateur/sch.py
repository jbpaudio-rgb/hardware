"""Writes a KiCad schematic of the front-panel board from design.py:
one box symbol per part, every pin tied to its net with a label (same uuids as the board footprints)."""
import uuid
import design as D
from pcb import uid, NETNUM, pads_abs
PADNAMES = {}
for q in pads_abs:
    if q["name"] and q["kind"] != "np":
        PADNAMES.setdefault(q["ref"], [])
        if q["name"] not in PADNAMES[q["ref"]]:
            PADNAMES[q["ref"]].append(q["name"])

G = 2.54


def snap(v):
    return round(round(v / 1.27) * 1.27, 4)


def esc(s):
    return s.replace('"', '\\"')


libs = {}       # lib name -> text
inst = []
labels = []
nocon = []
instances = []


def libsym(p):
    title, pins = p["sym"]
    pins = list(pins) + [x for x in PADNAMES.get(p["ref"], []) if x not in pins]
    names = D.PIN_NAMES.get(p["ref"], {})
    key = (title, tuple(pins), tuple(names.get(x, x) for x in pins))
    name = "".join(c if c.isalnum() else "_" for c in title) + f"_{len(pins)}p"
    if key in [v[0] for v in libs.values()]:
        for k, v in libs.items():
            if v[0] == key:
                return k, v[1]
    while name in libs:
        name += "x"
    n = len(pins)
    if n <= 2:
        left, right = pins[:1], pins[1:]
    else:
        half = (n + 1) // 2
        left, right = pins[:half], pins[half:]
    rows = max(len(left), len(right))
    namew = max([len(names.get(x, x)) for x in pins] + [len(title)]) * 1.0
    w = max(5.08, snap(namew * (1 if n <= 2 else 2) + 2.54))
    if n <= 2:
        w = 5.08
    h = rows * G + G
    top = snap(h / 2)
    pinpos = {}
    gfx = [f'      (rectangle (start {-w / 2} {top}) (end {w / 2} {top - h}) (stroke (width 0.254) (type default) (color 0 0 0 0)) (fill (type background)))']
    pl = []
    for i, x in enumerate(left):
        py = snap(top - G - i * G)
        px = snap(-w / 2 - G)
        pinpos[x] = (px, py, "L")
        pl.append(f'      (pin passive line (at {px} {py} 0) (length 2.54) (name "{esc(names.get(x, x) if n > 2 else "~")}" (effects (font (size 1.016 1.016)))) (number "{esc(x)}" (effects (font (size 1.016 1.016)))))')
    for i, x in enumerate(right):
        py = snap(top - G - i * G)
        px = snap(w / 2 + G)
        pinpos[x] = (px, py, "R")
        pl.append(f'      (pin passive line (at {px} {py} 180) (length 2.54) (name "{esc(names.get(x, x) if n > 2 else "~")}" (effects (font (size 1.016 1.016)))) (number "{esc(x)}" (effects (font (size 1.016 1.016)))))')
    text = (f'    (symbol "Octavio:{name}" (in_bom yes) (on_board yes)\n'
            f'      (property "Reference" "{p["ref"].rstrip("0123456789")}" (id 0) (at 0 {top + 1.27} 0) (effects (font (size 1.27 1.27))))\n'
            f'      (property "Value" "{esc(title)}" (id 1) (at 0 {top - h - 1.27} 0) (effects (font (size 1.27 1.27))))\n'
            f'      (property "Footprint" "" (id 2) (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n'
            f'      (property "Datasheet" "" (id 3) (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n'
            f'      (symbol "{name}_0_1"\n' + "\n".join(gfx) + "\n      )\n"
            f'      (symbol "{name}_1_1"\n' + "\n".join(pl) + "\n      )\n    )")
    libs[name] = (key, (text, pinpos, w, h, top))
    return name, libs[name][1]


# shelf layout on an A0 sheet
X0, Y0, XMAX = 25.4, 30.48, 1150.0
x, y, rowh = X0, Y0, 0
import re
PRI = ["U2", "U3", "Y1", "U1", "U7", "SW1", "SW2", "J4", "J1", "J2", "J3", "JP1", "D1", "D2", "U4", "U5", "U6"]
def key(p):
    r = p["ref"]
    if r in PRI:
        return (0, PRI.index(r))
    pre = re.match(r"[A-Z]+", r).group(0)
    num = int(r[len(pre):] or 0)
    return (1, ["ENC", "SW", "RV", "R", "C", "LED"].index(pre) if pre in ["ENC", "SW", "RV", "R", "C", "LED"] else 9, num)
order = sorted([p for p in D.parts if p["sym"] is not None], key=key)

prev_group = None
for p in order:
    name, (text, pinpos, w, h, top) = libsym(p)
    labw = max([len(n) for n in p["pads"].values()] + [4]) * 1.1 + 5
    cw, ch = w + 2 * labw + 2 * G, h + 7.62
    grp = p["ref"].rstrip("0123456789")
    if (x + cw > XMAX) or (grp != prev_group and grp in ("LED", "ENC", "R", "SW") and x > X0 and prev_group != grp and not (prev_group == "SW" and grp == "SW")):
        x, y, rowh = X0, y + rowh + 5.08, 0
    prev_group = grp
    cx, cy = snap(x + cw / 2), snap(y + ch / 2 - 1.27)
    sid = uid("sym", p["ref"])
    pins_txt = " ".join(f'(pin "{esc(pn)}" (uuid {uid("pin", p["ref"], pn)}))' for pn in pinpos)
    fpname = D.FPNAME[p["fp"]]
    lc = p["lcsc"].rstrip("?")
    inst.append(
        f'  (symbol (lib_id "Octavio:{name}") (at {cx} {cy} 0) (unit 1)\n'
        f'    (in_bom {"no" if p["dnp"] else "yes"}) (on_board yes) (uuid {sid})\n'
        f'    (property "Reference" "{p["ref"]}" (id 0) (at {cx} {snap(cy - top - 1.27)} 0) (effects (font (size 1.27 1.27))))\n'
        f'    (property "Value" "{esc(p["value"])}" (id 1) (at {cx} {snap(cy - top + h + 1.27)} 0) (effects (font (size 1.27 1.27))))\n'
        f'    (property "Footprint" "{fpname}" (id 2) (at {cx} {cy} 0) (effects (font (size 1.27 1.27)) hide))\n'
        f'    (property "Datasheet" "" (id 3) (at {cx} {cy} 0) (effects (font (size 1.27 1.27)) hide))\n'
        f'    (property "LCSC" "{lc}" (id 4) (at {cx} {cy} 0) (effects (font (size 1.27 1.27)) hide))\n'
        f'    {pins_txt}\n  )')
    instances.append(f'    (path "/{sid}" (reference "{p["ref"]}") (unit 1) (value "{esc(p["value"])}") (footprint "{fpname}"))')
    for pn, (px, py, side) in pinpos.items():
        ax, ay = snap(cx + px), snap(cy - py)
        net = p["pads"].get(pn)
        if net:
            if side == "L":
                labels.append(f'  (label "{esc(net)}" (at {ax} {ay} 180) (effects (font (size 1.27 1.27)) (justify right)) (uuid {uid("lab", p["ref"], pn)}))')
            else:
                labels.append(f'  (label "{esc(net)}" (at {ax} {ay} 0) (effects (font (size 1.27 1.27)) (justify left)) (uuid {uid("lab", p["ref"], pn)}))')
        else:
            nocon.append(f'  (no_connect (at {ax} {ay}) (uuid {uid("nc", p["ref"], pn)}))')
    x += cw
    rowh = max(rowh, ch)

notes = ('  (text "Octavio Synth - carte de facade v1 (RP2040). Genere depuis design.py : chaque broche est reliee a son net par une etiquette.\\n'
         'Liaison interne J2 (JST-SH) : 1 GND, 2 +5V, 3 D+, 4 D-. J3 (JST-PH) : 1 +5V LED, 2 GND. JP1 : pont 0R a souder pour alimenter les LED par l\'USB-C de test." '
         f'(at 25.4 15.24 0) (effects (font (size 2 2)) (justify left bottom)) (uuid {uid("note")}))')
out = ['(kicad_sch (version 20211123) (generator octavio)', f'  (uuid {uid("sheet")})', '  (paper "A0")',
       '  (title_block (title "Octavio Synth - facade") (rev "1") (company "Octavio"))', '  (lib_symbols']
out += [v[1][0] for v in libs.values()]
out.append('  )')
out.append(notes)
out += inst + labels + nocon
out.append('  (sheet_instances (path "/" (page "1")))')
out.append('  (symbol_instances')
out += instances
out.append('  )\n)')
open("build/octavio-facade.kicad_sch", "w").write("\n".join(out) + "\n")
print("symbols", len(inst), "lib symbols", len(libs), "labels", len(labels), "no-connects", len(nocon), "rows end y", round(y + rowh))
