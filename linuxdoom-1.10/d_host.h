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
//	A host reads the game's state and never writes it. What the
//	player does it posts as Doom's own input events, which the game
//	takes as it takes the SDL3 build's, so a demo plays the same under
//	any host as under the SDL3 build.
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
    // The eye a tic before, to draw it in between as Doom does; the same
    // as x, y, z and angle when it didn't move. The view turns between
    // tics only in a demo: playing, the angle is this tic's and turn is
    // how far the mouse will turn it next tic, radians, to turn the view
    // at once with the mouse. So Doom's view looks from oldangle to
    // angle, the short way round, by how far between tics a frame is,
    // plus turn.
    float	oldx, oldy, oldz;
    float	oldangle;
    float	turn;
    // Whether the menu is open, the game paused, a demo playing and the
    // automap up, each 0 or 1.
    int		menu;
    int		paused;
    int		demoplayback;
    int		automap;
} dh_state_t;

void		DH_GetState (dh_state_t* state);

// Input: Doom's event_t. A key is a lowercase character or one of the
// DH_KEY_ codes; a mouse event carries the buttons held, 1 left, 2
// right and 4 middle, and how far the mouse moved right and away from
// the player since the last, in the SDL3 build's mouse counts (a pixel
// of relative motion), which the game scales by its sensitivity.
enum
{
    DH_KEYDOWN,
    DH_KEYUP,
    DH_MOUSE
};

typedef struct
{
    int		type;
    int		data1;		// the key, or the mouse buttons
    int		data2;		// mouse motion right
    int		data3;		// mouse motion forward
} dh_event_t;

// Doom's codes for keys that aren't characters.
enum
{
    DH_KEY_RIGHTARROW = 0xae,
    DH_KEY_LEFTARROW = 0xac,
    DH_KEY_UPARROW = 0xad,
    DH_KEY_DOWNARROW = 0xaf,
    DH_KEY_ESCAPE = 27,
    DH_KEY_ENTER = 13,
    DH_KEY_TAB = 9,
    DH_KEY_F1 = 0x80+0x3b,	// to F10, in order
    DH_KEY_F11 = 0x80+0x57,
    DH_KEY_F12 = 0x80+0x58,
    DH_KEY_BACKSPACE = 127,
    DH_KEY_PAUSE = 0xff,
    DH_KEY_EQUALS = 0x3d,
    DH_KEY_MINUS = 0x2d,
    DH_KEY_RSHIFT = 0x80+0x36,
    DH_KEY_RCTRL = 0x80+0x1d,
    DH_KEY_RALT = 0x80+0x38,
    DH_KEY_CAPSLOCK = 0x80+0x3a,
    DH_KEY_HOME = 0x80+0x47,
    DH_KEY_PGUP = 0x80+0x49,
    DH_KEY_END = 0x80+0x4f,
    DH_KEY_PGDN = 0x80+0x51,
    DH_KEY_INS = 0x80+0x52,
    DH_KEY_DEL = 0x80+0x53
};

// Hands the game an event, at once, so that the menu answers and the
// view turns with the mouse between tics, as in the SDL3 build. While
// a demo plays, its tics come from the demo whatever is posted; a key
// opens the menu, as it does there. Returns as DH_Tic does, since the
// menu can quit the game.
int		DH_PostEvent (const dh_event_t* event);

// The game's controls, which its config binds to keys.
enum
{
    DH_FORWARD,
    DH_BACKWARD,
    DH_TURNLEFT,
    DH_TURNRIGHT,
    DH_STRAFELEFT,
    DH_STRAFERIGHT,
    DH_FIRE,
    DH_USE,
    DH_STRAFE,		// held, turning strafes
    DH_RUN,
    DH_NUMCONTROLS
};

// The key the game's config binds a control to, for a host that has
// bindings of its own to post as that key.
int		DH_ControlKey (int control);

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
//
// bottom and top are the heights of two sector planes, numbered as
// DH_Plane numbers them, and the texture moves up and down with a
// third, pegplane: v1 less pegplane's height, plus top, stays the same
// as they move. Doom draws nothing where top is not above bottom.
typedef struct
{
    float	x1, y1, x2, y2;
    float	bottom, top;
    float	u1, u2, v1, v2;
    int		texture;	// as drawn: animation applied, or DH_SKY
    int		sector;		// the one it faces, for its light
    int		bottomplane, topplane, pegplane;
} dh_wall_t;

// A sector's floor is plane sector*2, and its ceiling sector*2+1.
#define DH_Plane(sector, ceiling)	((sector)*2 + ((ceiling) ? 1 : 0))

// Fills walls with up to max of the level's walls, and returns how
// many there are. Pass NULL and 0 to count them. The same walls come
// back in the same order all level, with empty ones (top at or below
// bottom) among them, until a texture changes; so a host can move
// walls between tics by their planes alone.
int		DH_GetWalls (dh_wall_t* walls, int max);

typedef struct
{
    float	x, y, z;	// the bottom middle, in map units
    float	radius, height;
    float	angle;		// radians counterclockwise from east
    int		type;		// mobjtype_t
    int		flags;		// MF_*
    // Where it was a tic before, to draw it in between: the same as
    // x, y, z and angle when it didn't move, wasn't there, teleported,
    // or the game stood still (paused, or in the menu).
    float	oldx, oldy, oldz;
    float	oldangle;
    // Tells it from every other thing the game made this session, to
    // follow it from tic to tic; never 0.
    int		id;
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
