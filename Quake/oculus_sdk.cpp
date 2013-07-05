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
	fusion->SetPredictionEnabled(true);
	fusion->SetYawCorrectionEnabled(true);
	return 1;
}

void GetOculusView(float view[3])
{
	if (!fusion) {
		return;
	}
	OVR::Quatf q = fusion->GetOrientation();
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
	OVR::System::Destroy();
}