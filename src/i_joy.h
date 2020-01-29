// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_joy.h
/// \brief share joystick information with game control code

#ifndef __I_JOY_H__
#define __I_JOY_H__

#include "g_input.h"

#ifdef HAVE_SDL
#include "SDL.h"
#include "sdl/sdlmain.h"
#endif

// Max gamepad/joysticks that can be detected/used.
#define MAX_JOYSTICKS 8

/*!
  \brief	-JOYAXISRANGE to +JOYAXISRANGE for each axis

	(1024-1) so we can do a right shift instead of division
	(doesnt matter anyway, just give enough precision)
	a gamepad will return -1, 0, or 1 in the event data
	an analog type joystick will return a value
	from -JOYAXISRANGE to +JOYAXISRANGE for each axis
*/

#define JOYAXISRANGE 1023

// detect a bug if we increase JOYBUTTONS above DIJOYSTATE's number of buttons
#if (JOYBUTTONS > 64)
"JOYBUTTONS is greater than INT64 bits can hold"
#endif

/**	\brief	The struct JoyType_s

 share some joystick information (maybe 2 for splitscreen), to the game input code,
 actually, we need to know if it is a gamepad or analog controls
*/

struct JoyType_s
{
	/*! if true, we MUST Poll() to get new joystick data,
	that is: we NEED the DIRECTINPUTDEVICE2 ! (watchout NT compatibility) */
	INT32 bJoyNeedPoll;
	/*! this joystick is a gamepad, read: digital axes
	if FALSE, interpret the joystick event data as JOYAXISRANGE (see above) */
	INT32 bGamepadStyle;

};
typedef struct JoyType_s JoyType_t;
/**	\brief Joystick info
	for palyer[sic] 1-4's joystick/gamepad
*/

extern JoyType_t Joystick, Joystick2, Joystick3, Joystick4;

/**	\brief	The struct JoyDev_s

 Holds the names of the joystick devices and interface info about the devices.
*/

#ifdef HAVE_SDL
typedef struct JoyDev_s
{
	char name[29];
	SDLJoyInfo_t info;
	SDL_Haptic *haptic;
} JoyDev_t;

extern JoyDev_t joydevices[MAX_JOYSTICKS];
#endif

#endif // __I_JOY_H__
