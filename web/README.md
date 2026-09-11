# Doom in the browser (WASM)

Phase 0/1 of the browser port described in
[`docs/linuxdoom-browser-port.md`](../docs/linuxdoom-browser-port.md): the
original software renderer is compiled to WebAssembly and its 320×200 8-bit
paletted framebuffer is presented to a `<canvas>`. The Linux/X11 platform files
are replaced by a small browser platform layer in `native/`.

No game data is bundled. Supply an IWAD you own (`doom.wad`, `doom2.wad`,
shareware `doom1.wad`, …) or a freely licensed one such as Freedoom.

## Layout

| Path | Role |
|------|------|
| `native/i_video_web.c` | Present hook: exposes `screens[0]` + gamma-corrected `PLAYPAL`; DOM input → `D_PostEvent`. |
| `native/i_system_web.c` | Timing from the browser clock; no busy-wait; `I_Error`/`I_Quit` map to JS. |
| `native/i_sound_web.c` | Silent sound stub (Web Audio is Phase 3). |
| `native/i_net_web.c` | Single-node network stub so `TryRunTics` still runs. |
| `compositor.js` | Phase 1 present: CPU palette-expand → integer upscale → 4:3 (WebGL LUT is Phase 2). |
| `input.js` | Browser key codes → Doom key codes. |
| `main.js` | Boots the module, loads the IWAD into MEMFS, drives `D_DoomFrame` from `requestAnimationFrame`. |
| `build.sh` | Emscripten build → `dist/doom.{js,wasm}`. |

The blocking `while (1)` game loop and busy-wait screen wipes were replaced by a
non-blocking `D_DoomFrame` (see the `__EMSCRIPTEN__` paths in
`linuxdoom-1.10/d_main.c`). The software renderer (`r_*.c`) is untouched.

## Build

Requires the Emscripten SDK (`emcc` on `PATH`):

```sh
# one-time toolchain (or use the .cursor Docker image)
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh

bash web/build.sh          # -> web/dist/doom.js + web/dist/doom.wasm
```

## Run

```sh
cd web && python3 -m http.server 8000
```

Open <http://localhost:8000/>, then either pick an IWAD with the file chooser or
auto-load one you self-host:

```
http://localhost:8000/?wad=your-iwad.wad
```

Controls: arrows move/turn, `Ctrl` fire, `Space` use, `Shift` run, `Alt`+arrows
strafe, `1`–`7` weapons, `Esc` menu, `Tab` map.
