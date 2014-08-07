// 2013 Dominic Szablewski - phoboslab.org
// 2014 Jeremiah Sypult - github.com/jeremiah-sypult

#include "quakedef.h"
#include "vr.h"
#include "vr_ovr.h"

typedef struct
{
	GLhandleARB program;
	GLhandleARB vert_shader;
	GLhandleARB frag_shader;
	const char *vert_source;
	const char *frag_source;
} shader_t;

typedef struct
{
	GLuint framebuffer;
	GLuint texture;
	GLuint renderbuffer;
} fbo_t;

typedef struct {
	GLfloat left;
	GLfloat top;
	GLfloat width;
	GLfloat height;
} viewport_t;

typedef struct
{
	eye_t       index;
	vec3_t      view_adjust;
	vec3_t      view_offset;
	viewport_t  viewport;
	fbo_t       fbo;
} vr_eye_t;

typedef struct
{
	vr_library_t    *lib;
	vr_eye_t        *rendering_eye;
	vr_eye_t        eye[EYE_ALL];
	float           viewport_fov_x;
	float           viewport_fov_y;
	GLboolean		gl_extensions_initialized;
	vec3_t          lastAim;
	vec3_t          lastOrientation;
	vec3_t          lastPosition;
} vr_t;

// OpenGL Extensions
static PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
static PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
static PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
static PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
static PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
static PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
static PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
static PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
static PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
static PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
static PFNGLUNIFORM2FARBPROC glUniform2fARB;
static PFNGLUNIFORM4FARBPROC glUniform4fARB;
static PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
static PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
static PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
static PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
static PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
static PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
static PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
static PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
static PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
static PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;

struct
{
	void *func; char *name;
} gl_extensions[] = {
	{&glAttachObjectARB, "glAttachObjectARB"},
	{&glCompileShaderARB, "glCompileShaderARB"},
	{&glCreateProgramObjectARB, "glCreateProgramObjectARB"},
	{&glCreateShaderObjectARB, "glCreateShaderObjectARB"},
	{&glDeleteObjectARB, "glDeleteObjectARB"},
	{&glGetInfoLogARB, "glGetInfoLogARB"},
	{&glGetObjectParameterivARB, "glGetObjectParameterivARB"},
	{&glGetUniformLocationARB, "glGetUniformLocationARB"},
	{&glLinkProgramARB, "glLinkProgramARB"},
	{&glShaderSourceARB, "glShaderSourceARB"},
	{&glUniform2fARB, "glUniform2fARB"},
	{&glUniform4fARB, "glUniform4fARB"},
	{&glUseProgramObjectARB, "glUseProgramObjectARB"},
	{&glBindRenderbufferEXT, "glBindRenderbufferEXT"},
	{&glDeleteRenderbuffersEXT, "glDeleteRenderbuffersEXT"},
	{&glGenRenderbuffersEXT, "glGenRenderbuffersEXT"},
	{&glRenderbufferStorageEXT, "glRenderbufferStorageEXT"},
	{&glBindFramebufferEXT, "glBindFramebufferEXT"},
	{&glDeleteFramebuffersEXT, "glDeleteFramebuffersEXT"},
	{&glGenFramebuffersEXT, "glGenFramebuffersEXT"},
	{&glFramebufferTexture2DEXT, "glFramebufferTexture2DEXT"},
	{&glFramebufferRenderbufferEXT, "glFramebufferRenderbufferEXT"},
	{NULL, NULL},
};

extern int glx, gly, glwidth, glheight;
extern void SCR_UpdateScreenContent();
extern void SCR_CalcRefdef();
extern void R_RenderScene(void);
extern refdef_t r_refdef;
extern vec3_t vright;


// all VR-related globals are within vr_t
static vr_t _vr = {0};


cvar_t  vr_enabled = {"vr_enabled", "0", CVAR_NONE};
cvar_t  vr_debug = {"vr_debug", "1", CVAR_ARCHIVE};
cvar_t  vr_ipd = {"vr_ipd","60", CVAR_ARCHIVE};

// HMD settings
cvar_t  vr_multisample = {"vr_multisample", "1", CVAR_ARCHIVE};
cvar_t  vr_lowpersistence = {"vr_lowpersistence", "0", CVAR_ARCHIVE};
cvar_t  vr_latencytest = {"vr_latencytest", "0", CVAR_ARCHIVE};
cvar_t  vr_dynamicprediction = {"vr_dynamicprediction", "0", CVAR_ARCHIVE};
cvar_t  vr_vsync = {"vr_vsync", "0", CVAR_ARCHIVE};

// distortion settings
cvar_t  vr_chromatic = {"vr_chromatic","1", CVAR_ARCHIVE};
cvar_t  vr_timewarp = {"vr_timewarp","0", CVAR_ARCHIVE};
cvar_t  vr_vignette = {"vr_vignette","1", CVAR_ARCHIVE};

// gameplay settings
cvar_t  vr_crosshair = {"vr_crosshair","1", CVAR_ARCHIVE};
cvar_t  vr_crosshair_depth = {"vr_crosshair_depth","0", CVAR_ARCHIVE};
cvar_t  vr_crosshair_size = {"vr_crosshair_size","3.0", CVAR_ARCHIVE};
cvar_t  vr_crosshair_alpha = {"vr_crosshair_alpha","0.25", CVAR_ARCHIVE};
cvar_t  vr_aimmode = {"vr_aimmode","5", CVAR_ARCHIVE};
cvar_t  vr_deadzone = {"vr_deadzone","30", CVAR_ARCHIVE};


// Wolfenstein 3d, DOOM and QUAKE use the same coordinate/unit system:
// 8 foot (96 inch) height wall == 64 pixel units
// 1.5 inches per pixel unit
// 1.0 pixel unit / 1.5 inch == 0.666666 pixel units per inch
// QuakeEd shows the *eye* height to be ~46 units high
// 46 units * 1.5 inch ratio == 69 inches / 12 inches == 5 foot 9 inch *eye level*

#define INCH_TO_METER (0.0254f)
#define QUAKE_TO_METER ((1.0f/1.5f) * INCH_TO_METER)
#define METER_TO_QUAKE (1.0f/QUAKE_TO_METER)

// ----------------------------------------------------------------------------
// inline utilities

static inline float QuakeToMeter(float q) { return q * QUAKE_TO_METER; }
static inline float MeterToQuake(float m) { return m * METER_TO_QUAKE; }

static inline float HMDViewOffset(float mm) { return MeterToQuake( mm * 0.001 ) * 0.5; }

static inline viewport_t HMDViewportHorizontal(eye_t eye)
{
	viewport_t viewport = {
		eye == EYE_LEFT ? 0 : 0.5, 0,
		0.5, 1
	};

	return viewport;
}

static inline viewport_t HMDViewportVertical(eye_t eye)
{
	viewport_t viewport = {
		eye == EYE_LEFT ? 0.5 : 0, 0,
		1, 0.5
	};

	return viewport;
}


// ----------------------------------------------------------------------------
// OpenGL utilities for extension function pointers, shaders and FBOs

static GLboolean InitGLExtensions()
{
	static GLboolean initialized;
	GLint i;

	if ( initialized ) {
		return true;
	}

	for( i = 0; gl_extensions[i].func; i++ ) {
		void *func = SDL_GL_GetProcAddress( gl_extensions[i].name );

		if ( ! func ) {
			return GL_FALSE;
		}

		*((void **)gl_extensions[i].func) = func;
	}

	initialized = GL_TRUE;

	return initialized;
}

static qboolean CompileShader(GLhandleARB shader, const char *source)
{
	GLint status;

	glShaderSourceARB( shader, 1, &source, NULL );
	glCompileShaderARB( shader );
	glGetObjectParameterivARB( shader, GL_OBJECT_COMPILE_STATUS_ARB, &status );

	if ( status == 0 ) {
		GLint length;
		char *info;

		glGetObjectParameterivARB( shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length );
		info = SDL_stack_alloc( char, length+1 );
		glGetInfoLogARB( shader, length, NULL, info );
		Con_Warning( "Failed to compile shader:\n%s\n%s", source, info );
		SDL_stack_free( info );
	}

	return !!status;
}

static qboolean CompileShaderProgram(shader_t *shader)
{
	glGetError();

	if ( shader )
	{
		shader->program = glCreateProgramObjectARB();
		shader->vert_shader = glCreateShaderObjectARB( GL_VERTEX_SHADER_ARB );

		if ( ! CompileShader( shader->vert_shader, shader->vert_source ) ) {
			return false;
		}

		shader->frag_shader = glCreateShaderObjectARB( GL_FRAGMENT_SHADER_ARB );

		if ( ! CompileShader( shader->frag_shader, shader->frag_source ) ) {
			return false;
		}

		glAttachObjectARB( shader->program, shader->vert_shader );
		glAttachObjectARB( shader->program, shader->frag_shader );
		glLinkProgramARB( shader->program );
	}

	return ( glGetError() == GL_NO_ERROR );
}

static void DestroyShaderProgram(shader_t *shader)
{
	if ( _vr.gl_extensions_initialized && shader ) {
		glDeleteObjectARB( shader->vert_shader );
		glDeleteObjectARB( shader->frag_shader );
		glDeleteObjectARB( shader->program );
	}
}

static fbo_t CreateFBO(GLsizei width, GLsizei height)
{
	fbo_t fbo;
	GLuint framebuffer, texture, renderbuffer;

	glGenFramebuffersEXT( 1, &framebuffer );
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, framebuffer );

	glGenRenderbuffersEXT( 1, &renderbuffer );
	glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, renderbuffer );
	glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, width, height );

	glGenTextures( 1, &texture );
	glBindTexture( GL_TEXTURE_2D, texture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

	glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0 );
	glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, renderbuffer );
	glBindTexture( GL_TEXTURE_2D, 0 );
	
	fbo.framebuffer = framebuffer;
	fbo.texture = texture;
	fbo.renderbuffer = renderbuffer;

	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

	return fbo;
}

static void DeleteFBO(fbo_t fbo)
{
	if ( fbo.framebuffer ) {
		glDeleteFramebuffersEXT( 1, &fbo.framebuffer );
		fbo.framebuffer = 0;
	}

	if ( fbo.texture ) {
		glDeleteTextures( 1, &fbo.texture );
		fbo.texture = 0;
	}

	if ( fbo.renderbuffer ) {
		glDeleteRenderbuffersEXT( 1, &fbo.renderbuffer );
		fbo.renderbuffer = 0;
	}
}


// ----------------------------------------------------------------------------
// Callbacks for cvars

static void VR_Enabled_f (cvar_t *var)
{
	VR_Disable();

	if ( var->value != 0.0f ) {
		VR_Enable();
	}
}

static void VR_IPD_f (cvar_t *var)
{
	float view_offset = HMDViewOffset( vr_ipd.value );

	if ( ! _vr.lib ) { return; }

	//Con_Printf( "IPD set to %.1fmm (VR profile default: %.1fmm)\n", vr_ipd.value, _vr.lib->GetIPD() );
	_vr.eye[EYE_LEFT].view_offset[0] = -view_offset;
	_vr.eye[EYE_RIGHT].view_offset[0] = view_offset;
}

static void VR_Deadzone_f (cvar_t *var)
{
	// clamp the deadzone to a max of 0 - 180 degrees.
	// value of 180 will totally separate view/aim angles in VR_UpdatePlayerPose
	float deadzone = CLAMP( 0.0f, vr_deadzone.value, 180.0f );

	if ( deadzone != vr_deadzone.value ) {
		Cvar_SetValueQuick( &vr_deadzone, deadzone );
	}
}

static void VR_RendererReinit_f (cvar_t *var)
{
	VR_RendererInit();
}


// ----------------------------------------------------------------------------
// Public vars and functions

void VR_Init()
{
	// VR_Init is only called once at game start

	// HMD settings
	Cvar_RegisterVariable( &vr_enabled );
	Cvar_SetCallback( &vr_enabled, VR_Enabled_f );
	Cvar_RegisterVariable( &vr_debug );
	Cvar_RegisterVariable( &vr_ipd );
	Cvar_SetCallback( &vr_ipd, VR_IPD_f );

	Cvar_RegisterVariable( &vr_multisample );
	Cvar_SetCallback( &vr_multisample, VR_RendererReinit_f );
	Cvar_RegisterVariable( &vr_lowpersistence );
	Cvar_SetCallback( &vr_lowpersistence, VR_RendererReinit_f );
	Cvar_RegisterVariable( &vr_latencytest );
	Cvar_SetCallback( &vr_latencytest, VR_RendererReinit_f );
	Cvar_RegisterVariable( &vr_dynamicprediction );
	Cvar_SetCallback( &vr_dynamicprediction, VR_RendererReinit_f );
	Cvar_RegisterVariable( &vr_vsync );
	Cvar_SetCallback( &vr_vsync, VR_RendererReinit_f );

	// distortion settings
	Cvar_RegisterVariable( &vr_chromatic );
	Cvar_SetCallback( &vr_chromatic, VR_RendererReinit_f );
	Cvar_RegisterVariable( &vr_timewarp );
	Cvar_SetCallback( &vr_timewarp, VR_RendererReinit_f );
	Cvar_RegisterVariable( &vr_vignette );
	Cvar_SetCallback( &vr_vignette, VR_RendererReinit_f );

	// gameplay settings
	Cvar_RegisterVariable( &vr_crosshair );
	Cvar_RegisterVariable( &vr_crosshair_depth );
	Cvar_RegisterVariable( &vr_crosshair_size );
	Cvar_RegisterVariable( &vr_crosshair_alpha );
	Cvar_RegisterVariable( &vr_aimmode );
	Cvar_RegisterVariable( &vr_deadzone );
	Cvar_SetCallback( &vr_deadzone, VR_Deadzone_f );
}

static void VR_EyeInit(eye_t eye, vr_eye_t *vrEye, qboolean isHMDHorizontal)
{
	GLfloat multisample = vr_multisample.value < 1 ? 1 : vr_multisample.value;
	GLsizei width;
	GLsizei height;

	// initialize eyes
	vrEye->index = eye;
	vrEye->view_adjust[0] = vrEye->view_adjust[1] = vrEye->view_adjust[2] = 0;
	vrEye->view_offset[0] = vrEye->view_offset[1] = vrEye->view_offset[2] = 0;
	vrEye->viewport = isHMDHorizontal ? HMDViewportHorizontal( eye ) : HMDViewportVertical( eye );

	// calculate size with multisampling multiplier
	width = glwidth * vrEye->viewport.width * multisample;
	height = glheight * vrEye->viewport.height * multisample;

	if ( vrEye->fbo.framebuffer && vrEye->fbo.renderbuffer && vrEye->fbo.texture ) {
		DeleteFBO( vrEye->fbo );
	}

	vrEye->fbo = CreateFBO( width, height );

	_vr.lib->ConfigureEye( vrEye->index,
						   vrEye->viewport.left, vrEye->viewport.top,
						   width, height,
						   vrEye->fbo.texture );
}

void VR_RendererInit()
{
	qboolean isHMDHorizontal = true; // TODO: jeremiah sypult, support vertically-rendered HMDs

	if ( ! _vr.lib ) { return; }

	if ( ! _vr.gl_extensions_initialized ) {
		_vr.gl_extensions_initialized = InitGLExtensions();
	}

	if ( ! _vr.gl_extensions_initialized ) {
		Con_Printf( "VR_RendererInit: failed to initialize OpenGL Extensions" );
		return;
	}

	// initialize eyes
	VR_EyeInit( EYE_LEFT, &_vr.eye[EYE_LEFT], isHMDHorizontal );
	VR_EyeInit( EYE_RIGHT, &_vr.eye[EYE_RIGHT], isHMDHorizontal );

	// configure the renderer
	// calculates the FOV and view adjustments that are gotten below
	_vr.lib->ConfigureRenderer( vr_multisample.value,
							    vr_lowpersistence.value,
							    vr_latencytest.value,
							    vr_dynamicprediction.value,
							    vr_vsync.value,
							    vr_chromatic.value,
							    vr_timewarp.value,
							    vr_vignette.value );

	// set the viewport FOV in degrees **** calculated after ConfigureRenderer()
	_vr.lib->GetFOV( &_vr.viewport_fov_x, &_vr.viewport_fov_y );

	// set the view adjust for each eye **** calculated after ConfigureRenderer()
	_vr.lib->GetViewAdjustForEye( EYE_LEFT, _vr.eye[EYE_LEFT].view_adjust );
	_vr.lib->GetViewAdjustForEye( EYE_RIGHT, _vr.eye[EYE_RIGHT].view_adjust );

	// set the view offset for each eye, derived from the interpupillary distance
	// convert the IPD from millimeters to QUAKE units
	_vr.eye[EYE_LEFT].view_offset[0] = -HMDViewOffset( vr_ipd.value );
	_vr.eye[EYE_RIGHT].view_offset[0] = HMDViewOffset( vr_ipd.value );

	// TODO: jeremiah sypult - HMD doesn't render on OS X unless this is called
	glUseProgramObjectARB( 0 );

	vid.recalc_refdef = true;
}

static void VR_RendererShutdown()
{
	DeleteFBO( _vr.eye[EYE_LEFT].fbo );
	DeleteFBO( _vr.eye[EYE_RIGHT].fbo );

	vid.recalc_refdef = true;
}

qboolean VR_Enable()
{
	qboolean vr_initialized = false;

	_vr.lib = &OVRLibrary;

	vr_initialized = _vr.lib->Initialize();

	if ( ! vr_initialized ) {
		Cvar_SetValueQuick( &vr_enabled, 0 );
		Con_Printf( "VR_Enable: failed to initialize" );
		return false;
	}

	VR_RendererInit();

	return true;
}

void VR_Disable()
{
	VR_RendererShutdown();

	if ( _vr.lib ) {
		_vr.lib->Shutdown();
		_vr.lib = NULL;
	}
}

static void VR_UpdatePlayerPose()
{
	vec3_t orientation = {0};

	// get current orientation of the HMD
	// TODO: position translation, ala Oculus Rift DK2 HMD
	_vr.lib->GetViewAngles( orientation );

	switch ( (int)vr_aimmode.value ) {
		// 1: Head Aiming; View YAW is mouse+head, PITCH is head
		case VR_AIMMODE_HEAD_MYAW:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] = orientation[PITCH];
			cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - _vr.lastOrientation[YAW];
			break;

		// 2: Head Aiming; View YAW and PITCH is mouse+head
		case VR_AIMMODE_HEAD_MYAW_MPITCH:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH] - _vr.lastOrientation[PITCH];
			cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - _vr.lastOrientation[YAW];
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

		// 5: (Default) Blended
		default:
		case VR_AIMMODE_BLENDED:
		case VR_AIMMODE_BLENDED_NOPITCH: {
			float diffHMDYaw = orientation[YAW] - _vr.lastOrientation[YAW];
			float diffHMDPitch = orientation[PITCH] - _vr.lastOrientation[PITCH];
			float diffAimYaw = cl.aimangles[YAW] - _vr.lastAim[YAW];
			//float diffAimPitch = cl.aimangles[PITCH] - lastAim[PITCH];
			float diffYaw, diffPitch;

			cl.viewangles[PITCH] = orientation[PITCH];

			// find new view position based on orientation delta
			cl.viewangles[YAW] += diffHMDYaw;

			// if deadzone is >= 180, totally separate gun aiming frim the view
			// this also disables the aiming from following the HMD pitch (up/down)
			if ( vr_deadzone.value < 180 ) {
				// find difference between view and aim yaw
				diffYaw = cl.viewangles[YAW] - cl.aimangles[YAW];
				diffPitch = cl.viewangles[PITCH] - cl.aimangles[PITCH];

				if ( abs( diffYaw ) > vr_deadzone.value * 0.5f ) {
					// apply the difference from each set of angles to the other
					cl.aimangles[YAW] += diffHMDYaw;
					cl.viewangles[YAW] += diffAimYaw;
				}
#if 0
				// TODO: aim pitch uses deadzone within the deadzone radius?
				if ( abs( diffPitch ) > vr_deadzone.value * 0.5f ) {
					cl.aimangles[PITCH] += diffHMDPitch;
				}
#endif
				if ( (int)vr_aimmode.value == VR_AIMMODE_BLENDED ) {
					cl.aimangles[PITCH] += diffHMDPitch;
				}
			}

			break;
		}
	}

	cl.viewangles[ROLL]  = orientation[ROLL];

	VectorCopy( orientation,_vr.lastOrientation );
	VectorCopy( cl.aimangles,_vr.lastAim );

	VectorCopy( cl.viewangles, r_refdef.viewangles );
	VectorCopy( cl.aimangles, r_refdef.aimangles );
}

void VR_SetFrustum()
{
	// called from R_SetupGL for each eye
	if ( _vr.rendering_eye != NULL ) {
		GLfloat *projection = _vr.lib->GetProjectionForEye( _vr.rendering_eye->index );
		glLoadMatrixf( projection );
	} else {
		Sys_Error( "VR_SetFrustum: NULL VR rendering_eye" );
	}
}

void VR_RenderScene()
{
	// called from R_RenderView for each eye
	if ( _vr.rendering_eye != NULL ) {
		float view_offset_scale = _vr.rendering_eye->view_offset[0];
		VectorMA( r_refdef.vieworg, view_offset_scale, vright, r_refdef.vieworg );
		R_RenderScene();
	} else {
		Sys_Error( "VR_RenderScene: NULL VR rendering_eye" );
	}
}

static void RenderScreenForEye(vr_eye_t *eye)
{
	// Remember the current vrect.width and vieworg; we have to modify it here
	// for each eye
	int oldwidth = r_refdef.vrect.width;
	int oldheight = r_refdef.vrect.height;
	int oldglheight = glheight;
	int oldglwidth = glwidth;
	float multisample = vr_multisample.value < 1 ? 1 : vr_multisample.value;

	r_refdef.vrect.width *= eye->viewport.width * multisample;
	r_refdef.vrect.height *= eye->viewport.height * multisample;
	glwidth *= multisample;
	glheight *= multisample;

	srand( (int)(cl.time * 1000) ); //sync random stuff between eyes

	r_refdef.fov_x = _vr.viewport_fov_x;
	r_refdef.fov_y = _vr.viewport_fov_y;

	// draw everything to the eye framebuffer
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, eye->fbo.framebuffer );
	glClear( GL_DEPTH_BUFFER_BIT );

	_vr.rendering_eye = eye; // for VR_SetFrustum and VR_RenderScene
	_vr.lib->BeginEyeRender( eye->index );
	SCR_UpdateScreenContent();
	_vr.lib->EndEyeRender( eye->index );
	_vr.rendering_eye = NULL;

	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

	// set old variables back
	r_refdef.vrect.width = oldwidth;
	r_refdef.vrect.height = oldheight;

	glwidth = oldglwidth;
	glheight = oldglheight;
}

static void RenderEyeOnScreen(vr_eye_t *eye)
{
	glViewport(
		(glwidth-glx) * eye->viewport.left,
		(glheight-gly) * eye->viewport.top,
		glwidth * eye->viewport.width,
		glheight * eye->viewport.height
	);

	glBindTexture( GL_TEXTURE_2D, eye->fbo.texture );
	
	glBegin( GL_QUADS );
	glTexCoord2f( 0, 0 ); glVertex2f( -1, -1 );
	glTexCoord2f( 1, 0 ); glVertex2f(  1, -1 );
	glTexCoord2f( 1, 1 ); glVertex2f(  1,  1 );
	glTexCoord2f( 0, 1 ); glVertex2f( -1,  1 );
	glEnd();

	glBindTexture( GL_TEXTURE_2D, 0 );
}

void VR_UpdateScreenContent()
{
	// handle VR frame initialization
	_vr.lib->BeginFrame();

	// updates the player pose from the VR HMD sensor(s)
	// (this includes the viewing angles, TODO: position translation)
	VR_UpdatePlayerPose();

	// render each eye to their respective FBO
	RenderScreenForEye( &_vr.eye[EYE_LEFT] );
	RenderScreenForEye( &_vr.eye[EYE_RIGHT] );

	// finalize VR frame, which may result in flushing to the screen
	_vr.lib->EndFrame();
}

void VR_AddOrientationToViewAngles(vec3_t angles)
{
	if ( vr_enabled.value ) {
		vec3_t orientation;

		// Get current orientation of the HMD
		_vr.lib->GetViewAngles( orientation );

		angles[PITCH] = angles[PITCH] + orientation[PITCH];
		angles[YAW] = angles[YAW] + orientation[YAW];
		angles[ROLL] = orientation[ROLL];
	}
}

void VR_ShowCrosshair()
{
	vec3_t forward, up, right;
	vec3_t start, end, impact;
	float crosshair_size = CLAMP( 0.0f, vr_crosshair_size.value, 32.0f );
	float crosshair_alpha = CLAMP( 0.0f, vr_crosshair_alpha.value, 1.0f );

	if( sv_player && (int)(sv_player->v.weapon) == IT_AXE )
		return;

	if ( ( vr_crosshair_size.value == 0.0f ) || ( vr_crosshair_alpha.value == 0.0f ) ) {
		return;
	}

	// setup gl
	glDisable( GL_DEPTH_TEST );
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	GL_PolygonOffset( OFFSET_SHOWTRIS );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDisable( GL_TEXTURE_2D );
	glDisable( GL_CULL_FACE );

	// calc the line and draw
	VectorCopy( cl.viewent.origin, start );
	start[2] -= cl.viewheight - 10;
	AngleVectors( cl.aimangles, forward, right, up );

	switch ( (int)vr_crosshair.value ) {
	// point crosshair
	default:
	case VR_CROSSHAIR_POINT:
		if ( vr_crosshair_depth.value <= 0 ) {
			 // trace to first wall
			VectorMA( start, 4096, forward, end );
			TraceLine( start, end, impact );
		} else {
			// fix crosshair to specific depth
			VectorMA( start, vr_crosshair_depth.value, forward, impact );
		}

		glEnable( GL_POINT_SMOOTH );
		glColor4f( 1, 0, 0, crosshair_alpha );
		glPointSize( crosshair_size * glwidth / vid.width );

		glBegin( GL_POINTS );
		glVertex3f( impact[0], impact[1], impact[2] );
		glEnd();
		glDisable( GL_POINT_SMOOTH );
		break;

	// laser crosshair
	case VR_CROSSHAIR_LINE:

		// trace to first entity or depth
		VectorMA( start, vr_crosshair_depth.value > 0 ? vr_crosshair_depth.value : 4096, forward, end );
		TraceLineToEntity( start, end, impact, sv_player );

		glColor4f( 1, 0, 0, crosshair_alpha );
		glLineWidth( crosshair_size * glwidth / vid.width );
		glBegin( GL_LINES );
		glVertex3f( start[0], start[1], start[2] );
		glVertex3f( impact[0], impact[1], impact[2] );
		glEnd();
		break;
	}
	// cleanup gl
	glColor3f( 1,1,1 );
	glEnable( GL_TEXTURE_2D );
	glEnable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	GL_PolygonOffset( OFFSET_NONE );
	glEnable( GL_DEPTH_TEST );
}

void VR_Sbar_Draw()
{	
	vec3_t sbar_angles, forward, right, up, target;
	float scale_hud = 0.04;

	glPushMatrix();
	glDisable( GL_DEPTH_TEST ); // prevents drawing sprites on sprites from interferring with one another

	VectorCopy( cl.aimangles, sbar_angles )

	if ( vr_aimmode.value == VR_AIMMODE_HEAD_MYAW || vr_aimmode.value == VR_AIMMODE_HEAD_MYAW_MPITCH )
		sbar_angles[PITCH] = 0;

	AngleVectors( sbar_angles, forward, right, up );

	VectorMA( cl.viewent.origin, -0.7, forward, target );

	glTranslatef( target[0],  target[1],  target[2] );
	glRotatef( sbar_angles[YAW] - 90, 0, 0, 1 ); // rotate around z
	glRotatef( 90 + 45 + sbar_angles[PITCH], -1, 0, 0 ); // keep bar at constant angled pitch towards user
	glTranslatef( -(320.0 * scale_hud / 2), 0, 0 ); // center the status bar
	glTranslatef( 0,  0,  10 ); // move hud down a bit
	glScalef( scale_hud, scale_hud, scale_hud );

	Sbar_Draw();

	glEnable( GL_DEPTH_TEST );
	glPopMatrix();
}

void VR_SetAngles(vec3_t angles)
{
	VectorCopy( angles, cl.aimangles );
	VectorCopy( angles, cl.viewangles );
	VectorCopy( angles, _vr.lastAim );
}

void VR_ResetOrientation()
{
	cl.aimangles[YAW] = cl.viewangles[YAW];	
	cl.aimangles[PITCH] = cl.viewangles[PITCH];
//	cl.aimangles[ROLL] = 0;

	if ( vr_enabled.value ) {
		_vr.lib->ResetSensor();
		_vr.lib->GetViewAngles( _vr.lastOrientation );
		VectorCopy( cl.aimangles, _vr.lastAim );
	}
}
