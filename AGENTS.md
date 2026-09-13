# Agent instructions

This repository is the 1997 Linux Doom 1.10 source (GPL-2.0). Game IWAD data
is **not** in the tree and must never be committed.

Read `docs/linuxdoom-browser-port.md` before changing behavior.

## Invariants

- Keep the software renderer (`linuxdoom-1.10/r_*.c`) as the source of pixels.
- Present is an 8-bit paletted 320×200 buffer (`screens[0]` + `PLAYPAL`).
- Modern effects belong in a WebGL/WebGPU compositor after palette expand.
- Do not host or add commercial WAD files.
- Do not reintroduce a blocking `while (1)` game loop or busy-wait wipes in
  the browser build.

## Cursor Cloud specific instructions

- Build and test only the documented web/WASM path once it exists; the original
  Linux/X11 binary is not expected to compile in a typical Cloud VM (no X11
  development headers required for the browser port).
- Specialist subagents live in `.cursor/agents/`. Prefer `/wasm-platform`,
  `/pixel-compositor`, and `/browser-verifier` over a single mixed change.
- If an IWAD is needed for a runtime test, use a user-supplied path or skip
 the playable test and report that it was not run.

## Development environment (browser/WASM)

- `.cursor/environment.json` provisions the Emscripten toolchain and serves
 the `web/` shell. Its `install` runs `web/env-smoke/install.sh` (installs
 `emsdk` if missing, builds, and headlessly verifies). A `web-server`
 terminal serves `web/` on port 8000.
- Locally, activate the toolchain with `source "$HOME/emsdk/emsdk_env.sh"`.
- Build the WASM module: `bash web/env-smoke/build.sh` (output in `web/dist/`,
 which is git-ignored). Headless check: `node web/env-smoke/node_check.mjs`.
- `web/env-smoke/` is an environment smoke test only: it compiles the real,
 unmodified engine math (`m_fixed.c`, `tables.c`, `m_random.c`) to WASM to
 prove the toolchain. It is not the game and must not stand in for the port
 or the software renderer.
