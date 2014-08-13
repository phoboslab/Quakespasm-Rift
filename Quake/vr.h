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

typedef enum _vr_menu_options_t
{
	VR_OPTION_ENABLED,
	VR_OPTION_DEBUG,
	VR_OPTION_IPD,
	VR_OPTION_POSITION,

	VR_OPTION_MULTISAMPLE,
	VR_OPTION_LOWPERSISTENCE,
	VR_OPTION_DYNAMICPREDICTION,
	VR_OPTION_VSYNC,

	VR_OPTION_CHROMATIC,
	VR_OPTION_TIMEWARP,
	VR_OPTION_VIGNETTE,
	VR_OPTION_OVERDRIVE,

	VR_OPTION_AIMMODE,
	VR_OPTION_DEADZONE,
	VR_OPTION_CROSSHAIR,
	VR_OPTION_CROSSHAIR_DEPTH,
	VR_OPTION_CROSSHAIR_SIZE,
	VR_OPTION_CROSSHAIR_ALPHA,

	VR_OPTION_MAX
} vr_menu_options_t;

typedef enum _eye_t
{
	EYE_LEFT     = 0,
	EYE_RIGHT    = 1,
	EYE_ALL      = 2,
	EYE_BOTH     = 2
} eye_t;

typedef enum _vr_position_t
{
	VR_POSITION_NONE = 0,
	VR_POSITION_DEFAULT = 1,
	VR_POSITION_VIEWENTITY = 2,
	VR_POSITION_MAX
} vr_position_t;

typedef struct {
	int  (*Initialize)(int debug);
	void (*Shutdown)();
	void (*ResetTracking)();
	void (*ConfigureEye)(eye_t eye, int px, int py, int sw, int sh, GLuint texture);
	int  (*ConfigureRenderer)(int width, int height, float znear, float zfar, float ipd, float multisample, int lowpersistence, int dynamicprediction, int vsync, int chromatic, int timewarp, int vignette, int state, int flip, int srgb, int overdrive, int profile);
	void (*GetFOV)(float *horizontalFOV, float *verticalFOV);
	GLfloat* (*GetProjectionForEye)(eye_t eye);
	void (*GetViewAdjustForEye)(eye_t eye, float viewAdjust[3]);
	int  (*GetPose)(float viewAngles[3], float position[3]);
	void (*BeginFrame)();
	void (*EndFrame)();
} vr_library_t;

void VR_Init();
void VR_RendererInit();

qboolean VR_Enable();
void VR_Disable();

void VR_SetFrustum();
void VR_SetupView();
void VR_UpdateScreenContent();

void VR_ShowCrosshair();
void VR_Sbar_Draw();
void VR_AddPositionToViewOrigin(vec3_t position);
void VR_AddOrientationToViewAngles(vec3_t angles);

void VR_Draw2D();
void VR_SetAngles();
void VR_ResetOrientation();

extern void (*vr_menucmdfn)(void); // jeremiah sypult
extern void (*vr_menudrawfn)(void);
extern void (*vr_menukeyfn)(int key);

#endif
