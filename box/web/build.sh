#!/usr/bin/env bash
# Builds octavio-box for the browser: box/web/octavio-box.wasm (wasi-sdk), then the page box/web/octavio-box.html
# (the module and the fonts are put inside it, so it is one file). Usage: WASI_SDK=/path/to/wasi-sdk box/web/build.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
SDK="${WASI_SDK:?set WASI_SDK to the wasi-sdk folder (https://github.com/WebAssembly/wasi-sdk/releases)}"
"$SDK/bin/clang++" --target=wasm32-wasip1 --sysroot="$SDK/share/wasi-sysroot" -std=c++17 -O3 -fno-exceptions \
  -mexec-model=reactor -Wl,--strip-all -Wl,--initial-memory=67108864 -Wl,--max-memory=1073741824 -Wl,--growable-table \
  -I. -Icore -o box/web/octavio-box.wasm box/web/boxwasm.cpp box/ui/Canvas.cpp
ls -l box/web/octavio-box.wasm
python3 box/web/page.py
