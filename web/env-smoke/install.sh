#!/usr/bin/env bash
#
# Cloud Agent install phase for the Linux Doom browser/WASM port.
#
# Idempotent: provisions the Emscripten SDK if it is not already present, then
# builds the environment smoke-test WASM module from the repository's real
# engine sources and verifies it headlessly. Safe to run repeatedly and safe
# on a fresh default image that has never seen emsdk.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EMSDK_DIR="${EMSDK_DIR:-$HOME/emsdk}"
EMSDK_VERSION="3.1.74"

echo "==> Ensuring Emscripten SDK ${EMSDK_VERSION} is installed at ${EMSDK_DIR}"
if [ ! -f "$EMSDK_DIR/emsdk_env.sh" ]; then
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
"$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
"$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh"

echo "==> Toolchain versions"
emcc --version | head -1
node --version
python3 --version

echo "==> Building WASM smoke module from real engine sources"
bash "$ROOT/web/env-smoke/build.sh"

echo "==> Headless verification"
node "$ROOT/web/env-smoke/node_check.mjs"

echo "==> Install complete. Serve with: cd web && python3 -m http.server 8000"
