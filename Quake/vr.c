// 2016 Dominic Szablewski - phoboslab.org

#include "quakedef.h"
#include "vr.h"
#include "vr_menu.h"

#define UNICODE 1
#include <mmsystem.h>
#undef UNICODE

#include "OVR_CAPI_GL.h"
#include "OVR_CAPI_Audio.h"

extern void VID_Refocus();

typedef struct {
	GLuint framebuffer, depth_texture;
	ovrTextureSwapChain swap_chain;
	struct {
		float width, height;
	} size;
} fbo_t;

typedef struct {
	int index;
	fbo_t fbo;
	ovrEyeRenderDesc render_desc;
	ovrPosef pose;
	float fov_x, fov_y;
} vr_eye_t;


// OpenGL Extensions
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9

typedef void (APIENTRYP PFNGLBLITFRAMEBUFFEREXTPROC) (GLint,  GLint,  GLint,  GLint,  GLint,  GLint,  GLint,  GLint,  GLbitfield,  GLenum);
typedef BOOL (APIENTRYP PFNWGLSWAPINTERVALEXTPROC) (int);

static PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
static PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT;
static PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
static PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
static PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
static PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

struct {
	void *func; char *name;
} gl_extensions[] = {
	{&glBindFramebufferEXT, "glBindFramebufferEXT"},
	{&glBlitFramebufferEXT, "glBlitFramebufferEXT"},
	{&glDeleteFramebuffersEXT, "glDeleteFramebuffersEXT"},
	{&glGenFramebuffersEXT, "glGenFramebuffersEXT"},
	{&glFramebufferTexture2DEXT, "glFramebufferTexture2DEXT"},
	{&glFramebufferRenderbufferEXT, "glFramebufferRenderbufferEXT"},
	{&wglSwapIntervalEXT, "wglSwapIntervalEXT"},
	{NULL, NULL},
};

// main screen & 2D drawing
extern void SCR_SetUpToDrawConsole (void);
extern void SCR_UpdateScreenContent();
extern qboolean	scr_drawdialog;
extern void SCR_DrawNotifyString (void);
extern qboolean	scr_drawloading;
extern void SCR_DrawLoading (void);
extern void SCR_CheckDrawCenterString (void);
extern void SCR_DrawRam (void);
extern void SCR_DrawNet (void);
extern void SCR_DrawTurtle (void);
extern void SCR_DrawPause (void);
extern void SCR_DrawDevStats (void);
extern void SCR_DrawFPS (void);
extern void SCR_DrawClock (void);
extern void SCR_DrawConsole (void);

// rendering
extern void R_SetupView(void);
extern void R_RenderScene(void);
extern int glx, gly, glwidth, glheight;
extern refdef_t r_refdef;
extern vec3_t vright;


static ovrSession session;
static ovrHmdDesc hmd;

static vr_eye_t eyes[2];
static vr_eye_t *current_eye = NULL;
static vec3_t lastOrientation = {0, 0, 0};
static vec3_t lastAim = {0, 0, 0};

static qboolean vr_initialized = false;
static ovrMirrorTexture mirror_texture;
static ovrMirrorTextureDesc mirror_texture_desc;
static GLuint mirror_fbo = 0;
static int attempt_to_refocus_retry = 0;


// Wolfenstein 3D, DOOM and QUAKE use the same coordinate/unit system:
// 8 foot (96 inch) height wall == 64 units, 1.5 inches per pixel unit
// 1.0 pixel unit / 1.5 inch == 0.666666 pixel units per inch
static const float meters_to_units = 1.0f/(1.5f * 0.0254f);


extern cvar_t gl_farclip;
extern int glwidth, glheight;
extern void SCR_UpdateScreenContent();
extern refdef_t r_refdef;
extern cvar_t snd_device;

cvar_t vr_enabled = {"vr_enabled", "0", CVAR_NONE};
cvar_t vr_crosshair = {"vr_crosshair","1", CVAR_ARCHIVE};
cvar_t vr_crosshair_depth = {"vr_crosshair_depth","0", CVAR_ARCHIVE};
cvar_t vr_crosshair_size = {"vr_crosshair_size","3.0", CVAR_ARCHIVE};
cvar_t vr_crosshair_alpha = {"vr_crosshair_alpha","0.25", CVAR_ARCHIVE};
cvar_t vr_aimmode = {"vr_aimmode","1", CVAR_ARCHIVE};
cvar_t vr_deadzone = {"vr_deadzone","30",CVAR_ARCHIVE};
cvar_t vr_perfhud = {"vr_perfhud", "0", CVAR_ARCHIVE};


static qboolean InitOpenGLExtensions()
{
	int i;
	static qboolean extensions_initialized;

	if (extensions_initialized)
		return true;

	for( i = 0; gl_extensions[i].func; i++ ) {
		void *func = SDL_GL_GetProcAddress(gl_extensions[i].name);
		if (!func) 
			return false;

		*((void **)gl_extensions[i].func) = func;
	}

	extensions_initialized = true;
	return extensions_initialized;
}

fbo_t CreateFBO(int width, int height) {
	int i;
	fbo_t fbo;
	ovrTextureSwapChainDesc swap_desc;
	int swap_chain_length = 0;

	fbo.size.width = width;
	fbo.size.height = height;

	glGenFramebuffersEXT(1, &fbo.framebuffer);

	glGenTextures(1, &fbo.depth_texture);
	glBindTexture(GL_TEXTURE_2D,  fbo.depth_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

	
	swap_desc.Type = ovrTexture_2D;
	swap_desc.ArraySize = 1;
	swap_desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	swap_desc.Width = width;
	swap_desc.Height = height;
	swap_desc.MipLevels = 1;
	swap_desc.SampleCount = 1;
	swap_desc.StaticImage = ovrFalse;
	swap_desc.MiscFlags = 0;
	swap_desc.BindFlags = 0;

	ovr_CreateTextureSwapChainGL(session, &swap_desc, &fbo.swap_chain);	
	ovr_GetTextureSwapChainLength(session, fbo.swap_chain, &swap_chain_length);
	for( i = 0; i < swap_chain_length; ++i ) {
		int swap_texture_id = 0;
		ovr_GetTextureSwapChainBufferGL(session, fbo.swap_chain, i, &swap_texture_id);

		glBindTexture(GL_TEXTURE_2D, swap_texture_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	return fbo;
}

void DeleteFBO(fbo_t fbo) {
	glDeleteFramebuffersEXT(1, &fbo.framebuffer);
	glDeleteTextures(1, &fbo.depth_texture);
	ovr_DestroyTextureSwapChain(session, fbo.swap_chain);
}

void QuatToYawPitchRoll(ovrQuatf q, vec3_t out) {
	float sqw = q.w*q.w;
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;
	float unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
	float test = q.x*q.y + q.z*q.w;
	if( test > 0.499*unit ) { // singularity at north pole
		out[YAW] = 2 * atan2(q.x,q.w) / M_PI_DIV_180;
		out[ROLL] = -M_PI/2 / M_PI_DIV_180 ;
		out[PITCH] = 0;
	}
	else if( test < -0.499*unit ) { // singularity at south pole
		out[YAW] = -2 * atan2(q.x,q.w) / M_PI_DIV_180;
		out[ROLL] = M_PI/2 / M_PI_DIV_180;
		out[PITCH] = 0;
	}
	else {
		out[YAW] = atan2(2*q.y*q.w-2*q.x*q.z , sqx - sqy - sqz + sqw) / M_PI_DIV_180;
		out[ROLL] = -asin(2*test/unit) / M_PI_DIV_180;
		out[PITCH] = -atan2(2*q.x*q.w-2*q.y*q.z , -sqx + sqy - sqz + sqw) / M_PI_DIV_180;
	}
}

void Vec3RotateZ(vec3_t in, float angle, vec3_t out) {
	out[0] = in[0]*cos(angle) - in[1]*sin(angle);
	out[1] = in[0]*sin(angle) + in[1]*cos(angle);
	out[2] = in[2];
}

ovrMatrix4f TransposeMatrix(ovrMatrix4f in) {
	ovrMatrix4f out;
	int y, x;
	for( y = 0; y < 4; y++ )
		for( x = 0; x < 4; x++ )
			out.M[x][y] = in.M[y][x];

	return out;
}


// ----------------------------------------------------------------------------
// Callbacks for cvars

static void VR_Enabled_f (cvar_t *var)
{
	VR_Disable();

	if (!vr_enabled.value) 
		return;

	if( !VR_Enable() )
		Cvar_SetValueQuick(&vr_enabled,0);
}



static void VR_Deadzone_f (cvar_t *var)
{
	// clamp the mouse to a max of 0 - 70 degrees
	float deadzone = CLAMP (0.0f, vr_deadzone.value, 70.0f);
	if (deadzone != vr_deadzone.value)
		Cvar_SetValueQuick(&vr_deadzone, deadzone);
}



static void VR_Perfhud_f (cvar_t *var)
{
	if (vr_initialized)
	{
		ovr_SetInt(session, OVR_PERF_HUD_MODE, (int)vr_perfhud.value);
	}
}



// ----------------------------------------------------------------------------
// Public vars and functions

void VR_Init()
{
	// This is only called once at game start
	Cvar_RegisterVariable (&vr_enabled);
	Cvar_SetCallback (&vr_enabled, VR_Enabled_f);
	Cvar_RegisterVariable (&vr_crosshair);
	Cvar_RegisterVariable (&vr_crosshair_depth);
	Cvar_RegisterVariable (&vr_crosshair_size);
	Cvar_RegisterVariable (&vr_crosshair_alpha);
	Cvar_RegisterVariable (&vr_aimmode);
	Cvar_RegisterVariable (&vr_deadzone);
	Cvar_SetCallback (&vr_deadzone, VR_Deadzone_f);
	Cvar_RegisterVariable (&vr_perfhud);
	Cvar_SetCallback (&vr_perfhud, VR_Perfhud_f);

	VR_Menu_Init();

	// Set the cvar if invoked from a command line parameter
	{
		int i = COM_CheckParm("-vr");
		if ( i && i < com_argc - 1 ) {
			Cvar_SetQuick( &vr_enabled, "1" );
		}
	}
}

qboolean VR_Enable()
{
	int i;
	static ovrGraphicsLuid luid;
	int mirror_texture_id = 0;
	UINT ovr_audio_id;

	if( ovr_Initialize(NULL) != ovrSuccess ) {
		Con_Printf("Failed to Initialize Oculus SDK");
		return false;
	}

	if( ovr_Create(&session, &luid) != ovrSuccess ) {
		Con_Printf("Failed to get HMD");
		return false;
	}

	if( !InitOpenGLExtensions() ) {
		Con_Printf("Failed to initialize OpenGL extensions");
		return false;
	}
	

	mirror_texture_desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	mirror_texture_desc.Width = glwidth;
	mirror_texture_desc.Height = glheight;

	ovr_CreateMirrorTextureGL(session, &mirror_texture_desc, &mirror_texture);
	glGenFramebuffersEXT(1, &mirror_fbo);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, mirror_fbo);

	ovr_GetMirrorTextureBufferGL(session, mirror_texture, &mirror_texture_id);
	glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mirror_texture_id, 0);
	glFramebufferRenderbufferEXT(GL_READ_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
	
	hmd = ovr_GetHmdDesc(session);
	
	for( i = 0; i < 2; i++ ) {
		ovrSizei size = ovr_GetFovTextureSize(session, (ovrEyeType)i, hmd.DefaultEyeFov[i], 1.0f);

		eyes[i].index = i;
		eyes[i].fbo = CreateFBO(size.w, size.h);
		eyes[i].render_desc = ovr_GetRenderDesc(session, (ovrEyeType)i, hmd.DefaultEyeFov[i]);
		eyes[i].fov_x = (atan(hmd.DefaultEyeFov[i].LeftTan) + atan(hmd.DefaultEyeFov[i].RightTan)) / M_PI_DIV_180;
		eyes[i].fov_y = (atan(hmd.DefaultEyeFov[i].UpTan) + atan(hmd.DefaultEyeFov[i].DownTan)) / M_PI_DIV_180;
	}
	
	wglSwapIntervalEXT(0); // Disable V-Sync
	
	// Set the Rift as audio device
	ovr_GetAudioDeviceOutWaveId(&ovr_audio_id);
	if (ovr_audio_id != WAVE_MAPPER)
	{
		// Get the name of the device and set it as snd_device. 
		WAVEOUTCAPS caps;
		MMRESULT mmr = waveOutGetDevCaps(ovr_audio_id, &caps, sizeof(caps));
		if (mmr == MMSYSERR_NOERROR)
		{
			char *name = SDL_iconv_string("UTF-8", "UTF-16LE", (char *)(caps.szPname), (SDL_wcslen((WCHAR *)caps.szPname)+1)*sizeof(WCHAR));
			Cvar_SetQuick(&snd_device, name);
		}
	}

	attempt_to_refocus_retry = 900; // Try to refocus our for the first 900 frames :/
	vr_initialized = true;
	return true;
}


void VR_Shutdown() {
	VR_Disable();
}

void VR_Disable()
{
	int i;
	if( !vr_initialized )
		return;
	
	Cvar_SetQuick(&snd_device, "default");

	for( i = 0; i < 2; i++ ) {
		DeleteFBO(eyes[i].fbo);
	}
	ovr_DestroyMirrorTexture(session, mirror_texture);
	ovr_Destroy(session);
	ovr_Shutdown();

	vr_initialized = false;
}

static void RenderScreenForCurrentEye()
{
	int swap_index = 0;
	int swap_texture_id = 0;

	// Remember the current glwidht/height; we have to modify it here for each eye
	int oldglheight = glheight;
	int oldglwidth = glwidth;

	glwidth = current_eye->fbo.size.width;
	glheight = current_eye->fbo.size.height;

	
	ovr_GetTextureSwapChainCurrentIndex(session, current_eye->fbo.swap_chain, &swap_index);
	ovr_GetTextureSwapChainBufferGL(session, current_eye->fbo.swap_chain, swap_index, &swap_texture_id);

	// Set up current FBO
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, current_eye->fbo.framebuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, swap_texture_id, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, current_eye->fbo.depth_texture, 0);

	glViewport(0, 0, current_eye->fbo.size.width, current_eye->fbo.size.height);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	// Draw everything
	srand((int) (cl.time * 1000)); //sync random stuff between eyes

	r_refdef.fov_x = current_eye->fov_x;
	r_refdef.fov_y = current_eye->fov_y;

	SCR_UpdateScreenContent ();
	ovr_CommitTextureSwapChain(session, current_eye->fbo.swap_chain);

	
	// Reset
	glwidth = oldglwidth;
	glheight = oldglheight;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0);
}

void VR_UpdateScreenContent()
{
	int i;
	vec3_t orientation;
	ovrVector3f view_offset[2];
	ovrPosef render_pose[2];

	double ftiming, pose_time;
	ovrTrackingState hmdState;

	ovrViewScaleDesc viewScaleDesc;
	ovrLayerEyeFov ld;
	ovrLayerHeader* layers;
	GLint w, h;
	
	// Last chance to enable VR Mode - we get here when the game already start up with vr_enabled 1
	// If enabling fails, unset the cvar and return.
	if( !vr_initialized && !VR_Enable() ) {
		Cvar_Set ("vr_enabled", "0");
		return;
	}

	w = mirror_texture_desc.Width;
	h = mirror_texture_desc.Height;

	// Get current orientation of the HMD
	ftiming = ovr_GetPredictedDisplayTime(session, 0);
	pose_time = ovr_GetTimeInSeconds();
	hmdState = ovr_GetTrackingState(session, ftiming, false);


	// Calculate HMD angles and blend with input angles based on current aim mode
	QuatToYawPitchRoll(hmdState.HeadPose.ThePose.Orientation, orientation);
	switch( (int)vr_aimmode.value )
	{
		// 1: (Default) Head Aiming; View YAW is mouse+head, PITCH is head
		default:
		case VR_AIMMODE_HEAD_MYAW:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] = orientation[PITCH];
			cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - lastOrientation[YAW];
			break;
		
		// 2: Head Aiming; View YAW and PITCH is mouse+head (this is stupid)
		case VR_AIMMODE_HEAD_MYAW_MPITCH:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH] - lastOrientation[PITCH];
			cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - lastOrientation[YAW];
			break;
		
		// 3: Mouse Aiming; View YAW is mouse+head, PITCH is head
		case VR_AIMMODE_MOUSE_MYAW:
			cl.viewangles[PITCH] = orientation[PITCH];
			cl.viewangles[YAW]   = cl.aimangles[YAW] + orientation[YAW];
			break;
		
		// 4: Mouse Aiming; View YAW and PITCH is mouse+head
		case VR_AIMMODE_MOUSE_MYAW_MPITCH:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH];
			cl.viewangles[YAW]   = cl.aimangles[YAW] + orientation[YAW];
			break;
		
		case VR_AIMMODE_BLENDED:
		case VR_AIMMODE_BLENDED_NOPITCH:
			{
				float diffHMDYaw = orientation[YAW] - lastOrientation[YAW];
				float diffHMDPitch = orientation[PITCH] - lastOrientation[PITCH];
				float diffAimYaw = cl.aimangles[YAW] - lastAim[YAW];
				float diffYaw;

				// find new view position based on orientation delta
				cl.viewangles[YAW] += diffHMDYaw;

				// find difference between view and aim yaw
				diffYaw = cl.viewangles[YAW] - cl.aimangles[YAW];

				if (abs(diffYaw) > vr_deadzone.value / 2.0f)
				{
					// apply the difference from each set of angles to the other
					cl.aimangles[YAW] += diffHMDYaw;
					cl.viewangles[YAW] += diffAimYaw;
				}
				if ( (int)vr_aimmode.value == VR_AIMMODE_BLENDED ) {
					cl.aimangles[PITCH] += diffHMDPitch;
				}
				cl.viewangles[PITCH]  = orientation[PITCH];
			}
			break;
	}
	cl.viewangles[ROLL]  = orientation[ROLL];

	VectorCopy (orientation, lastOrientation);
	VectorCopy (cl.aimangles, lastAim);
	
	VectorCopy (cl.viewangles, r_refdef.viewangles);
	VectorCopy (cl.aimangles, r_refdef.aimangles);


	// Calculate eye poses
	view_offset[0] = eyes[0].render_desc.HmdToEyeOffset;
	view_offset[1] = eyes[1].render_desc.HmdToEyeOffset;

	ovr_CalcEyePoses(hmdState.HeadPose.ThePose, view_offset, render_pose);
	eyes[0].pose = render_pose[0];
	eyes[1].pose = render_pose[1];


	// Render the scene for each eye into their FBOs
	for( i = 0; i < 2; i++ ) {
		current_eye = &eyes[i];
		RenderScreenForCurrentEye();
	}
	

	// Submit the FBOs to OVR
	viewScaleDesc.HmdSpaceToWorldScaleInMeters = meters_to_units;
	viewScaleDesc.HmdToEyeOffset[0] = view_offset[0];
	viewScaleDesc.HmdToEyeOffset[1] = view_offset[1];

	ld.Header.Type = ovrLayerType_EyeFov;
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

	for( i = 0; i < 2; i++ ) {
		ld.ColorTexture[i] = eyes[i].fbo.swap_chain;
		ld.Viewport[i].Pos.x = 0;
		ld.Viewport[i].Pos.y = 0;
		ld.Viewport[i].Size.w = eyes[i].fbo.size.width;
		ld.Viewport[i].Size.h = eyes[i].fbo.size.height;
		ld.Fov[i] = hmd.DefaultEyeFov[i];
		ld.RenderPose[i] = eyes[i].pose;
		ld.SensorSampleTime = pose_time;
	}

	layers = &ld.Header;
	ovr_SubmitFrame(session, 0, &viewScaleDesc, &layers, 1);

	// Blit mirror texture to backbuffer
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, mirror_fbo);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
	glBlitFramebufferEXT(0, h, w, 0, 0, 0, w, h,GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);


	// OVR steals the mouse focus when fading in our window. As a stupid workaround, we simply
	// refocus the window each frame, for the first 900 frames.
	if (attempt_to_refocus_retry > 0)
	{
		VID_Refocus();
		attempt_to_refocus_retry--;
	}
}

void VR_SetMatrices() {
	vec3_t temp, orientation, position;
	ovrMatrix4f projection;

	// Calculat HMD projection matrix and view offset position
	projection = TransposeMatrix(ovrMatrix4f_Projection(hmd.DefaultEyeFov[current_eye->index], 4, gl_farclip.value, ovrProjection_None));
	
	// We need to scale the view offset position to quake units and rotate it by the current input angles (viewangle - eye orientation)
	QuatToYawPitchRoll(current_eye->pose.Orientation, orientation);
	temp[0] = -current_eye->pose.Position.z * meters_to_units;
	temp[1] = -current_eye->pose.Position.x * meters_to_units;
	temp[2] = current_eye->pose.Position.y * meters_to_units;
	Vec3RotateZ(temp, (r_refdef.viewangles[YAW]-orientation[YAW])*M_PI_DIV_180, position);


	// Set OpenGL projection and view matrices
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((GLfloat*)projection.M);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity ();
	
	glRotatef (-90,  1, 0, 0); // put Z going up
	glRotatef (90,  0, 0, 1); // put Z going up
		
	glRotatef (-r_refdef.viewangles[PITCH],  0, 1, 0);
	glRotatef (-r_refdef.viewangles[ROLL],  1, 0, 0);
	glRotatef (-r_refdef.viewangles[YAW],  0, 0, 1);
	
	glTranslatef (-r_refdef.vieworg[0] -position[0],  -r_refdef.vieworg[1]-position[1],  -r_refdef.vieworg[2]-position[2]);
}

void VR_AddOrientationToViewAngles(vec3_t angles)
{
	vec3_t orientation;
	QuatToYawPitchRoll(current_eye->pose.Orientation, orientation);

	angles[PITCH] = angles[PITCH] + orientation[PITCH]; 
	angles[YAW] = angles[YAW] + orientation[YAW]; 
	angles[ROLL] = orientation[ROLL];
}

void VR_ShowCrosshair ()
{
	vec3_t forward, up, right;
	vec3_t start, end, impact;
	float size, alpha;

	if( (sv_player && (int)(sv_player->v.weapon) == IT_AXE) )
		return;

	size = CLAMP (0.0, vr_crosshair_size.value, 32.0);
	alpha = CLAMP (0.0, vr_crosshair_alpha.value, 1.0);

	if ( size <= 0 || alpha <= 0 )
		return;

	// setup gl
	glDisable (GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_PolygonOffset (OFFSET_SHOWTRIS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_CULL_FACE);

	// calc the line and draw
	VectorCopy (cl.viewent.origin, start);
	start[2] -= cl.viewheight - 10;
	AngleVectors (cl.aimangles, forward, right, up);

	switch((int) vr_crosshair.value)
	{	
		default:
		case VR_CROSSHAIR_POINT:
			if (vr_crosshair_depth.value <= 0) {
				 // trace to first wall
				VectorMA (start, 4096, forward, end);
				TraceLine (start, end, impact);
			} else {
				// fix crosshair to specific depth
				VectorMA (start, vr_crosshair_depth.value * meters_to_units, forward, impact);
			}

			glEnable(GL_POINT_SMOOTH);
			glColor4f (1, 0, 0, alpha);
			glPointSize( size * glwidth / vid.width );

			glBegin(GL_POINTS);
			glVertex3f (impact[0], impact[1], impact[2]);
			glEnd();
			glDisable(GL_POINT_SMOOTH);
			break;

		case VR_CROSSHAIR_LINE:
			// trace to first entity
			VectorMA (start, 4096, forward, end);
			TraceLineToEntity (start, end, impact, sv_player);

			glColor4f (1, 0, 0, alpha);
			glLineWidth( size * glwidth / vid.width );
			glBegin (GL_LINES);
			glVertex3f (start[0], start[1], start[2]);
			glVertex3f (impact[0], impact[1], impact[2]);
			glEnd ();
			break;
	}

	// cleanup gl
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_CULL_FACE);
	glDisable(GL_BLEND);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	GL_PolygonOffset (OFFSET_NONE);
	glEnable (GL_DEPTH_TEST);
}

void VR_Draw2D()
{
	qboolean draw_sbar = false;
	vec3_t menu_angles, forward, right, up, target;
	float scale_hud = 0.13;

	int oldglwidth = glwidth, 
		oldglheight = glheight,
		oldconwidth = vid.conwidth,
		oldconheight = vid.conheight;

	glwidth = 320;
	glheight = 200;
	
	vid.conwidth = 320;
	vid.conheight = 200;

	// draw 2d elements 1m from the users face, centered
	glPushMatrix();
	glDisable (GL_DEPTH_TEST); // prevents drawing sprites on sprites from interferring with one another
	glEnable (GL_BLEND);

	VectorCopy(r_refdef.aimangles, menu_angles)

	if (vr_aimmode.value == VR_AIMMODE_HEAD_MYAW || vr_aimmode.value == VR_AIMMODE_HEAD_MYAW_MPITCH)
		menu_angles[PITCH] = 0;

	AngleVectors (menu_angles, forward, right, up);

	VectorMA (r_refdef.vieworg, 48, forward, target);

	glTranslatef (target[0],  target[1],  target[2]);
	glRotatef(menu_angles[YAW] - 90, 0, 0, 1); // rotate around z
	glRotatef(90 + menu_angles[PITCH], -1, 0, 0); // keep bar at constant angled pitch towards user
	glTranslatef (-(320.0 * scale_hud / 2), -(200.0 * scale_hud / 2), 0); // center the status bar
	glScalef(scale_hud, scale_hud, scale_hud);


	if (scr_drawdialog) //new game confirm
	{
		if (con_forcedup)
			Draw_ConsoleBackground ();
		else
			draw_sbar = true; //Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	}
	else if (scr_drawloading) //loading
	{
		SCR_DrawLoading ();
		draw_sbar = true; //Sbar_Draw ();
	}
	else if (cl.intermission == 1 && key_dest == key_game) //end of level
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game) //end of episode
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else
	{
		//SCR_DrawCrosshair (); //johnfitz
		SCR_DrawRam ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		draw_sbar = true; //Sbar_Draw ();
		SCR_DrawDevStats (); //johnfitz
		SCR_DrawFPS (); //johnfitz
		SCR_DrawClock (); //johnfitz
		SCR_DrawConsole ();
		M_Draw ();
	}

	glDisable (GL_BLEND);
	glEnable (GL_DEPTH_TEST);
	glPopMatrix();

	if(draw_sbar)
		VR_DrawSbar();

	glwidth = oldglwidth;
	glheight = oldglheight;
	vid.conwidth = oldconwidth;
	vid.conheight =	oldconheight;
}

void VR_DrawSbar()
{	
	vec3_t sbar_angles, forward, right, up, target;
	float scale_hud = 0.025;

	glPushMatrix();
	glDisable (GL_DEPTH_TEST); // prevents drawing sprites on sprites from interferring with one another

	VectorCopy(cl.aimangles, sbar_angles)

	if (vr_aimmode.value == VR_AIMMODE_HEAD_MYAW || vr_aimmode.value == VR_AIMMODE_HEAD_MYAW_MPITCH)
		sbar_angles[PITCH] = 0;

	AngleVectors (sbar_angles, forward, right, up);

	VectorMA (cl.viewent.origin, 1.0, forward, target);

	glTranslatef (target[0],  target[1],  target[2]);
	glRotatef(sbar_angles[YAW] - 90, 0, 0, 1); // rotate around z
	glRotatef(90 + 45 + sbar_angles[PITCH], -1, 0, 0); // keep bar at constant angled pitch towards user
	glTranslatef (-(320.0 * scale_hud / 2), 0, 0); // center the status bar
	glTranslatef (0,  0,  10); // move hud down a bit
	glScalef(scale_hud, scale_hud, scale_hud);

	Sbar_Draw ();

	glEnable (GL_DEPTH_TEST);
	glPopMatrix();
}

void VR_SetAngles(vec3_t angles)
{
	VectorCopy(angles,cl.aimangles);
	VectorCopy(angles,cl.viewangles);
	VectorCopy(angles,lastAim);
}

void VR_ResetOrientation()
{
	cl.aimangles[YAW] = cl.viewangles[YAW];	
	cl.aimangles[PITCH] = cl.viewangles[PITCH];
	if (vr_enabled.value) {
		ovr_RecenterTrackingOrigin(session);
		VectorCopy(cl.aimangles,lastAim);
	}
}
