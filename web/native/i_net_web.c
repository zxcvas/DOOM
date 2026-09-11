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
//	Browser (Emscripten/WASM) networking stub. Replaces i_net.c.
//
//	A page cannot open UDP sockets, so we configure a single local node.
//	The portable d_net.c / TryRunTics path still runs unchanged; it just
//	never has a remote peer. Real multiplayer would add a WebRTC/WebSocket
//	transport behind I_NetCmd later. See docs/linuxdoom-browser-port.md 6.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "d_net.h"
#include "m_argv.h"
#include "doomstat.h"
#include "i_net.h"


void I_InitNetwork (void)
{
    doomcom = malloc (sizeof (*doomcom));
    memset (doomcom, 0, sizeof(*doomcom));

    // Single-player: one node, this node.
    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
}


void I_NetCmd (void)
{
    // No transport in the browser stub. TryRunTics only reaches CMD_SEND /
    // CMD_GET for real net games; single-player never issues them.
}
