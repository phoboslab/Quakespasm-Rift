// 2013 Dominic Szablewski - phoboslab.org
#include "OVR.h"
#include "vr.h"

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

	magnet = NULL;

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

		fusion->SetPrediction(time, true);
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
		if (!magnet)
				magnet = new OVR::Util::MagCalibration();
		magnet->BeginAutoCalibration(*fusion);
	} else {
		if (magnet)
		{
			magnet->ClearCalibration(*fusion);
			delete magnet;
			magnet = NULL;
		}
	}
}

int GetOculusDeviceInfo(vr_hmd_settings_t *hmd_settings)
{
	if(!hmd || !hmd->GetDeviceInfo(&hmdinfo)) {
		return 0;
	}

	hmd_settings->h_resolution = hmdinfo.HResolution;
	hmd_settings->v_resolution = hmdinfo.VResolution;
	hmd_settings->h_screen_size = hmdinfo.HScreenSize;
	hmd_settings->v_screen_size = hmdinfo.VScreenSize;

	hmd_settings->interpupillary_distance = hmdinfo.InterpupillaryDistance;
	hmd_settings->lens_separation_distance = hmdinfo.LensSeparationDistance;
	hmd_settings->eye_to_screen_distance = hmdinfo.EyeToScreenDistance;

	memcpy(hmd_settings->distortion_k, hmdinfo.DistortionK, sizeof(float) * 4);
	memcpy(hmd_settings->chrom_abr, hmdinfo.ChromaAbCorrection, sizeof(float) * 4);

	return 1;
}

void ResetOculusOrientation()
{
	if(fusion)
		fusion->Reset();

	if (magnet && magnet->IsAutoCalibrating()) {
		magnet->BeginAutoCalibration(*fusion);
	}

}

extern "C" {
	vr_interface_t vr_interface_ovr = {
		InitOculusSDK,
		ReleaseOculusSDK,
		GetOculusDeviceInfo,
		GetOculusView,
		ResetOculusOrientation,
		SetOculusPrediction,
		SetOculusDriftCorrect
	};
}
