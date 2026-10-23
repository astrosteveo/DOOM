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
//	Locating the IWAD and identifying the game it contains.
//
//-----------------------------------------------------------------------------

#ifndef __D_IWAD__
#define __D_IWAD__

// Finds the IWAD (honouring -iwad) and sets gamemode, gamemission
// and language. Returns NULL if none is found.
char* D_FindIWAD (void);

// Searches the IWAD directories for a WAD by file name.
char* D_FindWADByName (const char* name);

// Lower case IWAD name without extension, eg. "doom2".
char* D_IWADBaseName (void);

// Path of savegame slot for the current IWAD.
void D_SaveGamePath (char* buf, int size, int slot);

#endif
