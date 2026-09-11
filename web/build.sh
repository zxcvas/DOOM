#!/usr/bin/env bash
#
# Build Linux Doom 1.10 to WebAssembly (Phase 0/1 browser port).
#
# Produces web/dist/doom.{js,wasm}. No IWAD is bundled: the page loads a
# user-supplied WAD at runtime (see docs/linuxdoom-browser-port.md, AGENTS.md).
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC="$ROOT/linuxdoom-1.10"
NATIVE="$HERE/native"
DIST="$HERE/dist"

if ! command -v emcc >/dev/null 2>&1; then
    echo "emcc not found. Activate the Emscripten SDK first:" >&2
    echo "  source \"\$HOME/emsdk/emsdk_env.sh\"" >&2
    exit 1
fi

mkdir -p "$DIST"

# Original engine sources, minus the Linux/X11 platform files we replace with
# the browser platform layer in web/native/.
EXCLUDE="i_system.c i_video.c i_net.c i_sound.c"
GAME_SRCS=()
for f in "$SRC"/*.c; do
    base="$(basename "$f")"
    skip=0
    for e in $EXCLUDE; do
        [ "$base" = "$e" ] && skip=1
    done
    [ "$skip" -eq 0 ] && GAME_SRCS+=("$f")
done

NATIVE_SRCS=(
    "$NATIVE/i_system_web.c"
    "$NATIVE/i_video_web.c"
    "$NATIVE/i_net_web.c"
    "$NATIVE/i_sound_web.c"
)

# Match the historical Makefile's macros so the Unix code paths compile.
DEFINES="-DNORMALUNIX -DLINUX"

# The 1997 sources predate modern C rules: tentative common globals, implicit
# declarations, and loose pointer/int conversions. Keep them as warnings.
COMPAT="-std=gnu11 \
  -fcommon \
  -Wno-implicit-function-declaration \
  -Wno-implicit-int \
  -Wno-int-conversion \
  -Wno-incompatible-pointer-types \
  -Wno-error"

EXPORTS="_main,_D_DoomFrame,_Doom_FrameBuffer,_Doom_Palette,_Doom_PaletteVersion,_Doom_Width,_Doom_Height,_Doom_PostKey,_Doom_PostMouse,_malloc,_free"

RUNTIME_METHODS="callMain,ccall,cwrap,FS,HEAPU8,ENV"

emcc \
    -O2 \
    $DEFINES \
    $COMPAT \
    -I"$SRC" \
    "${GAME_SRCS[@]}" \
    "${NATIVE_SRCS[@]}" \
    -sWASM=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=134217728 \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sASSERTIONS=1 \
    -sEXPORTED_FUNCTIONS="[$EXPORTS]" \
    -sEXPORTED_RUNTIME_METHODS="[$RUNTIME_METHODS]" \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createDoomModule \
    -sENVIRONMENT=web \
    -o "$DIST/doom.js"

echo "Built: $DIST/doom.js + $DIST/doom.wasm"
