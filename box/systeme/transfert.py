#!/usr/bin/env python3
"""Octavio box: the file transfer page, over the USB-C cable (USB gadget network) to a computer.

With the box plugged into a computer, open http://octavio.local (or http://10.55.0.1) in a browser:
  - PRESETS: the multis (*.oct in ~/octavio): download one or all (zip), upload, delete;
  - SAMPLES: the WAV files of the SAMPLER (~/octavio/samples, folders allowed): upload (drag and drop, several at
    once, folders too), download, delete.
The box sees the new files at once (its lists read the folders each time). Only the standard library: no install.
Answers only on the USB link (usb0) unless --any is given, so the page is not open on a Wi-Fi network.
    transfert.py [--root ~/octavio] [--port 80] [--any]
"""
import argparse, html, io, json, os, shutil, subprocess, sys, urllib.parse, zipfile
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

KINDS = {"presets": (".oct",), "samples": (".wav",)}
MAX_UPLOAD = 512 * 1024 * 1024   # 512 MB a file

def usb_addresses():
    """The box's own IPv4 addresses on the USB gadget link(s)."""
    out = set()
    try:
        txt = subprocess.run(["ip", "-4", "-o", "addr", "show"], capture_output=True, text=True, timeout=2).stdout
        for line in txt.splitlines():
            w = line.split()
            if len(w) > 3 and (w[1].startswith("usb") or w[1].startswith("ncm")): out.add(w[3].split("/")[0])
    except Exception:
        pass
    return out

class Store:
    def __init__(self, root):
        self.root = os.path.abspath(os.path.expanduser(root))
        os.makedirs(os.path.join(self.root, "samples"), exist_ok=True)
    def base(self, kind):
        return self.root if kind == "presets" else os.path.join(self.root, "samples")
    def path(self, kind, rel):
        """A safe absolute path for a file of this kind, or None (no '..', the right extension, presets flat)."""
        if kind not in KINDS: return None
        rel = rel.replace("\\", "/").strip("/")
        parts = [p for p in rel.split("/") if p not in ("", ".")]
        if not parts or any(p == ".." or p.startswith(".") for p in parts): return None
        if kind == "presets" and len(parts) != 1: return None
        if not parts[-1].lower().endswith(KINDS[kind]): return None
        full = os.path.abspath(os.path.join(self.base(kind), *parts))
        return full if full.startswith(self.base(kind) + os.sep) else None
    def list(self):
        pres = sorted(({"name": f, "size": os.path.getsize(os.path.join(self.root, f))}
                       for f in os.listdir(self.root) if f.lower().endswith(".oct") and not f.startswith(".")), key=lambda x: x["name"].lower())
        smp, b = [], self.base("samples")
        for d, dirs, files in os.walk(b):
            dirs[:] = sorted(x for x in dirs if not x.startswith("."))
            for f in sorted(files, key=str.lower):
                if f.lower().endswith(".wav") and not f.startswith("."):
                    full = os.path.join(d, f); smp.append({"name": os.path.relpath(full, b).replace(os.sep, "/"), "size": os.path.getsize(full)})
        du = shutil.disk_usage(self.root)
        return {"presets": pres, "samples": smp, "free": du.free, "total": du.total}

PAGE = r"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Octavio · files</title><style>
:root{color-scheme:dark}body{margin:0;background:#101012;color:#eee;font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
main{max-width:860px;margin:0 auto;padding:28px 20px 60px}h1{font-size:22px;letter-spacing:.04em;margin:0 0 4px}.sub{color:#8a8a90;margin:0 0 22px}
section{background:#18181b;border:1px solid #26262b;border-radius:14px;padding:16px 18px;margin:0 0 18px}
h2{font-size:15px;letter-spacing:.08em;margin:0;color:#ff7a1a}.row{display:flex;align-items:center;gap:10px;justify-content:space-between;margin-bottom:10px}
.drop{border:2px dashed #34343a;border-radius:12px;padding:16px;text-align:center;color:#8a8a90;margin:10px 0;cursor:pointer}.drop.on{border-color:#ff7a1a;color:#eee}
table{width:100%;border-collapse:collapse}td{padding:7px 4px;border-top:1px solid #222227}td.s{color:#8a8a90;text-align:right;white-space:nowrap;width:90px}
td.a{text-align:right;white-space:nowrap;width:140px}a,button{font:inherit}button,.btn{background:#26262b;color:#eee;border:0;border-radius:8px;padding:5px 10px;cursor:pointer;text-decoration:none}
button.del{background:transparent;color:#8a8a90}button:hover,.btn:hover{background:#34343a}.msg{min-height:20px;color:#8a8a90}.bar{height:4px;background:#26262b;border-radius:2px;overflow:hidden}.bar i{display:block;height:100%;background:#ff7a1a;width:0}
.empty{color:#6a6a70;padding:8px 4px}</style></head><body><main>
<h1>OCTAVIO · FILES</h1><p class="sub">The box's SD card, over the USB cable. New files show up on the box at once.</p>
<section><div class="row"><h2>PRESETS · ALL TRACKS</h2><a class="btn" href="/zip/presets">Download all (.zip)</a></div>
<div class="drop" data-kind="presets">Drop .oct presets here, or click to choose</div><table id="presets"></table></section>
<section><div class="row"><h2>SAMPLES</h2><span id="free" class="sub" style="margin:0"></span></div>
<div class="drop" data-kind="samples">Drop WAV files or folders here, or click to choose</div><table id="samples"></table></section>
<div class="bar"><i id="bar"></i></div><p class="msg" id="msg"></p></main>
<input type="file" id="pick" multiple hidden>
<script>
const $=s=>document.querySelector(s), fmt=n=>n<1e6?(n/1e3).toFixed(0)+' kB':n<1e9?(n/1e6).toFixed(1)+' MB':(n/1e9).toFixed(2)+' GB';
const enc=p=>p.split('/').map(encodeURIComponent).join('/');
async function load(){const r=await fetch('/api/list');const d=await r.json();
 for(const k of ['presets','samples']){const t=$('#'+k);t.innerHTML='';
  if(!d[k].length){t.innerHTML='<tr><td class="empty">Nothing yet.</td></tr>';continue}
  for(const f of d[k]){const tr=document.createElement('tr');
   tr.innerHTML=`<td></td><td class="s">${fmt(f.size)}</td><td class="a"><a class="btn" href="/dl/${k}/${enc(f.name)}" download>Download</a> <button class="del">Delete</button></td>`;
   tr.firstChild.textContent=f.name;tr.querySelector('.del').onclick=async()=>{if(!confirm('Delete '+f.name+'?'))return;await fetch(`/del/${k}/${enc(f.name)}`,{method:'DELETE'});load()};t.appendChild(tr)}}
 $('#free').textContent=fmt(d.free)+' free of '+fmt(d.total)}
async function walk(entry,path,out){if(entry.isFile){await new Promise(r=>entry.file(f=>{out.push([path+f.name,f]);r()},r));return}
 const rd=entry.createReader();let all=[];for(;;){const b=await new Promise(r=>rd.readEntries(r,()=>r([])));if(!b.length)break;all=all.concat(b)}
 for(const e of all)await walk(e,path+entry.name+'/',out)}
async function send(kind,files){const ext=kind==='presets'?'.oct':'.wav';files=files.filter(([n])=>n.toLowerCase().endsWith(ext));
 if(!files.length){$('#msg').textContent='Only '+ext+' files go there.';return}
 let total=files.reduce((a,[,f])=>a+f.size,0),done=0,ok=0;
 for(const [name,f] of files){$('#msg').textContent='Sending '+name+'…';
  const r=await fetch(`/up/${kind}/${enc(kind==='presets'?name.split('/').pop():name)}`,{method:'PUT',body:f});
  if(r.ok)ok++;else $('#msg').textContent=name+': '+await r.text();done+=f.size;$('#bar').style.width=(100*done/total)+'%'}
 $('#msg').textContent=ok+' of '+files.length+' file(s) sent.';setTimeout(()=>$('#bar').style.width='0',800);load()}
let pickKind='samples';$('#pick').onchange=e=>{send(pickKind,[...e.target.files].map(f=>[f.webkitRelativePath||f.name,f]));e.target.value=''};
document.querySelectorAll('.drop').forEach(d=>{const k=d.dataset.kind;
 d.onclick=()=>{pickKind=k;$('#pick').click()};
 d.ondragover=e=>{e.preventDefault();d.classList.add('on')};d.ondragleave=()=>d.classList.remove('on');
 d.ondrop=async e=>{e.preventDefault();d.classList.remove('on');const out=[];
  for(const it of e.dataTransfer.items){const en=it.webkitGetAsEntry&&it.webkitGetAsEntry();if(en)await walk(en,'',out);else{const f=it.getAsFile();if(f)out.push([f.name,f])}}
  send(k,out)}});
load();
</script></body></html>"""

class Handler(BaseHTTPRequestHandler):
    server_version = "Octavio/1"
    def allowed(self):
        if self.server.any: return True
        local = self.connection.getsockname()[0]
        return local in usb_addresses() or local.startswith("127.")
    def reply(self, code, body=b"", ctype="text/plain; charset=utf-8", extra=None):
        if isinstance(body, str): body = body.encode()
        self.send_response(code); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        for k, v in (extra or {}).items(): self.send_header(k, v)
        self.end_headers(); self.wfile.write(body)
    def route(self):
        u = urllib.parse.urlparse(self.path); parts = u.path.split("/", 3)
        return parts[1] if len(parts) > 1 else "", parts[2] if len(parts) > 2 else "", urllib.parse.unquote(parts[3]) if len(parts) > 3 else ""
    def do_GET(self):
        if not self.allowed(): return self.reply(403, "USB only")
        what, kind, rel = self.route(); st = self.server.store
        if what == "": return self.reply(200, PAGE, "text/html; charset=utf-8")
        if what == "api" and kind == "list": return self.reply(200, json.dumps(st.list()), "application/json")
        if what == "zip" and kind == "presets":
            buf = io.BytesIO()
            with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
                for f in st.list()["presets"]: z.write(os.path.join(st.root, f["name"]), f["name"])
            return self.reply(200, buf.getvalue(), "application/zip", {"Content-Disposition": 'attachment; filename="octavio-presets.zip"'})
        if what == "dl":
            p = st.path(kind, rel)
            if not p or not os.path.isfile(p): return self.reply(404, "not found")
            self.send_response(200); self.send_header("Content-Type", "application/octet-stream"); self.send_header("Content-Length", str(os.path.getsize(p)))
            self.send_header("Content-Disposition", "attachment; filename*=UTF-8''" + urllib.parse.quote(os.path.basename(p))); self.end_headers()
            with open(p, "rb") as f: shutil.copyfileobj(f, self.wfile)
            return
        self.reply(404, "not found")
    def do_PUT(self):
        if not self.allowed(): return self.reply(403, "USB only")
        what, kind, rel = self.route(); p = self.server.store.path(kind, rel) if what == "up" else None
        if not p: return self.reply(400, "only .oct presets and .wav samples")
        n = int(self.headers.get("Content-Length") or 0)
        if n <= 0 or n > MAX_UPLOAD: return self.reply(413, "empty or too big")
        if shutil.disk_usage(self.server.store.root).free < n + 50 * 1024 * 1024: return self.reply(507, "the SD card is full")
        os.makedirs(os.path.dirname(p), exist_ok=True); tmp = p + ".part"
        with open(tmp, "wb") as f:   # written aside, then renamed: the box never reads a half file
            left = n
            while left > 0:
                b = self.rfile.read(min(left, 1 << 20))
                if not b: break
                f.write(b); left -= len(b)
            f.flush(); os.fsync(f.fileno())
        if left: os.remove(tmp); return self.reply(400, "cut short")
        os.replace(tmp, p); self.reply(200, "ok")
    def do_DELETE(self):
        if not self.allowed(): return self.reply(403, "USB only")
        what, kind, rel = self.route(); p = self.server.store.path(kind, rel) if what == "del" else None
        if not p or not os.path.isfile(p): return self.reply(404, "not found")
        os.remove(p)
        d = os.path.dirname(p)   # empty sample folders go too
        while kind == "samples" and d != self.server.store.base("samples") and not os.listdir(d): os.rmdir(d); d = os.path.dirname(d)
        self.reply(200, "ok")
    def log_message(self, fmt, *a): pass

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--root", default="~/octavio"); ap.add_argument("--port", type=int, default=80)
    ap.add_argument("--any", action="store_true", help="answer on every network, not only the USB link")
    a = ap.parse_args()
    srv = ThreadingHTTPServer(("0.0.0.0", a.port), Handler); srv.store = Store(a.root); srv.any = a.any
    print(f"transfert: http://octavio.local:{a.port} ({'any network' if a.any else 'USB only'}), {srv.store.root}", flush=True)
    srv.serve_forever()

if __name__ == "__main__":
    main()
