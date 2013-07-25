#ifndef __R_OCULUSSDK_H
#define __R_OCULUSSDK_H

#ifdef __cplusplus
extern "C" {
#endif

int InitOculusSDK();
void GetOculusView(float view[3]);
void ReleaseOculusSDK();
void SetOculusPrediction(float time);
void SetOculusDriftCorrect(int enable);

int GetOculusDeviceInfo(unsigned int *h_resolution, unsigned int *v_resolution, float *h_screen_size, 
					    float *v_screen_size, float *interpupillary_distance, float *lens_separation_distance, 
				        float *eye_to_screen_distance, float* distortion_k, float* chrom_abr);

#ifdef __cplusplus
}
#endif

#endif