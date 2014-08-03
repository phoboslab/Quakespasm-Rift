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

typedef struct {
	unsigned int h_resolution;
	unsigned int v_resolution;
	float h_screen_size;
	float v_screen_size;
	float interpupillary_distance;
	float lens_separation_distance;
	float eye_to_screen_distance;
	float distortion_k[4];
	float chrom_abr[4];
} vr_hmd_settings_t;


typedef struct {
	int (*init)();
	void (*release)();
	int (*get_device_info)(vr_hmd_settings_t *hmd_settings);
	void (*get_view)(float view[3]);
	void (*reset_orientation)();
	void (*set_prediction)(float time);
	void (*set_drift_correction)(int enable);
} vr_interface_t;


void VR_Init();

qboolean VR_Enable();
void VR_Disable();

void VR_UpdateScreenContent();
void VR_ShowCrosshair();
void VR_Sbar_Draw();
void VR_AddOrientationToViewAngles(vec3_t angles);
void VR_SetAngles();
void VR_ResetOrientation();

#endif