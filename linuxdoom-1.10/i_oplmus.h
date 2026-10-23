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
//	MUS music on an emulated OPL3. Not thread safe: the caller
//	serializes these with the audio callback that calls Render.
//
//-----------------------------------------------------------------------------

#ifndef __I_OPLMUS__
#define __I_OPLMUS__

#include "doomtype.h"

// genmidi is the GENMIDI lump, which must stay loaded.
// opl3 selects DMX's OPL3 mode: 18 voices and stereo.
boolean OPLMUS_Init (int samplerate, boolean opl3,
		     const byte* genmidi, int genmidilen);
void OPLMUS_Shutdown (void);

// 0..127
void OPLMUS_SetVolume (int volume);

boolean OPLMUS_IsMUS (const byte* data, int len);

// The song data must stay valid until stopped.
void OPLMUS_Play (const byte* data, int len, boolean looping);
void OPLMUS_Stop (void);
void OPLMUS_Pause (boolean paused);
boolean OPLMUS_Playing (void);

// Interleaved stereo.
void OPLMUS_Render (short* out, int frames);

#endif
