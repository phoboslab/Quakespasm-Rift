// 2013 Dominic Szablewski - phoboslab.org

#ifndef __R_RENDERHMD_H
#define __R_RENDERHMD_H

#define HMD_AIMMODE_HEAD_MYAW 1 // Head Aiming; View YAW is mouse+head, PITCH is head
#define HMD_AIMMODE_HEAD_MYAW_MPITCH 2 // Head Aiming; View YAW and PITCH is mouse+head
#define HMD_AIMMODE_MOUSE_MYAW 3 // Mouse Aiming; View YAW is mouse+head, PITCH is head
#define HMD_AIMMODE_MOUSE_MYAW_MPITCH 4 // Mouse Aiming; View YAW and PITCH is mouse+head
#define HMD_AIMMODE_BLENDED 5 // Blended Aiming; Mouse aims, with YAW decoupled for limited area

#define	HMD_CROSSHAIR_NONE 0 // No crosshair
#define	HMD_CROSSHAIR_POINT 1 // Point crosshair projected to depth of object it is in front of
#define	HMD_CROSSHAIR_LINE 2 // Line crosshair

qboolean R_InitHMDRenderer();
void R_ReleaseHMDRenderer();

void R_SetHMDPredictionTime();
void R_SetHMDDriftCorrection();
void R_SetHMDChromaAbr();
void R_SetHMDIPD();

void SCR_UpdateHMDScreenContent();
void R_ShowHMDCrosshair();
void HMD_Sbar_Draw();
void V_AddOrientationToViewAngles(vec3_t angles);

#endif