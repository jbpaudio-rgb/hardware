#!/usr/bin/env python3
"""Reads how each plugin lays out its controls (web/*-web/*.html: its pages, sections and the order of the knobs) and
writes box/PluginPages.h, so the hardware synth shows the same controls in the same places.
It opens every plugin page in a headless browser (Playwright + Chromium), tags each widget with its parameter id
and reads the page back. Run it again when a plugin's layout changes:  python3 box/tools/plugin_pages.py"""
import asyncio, os, re
from playwright.async_api import async_playwright
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); W = os.path.join(ROOT, "web")
FILES = {'vact': 'vact-web/vact.html', 'mini': 'mini-web/mini.html', 'pro70': 'pro-web/pro.html', 'a26': 'arp-web/a26.html',
         'j8': 'poly-web/juno.html', 'cs8': 'poly-web/cs80.html', 'vc3': 'vcs-web/vc3.html', 'm20': 'ms-web/m20.html',
         'easel': 'easel-web/easel.html', 'dr8': 'drum-web/dr8.html', 'sp64': 'samp-web/sp64.html'}
# the plugin page that goes on the PAGE 2 button, and its name
PAGE2 = {'vact': 'SPECTRA', 'vc3': 'PATCH MATRIX', 'dr8': 'PATTERN'}
# plugin pages left off the hardware synth (the MINI's fixed filter bank)
SKIP = {('mini', 'spectra')}
WRAP = """
;try{fader}catch(_){window.fader=function(id,name){return knob(id,false,name)}};(function(){
const tag=(f,kind)=>function(id,...a){const e=f(id,...a);try{e.dataset.pid=id}catch(_){}return e};
for(const n of ['knob','toggle','select','bar','rotary','fader','esw','ck','seg'])try{const f=eval(n);if(typeof f==='function')eval(n+'=tag(f)')}catch(_){}})();
"""
READ = """()=>{const out=[];const eas=[...document.querySelectorAll('.easel')];
 document.querySelectorAll('.sec, .easel .emod, .easel .eswg').forEach(sec=>{const ez=sec.closest('.easel');let pg=sec.closest('[id]');
  while(pg&&!['voice','spectra','mod','seq','matrix','pattern','voices'].includes(pg.id)&&pg.parentElement)pg=pg.parentElement.closest('[id]');
  const h=sec.querySelector('h2, .etitle, .eswt');const ids=[...sec.querySelectorAll('[data-pid]')].map(e=>{const g=e.closest('.grp');const c=g&&g.querySelector(':scope>.cap');return [e.dataset.pid,c?c.textContent.trim():'']});
  out.push([ez?'voice':(pg?pg.id:'?'),h?h.textContent:'',ids])});return out}"""

# the FX plugin (web/fx-web/fx.html): for each effect and model, the controls it shows, in its order
FX_READ = """async ()=>{const out={};
 for(let f=0;f<16;f++){ setSlot(0,f+1); rebuildPages(); await new Promise(r=>setTimeout(r,20));
   const mid='q0f'+f+'model'; const nm=IDX[mid]!==undefined?Math.round(D[IDX[mid]].mx)+1:1; out[f]={};
   for(let m=0;m<nm;m++){
     const knobs=(MODELS[f]?FXK[f].filter(x=>ownsKnob(x[3],m)):FXK[f]).map(([k,n])=>['f'+f+k,n]); knobs.push(['f'+f+'mix','MIX']);
     const extra=(FXOWN[0][m]||(MODELS[f]?[]:FXOWN[0][0])||[]).filter(id=>!id.endsWith('model')).map(id=>{const e=document.querySelector('[data-p="'+id+'"]');const n=e&&e.querySelector('.nm');
        return [id.replace(/^q0/,''), n?n.textContent.trim():(D[IDX[id]]?D[IDX[id]].nm.replace(/^S1 /,''):id)]});
     const seen=new Set(knobs.map(x=>x[0])); out[f][m]=knobs.concat(extra.filter(x=>!seen.has(x[0]))); } }
 return out}"""

def tidy(t):   # a short section title: what comes before a long dash
    t = t.split(' — ')[0].strip()
    return re.sub(r'\s+', ' ', t)

async def read_all():
    res = {}
    async with async_playwright() as p:
        b = await p.chromium.launch()
        for k, f in FILES.items():
            s = open(os.path.join(W, f)).read()
            i = s.index('// ===================== Widgets'); j = s.index('// =====================', i + 40)
            tmp = f"/tmp/plugin_pages_{k}.html"; open(tmp, "w").write(s[:j] + WRAP + s[j:])
            pg = await b.new_page(); await pg.goto("file://" + tmp); await pg.wait_for_timeout(500)
            res[k] = await pg.evaluate(READ); await pg.close()
        pg = await b.new_page(); await pg.goto("file://" + os.path.join(W, "fx-web/fx.html")); await pg.wait_for_timeout(600)
        res['__fx'] = await pg.evaluate(FX_READ); await pg.close()
        await b.close()
    return res

def build(res):
    secs = []   # (engine, hardware page, title, ids)
    for eng, rows in res.items():
        if eng == '__fx': continue
        merged = []   # sections with the same title on one page are one section (Easel: its sliders, then its switches)
        for page, title, ids in rows:
            if (eng, page) in SKIP: continue
            hw = {'voice': 'VOICE', 'voices': 'VOICE', 'mod': 'MOD', 'seq': 'SEQ'}.get(page, 'PAGE2')
            title = tidy(title); ids = [i for i in ids if i[0] not in ('bpm', 'play', 'master')]
            if eng == 'dr8' and page == 'pattern' and title.startswith('PATTERN'):   # its step grid is drawn by hand
                for t in range(8):
                    merged.append(['PAGE2', 'TRACK %d' % (t + 1), [['p%d_%d' % (t, k), ''] for k in range(16)]])
                continue
            if eng == 'vc3' and page == 'matrix':   # 15 sources x 14 destinations
                src = re.findall(r"const PSRC=\[([^\]]*)\]", open(os.path.join(W, FILES['vc3'])).read())
                names = re.findall(r"'([^']*)'", src[0]) if src else ['SOURCE %d' % (s + 1) for s in range(15)]
                for s in range(15): merged.append(['PAGE2', names[s] if s < len(names) else 'SOURCE %d' % (s + 1), [['pin%d_%d' % (s, d), ''] for d in range(14)]])
                continue
            if not ids: continue
            for m in merged:
                if m[0] == hw and m[1] == title: m[2] += [i for i in ids if i[0] not in [x[0] for x in m[2]]]; break
            else: merged.append([hw, title, ids])
        # controls a plugin draws by hand (no widget to read): put them where the plugin shows them
        for m in merged:
            if eng == 'j8' and m[1].startswith('ASSIGN') and 'chorus' not in [x[0] for x in m[2]]: m[2].append(['chorus', ''])
            if eng == 'vc3' and m[1] == 'JOYSTICK': m[2] = [['jx', ''], ['jy', '']] + [i for i in m[2] if i[0] not in ('jx', 'jy')]
        if eng == 'sp64':
            merged.insert(0, ['VOICE', 'GLOBAL', [[i, ''] for i in ('tune', 'gcut', 'gdec', 'chrom')]])
        if eng == 'sp64':   # the pad editor, for the pad chosen with its first knob ("@" = the pad's number)
            pad = [('@pad', ''), ('k@src', 'SAMPLE'), ('k@st', 'SAMPLE'), ('k@en', 'SAMPLE'), ('k@rev', 'SAMPLE'), ('k@tune', 'PITCH & LEVEL'),
                   ('k@lvl', 'PITCH & LEVEL'), ('k@pan', 'PITCH & LEVEL'), ('k@vel', 'PITCH & LEVEL'), ('k@att', 'ENVELOPE'), ('k@dec', 'ENVELOPE'),
                   ('k@rel', 'ENVELOPE'), ('k@mode', 'ENVELOPE'), ('k@grp', 'ENVELOPE'), ('k@cut', 'FILTER'), ('k@res', 'FILTER')]
            merged.insert(0, ['VOICE', 'PAD', [list(x) for x in pad]])
        for hw, title, ids in merged: secs.append((eng, hw, title, ids))
    return secs

def write(secs, fx):
    out = ["// Generated by box/tools/plugin_pages.py from the plugins' own pages (web/*-web/*.html): do not edit.",
           "// Each engine's controls, page by page and section by section, in the plugin's order.",
           "// Pages: VOICE (the plugin's main page), PAGE2 (its own extra page), MOD, SEQ. \"k@src\": @ = the pad chosen (SAMPLER).",
           "#pragma once", "", "namespace box { namespace layout {", "",
           "// caps: the plugin's group caption of each control, separated by |",
           "struct Section { const char* engine; const char* page; const char* title; const char* ids; const char* caps; };", "",
           "static const Section SECTIONS[] = {"]
    for eng, hw, title, ids in secs:
        out.append('    { "%s", "%s", "%s", "%s", "%s" },' % (eng, hw, title.replace('"', '\\"'), " ".join(i[0] for i in ids), "|".join(i[1].replace('"', "'") for i in ids)))
    out += ["};", "", "// the name of PAGE2 for the engines that have one",
            "static const char* const PAGE2_NAME[][2] = {" + ", ".join('{ "%s", "%s" }' % kv for kv in PAGE2.items()) + " };", "",
            "// the FX plugin: each effect's controls for each of its models (ids without the slot's q<n> prefix, names by |)",
            "struct FxModel { int fx, model; const char* ids; const char* names; };",
            "static const FxModel FX_MODELS[] = {"] + ['    { %d, %d, "%s", "%s" },' % (int(f), int(m), " ".join(i for i, _ in v), "|".join(n.upper().replace('"', "'") for _, n in v))
                for f in sorted(fx, key=int) for m, v in sorted(fx[f].items(), key=lambda kv: int(kv[0]))] + ["};", "",
            "} } // namespace box::layout", ""]
    open(os.path.join(ROOT, "box", "PluginPages.h"), "w").write("\n".join(out))

if __name__ == "__main__":
    res = asyncio.run(read_all()); secs = build(res); write(secs, res['__fx'])
    from collections import Counter
    c = Counter((e, p) for e, p, _, _ in secs)
    for e in FILES: print("%-6s" % e, " ".join("%s %d" % (p, sum(len(ids) for ee, pp, _, ids in secs if ee == e and pp == p)) for p in ('VOICE', 'PAGE2', 'MOD', 'SEQ')))
