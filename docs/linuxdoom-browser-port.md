# Linux Doom 1.10 — Architecture, Browser Port, and Agent Plan

This document records an analysis of the id Software Linux Doom 1.10 source
in this repository, a concrete plan to run it in a browser while keeping the
original pixel look, a map for adding modern post-process effects, and a
walkthrough for creating the Cursor agents that should do the work.

**Audience:** engineers and Cursor agents who will port, mod, or present this
engine.

**Non-goals:** rewriting the software renderer into a GPU world renderer,
replacing the 35 Hz tic model, or shipping commercial IWAD data.

---

## 1. Executive summary

This tree is the 23 December 1997 public release of Doom, cleaned for Linux/X11
by Bernd Kreimeier. The engine is already split into a portable game/renderer
core and a thin `I_*` platform layer. That split is the whole porting story.

The recommended browser architecture is:

1. Compile the portable C (simulation + software renderer + WAD/UI) to WebAssembly.
2. Replace Linux/X11 `I_video`, `I_sound`, `I_net`, `I_system` timing, and the
   blocking `D_DoomLoop` with a `requestAnimationFrame` host.
3. Keep writing a **320×200 8-bit paletted framebuffer**.
4. Upload that index buffer plus the current `PLAYPAL` to WebGL/WebGPU, expand
   to RGB on the GPU, integer-upscale with nearest sampling, then run modern
   effects (CRT, bloom, color grading) as a compositor.

The original pixel graphics stay authentic because colormap lighting, fuzz
shadows, palette flashes, and patch drawing remain CPU software. Effects live
*after* that buffer is complete.

Source is GPL-2.0. Game assets are **not** in this tree. The engine will not
start without a user-supplied IWAD (`doom.wad`, `doom2.wad`, shareware
`doom1.wad`, etc.).

---

## 2. What this repository is

John Carmack’s release note (`README.TXT`) is still the best orientation:

- Linux only in this dump (DOS sound library was copyrighted; Win32 port was
  not included).
- The code is portable behind `I_*`.
- The software renderer is “horizontal and vertical lines of constant Z with
  fixed light shading per band.”
- Suggested experiments already include ports, extra rendering features,
  gameplay features, and a 3D-accelerated version.

Carmack also sketched a better renderer (floors as polygons, single front-to-back
BSP walk, sprites clipped into subsectors). That is a different project. For a
pixel-preserving browser build, **do not take that fork**.

| Path | Role |
|------|------|
| `linuxdoom-1.10/` | Game engine. This is the code to port. |
| `sndserv/` | Separate Linux OSS sound-server process. Drop for browser. |
| `sersrc/` | DOS serial/modem driver. Not used by the Linux build. |
| `ipx/` | DOS IPX driver. Not used by the Linux build. |
| `LICENSE.TXT` | GNU GPL 2.0. |

The Linux binary is built by `linuxdoom-1.10/Makefile` as `linux/linuxxdoom`,
linking X11 and MIT-SHM (`-lXext -lX11 -lnsl -lm`).

---

## 3. Layered architecture

Doom never talks to X11, OSS, or sockets from game code. Everything goes
through events, ticcmds, an indexed framebuffer, and `I_*` calls.

```
┌─────────────────────────────────────────────────────────────┐
│ UI / presentation                                            │
│  m_menu, st_stuff, hu_stuff, am_map, wi_stuff, f_finale/wipe │
│  v_video (patches → screens[]), r_* (software renderer)      │
├─────────────────────────────────────────────────────────────┤
│ Game logic                                                   │
│  g_game, p_user, p_pspr, p_enemy, p_inter, p_map, p_mobj,    │
│  p_spec/*, info.c (states, actors, weapons)                  │
├─────────────────────────────────────────────────────────────┤
│ Engine core (keep as C → WASM)                               │
│  d_main, d_net (TryRunTics), p_tick, p_setup, w_wad, z_zone, │
│  s_sound, m_fixed, tables, m_random                          │
├─────────────────────────────────────────────────────────────┤
│ Platform I_*  — rewrite for the browser                      │
│  i_video (X11 PseudoColor + MIT-SHM)                         │
│  i_sound (sndserver popen or OSS /dev/dsp)                   │
│  i_net (UDP sockets)                                         │
│  i_system (gettimeofday, 6MB zone, usleep)                   │
│  i_main (calls D_DoomMain)                                   │
└─────────────────────────────────────────────────────────────┘
```

**Contract:** platform posts `event_t`, game builds `ticcmd_t`, renderer writes
`screens[0]` (320×200×8), platform presents and mixes audio.

---

## 4. The 35 Hz tic loop

### Entry

`i_main.c` is three assignments and `D_DoomMain()`. Startup (`d_main.c`)
identifies the IWAD, parses argv, then:

`V_Init` → `M_LoadDefaults` → `Z_Init` → `W_InitMultipleFiles` → `M_Init` →
`R_Init` → `P_Init` → `I_Init` → `D_CheckNetGame` → `S_Init` → `HU_Init` →
`ST_Init` → demo/title/`G_InitNew` → **`D_DoomLoop()`**.

### Outer loop (blocking today)

```
while (1) {
    I_StartFrame();
    if (singletics) { /* one tic, for profiling */ }
    else            TryRunTics();          // at least one 35 Hz tic
    S_UpdateSounds(players[consoleplayer].mo);
    D_Display();                           // draw + I_FinishUpdate
    I_UpdateSound() / I_SubmitSound();     // unless SNDSERV
}
```

`TICRATE` is 35 (`doomdef.h`). `TryRunTics` (`d_net.c`) uses `I_GetTime()`,
builds local ticcmds via `NetUpdate`, and runs `M_Ticker` + `G_Ticker` until
simulation catches wall-clock time. Single-player still uses this path with
`numnodes == 1`.

### Input → ticcmd → world

1. Platform calls `D_PostEvent` into a 64-slot ring (`d_event.h`).
2. `D_ProcessEvents`: `M_Responder` (menus) then `G_Responder`.
3. `G_BuildTiccmd` samples keys/mouse/joy into one `ticcmd_t` for `maketic`.
4. `G_Ticker` copies `netcmds[][]` into `players[i].cmd` and ticks the state.
5. `P_Ticker` runs player think, the thinker list, and specials.

`ticcmd_t` (`d_ticcmd.h`) is the deterministic wire format: forward/side move,
angle turn, buttons, chat, consistancy checksum. Same cmds + RNG seed → same
demo / net game.

### Thinkers

`thinker_t` is a doubly linked list with an `actionf_t` union. `mobj_t`
embeds it first. Default actor thinker is `P_MobjThinker` (momentum, state
machine, nightmare respawn). Weapons use `acp2(player, psp)` from `info.c`
states via `p_pspr.c`.

### Display order (`D_Display`)

For a normal level frame:

1. Automap **or** status bar (`ST_Drawer`) — bar lives in the bottom 32 pixels
   unless fullscreen (`viewheight == 200`).
2. `I_UpdateNoBlit` (empty on Linux).
3. `R_RenderPlayerView` — 3D view into the view window.
4. `HU_Drawer` — messages / chat.
5. Pause patch, then `M_Drawer` on top of everything.
6. `I_FinishUpdate` — present.

Screen wipes (`f_wipe.c`) **busy-wait** on `I_GetTime` inside `D_Display`.
That must be sliced across frames in a browser or the tab freezes.

---

## 5. Software renderer — the pixel look

The renderer is a classic front-to-back BSP software rasterizer that writes
**palette indices**, not RGB.

### Frame (`R_RenderPlayerView` in `r_main.c`)

```
R_SetupFrame
R_ClearClipSegs / DrawSegs / Planes / Sprites
R_RenderBSPNode(numnodes-1)   // walls drawn now; floors/sprites collected
R_DrawPlanes()                // floors, ceilings, sky
R_DrawMasked()                // sprites, masked midtextures, weapon
```

BSP walk (`r_bsp.c`): recurse front child; maybe back child if bbox is visible.
At a subsector: find visplanes, queue sprites, clip segs, draw wall columns
immediately (`R_RenderSegLoop` in `r_segs.c`).

Floors are **not polygons**. They are constant-Z horizontal spans recorded as
per-column openings on visplanes (`r_plane.c`) and later drawn by `R_DrawSpan`.
Sky is special-cased as wall columns from `skytexture`.

Sprites (`r_things.c`) are painter-sorted, clipped against `drawseg_t`
silhouettes, and drawn as masked posts.

### Pixel format

| Item | Value |
|------|--------|
| Logical resolution | `SCREENWIDTH` 320 × `SCREENHEIGHT` 200 |
| Buffer | `byte screens[0][320*200]` — one palette index per pixel |
| Palette lump | `PLAYPAL` — 256 RGB triples, plus pain/bonus/radiation palettes |
| Lighting | `COLORMAP` — 32 light tables × 256 bytes (index → darker index) |
| Present (Linux) | X11 8-bit PseudoColor; `I_SetPalette` → `XStoreColors` |

Column drawing (`r_draw.c`) is the look:

```c
*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
dest += SCREENWIDTH;
```

Fuzz (spectre), translations (player colors), and invulnerability inverse
colormap are all index remaps. There is no RGB math in the refresh.

`V_Init` allocates four 320×200 screens (front, back/border, two wipe
buffers). Status bar allocates another. Linux `I_InitGraphics` can point
`screens[0]` at the X image.

### Hard caps (why 320×200 stays)

- `visplane_t` embeds `top[SCREENWIDTH]` / `bottom[SCREENWIDTH]`.
- `MAXVISPLANES` 128, `MAXDRAWSEGS` 256, `MAXVISSPRITES` 128.
- HUD, menus, and status-bar patches are authored for this scale.

Carmack and the `TODO` file both say raising resolution in this software path
is educational but painful; GLDoom was the intended way out. For this project,
**freeze 320×200 as the simulation framebuffer** and upscale in the compositor.

### Aspect

`INV_ASPECT_RATIO` is 0.625. 320×200 was shown on 4:3 monitors, so the
original pixels are rectangular. Integer-scale then letterbox/pillarbox to 4:3
(e.g. 1280×800 logical pixels stretched onto a 4:3 viewport), not a square-pixel
320×200.

---

## 6. Platform contract (`I_*`)

These headers are the ABI the WASM host must implement. Keep the `.h` files;
replace the `.c` files.

### Video — `i_video.h`

| Function | Meaning |
|----------|---------|
| `I_InitGraphics` | Create the present surface (today: X11 window + SHM image). |
| `I_ShutdownGraphics` | Tear down. |
| `I_SetPalette(byte* pal)` | 256 RGB triples, already 8-bit. Apply gamma here today. |
| `I_UpdateNoBlit` | No-op on Linux; optional sync point. |
| `I_FinishUpdate` | **Present hook.** Blit `screens[0]`. |
| `I_ReadScreen` | Copy for wipes/screenshots. |

`I_FinishUpdate` currently does optional CPU nearest multiply (`-2`/`-3`/`-4`)
then `XShmPutImage`. In the browser it should upload indices + palette and
return immediately.

### System — `i_system.h`

| Function | Meaning |
|----------|---------|
| `I_GetTime` | Integer tics at `TICRATE` (35). Drive from `performance.now()`. |
| `I_ZoneBase` | ~6MB for `z_zone`. `malloc` is fine. |
| `I_StartFrame` / `I_StartTic` | Pump input into `D_PostEvent`. |
| `I_Quit` / `I_Error` | Map to JS (don’t `exit()` the tab). |

### Sound — `i_sound.h`

Linux default is `#define SNDSERV 1`: `popen("./sndserver")` and a text
protocol. The in-process OSS mixer is experimental. Browser: disable
`SNDSERV`, keep `s_sound.c` (channels, distance, sep), implement
`I_StartSound` / mix / submit with Web Audio. Music is largely stubbed on
Linux (“No music with Linux”) — optional later (WebAudio + MUS/MIDI or
pre-rendered).

### Net — `i_net.h`

`d_net.c` is portable. `i_net.c` is UDP. For v1, stub a single-node
`doomcom` so `TryRunTics` still runs. Multiplayer would be a new transport
(WebRTC/WebSocket) behind `I_NetCmd`.

### Files

`w_wad.c` uses POSIX `open`/`read`/`lseek`. Emscripten MEMFS/IDBFS or a
memory-backed WAD (`W_AddFile` from an ArrayBuffer) is enough. Zone memory
(`z_zone.c`) stays.

---

## 7. Assets, IWAD, license

- **Source:** GPL-2.0 (`LICENSE.TXT`). Carmack’s original note still says you
  need real Doom data.
- **IWAD search** (`IdentifyVersion` in `d_main.c`): `doom2.wad`, `doom2f.wad`,
  `plutonia.wad`, `tnt.wad`, `doomu.wad`, `doom.wad`, shareware `doom1.wad`,
  plus `$DOOMWADDIR`.
- **Do not** host or commit commercial IWADs. The web shell should prompt the
  user to drop in a WAD they own (or a freely licensed IWAD if you retarget).
- PWADs (`-file`) still work once `W_InitMultipleFiles` can see them in the
  virtual FS.

WAD contents the renderer cares about: `PLAYPAL`, `COLORMAP`, `PNAMES`,
`TEXTURE1`/`TEXTURE2`, `F_START`/`F_END` flats, `S_START`/`S_END` sprites,
map lumps (`VERTEXES`, `LINEDEFS`, `SIDEDEFS`, `SECTORS`, `SSECTORS`,
`NODES`, `SEGS`, `REJECT`, `BLOCKMAP`, `THINGS`).

---

## 8. Browser port strategy

### What to keep as C → WASM

Almost all of `linuxdoom-1.10/` except the Linux `I_*.c` implementations:

- Simulation: `p_*.c`, `g_game.c`, `d_net.c` logic, `info.c`
- Renderer: `r_*.c`, `v_video.c` (into the index buffer)
- WAD/zone: `w_wad.c`, `z_zone.c`
- UI: `m_menu.c`, `st_*`, `hu_*`, `am_map.c`, finales, wipes
- Sound logic: `s_sound.c`

Leave out `sndserv/`, `sersrc/`, `ipx/`.

### What to rewrite

| Area | Why | Browser replacement |
|------|-----|---------------------|
| Video | No X11/PseudoColor | Canvas + WebGL/WebGPU compositor |
| Input | No X keysyms / `XWarpPointer` | DOM + Pointer Lock + gamepad → `event_t` |
| Timing | `while(1)` + wipe busy-wait | `emscripten_set_main_loop` / rAF; slice wipes |
| Sound | No OSS/`popen` | Web Audio; optional ScriptProcessor/AudioWorklet mixer |
| Files | No raw POSIX without FS | Pack/fetch IWAD into MEMFS or memory WAD |
| Net | No UDP from a page | Stub single-player; later WebRTC |
| Exit | `I_Error` / `exit` | JS overlay, don’t kill the runtime |

### Recommended module split

```
web/
  index.html              # drop-zone for IWAD, canvas, settings
  compositor.js / .ts     # WebGL present + post FX
  input.js                # keymap → D_PostEvent via cwrap
  audio.js                # Web Audio
  main.js                 # glue, rAF, IWAD load
native/
  i_video_web.c           # I_FinishUpdate uploads screens[0]
  i_sound_web.c
  i_net_stub.c
  i_system_web.c          # I_GetTime from EM_ASM / emscripten_get_now
  i_main_web.c            # no blocking loop
```

Emscripten flags of interest: `-sALLOW_MEMORY_GROWTH`, `-sASYNCIFY` only if
you cannot eliminate blocking wipes, `-sEXPORTED_FUNCTIONS` for
`D_DoomMain` / a new `D_DoomFrame`, `-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS`.

Prefer **one exported `D_DoomFrame()`** that runs `TryRunTics` +
`S_UpdateSounds` + a non-blocking `D_Display`, called from JS rAF. Do not
keep `while (1)`.

### Existing ports (context, not a requirement)

Chocolate Doom, doomgeneric, and several Emscripten Doom ports already prove
the I_* rewrite. This document describes doing it **from this tree** so the
pixel buffer stays the original refresh, which is the right base for a
custom compositor.

---

## 9. Pixel graphics + modern effects

### Principle

**Indices on the CPU. RGB and effects on the GPU.** Never let bilinear
filtering touch the index buffer. Never replace `R_DrawColumn` unless you
want a different game.

### Compositor pipeline

```
screens[0]  320×200 R8 indices     (nearest)
PLAYPAL     256×RGB8, post-gamma   (1D LUT or 256×1 texture)
        ↓
Pass 0  palette expand → RGB8 320×200
Pass 1  integer upscale (nearest or sharp-bilinear), 4:3 present
Pass 2  bloom (threshold bright palette colors, downsample, add)
Pass 3  CRT (scanlines, mask, mild curvature, halation) at display res
Pass 4  color grade / vignette / film grain
        ↓
canvas
```

### Effect notes that preserve the look

| Effect | Where | Constraint |
|--------|-------|------------|
| Palette expand | First GPU pass | Must use the *current* `I_SetPalette` table so pain/bonus/radiation tints stay authentic. |
| Upscale | After expand | Nearest at integer scales (3×, 4×). Optional sharp-bilinear only after RGB. |
| Bloom | After upscale | Doom’s bright indices are few; threshold carefully or bloom looks like fog. Optional: treat fullbright/sky/FIRE as emissive. |
| CRT | Display resolution | Scanlines and shadow-mask on the *upscaled* image, not 320×200. |
| Color grade | Last | Lift/gamma/gain after palette; don’t double-apply gamma (Linux bakes `gammatable` in `UploadNewPalette`). |
| HUD | Same buffer | Status bar is already in `screens[0]`. FX apply to HUD too unless you add a mask later. |

### What not to do in v1

- GPU rasterizing walls/floors/sprites while “keeping the pixel look.”
- True look-up/down (needs a different projection; y-shear is a cheap cheat).
- Raising `SCREENWIDTH` without rewriting visplanes and HUD.
- Filtering the 8-bit index texture.

### Settings worth exposing

- Integer scale (fit / 2× / 3× / 4×)
- CRT amount
- Bloom amount
- Color grade preset (none / slightly cooler / crushed blacks)
- Authentic 4:3 vs stretch
- Palette-only (effects off) for comparison

---

## 10. Modding map (after it runs)

Keep gameplay mods in C (or Dehacked-style tables) so demos stay meaningful.
The compositor should not become a gameplay engine.

| Mod | Where | Notes |
|-----|-------|--------|
| Weapons / actors / states | `info.c`, `info.h`, `d_items.c`, `p_pspr.c`, `p_inter.c` | Classic Dehacked surface. |
| Enemy AI | `p_enemy.c` | |
| Maps | PWAD via `-file` / extra WAD in MEMFS | No engine change. |
| Palette / COLORMAP | WAD lumps | Automatically affects compositor LUT. |
| Status bar / menu art | `st_stuff.c`, `m_menu.c`, patches in WAD | Still 320-wide. |
| Jump / freelook / slopes | `p_user.c` + renderer | Large; not required for browser present. |
| Transparency / additive | `r_draw.c` | Possible in software; easier as a GPU blit of a second layer later. |

Carmack’s README already lists: transparency, look up/down, slopes, extra
weapons, jumping, packet or client/server net. Treat those as a second
milestone after a playable browser present.

---

## 11. Implementation phases

### Phase 0 — Host skeleton

- Emscripten toolchain, `web/index.html`, WASM build of the C core.
- IWAD file-picker; refuse to run without one.
- Stub `I_*` so `D_DoomMain` gets past init.

### Phase 1 — Visible pixels

- `I_FinishUpdate` copies `screens[0]` to a 2D canvas via palette expand in JS
  (good enough to debug).
- Keyboard → `D_PostEvent`.
- Non-blocking loop; wipes sliced or skipped.

**Exit criterion:** title screen, menu, E1M1, status bar, palette pain flash.

### Phase 2 — Real compositor

- Move present to WebGL. Index texture + palette LUT.
- Integer upscale, 4:3 letterbox.
- Wire `I_SetPalette` to LUT updates.

### Phase 3 — Audio + input polish

- Web Audio SFX from `s_sound` / `I_StartSound`.
- Pointer lock mouse, optional analog stick as joystick events.

### Phase 4 — Modern effects

- CRT, bloom, color grade as toggles.
- A/B against palette-only to prove the pixel buffer is unchanged.

### Phase 5 — Mods (optional)

- One vertical-slice gameplay change *or* PWAD loader UI.
- Do not mix renderer rewrites into this phase.

---

## 12. Creating the relevant Cursor agents

Cursor does not have a single “Custom Agent” wizard. Specialists are
**subagent markdown files**. Cloud Agents are the same Agent running on a
remote VM. Use both: check specialists into the repo, run heavy port work in
Cloud.

### Product steps

1. **Standing instructions** — this file plus root `AGENTS.md`. Nested
   `AGENTS.md` would apply under a subdirectory if you split `web/` later.
2. **Subagents** — markdown in `.cursor/agents/` (project) or
   `~/.cursor/agents/` (personal). Also accepted: `.claude/agents/`,
   `.codex/agents/` (`.cursor/` wins on name collision).
3. **Create a subagent**
   - In Agent chat: `/create-subagent`, or
   - Customize sidebar → Subagents, or
   - Write the file yourself (what this repo does).
4. **Invoke**
   - Automatic: the parent Agent reads each file’s `description`.
   - Explicit: `/wasm-platform …`, `/pixel-compositor …`, `/browser-verifier …`
   - Parallel: ask the parent to run specialists together (isolated context).
5. **Cloud**
   - Agent input environment dropdown → **Cloud**, or
   - [cursor.com/agents](https://cursor.com/agents), or
   - `/in-cloud` from a local session.
   - Requires a connected GitHub repo and a paid plan.
   - Cloud picks up `AGENTS.md`, `.cursor/rules`, `.cursor/agents`, and project
     skills automatically.
6. **Optional later:** Automations (`/automate` or cursor.com/automations) for
   “on PR, run browser-verifier”.

### File format

```markdown
---
name: lowercase-hyphen-id
description: When the parent should delegate. Include “use for …” phrases.
model: inherit
readonly: true   # only on auditors
---

You are …
When invoked:
1. …
Report: …
```

Keep bodies short. One responsibility per file. Do not create a generic
“helper.” Start with two or three specialists; add more only for a distinct
job.

### Specialist roster for this repo

| Agent | File | Job |
|-------|------|-----|
| WASM platform | `.cursor/agents/wasm-platform.md` | Emscripten, `I_*`, rAF loop, FS, input, audio, net stub |
| Pixel compositor | `.cursor/agents/pixel-compositor.md` | WebGL present, palette LUT, upscale, CRT/bloom/grade. Must not rewrite `r_*` |
| Browser verifier | `.cursor/agents/browser-verifier.md` | Build, load IWAD path, exercise menu/level, catch freezes |

A fourth **gameplay-mod** agent is useful only after Phase 4. Until then it
would compete with the compositor for attention.

### How the parent should orchestrate

```
User / Cloud Agent (orchestrator)
    ├─ /wasm-platform     → loop + I_* + WASM glue
    ├─ /pixel-compositor  → present + FX (after screens[0] exists)
    └─ /browser-verifier  → independent check of claimed work
```

Handoff rule: platform reports “`screens[0]` and `I_SetPalette` are exported.”
Compositor does not start from a blank engine. Verifier does not trust either
author — it runs the page.

### Rules vs skills vs agents

| Mechanism | Use for this project |
|-----------|----------------------|
| `AGENTS.md` | Architecture invariants (“don’t rewrite r_* for FX”). |
| `.cursor/rules/*.mdc` | Path-scoped conventions (`web/**` vs `linuxdoom-1.10/**`). |
| `.cursor/skills/` | Repeatable recipes (emcc build, IWAD test). Add when the command exists. |
| `.cursor/agents/` | Isolated workers with their own context. |
| `.cursor/environment.json` | Cloud VM install (emcc, python http server). Add when Phase 0 starts. |

### Prompting a Cloud run

Be specific: name the phase, name the files, name the IWAD policy, name the
exit criterion. Example:

> Implement Phase 1 from `docs/linuxdoom-browser-port.md`. Use the
> `wasm-platform` subagent. Do not modify `r_*.c`. Do not commit a WAD.
> Stop when the title screen renders to the canvas from a user-provided IWAD
> path. Then run `browser-verifier`.

---

## 13. Risks and constraints

| Risk | Mitigation |
|------|------------|
| Blocking wipe / `I_Error` / `exit` kills the page | Slice wipes; map errors to JS. |
| 35 Hz sim vs 60/120 Hz display | Decouple: many presents per tic, or present after tics; never change `TICRATE` casually (demos/net). |
| Rectangular pixels forgotten | 4:3 present path. |
| Bloom washing out the look | Default FX mild; palette-only toggle. |
| IWAD legal | User-supplied only; never fetch a commercial WAD. |
| `MAXVISPLANES` etc. | Don’t raise resolution in software as a shortcut. |
| ASYNCIFY hiding leftover blocking calls | Prefer deleting the waits. |
| Sound latency | AudioWorklet mixer; don’t mix on the main thread long-term. |

---

## 14. Key source index

| Concern | Files |
|---------|--------|
| Main loop / display | `linuxdoom-1.10/d_main.c`, `i_main.c` |
| Tics / net | `d_net.c`, `d_ticcmd.h`, `g_game.c` |
| Events | `d_event.h` |
| Renderer | `r_main.c`, `r_bsp.c`, `r_segs.c`, `r_plane.c`, `r_things.c`, `r_draw.c`, `r_data.c`, `r_defs.h` |
| Video buffers | `v_video.c`, `v_video.h`, `i_video.c` |
| Thinkers / map | `p_tick.c`, `p_mobj.c`, `p_setup.c`, `p_map.c`, `d_think.h` |
| Player / weapons | `p_user.c`, `p_pspr.c`, `info.c` |
| Sound | `s_sound.c`, `i_sound.c`, `sndserv/` |
| WAD / memory | `w_wad.c`, `z_zone.c` |
| Defines | `doomdef.h` (`SCREENWIDTH`, `TICRATE`, `VERSION`) |
| Design notes | `README.TXT`, `linuxdoom-1.10/README.gl`, `linuxdoom-1.10/TODO` |

---

## 15. Decision log

| Decision | Choice | Why |
|----------|--------|-----|
| Renderer | Keep software `R_*` | Pixel authenticity; colormap lighting. |
| Resolution | Stay 320×200 internally | Visplanes, HUD, and the original look. |
| GPU | Compositor only | Effects without a GLDoom rewrite. |
| Loop | rAF + `D_DoomFrame` | Browsers cannot `while(1)`. |
| Multiplayer | Stub v1 | UDP is not available from the page. |
| IWAD | User-provided | License. |
| Agents | Three specialists | Platform, compositor, verifier — matching the seams in this codebase. |
