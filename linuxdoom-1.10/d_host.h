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
//	The game as a library, for a host that runs it in its own
//	process and draws it itself: the Godot extension. Plain C types
//	only, so that C++ can include it; the engine's own headers
//	don't compile as C++.
//
//	A host reads the game's state and never writes it, so a demo
//	plays the same under any host as under the SDL3 build.
//
//-----------------------------------------------------------------------------

#ifndef __D_HOST__
#define __D_HOST__

#ifdef __cplusplus
extern "C" {
#endif

// What DH_Start and DH_Tic return.
enum
{
    DH_RUNNING,
    DH_QUIT,		// the game quit, as at the end of -playdemo
    DH_ERROR		// I_Error; DH_ErrorMessage says why
};

// Starts the game with a command line, as main would. The strings
// must outlive the game. Only one game runs in a process.
int		DH_Start (int argc, char** argv);

// Runs one tic. Once the game has ended, returns how it ended
// without running anything.
int		DH_Tic (void);

const char*	DH_ErrorMessage (void);

typedef struct
{
    int		gametic;
    int		gamestate;	// 0 level, 1 intermission, 2 finale, 3 title
    int		episode;
    int		map;
    int		leveltime;
    int		levelcount;	// goes up each time a level is loaded
    // The console player's eye, in map units, and the way it looks:
    // radians counterclockwise from east. All 0 outside a level.
    float	x, y, z;
    float	angle;
} dh_state_t;

void		DH_GetState (dh_state_t* state);

// One side of a wall: a quad from (x1,y1) to (x2,y2), between bottom
// and top, seen from the right of that direction, as Doom does.
// u and v are texture coordinates in texels.
typedef struct
{
    float	x1, y1, x2, y2;
    float	bottom, top;
    float	u1, u2, v1, v2;
    int		texture;	// as drawn: animation applied
    int		light;		// the sector's light level, 0 to 255
} dh_wall_t;

// Fills walls with up to max of the level's walls, and returns how
// many there are. Pass NULL and 0 to count them.
int		DH_GetWalls (dh_wall_t* walls, int max);

typedef struct
{
    float	x, y, z;	// the bottom middle, in map units
    float	radius, height;
    float	angle;		// radians counterclockwise from east
    int		type;		// mobjtype_t
    int		flags;		// MF_*
} dh_thing_t;

// The same for the things in the level.
int		DH_GetThings (dh_thing_t* things, int max);

// A wall texture's name and size; 0 when there is no such texture.
// The width is the one it repeats at, which is less than the width
// it is built at when that isn't a power of two.
int		DH_TextureInfo (int tex, char name[9], int* width, int* height);

// A wall texture as width*height RGBA pixels, row by row from the
// top, in the game's palette at full brightness.
void		DH_TextureRGBA (int tex, unsigned char* rgba);

#ifdef __cplusplus
}
#endif

#endif
