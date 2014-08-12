// 2013 Dominic Szablewski - phoboslab.org
// 2014 Jeremiah Sypult - github.com/jeremiah-sypult

#include "vr.h"
#include "OVR.h"
#include "OVR_CAPI_GL.h"
#include "CAPI/CAPI_HSWDisplay.h"
#include "OVR_Stereo.h"

// TODO: dismiss HSW?
// ovrHmd_DismissHSWDisplay
// ovrhmd_EnableHSWDisplaySDKRender(hmd, false)

typedef struct
{
    ovrPosef            Pose;
    ovrTexture          Texture;
    ovrMatrix4f         Projection;
    ovrMatrix4f         OrthoProjection;
} OVREyeGlobals;

typedef struct
{
    ovrHmd              HMD;
    ovrHmdDesc          HMDDesc;
    unsigned int        HMDCaps;
    unsigned int        SensorCaps;
    unsigned int        DistortionCaps;
	OVREyeGlobals       Eye[EYE_ALL];
	float               IPD;
    ovrFrameTiming      FrameTiming;
	ovrEyeRenderDesc    EyeRenderDesc[EYE_ALL];
} OVRGlobals;

static OVRGlobals _OVRGlobals = {0};

int OVRInitialize(int debug)
{
	unsigned int supportedTrackingCaps =
		ovrTrackingCap_Orientation|
		ovrTrackingCap_MagYawCorrection|
		ovrTrackingCap_Position;
	ovrHmdType debugHMDType = ovrHmd_None;

	if ( debug != 0 ) {
		debugHMDType = (debug == 1 ? ovrHmd_DK1 : debug == 2 ? ovrHmd_DK2 : ovrHmd_Other);
	}

	if ( ! ovr_Initialize() ) {
		return 0;
	}

	_OVRGlobals.HMD = ovrHmd_Create(0);

	// no HMD? check for vr_debug and attempt to create a debug HMD
	if ( ! _OVRGlobals.HMD && (
		 ! ( debugHMDType != ovrHmd_None ) ||
		 ! ( _OVRGlobals.HMD = ovrHmd_CreateDebug( debugHMDType ) ) ) ) {
		return 0;
	}

	if ( ! ovrHmd_ConfigureTracking( _OVRGlobals.HMD, supportedTrackingCaps, ovrTrackingCap_Orientation ) ) {
		return 0;
	}

	return 1;
}

void OVRShutdown()
{
	if ( _OVRGlobals.HMD ) {
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

int OVRConfigureRenderer(int width, int height, float znear, float zfar, float ipd, float multisample, int lowpersistence, int dynamicprediction, int vsync, int chromatic, int timewarp, int vignette, int state, int flip, int srgb, int overdrive, int profile)
{
    unsigned hmdCaps;
	unsigned int distortionCaps;
    ovrFovPort eyeFov[EYE_ALL] = { _OVRGlobals.HMD->DefaultEyeFov[EYE_LEFT], _OVRGlobals.HMD->DefaultEyeFov[EYE_RIGHT] };
    float FovSideTanMax   = OVR::FovPort::Max(_OVRGlobals.HMD->DefaultEyeFov[EYE_LEFT], _OVRGlobals.HMD->DefaultEyeFov[EYE_RIGHT]).GetMaxSideTan();
	//float FovSideTanLimit = OVR::FovPort::Max(_OVRGlobals.HMD->MaxEyeFov[EYE_LEFT], _OVRGlobals.HMD->MaxEyeFov[EYE_RIGHT]).GetMaxSideTan();
	ovrBool didSetIPD = 0;

	// generate the HMD and distortion caps
	hmdCaps = (lowpersistence ? ovrHmdCap_LowPersistence : 0) |
	          (dynamicprediction ? ovrHmdCap_DynamicPrediction : 0) |
	          (vsync ? 0 : ovrHmdCap_NoVSync);

	distortionCaps = (chromatic ? ovrDistortionCap_Chromatic : 0) |
	                 (timewarp ? ovrDistortionCap_TimeWarp : 0) |
	                 (vignette ? ovrDistortionCap_Vignette : 0) |
					(state ? 0 : ovrDistortionCap_NoRestore) |
					(flip ? ovrDistortionCap_FlipInput : 0) |
					(srgb ? ovrDistortionCap_SRGB : 0) |
					(overdrive ? ovrDistortionCap_Overdrive : 0) |
					(profile ? ovrDistortionCap_ProfileNoTimewarpSpinWaits : 0);

	didSetIPD = ovrHmd_SetFloat( _OVRGlobals.HMD, OVR_KEY_IPD, ipd * 0.001 );

	ovrHmd_SetEnabledCaps( _OVRGlobals.HMD, hmdCaps );

	ovrRenderAPIConfig config = ovrRenderAPIConfig();
	config.Header.API = ovrRenderAPI_OpenGL;
	config.Header.RTSize.w = width;
	config.Header.RTSize.h = height;
	config.Header.Multisample = multisample > 1 ? 1 : 0;

	// clamp fov
    eyeFov[EYE_LEFT] = OVR::FovPort::Min(eyeFov[EYE_LEFT], OVR::FovPort(FovSideTanMax));
    eyeFov[EYE_RIGHT] = OVR::FovPort::Min(eyeFov[EYE_RIGHT], OVR::FovPort(FovSideTanMax));

    if ( !ovrHmd_ConfigureRendering( _OVRGlobals.HMD, &config, distortionCaps, eyeFov, _OVRGlobals.EyeRenderDesc ) ) {
        return 0;
    }

#ifdef DEBUG
	ovrhmd_EnableHSWDisplaySDKRender( _OVRGlobals.HMD, false );
#else
	ovrHmd_DismissHSWDisplay( _OVRGlobals.HMD );
#endif

	_OVRGlobals.IPD = ovrHmd_GetFloat( _OVRGlobals.HMD, OVR_KEY_IPD, ipd * 0.001 );

	// create the projection
	_OVRGlobals.Eye[EYE_LEFT].Projection =
		ovrMatrix4f_Projection( _OVRGlobals.EyeRenderDesc[EYE_LEFT].Fov, znear, zfar, true );
    _OVRGlobals.Eye[EYE_RIGHT].Projection =
		ovrMatrix4f_Projection( _OVRGlobals.EyeRenderDesc[EYE_RIGHT].Fov, znear, zfar, true );

	// transpose the projection
	OVR::Matrix4 <float>transposeLeft = _OVRGlobals.Eye[EYE_LEFT].Projection;
	OVR::Matrix4 <float>transposeRight = _OVRGlobals.Eye[EYE_RIGHT].Projection;

	_OVRGlobals.Eye[EYE_LEFT].Projection = transposeLeft.Transposed();
	_OVRGlobals.Eye[EYE_RIGHT].Projection = transposeRight.Transposed();

	// TODO: ortho
	{
		float    orthoDistance = 0.8f; // 2D is 0.8 meter from camera
		OVR::Vector2f orthoScale0   = OVR::Vector2f(1.0f) / OVR::Vector2f(_OVRGlobals.EyeRenderDesc[EYE_LEFT].PixelsPerTanAngleAtCenter);
		OVR::Vector2f orthoScale1   = OVR::Vector2f(1.0f) / OVR::Vector2f(_OVRGlobals.EyeRenderDesc[EYE_RIGHT].PixelsPerTanAngleAtCenter);

		_OVRGlobals.Eye[EYE_LEFT].OrthoProjection =
			ovrMatrix4f_OrthoSubProjection(_OVRGlobals.Eye[EYE_LEFT].Projection, orthoScale0, orthoDistance, _OVRGlobals.EyeRenderDesc[EYE_LEFT].ViewAdjust.x);

		_OVRGlobals.Eye[EYE_RIGHT].OrthoProjection =
			ovrMatrix4f_OrthoSubProjection(_OVRGlobals.Eye[EYE_RIGHT].Projection, orthoScale1, orthoDistance, _OVRGlobals.EyeRenderDesc[EYE_RIGHT].ViewAdjust.x);

		OVR::Matrix4 <float>transposeLeftOrtho = _OVRGlobals.Eye[EYE_LEFT].OrthoProjection;
		OVR::Matrix4 <float>transposeRightOrtho = _OVRGlobals.Eye[EYE_RIGHT].OrthoProjection;

		_OVRGlobals.Eye[EYE_LEFT].OrthoProjection = transposeLeftOrtho.Transposed();
		_OVRGlobals.Eye[EYE_RIGHT].OrthoProjection = transposeRightOrtho.Transposed();
	}

	return 1;
}

void OVRResetTracking()
{
	ovrHmd_RecenterPose( _OVRGlobals.HMD );
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

GLfloat *OVRGetOrthoProjectionForEye(eye_t eye)
{
	switch ( eye ) {
		case EYE_LEFT: return (GLfloat*)&_OVRGlobals.Eye[EYE_LEFT].OrthoProjection; break;
		case EYE_RIGHT: return (GLfloat*)&_OVRGlobals.Eye[EYE_RIGHT].OrthoProjection; break;
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

int OVRGetPose(float viewAngles[3], float position[3])
{
	double absTime = _OVRGlobals.FrameTiming.ThisFrameSeconds ?
		_OVRGlobals.FrameTiming.ScanoutMidpointSeconds :
		ovr_GetTimeInSeconds();
	ovrTrackingState ts = ovrHmd_GetTrackingState( _OVRGlobals.HMD, absTime );
	ovrPosef pose = ts.HeadPose.ThePose;
	unsigned int statusFlags = ts.StatusFlags;

	if ( viewAngles && ( statusFlags & ovrStatus_OrientationTracked ) ) {
		OVR::Quatf q = pose.Orientation;

		q.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>( &viewAngles[1], &viewAngles[0], &viewAngles[2] );
	}

	if ( position && ( statusFlags & ovrStatus_PositionTracked ) ) {
		position[0] = -pose.Position.z;
		position[1] = pose.Position.x;
		position[2] = pose.Position.y;
	}

	return (statusFlags & ovrStatus_PositionTracked);
}

void OVRBeginFrame()
{
	_OVRGlobals.FrameTiming = ovrHmd_BeginFrame( _OVRGlobals.HMD, 0 );
}

void OVREndFrame()
{
	ovrPosef headPose[2] = {
		ovrHmd_GetEyePose( _OVRGlobals.HMD, _OVRGlobals.HMD->EyeRenderOrder[0] ),
		ovrHmd_GetEyePose( _OVRGlobals.HMD, _OVRGlobals.HMD->EyeRenderOrder[1] )
	};
	ovrTexture eyeTextures[2] = {
		_OVRGlobals.Eye[0].Texture,
		_OVRGlobals.Eye[1].Texture
	};

	ovrHmd_EndFrame( _OVRGlobals.HMD, headPose, eyeTextures );
	memset( &_OVRGlobals.FrameTiming, 0, sizeof(ovrFrameTiming) );
}

extern "C" {
	vr_library_t OVRLibrary = {
		OVRInitialize,
		OVRShutdown,
		OVRResetTracking,
		OVRConfigureEye,
		OVRConfigureRenderer,
		OVRGetFOV,
		OVRGetProjectionForEye,
		OVRGetOrthoProjectionForEye,
		OVRGetViewAdjustForEye,
		OVRGetPose,
		OVRBeginFrame,
		OVREndFrame
	};
}
