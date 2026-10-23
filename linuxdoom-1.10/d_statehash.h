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
//	Per-tic game state hashes, for checking demo compatibility.
//
//-----------------------------------------------------------------------------

#ifndef __D_STATEHASH__
#define __D_STATEHASH__

// Opens the -hashlog file, if given.
void D_InitStateHash (void);

// Called after every tic.
void D_LogStateHash (void);

#endif
