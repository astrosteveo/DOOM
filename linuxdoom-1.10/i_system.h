// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include "d_ticcmd.h"
#include "m_fixed.h"
#include "d_event.h"

#include <setjmp.h>

#ifdef __GNUG__
#pragma interface
#endif


// Called by DoomMain.
void I_Init (void);

// Called by startup code
// to get the ammount of memory to malloc
// for the zone management.
byte*	I_ZoneBase (int *size);


// Called by D_DoomLoop,
// returns current time in tics.
int I_GetTime (void);

// How far into the current tic we are, 0..FRACUNIT-1.
// Used to interpolate rendering between tics.
fixed_t I_GetTimeFrac (void);

// Note that the game has just run the tics due, and how far real time
// has moved on since, 0..FRACUNIT.
void I_MarkTicTime (void);
fixed_t I_GetTicFrac (void);

// Yield the CPU for about ms milliseconds.
void I_Sleep (int ms);


//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//
void I_StartFrame (void);


//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic (void);

// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.
ticcmd_t* I_BaseTiccmd (void);


// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit (void);

// A host that runs the game inside its own process (the Godot
// extension) points this at a jump buffer: I_Quit then jumps to it
// with 1 and I_Error with 2, instead of ending the process. I_Error
// leaves its message in I_ErrorMessage.
extern jmp_buf*	I_HostReturn;
extern char	I_ErrorMessage[1024];


// Allocates from low memory under dos,
// just mallocs under unix
byte* I_AllocLow (int length);

void I_Tactile (int on, int off, int total);


void I_Error (char *error, ...) __attribute__((format(printf, 1, 2), noreturn));

// Per-user directories (created on first use), without trailing slash.
char* I_ConfigDir (void);
char* I_DataDir (void);
void I_MakeDirectory (const char* path);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
