# FOON

Classic Doom pixels in the browser.

**FOON** is a browser host for the 1997 Linux Doom 1.10 source: the original
software renderer compiled **C → WASM**, presented through a **WebGL CRT
compositor**. You supply the IWAD. Commercial WAD files are never hosted or
committed.

The GitHub repository may still be named DOOM. The product name is FOON.

## Public site

The public landing page lives in [`web/site/`](web/site/) and is deployed to
GitHub Pages from that directory (see `.github/workflows/pages.yml`).

Play is a disabled placeholder until the WASM host exists.

## This repository

This tree is the 23 December 1997 public Linux Doom 1.10 source (GPL-2.0).

| Path | Role |
|------|------|
| `linuxdoom-1.10/` | Engine. Do not rewrite `r_*.c` for effects. |
| `web/site/` | Public marketing site (this drop). |
| `docs/linuxdoom-browser-port.md` | Port architecture and agent plan. |
| `README.TXT` | John Carmack’s original release note. |

Game IWAD data is **not** in this tree. The engine will not start without a
user-supplied IWAD (`doom.wad`, `doom2.wad`, shareware `doom1.wad`, or a
freely licensed IWAD).

## License

GPL-2.0. Source is free. Assets are not. Bring your own WAD.
