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
//	DOOM graphics and input on SDL3.
//	The game draws a paletted frame into screens[0]; it is expanded
//	to 32-bit color here, scaled with the 4:3 aspect the original
//	320x200 mode had on a CRT, and presented through SDL_Renderer.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL3/SDL.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#ifdef __GNUG__
#pragma implementation "i_video.h"
#endif
#include "i_video.h"


// Options, saved in the config file.
int		fullscreen = 0;
int		window_width = 0;	// 0 picks a size from the desktop
int		window_height = 0;
int		vsync = 1;
int		aspect_ratio_correct = 1;
int		integer_scaling = 0;
int		smooth_scaling = 1;
int		grabmouse = 1;
int		mouse_novert = 1;
int		render_scale = 0;	// framebuffer = this * 200 rows; 0 to fit the window
int		widescreen = 1;

extern int	usemouse;


static SDL_Window*	window;
static SDL_Renderer*	renderer;

// The game's frame, expanded to 32-bit color.
static SDL_Texture*	screentex;

// For smooth scaling: the frame blown up by a whole multiple with
// nearest filtering, then filtered down linearly to the window.
// Pixels stay sharp but rows and columns come out even.
static SDL_Texture*	upscaletex;
static int		upscalefactor;

static Uint32		palette32[256];

static boolean		resizepending;

static boolean		mousegrabbed;
static float		mouseaccumx;
static float		mouseaccumy;
static int		mousebuttonstate;
static boolean		mousechanged;


//
// TranslateScancode
// Maps physical keys to DOOM key codes, by their US layout position,
// so that bindings like WASD do not move with the keyboard layout.
//
static int TranslateScancode (SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
	return 'a' + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
	return '1' + (sc - SDL_SCANCODE_1);

    switch (sc)
    {
      case SDL_SCANCODE_0:		return '0';
      case SDL_SCANCODE_LEFT:		return KEY_LEFTARROW;
      case SDL_SCANCODE_RIGHT:		return KEY_RIGHTARROW;
      case SDL_SCANCODE_DOWN:		return KEY_DOWNARROW;
      case SDL_SCANCODE_UP:		return KEY_UPARROW;
      case SDL_SCANCODE_ESCAPE:		return KEY_ESCAPE;
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_KP_ENTER:	return KEY_ENTER;
      case SDL_SCANCODE_TAB:		return KEY_TAB;
      case SDL_SCANCODE_F1:		return KEY_F1;
      case SDL_SCANCODE_F2:		return KEY_F2;
      case SDL_SCANCODE_F3:		return KEY_F3;
      case SDL_SCANCODE_F4:		return KEY_F4;
      case SDL_SCANCODE_F5:		return KEY_F5;
      case SDL_SCANCODE_F6:		return KEY_F6;
      case SDL_SCANCODE_F7:		return KEY_F7;
      case SDL_SCANCODE_F8:		return KEY_F8;
      case SDL_SCANCODE_F9:		return KEY_F9;
      case SDL_SCANCODE_F10:		return KEY_F10;
      case SDL_SCANCODE_F11:		return KEY_F11;
      case SDL_SCANCODE_F12:		return KEY_F12;
      case SDL_SCANCODE_BACKSPACE:	return KEY_BACKSPACE;
      case SDL_SCANCODE_DELETE:		return KEY_DEL;
      case SDL_SCANCODE_PAUSE:		return KEY_PAUSE;
      case SDL_SCANCODE_EQUALS:
      case SDL_SCANCODE_KP_EQUALS:
      case SDL_SCANCODE_KP_PLUS:	return KEY_EQUALS;
      case SDL_SCANCODE_MINUS:
      case SDL_SCANCODE_KP_MINUS:	return KEY_MINUS;
      case SDL_SCANCODE_LSHIFT:
      case SDL_SCANCODE_RSHIFT:		return KEY_RSHIFT;
      case SDL_SCANCODE_LCTRL:
      case SDL_SCANCODE_RCTRL:		return KEY_RCTRL;
      case SDL_SCANCODE_LALT:
      case SDL_SCANCODE_RALT:		return KEY_RALT;
      case SDL_SCANCODE_CAPSLOCK:	return KEY_CAPSLOCK;
      case SDL_SCANCODE_HOME:		return KEY_HOME;
      case SDL_SCANCODE_END:		return KEY_END;
      case SDL_SCANCODE_PAGEUP:		return KEY_PGUP;
      case SDL_SCANCODE_PAGEDOWN:	return KEY_PGDN;
      case SDL_SCANCODE_INSERT:		return KEY_INS;
      case SDL_SCANCODE_SPACE:		return ' ';
      case SDL_SCANCODE_APOSTROPHE:	return '\'';
      case SDL_SCANCODE_COMMA:		return ',';
      case SDL_SCANCODE_PERIOD:		return '.';
      case SDL_SCANCODE_SLASH:		return '/';
      case SDL_SCANCODE_SEMICOLON:	return ';';
      case SDL_SCANCODE_LEFTBRACKET:	return '[';
      case SDL_SCANCODE_RIGHTBRACKET:	return ']';
      case SDL_SCANCODE_BACKSLASH:	return '\\';
      case SDL_SCANCODE_GRAVE:		return '`';
      case SDL_SCANCODE_KP_0:		return '0';
      case SDL_SCANCODE_KP_1:		return '1';
      case SDL_SCANCODE_KP_2:		return '2';
      case SDL_SCANCODE_KP_3:		return '3';
      case SDL_SCANCODE_KP_4:		return '4';
      case SDL_SCANCODE_KP_5:		return '5';
      case SDL_SCANCODE_KP_6:		return '6';
      case SDL_SCANCODE_KP_7:		return '7';
      case SDL_SCANCODE_KP_8:		return '8';
      case SDL_SCANCODE_KP_9:		return '9';
      case SDL_SCANCODE_KP_DIVIDE:	return '/';
      case SDL_SCANCODE_KP_MULTIPLY:	return '*';
      case SDL_SCANCODE_KP_PERIOD:	return '.';
      default:				return 0;
    }
}


//
// UpdateMouseGrab
// The mouse belongs to the game only while a level is being played;
// menus, pauses, demos and other windows get the cursor back.
//
static void UpdateMouseGrab (void)
{
    boolean	grab;

    grab = grabmouse
	&& usemouse
	&& (SDL_GetWindowFlags (window) & SDL_WINDOW_INPUT_FOCUS)
	&& gamestate == GS_LEVEL
	&& !menuactive
	&& !paused
	&& !demoplayback;

    if (grab == mousegrabbed)
	return;

    SDL_SetWindowRelativeMouseMode (window, grab);
    mousegrabbed = grab;
    mouseaccumx = mouseaccumy = 0;
}


void I_ShutdownGraphics(void)
{
    if (!window)
	return;

    SDL_SetWindowRelativeMouseMode (window, false);
    if (upscaletex)
	SDL_DestroyTexture (upscaletex);
    if (screentex)
	SDL_DestroyTexture (screentex);
    if (renderer)
	SDL_DestroyRenderer (renderer);
    SDL_DestroyWindow (window);
    window = NULL;
    renderer = NULL;
    screentex = upscaletex = NULL;
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}


static void ToggleFullscreen (void)
{
    fullscreen = !fullscreen;
    SDL_SetWindowFullscreen (window, fullscreen);
}

static void HandleKey (SDL_KeyboardEvent* key)
{
    event_t	event;

    // Alt+Enter is the usual fullscreen toggle; keep it from the game.
    if (key->down && key->scancode == SDL_SCANCODE_RETURN
	&& (key->mod & SDL_KMOD_ALT))
    {
	if (!key->repeat)
	    ToggleFullscreen ();
	return;
    }

    // Held keys repeat only where the menus expect it,
    // so that toggles like the automap don't flicker.
    if (key->repeat && !menuactive)
	return;

    event.data1 = TranslateScancode (key->scancode);
    if (!event.data1)
	return;

    event.type = key->down ? ev_keydown : ev_keyup;
    event.data2 = event.data3 = 0;
    D_PostEvent (&event);
}

static void HandleMouseButton (SDL_MouseButtonEvent* button)
{
    int		bit;

    // DOOM numbers buttons left, right, middle.
    switch (button->button)
    {
      case SDL_BUTTON_LEFT:	bit = 1; break;
      case SDL_BUTTON_RIGHT:	bit = 2; break;
      case SDL_BUTTON_MIDDLE:	bit = 4; break;
      default:			return;
    }

    if (button->down)
	mousebuttonstate |= bit;
    else
	mousebuttonstate &= ~bit;
    mousechanged = true;
}

//
// PostMouseEvent
// G_Responder takes the motion of a mouse event as the motion for
// the whole tic, so everything since the last tic goes in one event.
//
static void PostMouseEvent (void)
{
    event_t	event;
    int		dx;
    int		dy;

    dx = (int)mouseaccumx;
    dy = (int)mouseaccumy;
    mouseaccumx -= dx;
    mouseaccumy -= dy;

    if (mouse_novert)
	dy = 0;

    if (!dx && !dy && !mousechanged)
	return;

    event.type = ev_mouse;
    event.data1 = mousebuttonstate;
    event.data2 = dx;
    event.data3 = -dy;
    D_PostEvent (&event);
    mousechanged = false;
}

//
// I_StartTic
//
void I_StartTic (void)
{
    SDL_Event	sdlevent;

    if (!window)
	return;

    while (SDL_PollEvent (&sdlevent))
    {
	switch (sdlevent.type)
	{
	  case SDL_EVENT_QUIT:
	    I_Quit ();
	    break;

	  case SDL_EVENT_KEY_DOWN:
	  case SDL_EVENT_KEY_UP:
	    HandleKey (&sdlevent.key);
	    break;

	  case SDL_EVENT_MOUSE_BUTTON_DOWN:
	  case SDL_EVENT_MOUSE_BUTTON_UP:
	    if (mousegrabbed)
		HandleMouseButton (&sdlevent.button);
	    break;

	  case SDL_EVENT_MOUSE_MOTION:
	    if (mousegrabbed)
	    {
		mouseaccumx += sdlevent.motion.xrel;
		mouseaccumy += sdlevent.motion.yrel;
	    }
	    break;

	  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	    resizepending = true;
	    break;

	  case SDL_EVENT_WINDOW_FOCUS_LOST:
	    // Don't leave buttons held down while away.
	    if (mousebuttonstate)
	    {
		mousebuttonstate = 0;
		mousechanged = true;
	    }
	    break;

	  default:
	    break;
	}
    }

    UpdateMouseGrab ();
    if (mousegrabbed || mousechanged)
	PostMouseEvent ();
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}


//
// GetDestRect
// Where the frame goes in the window: the largest rectangle of the
// right shape that fits, centered.
//
static void GetDestRect (int outw, int outh, SDL_FRect* rect)
{
    double	aspect;
    double	pixelaspect;
    double	w;
    double	h;
    int		scale;

    // 320x200 filled a 4:3 monitor, so pixels were 20% taller than wide.
    pixelaspect = aspect_ratio_correct ? 1.2 : 1.0;
    aspect = SCREENWIDTH / (SCREENHEIGHT * pixelaspect);

    scale = 0;
    if (integer_scaling)
    {
	scale = outw / SCREENWIDTH;
	if (outh / (int)(SCREENHEIGHT*pixelaspect) < scale)
	    scale = outh / (int)(SCREENHEIGHT*pixelaspect);
    }

    if (scale >= 1)
    {
	w = SCREENWIDTH * scale;
	h = (int)(SCREENHEIGHT*pixelaspect) * scale;
    }
    else if (outw > outh * aspect)
    {
	h = outh;
	w = h * aspect;
    }
    else
    {
	w = outw;
	h = w / aspect;
    }

    rect->w = floor (w);
    rect->h = floor (h);
    rect->x = floor ((outw - rect->w) / 2);
    rect->y = floor ((outh - rect->h) / 2);
}

//
// UpdateUpscaleTexture
// Keeps the intermediate texture big enough that the final linear
// pass only ever scales down. Returns false if the framebuffer is
// already that big, as it normally is.
//
static boolean UpdateUpscaleTexture (const SDL_FRect* dest)
{
    int		factor;

    factor = (int)ceil (dest->h / SCREENHEIGHT);
    if (factor < (int)ceil (dest->w / SCREENWIDTH))
	factor = (int)ceil (dest->w / SCREENWIDTH);
    if (factor <= 1)
	return false;

    if (upscaletex && factor == upscalefactor)
	return true;

    if (upscaletex)
	SDL_DestroyTexture (upscaletex);
    upscaletex = SDL_CreateTexture (renderer, SDL_PIXELFORMAT_XRGB8888,
				    SDL_TEXTUREACCESS_TARGET,
				    SCREENWIDTH*factor, SCREENHEIGHT*factor);
    if (!upscaletex)
	I_Error ("Could not create upscale texture: %s", SDL_GetError ());
    SDL_SetTextureScaleMode (upscaletex, SDL_SCALEMODE_LINEAR);
    upscalefactor = factor;
    return true;
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    static int	lasttic;
    int		tics;
    int		i;
    int		x;
    int		y;
    int		outw;
    int		outh;
    void*	pixels;
    int		pitch;
    byte*	src;
    Uint32*	dest;
    SDL_FRect	destrect;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;

    }

    // Expand the paletted frame.
    if (!SDL_LockTexture (screentex, NULL, &pixels, &pitch))
	I_Error ("I_FinishUpdate: SDL_LockTexture: %s", SDL_GetError ());

    src = screens[0];
    for (y=0 ; y<SCREENHEIGHT ; y++)
    {
	dest = (Uint32 *)((byte *)pixels + y*pitch);
	for (x=0 ; x<SCREENWIDTH ; x++)
	    dest[x] = palette32[*src++];
    }
    SDL_UnlockTexture (screentex);

    SDL_GetCurrentRenderOutputSize (renderer, &outw, &outh);
    GetDestRect (outw, outh, &destrect);

    SDL_SetRenderDrawColor (renderer, 0, 0, 0, 255);
    SDL_RenderClear (renderer);

    if (smooth_scaling && !integer_scaling && UpdateUpscaleTexture (&destrect))
    {
	SDL_SetTextureScaleMode (screentex, SDL_SCALEMODE_NEAREST);
	SDL_SetRenderTarget (renderer, upscaletex);
	SDL_RenderTexture (renderer, screentex, NULL, NULL);
	SDL_SetRenderTarget (renderer, NULL);
	SDL_RenderTexture (renderer, upscaletex, NULL, &destrect);
    }
    else
    {
	SDL_SetTextureScaleMode (screentex, smooth_scaling && !integer_scaling
				 ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	SDL_RenderTexture (renderer, screentex, NULL, &destrect);
    }

    SDL_RenderPresent (renderer);
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    int		i;
    int		r;
    int		g;
    int		b;

    for (i=0 ; i<256 ; i++)
    {
	r = gammatable[usegamma][*palette++];
	g = gammatable[usegamma][*palette++];
	b = gammatable[usegamma][*palette++];
	palette32[i] = 0xff000000u | (r << 16) | (g << 8) | b;
    }
}


//
// ChooseResolution
// Sets the framebuffer size for the window's current size.
//
static void ChooseResolution (void)
{
    int		w;
    int		h;
    int		scale;
    double	pixelaspect;
    double	aspect;

    SDL_GetWindowSizeInPixels (window, &w, &h);
    if (w < 1 || h < 1)
	return;

    pixelaspect = aspect_ratio_correct ? 1.2 : 1.0;

    // Enough rows that the picture is only ever scaled down.
    scale = render_scale;
    if (scale <= 0)
	scale = (int)ceil (h / (ORIGHEIGHT * pixelaspect));

    if (widescreen)
	aspect = (double)w / h * pixelaspect;
    else
	aspect = (double)ORIGWIDTH / ORIGHEIGHT;

    V_SetResolution (scale, aspect);
}

static void CreateScreenTexture (void)
{
    if (screentex)
	SDL_DestroyTexture (screentex);
    if (upscaletex)
	SDL_DestroyTexture (upscaletex);
    upscaletex = NULL;

    screentex = SDL_CreateTexture (renderer, SDL_PIXELFORMAT_XRGB8888,
				   SDL_TEXTUREACCESS_STREAMING,
				   SCREENWIDTH, SCREENHEIGHT);
    if (!screentex)
	I_Error ("Could not create screen texture: %s", SDL_GetError ());
}

//
// I_CheckResize
// Called between frames. If the window changed size, picks a new
// framebuffer size and returns true if it differs, after which the
// screens must be reallocated and everything redrawn.
//
boolean I_CheckResize (void)
{
    int		oldw;
    int		oldh;

    if (!resizepending)
	return false;
    resizepending = false;

    oldw = SCREENWIDTH;
    oldh = SCREENHEIGHT;
    ChooseResolution ();
    if (SCREENWIDTH == oldw && SCREENHEIGHT == oldh)
	return false;

    CreateScreenTexture ();
    printf ("I_CheckResize: %dx%d framebuffer\n", SCREENWIDTH, SCREENHEIGHT);
    return true;
}

//
// PickWindowSize
// Without a configured size, take the largest whole multiple of
// 320x240 that leaves some of the desktop showing.
//
static void PickWindowSize (int* w, int* h)
{
    const SDL_DisplayMode*	mode;
    int				scale;

    if (window_width > 0 && window_height > 0)
    {
	*w = window_width;
	*h = window_height;
	return;
    }

    scale = 3;
    mode = SDL_GetDesktopDisplayMode (SDL_GetPrimaryDisplay ());
    if (mode)
    {
	scale = 1;
	while (320*(scale+1) <= mode->w * 0.85
	       && 240*(scale+1) <= mode->h * 0.85)
	    scale++;
    }

    *w = 320*scale;
    *h = 240*scale;
}

void I_InitGraphics(void)
{
    static int		firsttime=1;
    int			w;
    int			h;
    SDL_WindowFlags	flags;

    if (!firsttime)
	return;
    firsttime = 0;

    if (M_CheckParm ("-fullscreen"))
	fullscreen = 1;
    if (M_CheckParm ("-window"))
	fullscreen = 0;
    if (M_CheckParm ("-nograbmouse"))
	grabmouse = 0;
    {
	int	p = M_CheckParm ("-scale");

	if (p && p < myargc-1)
	    render_scale = atoi (myargv[p+1]);
    }

    if (!SDL_InitSubSystem (SDL_INIT_VIDEO))
	I_Error ("Could not initialize SDL video: %s", SDL_GetError ());

    PickWindowSize (&w, &h);

    flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (fullscreen)
	flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow (PROGRAM_TITLE, w, h, flags);
    if (!window)
	I_Error ("Could not create window: %s", SDL_GetError ());
    SDL_SetWindowMinimumSize (window, ORIGWIDTH, ORIGWIDTH*3/4);

    renderer = SDL_CreateRenderer (window, NULL);
    if (!renderer)
	I_Error ("Could not create renderer: %s", SDL_GetError ());
    SDL_SetRenderVSync (renderer, vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED);

    // On Wayland the window's real size is only known once the
    // compositor has configured it.
    SDL_SyncWindow (window);
    SDL_GetWindowSizeInPixels (window, &w, &h);

    ChooseResolution ();
    CreateScreenTexture ();

    printf ("I_InitGraphics: %dx%d window (%s), %dx%d framebuffer, %s renderer\n",
	    w, h, SDL_GetCurrentVideoDriver (), SCREENWIDTH, SCREENHEIGHT,
	    SDL_GetRendererName (renderer));

    UpdateMouseGrab ();
}
