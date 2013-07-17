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
	fusion->SetPrediction(0.04f, true);
	fusion->SetYawCorrectionEnabled(true);

	magnet = new OVR::Util::MagCalibration();
	magnet->BeginAutoCalibration(*fusion);

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