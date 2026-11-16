#!/usr/bin/env python3
"""box/sounds/factory.txt -> box/FactorySounds.h (the factory sounds, 5 per engine). Run from anywhere."""
import os, re
here = os.path.dirname(os.path.abspath(__file__)); box = os.path.dirname(here)
src = os.path.join(box, "sounds", "factory.txt"); dst = os.path.join(box, "FactorySounds.h")
sounds = []; eng = None
for raw in open(src, encoding="utf-8"):
    line = raw.split("//")[0].strip()
    if not line: continue
    if line.startswith("## "): sounds.append([eng, line[3:].strip(), []]); continue
    if line.startswith("#"):
        m = re.match(r"#\s+([a-z0-9]+)\s*$", line)
        if m: eng = m.group(1)
        continue
    w = line.split(); body = sounds[-1][2]
    if w[0] == "pat":
        t, d = w[1], w[2]; assert len(d) == 16 and set(d) <= set("012"), line
        body += [f"p{t}_{k} {c}" for k, c in enumerate(d)]
    elif w[0] == "pit":
        t, v = w[1], w[2:]; assert len(v) == 16, line
        body += [f"n{t}_{k} {x}" for k, x in enumerate(v)]
    else:
        assert len(w) == 2, line; float(w[1]); body.append(f"{w[0]} {w[1]}")
esc = lambda s: s.replace("\\", "\\\\").replace('"', '\\"')
with open(dst, "w", encoding="utf-8") as f:
    f.write("// Made by tools/sounds.py from sounds/factory.txt. Do not edit.\n#pragma once\nnamespace box {\n")
    f.write("struct FactorySound { const char* eng; const char* name; const char* body; };   // body: \"id value\\n\" lines\n")
    f.write("static const FactorySound FACTORY[] = {\n")
    for e, n, b in sounds: f.write(f'    {{ "{e}", "{esc(n)}", "' + "\\n".join(b) + '\\n" },\n')
    f.write("};\nconstexpr int NFACTORY = (int) (sizeof FACTORY / sizeof FACTORY[0]);\n} // namespace box\n")
per = {}
for e, n, b in sounds: per[e] = per.get(e, 0) + 1
print(f"{len(sounds)} sounds:", per)
