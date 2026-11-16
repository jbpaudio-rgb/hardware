"""BOM and placement files for JLCPCB, plus a PNG preview of the routed board."""
import csv, json, math, collections
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Circle
import design as D
from pcb import pads_abs, pad_poly, OX, OY

# through-hole footprints whose origin is pin 1 rather than the body centre
PIN1_ORIGIN = ("ENC", "POT", "GPIO40", "JACK", "JSTPH3V", "HDR2")

# ---- BOM (JLCPCB format) ----
groups = collections.OrderedDict()
for p in D.parts:
    if p["dnp"] or p["fp"] in ("ENC", "CHOC", "POT", "HDR3", "HOLE"):
        continue
    k = (p["value"], D.FPNAME[p["fp"]], p["lcsc"])
    groups.setdefault(k, []).append(p["ref"])
with open("build/jlcpcb-bom.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for (val, fp, lc), refs in groups.items():
        w.writerow([val, ",".join(refs), fp.split(":")[1], lc.rstrip("?")])
with open("build/jlcpcb-cpl.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for p in D.parts:
        if p["dnp"] or p["fp"] in ("ENC", "CHOC", "POT", "HDR3", "HOLE"):
            continue
        x, y = p["x"] + OX, p["y"] + OY
        if p["fp"] in PIN1_ORIGIN:   # footprint origin on pin 1: give JLCPCB the body centre (pad extents)
            ps = [q for q in pads_abs if q["ref"] == p["ref"]]
            x = (min(q["x"] for q in ps) + max(q["x"] for q in ps)) / 2
            y = (min(q["y"] for q in ps) + max(q["y"] for q in ps)) / 2
        w.writerow([p["ref"], f"{x:.3f}mm", f"{-y:.3f}mm", p.get("side", "Top"), f"{p['rot'] % 360:.1f}"])
with open("build/pieces-a-souder.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Pièce", "Repères", "Qté"])
    hand = collections.OrderedDict()
    for p in D.parts:
        if p["dnp"] and p["fp"] not in ("HOLE",) and not p["ref"].startswith("JP"):
            hand.setdefault(p["value"], []).append(p["ref"])
    for v, refs in hand.items():
        w.writerow([v, ",".join(refs), len(refs)])
unverified = sorted({p["lcsc"] for p in D.parts if p["lcsc"].endswith("?")})
print("BOM lines", len(groups), "unverified LCSC:", unverified)

# ---- preview ----
r = json.load(open("build/routes.json")) if __import__("os").path.exists("build/routes.json") else dict(segs=[], vias=[])
fig, ax = plt.subplots(figsize=(18, 12), dpi=100)
pts = [(x + OX, y + OY) for x, y in D.OUTLINE]
ax.add_patch(Polygon(pts, closed=True, fill=True, fc="#0b3d1e", ec="yellow", lw=1.5))
for s in r["segs"]:
    if s["layer"] == "B.Cu":
        ax.plot([s["a"][0], s["b"][0]], [s["a"][1], s["b"][1]], color="#3a7bd5", lw=s["w"] * 3.3, solid_capstyle="round", alpha=0.85)
for s in r["segs"]:
    if s["layer"] == "F.Cu":
        ax.plot([s["a"][0], s["b"][0]], [s["a"][1], s["b"][1]], color="#e0483e", lw=s["w"] * 3.3, solid_capstyle="round", alpha=0.85)
for pd in pads_abs:
    if pd["kind"] == "np":
        ax.add_patch(Circle((pd["x"], pd["y"]), pd["drill"] / 2, fc="black", ec="grey", lw=0.3))
    elif pd["shape"] == "circle":
        ax.add_patch(Circle((pd["x"], pd["y"]), pd["w"] / 2, fc="#d4af37", ec="none"))
    else:
        ax.add_patch(Polygon(pad_poly(pd), closed=True, fc="#d4af37", ec="none"))
for v in r["vias"]:
    ax.add_patch(Circle((v["x"], v["y"]), 0.3, fc="#cccccc", ec="none"))
for p in D.parts:
    if p["fp"] in ("ENC", "CHOC", "POT", "RP2040", "SOIC16", "SOIC8W", "USBC", "JSTSH4", "JSTPH2"):
        cx, cy = p["x"] + OX, p["y"] + OY
        if p["fp"] == "ENC":
            cx, cy = cx + 7.5, cy + 2.5
        if p["fp"] == "POT":
            cx, cy = cx + 7.5, cy - 2.5
        ax.text(cx, cy, p["ref"], color="white", fontsize=7, ha="center", va="center")
_xs = [x + OX for x, y in D.OUTLINE]; _ys = [y + OY for x, y in D.OUTLINE]
ax.set_xlim(min(_xs) - 3, max(_xs) + 3); ax.set_ylim(max(_ys) + 3, min(_ys) - 3); ax.set_aspect("equal"); ax.axis("off")
fig.savefig("build/octavio-hat-preview.png", bbox_inches="tight", facecolor="#111111")
print("preview written")
