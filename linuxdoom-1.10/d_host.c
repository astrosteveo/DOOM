//-----------------------------------------------------------------------------
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
// DESCRIPTION:
//	The game as a library, for a host: see d_host.h.
//
//-----------------------------------------------------------------------------

#include <math.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_video.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_menu.h"
#include "p_local.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_state.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "d_host.h"


// The controls' keys, from g_game.c, and whether the mouse moves the
// player forward, from i_video.c.
extern int	key_right;
extern int	key_left;
extern int	key_up;
extern int	key_down;
extern int	key_strafeleft;
extern int	key_straferight;
extern int	key_fire;
extern int	key_use;
extern int	key_strafe;
extern int	key_speed;
extern int	mouse_novert;

static jmp_buf	hostreturn;
static int	ended;

static int	levelcount;
static int	lastepisode;
static int	lastmap;
static int	lastnumlines;
static int	lastleveltime;


//
// DH_Start
//
int DH_Start (int argc, char** argv)
{
    myargc = argc;
    myargv = argv;
    I_HostReturn = &hostreturn;

    switch (setjmp (hostreturn))
    {
      case 0:
	break;
      case 1:
	return ended = DH_QUIT;
      default:
	return ended = DH_ERROR;
    }

    // No window: the host shows the frame, and draws the 3D view.
    I_SetHostScreen (640, 480);
    hostview = true;

    D_DoomSetup ();
    D_StartLoop ();
    return DH_RUNNING;
}


//
// NoteLevel
// A level counts as new when the map changes, or when the same map
// starts over (leveltime goes back), as after dying.
//
static void NoteLevel (void)
{
    if (gamestate != GS_LEVEL)
	return;
    if (gameepisode != lastepisode || gamemap != lastmap
	|| numlines != lastnumlines || leveltime < lastleveltime)
	levelcount++;
    lastepisode = gameepisode;
    lastmap = gamemap;
    lastnumlines = numlines;
    lastleveltime = leveltime;
}


//
// DH_Tic
//
int DH_Tic (void)
{
    if (ended)
	return ended;

    switch (setjmp (hostreturn))
    {
      case 0:
	break;
      case 1:
	return ended = DH_QUIT;
      default:
	return ended = DH_ERROR;
    }

    D_RunTic ();
    NoteLevel ();
    return DH_RUNNING;
}


const char* DH_ErrorMessage (void)
{
    return I_ErrorMessage;
}


static float Units (fixed_t f)
{
    return f / (float)FRACUNIT;
}

static float Radians (angle_t a)
{
    return a * (float)(2*M_PI / 4294967296.0);
}

//
// Moved
// Whether the world moved in the last tic, which it doesn't while the
// game is paused or in the menu: if not, what was a tic before is
// where it is now, as D_Display has it.
//
static boolean Moved (void)
{
    return lastsimtic == gametic-1;
}


//
// DH_GetState
//
void DH_GetState (dh_state_t* state)
{
    player_t*	player;
    mobj_t*	mo;

    state->gametic = gametic;
    state->gamestate = gamestate;
    state->episode = gameepisode;
    state->map = gamemap;
    state->leveltime = leveltime;
    state->levelcount = levelcount;

    player = &players[consoleplayer];
    mo = player->mo;
    if (gamestate == GS_LEVEL && mo)
    {
	state->x = Units (mo->x);
	state->y = Units (mo->y);
	state->z = Units (player->viewz);
	state->angle = Radians (mo->angle);
	state->extralight = player->extralight;
	state->fixedcolormap = player->fixedcolormap;
	state->palette = ST_Palette (player);

	// As R_SetupFrame places the view.
	if (Moved () && mo->interp)
	{
	    state->oldx = Units (mo->oldx);
	    state->oldy = Units (mo->oldy);
	    state->oldz = Units (player->oldviewz);
	}
	else
	{
	    state->oldx = state->x;
	    state->oldy = state->y;
	    state->oldz = state->z;
	}
	if (Moved () && mo->interp && demoplayback)
	    state->oldangle = Radians (mo->oldangle);
	else
	    state->oldangle = state->angle;
	// G_PendingTurn is an angle_t; it turns either way.
	state->turn = (int)G_PendingTurn () * (float)(2*M_PI / 4294967296.0);
    }
    else
    {
	state->x = state->y = state->z = state->angle = 0;
	state->oldx = state->oldy = state->oldz = state->oldangle = 0;
	state->turn = 0;
	state->extralight = state->fixedcolormap = 0;
	state->palette = 0;
    }
    state->sky = skytexture;
    state->menu = menuactive;
    state->paused = paused;
    state->demoplayback = demoplayback;
    state->automap = automapactive;
}


_Static_assert ((int)DH_KEYDOWN == (int)ev_keydown && (int)DH_KEYUP == (int)ev_keyup
		&& (int)DH_MOUSE == (int)ev_mouse, "dh_event_t's types are evtype_t's");
_Static_assert (DH_KEY_RIGHTARROW == KEY_RIGHTARROW
		&& DH_KEY_LEFTARROW == KEY_LEFTARROW
		&& DH_KEY_UPARROW == KEY_UPARROW
		&& DH_KEY_DOWNARROW == KEY_DOWNARROW
		&& DH_KEY_ESCAPE == KEY_ESCAPE && DH_KEY_ENTER == KEY_ENTER
		&& DH_KEY_TAB == KEY_TAB && DH_KEY_F1 == KEY_F1
		&& DH_KEY_F1+9 == KEY_F10 && DH_KEY_F11 == KEY_F11
		&& DH_KEY_F12 == KEY_F12 && DH_KEY_BACKSPACE == KEY_BACKSPACE
		&& DH_KEY_PAUSE == KEY_PAUSE && DH_KEY_EQUALS == KEY_EQUALS
		&& DH_KEY_MINUS == KEY_MINUS && DH_KEY_RSHIFT == KEY_RSHIFT
		&& DH_KEY_RCTRL == KEY_RCTRL && DH_KEY_RALT == KEY_RALT
		&& DH_KEY_CAPSLOCK == KEY_CAPSLOCK && DH_KEY_HOME == KEY_HOME
		&& DH_KEY_PGUP == KEY_PGUP && DH_KEY_END == KEY_END
		&& DH_KEY_PGDN == KEY_PGDN && DH_KEY_INS == KEY_INS
		&& DH_KEY_DEL == KEY_DEL, "DH_KEY_ codes are doomdef.h's");


//
// DH_PostEvent
// As i_video.c posts SDL's events, and D_DoomLoop takes them between
// tics.
//
int DH_PostEvent (const dh_event_t* event)
{
    event_t	ev;

    if (ended)
	return ended;

    switch (setjmp (hostreturn))
    {
      case 0:
	break;
      case 1:
	return ended = DH_QUIT;
      default:
	return ended = DH_ERROR;
    }

    ev.type = event->type;
    ev.data1 = event->data1;
    ev.data2 = event->data2;
    ev.data3 = event->data3;
    if (ev.type == ev_mouse && mouse_novert)
	ev.data3 = 0;
    D_PostEvent (&ev);
    D_ProcessEvents ();
    return DH_RUNNING;
}


//
// The screen
// D_Display draws it as for the SDL3 build, but for the 3D view, which
// it leaves filled with one color under the player's weapon. To tell
// what else was drawn over the view, the status messages, the pause
// sign and the menu, DH_Draw draws all of that again over a second
// color: where the two frames differ, the view shows through.
//
static byte*	screencopy;
static byte*	shown;		// 0 where the view shows through
static int	screensize;
static boolean	viewshown;


//
// DH_SetScreenSize
//
void DH_SetScreenSize (int width, int height)
{
    I_SetHostScreen (width, height);
}


//
// DH_Draw
//
int DH_Draw (float fraction)
{
    int		n;
    int		i;
    int		x;
    int		y;

    if (ended)
	return ended;

    switch (setjmp (hostreturn))
    {
      case 0:
	break;
      case 1:
	return ended = DH_QUIT;
      default:
	return ended = DH_ERROR;
    }

    if (fraction < 0)
	fraction = 0;
    if (fraction > 1)
	fraction = 1;
    hostfrac = (fixed_t)(fraction * FRACUNIT);
    hostviewfill = 0;
    D_Display ();

    n = SCREENWIDTH*SCREENHEIGHT;
    if (n != screensize)
    {
	free (screencopy);
	free (shown);
	screencopy = malloc (n);
	shown = malloc (n);
	if (!screencopy || !shown)
	    I_Error ("DH_Draw: no memory for a %dx%d screen",
		     SCREENWIDTH, SCREENHEIGHT);
	screensize = n;
    }
    memset (shown, 1, n);

    // As D_Display decides whether to draw the view.
    viewshown = gamestate == GS_LEVEL && !automapactive && gametic;
    if (!viewshown || nodrawers)
	return DH_RUNNING;

    memcpy (screencopy, screens[0], n);
    renderfrac = Moved () ? hostfrac : FRACUNIT;
    R_RenderPlayerSprites (&players[displayplayer], 255, false);
    renderfrac = FRACUNIT;
    HU_Drawer ();
    D_DrawPause ();
    M_Drawer ();
    for (y=viewwindowy ; y<viewwindowy+viewheight ; y++)
    {
	for (x=viewwindowx ; x<viewwindowx+scaledviewwidth ; x++)
	{
	    i = y*SCREENWIDTH + x;
	    shown[i] = screens[0][i] == screencopy[i];
	}
    }
    memcpy (screens[0], screencopy, n);
    return DH_RUNNING;
}


//
// DH_GetScreen
//
void DH_GetScreen (dh_screen_t* screen)
{
    screen->width = SCREENWIDTH;
    screen->height = SCREENHEIGHT;
    screen->projection = projectionwidth / 2.0f;
    if (viewshown)
    {
	screen->viewx = viewwindowx;
	screen->viewy = viewwindowy;
	screen->viewwidth = scaledviewwidth;
	screen->viewheight = viewheight;
    }
    else
	screen->viewx = screen->viewy
	    = screen->viewwidth = screen->viewheight = 0;
}


//
// DH_ScreenRGBA
//
void DH_ScreenRGBA (unsigned char* rgba)
{
    byte	palette[768];
    byte*	c;
    int		i;

    memcpy (palette, W_CacheLumpName ("PLAYPAL", PU_CACHE), sizeof(palette));
    for (i=0 ; i<SCREENWIDTH*SCREENHEIGHT ; i++)
    {
	c = palette + screens[0][i]*3;
	rgba[i*4] = c[0];
	rgba[i*4+1] = c[1];
	rgba[i*4+2] = c[2];
	rgba[i*4+3] = !shown || i >= screensize || shown[i] ? 255 : 0;
    }
}


//
// DH_PaletteTint
// Each PLAYPAL palette is the first blended toward one color: fits
// that color and how far, by least squares over the 256 entries.
//
void DH_PaletteTint (int palette, float rgb[3], float* amount)
{
    byte*	playpal;
    byte*	base;
    byte*	tinted;
    int		i;
    int		c;
    double	slope;
    double	mx;
    double	my;
    double	sxx;
    double	sxy;
    double	keep;
    double	intercept[3];

    rgb[0] = rgb[1] = rgb[2] = 0;
    *amount = 0;
    if (palette <= 0 || palette >= 14)
	return;

    playpal = W_CacheLumpName ("PLAYPAL", PU_CACHE);
    base = playpal;
    tinted = playpal + palette*768;

    keep = 0;
    for (c=0 ; c<3 ; c++)
    {
	mx = my = 0;
	for (i=0 ; i<256 ; i++)
	{
	    mx += base[i*3+c];
	    my += tinted[i*3+c];
	}
	mx /= 256;
	my /= 256;
	sxx = sxy = 0;
	for (i=0 ; i<256 ; i++)
	{
	    sxx += (base[i*3+c] - mx) * (base[i*3+c] - mx);
	    sxy += (base[i*3+c] - mx) * (tinted[i*3+c] - my);
	}
	slope = sxx ? sxy / sxx : 1;
	keep += slope / 3;
	intercept[c] = my - slope*mx;
    }

    if (keep >= 1)
	return;
    *amount = 1 - keep;
    for (c=0 ; c<3 ; c++)
    {
	rgb[c] = intercept[c] / *amount / 255;
	if (rgb[c] < 0)
	    rgb[c] = 0;
	if (rgb[c] > 1)
	    rgb[c] = 1;
    }
}


//
// DH_ControlKey
//
int DH_ControlKey (int control)
{
    switch (control)
    {
      case DH_FORWARD:		return key_up;
      case DH_BACKWARD:		return key_down;
      case DH_TURNLEFT:		return key_left;
      case DH_TURNRIGHT:	return key_right;
      case DH_STRAFELEFT:	return key_strafeleft;
      case DH_STRAFERIGHT:	return key_straferight;
      case DH_FIRE:		return key_fire;
      case DH_USE:		return key_use;
      case DH_STRAFE:		return key_strafe;
      case DH_RUN:		return key_speed;
      default:			return 0;
    }
}


//
// PlaneHeight
//
static fixed_t PlaneHeight (int plane)
{
    sector_t*	sec = &sectors[plane/2];

    return plane & 1 ? sec->ceilingheight : sec->floorheight;
}


//
// AddWall
// One section of a side, between the bottomplane and topplane, which
// may leave it empty. Its texture's top row is texturetop above
// pegplane, from Doom's pegging rules.
//
static int AddWall (dh_wall_t* walls, int max, int count,
		    line_t* line, int sidenum, int texture,
		    int bottomplane, int topplane,
		    int pegplane, fixed_t texturetop)
{
    side_t*	side;
    vertex_t*	a;
    vertex_t*	b;
    float	length;
    fixed_t	bottom;
    fixed_t	top;
    dh_wall_t*	w;

    if (texture == 0)
	return count;
    if (count >= max || !walls)
	return count+1;

    side = &sides[line->sidenum[sidenum]];
    // What the renderer draws: wall animation shows through here.
    if (texture != DH_SKY)
	texture = texturetranslation[texture];
    a = sidenum ? line->v2 : line->v1;
    b = sidenum ? line->v1 : line->v2;
    length = hypotf (Units (b->x - a->x), Units (b->y - a->y));
    bottom = PlaneHeight (bottomplane);
    top = PlaneHeight (topplane);
    texturetop += PlaneHeight (pegplane);

    w = &walls[count];
    w->x1 = Units (a->x);
    w->y1 = Units (a->y);
    w->x2 = Units (b->x);
    w->y2 = Units (b->y);
    w->bottom = Units (bottom);
    w->top = Units (top);
    w->u1 = Units (side->textureoffset);
    w->u2 = w->u1 + length;
    w->v1 = Units (texturetop - top + side->rowoffset);
    w->v2 = Units (texturetop - bottom + side->rowoffset);
    w->texture = texture;
    w->sector = side->sector - sectors;
    w->bottomplane = bottomplane;
    w->topplane = topplane;
    w->pegplane = pegplane;
    return count+1;
}


//
// DH_GetWalls
// The rules are R_StoreWallRange's, for each side of each line, with
// each section kept whether or not the heights leave it empty now.
// Masked middle textures on two-sided lines aren't included yet.
// Where the ceiling steps down between two sky sectors, Doom draws
// no upper wall and the sky shows above the lower ceiling; that wall
// comes back as one showing the sky.
//
int DH_GetWalls (dh_wall_t* walls, int max)
{
    int		i;
    int		s;
    int		count;
    line_t*	line;
    side_t*	side;
    int		front;
    int		back;

    if (gamestate != GS_LEVEL)
	return 0;

    count = 0;
    for (i=0 ; i<numlines ; i++)
    {
	line = &lines[i];
	for (s=0 ; s<2 ; s++)
	{
	    if (line->sidenum[s] == -1)
		continue;
	    side = &sides[line->sidenum[s]];
	    front = (s ? line->backsector : line->frontsector) - sectors;

	    if (!line->backsector)
	    {
		if (line->flags & ML_DONTPEGBOTTOM)
		    count = AddWall (walls, max, count, line, s,
				     side->midtexture,
				     DH_Plane (front, 0), DH_Plane (front, 1),
				     DH_Plane (front, 0),
				     textureheight[side->midtexture]);
		else
		    count = AddWall (walls, max, count, line, s,
				     side->midtexture,
				     DH_Plane (front, 0), DH_Plane (front, 1),
				     DH_Plane (front, 1), 0);
		continue;
	    }
	    back = (s ? line->frontsector : line->backsector) - sectors;

	    // Upper: where the ceiling steps down, and sky if both are.
	    if (sectors[front].ceilingpic == skyflatnum
		&& sectors[back].ceilingpic == skyflatnum)
		count = AddWall (walls, max, count, line, s, DH_SKY,
				 DH_Plane (back, 1), DH_Plane (front, 1),
				 DH_Plane (front, 1), 0);
	    else if (line->flags & ML_DONTPEGTOP)
		count = AddWall (walls, max, count, line, s,
				 side->toptexture,
				 DH_Plane (back, 1), DH_Plane (front, 1),
				 DH_Plane (front, 1), 0);
	    else
		count = AddWall (walls, max, count, line, s,
				 side->toptexture,
				 DH_Plane (back, 1), DH_Plane (front, 1),
				 DH_Plane (back, 1),
				 textureheight[side->toptexture]);

	    // Lower: where the floor steps up.
	    if (line->flags & ML_DONTPEGBOTTOM)
		count = AddWall (walls, max, count, line, s,
				 side->bottomtexture,
				 DH_Plane (front, 0), DH_Plane (back, 0),
				 DH_Plane (front, 1), 0);
	    else
		count = AddWall (walls, max, count, line, s,
				 side->bottomtexture,
				 DH_Plane (front, 0), DH_Plane (back, 0),
				 DH_Plane (back, 0), 0);
	}
    }
    return count;
}


//
// DH_GetThings
//
int DH_GetThings (dh_thing_t* things, int max)
{
    thinker_t*	th;
    mobj_t*	mo;
    dh_thing_t*	t;
    int		count;

    if (gamestate != GS_LEVEL)
	return 0;

    count = 0;
    for (th = thinkercap.next ; th != &thinkercap ; th = th->next)
    {
	if (th->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;
	if (things && count < max)
	{
	    mo = (mobj_t *)th;
	    t = &things[count];
	    t->x = Units (mo->x);
	    t->y = Units (mo->y);
	    t->z = Units (mo->z);
	    t->radius = Units (mo->radius);
	    t->height = Units (mo->height);
	    t->angle = Radians (mo->angle);
	    t->type = mo->type;
	    t->flags = mo->flags;
	    t->id = mo->id;
	    if (Moved () && mo->interp)
	    {
		t->oldx = Units (mo->oldx);
		t->oldy = Units (mo->oldy);
		t->oldz = Units (mo->oldz);
		t->oldangle = Radians (mo->oldangle);
	    }
	    else
	    {
		t->oldx = t->x;
		t->oldy = t->y;
		t->oldz = t->z;
		t->oldangle = t->angle;
	    }
	}
	count++;
    }
    return count;
}


//
// Flat
// A sector's flat as the renderer draws it: R_DrawPlanes draws the
// sky for skyflatnum, on floors as on ceilings.
//
static int Flat (int pic)
{
    if (pic == skyflatnum)
	return DH_SKY;
    return flattranslation[pic];
}


//
// DH_GetSectors
//
int DH_GetSectors (dh_sector_t* sectors_, int max)
{
    int		i;
    sector_t*	sec;
    dh_sector_t* s;

    if (gamestate != GS_LEVEL)
	return 0;

    for (i=0 ; i<numsectors && i<max && sectors_ ; i++)
    {
	sec = &sectors[i];
	s = &sectors_[i];
	s->floorheight = Units (sec->floorheight);
	s->ceilingheight = Units (sec->ceilingheight);
	s->floorflat = Flat (sec->floorpic);
	s->ceilingflat = Flat (sec->ceilingpic);
	s->light = sec->lightlevel;
    }
    return numsectors;
}


//
// Subsector shapes
// The renderer never needed them: it fills floors in between what the
// walls leave on screen. A subsector is a leaf of the BSP tree, so its
// shape is a box around the level cut by each partition line on the
// way down to it, keeping the side the leaf is on, then by each of its
// segs, keeping the inside. The partitions alone leave the leaf the
// void behind its one-sided walls.
//
// Split segs end at vertices rounded to whole units, so a seg can lie
// a little inside the partition it was cut along. A seg cuts only what
// lies more than SEGSLACK behind it, which leaves no gap there; the
// most it leaves of the void is SEGSLACK, under a wall.
//
#define MAXCORNERS	256
#define SEGSLACK	1.0

typedef struct
{
    int		count;
    double	x[MAXCORNERS];
    double	y[MAXCORNERS];
} shape_t;

static int		shapelevel = -1;	// the levelcount shapes are for
static dh_subsector_t*	shapes;
static dh_point_t*	shapepoints;
static int		numshapepoints;
static int		maxshapepoints;


//
// Cut
// Puts in out the part of in on the right of the line through (x,y)
// going (dx,dy), and up to slack units to its left. The right is a
// seg's front, and a node's front child.
//
static void Cut (shape_t* in, shape_t* out,
		 double x, double y, double dx, double dy, double slack)
{
    double	d[MAXCORNERS];
    double	length;
    double	t;
    int		i;
    int		j;

    length = sqrt (dx*dx + dy*dy);
    if (length == 0)
    {
	*out = *in;
	return;
    }

    // How far right of the line each corner is, from where it cuts.
    for (i=0 ; i<in->count ; i++)
	d[i] = (dy*(in->x[i] - x) - dx*(in->y[i] - y)) / length + slack;

    out->count = 0;
    for (i=0 ; i<in->count && out->count < MAXCORNERS-1 ; i++)
    {
	j = (i+1) % in->count;
	if (d[i] >= 0)
	{
	    out->x[out->count] = in->x[i];
	    out->y[out->count] = in->y[i];
	    out->count++;
	}
	if ((d[i] >= 0) != (d[j] >= 0))
	{
	    t = d[i] / (d[i] - d[j]);
	    out->x[out->count] = in->x[i] + t*(in->x[j] - in->x[i]);
	    out->y[out->count] = in->y[i] + t*(in->y[j] - in->y[i]);
	    out->count++;
	}
    }
}


//
// ShapeSubsector
// Cuts the leaf's shape by its segs and keeps it. A subsector left
// with no area gets no corners.
//
static void ShapeSubsector (int num, shape_t* shape)
{
    static shape_t	cut[2];
    subsector_t*	sub;
    seg_t*		seg;
    shape_t*		from;
    shape_t*		to;
    shape_t*		swap;
    dh_point_t*		grown;
    int			i;

    sub = &subsectors[num];
    from = shape;
    to = &cut[0];
    for (i=0 ; i<sub->numlines ; i++)
    {
	seg = &segs[sub->firstline + i];
	Cut (from, to, seg->v1->x / (double)FRACUNIT, seg->v1->y / (double)FRACUNIT,
	     (seg->v2->x - seg->v1->x) / (double)FRACUNIT,
	     (seg->v2->y - seg->v1->y) / (double)FRACUNIT, SEGSLACK);
	swap = (to == &cut[0]) ? &cut[1] : &cut[0];
	from = to;
	to = swap;
    }

    shapes[num].sector = sub->sector - sectors;
    shapes[num].first = numshapepoints;
    shapes[num].count = 0;
    if (from->count < 3)
	return;

    if (numshapepoints + from->count > maxshapepoints)
    {
	grown = realloc (shapepoints, (maxshapepoints*2 + from->count)
			 * sizeof(*shapepoints));
	if (!grown)
	    return;
	shapepoints = grown;
	maxshapepoints = maxshapepoints*2 + from->count;
    }
    for (i=0 ; i<from->count ; i++)
    {
	shapepoints[numshapepoints + i].x = from->x[i];
	shapepoints[numshapepoints + i].y = from->y[i];
    }
    numshapepoints += from->count;
    shapes[num].count = from->count;
}


//
// ShapeNode
// Cuts shape, the part of the level under node, for each child.
//
static void ShapeNode (int nodenum, shape_t* shape)
{
    shape_t	part;
    node_t*	node;
    double	x;
    double	y;
    double	dx;
    double	dy;

    if (nodenum & NF_SUBSECTOR)
    {
	ShapeSubsector (nodenum & ~NF_SUBSECTOR, shape);
	return;
    }

    node = &nodes[nodenum];
    x = node->x / (double)FRACUNIT;
    y = node->y / (double)FRACUNIT;
    dx = node->dx / (double)FRACUNIT;
    dy = node->dy / (double)FRACUNIT;
    Cut (shape, &part, x, y, dx, dy, 0);
    ShapeNode (node->children[0], &part);
    Cut (shape, &part, x, y, -dx, -dy, 0);
    ShapeNode (node->children[1], &part);
}


//
// BuildShapes
// Once a level, when a host first asks.
//
static void BuildShapes (void)
{
    shape_t	box;
    fixed_t	left, right, bottom, top;
    int		i;

    if (shapelevel == levelcount && shapes)
	return;

    free (shapes);
    shapes = calloc (numsubsectors, sizeof(*shapes));
    numshapepoints = 0;
    if (!shapes)
	return;
    shapelevel = levelcount;

    left = right = vertexes[0].x;
    bottom = top = vertexes[0].y;
    for (i=1 ; i<numvertexes ; i++)
    {
	if (vertexes[i].x < left)
	    left = vertexes[i].x;
	if (vertexes[i].x > right)
	    right = vertexes[i].x;
	if (vertexes[i].y < bottom)
	    bottom = vertexes[i].y;
	if (vertexes[i].y > top)
	    top = vertexes[i].y;
    }

    // Clockwise seen from above, with a margin.
    box.count = 4;
    box.x[0] = box.x[3] = left / (double)FRACUNIT - 64;
    box.x[1] = box.x[2] = right / (double)FRACUNIT + 64;
    box.y[0] = box.y[1] = top / (double)FRACUNIT + 64;
    box.y[2] = box.y[3] = bottom / (double)FRACUNIT - 64;

    // With no nodes, the level is the one subsector.
    if (numnodes)
	ShapeNode (numnodes-1, &box);
    else
	ShapeSubsector (0, &box);
}


//
// DH_GetSubsectors
//
int DH_GetSubsectors (dh_subsector_t* subsectors_, int max)
{
    if (gamestate != GS_LEVEL)
	return 0;
    BuildShapes ();
    if (!shapes)
	return 0;
    if (subsectors_)
	memcpy (subsectors_, shapes,
		(max < numsubsectors ? max : numsubsectors) * sizeof(*shapes));
    return numsubsectors;
}


//
// DH_GetSubsectorPoints
//
int DH_GetSubsectorPoints (dh_point_t* points, int max)
{
    if (gamestate != GS_LEVEL)
	return 0;
    BuildShapes ();
    if (!shapes)
	return 0;
    if (points)
	memcpy (points, shapepoints,
		(max < numshapepoints ? max : numshapepoints) * sizeof(*points));
    return numshapepoints;
}


//
// DH_TextureInfo
//
int DH_TextureInfo (int tex, char name[9], int* width, int* height)
{
    return R_TextureInfo (tex, name, width, height);
}


//
// DH_TextureRGBA
// Column by column, as the renderer reads it: R_GetColumn wraps the
// column number the way the game does for widths that aren't a power
// of two.
//
void DH_TextureRGBA (int tex, unsigned char* rgba)
{
    char	name[9];
    int		width;
    int		height;
    int		x;
    int		y;
    byte	palette[768];
    byte*	column;
    byte*	c;
    unsigned char* p;

    if (!R_TextureInfo (tex, name, &width, &height))
	return;

    // Copied, since R_GetColumn may purge cached lumps.
    memcpy (palette, W_CacheLumpName ("PLAYPAL", PU_CACHE), sizeof(palette));
    for (x=0 ; x<width ; x++)
    {
	column = R_GetColumn (tex, x);
	for (y=0 ; y<height ; y++)
	{
	    c = palette + column[y]*3;
	    p = rgba + (y*width + x)*4;
	    p[0] = c[0];
	    p[1] = c[1];
	    p[2] = c[2];
	    p[3] = 255;
	}
    }
}


//
// DH_FlatInfo
//
int DH_FlatInfo (int flat, char name[9])
{
    if (flat < 0 || flat >= numflats
	|| W_LumpLength (firstflat + flat) < DH_FLATSIZE*DH_FLATSIZE)
	return 0;
    memcpy (name, lumpinfo[firstflat + flat].name, 8);
    name[8] = 0;
    return 1;
}


//
// DH_FlatRGBA
//
void DH_FlatRGBA (int flat, unsigned char* rgba)
{
    char	name[9];
    byte	palette[768];
    byte*	source;
    byte*	c;
    int		i;

    if (!DH_FlatInfo (flat, name))
	return;

    // Copied, since caching the flat may purge the palette.
    memcpy (palette, W_CacheLumpName ("PLAYPAL", PU_CACHE), sizeof(palette));
    source = W_CacheLumpNum (firstflat + flat, PU_CACHE);
    for (i=0 ; i<DH_FLATSIZE*DH_FLATSIZE ; i++)
    {
	c = palette + source[i]*3;
	rgba[i*4] = c[0];
	rgba[i*4+1] = c[1];
	rgba[i*4+2] = c[2];
	rgba[i*4+3] = 255;
    }
}
