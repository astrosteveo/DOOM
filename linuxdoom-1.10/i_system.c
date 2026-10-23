// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the GNU General Public License 2.0.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	System interface: timing, memory, paths, and fatal errors,
//	on top of SDL3.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

#include <SDL3/SDL.h>

#include "doomdef.h"
#include "m_argv.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"

#include "d_net.h"
#include "g_game.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"




// Zone size in megabytes. The original 6MB was sized for DOS;
// high resolution rendering and large PWADs want far more.
int	mb_used = 64;


void
I_Tactile
( int	on,
  int	off,
  int	total )
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t	emptycmd;
ticcmd_t*	I_BaseTiccmd(void)
{
    return &emptycmd;
}


int  I_GetHeapSize (void)
{
    return mb_used*1024*1024;
}

byte* I_ZoneBase (int*	size)
{
    int		p;
    byte*	zone;

    p = M_CheckParm ("-mb");
    if (p && p < myargc-1)
	mb_used = atoi (myargv[p+1]);
    if (mb_used < 8)
	mb_used = 8;

    *size = mb_used*1024*1024;
    zone = (byte *) malloc (*size);
    if (!zone)
	I_Error ("I_ZoneBase: could not allocate %d MB", mb_used);
    return zone;
}



//
// I_GetTime
// returns time in 1/35th second tics
//
static Uint64	basetime;

int  I_GetTime (void)
{
    Uint64	now;

    now = SDL_GetTicksNS ();
    if (!basetime)
	basetime = now;

    return (int)((now - basetime) * TICRATE / SDL_NS_PER_SECOND);
}

//
// I_GetTimeFrac
// Fraction of the way through the current tic, 0..FRACUNIT-1.
//
fixed_t I_GetTimeFrac (void)
{
    Uint64	now;
    Uint64	tics;

    now = SDL_GetTicksNS ();
    if (!basetime)
	basetime = now;

    tics = (now - basetime) * TICRATE;
    return (fixed_t)((tics % SDL_NS_PER_SECOND) * FRACUNIT / SDL_NS_PER_SECOND);
}


//
// I_MarkTicTime, I_GetTicFrac
// The game's latest state belongs to the real tic it was run in.
// Frames drawn later are that far into the next tic, which can only
// grow until another tic runs: unlike I_GetTimeFrac it never wraps
// back to 0 just before that.
//
static Uint64	lastticstart;

void I_MarkTicTime (void)
{
    Uint64	now;
    Uint64	tic;

    now = SDL_GetTicksNS ();
    if (!basetime)
	basetime = now;

    tic = (now - basetime) * TICRATE / SDL_NS_PER_SECOND;
    lastticstart = basetime + tic * SDL_NS_PER_SECOND / TICRATE;
}

fixed_t I_GetTicFrac (void)
{
    Uint64	elapsed;
    Uint64	frac;

    elapsed = SDL_GetTicksNS () - lastticstart;
    frac = elapsed * TICRATE * FRACUNIT / SDL_NS_PER_SECOND;
    return frac >= FRACUNIT ? FRACUNIT : (fixed_t)frac;
}

//
// I_Sleep
// Gives the CPU back while waiting for the next tic.
//
void I_Sleep (int ms)
{
    SDL_Delay (ms);
}



//
// I_Init
//
void I_Init (void)
{
    if (!SDL_Init (SDL_INIT_EVENTS))
	I_Error ("I_Init: SDL_Init failed: %s", SDL_GetError ());

    I_InitSound();
    //  I_InitGraphics();
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
    SDL_Quit ();
    exit(0);
}

void I_WaitVBL(int count)
{
    SDL_Delay (count * 1000/70);
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte*	I_AllocLow(int length)
{
    byte*	mem;

    mem = (byte *)calloc (1, length);
    if (!mem)
	I_Error ("I_AllocLow: could not allocate %d bytes", length);
    return mem;
}



//
// PATHS
// Configuration lives under $XDG_CONFIG_HOME, savegames and other
// generated data under $XDG_DATA_HOME, as the XDG base directory
// specification asks.
//

//
// I_MakeDirectory
// Creates a directory and any missing parents.
//
void I_MakeDirectory (const char* path)
{
    char	buf[1024];
    char*	p;

    snprintf (buf, sizeof(buf), "%s", path);
    for (p = buf+1 ; *p ; p++)
    {
	if (*p != '/')
	    continue;
	*p = 0;
	mkdir (buf, 0755);
	*p = '/';
    }
    if (mkdir (buf, 0755) && errno != EEXIST)
	fprintf (stderr, "I_MakeDirectory: could not create %s\n", buf);
}

static char* XDGDir (const char* var, const char* fallback)
{
    char	buf[1024];
    char*	base;
    char*	home;

    base = getenv (var);
    if (base && base[0] == '/')
	snprintf (buf, sizeof(buf), "%s/" PROGRAM_NAME, base);
    else
    {
	home = getenv ("HOME");
	if (!home)
	    I_Error ("Please set $HOME to your home directory");
	snprintf (buf, sizeof(buf), "%s/%s/" PROGRAM_NAME, home, fallback);
    }

    I_MakeDirectory (buf);
    return strdup (buf);
}

char* I_ConfigDir (void)
{
    static char*	dir;

    if (!dir)
	dir = XDGDir ("XDG_CONFIG_HOME", ".config");
    return dir;
}

char* I_DataDir (void)
{
    static char*	dir;

    if (!dir)
	dir = XDGDir ("XDG_DATA_HOME", ".local/share");
    return dir;
}



//
// I_Error
//
extern boolean demorecording;

void I_Error (char *error, ...)
{
    static boolean	inerror;
    va_list		argptr;
    char		msg[1024];

    // Message first.
    va_start (argptr,error);
    vsnprintf (msg, sizeof(msg), error, argptr);
    va_end (argptr);

    fprintf (stderr, "Error: %s\n", msg);
    fflush( stderr );

    // An error during shutdown must not recurse.
    if (inerror)
	exit(-1);
    inerror = true;

    // Shutdown. Here might be other errors.
    if (demorecording)
	G_CheckDemoStatus();

    D_QuitNetGame ();
    I_ShutdownSound ();
    I_ShutdownGraphics();

    if (!M_CheckParm ("-nogui"))
	SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_ERROR, PROGRAM_TITLE, msg, NULL);

    SDL_Quit ();
    exit(-1);
}
