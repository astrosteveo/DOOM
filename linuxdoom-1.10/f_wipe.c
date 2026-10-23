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
// $Log:$
//
// DESCRIPTION:
//	Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------


static const char rcsid[] = "$Id: f_wipe.c,v 1.2 1997/02/03 22:45:09 b1 Exp $";



#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"

#include "doomdef.h"

#include "f_wipe.h"

//
//                       SCREEN WIPE PACKAGE
//

// when zero, stop the wipe
static boolean	go = 0;

static byte*	wipe_scr_start;
static byte*	wipe_scr_end;
static byte*	wipe_scr;


int
wipe_initColorXForm
( int	width,
  int	height,
  int	ticks )
{
    memcpy(wipe_scr, wipe_scr_start, width*height);
    return 0;
}

int
wipe_doColorXForm
( int	width,
  int	height,
  int	ticks )
{
    boolean	changed;
    byte*	w;
    byte*	e;
    int		newval;

    changed = false;
    w = wipe_scr;
    e = wipe_scr_end;
    
    while (w!=wipe_scr+width*height)
    {
	if (*w != *e)
	{
	    if (*w > *e)
	    {
		newval = *w - ticks;
		if (newval < *e)
		    *w = *e;
		else
		    *w = newval;
		changed = true;
	    }
	    else if (*w < *e)
	    {
		newval = *w + ticks;
		if (newval > *e)
		    *w = *e;
		else
		    *w = newval;
		changed = true;
	    }
	}
	w++;
	e++;
    }

    return !changed;

}

int
wipe_exitColorXForm
( int	width,
  int	height,
  int	ticks )
{
    return 0;
}


//
// The melt works on the original screen's 160 two pixel wide columns
// and 200 rows, scaled to the framebuffer, so it looks the same at
// any resolution. Widescreen framebuffers get extra columns.
//
static int*	ypos;
static int	meltcols;
static int	meltcolwidth;

int
wipe_initMelt
( int	width,
  int	height,
  int	ticks )
{
    int i, r;
    
    // copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, width*height);
    
    meltcolwidth = 2*screenscale;
    meltcols = (width + meltcolwidth-1) / meltcolwidth;

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    ypos = (int *) Z_Malloc(meltcols*sizeof(int), PU_STATIC, 0);
    ypos[0] = -(M_Random()%16);
    for (i=1;i<meltcols;i++)
    {
	r = (M_Random()%3) - 1;
	ypos[i] = ypos[i-1] + r;
	if (ypos[i] > 0) ypos[i] = 0;
	else if (ypos[i] == -16) ypos[i] = -15;
    }

    return 0;
}

int
wipe_doMelt
( int	width,
  int	height,
  int	ticks )
{
    int		i;
    int		dy;
    int		x1;
    int		w;
    int		row;
    int		split;
    boolean	done = true;

    while (ticks--)
    {
	for (i=0;i<meltcols;i++)
	{
	    if (ypos[i]<0)
	    {
		ypos[i]++; done = false;
	    }
	    else if (ypos[i] < ORIGHEIGHT)
	    {
		dy = (ypos[i] < 16) ? ypos[i]+1 : 8;
		if (ypos[i]+dy >= ORIGHEIGHT) dy = ORIGHEIGHT - ypos[i];
		ypos[i] += dy;
		done = false;
	    }
	}
    }

    // The new screen shows above each column's edge, the old one
    // slides down below it.
    for (i=0;i<meltcols;i++)
    {
	x1 = i*meltcolwidth;
	w = meltcolwidth;
	if (x1 + w > width)
	    w = width - x1;
	split = (ypos[i] > 0 ? ypos[i] : 0) * screenscale;
	if (split > height)
	    split = height;

	for (row=0 ; row<split ; row++)
	    memcpy (wipe_scr + row*width + x1,
		    wipe_scr_end + row*width + x1, w);
	for (row=split ; row<height ; row++)
	    memcpy (wipe_scr + row*width + x1,
		    wipe_scr_start + (row-split)*width + x1, w);
    }

    return done;

}

int
wipe_exitMelt
( int	width,
  int	height,
  int	ticks )
{
    Z_Free(ypos);
    return 0;
}

int
wipe_StartScreen
( int	x,
  int	y,
  int	width,
  int	height )
{
    wipe_scr_start = screens[2];
    I_ReadScreen(wipe_scr_start);
    return 0;
}

int
wipe_EndScreen
( int	x,
  int	y,
  int	width,
  int	height )
{
    wipe_scr_end = screens[3];
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(x, y, 0, width, height, wipe_scr_start); // restore start scr.
    return 0;
}

int
wipe_ScreenWipe
( int	wipeno,
  int	x,
  int	y,
  int	width,
  int	height,
  int	ticks )
{
    int rc;
    static int (*wipes[])(int, int, int) =
    {
	wipe_initColorXForm, wipe_doColorXForm, wipe_exitColorXForm,
	wipe_initMelt, wipe_doMelt, wipe_exitMelt
    };

    void V_MarkRect(int, int, int, int);

    // initial stuff
    if (!go)
    {
	go = 1;
	// wipe_scr = (byte *) Z_Malloc(width*height, PU_STATIC, 0); // DEBUG
	wipe_scr = screens[0];
	(*wipes[wipeno*3])(width, height, ticks);
    }

    // do a piece of wipe-in
    V_MarkRect(0, 0, width, height);
    rc = (*wipes[wipeno*3+1])(width, height, ticks);
    //  V_DrawBlock(x, y, 0, width, height, wipe_scr); // DEBUG

    // final stuff
    if (rc)
    {
	go = 0;
	(*wipes[wipeno*3+2])(width, height, ticks);
    }

    return !go;

}
