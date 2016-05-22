// 2013 Dominic Szablewski - phoboslab.org

#include "quakedef.h"

#ifndef __R_VR_H
#define __R_VR_H

#define VR_AIMMODE_HEAD_MYAW 1 // Head Aiming; View YAW is mouse+head, PITCH is head
#define VR_AIMMODE_HEAD_MYAW_MPITCH 2 // Head Aiming; View YAW and PITCH is mouse+head
#define VR_AIMMODE_MOUSE_MYAW 3 // Mouse Aiming; View YAW is mouse+head, PITCH is head
#define VR_AIMMODE_MOUSE_MYAW_MPITCH 4 // Mouse Aiming; View YAW and PITCH is mouse+head
#define VR_AIMMODE_BLENDED 5 // Blended Aiming; Mouse aims, with YAW decoupled for limited area

#define	VR_CROSSHAIR_NONE 0 // No crosshair
#define	VR_CROSSHAIR_POINT 1 // Point crosshair projected to depth of object it is in front of
#define	VR_CROSSHAIR_LINE 2 // Line crosshair

void VR_Init();
void VR_Shutdown();
qboolean VR_Enable();
void VR_Disable();

void VR_UpdateScreenContent();
void VR_ShowCrosshair();
void VR_Sbar_Draw();
void VR_AddOrientationToViewAngles(vec3_t angles);
void VR_SetAngles(vec3_t angles);
void VR_ResetOrientation();
void VR_SetMatrices();

#endif