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
//	Archiving: SaveGame I/O.
//
//	The original wrote structures straight out of memory, so the
//	format is the in-memory layout of a 32-bit DOS build: 4-byte
//	pointers and enums, little endian. Each structure is written
//	field by field here to reproduce that layout exactly, which
//	keeps savegames interchangeable with DOOM v1.9.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "z_zone.h"
#include "p_local.h"

// State.
#include "doomstat.h"
#include "r_state.h"

byte*		save_p;


// Pads save_p to a 4-byte boundary
//  so that the load/save works on SGI&Gecko.
#define PADSAVEP()	save_p += (4 - ((uintptr_t) save_p & 3)) & 3



//
// Primitive I/O. Pointers are written as 32-bit placeholders: the
// loader never trusts them, except to tell NULL from not NULL.
//

static void saveg_write8 (int value)
{
    *save_p++ = value;
}

static void saveg_write16 (int value)
{
    saveg_write8 (value);
    saveg_write8 (value >> 8);
}

static void saveg_write32 (int value)
{
    saveg_write8 (value);
    saveg_write8 (value >> 8);
    saveg_write8 (value >> 16);
    saveg_write8 (value >> 24);
}

static void saveg_writep (const void* p)
{
    saveg_write32 (p ? 1 : 0);
}

static int saveg_read8 (void)
{
    return *save_p++;
}

static int saveg_read16 (void)
{
    int		value;

    value = saveg_read8 ();
    value |= saveg_read8 () << 8;
    return (short)value;
}

static int saveg_read32 (void)
{
    unsigned	value;

    value = saveg_read8 ();
    value |= saveg_read8 () << 8;
    value |= saveg_read8 () << 16;
    value |= (unsigned)saveg_read8 () << 24;
    return (int)value;
}

// A pointer field: non-zero when the original pointer was set.
#define saveg_readp()	saveg_read32 ()

static void saveg_pad (int bytes)
{
    while (bytes--)
	saveg_write8 (0);
}



//
// thinker_t: prev, next, function (12 bytes)
//
static void saveg_write_thinker (thinker_t* th)
{
    saveg_writep (th->prev);
    saveg_writep (th->next);
    saveg_writep (th->function.acv);
}

// Returns whether the saved function was set.
static boolean saveg_read_thinker (void)
{
    saveg_readp ();
    saveg_readp ();
    return saveg_readp () != 0;
}


//
// pspdef_t (16 bytes), with the state as an index into states[]
//
static void saveg_write_pspdef (pspdef_t* psp)
{
    saveg_write32 (psp->state ? psp->state - states : 0);
    saveg_write32 (psp->tics);
    saveg_write32 (psp->sx);
    saveg_write32 (psp->sy);
}

static void saveg_read_pspdef (pspdef_t* psp)
{
    int		state;

    state = saveg_read32 ();
    psp->state = state ? &states[state] : NULL;
    psp->tics = saveg_read32 ();
    psp->sx = saveg_read32 ();
    psp->sy = saveg_read32 ();
}


//
// ticcmd_t (8 bytes)
//
static void saveg_write_ticcmd (ticcmd_t* cmd)
{
    saveg_write8 (cmd->forwardmove);
    saveg_write8 (cmd->sidemove);
    saveg_write16 (cmd->angleturn);
    saveg_write16 (cmd->consistancy);
    saveg_write8 (cmd->chatchar);
    saveg_write8 (cmd->buttons);
}

static void saveg_read_ticcmd (ticcmd_t* cmd)
{
    cmd->forwardmove = saveg_read8 ();
    cmd->sidemove = saveg_read8 ();
    cmd->angleturn = saveg_read16 ();
    cmd->consistancy = saveg_read16 ();
    cmd->chatchar = saveg_read8 ();
    cmd->buttons = saveg_read8 ();
}


//
// player_t (280 bytes)
//
static void saveg_write_player (player_t* p)
{
    int		i;

    saveg_writep (p->mo);
    saveg_write32 (p->playerstate);
    saveg_write_ticcmd (&p->cmd);
    saveg_write32 (p->viewz);
    saveg_write32 (p->viewheight);
    saveg_write32 (p->deltaviewheight);
    saveg_write32 (p->bob);
    saveg_write32 (p->health);
    saveg_write32 (p->armorpoints);
    saveg_write32 (p->armortype);
    for (i=0 ; i<NUMPOWERS ; i++)
	saveg_write32 (p->powers[i]);
    for (i=0 ; i<NUMCARDS ; i++)
	saveg_write32 (p->cards[i]);
    saveg_write32 (p->backpack);
    for (i=0 ; i<MAXPLAYERS ; i++)
	saveg_write32 (p->frags[i]);
    saveg_write32 (p->readyweapon);
    saveg_write32 (p->pendingweapon);
    for (i=0 ; i<NUMWEAPONS ; i++)
	saveg_write32 (p->weaponowned[i]);
    for (i=0 ; i<NUMAMMO ; i++)
	saveg_write32 (p->ammo[i]);
    for (i=0 ; i<NUMAMMO ; i++)
	saveg_write32 (p->maxammo[i]);
    saveg_write32 (p->attackdown);
    saveg_write32 (p->usedown);
    saveg_write32 (p->cheats);
    saveg_write32 (p->refire);
    saveg_write32 (p->killcount);
    saveg_write32 (p->itemcount);
    saveg_write32 (p->secretcount);
    saveg_writep (p->message);
    saveg_write32 (p->damagecount);
    saveg_write32 (p->bonuscount);
    saveg_writep (p->attacker);
    saveg_write32 (p->extralight);
    saveg_write32 (p->fixedcolormap);
    saveg_write32 (p->colormap);
    for (i=0 ; i<NUMPSPRITES ; i++)
	saveg_write_pspdef (&p->psprites[i]);
    saveg_write32 (p->didsecret);
}

static void saveg_read_player (player_t* p)
{
    int		i;

    // will be set when unarc thinker
    saveg_readp ();
    p->mo = NULL;
    p->playerstate = saveg_read32 ();
    saveg_read_ticcmd (&p->cmd);
    p->viewz = saveg_read32 ();
    p->viewheight = saveg_read32 ();
    p->deltaviewheight = saveg_read32 ();
    p->bob = saveg_read32 ();
    p->health = saveg_read32 ();
    p->armorpoints = saveg_read32 ();
    p->armortype = saveg_read32 ();
    for (i=0 ; i<NUMPOWERS ; i++)
	p->powers[i] = saveg_read32 ();
    for (i=0 ; i<NUMCARDS ; i++)
	p->cards[i] = saveg_read32 ();
    p->backpack = saveg_read32 ();
    for (i=0 ; i<MAXPLAYERS ; i++)
	p->frags[i] = saveg_read32 ();
    p->readyweapon = saveg_read32 ();
    p->pendingweapon = saveg_read32 ();
    for (i=0 ; i<NUMWEAPONS ; i++)
	p->weaponowned[i] = saveg_read32 ();
    for (i=0 ; i<NUMAMMO ; i++)
	p->ammo[i] = saveg_read32 ();
    for (i=0 ; i<NUMAMMO ; i++)
	p->maxammo[i] = saveg_read32 ();
    p->attackdown = saveg_read32 ();
    p->usedown = saveg_read32 ();
    p->cheats = saveg_read32 ();
    p->refire = saveg_read32 ();
    p->killcount = saveg_read32 ();
    p->itemcount = saveg_read32 ();
    p->secretcount = saveg_read32 ();
    saveg_readp ();
    p->message = NULL;
    p->damagecount = saveg_read32 ();
    p->bonuscount = saveg_read32 ();
    saveg_readp ();
    p->attacker = NULL;
    p->extralight = saveg_read32 ();
    p->fixedcolormap = saveg_read32 ();
    p->colormap = saveg_read32 ();
    for (i=0 ; i<NUMPSPRITES ; i++)
	saveg_read_pspdef (&p->psprites[i]);
    p->didsecret = saveg_read32 ();
}


//
// mobj_t (156 bytes)
//
static void saveg_write_mobj (mobj_t* mobj)
{
    saveg_write_thinker (&mobj->thinker);
    saveg_write32 (mobj->x);
    saveg_write32 (mobj->y);
    saveg_write32 (mobj->z);
    saveg_writep (mobj->snext);
    saveg_writep (mobj->sprev);
    saveg_write32 (mobj->angle);
    saveg_write32 (mobj->sprite);
    saveg_write32 (mobj->frame);
    saveg_writep (mobj->bnext);
    saveg_writep (mobj->bprev);
    saveg_writep (mobj->subsector);
    saveg_write32 (mobj->floorz);
    saveg_write32 (mobj->ceilingz);
    saveg_write32 (mobj->radius);
    saveg_write32 (mobj->height);
    saveg_write32 (mobj->momx);
    saveg_write32 (mobj->momy);
    saveg_write32 (mobj->momz);
    saveg_write32 (mobj->validcount);
    saveg_write32 (mobj->type);
    saveg_writep (mobj->info);
    saveg_write32 (mobj->tics);
    saveg_write32 (mobj->state - states);
    saveg_write32 (mobj->flags);
    saveg_write32 (mobj->health);
    saveg_write32 (mobj->movedir);
    saveg_write32 (mobj->movecount);
    saveg_writep (mobj->target);
    saveg_write32 (mobj->reactiontime);
    saveg_write32 (mobj->threshold);
    saveg_write32 (mobj->player ? (mobj->player - players) + 1 : 0);
    saveg_write32 (mobj->lastlook);
    saveg_write16 (mobj->spawnpoint.x);
    saveg_write16 (mobj->spawnpoint.y);
    saveg_write16 (mobj->spawnpoint.angle);
    saveg_write16 (mobj->spawnpoint.type);
    saveg_write16 (mobj->spawnpoint.options);
    saveg_pad (2);
    saveg_writep (mobj->tracer);
}

static void saveg_read_mobj (mobj_t* mobj)
{
    int		player;

    saveg_read_thinker ();
    mobj->x = saveg_read32 ();
    mobj->y = saveg_read32 ();
    mobj->z = saveg_read32 ();
    saveg_readp ();
    saveg_readp ();
    mobj->angle = saveg_read32 ();
    mobj->sprite = saveg_read32 ();
    mobj->frame = saveg_read32 ();
    saveg_readp ();
    saveg_readp ();
    saveg_readp ();
    mobj->floorz = saveg_read32 ();
    mobj->ceilingz = saveg_read32 ();
    mobj->radius = saveg_read32 ();
    mobj->height = saveg_read32 ();
    mobj->momx = saveg_read32 ();
    mobj->momy = saveg_read32 ();
    mobj->momz = saveg_read32 ();
    mobj->validcount = saveg_read32 ();
    mobj->type = saveg_read32 ();
    saveg_readp ();
    mobj->tics = saveg_read32 ();
    mobj->state = &states[saveg_read32 ()];
    mobj->flags = saveg_read32 ();
    mobj->health = saveg_read32 ();
    mobj->movedir = saveg_read32 ();
    mobj->movecount = saveg_read32 ();
    saveg_readp ();
    mobj->target = NULL;
    mobj->reactiontime = saveg_read32 ();
    mobj->threshold = saveg_read32 ();
    player = saveg_read32 ();
    mobj->player = player ? &players[player-1] : NULL;
    mobj->lastlook = saveg_read32 ();
    mobj->spawnpoint.x = saveg_read16 ();
    mobj->spawnpoint.y = saveg_read16 ();
    mobj->spawnpoint.angle = saveg_read16 ();
    mobj->spawnpoint.type = saveg_read16 ();
    mobj->spawnpoint.options = saveg_read16 ();
    save_p += 2;
    // The original kept the stale pointer, which crashed when a
    // loaded tracer missile went looking for its target.
    saveg_readp ();
    mobj->tracer = NULL;
}


//
// Special thinkers. The sector pointer is saved as a sector number.
//

// ceiling_t (48 bytes)
static void saveg_write_ceiling (ceiling_t* c)
{
    saveg_write_thinker (&c->thinker);
    saveg_write32 (c->type);
    saveg_write32 (c->sector - sectors);
    saveg_write32 (c->bottomheight);
    saveg_write32 (c->topheight);
    saveg_write32 (c->speed);
    saveg_write32 (c->crush);
    saveg_write32 (c->direction);
    saveg_write32 (c->tag);
    saveg_write32 (c->olddirection);
}

// Returns whether the ceiling was active (not in stasis).
static boolean saveg_read_ceiling (ceiling_t* c)
{
    boolean	active;

    active = saveg_read_thinker ();
    c->type = saveg_read32 ();
    c->sector = &sectors[saveg_read32 ()];
    c->bottomheight = saveg_read32 ();
    c->topheight = saveg_read32 ();
    c->speed = saveg_read32 ();
    c->crush = saveg_read32 ();
    c->direction = saveg_read32 ();
    c->tag = saveg_read32 ();
    c->olddirection = saveg_read32 ();
    return active;
}

// vldoor_t (40 bytes)
static void saveg_write_door (vldoor_t* d)
{
    saveg_write_thinker (&d->thinker);
    saveg_write32 (d->type);
    saveg_write32 (d->sector - sectors);
    saveg_write32 (d->topheight);
    saveg_write32 (d->speed);
    saveg_write32 (d->direction);
    saveg_write32 (d->topwait);
    saveg_write32 (d->topcountdown);
}

static void saveg_read_door (vldoor_t* d)
{
    saveg_read_thinker ();
    d->type = saveg_read32 ();
    d->sector = &sectors[saveg_read32 ()];
    d->topheight = saveg_read32 ();
    d->speed = saveg_read32 ();
    d->direction = saveg_read32 ();
    d->topwait = saveg_read32 ();
    d->topcountdown = saveg_read32 ();
}

// floormove_t (44 bytes)
static void saveg_write_floor (floormove_t* f)
{
    saveg_write_thinker (&f->thinker);
    saveg_write32 (f->type);
    saveg_write32 (f->crush);
    saveg_write32 (f->sector - sectors);
    saveg_write32 (f->direction);
    saveg_write32 (f->newspecial);
    saveg_write16 (f->texture);
    saveg_pad (2);
    saveg_write32 (f->floordestheight);
    saveg_write32 (f->speed);
}

static void saveg_read_floor (floormove_t* f)
{
    saveg_read_thinker ();
    f->type = saveg_read32 ();
    f->crush = saveg_read32 ();
    f->sector = &sectors[saveg_read32 ()];
    f->direction = saveg_read32 ();
    f->newspecial = saveg_read32 ();
    f->texture = saveg_read16 ();
    save_p += 2;
    f->floordestheight = saveg_read32 ();
    f->speed = saveg_read32 ();
}

// plat_t (56 bytes)
static void saveg_write_plat (plat_t* p)
{
    saveg_write_thinker (&p->thinker);
    saveg_write32 (p->sector - sectors);
    saveg_write32 (p->speed);
    saveg_write32 (p->low);
    saveg_write32 (p->high);
    saveg_write32 (p->wait);
    saveg_write32 (p->count);
    saveg_write32 (p->status);
    saveg_write32 (p->oldstatus);
    saveg_write32 (p->crush);
    saveg_write32 (p->tag);
    saveg_write32 (p->type);
}

// Returns whether the platform was active (not in stasis).
static boolean saveg_read_plat (plat_t* p)
{
    boolean	active;

    active = saveg_read_thinker ();
    p->sector = &sectors[saveg_read32 ()];
    p->speed = saveg_read32 ();
    p->low = saveg_read32 ();
    p->high = saveg_read32 ();
    p->wait = saveg_read32 ();
    p->count = saveg_read32 ();
    p->status = saveg_read32 ();
    p->oldstatus = saveg_read32 ();
    p->crush = saveg_read32 ();
    p->tag = saveg_read32 ();
    p->type = saveg_read32 ();
    return active;
}

// lightflash_t (36 bytes)
static void saveg_write_flash (lightflash_t* l)
{
    saveg_write_thinker (&l->thinker);
    saveg_write32 (l->sector - sectors);
    saveg_write32 (l->count);
    saveg_write32 (l->maxlight);
    saveg_write32 (l->minlight);
    saveg_write32 (l->maxtime);
    saveg_write32 (l->mintime);
}

static void saveg_read_flash (lightflash_t* l)
{
    saveg_read_thinker ();
    l->sector = &sectors[saveg_read32 ()];
    l->count = saveg_read32 ();
    l->maxlight = saveg_read32 ();
    l->minlight = saveg_read32 ();
    l->maxtime = saveg_read32 ();
    l->mintime = saveg_read32 ();
}

// strobe_t (36 bytes)
static void saveg_write_strobe (strobe_t* s)
{
    saveg_write_thinker (&s->thinker);
    saveg_write32 (s->sector - sectors);
    saveg_write32 (s->count);
    saveg_write32 (s->minlight);
    saveg_write32 (s->maxlight);
    saveg_write32 (s->darktime);
    saveg_write32 (s->brighttime);
}

static void saveg_read_strobe (strobe_t* s)
{
    saveg_read_thinker ();
    s->sector = &sectors[saveg_read32 ()];
    s->count = saveg_read32 ();
    s->minlight = saveg_read32 ();
    s->maxlight = saveg_read32 ();
    s->darktime = saveg_read32 ();
    s->brighttime = saveg_read32 ();
}

// glow_t (28 bytes)
static void saveg_write_glow (glow_t* g)
{
    saveg_write_thinker (&g->thinker);
    saveg_write32 (g->sector - sectors);
    saveg_write32 (g->minlight);
    saveg_write32 (g->maxlight);
    saveg_write32 (g->direction);
}

static void saveg_read_glow (glow_t* g)
{
    saveg_read_thinker ();
    g->sector = &sectors[saveg_read32 ()];
    g->minlight = saveg_read32 ();
    g->maxlight = saveg_read32 ();
    g->direction = saveg_read32 ();
}



//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
    int		i;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;

	PADSAVEP();
	saveg_write_player (&players[i]);
    }
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
    int		i;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;

	PADSAVEP();
	saveg_read_player (&players[i]);
    }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;

    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	saveg_write16 (sec->floorheight >> FRACBITS);
	saveg_write16 (sec->ceilingheight >> FRACBITS);
	saveg_write16 (sec->floorpic);
	saveg_write16 (sec->ceilingpic);
	saveg_write16 (sec->lightlevel);
	saveg_write16 (sec->special);		// needed?
	saveg_write16 (sec->tag);		// needed?
    }


    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	saveg_write16 (li->flags);
	saveg_write16 (li->special);
	saveg_write16 (li->tag);
	for (j=0 ; j<2 ; j++)
	{
	    if (li->sidenum[j] == -1)
		continue;

	    si = &sides[li->sidenum[j]];

	    saveg_write16 (si->textureoffset >> FRACBITS);
	    saveg_write16 (si->rowoffset >> FRACBITS);
	    saveg_write16 (si->toptexture);
	    saveg_write16 (si->bottomtexture);
	    saveg_write16 (si->midtexture);
	}
    }
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;

    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	sec->floorheight = saveg_read16 () << FRACBITS;
	sec->ceilingheight = saveg_read16 () << FRACBITS;
	sec->floorpic = saveg_read16 ();
	sec->ceilingpic = saveg_read16 ();
	sec->lightlevel = saveg_read16 ();
	sec->special = saveg_read16 ();		// needed?
	sec->tag = saveg_read16 ();		// needed?
	sec->specialdata = 0;
	sec->soundtarget = 0;
    }

    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	li->flags = saveg_read16 ();
	li->special = saveg_read16 ();
	li->tag = saveg_read16 ();
	for (j=0 ; j<2 ; j++)
	{
	    if (li->sidenum[j] == -1)
		continue;
	    si = &sides[li->sidenum[j]];
	    si->textureoffset = saveg_read16 () << FRACBITS;
	    si->rowoffset = saveg_read16 () << FRACBITS;
	    si->toptexture = saveg_read16 ();
	    si->bottomtexture = saveg_read16 ();
	    si->midtexture = saveg_read16 ();
	}
    }
}





//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj

} thinkerclass_t;



//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
    thinker_t*		th;

    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acp1 == (actionf_p1)P_MobjThinker)
	{
	    saveg_write8 (tc_mobj);
	    PADSAVEP();
	    saveg_write_mobj ((mobj_t *)th);
	    continue;
	}

	// I_Error ("P_ArchiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    saveg_write8 (tc_end);
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
    byte		tclass;
    thinker_t*		currentthinker;
    thinker_t*		next;
    mobj_t*		mobj;

    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	next = currentthinker->next;

	if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
	    P_RemoveMobj ((mobj_t *)currentthinker);
	else
	    Z_Free (currentthinker);

	currentthinker = next;
    }
    P_InitThinkers ();

    // read in saved thinkers
    while (1)
    {
	tclass = saveg_read8 ();
	switch (tclass)
	{
	  case tc_end:
	    return; 	// end of list

	  case tc_mobj:
	    PADSAVEP();
	    mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
	    memset (mobj, 0, sizeof(*mobj));
	    saveg_read_mobj (mobj);

	    if (mobj->player)
		mobj->player->mo = mobj;

	    P_SetThingPosition (mobj);
	    mobj->info = &mobjinfo[mobj->type];
	    mobj->floorz = mobj->subsector->sector->floorheight;
	    mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	    mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
	    P_AddThinker (&mobj->thinker);
	    break;

	  default:
	    I_Error ("Unknown tclass %i in savegame",tclass);
	}

    }

}


//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials

} specials_e;



//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    int			i;

    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function.acv == (actionf_v)NULL)
	{
	    // A ceiling in stasis.
	    for (i = 0; i < MAXCEILINGS;i++)
		if (activeceilings[i] == (ceiling_t *)th)
		    break;

	    if (i<MAXCEILINGS)
	    {
		saveg_write8 (tc_ceiling);
		PADSAVEP();
		saveg_write_ceiling ((ceiling_t *)th);
	    }
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
	{
	    saveg_write8 (tc_ceiling);
	    PADSAVEP();
	    saveg_write_ceiling ((ceiling_t *)th);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_VerticalDoor)
	{
	    saveg_write8 (tc_door);
	    PADSAVEP();
	    saveg_write_door ((vldoor_t *)th);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_MoveFloor)
	{
	    saveg_write8 (tc_floor);
	    PADSAVEP();
	    saveg_write_floor ((floormove_t *)th);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_PlatRaise)
	{
	    saveg_write8 (tc_plat);
	    PADSAVEP();
	    saveg_write_plat ((plat_t *)th);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_LightFlash)
	{
	    saveg_write8 (tc_flash);
	    PADSAVEP();
	    saveg_write_flash ((lightflash_t *)th);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
	{
	    saveg_write8 (tc_strobe);
	    PADSAVEP();
	    saveg_write_strobe ((strobe_t *)th);
	    continue;
	}

	if (th->function.acp1 == (actionf_p1)T_Glow)
	{
	    saveg_write8 (tc_glow);
	    PADSAVEP();
	    saveg_write_glow ((glow_t *)th);
	    continue;
	}
    }

    // add a terminating marker
    saveg_write8 (tc_endspecials);

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
    byte		tclass;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*		plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*		glow;


    // read in saved thinkers
    while (1)
    {
	tclass = saveg_read8 ();

	switch (tclass)
	{
	  case tc_endspecials:
	    return;	// end of list

	  case tc_ceiling:
	    PADSAVEP();
	    ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
	    memset (ceiling, 0, sizeof(*ceiling));
	    if (saveg_read_ceiling (ceiling))
		ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
	    ceiling->sector->specialdata = ceiling;
	    P_AddThinker (&ceiling->thinker);
	    P_AddActiveCeiling(ceiling);
	    break;

	  case tc_door:
	    PADSAVEP();
	    door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
	    memset (door, 0, sizeof(*door));
	    saveg_read_door (door);
	    door->sector->specialdata = door;
	    door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
	    P_AddThinker (&door->thinker);
	    break;

	  case tc_floor:
	    PADSAVEP();
	    floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
	    memset (floor, 0, sizeof(*floor));
	    saveg_read_floor (floor);
	    floor->sector->specialdata = floor;
	    floor->thinker.function.acp1 = (actionf_p1)T_MoveFloor;
	    P_AddThinker (&floor->thinker);
	    break;

	  case tc_plat:
	    PADSAVEP();
	    plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
	    memset (plat, 0, sizeof(*plat));
	    if (saveg_read_plat (plat))
		plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
	    plat->sector->specialdata = plat;
	    P_AddThinker (&plat->thinker);
	    P_AddActivePlat(plat);
	    break;

	  case tc_flash:
	    PADSAVEP();
	    flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
	    memset (flash, 0, sizeof(*flash));
	    saveg_read_flash (flash);
	    flash->thinker.function.acp1 = (actionf_p1)T_LightFlash;
	    P_AddThinker (&flash->thinker);
	    break;

	  case tc_strobe:
	    PADSAVEP();
	    strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
	    memset (strobe, 0, sizeof(*strobe));
	    saveg_read_strobe (strobe);
	    strobe->thinker.function.acp1 = (actionf_p1)T_StrobeFlash;
	    P_AddThinker (&strobe->thinker);
	    break;

	  case tc_glow:
	    PADSAVEP();
	    glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
	    memset (glow, 0, sizeof(*glow));
	    saveg_read_glow (glow);
	    glow->thinker.function.acp1 = (actionf_p1)T_Glow;
	    P_AddThinker (&glow->thinker);
	    break;

	  default:
	    I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
		     "in savegame",tclass);
	}

    }

}
