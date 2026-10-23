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
//	Gamma correction LUT stuff.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: v_video.c,v 1.5 1997/02/03 22:45:13 b1 Exp $";


#include <stdlib.h>

#include "i_system.h"
#include "r_local.h"

#include "doomdef.h"
#include "doomdata.h"

#include "m_bbox.h"
#include "m_swap.h"

#include "v_video.h"


// Each screen is [SCREENWIDTH*SCREENHEIGHT]; 
byte*				screens[5];	
 
int				dirtybox[4]; 



// Now where did these came from?
byte gammatable[5][256] =
{
    {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
     17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
     33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
     49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
     65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
     81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
     97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
     113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
     144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
     160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
     176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
     192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
     208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
     224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
     240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},

    {2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,24,25,26,27,29,30,31,
     32,33,34,36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52,54,55,
     56,57,58,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,77,
     78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,
     99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
     115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,129,
     130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
     146,147,148,148,149,150,151,152,153,154,155,156,157,158,159,160,
     161,162,163,163,164,165,166,167,168,169,170,171,172,173,174,175,
     175,176,177,178,179,180,181,182,183,184,185,186,186,187,188,189,
     190,191,192,193,194,195,196,196,197,198,199,200,201,202,203,204,
     205,205,206,207,208,209,210,211,212,213,214,214,215,216,217,218,
     219,220,221,222,222,223,224,225,226,227,228,229,230,230,231,232,
     233,234,235,236,237,237,238,239,240,241,242,243,244,245,245,246,
     247,248,249,250,251,252,252,253,254,255},

    {4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,33,35,36,38,39,40,42,
     43,45,46,47,48,50,51,52,54,55,56,57,59,60,61,62,63,65,66,67,68,69,
     70,72,73,74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,91,92,93,
     94,95,96,97,98,100,101,102,103,104,105,106,107,108,109,110,111,112,
     113,114,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
     129,130,131,132,133,133,134,135,136,137,138,139,140,141,142,143,144,
     144,145,146,147,148,149,150,151,152,153,153,154,155,156,157,158,159,
     160,160,161,162,163,164,165,166,166,167,168,169,170,171,172,172,173,
     174,175,176,177,178,178,179,180,181,182,183,183,184,185,186,187,188,
     188,189,190,191,192,193,193,194,195,196,197,197,198,199,200,201,201,
     202,203,204,205,206,206,207,208,209,210,210,211,212,213,213,214,215,
     216,217,217,218,219,220,221,221,222,223,224,224,225,226,227,228,228,
     229,230,231,231,232,233,234,235,235,236,237,238,238,239,240,241,241,
     242,243,244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,
     255},

    {8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,47,49,50,52,53,55,
     57,58,60,61,63,64,65,67,68,70,71,72,74,75,76,77,79,80,81,82,84,85,
     86,87,88,90,91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
     108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
     125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,139,140,
     141,142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,
     155,156,157,158,159,160,160,161,162,163,164,165,165,166,167,168,169,
     169,170,171,172,173,173,174,175,176,176,177,178,179,180,180,181,182,
     183,183,184,185,186,186,187,188,189,189,190,191,192,192,193,194,195,
     195,196,197,197,198,199,200,200,201,202,202,203,204,205,205,206,207,
     207,208,209,210,210,211,212,212,213,214,214,215,216,216,217,218,219,
     219,220,221,221,222,223,223,224,225,225,226,227,227,228,229,229,230,
     231,231,232,233,233,234,235,235,236,237,237,238,238,239,240,240,241,
     242,242,243,244,244,245,246,246,247,247,248,249,249,250,251,251,252,
     253,253,254,254,255},

    {16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,66,68,69,71,73,75,76,
     78,80,81,83,84,86,87,89,90,92,93,94,96,97,98,100,101,102,103,105,106,
     107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,
     125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
     142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
     156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
     169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,181,
     182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,
     193,194,195,195,196,196,197,198,198,199,200,200,201,202,202,203,203,
     204,205,205,206,207,207,208,208,209,210,210,211,211,212,213,213,214,
     214,215,216,216,217,217,218,219,219,220,220,221,221,222,223,223,224,
     224,225,225,226,227,227,228,228,229,229,230,230,231,232,232,233,233,
     234,234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
     243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,
     251,252,252,253,254,254,255,255}
};



int	usegamma;
			 
//
// V_MarkRect 
// 
void
V_MarkRect
( int		x,
  int		y,
  int		width,
  int		height ) 
{ 
    M_AddToBox (dirtybox, x, y); 
    M_AddToBox (dirtybox, x+width-1, y+height-1); 
} 
 

//
// V_CopyRect 
// Takes original screen coordinates, like the patch drawers.
// 
void
V_CopyRect
( int		srcx,
  int		srcy,
  int		srcscrn,
  int		width,
  int		height,
  int		destx,
  int		desty,
  int		destscrn ) 
{ 
    byte*	src;
    byte*	dest; 
	 
#ifdef RANGECHECK 
    if (srcx<0
	||srcx+width >ORIGWIDTH
	|| srcy<0
	|| srcy+height>ORIGHEIGHT 
	||destx<0||destx+width >ORIGWIDTH
	|| desty<0
	|| desty+height>ORIGHEIGHT 
	|| (unsigned)srcscrn>4
	|| (unsigned)destscrn>4)
    {
	I_Error ("Bad V_CopyRect");
    }
#endif 
    V_MarkRect (destx, desty, width, height); 
	 
    width *= screenscale;
    height *= screenscale;
    src = screens[srcscrn] + SCREENWIDTH*srcy*screenscale
	+ screenleft + srcx*screenscale;
    dest = screens[destscrn] + SCREENWIDTH*desty*screenscale
	+ screenleft + destx*screenscale;

    for ( ; height>0 ; height--) 
    { 
	memcpy (dest, src, width); 
	src += SCREENWIDTH; 
	dest += SCREENWIDTH; 
    } 
} 
 

//
// V_CopyRectFB
// V_CopyRect in framebuffer coordinates.
//
void
V_CopyRectFB
( int		srcx,
  int		srcy,
  int		srcscrn,
  int		width,
  int		height,
  int		destx,
  int		desty,
  int		destscrn ) 
{ 
    byte*	src;
    byte*	dest; 

    src = screens[srcscrn] + SCREENWIDTH*srcy + srcx;
    dest = screens[destscrn] + SCREENWIDTH*desty + destx;
    for ( ; height>0 ; height--) 
    { 
	memcpy (dest, src, width); 
	src += SCREENWIDTH; 
	dest += SCREENWIDTH; 
    } 
}

//
// DrawPatchColumns
// Draws a patch scaled up by screenscale with its top left corner at
// framebuffer position x, y, clipped to the framebuffer. Optionally
// mirrored.
//
static void
DrawPatchColumns
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch,
  boolean	flipped ) 
{ 
    int		col; 
    int		w; 
    int		scale;
    int		x1;
    int		x2;
    int		top;
    int		count;
    int		y1;
    int		y2;
    int		py;
    column_t*	column; 
    byte*	source; 
    byte*	screen;

    scale = screenscale;
    screen = screens[scrn];
    w = SHORT(patch->width); 

    for (col=0 ; col<w ; col++)
    { 
	x1 = x + col*scale;
	x2 = x1 + scale;
	if (x2 <= 0)
	    continue;
	if (x1 >= SCREENWIDTH)
	    break;
	if (x1 < 0)
	    x1 = 0;
	if (x2 > SCREENWIDTH)
	    x2 = SCREENWIDTH;

	column = (column_t *)((byte *)patch
		    + LONG(patch->columnofs[flipped ? w-1-col : col])); 
	 
	// step through the posts in a column 
	while (column->topdelta != 0xff ) 
	{ 
	    source = (byte *)column + 3; 
	    top = y + column->topdelta*scale;
	    count = column->length;

	    for ( ; count>0 ; count--, source++, top += scale)
	    {
		y1 = top;
		y2 = top + scale;
		if (y2 <= 0)
		    continue;
		if (y1 >= SCREENHEIGHT)
		    break;
		if (y1 < 0)
		    y1 = 0;
		if (y2 > SCREENHEIGHT)
		    y2 = SCREENHEIGHT;

		for (py=y1 ; py<y2 ; py++)
		    memset (screen + py*SCREENWIDTH + x1, *source, x2-x1);
	    }
	    column = (column_t *)(  (byte *)column + column->length 
				    + 4 ); 
	} 
    }			 
} 

//
// V_DrawPatchColumn
// Draws column col of a patch at x, y on the original screen.
//
void
V_DrawPatchColumn
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch,
  int		col )
{
    column_t*	column;
    byte*	source;
    int		count;
    int		top;
    int		py;
    int		x1;

    x1 = screenleft + x*screenscale;
    if (x1 < 0 || x1 + screenscale > SCREENWIDTH)
	return;

    column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));
    while (column->topdelta != 0xff )
    {
	source = (byte *)column + 3;
	top = (y + column->topdelta) * screenscale;
	for (count = column->length ; count>0 ; count--, source++)
	{
	    for (py=top ; py<top+screenscale ; py++)
		if (py >= 0 && py < SCREENHEIGHT)
		    memset (screens[scrn] + py*SCREENWIDTH + x1,
			    *source, screenscale);
	    top += screenscale;
	}
	column = (column_t *)(  (byte *)column + column->length + 4 );
    }
}

//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
// Coordinates are on the original 320x200 screen; parts of a patch
// that hang off it are drawn into the widescreen margins if there are
// any, and clipped at the framebuffer's edges.
//
void
V_DrawPatch
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch ) 
{ 
    y -= SHORT(patch->topoffset); 
    x -= SHORT(patch->leftoffset); 

    if ((unsigned)scrn>4)
	I_Error ("V_DrawPatch: bad screen %d", scrn);

    if (!scrn)
	V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height)); 

    DrawPatchColumns (screenleft + x*screenscale, y*screenscale,
		      scrn, patch, false);
} 

//
// V_DrawPatchFlipped 
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
void
V_DrawPatchFlipped
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch ) 
{ 
    y -= SHORT(patch->topoffset); 
    x -= SHORT(patch->leftoffset); 

    if ((unsigned)scrn>4)
	I_Error ("V_DrawPatchFlipped: bad screen %d", scrn);

    if (!scrn)
	V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height)); 

    DrawPatchColumns (screenleft + x*screenscale, y*screenscale,
		      scrn, patch, true);
} 

//
// V_DrawPatchDirect
// Draws directly to the screen on the pc. 
//
void
V_DrawPatchDirect
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch ) 
{
    V_DrawPatch (x,y,scrn, patch); 
}

//
// V_DrawPatchFB
// Like V_DrawPatch, but at a framebuffer position (the patch is still
// scaled up). For decorations placed around things that are laid out
// in framebuffer pixels, such as the view border.
//
void
V_DrawPatchFB
( int		x,
  int		y,
  int		scrn,
  patch_t*	patch ) 
{
    x -= SHORT(patch->leftoffset)*screenscale;
    y -= SHORT(patch->topoffset)*screenscale;
    DrawPatchColumns (x, y, scrn, patch, false);
}


//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
// Framebuffer coordinates.
//
void
V_DrawBlock
( int		x,
  int		y,
  int		scrn,
  int		width,
  int		height,
  byte*		src ) 
{ 
    byte*	dest; 
	 
#ifdef RANGECHECK 
    if (x<0
	||x+width >SCREENWIDTH
	|| y<0
	|| y+height>SCREENHEIGHT 
	|| (unsigned)scrn>4 )
    {
	I_Error ("Bad V_DrawBlock");
    }
#endif 
 
    dest = screens[scrn] + y*SCREENWIDTH+x; 

    while (height--) 
    { 
	memcpy (dest, src, width); 
	src += width; 
	dest += SCREENWIDTH; 
    } 
} 
 


//
// V_GetBlock
// Gets a linear block of pixels from the view buffer.
// Framebuffer coordinates.
//
void
V_GetBlock
( int		x,
  int		y,
  int		scrn,
  int		width,
  int		height,
  byte*		dest ) 
{ 
    byte*	src; 
	 
#ifdef RANGECHECK 
    if (x<0
	||x+width >SCREENWIDTH
	|| y<0
	|| y+height>SCREENHEIGHT 
	|| (unsigned)scrn>4 )
    {
	I_Error ("Bad V_DrawBlock");
    }
#endif 
 
    src = screens[scrn] + y*SCREENWIDTH+x; 

    while (height--) 
    { 
	memcpy (dest, src, width); 
	src += SCREENWIDTH; 
	dest += width; 
    } 
} 


//
// V_FillRect
// Fills a framebuffer rectangle with one color, clipped.
//
void
V_FillRect
( int		x,
  int		y,
  int		width,
  int		height,
  int		scrn,
  int		color )
{
    byte*	dest;

    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > SCREENWIDTH) width = SCREENWIDTH - x;
    if (y + height > SCREENHEIGHT) height = SCREENHEIGHT - y;
    if (width <= 0 || height <= 0)
	return;

    dest = screens[scrn] + y*SCREENWIDTH + x;
    for ( ; height>0 ; height--, dest += SCREENWIDTH)
	memset (dest, color, width);
}

//
// V_TileFlat
// Fills a framebuffer rectangle with a 64x64 flat, each of its pixels
// scaled up like patches are, tiled from the framebuffer's origin.
//
void
V_TileFlat
( int		x,
  int		y,
  int		width,
  int		height,
  int		scrn,
  byte*		flat )
{
    int		px;
    int		py;
    int		x2;
    int		y2;
    byte*	dest;
    byte*	row;

    x2 = x + width;
    y2 = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > SCREENWIDTH) x2 = SCREENWIDTH;
    if (y2 > SCREENHEIGHT) y2 = SCREENHEIGHT;

    for (py=y ; py<y2 ; py++)
    {
	dest = screens[scrn] + py*SCREENWIDTH;
	row = flat + ((py/screenscale) & 63) * 64;
	for (px=x ; px<x2 ; px++)
	    dest[px] = row[(px/screenscale) & 63];
    }
}

//
// V_ClearMargins
// Blacks out the widescreen margins either side of the original
// screen, which full screen pictures don't cover.
//
void V_ClearMargins (int scrn)
{
    if (!screenleft)
	return;

    V_FillRect (0, 0, screenleft, SCREENHEIGHT, scrn, 0);
    V_FillRect (screenleft + ORIGWIDTH*screenscale, 0,
		SCREENWIDTH - screenleft - ORIGWIDTH*screenscale,
		SCREENHEIGHT, scrn, 0);
}



//
// V_SetResolution
// Picks the framebuffer size: scale times the original height, and as
// wide as needed to fill the shape of the window (or 4:3, when not
// widescreen).
//
int	screenwidth = ORIGWIDTH;
int	screenheight = ORIGHEIGHT;
int	screenscale = 1;
int	screenleft = 0;

void V_SetResolution (int scale, double aspect)
{
    int		width;

    if (scale < 1)
	scale = 1;
    if (scale > MAXSCREENSCALE)
	scale = MAXSCREENSCALE;

    // aspect is the shape the framebuffer is to be shown at;
    // it is never narrower than the original screen.
    width = (int)(ORIGHEIGHT*scale * aspect + 0.5);
    if (width < ORIGWIDTH*scale)
	width = ORIGWIDTH*scale;
    if (width > MAXWIDTH)
	width = MAXWIDTH;
    // Keep it even so the view centers exactly.
    width &= ~1;

    screenscale = scale;
    screenwidth = width;
    screenheight = ORIGHEIGHT*scale;
    screenleft = (screenwidth - ORIGWIDTH*scale) / 2;
}


//
// V_Init
// 
void V_Init (void) 
{ 
    int		i;
    static byte* base;
		
    if (base)
	free (base);

    base = I_AllocLow (SCREENWIDTH*SCREENHEIGHT*5);

    for (i=0 ; i<5 ; i++)
	screens[i] = base + i*SCREENWIDTH*SCREENHEIGHT;
}
