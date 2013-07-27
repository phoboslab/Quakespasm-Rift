// 2013 Dominic Szablewski - phoboslab.org

#ifndef __R_RENDERHMD_H
#define __R_RENDERHMD_H


qboolean R_InitHMDRenderer();
void R_ReleaseHMDRenderer();

void R_SetHMDPredictionTime();
void R_SetHMDDriftCorrection();

void SCR_UpdateHMDScreenContent();
void R_ShowHMDCrosshair();
void HMD_Sbar_Draw();
void V_AddOrientationToViewAngles(vec3_t angles);

#endif