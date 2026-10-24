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

// How the host plays the game's sound, set before DH_Start; no audio
// device is opened either way. By default the host plays each sound
// itself, from DH_GetSounds, and the music from DH_RenderAudio. With
// mix set, DH_RenderAudio gives the whole mix instead, music and sound
// as the SDL3 build plays them, at the menu's volumes.
void		DH_MixSound (int mix);

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
    // The PLAYPAL palette Doom shows the screen in, for a flash: 0 for
    // none, 1 to 8 red for damage and berserk, 9 to 12 gold for a
    // pickup, 13 green for the radiation suit. DH_PaletteTint says
    // what each looks like.
    int		palette;
    // The menu's sound and music volumes, 0 to 127.
    int		sfxvolume;
    int		musicvolume;
} dh_state_t;

void		DH_GetState (dh_state_t* state);

// Input: Doom's event_t. A key is a lowercase character or one of the
// DH_KEY_ codes; a mouse event carries the buttons held, 1 left, 2
// right and 4 middle, and how far the mouse moved right and away from
// the player since the last, in the SDL3 build's mouse counts (a pixel
// of relative motion), which the game scales by its sensitivity. A
// pointer event is the cursor, for the menu, while the mouse isn't the
// game's: the buttons held as for the mouse, with 8 and 16 while the
// wheel turns up and down, and where the cursor is in the screen's
// pixels (DH_GetScreen), right and down from the top left. The game
// itself never sees it, so it can't change how a level plays.
enum
{
    DH_KEYDOWN,
    DH_KEYUP,
    DH_MOUSE,
    DH_POINTER
};

typedef struct
{
    int		type;
    int		data1;		// the key, or the mouse buttons
    int		data2;		// mouse motion right, or the cursor's x
    int		data3;		// mouse motion forward, or the cursor's y
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

// The screen: what Doom draws in 2D, the status bar, messages, menus,
// the automap, the intermission and the rest, and the player's weapon,
// for a host to show over the 3D view it draws itself. The view is
// left transparent, where Doom would draw it.

// The host's window, in pixels. The screen is sized for it as the SDL3
// build sizes its framebuffer for its window, as wide as the window
// and twice the original 200 rows, or -scale times. 640x480 until a
// host says otherwise.
void		DH_SetScreenSize (int width, int height);

// Draws the screen as it is now, fraction (0 to 1) of the way from the
// last tic to the next, as the weapon bobs. Returns as DH_Tic does.
int		DH_Draw (float fraction);

typedef struct
{
    int		width, height;	// pixels
    // Where the 3D view shows, in pixels from the top left; all 0 when
    // the screen covers it, as for the automap or the intermission.
    int		viewx, viewy;
    int		viewwidth, viewheight;
    // How many pixels from the view's middle Doom sees 45 degrees to
    // the side: its view is 90 degrees across twice this, and a pixel
    // up is as far as a pixel across.
    float	projection;
} dh_screen_t;

void		DH_GetScreen (dh_screen_t* screen);

// The screen as width*height RGBA pixels, row by row from the top, in
// the game's first palette, with alpha 0 where the 3D view shows.
void		DH_ScreenRGBA (unsigned char* rgba);

// What a palette flash looks like: the color, 0 to 1, that the screen
// is blended toward, and how far, 0 to 1.
void		DH_PaletteTint (int palette, float rgb[3], float* amount);

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
    // What Doom draws it as, this tic: a frame of a sprite, for
    // DH_SpriteFrame, and whether the frame is full bright, drawn at
    // full light whatever its sector's. MF_SHADOW in flags draws it
    // as fuzz instead: what is behind it, a little darker, shifted up
    // or down a row.
    int		sprite;
    int		frame;
    int		fullbright;
    int		sector;		// the one it is in, for its light
} dh_thing_t;

// The same for the things in the level.
int		DH_GetThings (dh_thing_t* things, int max);

// Sprites are drawn from sprite lumps, patches numbered from 0 up to
// DH_SpriteLumps, standing upright and facing the eye: across, along
// the view plane, and up, along the vertical, a pixel to a map unit.
int		DH_SpriteLumps (void);

// A sprite lump's name and size, in pixels, and where the thing
// stands on it: the thing's x and y are leftoffset pixels in from the
// left edge, and its z topoffset pixels below the top edge. 0 when
// there is no such lump.
int		DH_SpriteInfo (int lump, char name[9], int* width, int* height,
			       int* leftoffset, int* topoffset);

// A sprite lump as width*height pairs of bytes, row by row from the
// top: the pixel's palette index, and 255 where it has a pixel or 0
// where it has none. DH_ColormapRGBA gives the colors, at any light.
void		DH_SpritePixels (int lump, unsigned char* pixels);

// COLORMAP, which Doom draws everything through: DH_COLORMAPS rows of
// 256 RGBA colors, each palette index as drawn at one light. Row 0 is
// full brightness, the palette itself; each row is a step darker, to
// black at 31; 32 is invulnerability's inverted gray.
enum
{
    DH_COLORMAPS = 34
};

void		DH_ColormapRGBA (unsigned char* rgba);

// A frame of a sprite: the lump for each of 8 views of the thing, and
// whether it is drawn mirrored, left for right with its offsets kept.
// The view, as R_ProjectSprite picks it, is the angle from the eye to
// the thing, less the thing's angle, plus 202.5 degrees, in 45 degree
// steps from 0 to 7: 0 seen from the front, 4 from behind. Returns 1
// when the views differ, 0 when the frame looks the same from all
// round (all 8 are the same), and -1 when there is no such frame.
int		DH_SpriteFrame (int sprite, int frame, int lumps[8], int flips[8]);

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

// Sound. The game decides what plays as it does for the SDL3 build:
// a few channels, one sound to a thing, the more important sound
// cutting off the less, and nothing beyond 1200 units. The host plays
// what DH_GetSounds says is playing. Where a sound comes from:
enum
{
    DH_SOUND_LOCAL,	// nowhere: the player's own sounds and the menu's
    DH_SOUND_THING,	// a thing, by its id; it moves with the thing
    DH_SOUND_SECTOR,	// a door, lift or floor: the sector's middle
    DH_SOUND_POINT	// somewhere else on the map
};

typedef struct
{
    // A new handle each time a sound starts, which stays the same
    // until it stops: when the sound ends, is cut off, or its thing
    // goes, it is no longer listed.
    int		handle;
    int		sfx;		// for DH_SoundInfo
    // As Doom would play it: the volume, 0 to 127, the menu's sound
    // volume less for distance; the stereo separation, 0 all left,
    // 128 the middle and 255 all right; and how many times faster than
    // its own rate it plays, 1 unless the config shifts pitches.
    int		volume;
    int		sep;
    float	speed;
    int		origin;		// DH_SOUND_
    int		id;		// the thing's id, or the sector's number
    // Where it is, in map units, and was a tic before, as for things:
    // a thing's middle, the sector's middle between floor and
    // ceiling, or a point at the height of the player's eye. 0 for a
    // local sound.
    float	x, y, z;
    float	oldx, oldy, oldz;
} dh_sound_t;

// The same for the sounds playing now, as DH_GetThings. They change
// on tics and when an event makes a menu sound.
int		DH_GetSounds (dh_sound_t* sounds, int max);

// A sound effect's lump name, its rate (samples a second) and length;
// 0 when there is no such sound, or it has no samples, as for Doom II
// sounds in Doom. Sounds are numbered from 1.
int		DH_SoundInfo (int sfx, char name[9], int* rate, int* length);

// Its samples, 8-bit unsigned, 128 the middle.
void		DH_SoundSamples (int sfx, unsigned char* samples);

typedef struct
{
    char	name[9];	// the lump, D_E1M1 and so on
    int		looping;	// it starts over at the end
    int		paused;		// with the game; it goes on from there
} dh_music_t;

// The music playing, if any: 0 when none, or the music is off.
int		DH_GetMusic (dh_music_t* music);

// The rate DH_RenderAudio renders at, samples a second.
int		DH_SampleRate (void);

// The next frames of the music, interleaved stereo, -1 to 1: and of
// the sounds too, mixed, with DH_MixSound. The music moves on by as
// many frames as the host renders, so it keeps time with the host's
// audio, not its frame rate. Silence when the sound is off.
void		DH_RenderAudio (float* stereo, int frames);

#ifdef __cplusplus
}
#endif

#endif
