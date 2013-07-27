// 2013 Dominic Szablewski - phoboslab.org

#ifndef __R_RENDERHMD_H
#define __R_RENDERHMD_H

#define HMD_AIMMODE_HEAD_MYAW 1 // Head Aiming; View YAW is mouse+head, PITCH is head
#define HMD_AIMMODE_HEAD_MYAW_MPITCH 2 // Head Aiming; View YAW and PITCH is mouse+head
#define HMD_AIMMODE_MOUSE_MYAW 3 // Mouse Aiming; View YAW is mouse+head, PITCH is head
#define HMD_AIMMODE_MOUSE_MYAW_MPITCH 4 // Mouse Aiming; View YAW and PITCH is mouse+head

qboolean R_InitHMDRenderer();
void R_ReleaseHMDRenderer();

void R_SetHMDPredictionTime();
void R_SetHMDDriftCorrection();

void SCR_UpdateHMDScreenContent();
void R_ShowHMDCrosshair();
void HMD_Sbar_Draw();
void V_AddOrientationToViewAngles(vec3_t angles);

#endif