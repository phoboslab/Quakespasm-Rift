// 2013 Dominic Szablewski - phoboslab.org

#include "quakedef.h"

#ifndef __R_VR_H
#define __R_VR_H

#define VR_AIMMODE_HEAD_MYAW 1 // Head Aiming; View YAW is mouse+head, PITCH is head
#define VR_AIMMODE_HEAD_MYAW_MPITCH 2 // Head Aiming; View YAW and PITCH is mouse+head
#define VR_AIMMODE_MOUSE_MYAW 3 // Mouse Aiming; View YAW is mouse+head, PITCH is head
#define VR_AIMMODE_MOUSE_MYAW_MPITCH 4 // Mouse Aiming; View YAW and PITCH is mouse+head
#define VR_AIMMODE_BLENDED 5 // Blended Aiming; Mouse aims, with YAW decoupled for limited area
#define VR_AIMMODE_BLENDED_NOPITCH 6 // Blended Aiming; Mouse aims, with YAW decoupled for limited area, pitch decoupled entirely

#define	VR_CROSSHAIR_NONE 0 // No crosshair
#define	VR_CROSSHAIR_POINT 1 // Point crosshair projected to depth of object it is in front of
#define	VR_CROSSHAIR_LINE 2 // Line crosshair

typedef enum _eye_t
{
	EYE_LEFT     = 0,
	EYE_RIGHT    = 1,
	EYE_ALL      = 2,
	EYE_BOTH     = 2
} eye_t;

typedef struct {
	int  (*Initialize)();
	void (*Shutdown)();
	void (*ResetSensor)();
	void (*ConfigureEye)(eye_t eye, int px, int py, int sw, int sh, GLuint texture);
	void (*ConfigureRenderer)(float multisample, int lowpersistence, int latencytest, int dynamicprediction, int vsync, int chromatic, int timewarp, int vignette);
	void (*GetFOV)(float *horizontalFOV, float *verticalFOV);
	GLfloat*(*GetProjectionForEye)(eye_t eye);
	void (*GetViewAdjustForEye)(eye_t eye, float viewAdjust[3]);
	void (*GetViewAngles)(float viewAngles[3]);
	void (*BeginFrame)();
	void (*EndFrame)();
	void (*BeginEyeRender)(eye_t eye);
	void (*EndEyeRender)(eye_t eye);
} vr_library_t;

void VR_Init();
void VR_RendererInit();

qboolean VR_Enable();
void VR_Disable();

void VR_SetFrustum();
void VR_RenderScene();
void VR_UpdateScreenContent();

void VR_ShowCrosshair();
void VR_Sbar_Draw();
void VR_AddOrientationToViewAngles(vec3_t angles);
void VR_SetAngles();
void VR_ResetOrientation();

#endif
