#!/usr/bin/env python3
"""Puts the WebAssembly module, the fonts and the logo into page.html -> octavio-box.html (one self-contained file)."""
import base64, os
here = os.path.dirname(os.path.abspath(__file__)); box = os.path.dirname(here); root = os.path.dirname(box)
b64 = lambda p: base64.b64encode(open(p, "rb").read()).decode()
logo = open(os.path.join(root, "web/octavio-app/logo-only.svg")).read().replace("<svg ", '<svg style="width:100%;height:auto;display:block" ', 1)
html = open(os.path.join(here, "page.html")).read()
html = html.replace("__WASM__", b64(os.path.join(here, "octavio-box.wasm")))
html = html.replace("__FONTR__", b64(os.path.join(box, "third_party/fonts/DejaVuSansCondensed.ttf")))
html = html.replace("__FONTB__", b64(os.path.join(box, "third_party/fonts/DejaVuSansCondensed-Bold.ttf")))
html = html.replace("__LOGO__", logo)
open(os.path.join(here, "octavio-box.html"), "w").write(html)
print("octavio-box.html", round(len(html) / 1e6, 1), "MB")
