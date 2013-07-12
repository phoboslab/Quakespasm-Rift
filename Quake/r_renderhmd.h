// 2013 Dominic Szablewski - phoboslab.org

#ifndef __R_RENDERHMD_H
#define __R_RENDERHMD_H

struct hmd_settings_t;
extern struct hmd_settings_t oculus_rift_hmd;

qboolean R_InitOculusRift();
void R_ReleaseOculusRift();
qboolean R_InitHMDRenderer(struct hmd_settings_t *hmd);
void R_ReleaseHMDRenderer();
void SCR_UpdateHMDScreenContent();

#endif
