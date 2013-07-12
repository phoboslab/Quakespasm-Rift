#ifndef __R_OCULUSSDK_H
#define __R_OCULUSSDK_H

#ifdef __cplusplus
extern "C" {
#endif

int InitOculusSDK();
void GetOculusView(float view[3]);
void ReleaseOculusSDK();

#ifdef __cplusplus
}
#endif

#endif
