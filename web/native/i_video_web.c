// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// DESCRIPTION:
//	Browser (Emscripten/WASM) video platform layer.
//
//	This replaces the X11/MIT-SHM implementation in i_video.c. It keeps
//	Doom's software renderer intact: the game keeps writing 320x200 8-bit
//	palette indices into screens[0]. Here we only:
//	  - expose screens[0] and a gamma-corrected PLAYPAL to JS, and
//	  - accept DOM input events pushed from JS into the Doom event queue.
//	Palette expansion to RGB and any upscale/effects happen in the JS/WebGL
//	compositor, AFTER this buffer is complete. See
//	docs/linuxdoom-browser-port.md sections 6 and 9.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include <emscripten.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"
#include "doomdef.h"

// Gamma-corrected 256*RGB palette, kept in sync with the game's I_SetPalette.
// JS reads this to build the palette LUT so pain/bonus/radiation tints stay
// authentic.
static byte	web_palette[256 * 3];

// Bumped every time the palette changes so the compositor can re-upload its
// LUT lazily instead of every frame.
static int	web_palette_version = 0;


//
// I_InitGraphics
//
// screens[0] was already allocated as a 320x200x8 buffer by V_Init(). Unlike
// the X11 build we do NOT repoint it at a server image; the software renderer
// keeps drawing into that malloc'd buffer and JS reads it directly.
//
void I_InitGraphics (void)
{
    static int firsttime = 1;

    if (!firsttime)
	return;
    firsttime = 0;

    // Publish an initial (all-black) palette so JS has something valid before
    // the first I_SetPalette call.
    memset (web_palette, 0, sizeof(web_palette));
    web_palette_version++;
}


void I_ShutdownGraphics (void)
{
}


//
// I_StartFrame / I_StartTic
//
// On X11 these pumped the server event queue. In the browser, input arrives
// asynchronously from the DOM and is pushed straight into Doom's event ring by
// Doom_PostKey / Doom_PostMouse below, so there is nothing to do here.
//
void I_StartFrame (void)
{
}

void I_StartTic (void)
{
}


void I_UpdateNoBlit (void)
{
}


//
// I_FinishUpdate
//
// The present point. The JS host reads screens[0] + the palette after each
// D_DoomFrame and draws them, so there is nothing to blit here. We just track a
// frame counter for diagnostics.
//
volatile int web_frames_presented = 0;

void I_FinishUpdate (void)
{
    web_frames_presented++;
}


void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}


//
// I_SetPalette
//
// Takes 256 RGB triples (8-bit). We apply Doom's gamma table here, exactly like
// the X11 UploadNewPalette did, so gamma is baked in exactly one place.
//
void I_SetPalette (byte* palette)
{
    int	i;
    int	c;

    for (i = 0; i < 256; i++)
    {
	c = gammatable[usegamma][*palette++];
	web_palette[i * 3 + 0] = (byte) c;
	c = gammatable[usegamma][*palette++];
	web_palette[i * 3 + 1] = (byte) c;
	c = gammatable[usegamma][*palette++];
	web_palette[i * 3 + 2] = (byte) c;
    }

    web_palette_version++;
}


//
// Exports for the JS host / compositor.
//
EMSCRIPTEN_KEEPALIVE
unsigned char* Doom_FrameBuffer (void)
{
    return screens[0];
}

EMSCRIPTEN_KEEPALIVE
unsigned char* Doom_Palette (void)
{
    return web_palette;
}

EMSCRIPTEN_KEEPALIVE
int Doom_PaletteVersion (void)
{
    return web_palette_version;
}

EMSCRIPTEN_KEEPALIVE
int Doom_Width (void)
{
    return SCREENWIDTH;
}

EMSCRIPTEN_KEEPALIVE
int Doom_Height (void)
{
    return SCREENHEIGHT;
}


//
// Input: JS translates browser key/mouse events into Doom key codes and calls
// these. They post directly into Doom's asynchronous event ring.
//
EMSCRIPTEN_KEEPALIVE
void Doom_PostKey (int down, int doomKey)
{
    event_t event;

    event.type = down ? ev_keydown : ev_keyup;
    event.data1 = doomKey;
    event.data2 = 0;
    event.data3 = 0;
    D_PostEvent (&event);
}

EMSCRIPTEN_KEEPALIVE
void Doom_PostMouse (int buttons, int dx, int dy)
{
    event_t event;

    event.type = ev_mouse;
    event.data1 = buttons;
    event.data2 = dx << 2;
    event.data3 = dy << 2;
    D_PostEvent (&event);
}
