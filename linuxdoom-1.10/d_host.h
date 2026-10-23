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
    int		sky;		// the sky's wall texture, for DH_TextureRGBA
    // Light the console player sees by, 0 outside a level. extralight
    // brightens everything by that many of Doom's 16 light steps, for
    // a muzzle flash. A fixedcolormap other than 0 draws everything but
    // the sky through that COLORMAP row instead: 1 is the light
    // amplification visor, all at almost full brightness; 32 is
    // invulnerability, in inverted gray.
    int		extralight;
    int		fixedcolormap;
} dh_state_t;

void		DH_GetState (dh_state_t* state);

// What a wall's texture or a sector's flat is where Doom draws the sky
// in its place: DH_GetState's sky, drawn by view angle, not mapped
// onto the surface.
enum
{
    DH_SKY = -1
};

// One side of a wall: a quad from (x1,y1) to (x2,y2), between bottom
// and top, seen from the right of that direction, as Doom does.
// u and v are texture coordinates in texels.
typedef struct
{
    float	x1, y1, x2, y2;
    float	bottom, top;
    float	u1, u2, v1, v2;
    int		texture;	// as drawn: animation applied, or DH_SKY
    int		sector;		// the one it faces, for its light
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

// A sector as it is this tic. Floors and ceilings move and their
// flats animate, so a host reads them every tic; the shape of each
// sector, its subsectors, stays the same for the level.
typedef struct
{
    float	floorheight, ceilingheight;
    int		floorflat;	// as drawn: animation applied, or DH_SKY
    int		ceilingflat;
    int		light;		// 0 to 255
} dh_sector_t;

// The same for the level's sectors, numbered as the level numbers them.
int		DH_GetSectors (dh_sector_t* sectors, int max);

// A subsector: a convex piece of one sector's floor and ceiling, a leaf
// of the level's BSP tree. Doom never stored its shape, so this is
// rebuilt from the tree. Its corners are count points from first in
// DH_GetSubsectorPoints, clockwise seen from above. A subsector the
// tree leaves no area has none.
typedef struct
{
    int		sector;
    int		first;
    int		count;
} dh_subsector_t;

typedef struct
{
    float	x, y;		// map units
} dh_point_t;

// The same for the level's subsectors, and the points of their corners.
int		DH_GetSubsectors (dh_subsector_t* subsectors, int max);
int		DH_GetSubsectorPoints (dh_point_t* points, int max);

// A wall texture's name and size; 0 when there is no such texture.
// The width is the one it repeats at, which is less than the width
// it is built at when that isn't a power of two.
int		DH_TextureInfo (int tex, char name[9], int* width, int* height);

// A wall texture as width*height RGBA pixels, row by row from the
// top, in the game's palette at full brightness.
void		DH_TextureRGBA (int tex, unsigned char* rgba);

// Flats are the floor and ceiling textures, DH_FLATSIZE square, and
// repeat on a grid fixed to the map: a point (x,y) shows texel
// (x, -y), each modulo the size.
enum
{
    DH_FLATSIZE = 64
};

// A flat's name; 0 when there is no such flat, as for the markers
// between F_START and F_END.
int		DH_FlatInfo (int flat, char name[9]);

// A flat as DH_FLATSIZE*DH_FLATSIZE RGBA pixels, row by row, in the
// game's palette at full brightness.
void		DH_FlatRGBA (int flat, unsigned char* rgba);

#ifdef __cplusplus
}
#endif

#endif
