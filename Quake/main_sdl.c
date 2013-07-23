/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL/SDL.h>
#else
#include "SDL.h"
#endif
#include <stdio.h>

/* need at least SDL_1.2.10 */
#define SDL_MIN_X	1
#define SDL_MIN_Y	2
#define SDL_MIN_Z	10
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
/* reject 1.3.0 and newer at runtime. */
#define SDL_NEW_VERSION_REJECT	(SDL_VERSIONNUM(1,3,0))

static void Sys_CheckSDL (void)
{
	const SDL_version *sdl_version = SDL_Linked_Version();

	Sys_Printf("Found SDL version %i.%i.%i\n",sdl_version->major,sdl_version->minor,sdl_version->patch);
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) < SDL_REQUIREDVERSION)
	{	/*reject running under older SDL versions */
		Sys_Error("You need at least v%d.%d.%d of SDL to run this game.", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) >= SDL_NEW_VERSION_REJECT)
	{	/*reject running under newer (1.3.x) SDL */
		Sys_Error("Your version of SDL library is incompatible with me.\n"
			  "You need a library version in the line of %d.%d.%d\n", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}
}

#define DEFAULT_MEMORY 0x4000000

static quakeparms_t	parms;
static Uint8		appState;

int main(int argc, char *argv[])
{
	int		t;
	double		time, oldtime, newtime;

	host_parms = &parms;
	parms.basedir = ".";

	parms.argc = argc;
	parms.argv = argv;

	COM_InitArgv(parms.argc, parms.argv);

	isDedicated = (COM_CheckParm("-dedicated") != 0);

	Sys_CheckSDL ();

	Sys_Init();

	// default memory size
	// TODO: less mem, eg. 16 mb, for dedicated server??
	parms.memsize = DEFAULT_MEMORY;

	if (COM_CheckParm("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;
		if (t < com_argc)
			parms.memsize = Q_atoi(com_argv[t]) * 1024;
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

	Sys_Printf("Quake %1.2f (c) id Software\n", VERSION);
	Sys_Printf("GLQuake %1.2f (c) id Software\n", GLQUAKE_VERSION);
	Sys_Printf("FitzQuake %1.2f (c) John Fitzgibbons\n", FITZQUAKE_VERSION);
	Sys_Printf("FitzQuake SDL port (c) SleepwalkR, Baker\n");
	Sys_Printf("QuakeSpasm %1.2f.%d (c) Ozkan Sezer, Stevenaaus\n", FITZQUAKE_VERSION, QUAKESPASM_VER_PATCH);
	Sys_Printf("RiftQuake %1.2f\n", RIFTQUAKE_VERSION);

	Sys_Printf("Host_Init\n");
	Host_Init();

	oldtime = Sys_DoubleTime();
	if (isDedicated)
	{
		while (1)
		{
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate.value )
			{
				SDL_Delay(1);
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}

			Host_Frame (time);
			oldtime = newtime;
		}
	}
	else
	while (1)
	{
		appState = SDL_GetAppState();
		/* If we have no input focus at all, sleep a bit */
		if ( !(appState & (SDL_APPMOUSEFOCUS | SDL_APPINPUTFOCUS)) || cl.paused)
		{
			SDL_Delay(16);
		}
		/* If we're minimised, sleep a bit more */
		if ( !(appState & SDL_APPACTIVE) )
		{
			scr_skipupdate = 1;
			SDL_Delay(32);
		}
		else
		{
			scr_skipupdate = 0;
		}
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame (time);

		if (time < sys_throttle.value)
			SDL_Delay(1);

		oldtime = newtime;
	}

	return 0;
}

