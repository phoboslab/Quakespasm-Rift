// 2013 Dominic Szablewski - phoboslab.org
// 2014 Jeremiah Sypult - github.com/jeremiah-sypult

#include "vr.h"
#include "../Src/Kernel/OVR_Math.h"
#include "../Src/OVR_Stereo.h"
#include "../Src/OVR_CAPI.h"
#include "../Src/OVR_CAPI_GL.h"

#define BIT_TEST( bits, flag )	( (bits) &   (flag) )		// test
#define BIT_ON( bits, flag )	( (bits) |=  (flag) )		// on
#define BIT_OFF( bits, flag )	( (bits) &= ~(flag) )		// off
#define BIT_FLIP( bits, flag )	( (bits) ^=  (flag) )		// flip

typedef struct
{
//  ovrEyeRenderDesc    RenderDesc;
    ovrPosef            Pose;
    ovrTexture          Texture;
    ovrMatrix4f         Projection;
} OVREyeGlobals;

typedef struct
{
    ovrHmd              HMD;
    ovrHmdDesc          HMDDesc;
    unsigned int        HMDCaps;
    unsigned int        SensorCaps;
    unsigned int        DistortionCaps;
	OVREyeGlobals       Eye[EYE_ALL];
	float				IPD;
    ovrFrameTiming      FrameTiming;
	ovrEyeRenderDesc	EyeRenderDesc[EYE_ALL];
} OVRGlobals;

static OVRGlobals _OVRGlobals = {0};

int OVRInitialize()
{
	unsigned int supportedSensorCaps =
		ovrSensorCap_Orientation|
		ovrSensorCap_YawCorrection|
		ovrSensorCap_Position;

	if ( ! ovr_Initialize() ) {
		return 0;
	}

	_OVRGlobals.HMD = ovrHmd_Create(0);

	if ( ! ( _OVRGlobals.HMD ) && ! ( _OVRGlobals.HMD = ovrHmd_CreateDebug( ovrHmd_DK1 ) ) ) {
		return 0;
	}

	ovrHmd_GetDesc( _OVRGlobals.HMD, &_OVRGlobals.HMDDesc );

	if ( ! ovrHmd_StartSensor( _OVRGlobals.HMD, supportedSensorCaps, ovrSensorCap_Orientation ) ) {
		return 0;
	}

	return 1;
}

void OVRShutdown()
{
	if ( _OVRGlobals.HMD ) {
		ovrHmd_StopSensor( _OVRGlobals.HMD );
		ovrHmd_ConfigureRendering( _OVRGlobals.HMD, NULL, 0, NULL, NULL );
		ovrHmd_Destroy( _OVRGlobals.HMD );
		_OVRGlobals.HMD = NULL;
	}

	ovr_Shutdown();
}

void OVRConfigureEye(eye_t eye, int px, int py, int sw, int sh, GLuint texture)
{
	ovrVector2i texturePos = { px, py };
	ovrSizei textureSize = { sw, sh };
	ovrRecti renderViewport = { texturePos, textureSize };
	ovrGLTextureData* textureData = (ovrGLTextureData*)&_OVRGlobals.Eye[eye].Texture;

    textureData->Header.API            = ovrRenderAPI_OpenGL;
    textureData->Header.TextureSize    = textureSize;
    textureData->Header.RenderViewport = renderViewport;
    textureData->TexId                 = texture;
}

void OVRConfigureRenderer(float multisample, int lowpersistence, int latencytest, int dynamicprediction, int vsync, int chromatic, int timewarp, int vignette)
{
	extern cvar_t gl_nearclip;
	extern cvar_t gl_farclip;
	extern cvar_t vid_vsync;
	extern cvar_t vr_vsync;
	extern cvar_t vr_ipd;
    unsigned hmdCaps;
	unsigned int distortionCaps;
    ovrFovPort eyeFov[EYE_ALL] = { _OVRGlobals.HMDDesc.DefaultEyeFov[EYE_LEFT], _OVRGlobals.HMDDesc.DefaultEyeFov[EYE_RIGHT] };
    float FovSideTanMax   = OVR::FovPort::Max(_OVRGlobals.HMDDesc.DefaultEyeFov[EYE_LEFT], _OVRGlobals.HMDDesc.DefaultEyeFov[EYE_RIGHT]).GetMaxSideTan();
	//float FovSideTanLimit = OVR::FovPort::Max(_OVRGlobals.HMDDesc.MaxEyeFov[EYE_LEFT], _OVRGlobals.HMDDesc.MaxEyeFov[EYE_RIGHT]).GetMaxSideTan();

	// generate the HMD and distortion caps
	hmdCaps = (lowpersistence ? ovrHmdCap_LowPersistence : 0) |
	          (latencytest ? ovrHmdCap_LatencyTest : 0) |
	          (dynamicprediction ? ovrHmdCap_DynamicPrediction : 0) |
	          (vsync ? 0 : ovrHmdCap_NoVSync);

	distortionCaps = (chromatic ? ovrDistortionCap_Chromatic : 0) |
	                 (timewarp ? ovrDistortionCap_TimeWarp : 0) |
	                 (vignette ? ovrDistortionCap_Vignette : 0);

	ovrHmd_SetEnabledCaps( _OVRGlobals.HMD, hmdCaps );

	ovrRenderAPIConfig config = ovrRenderAPIConfig();
	config.Header.API = ovrRenderAPI_OpenGL;
	config.Header.RTSize.w = vid.width;
	config.Header.RTSize.h = vid.height;
	config.Header.Multisample = multisample > 1 ? 1 : 0;

	// clamp fov
    eyeFov[EYE_LEFT] = OVR::FovPort::Min(eyeFov[EYE_LEFT], OVR::FovPort(FovSideTanMax));
    eyeFov[EYE_RIGHT] = OVR::FovPort::Min(eyeFov[EYE_RIGHT], OVR::FovPort(FovSideTanMax));

    if ( !ovrHmd_ConfigureRendering( _OVRGlobals.HMD, &config, distortionCaps, eyeFov, _OVRGlobals.EyeRenderDesc )) {
		// TODO: should this function return a boolean indicator of success?
        return;
    }

	// update the HMD descriptor
	ovrHmd_GetDesc( _OVRGlobals.HMD, &_OVRGlobals.HMDDesc );
	_OVRGlobals.IPD = ovrHmd_GetFloat( _OVRGlobals.HMD, OVR_KEY_IPD, 0.0f );

	// create the projection
	_OVRGlobals.Eye[EYE_LEFT].Projection =
		ovrMatrix4f_Projection( _OVRGlobals.EyeRenderDesc[EYE_LEFT].Fov,  gl_nearclip.value, gl_farclip.value, true );
    _OVRGlobals.Eye[EYE_RIGHT].Projection =
		ovrMatrix4f_Projection( _OVRGlobals.EyeRenderDesc[EYE_RIGHT].Fov,  gl_nearclip.value, gl_farclip.value, true );

	// transpose the projection
	OVR::Matrix4 <float>transposeLeft = _OVRGlobals.Eye[EYE_LEFT].Projection;
	OVR::Matrix4 <float>transposeRight = _OVRGlobals.Eye[EYE_RIGHT].Projection;

	_OVRGlobals.Eye[EYE_LEFT].Projection = transposeLeft.Transposed();
	_OVRGlobals.Eye[EYE_RIGHT].Projection = transposeRight.Transposed();
}

void OVRResetSensor()
{
	ovrHmd_ResetSensor( _OVRGlobals.HMD );
}

void OVRGetFOV(float *horizontalFOV, float *verticalFOV)
{
	OVR::FovPort leftFov = _OVRGlobals.EyeRenderDesc[EYE_LEFT].Fov;
	OVR::FovPort rightFov = _OVRGlobals.EyeRenderDesc[EYE_RIGHT].Fov;

	if ( horizontalFOV ) {
		float horizontalAvgFov = ( leftFov.GetHorizontalFovRadians() + rightFov.GetHorizontalFovRadians() ) * 0.5f;
		float hFOV = OVR::RadToDegree(horizontalAvgFov);

		*horizontalFOV = hFOV;
	}

	if ( verticalFOV ) {
		float verticalAvgFov = ( leftFov.GetVerticalFovRadians() + rightFov.GetVerticalFovRadians() ) * 0.5f;
		float vFOV = OVR::RadToDegree(verticalAvgFov);

		*verticalFOV = vFOV;
	}
}

GLfloat *OVRGetProjectionForEye(eye_t eye)
{
	switch ( eye ) {
		case EYE_LEFT: return (GLfloat*)&_OVRGlobals.Eye[EYE_LEFT].Projection; break;
		case EYE_RIGHT: return (GLfloat*)&_OVRGlobals.Eye[EYE_RIGHT].Projection; break;
		default: return NULL; break;
	}

	return NULL;
}

void OVRGetViewAdjustForEye(eye_t eye, float viewAdjust[3])
{
	ovrVector3f eyeViewAdjust = _OVRGlobals.EyeRenderDesc[eye].ViewAdjust;

	if ( viewAdjust ) {
		viewAdjust[0] = eyeViewAdjust.x;
		viewAdjust[1] = eyeViewAdjust.y;
		viewAdjust[2] = eyeViewAdjust.z;
	}
}

void OVRGetViewAngles(float viewAngles[3])
{
	double absTime = _OVRGlobals.FrameTiming.ThisFrameSeconds ?
		_OVRGlobals.FrameTiming.ScanoutMidpointSeconds :
		ovr_GetTimeInSeconds();
	ovrSensorState ss = ovrHmd_GetSensorState( _OVRGlobals.HMD, absTime );

	OVR::Quatf q;

	if ( ss.StatusFlags & ovrStatus_OrientationTracked ) {
		ovrPosef pose = ss.Predicted.Pose;

		q = pose.Orientation;
		q.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&viewAngles[1], &viewAngles[0], &viewAngles[2]);

		viewAngles[0] = (-viewAngles[0] * 180.0f) / M_PI;
		viewAngles[1] = (viewAngles[1] * 180.0f) / M_PI;
		viewAngles[2] = (-viewAngles[2] * 180.0f) / M_PI;
	}
}

void OVRBeginFrame()
{
	_OVRGlobals.FrameTiming = ovrHmd_BeginFrame( _OVRGlobals.HMD, 0 );
}

void OVREndFrame()
{
	ovrHmd_EndFrame( _OVRGlobals.HMD );
	memset( &_OVRGlobals.FrameTiming, 0, sizeof(ovrFrameTiming) );
}

void OVRBeginEyeRender(eye_t eye)
{
	ovrEyeType eyeType = _OVRGlobals.HMDDesc.EyeRenderOrder[eye];
	_OVRGlobals.Eye[eyeType].Pose = ovrHmd_BeginEyeRender( _OVRGlobals.HMD, eyeType );
}

void OVREndEyeRender(eye_t eye)
{
	ovrEyeType eyeType = _OVRGlobals.HMDDesc.EyeRenderOrder[eye];
	ovrHmd_EndEyeRender( _OVRGlobals.HMD, eyeType, _OVRGlobals.Eye[eyeType].Pose, &_OVRGlobals.Eye[eyeType].Texture );
}

extern "C" {
	vr_library_t OVRLibrary = {
		OVRInitialize,
		OVRShutdown,
		OVRResetSensor,
		OVRConfigureEye,
		OVRConfigureRenderer,
		OVRGetFOV,
		OVRGetProjectionForEye,
		OVRGetViewAdjustForEye,
		OVRGetViewAngles,
		OVRBeginFrame,
		OVREndFrame,
		OVRBeginEyeRender,
		OVREndEyeRender
	};
}
