#!/usr/bin/env bash
#
# Build the environment smoke-test WASM module from the repository's real,
# unmodified engine sources. Emits web/dist/doom_smoke.{js,wasm}.
#
# Requires emcc on PATH (provided by the Cloud Agent environment; locally,
# `source "$HOME/emsdk/emsdk_env.sh"` first).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
ENGINE="$ROOT/linuxdoom-1.10"
OUT="$ROOT/web/dist"

if ! command -v emcc >/dev/null 2>&1; then
    echo "error: emcc not found on PATH. Install/activate emsdk first." >&2
    exit 1
fi

mkdir -p "$OUT"

# Note: no -DLINUX. That macro makes doomtype.h include <values.h> and
# tables.h include <math.h>, neither of which we need for the smoke test.
emcc \
    -O2 \
    -I"$ENGINE" \
    "$HERE/doom_smoke.c" \
    "$ENGINE/m_fixed.c" \
    "$ENGINE/tables.c" \
    "$ENGINE/m_random.c" \
    -sMODULARIZE=1 \
    -sEXPORT_ES6=1 \
    -sEXPORT_NAME=createDoomSmoke \
    -sENVIRONMENT=web,node \
    -sALLOW_MEMORY_GROWTH=1 \
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8 \
    -sEXPORTED_FUNCTIONS=_selftest,_init_palette,_render_frame,_get_framebuffer,_get_palette,_framebuffer_width,_framebuffer_height \
    -o "$OUT/doom_smoke.js"

echo "built: $OUT/doom_smoke.js ($(du -h "$OUT/doom_smoke.wasm" | cut -f1) wasm)"
