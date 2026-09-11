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
//	Browser (Emscripten/WASM) system platform layer. Replaces i_system.c.
//
//	Key differences from the Linux version:
//	  - I_GetTime is driven by the high-resolution browser clock.
//	  - I_WaitVBL is a no-op (the rAF host paces frames; never busy-wait).
//	  - I_Error / I_Quit map to JS and never exit() the runtime / tab.
//	See docs/linuxdoom-browser-port.md section 6.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <emscripten.h>

#include "doomdef.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"

#include "d_net.h"
#include "g_game.h"

#include "i_system.h"


// A larger zone than the historical 6MB: modern browsers have plenty of memory
// and this avoids purge churn with larger (e.g. Freedoom) IWADs.
int	mb_used = 16;


void
I_Tactile
( int	on,
  int	off,
  int	total )
{
    on = off = total = 0;
}

ticcmd_t	emptycmd;
ticcmd_t*	I_BaseTiccmd (void)
{
    return &emptycmd;
}


int I_GetHeapSize (void)
{
    return mb_used * 1024 * 1024;
}

byte* I_ZoneBase (int* size)
{
    *size = mb_used * 1024 * 1024;
    return (byte *) malloc (*size);
}


//
// I_GetTime
// returns time in 1/TICRATE second tics, from the browser clock.
//
int I_GetTime (void)
{
    static double	basetime = 0;
    double		now = emscripten_get_now();	// milliseconds

    if (!basetime)
	basetime = now;

    return (int) ((now - basetime) * TICRATE / 1000.0);
}


//
// I_Init
//
void I_Init (void)
{
    I_InitSound();
}


//
// I_Quit
//
void I_Quit (void)
{
    D_QuitNetGame ();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults ();
    I_ShutdownGraphics();

    // Don't kill the tab; just report. The JS host can offer a reload.
    EM_ASM({
	if (typeof Module !== 'undefined' && Module.onDoomQuit)
	    Module.onDoomQuit();
    });
}

// Never busy-wait in the browser: the requestAnimationFrame host paces frames.
void I_WaitVBL (int count)
{
    (void) count;
}

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

byte* I_AllocLow (int length)
{
    byte*	mem;

    mem = (byte *) malloc (length);
    memset (mem, 0, length);
    return mem;
}


//
// I_Error
//
extern boolean demorecording;

void I_Error (char* error, ...)
{
    va_list	argptr;
    char	buffer[1024];

    va_start (argptr, error);
    vsnprintf (buffer, sizeof(buffer), error, argptr);
    va_end (argptr);

    fprintf (stderr, "Error: %s\n", buffer);
    fflush (stderr);

    if (demorecording)
	G_CheckDemoStatus();

    D_QuitNetGame ();
    I_ShutdownGraphics();

    // Surface the error to the page, then abort this call chain. We do not
    // exit() so the runtime stays inspectable.
    EM_ASM({
	if (typeof Module !== 'undefined' && Module.onDoomError)
	    Module.onDoomError(UTF8ToString($0));
    }, buffer);

    // abort() unwinds without tearing down the whole tab like exit() would.
    abort();
}
