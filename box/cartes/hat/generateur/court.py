import math, sys
sys.path.insert(0,'.')
import design as D
from pcb import load_fp, rot, OX, OY
from sexpr import find, first
def crt(key):
    fp=load_fp(key); xs=[];ys=[]
    for e in fp:
        if not isinstance(e,list) or not e: continue
        lay=first(e,'layer')
        if lay is None or 'CrtYd' not in str(lay[1]): continue
        for k in ('start','end','center'):
            v=first(e,k)
            if v: xs.append(float(v[1])); ys.append(float(v[2]))
        pts=first(e,'pts')
        if pts:
            for xy in find(pts,'xy'): xs.append(float(xy[1])); ys.append(float(xy[2]))
        if e[0]=='fp_circle':
            c=first(e,'center'); en=first(e,'end'); r=math.hypot(float(en[1])-float(c[1]),float(en[2])-float(c[2]))
            xs+= [float(c[1])-r,float(c[1])+r]; ys+=[float(c[2])-r,float(c[2])+r]
    if not xs:
        # fall back on pads
        for e in find(fp,'pad'):
            at=first(e,'at'); sz=first(e,'size')
            xs+=[float(at[1])-float(sz[1])/2,float(at[1])+float(sz[1])/2]; ys+=[float(at[2])-float(sz[2])/2,float(at[2])+float(sz[2])/2]
    return min(xs),min(ys),max(xs),max(ys)
cache={}
rects=[]
for p in D.parts:
    if p['fp'] not in cache: cache[p['fp']]=crt(p['fp'])
    x0,y0,x1,y1=cache[p['fp']]
    pts=[]
    for cx,cy in ((x0,y0),(x1,y0),(x1,y1),(x0,y1)):
        dx,dy=rot(cx,cy,p['rot']); pts.append((p['x']+dx,p['y']+dy))
    rects.append((p['ref'],pts))
def sat(A,B):
    for poly in (A,B):
        for i in range(4):
            x1,y1=poly[i]; x2,y2=poly[(i+1)%4]; nx,ny=y2-y1,x1-x2
            pa=[nx*x+ny*y for x,y in A]; pb=[nx*x+ny*y for x,y in B]
            if max(pa)<=min(pb) or max(pb)<=min(pa): return False
    return True
hits=[]
for i,(ra,A) in enumerate(rects):
    for rb,B in rects[i+1:]:
        ax=[x for x,y in A]; bx=[x for x,y in B]
        if max(ax)<min(bx) or max(bx)<min(ax): continue
        ay=[y for x,y in A]; by=[y for x,y in B]
        if max(ay)<min(by) or max(by)<min(ay): continue
        if sat(A,B): hits.append((ra,rb))
print(len(hits)); 
from collections import Counter
print(hits[:80])
print({k:tuple(round(v,1) for v in cache[k]) for k in cache})
