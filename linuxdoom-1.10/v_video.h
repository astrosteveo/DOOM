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
//	Gamma correction LUT.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"

#include "doomdef.h"

// Needed because we are refering to patches.
#include "r_data.h"

//
// VIDEO
//

#define CENTERY			(ORIGHEIGHT/2)


// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is an extra buffer.



extern	byte*		screens[5];

extern  int	dirtybox[4];

extern	byte	gammatable[5][256];
extern	int	usegamma;



// Chooses the framebuffer size; see doomdef.h.
// aspect is the width:height of the framebuffer in its own pixels.
void V_SetResolution (int scale, double aspect);

// Allocates buffer screens, call before R_Init.
// Called again after the resolution changes.
void V_Init (void);

// Unlike the patch drawers, these take framebuffer coordinates.
void V_DrawPatchFB (int x, int y, int scrn, patch_t* patch);

// One column of a patch, at original screen coordinates.
void V_DrawPatchColumn (int x, int y, int scrn, patch_t* patch, int col);
void V_CopyRectFB (int srcx, int srcy, int srcscrn, int width, int height,
		   int destx, int desty, int destscrn);
void V_FillRect (int x, int y, int width, int height, int scrn, int color);
void V_TileFlat (int x, int y, int width, int height, int scrn, byte* flat);
void V_ClearMargins (int scrn);


void
V_CopyRect
( int		srcx,
  int		srcy,
  int		srcscrn,
  int		width,
  int		height,
  int		destx,
  int		desty,
  int		destscrn );

void
V_DrawPatch
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch);

void
V_DrawPatchFlipped
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch);

void
V_DrawPatchDirect
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch );


// Draw a linear block of pixels into the view buffer.
void
V_DrawBlock
( int		x,
  int		y,
  int		scrn,
  int		width,
  int		height,
  byte*		src );

// Reads a linear block of pixels into the view buffer.
void
V_GetBlock
( int		x,
  int		y,
  int		scrn,
  int		width,
  int		height,
  byte*		dest );


void
V_MarkRect
( int		x,
  int		y,
  int		width,
  int		height );

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
