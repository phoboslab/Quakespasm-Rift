#include "quakedef.h"
#include "vr.h"

#ifndef __R_VR_MENU_H
#define __R_VR_MENU_H

typedef enum _vr_menu_options_t
{
	VR_OPTION_ENABLED,
	VR_OPTION_PERFHUD,

	VR_OPTION_AIMMODE,
	VR_OPTION_DEADZONE,
	VR_OPTION_CROSSHAIR,
	VR_OPTION_CROSSHAIR_DEPTH,
	VR_OPTION_CROSSHAIR_SIZE,
	VR_OPTION_CROSSHAIR_ALPHA,

	VR_OPTION_MAX
} vr_menu_options_t;

void VR_Menu_Init ();
void VR_Menu_f (void);
void VR_MenuDraw (void);
void VR_MenuKey (int key);

extern void (*vr_menucmdfn)(void);
extern void (*vr_menudrawfn)(void);
extern void (*vr_menukeyfn)(int key);

#endif
