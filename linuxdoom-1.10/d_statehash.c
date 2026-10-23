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
//	-hashlog <file>: after every tic, write a hash of the game state.
//	Two builds that log the same hashes for a demo simulate it the
//	same way, which is how changes are checked against the original
//	code for vanilla compatibility.
//
//	Only simulation state goes in, never pointers, so a 64-bit build
//	can be compared with a 32-bit one.
//
//-----------------------------------------------------------------------------

#include <stdio.h>

#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "p_local.h"

#include "d_statehash.h"


extern int	prndindex;

static FILE*		hashlog;
static unsigned		hash;


static void HashInt (int value)
{
    unsigned	v;
    int		i;

    // FNV-1a over the four bytes.
    v = (unsigned)value;
    for (i=0 ; i<4 ; i++)
    {
	hash ^= v & 0xff;
	hash *= 16777619u;
	v >>= 8;
    }
}

void D_InitStateHash (void)
{
    int		p;

    p = M_CheckParm ("-hashlog");
    if (p && p < myargc-1)
    {
	hashlog = fopen (myargv[p+1], "w");
	if (!hashlog)
	    fprintf (stderr, "Could not open %s for writing\n", myargv[p+1]);
    }
}

void D_LogStateHash (void)
{
    thinker_t*	th;
    mobj_t*	mo;
    player_t*	player;
    int		i;
    int		j;
    int		things;

    if (!hashlog || gamestate != GS_LEVEL)
	return;

    hash = 2166136261u;
    HashInt (gametic);
    HashInt (leveltime);
    HashInt (prndindex);

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	player = &players[i];
	HashInt (player->playerstate);
	HashInt (player->viewz);
	HashInt (player->health);
	HashInt (player->armorpoints);
	HashInt (player->readyweapon);
	HashInt (player->killcount);
	HashInt (player->itemcount);
	HashInt (player->secretcount);
	for (j=0 ; j<NUMAMMO ; j++)
	    HashInt (player->ammo[j]);
    }

    things = 0;
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 != (actionf_p1)P_MobjThinker)
	    continue;
	mo = (mobj_t *)th;
	HashInt (mo->x);
	HashInt (mo->y);
	HashInt (mo->z);
	HashInt (mo->angle);
	HashInt (mo->momx);
	HashInt (mo->momy);
	HashInt (mo->momz);
	HashInt (mo->type);
	HashInt (mo->health);
	HashInt (mo->flags);
	HashInt (mo->tics);
	HashInt (mo->state - states);
	HashInt (mo->movedir);
	HashInt (mo->movecount);
	HashInt (mo->reactiontime);
	HashInt (mo->threshold);
	things++;
    }

    for (i=0 ; i<numsectors ; i++)
    {
	HashInt (sectors[i].floorheight);
	HashInt (sectors[i].ceilingheight);
	HashInt (sectors[i].lightlevel);
	HashInt (sectors[i].special);
    }

    fprintf (hashlog, "%d %d %d %08x\n", gametic, things, prndindex, hash);
}
