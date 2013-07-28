#ifndef __VR_OVR_H
#define __VR_OVR_H

#include "vr.h"

#ifdef __cplusplus
extern "C" {
#endif

int InitOculusSDK();
void GetOculusView(float view[3]);
void ReleaseOculusSDK();
void SetOculusPrediction(float time);
void SetOculusDriftCorrect(int enable);

int GetOculusDeviceInfo(hmd_settings_t *hmd_settings);

void ResetOculusOrientation();

#ifdef __cplusplus
}
#endif

#endif