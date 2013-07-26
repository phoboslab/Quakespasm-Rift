// 2013 Dominic Szablewski - phoboslab.org

#include "oculus_sdk.h"
#include "OVR.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f // matches value in gcc v2 math.h
#endif

static OVR::DeviceManager *manager;
static OVR::HMDDevice *hmd;
static OVR::SensorDevice *sensor;
static OVR::SensorFusion *fusion;
static OVR::Util::MagCalibration *magnet;
static OVR::HMDInfo hmdinfo;

int InitOculusSDK()
{
	OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
	manager = OVR::DeviceManager::Create();
	hmd  = manager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();

	if (!hmd) {
		return 0;
	}
	
	sensor = hmd->GetSensor();
	if (!sensor) {
		delete hmd;
		return 0;
	}

	fusion = new OVR::SensorFusion();
	fusion->AttachToSensor(sensor);
	fusion->SetYawCorrectionEnabled(true);

	magnet = new OVR::Util::MagCalibration();

	return 1;
}

void GetOculusView(float view[3])
{
	if (!fusion) {
		return;
	}

	if (magnet && magnet->IsAutoCalibrating()) {
		magnet->UpdateAutoCalibration(*fusion);
	}

	// GetPredictedOrientation() works even if prediction is disabled
	OVR::Quatf q = fusion->GetPredictedOrientation();
	
	q.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&view[1], &view[0], &view[2]);

	view[0] = (-view[0] * 180.0f) / M_PI;
	view[1] = (view[1] * 180.0f) / M_PI;
	view[2] = (-view[2] * 180.0f) / M_PI;
}

void ReleaseOculusSDK()
{
	if (manager) {
		manager->Release();
		manager = NULL;
	}
	if (hmd) {
		hmd->Release();
		hmd = NULL;
	}
	if (sensor) {
		sensor->Release();
		sensor = NULL;
	}
	if (fusion) {
		delete fusion;
		fusion = NULL;
	}

	if (magnet) {
		delete magnet;
		magnet = NULL;
	}

	OVR::System::Destroy();
}

void SetOculusPrediction(float time)
{
	if (!fusion) {
		return;
	}
	if (time > 0.0f) {
		// cap prediction time at 75ms
		fusion->SetPrediction(time < 0.075f ? time : 0.075f, true);
	} else {
		fusion->SetPrediction(0.0f,false);
	}

}

void SetOculusDriftCorrect(int enable)
{
	if (!fusion || !magnet) {
		return;
	}

	if (enable) {
		magnet->BeginAutoCalibration(*fusion);
	} else {
		magnet->ClearCalibration(*fusion);
	}
}

int GetOculusDeviceInfo(unsigned int *h_resolution, unsigned int *v_resolution, float *h_screen_size, 
				        float *v_screen_size, float *interpupillary_distance, float *lens_separation_distance, 
				        float *eye_to_screen_distance, float* distortion_k, float* chrom_abr)
{
	if(!hmd->GetDeviceInfo(&hmdinfo)) {
		return 0;
	}

	*h_resolution = hmdinfo.HResolution;
	*v_resolution = hmdinfo.VResolution;
	*h_screen_size = hmdinfo.HScreenSize;
	*v_screen_size = hmdinfo.VScreenSize;

	*interpupillary_distance = hmdinfo.InterpupillaryDistance;
	*lens_separation_distance = hmdinfo.LensSeparationDistance;
	*eye_to_screen_distance = hmdinfo.EyeToScreenDistance;

	memcpy(distortion_k, hmdinfo.DistortionK, sizeof(float) * 4);
	memcpy(chrom_abr, hmdinfo.ChromaAbCorrection, sizeof(float) * 4);

	return 1;
}

void ResetOculusOrientation()
{
	if(fusion)
		fusion->Reset();
}