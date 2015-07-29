// 2013 Dominic Szablewski - phoboslab.org

#include "quakedef.h"
#include "vr.h"

#include "OVR_CAPI_GL.h"

typedef struct {
	GLuint framebuffer, depth_texture;
	ovrSwapTextureSet *color_textures;
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


static ovrHmd hmd;
static vr_eye_t eyes[2];
static vr_eye_t *current_eye = NULL;
static vec3_t lastOrientation = {0, 0, 0};
static vec3_t lastAim = {0, 0, 0};

static qboolean vr_initialized = false;
static ovrGLTexture *mirror_texture;
static GLuint mirror_fbo = 0;

static const float meters_to_units = 32.0f;


extern cvar_t gl_farclip;
extern int glwidth, glheight;
extern void SCR_UpdateScreenContent();
extern refdef_t r_refdef;

cvar_t  vr_enabled = {"vr_enabled", "0", CVAR_NONE};
cvar_t  vr_crosshair = {"vr_crosshair","1", CVAR_ARCHIVE};
cvar_t  vr_crosshair_depth = {"vr_crosshair_depth","0", CVAR_ARCHIVE};
cvar_t  vr_crosshair_size = {"vr_crosshair_size","3.0", CVAR_ARCHIVE};
cvar_t  vr_aimmode = {"vr_aimmode","1", CVAR_ARCHIVE};
cvar_t  vr_deadzone = {"vr_deadzone","30",CVAR_ARCHIVE};


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

	ovrHmd_CreateSwapTextureSetGL(hmd, GL_RGBA, width, height, &fbo.color_textures);
	for( i = 0; i < fbo.color_textures->TextureCount; ++i ) {
		ovrGLTexture* tex = (ovrGLTexture*)&fbo.color_textures->Textures[i];

		glBindTexture(GL_TEXTURE_2D, tex->OGL.TexId);

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
	ovrHmd_DestroySwapTextureSet(hmd, fbo.color_textures);
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
	Cvar_RegisterVariable (&vr_aimmode);
	Cvar_RegisterVariable (&vr_deadzone);
	Cvar_SetCallback (&vr_deadzone, VR_Deadzone_f);
}

qboolean VR_Enable()
{
	int i;
	if( ovr_Initialize(NULL) != ovrSuccess ) {
		Con_Printf("Failed to Initialize Oculus SDK");
		return false;
	}

	if( ovrHmd_Create(0, &hmd) != ovrSuccess ) {
		Con_Printf("Failed to get HMD");
		return false;
	}

	if( !InitOpenGLExtensions() ) {
		Con_Printf("Failed to initialize OpenGL extensions");
		return false;
	}

	ovrHmd_CreateMirrorTextureGL(hmd, GL_RGBA, glwidth, glheight, (ovrTexture**)&mirror_texture);
	glGenFramebuffersEXT(1, &mirror_fbo);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, mirror_fbo);
	glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mirror_texture->OGL.TexId, 0);
	glFramebufferRenderbufferEXT(GL_READ_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);

	for( i = 0; i < 2; i++ ) {
		ovrSizei size = ovrHmd_GetFovTextureSize(hmd, (ovrEyeType)i, hmd->DefaultEyeFov[i], 1);

		eyes[i].index = i;
		eyes[i].fbo = CreateFBO(size.w, size.h);
		eyes[i].render_desc = ovrHmd_GetRenderDesc(hmd, (ovrEyeType)i, hmd->DefaultEyeFov[i]);
		eyes[i].fov_x = (atan(hmd->DefaultEyeFov[i].LeftTan) + atan(hmd->DefaultEyeFov[i].RightTan)) / M_PI_DIV_180;
		eyes[i].fov_y = (atan(hmd->DefaultEyeFov[i].UpTan) + atan(hmd->DefaultEyeFov[i].DownTan)) / M_PI_DIV_180;
	}

	ovrHmd_SetEnabledCaps(hmd, ovrHmdCap_LowPersistence|ovrHmdCap_DynamicPrediction);
	ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation|ovrTrackingCap_MagYawCorrection|ovrTrackingCap_Position, 0);
	
	wglSwapIntervalEXT(0); // Disable V-Sync

	vr_initialized = true;
	return true;
}

void VR_Disable()
{
	int i;
	if( !vr_initialized )
		return;
	
	for( i = 0; i < 2; i++ ) {
		DeleteFBO(eyes[i].fbo);
	}
	ovrHmd_DestroyMirrorTexture(hmd, (ovrTexture*)mirror_texture);
	ovrHmd_Destroy(hmd);
	ovr_Shutdown();
	vr_initialized = false;
}

static void RenderScreenForCurrentEye()
{
	ovrGLTexture *current_texture;

	// Remember the current glwidht/height; we have to modify it here for each eye
	int oldglheight = glheight;
	int oldglwidth = glwidth;

	glwidth = current_eye->fbo.size.width;
	glheight = current_eye->fbo.size.height;

	// Set up current FBO
	current_eye->fbo.color_textures->CurrentIndex = (current_eye->fbo.color_textures->CurrentIndex + 1) % current_eye->fbo.color_textures->TextureCount;
	current_texture = (ovrGLTexture*)&current_eye->fbo.color_textures->Textures[current_eye->fbo.color_textures->CurrentIndex];

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, current_eye->fbo.framebuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, current_texture->OGL.TexId, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, current_eye->fbo.depth_texture, 0);

	glViewport(0, 0, current_eye->fbo.size.width, current_eye->fbo.size.height);
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	// Draw everything
	srand((int) (cl.time * 1000)); //sync random stuff between eyes

	r_refdef.fov_x = current_eye->fov_x;
	r_refdef.fov_y = current_eye->fov_y;

	SCR_UpdateScreenContent ();

	
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

	ovrFrameTiming ftiming;
	ovrTrackingState hmdState;

	ovrViewScaleDesc viewScaleDesc;
	ovrLayerEyeFov ld;
	ovrLayerHeader* layers;
	
	GLint w = mirror_texture->OGL.Header.TextureSize.w;
	GLint h = mirror_texture->OGL.Header.TextureSize.h;
	

	// Get current orientation of the HMD
	ftiming = ovrHmd_GetFrameTiming(hmd, 0);
	hmdState = ovrHmd_GetTrackingState(hmd, ftiming.DisplayMidpointSeconds);


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
				cl.aimangles[PITCH] += diffHMDPitch;
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
	view_offset[0] = eyes[0].render_desc.HmdToEyeViewOffset;
	view_offset[1] = eyes[1].render_desc.HmdToEyeViewOffset;

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
	viewScaleDesc.HmdToEyeViewOffset[0] = view_offset[0];
	viewScaleDesc.HmdToEyeViewOffset[1] = view_offset[1];

	ld.Header.Type = ovrLayerType_EyeFov;
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

	for( i = 0; i < 2; i++ ) {
		ld.ColorTexture[i] = eyes[i].fbo.color_textures;
		ld.Viewport[i].Pos.x = 0;
		ld.Viewport[i].Pos.y = 0;
		ld.Viewport[i].Size.w = eyes[i].fbo.size.width;
		ld.Viewport[i].Size.h = eyes[i].fbo.size.height;
		ld.Fov[i] = hmd->DefaultEyeFov[i];
		ld.RenderPose[i] = eyes[i].pose;
	}

	layers = &ld.Header;
	ovrHmd_SubmitFrame(hmd, 0, &viewScaleDesc, &layers, 1);

	// Blit mirror texture to back buffer
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, mirror_fbo);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
	glBlitFramebufferEXT(0, h, w, 0, 0, 0, w, h,GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
}

void VR_SetMatrices() {
	vec3_t temp, orientation, position;
	ovrMatrix4f projection;

	// Calculat HMD projection matrix and view offset position
	projection = TransposeMatrix(ovrMatrix4f_Projection(hmd->DefaultEyeFov[current_eye->index], 4, gl_farclip.value, ovrProjection_RightHanded));
	
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
	float size;
	if( (sv_player && (int)(sv_player->v.weapon) == IT_AXE) )
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

	size = CLAMP (1.0, vr_crosshair_size.value, 5.0);

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
			glColor4f (1, 0, 0, 0.5);
			glPointSize( size * glwidth / 1280.0f );

			glBegin(GL_POINTS);
			glVertex3f (impact[0], impact[1], impact[2]);
			glEnd();
			glDisable(GL_POINT_SMOOTH);
			break;

		case VR_CROSSHAIR_LINE:
			// trace to first entity
			VectorMA (start, 4096, forward, end);
			TraceLineToEntity (start, end, impact, sv_player);

			glColor4f (1, 0, 0, 0.4);
			glLineWidth( size * glwidth / (1280.0f) );
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

void VR_Sbar_Draw()
{	
	vec3_t sbar_angles, forward, right, up, target;
	float scale_hud = 0.03;

	glPushMatrix();
	glDisable (GL_DEPTH_TEST); // prevents drawing sprites on sprites from interferring with one another

	VectorCopy(cl.aimangles, sbar_angles)

	if (vr_aimmode.value == VR_AIMMODE_HEAD_MYAW || vr_aimmode.value == VR_AIMMODE_HEAD_MYAW_MPITCH)
		sbar_angles[PITCH] = 0;

	AngleVectors (sbar_angles, forward, right, up);

	VectorMA (cl.viewent.origin, -0.7, forward, target);

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
		ovrHmd_RecenterPose(hmd);
		VectorCopy(cl.aimangles,lastAim);
	}
}
