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
//	Searches the usual Unix data directories plus any Steam
//	libraries, matching file names without regard to case.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "w_wad.h"
#include "m_argv.h"
#include "m_swap.h"
#include "i_system.h"
#include "d_iwad.h"


typedef struct
{
    char*		name;
    GameMission_t	mission;
    Language_t		language;
} iwadname_t;

// In order of preference, as the original IdentifyVersion checked.
static iwadname_t iwadnames[] =
{
    { "doom2f.wad",	doom2,		french },
    { "doom2.wad",	doom2,		english },
    { "plutonia.wad",	pack_plut,	english },
    { "tnt.wad",	pack_tnt,	english },
    { "doomu.wad",	doom,		english },
    { "doom.wad",	doom,		english },
    { "doom1.wad",	doom,		english },
    { "freedoom2.wad",	doom2,		english },
    { "freedoom1.wad",	doom,		english },
};

#define NUMIWADNAMES	(sizeof(iwadnames)/sizeof(iwadnames[0]))

// Steam installs, searched a few levels deep since their layout has
// changed between releases (base/, rerelease/, finaldoombase/, ...).
static char* steamgames[] =
{
    "Ultimate Doom",
    "Doom 2",
    "Final Doom",
    "DOOM 3 BFG Edition",
};

#define MAXSEARCHDIRS	64

static char*	searchdirs[MAXSEARCHDIRS];
static int	numsearchdirs;

static char*	iwadbasename = "unknown";


static void AddSearchDir (const char* dir)
{
    int		i;
    struct stat	st;

    if (!dir || !*dir || numsearchdirs == MAXSEARCHDIRS)
	return;
    if (stat (dir, &st) || !S_ISDIR (st.st_mode))
	return;
    for (i=0 ; i<numsearchdirs ; i++)
	if (!strcmp (searchdirs[i], dir))
	    return;

    searchdirs[numsearchdirs++] = strdup (dir);
}

static void AddSearchDirf (const char* fmt, const char* a, const char* b)
{
    char	buf[1024];

    snprintf (buf, sizeof(buf), fmt, a, b);
    AddSearchDir (buf);
}

//
// AddSteamSubdirs
// Adds dir and every subdirectory up to depth levels below it.
//
static void AddSteamSubdirs (const char* dir, int depth)
{
    DIR*		d;
    struct dirent*	ent;
    char		path[1024];

    AddSearchDir (dir);
    if (!depth)
	return;

    d = opendir (dir);
    if (!d)
	return;
    while ((ent = readdir (d)) != NULL)
    {
	if (ent->d_name[0] == '.')
	    continue;
	snprintf (path, sizeof(path), "%s/%s", dir, ent->d_name);
	AddSteamSubdirs (path, depth-1);
    }
    closedir (d);
}

//
// AddSteamLibrary
// Steam keeps a list of all library folders in libraryfolders.vdf.
//
static void AddSteamLibraries (const char* steamroot)
{
    char	path[1024];
    char	line[1024];
    char	libpath[1024];
    FILE*	f;
    char*	p;
    char*	q;
    unsigned	i;

    snprintf (path, sizeof(path), "%s/steamapps/libraryfolders.vdf", steamroot);
    f = fopen (path, "r");
    if (!f)
	return;

    while (fgets (line, sizeof(line), f))
    {
	// Lines look like:	"path"		"/home/user/.local/share/Steam"
	p = strstr (line, "\"path\"");
	if (!p)
	    continue;
	p = strchr (p+6, '"');
	if (!p)
	    continue;
	q = strchr (++p, '"');
	if (!q)
	    continue;
	*q = 0;

	for (i=0 ; i<sizeof(steamgames)/sizeof(steamgames[0]) ; i++)
	{
	    snprintf (libpath, sizeof(libpath), "%s/steamapps/common/%s",
		      p, steamgames[i]);
	    AddSteamSubdirs (libpath, 3);
	}
    }
    fclose (f);
}

static void BuildSearchDirs (void)
{
    char*	env;
    char*	path;
    char*	dir;
    char*	home;
    char*	xdgdata;

    AddSearchDir (getenv ("DOOMWADDIR"));

    env = getenv ("DOOMWADPATH");
    if (env)
    {
	path = strdup (env);
	for (dir = strtok (path, ":") ; dir ; dir = strtok (NULL, ":"))
	    AddSearchDir (dir);
	free (path);
    }

    AddSearchDir (".");
    AddSearchDir (I_DataDir ());

    home = getenv ("HOME");
    xdgdata = getenv ("XDG_DATA_HOME");
    if (xdgdata)
	AddSearchDirf ("%s/games/doom", xdgdata, NULL);
    else if (home)
	AddSearchDirf ("%s/.local/share/games/doom", home, NULL);

    AddSearchDir ("/usr/local/share/games/doom");
    AddSearchDir ("/usr/share/games/doom");
    AddSearchDir ("/usr/local/share/doom");
    AddSearchDir ("/usr/share/doom");

    if (home)
    {
	char	steamroot[1024];

	snprintf (steamroot, sizeof(steamroot), "%s/.local/share/Steam", home);
	AddSteamLibraries (steamroot);
	snprintf (steamroot, sizeof(steamroot), "%s/.steam/steam", home);
	AddSteamLibraries (steamroot);
	snprintf (steamroot, sizeof(steamroot),
		  "%s/.var/app/com.valvesoftware.Steam/.local/share/Steam", home);
	AddSteamLibraries (steamroot);
    }
}


//
// FindFileInDir
// Case-insensitive lookup of name in dir. Returns a malloced path.
//
static char* FindFileInDir (const char* dir, const char* name)
{
    DIR*		d;
    struct dirent*	ent;
    char*		result = NULL;
    char		path[1024];
    struct stat		st;

    d = opendir (dir);
    if (!d)
	return NULL;
    while ((ent = readdir (d)) != NULL)
    {
	if (strcasecmp (ent->d_name, name))
	    continue;
	snprintf (path, sizeof(path), "%s/%s", dir, ent->d_name);
	if (!stat (path, &st) && S_ISREG (st.st_mode))
	{
	    result = strdup (path);
	    break;
	}
    }
    closedir (d);
    return result;
}

//
// D_FindWADByName
// Searches the IWAD directories for a file.
// A name containing a slash is taken as a path.
//
char* D_FindWADByName (const char* name)
{
    int		i;
    char*	path;
    struct stat	st;

    if (strchr (name, '/'))
	return stat (name, &st) ? NULL : strdup (name);

    if (!numsearchdirs)
	BuildSearchDirs ();

    for (i=0 ; i<numsearchdirs ; i++)
    {
	path = FindFileInDir (searchdirs[i], name);
	if (path)
	    return path;
    }
    return NULL;
}


//
// IdentifyIWADContents
// Decides the game mode by which maps the WAD holds,
// as a file name alone cannot tell DOOM from Ultimate DOOM.
//
static GameMode_t IdentifyIWADContents (const char* path)
{
    FILE*	f;
    wadinfo_t	header;
    filelump_t	lump;
    int		i;
    int		numlumps;
    boolean	hasmap01 = false;
    boolean	hase2m1 = false;
    boolean	hase4m1 = false;

    f = fopen (path, "rb");
    if (!f)
	return indetermined;

    if (fread (&header, sizeof(header), 1, f) != 1
	|| (strncmp (header.identification, "IWAD", 4)
	    && strncmp (header.identification, "PWAD", 4)))
    {
	fclose (f);
	return indetermined;
    }

    numlumps = LONG (header.numlumps);
    fseek (f, LONG (header.infotableofs), SEEK_SET);
    for (i=0 ; i<numlumps ; i++)
    {
	if (fread (&lump, sizeof(lump), 1, f) != 1)
	    break;
	if (!strncasecmp (lump.name, "MAP01", 8))
	    hasmap01 = true;
	else if (!strncasecmp (lump.name, "E2M1", 8))
	    hase2m1 = true;
	else if (!strncasecmp (lump.name, "E4M1", 8))
	    hase4m1 = true;
    }
    fclose (f);

    if (hasmap01)
	return commercial;
    if (hase4m1)
	return retail;
    if (hase2m1)
	return registered;
    return shareware;
}

static void SetIWADBaseName (const char* path)
{
    const char*	slash;
    char*	dot;
    char*	p;

    slash = strrchr (path, '/');
    iwadbasename = strdup (slash ? slash+1 : path);
    dot = strrchr (iwadbasename, '.');
    if (dot)
	*dot = 0;
    for (p = iwadbasename ; *p ; p++)
	if (*p >= 'A' && *p <= 'Z')
	    *p += 'a' - 'A';
}

//
// D_FindIWAD
// Returns the path of the IWAD to use and sets
// gamemode, gamemission and language to match it.
//
char* D_FindIWAD (void)
{
    char*		path = NULL;
    const char*		name;
    const char*		slash;
    unsigned		i;
    int			p;

    p = M_CheckParm ("-iwad");
    if (p && p < myargc-1)
    {
	path = D_FindWADByName (myargv[p+1]);
	if (!path)
	    I_Error ("IWAD file '%s' not found.", myargv[p+1]);
    }
    else
    {
	for (i=0 ; i<NUMIWADNAMES && !path ; i++)
	    path = D_FindWADByName (iwadnames[i].name);
    }

    if (!path)
    {
	gamemode = indetermined;
	return NULL;
    }

    // Match the file name for the mission.
    slash = strrchr (path, '/');
    name = slash ? slash+1 : path;
    gamemission = doom;
    for (i=0 ; i<NUMIWADNAMES ; i++)
    {
	if (!strcasecmp (name, iwadnames[i].name))
	{
	    gamemission = iwadnames[i].mission;
	    language = iwadnames[i].language;
	    break;
	}
    }

    gamemode = IdentifyIWADContents (path);
    if (gamemode == commercial && gamemission == doom)
	gamemission = doom2;
    else if (gamemode != commercial)
	gamemission = doom;

    SetIWADBaseName (path);
    return path;
}

//
// D_IWADBaseName
// Lower case IWAD file name without extension, eg. "doom2".
// Keeps each game's savegames apart.
//
char* D_IWADBaseName (void)
{
    return iwadbasename;
}

//
// D_SaveGamePath
//
void D_SaveGamePath (char* buf, int size, int slot)
{
    static char*	savedir;

    if (!savedir)
    {
	char	dir[1024];

	snprintf (dir, sizeof(dir), "%s/savegames/%s",
		  I_DataDir (), iwadbasename);
	I_MakeDirectory (dir);
	savedir = strdup (dir);
    }
    snprintf (buf, size, "%s/" SAVEGAMENAME "%d.dsg", savedir, slot);
}
