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

#ifdef __APPLE__
/* Mouse acceleration needs to be disabled on OS X */
#define MACOS_X_ACCELERATION_HACK
#endif

#ifdef MACOS_X_ACCELERATION_HACK
#include <IOKit/IOTypes.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/event_status_driver.h>
#endif

/* analog axis ease math functions */
#define sine(x)      ((0.5f) * ( (1) - (cosf( (x) * M_PI )) ))
#define quadratic(x) ((x) * (x))
#define cubic(x)     ((x) * (x) * (x))
#define quartic(x)   ((x) * (x) * (x) * (x))
#define quintic(x)   ((x) * (x) * (x) * (x) * (x))

/* dual axis utility macro */
#define dualfunc(d,f) {        \
    d.left.x  = d.left.x < 0 ? -f( (float)-d.left.x ) : f( (float)d.left.x );  \
    d.left.y  = d.left.y < 0 ? -f( (float)-d.left.y ) : f( (float)d.left.y );  \
    d.right.x  = d.right.x < 0 ? -f( (float)-d.right.x ) : f( (float)d.right.x );  \
    d.right.y  = d.right.y < 0 ? -f( (float)-d.right.y ) : f( (float)d.right.y );  }

typedef struct
{
	float x;
	float y;
} joyAxis_t;

typedef struct
{
	joyAxis_t	_oldleft;
	joyAxis_t	_oldright;
	joyAxis_t	left;
	joyAxis_t	right;
} dualAxis_t;

static int buttonremap[] =
{
	K_MOUSE1,
	K_MOUSE3,	/* right button		*/
	K_MOUSE2,	/* middle button	*/
	K_MWHEELUP,
	K_MWHEELDOWN,
	K_MOUSE4,
	K_MOUSE5
};

static int joyremap[] =
{
	K_JOY1,
	K_JOY2,
	K_JOY3,
	K_JOY4,
	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,
	K_AUX17,
	K_AUX18,
	K_AUX19,
	K_AUX20,
	K_AUX21,
	K_AUX22,
	K_AUX23,
	K_AUX24,
	K_AUX25,
	K_AUX26,
	K_AUX27,
	K_AUX28
};

static qboolean	prev_gamekey, gamekey;
static qboolean	no_mouse = false;
static dualAxis_t _rawDualAxis = {0};

/* mouse variables */
cvar_t	m_filter = {"m_filter","0",CVAR_NONE};

/* joystick variables */
cvar_t	joy_sensitivity = { "joy_sensitivity", "32", CVAR_ARCHIVE };
cvar_t	joy_filter = { "joy_filter", "1", CVAR_ARCHIVE };
cvar_t	joy_deadzone = { "joy_deadzone", "0.125", CVAR_ARCHIVE };
cvar_t	joy_function = { "joy_function", "0", CVAR_ARCHIVE };
cvar_t	joy_axismove_x = { "joy_axismove_x", "0", CVAR_ARCHIVE };
cvar_t	joy_axismove_y = { "joy_axismove_y", "1", CVAR_ARCHIVE };
cvar_t	joy_axislook_x = { "joy_axislook_x", "2", CVAR_ARCHIVE };
cvar_t	joy_axislook_y = { "joy_axislook_y", "3", CVAR_ARCHIVE };
cvar_t	joy_axis_debug = { "joy_axis_debug", "0", CVAR_NONE };

/* total accumulated mouse movement since last frame,
 * gets updated from the main game loop via IN_MouseMove */
static int	total_dx, total_dy = 0;

static int FilterMouseEvents (const SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return 0;
	}

	return 1;
}

/* joystick support functions */

static float NormalizeJoyInputValue (const Sint16 input)
{
	Uint16 convert = (Uint16)(32768 + input);
	float output = (convert / 32767.5f) - 1.0f;
	return output;
}

/*
// adapted in part from:
// http://www.third-helix.com/2013/04/12/doing-thumbstick-dead-zones-right.html
*/
static joyAxis_t ApplyJoyDeadzone(joyAxis_t axis, float deadzone)
{
	joyAxis_t result = {0};
	float magnitude = sqrtf( (axis.x * axis.x) + (axis.y * axis.y) );

	if ( magnitude < deadzone ) {
		result.x = result.y = 0.0f;
	} else {
		joyAxis_t normalized;
		float gradient;

		if ( magnitude > 1.0f ) {
			magnitude = 1.0f;
		}

		normalized.x = axis.x / magnitude;
		normalized.y = axis.y / magnitude;
		gradient = ( (magnitude - deadzone) / (1.0f - deadzone) );
		result.x = normalized.x * gradient;
		result.y = normalized.y * gradient;
	}

	return result;
}


#ifdef MACOS_X_ACCELERATION_HACK
static cvar_t in_disablemacosxmouseaccel = {"in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE};
static double originalMouseSpeed = -1.0;

static io_connect_t IN_GetIOHandle(void)
{
	io_connect_t iohandle = MACH_PORT_NULL;
	io_service_t iohidsystem = MACH_PORT_NULL;
	mach_port_t masterport;
	kern_return_t status;

	status = IOMasterPort(MACH_PORT_NULL, &masterport);
	if (status != KERN_SUCCESS)
		return 0;

	iohidsystem = IORegistryEntryFromPath(masterport, kIOServicePlane ":/IOResources/IOHIDSystem");
	if (!iohidsystem)
		return 0;

	status = IOServiceOpen(iohidsystem, mach_task_self(), kIOHIDParamConnectType, &iohandle);
	IOObjectRelease(iohidsystem);

	return iohandle;
}

static void IN_DisableOSXMouseAccel (void)
{
	io_connect_t mouseDev = IN_GetIOHandle();
	if (mouseDev != 0)
	{
		if (IOHIDGetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), &originalMouseSpeed) == kIOReturnSuccess)
		{
			if (IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), -1.0) != kIOReturnSuccess)
			{
				Cvar_Set("in_disablemacosxmouseaccel", "0");
				Con_Printf("WARNING: Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
			}
		}
		else
		{
			Cvar_Set("in_disablemacosxmouseaccel", "0");
			Con_Printf("WARNING: Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n");
		}
		IOServiceClose(mouseDev);
	}
	else
	{
		Cvar_Set("in_disablemacosxmouseaccel", "0");
		Con_Printf("WARNING: Could not disable mouse acceleration (failed at IO_GetIOHandle).\n");
	}
}

static void IN_ReenableOSXMouseAccel (void)
{
	io_connect_t mouseDev = IN_GetIOHandle();
	if (mouseDev != 0)
	{
		if (IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), originalMouseSpeed) != kIOReturnSuccess)
			Con_Printf("WARNING: Could not re-enable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
		IOServiceClose(mouseDev);
	}
	else
	{
		Con_Printf("WARNING: Could not re-enable mouse acceleration (failed at IO_GetIOHandle).\n");
	}
	originalMouseSpeed = -1;
}
#endif /* MACOS_X_ACCELERATION_HACK */


void IN_Activate (void)
{
	if (no_mouse)
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	/* Save the status of mouse acceleration */
	if (originalMouseSpeed == -1 && in_disablemacosxmouseaccel.value)
		IN_DisableOSXMouseAccel();
#endif

	if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_ON)
	{
		SDL_WM_GrabInput(SDL_GRAB_ON);
		if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_ON)
			Con_Printf("WARNING: SDL_WM_GrabInput(SDL_GRAB_ON) failed.\n");
	}

	if (SDL_ShowCursor(SDL_QUERY) != SDL_DISABLE)
	{
		SDL_ShowCursor(SDL_DISABLE);
		if (SDL_ShowCursor(SDL_QUERY) != SDL_DISABLE)
			Con_Printf("WARNING: SDL_ShowCursor(SDL_DISABLE) failed.\n");
	}

	if (SDL_GetEventFilter() != NULL)
		SDL_SetEventFilter(NULL);

	total_dx = 0;
	total_dy = 0;
}

void IN_Deactivate (qboolean free_cursor)
{
	if (no_mouse)
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	if (originalMouseSpeed != -1)
		IN_ReenableOSXMouseAccel();
#endif

	if (free_cursor)
	{
		if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_OFF)
		{
			SDL_WM_GrabInput(SDL_GRAB_OFF);
			if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_OFF)
				Con_Printf("WARNING: SDL_WM_GrabInput(SDL_GRAB_OFF) failed.\n");
		}

		if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
		{
			SDL_ShowCursor(SDL_ENABLE);
			if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
				Con_Printf("WARNING: SDL_ShowCursor(SDL_ENABLE) failed.\n");
		}
	}

	/* discard all mouse events when input is deactivated */
	if (SDL_GetEventFilter() != FilterMouseEvents)
		SDL_SetEventFilter(FilterMouseEvents);
}

void IN_Init (void)
{
	Con_Printf( "\n------- Input Initialization -------\n" );

	prev_gamekey = ((key_dest == key_game && !con_forcedup) || m_keys_bind_grab);
	SDL_EnableUNICODE (!prev_gamekey);
	if (SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL) == -1)
		Con_Printf("Warning: SDL_EnableKeyRepeat() failed.\n");

	if (safemode || COM_CheckParm("-nomouse"))
	{
		no_mouse = true;
		/* discard all mouse events when input is deactivated */
		SDL_SetEventFilter(FilterMouseEvents);
	}

	// BEGIN jeremiah sypult
	Cvar_RegisterVariable( &joy_sensitivity );
	Cvar_RegisterVariable( &joy_filter );
	Cvar_RegisterVariable( &joy_deadzone );
	Cvar_RegisterVariable( &joy_function );
	Cvar_RegisterVariable( &joy_axismove_x );
	Cvar_RegisterVariable( &joy_axismove_y );
	Cvar_RegisterVariable( &joy_axislook_x );
	Cvar_RegisterVariable( &joy_axislook_y );
	Cvar_RegisterVariable( &joy_axis_debug );

	if ( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) == -1 ) {
		Con_Printf( "WARNING: Could not initialize SDL Joystick\n" );
	} else {
		int i;
		SDL_JoystickEventState( SDL_ENABLE );

		for ( i = 0; i < SDL_NumJoysticks(); i++ ) {
			if ( ! SDL_JoystickOpened( i ) ) {
				SDL_Joystick* controller = SDL_JoystickOpen( i );

				if ( controller ) {
					Con_Printf( "%s\n     axes: %d\n  buttons: %d\n    balls: %d\n     hats: %d\n",
							    SDL_JoystickName( i ),
							    SDL_JoystickNumAxes( controller ),
							    SDL_JoystickNumButtons( controller ),
							    SDL_JoystickNumBalls( controller ),
							    SDL_JoystickNumHats( controller ) );
				}
			}
		}
	}
	// END jeremiah sypult

#ifdef MACOS_X_ACCELERATION_HACK
	Cvar_RegisterVariable(&in_disablemacosxmouseaccel);
#endif

	IN_Activate();
}

void IN_Shutdown (void)
{
	IN_Deactivate(true);
}

void IN_Commands (void)
{
/* TODO: implement this for joystick support */
}

extern cvar_t cl_maxpitch; /* johnfitz -- variable pitch clamping */
extern cvar_t cl_minpitch; /* johnfitz -- variable pitch clamping */


void IN_MouseMove(int dx, int dy)
{
	total_dx += dx;
	total_dy += dy;
}

void IN_JoyAxisMove(Uint8 axis, Sint16 value)
{
	float axisValue = NormalizeJoyInputValue( value );
	Uint8 axisMap[] = {
		(Uint8)joy_axismove_x.value,
		(Uint8)joy_axismove_y.value,
		(Uint8)joy_axislook_x.value,
		(Uint8)joy_axislook_y.value
	};

	// attempt map the incoming axis to our cvars defining which axis index
	// controls movement.
	if ( axisMap[0] == axis ) {
		_rawDualAxis.left.x = axisValue;
	} else if ( axisMap[1] == axis ) {
		_rawDualAxis.left.y = axisValue;
	} else if ( axisMap[2] == axis ) {
		_rawDualAxis.right.x = axisValue;
	} else if ( axisMap[3] == axis ) {
		_rawDualAxis.right.y = axisValue;
	}

	if ( joy_axis_debug.value ) {
		Sint16 deadzone = joy_deadzone.value * 32767.6f;
		if ( value < -deadzone || value > deadzone ) {
			Con_Printf( "joy axis %i, value %i\n", axis, value );
		}
	}
}

void IN_Move (usercmd_t *cmd)
{
	int		dmx, dmy;

	// jeremiah sypult -- BEGIN joystick
	//
	dualAxis_t moveDualAxis = {0};

	if ( joy_filter.value ) {
		moveDualAxis.left.x = ( _rawDualAxis.left.x + _rawDualAxis._oldleft.x ) * 0.5;
		moveDualAxis.left.y = ( _rawDualAxis.left.y + _rawDualAxis._oldleft.y ) * 0.5;
		moveDualAxis.right.x = ( _rawDualAxis.right.x + _rawDualAxis._oldright.x ) * 0.5;
		moveDualAxis.right.y = ( _rawDualAxis.right.y + _rawDualAxis._oldright.y ) * 0.5;
	} else {
		moveDualAxis.left = _rawDualAxis.left;
		moveDualAxis.right = _rawDualAxis.right;
	}

	_rawDualAxis._oldleft = _rawDualAxis.left;
	_rawDualAxis._oldright = _rawDualAxis.right;

	switch ( (int)joy_function.value ) {
		default:
		case 0: break;
		case 1: dualfunc( moveDualAxis, sine );      break;
		case 2: dualfunc( moveDualAxis, quadratic ); break;
		case 3: dualfunc( moveDualAxis, cubic );     break;
		case 4: dualfunc( moveDualAxis, quartic );   break;
		case 5: dualfunc( moveDualAxis, quintic );   break;
	}

	// TODO: determine whether to apply deadzone before or after axis functions?
	moveDualAxis.left = ApplyJoyDeadzone( moveDualAxis.left, joy_deadzone.value );
	moveDualAxis.right = ApplyJoyDeadzone( moveDualAxis.right, joy_deadzone.value );

	// movements are not scaled by sensitivity
	if ( moveDualAxis.left.x != 0.0f ) {
		cmd->sidemove += (cl_sidespeed.value * moveDualAxis.left.x);
	}
	if ( moveDualAxis.left.y != 0.0f ) {
		cmd->forwardmove -= (cl_forwardspeed.value * moveDualAxis.left.y);
	}

	//
	// adjust for speed key
	//
	if (cl_forwardspeed.value > 200 && cl_movespeedkey.value)
		cmd->forwardmove /= cl_movespeedkey.value;
	if ((cl_forwardspeed.value > 200) ^ (in_speed.state & 1))
	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
	}

	// add the joy look axis to mouse look
	total_dx += moveDualAxis.right.x * joy_sensitivity.value;
	total_dy += moveDualAxis.right.y * joy_sensitivity.value;
	//
	// jeremiah sypult -- ENDjoystick

	/* TODO: fix this
	if (m_filter.value)
	{
		dmx = (2*mx - dmx) * 0.5;
		dmy = (2*my - dmy) * 0.5;
	}
	*/

	dmx = total_dx * sensitivity.value;
	dmy = total_dy * sensitivity.value;

	total_dx = 0;
	total_dy = 0;

	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * dmx;
	else
		cl.aimangles[YAW] -= m_yaw.value * dmx;

	if (in_mlook.state & 1)
	{
		if (dmx || dmy)
			V_StopPitchDrift ();
	}

	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.aimangles[PITCH] += m_pitch.value * dmy;
		/* johnfitz -- variable pitch clamping */
		if (cl.aimangles[PITCH] > cl_maxpitch.value)
			cl.aimangles[PITCH] = cl_maxpitch.value;
		if (cl.aimangles[PITCH] < cl_minpitch.value)
			cl.aimangles[PITCH] = cl_minpitch.value;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * dmy;
		else
			cmd->forwardmove -= m_forward.value * dmy;
	}
}

void IN_ClearStates (void)
{
}

void IN_UpdateForKeydest (void)
{
	gamekey = ((key_dest == key_game && !con_forcedup) || m_keys_bind_grab);
	if (gamekey != prev_gamekey)
	{
		prev_gamekey = gamekey;
		Key_ClearStates();
		SDL_EnableUNICODE(!gamekey);
	}
}

void IN_SendKeyEvents (void)
{
	SDL_Event event;
	int sym, state, modstate;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_ACTIVEEVENT:
			if (event.active.state & (SDL_APPINPUTFOCUS|SDL_APPACTIVE))
			{
				if (event.active.gain)
					S_UnblockSound();
				else	S_BlockSound();
			}
			break;

		case SDL_KEYDOWN:
			if ((event.key.keysym.sym == SDLK_RETURN) &&
			    (event.key.keysym.mod & KMOD_ALT))
			{
				VID_Toggle();
				break;
			}
			if ((event.key.keysym.sym == SDLK_ESCAPE) &&
			    (event.key.keysym.mod & KMOD_SHIFT))
			{
				Con_ToggleConsole_f();
				break;
			}
		/* fallthrough */
		case SDL_KEYUP:
			sym = event.key.keysym.sym;
			state = event.key.state;
			modstate = SDL_GetModState();

			if (event.key.keysym.unicode != 0)
			{
				if ((event.key.keysym.unicode & 0xFF80) == 0)
				{
					int usym = event.key.keysym.unicode & 0x7F;
					if (modstate & KMOD_CTRL && usym < 32 && sym >= 32)
					{
						/* control characters */
						if (modstate & KMOD_SHIFT)
							usym += 64;
						else	usym += 96;
					}
#if defined(__APPLE__) && defined(__MACH__)
					if (sym == SDLK_BACKSPACE)
						usym = sym;	/* avoid change to SDLK_DELETE */
#endif	/* Mac OS X */
#if defined(__QNX__) || defined(__QNXNTO__)
					if (sym == SDLK_BACKSPACE || sym == SDLK_RETURN)
						usym = sym;	/* S.A: fixes QNX weirdness */
#endif	/* __QNX__ */
					/* only use unicode for ` and ~ in game mode */
					if (!gamekey || usym == '`' || usym == '~')
						sym = usym;
				}
				/* else: it's an international character */
			}
			/*printf("You pressed %s (%d) (%c)\n", SDL_GetKeyName(sym), sym, sym);*/

			switch (sym)
			{
			case SDLK_DELETE:
				sym = K_DEL;
				break;
			case SDLK_BACKSPACE:
				sym = K_BACKSPACE;
				break;
			case SDLK_F1:
				sym = K_F1;
				break;
			case SDLK_F2:
				sym = K_F2;
				break;
			case SDLK_F3:
				sym = K_F3;
				break;
			case SDLK_F4:
				sym = K_F4;
				break;
			case SDLK_F5:
				sym = K_F5;
				break;
			case SDLK_F6:
				sym = K_F6;
				break;
			case SDLK_F7:
				sym = K_F7;
				break;
			case SDLK_F8:
				sym = K_F8;
				break;
			case SDLK_F9:
				sym = K_F9;
				break;
			case SDLK_F10:
				sym = K_F10;
				break;
			case SDLK_F11:
				sym = K_F11;
				break;
			case SDLK_F12:
				sym = K_F12;
				break;
			case SDLK_BREAK:
			case SDLK_PAUSE:
				sym = K_PAUSE;
				break;
			case SDLK_UP:
				sym = K_UPARROW;
				break;
			case SDLK_DOWN:
				sym = K_DOWNARROW;
				break;
			case SDLK_RIGHT:
				sym = K_RIGHTARROW;
				break;
			case SDLK_LEFT:
				sym = K_LEFTARROW;
				break;
			case SDLK_INSERT:
				sym = K_INS;
				break;
			case SDLK_HOME:
				sym = K_HOME;
				break;
			case SDLK_END:
				sym = K_END;
				break;
			case SDLK_PAGEUP:
				sym = K_PGUP;
				break;
			case SDLK_PAGEDOWN:
				sym = K_PGDN;
				break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
				sym = K_SHIFT;
				break;
			case SDLK_RCTRL:
			case SDLK_LCTRL:
				sym = K_CTRL;
				break;
			case SDLK_RALT:
			case SDLK_LALT:
				sym = K_ALT;
				break;
			case SDLK_RMETA:
			case SDLK_LMETA:
				sym = K_COMMAND;
				break;
			case SDLK_NUMLOCK:
				if (gamekey)
					sym = K_KP_NUMLOCK;
				else	sym = 0;
				break;
			case SDLK_KP0:
				if (gamekey)
					sym = K_KP_INS;
				else	sym = (modstate & KMOD_NUM) ? SDLK_0 : K_INS;
				break;
			case SDLK_KP1:
				if (gamekey)
					sym = K_KP_END;
				else	sym = (modstate & KMOD_NUM) ? SDLK_1 : K_END;
				break;
			case SDLK_KP2:
				if (gamekey)
					sym = K_KP_DOWNARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_2 : K_DOWNARROW;
				break;
			case SDLK_KP3:
				if (gamekey)
					sym = K_KP_PGDN;
				else	sym = (modstate & KMOD_NUM) ? SDLK_3 : K_PGDN;
				break;
			case SDLK_KP4:
				if (gamekey)
					sym = K_KP_LEFTARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_4 : K_LEFTARROW;
				break;
			case SDLK_KP5:
				if (gamekey)
					sym = K_KP_5;
				else	sym = SDLK_5;
				break;
			case SDLK_KP6:
				if (gamekey)
					sym = K_KP_RIGHTARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_6 : K_RIGHTARROW;
				break;
			case SDLK_KP7:
				if (gamekey)
					sym = K_KP_HOME;
				else	sym = (modstate & KMOD_NUM) ? SDLK_7 : K_HOME;
				break;
			case SDLK_KP8:
				if (gamekey)
					sym = K_KP_UPARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_8 : K_UPARROW;
				break;
			case SDLK_KP9:
				if (gamekey)
					sym = K_KP_PGUP;
				else	sym = (modstate & KMOD_NUM) ? SDLK_9 : K_PGUP;
				break;
			case SDLK_KP_PERIOD:
				if (gamekey)
					sym = K_KP_DEL;
				else	sym = (modstate & KMOD_NUM) ? SDLK_PERIOD : K_DEL;
				break;
			case SDLK_KP_DIVIDE:
				if (gamekey)
					sym = K_KP_SLASH;
				else	sym = SDLK_SLASH;
				break;
			case SDLK_KP_MULTIPLY:
				if (gamekey)
					sym = K_KP_STAR;
				else	sym = SDLK_ASTERISK;
				break;
			case SDLK_KP_MINUS:
				if (gamekey)
					sym = K_KP_MINUS;
				else	sym = SDLK_MINUS;
				break;
			case SDLK_KP_PLUS:
				if (gamekey)
					sym = K_KP_PLUS;
				else	sym = SDLK_PLUS;
				break;
			case SDLK_KP_ENTER:
				if (gamekey)
					sym = K_KP_ENTER;
				else	sym = SDLK_RETURN;
				break;
			case SDLK_KP_EQUALS:
				if (gamekey)
					sym = 0;
				else	sym = SDLK_EQUALS;
				break;
			case 178: /* the '²' key */
				sym = '~';
				break;
			default:
			/* If we are not directly handled and still above 255,
			 * just force it to 0. kill unsupported international
			 * characters, too.  */
				if ((sym >= SDLK_WORLD_0 && sym <= SDLK_WORLD_95) ||
									sym > 255)
					sym = 0;
				break;
			}
			Key_Event (sym, state);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (event.button.button < 1 ||
			    event.button.button > sizeof(buttonremap) / sizeof(buttonremap[0]))
			{
				Con_Printf ("Ignored event for mouse button %d\n",
							event.button.button);
				break;
			}
			Key_Event(buttonremap[event.button.button - 1], event.button.state == SDL_PRESSED);
			break;

		case SDL_MOUSEMOTION:
			IN_MouseMove(event.motion.xrel, event.motion.yrel);
			break;

		case SDL_JOYHATMOTION:
			// TODO: add hat support, AUX29-AUX32
			break;

		case SDL_JOYBALLMOTION:
			// TODO: VERIFY joyball support, assignment other than mouse?
			IN_MouseMove(event.jball.xrel, event.jball.yrel);
			break;

		case SDL_JOYAXISMOTION:
			IN_JoyAxisMove(event.jaxis.axis, event.jaxis.value);
			break;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			if (event.button.button > sizeof(joyremap) / sizeof(joyremap[0]))
			{
				Con_Printf ("Ignored event for joy button %d\n",
							event.button.button);
				break;
			}
			Key_Event(joyremap[event.button.button], event.button.state == SDL_PRESSED);
			break;
		case SDL_QUIT:
			CL_Disconnect ();
			Sys_Quit ();
			break;

		default:
			break;
		}
	}
}

