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
//	Browser (Emscripten/WASM) sound stub. Replaces i_sound.c for Phase 1.
//
//	The portable mixer logic in s_sound.c stays; these platform hooks are
//	silent no-ops for now. Web Audio output is Phase 3 in
//	docs/linuxdoom-browser-port.md. I_GetSfxLumpNum still resolves the lump
//	so s_sound bookkeeping stays valid.
//
//-----------------------------------------------------------------------------

#include <stdio.h>

#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"
#include "doomdef.h"


// doomdef.h hard-defines SNDSERV, so the config table in m_misc.c references
// these symbols that normally lived in the (excluded) Linux i_sound.c. There is
// no sound server in the browser; provide inert definitions so linking works.
#ifdef SNDSERV
FILE*	sndserver = NULL;
char*	sndserver_filename = "./sndserver";
#endif


void I_InitSound (void)
{
}

void I_UpdateSound (void)
{
}

void I_SubmitSound (void)
{
}

void I_ShutdownSound (void)
{
}

void I_SetChannels (void)
{
}


int I_GetSfxLumpNum (sfxinfo_t* sfx)
{
    char	namebuf[9];
    int		lump;

    sprintf (namebuf, "ds%s", sfx->name);
    lump = W_CheckNumForName (namebuf);
    return (lump < 0) ? 0 : lump;
}


int
I_StartSound
( int	id,
  int	vol,
  int	sep,
  int	pitch,
  int	priority )
{
    (void) id; (void) vol; (void) sep; (void) pitch; (void) priority;
    // Return a distinct, non-repeating handle so callers can track it.
    static int handle = 0;
    return handle++;
}

void I_StopSound (int handle)
{
    (void) handle;
}

int I_SoundIsPlaying (int handle)
{
    (void) handle;
    return 0;
}

void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch )
{
    (void) handle; (void) vol; (void) sep; (void) pitch;
}


//
// MUSIC I/O (all stubbed; Linux Doom had no music anyway).
//
void I_InitMusic (void)		{ }
void I_ShutdownMusic (void)	{ }
void I_SetMusicVolume (int volume)	{ (void) volume; }
void I_PauseSong (int handle)	{ (void) handle; }
void I_ResumeSong (int handle)	{ (void) handle; }

int I_RegisterSong (void* data)
{
    (void) data;
    return 0;
}

void
I_PlaySong
( int	handle,
  int	looping )
{
    (void) handle; (void) looping;
}

void I_StopSong (int handle)	{ (void) handle; }
void I_UnRegisterSong (int handle)	{ (void) handle; }
