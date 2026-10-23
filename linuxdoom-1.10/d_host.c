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
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "i_system.h"
#include "m_argv.h"
#include "p_local.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_zone.h"

#include "d_host.h"


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
// DH_GetState
//
void DH_GetState (dh_state_t* state)
{
    player_t*	player;

    state->gametic = gametic;
    state->gamestate = gamestate;
    state->episode = gameepisode;
    state->map = gamemap;
    state->leveltime = leveltime;
    state->levelcount = levelcount;

    player = &players[consoleplayer];
    if (gamestate == GS_LEVEL && player->mo)
    {
	state->x = Units (player->mo->x);
	state->y = Units (player->mo->y);
	state->z = Units (player->viewz);
	state->angle = Radians (player->mo->angle);
    }
    else
	state->x = state->y = state->z = state->angle = 0;
}


//
// AddWall
// One section of a side, between bottom and top. texturetop is the
// height the texture's top row sits at, from Doom's pegging rules.
//
static int AddWall (dh_wall_t* walls, int max, int count,
		    line_t* line, int sidenum, int texture,
		    fixed_t bottom, fixed_t top, fixed_t texturetop)
{
    side_t*	side;
    vertex_t*	a;
    vertex_t*	b;
    float	length;
    dh_wall_t*	w;

    if (texture <= 0 || top <= bottom)
	return count;
    if (count >= max || !walls)
	return count+1;

    side = &sides[line->sidenum[sidenum]];
    a = sidenum ? line->v2 : line->v1;
    b = sidenum ? line->v1 : line->v2;
    length = hypotf (Units (b->x - a->x), Units (b->y - a->y));

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
    w->light = side->sector->lightlevel;
    return count+1;
}


//
// DH_GetWalls
// The rules are R_StoreWallRange's, for each side of each line.
// Masked middle textures on two-sided lines aren't included yet.
//
int DH_GetWalls (dh_wall_t* walls, int max)
{
    int		i;
    int		s;
    int		count;
    line_t*	line;
    side_t*	side;
    sector_t*	front;
    sector_t*	back;
    fixed_t	top;

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
	    front = s ? line->backsector : line->frontsector;
	    back = s ? line->frontsector : line->backsector;

	    if (!back)
	    {
		if (line->flags & ML_DONTPEGBOTTOM)
		    top = front->floorheight
			+ textureheight[side->midtexture];
		else
		    top = front->ceilingheight;
		count = AddWall (walls, max, count, line, s,
				 side->midtexture, front->floorheight,
				 front->ceilingheight, top);
		continue;
	    }

	    // Upper: where the ceiling steps down, unless both are sky.
	    if (back->ceilingheight < front->ceilingheight
		&& !(front->ceilingpic == skyflatnum
		     && back->ceilingpic == skyflatnum))
	    {
		if (line->flags & ML_DONTPEGTOP)
		    top = front->ceilingheight;
		else
		    top = back->ceilingheight
			+ textureheight[side->toptexture];
		count = AddWall (walls, max, count, line, s,
				 side->toptexture, back->ceilingheight,
				 front->ceilingheight, top);
	    }

	    // Lower: where the floor steps up.
	    if (back->floorheight > front->floorheight)
	    {
		if (line->flags & ML_DONTPEGBOTTOM)
		    top = front->ceilingheight;
		else
		    top = back->floorheight;
		count = AddWall (walls, max, count, line, s,
				 side->bottomtexture, front->floorheight,
				 back->floorheight, top);
	    }
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
	}
	count++;
    }
    return count;
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
