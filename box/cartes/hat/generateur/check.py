import json, math, sys
sys.path.insert(0,'.')
from pcb import pads_abs, pad_poly, pad_dist, inside_board, OX, OY
# 1) pads of different nets closer than 0.2 mm (center-sampled check)
bad=[]
P=[p for p in pads_abs]
for i,a in enumerate(P):
    for b in P[i+1:]:
        if a['ref']==b['ref']: continue
        if abs(a['x']-b['x'])>8 or abs(a['y']-b['y'])>8: continue
        if a['net'] and a['net']==b['net']: continue
        # sample points on a's outline/center
        pts=[(a['x'],a['y'])]
        if a['kind']!='np' and a['shape']!='circle': pts+=pad_poly(a)
        else:
            r=(a['drill'] if a['kind']=='np' else a['w'])/2
            pts+=[(a['x']+r*math.cos(t),a['y']+r*math.sin(t)) for t in [k*math.pi/4 for k in range(8)]]
        d=min(pad_dist(b,p) for p in pts)
        if d<0.2: bad.append((round(d,2),a['ref'],a['name'],b['ref'],b['name']))
print('close pads:',len(bad)); [print(x) for x in bad[:40]]
out=[(p['ref'],p['name']) for p in P if not inside_board((p['x'],p['y']),0.3)]
print('outside board:',out[:20])
