// 2013 Dominic Szablewski - phoboslab.org

#include "quakedef.h"
#include "vr.h"
#include "vr_ovr.h"

typedef struct {
    GLhandleARB program;
    GLhandleARB vert_shader;
    GLhandleARB frag_shader;
    const char *vert_source;
    const char *frag_source;
} shader_t;


typedef struct {
	GLuint framebuffer, texture, renderbuffer;
} fbo_t;

typedef struct {
	float offset;
	float lens_shift;
	struct {
		float left, top, width, height;
	} viewport;
	fbo_t fbo;
	float projection_matrix[16];
} hmd_eye_t;

// GL Extensions
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


// Default Lens Warp Shader
static shader_t lens_warp_shader_norm = {
	0, 0, 0,
	
	// vertex shader (identity)
	"varying vec2 vUv;\n"
	"void main(void) {\n"
		"gl_Position = gl_Vertex;\n"
		"vUv = vec2(gl_MultiTexCoord0);\n"
	"}\n",

	// fragment shader
	"varying vec2 vUv;\n"
	"uniform vec2 scale;\n"
	"uniform vec2 scaleIn;\n"
	"uniform vec2 lensCenter;\n"
	"uniform vec4 hmdWarpParam;\n"
	"uniform sampler2D texture;\n"
	"void main()\n"
	"{\n"
		"vec2 uv = (vUv*2.0)-1.0;\n" // range from [0,1] to [-1,1]
		"vec2 theta = (uv-lensCenter)*scaleIn;\n"
		"float rSq = theta.x*theta.x + theta.y*theta.y;\n"
		"vec2 rvector = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"
		"vec2 tc = (lensCenter + scale * rvector);\n"
		"tc = (tc+1.0)/2.0;\n" // range from [-1,1] to [0,1]
		"if (any(bvec2(clamp(tc, vec2(0.0,0.0), vec2(1.0,1.0))-tc)))\n"
			"gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"else\n"
			"gl_FragColor = texture2D(texture, tc);\n"
	"}\n"
};

// Lens Warp Shader with Chromatic Aberration 
static shader_t lens_warp_shader_chrm = {
	0, 0, 0,
	
	// vertex shader (identity)
	"varying vec2 vUv;\n"
	"void main(void) {\n"
		"gl_Position = gl_Vertex;\n"
		"vUv = vec2(gl_MultiTexCoord0);\n"
	"}\n",

	// fragment shader
	"varying vec2 vUv;\n"
    "uniform vec2 lensCenter;\n"
    "uniform vec2 scale;\n"
    "uniform vec2 scaleIn;\n"
    "uniform vec4 hmdWarpParam;\n"
    "uniform vec4 chromAbParam;\n"
    "uniform sampler2D texture;\n"

    // Scales input texture coordinates for distortion.
    // ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
    // larger due to aspect ratio.
    "void main()\n"
    "{\n"
		"vec2 uv = (vUv*2.0)-1.0;\n" // range from [0,1] to [-1,1]
		"vec2 theta = (uv-lensCenter)*scaleIn;\n"
		"float rSq = theta.x*theta.x + theta.y*theta.y;\n"
		"vec2 theta1 = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"

		// Detect whether blue texture coordinates are out of range since these will scaled out the furthest.
		"vec2 thetaBlue = theta1 * (chromAbParam.z + chromAbParam.w * rSq);\n"
		"vec2 tcBlue = lensCenter + scale * thetaBlue;\n"
		"tcBlue = (tcBlue+1.0)/2.0;\n" // range from [-1,1] to [0,1]
		"if (any(bvec2(clamp(tcBlue, vec2(0.0,0.0), vec2(1.0,1.0))-tcBlue)))\n"
		"{\n"
			"gl_FragColor = vec4(0.0,0.0,0.0,1.0);\n"
			"return;\n"
		"}\n"

		// Now do blue texture lookup.
		"float blue = texture2D(texture, tcBlue).b;\n"

		// Do green lookup (no scaling).
		"vec2  tcGreen = lensCenter + scale * theta1;\n"
		"tcGreen = (tcGreen+1.0)/2.0;\n" // range from [-1,1] to [0,1]
		"vec4  center = texture2D(texture, tcGreen);\n"

		// Do red scale and lookup.
		"vec2  thetaRed = theta1 * (chromAbParam.x + chromAbParam.y * rSq);\n"
		"vec2  tcRed = lensCenter + scale * thetaRed;\n"
		"tcRed = (tcRed+1.0)/2.0;\n" // range from [-1,1] to [0,1]
		"float red = texture2D(texture, tcRed).r;\n"

		"gl_FragColor = vec4(red, center.g, blue, center.a);\n"
    "}\n"
};



// Current lens warp shader and uniform location
static struct {
	shader_t *shader;
	struct {
		GLuint scale;
		GLuint scale_in;
		GLuint lens_center;
		GLuint hmd_warp_param;
		GLuint chrom_ab_param;
	} uniform;
} lens_warp;

static hmd_eye_t left_eye = {0, 0, {0, 0, 0.5, 1}, 0};
static hmd_eye_t right_eye = {0, 0, {0.5, 0, 0.5, 1}, 0};
static float viewport_fov_x;
static float viewport_fov_y;
static float hmd_ipd;

static qboolean rift_enabled;
static qboolean shader_support;

static const float player_height_units = 56;
static const float player_height_m = 1.75;

extern cvar_t gl_farclip;
extern cvar_t r_stereodepth;

extern int glx, gly, glwidth, glheight;
extern void SCR_UpdateScreenContent();
extern void SCR_CalcRefdef();
extern refdef_t r_refdef;
extern vec3_t vright;


cvar_t  vr_enabled = {"vr_enabled", "0", CVAR_NONE};
cvar_t  vr_ipd = {"vr_ipd","0",CVAR_NONE};
cvar_t  vr_supersample = {"vr_supersample", "2", CVAR_ARCHIVE};
cvar_t  vr_prediction = {"vr_prediction","40", CVAR_ARCHIVE};
cvar_t  vr_driftcorrect = {"vr_driftcorrect","1", CVAR_ARCHIVE};
cvar_t  vr_crosshair = {"vr_crosshair","1", CVAR_ARCHIVE};
cvar_t  vr_chromabr = {"vr_chromabr","1", CVAR_ARCHIVE};
cvar_t  vr_aimmode = {"vr_aimmode","1", CVAR_ARCHIVE};
cvar_t  vr_deadzone = {"vr_deadzone","30",CVAR_ARCHIVE};





static qboolean CompileShader(GLhandleARB shader, const char *source)
{
    GLint status;

    glShaderSourceARB(shader, 1, &source, NULL);
    glCompileShaderARB(shader);
    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
    if (status == 0)
	{
        GLint length;
        char *info;

        glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
        info = SDL_stack_alloc(char, length+1);
        glGetInfoLogARB(shader, length, NULL, info);
        Con_Warning("Failed to compile shader:\n%s\n%s", source, info);
        SDL_stack_free(info);
    }

	return !!status;
}

static qboolean CompileShaderProgram(shader_t *shader)
{
	glGetError();
	if (shader)
	{
		shader->program = glCreateProgramObjectARB();

		shader->vert_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		if (!CompileShader(shader->vert_shader, shader->vert_source)) {
			return false;
		}

		shader->frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		if (!CompileShader(shader->frag_shader, shader->frag_source)) {
			return false;
		}

		glAttachObjectARB(shader->program, shader->vert_shader);
		glAttachObjectARB(shader->program, shader->frag_shader);
		glLinkProgramARB(shader->program); 
	}
	return (glGetError() == GL_NO_ERROR);
}

static void DestroyShaderProgram(shader_t *shader)
{
    if (shader_support && shader) 
	{
        glDeleteObjectARB(shader->vert_shader);
        glDeleteObjectARB(shader->frag_shader);
        glDeleteObjectARB(shader->program);
    }
}


static qboolean InitShaderExtension()
{
	static qboolean shader_support_initialized;

	if (shader_support_initialized)
		return true;

	glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) SDL_GL_GetProcAddress("glAttachObjectARB");
	glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) SDL_GL_GetProcAddress("glCompileShaderARB");
	glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) SDL_GL_GetProcAddress("glCreateProgramObjectARB");
	glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) SDL_GL_GetProcAddress("glCreateShaderObjectARB");
	glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC) SDL_GL_GetProcAddress("glDeleteObjectARB");
	glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) SDL_GL_GetProcAddress("glGetInfoLogARB");
	glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) SDL_GL_GetProcAddress("glGetObjectParameterivARB");
	glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) SDL_GL_GetProcAddress("glGetUniformLocationARB");
	glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) SDL_GL_GetProcAddress("glLinkProgramARB");
	glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) SDL_GL_GetProcAddress("glShaderSourceARB");
	glUniform2fARB = (PFNGLUNIFORM2FARBPROC) SDL_GL_GetProcAddress("glUniform2fARB");
	glUniform4fARB = (PFNGLUNIFORM4FARBPROC) SDL_GL_GetProcAddress("glUniform4fARB");
	glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) SDL_GL_GetProcAddress("glUseProgramObjectARB");

	glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC) SDL_GL_GetProcAddress("glBindRenderbufferEXT");
	glDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC) SDL_GL_GetProcAddress("glDeleteRenderbuffersEXT");
	glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC) SDL_GL_GetProcAddress("glGenRenderbuffersEXT");
	glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC) SDL_GL_GetProcAddress("glRenderbufferStorageEXT");
	glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC) SDL_GL_GetProcAddress("glBindFramebufferEXT");
	glDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC) SDL_GL_GetProcAddress("glDeleteFramebuffersEXT");
	glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC) SDL_GL_GetProcAddress("glGenFramebuffersEXT");
	glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
	glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) SDL_GL_GetProcAddress("glFramebufferRenderbufferEXT");

    if (
		glAttachObjectARB &&
        glCompileShaderARB &&
        glCreateProgramObjectARB &&
        glCreateShaderObjectARB &&
        glDeleteObjectARB &&
        glGetInfoLogARB &&
        glGetObjectParameterivARB &&
        glGetUniformLocationARB &&
        glLinkProgramARB &&
        glShaderSourceARB &&
        glUniform2fARB &&
		glUniform4fARB &&
        glUseProgramObjectARB &&
		glBindFramebufferEXT &&
		glBindRenderbufferEXT &&
		glDeleteRenderbuffersEXT &&
		glGenRenderbuffersEXT &&
		glRenderbufferStorageEXT &&
		glBindFramebufferEXT &&
		glDeleteFramebuffersEXT &&
		glGenFramebuffersEXT &&
		glFramebufferTexture2DEXT &&
		glFramebufferRenderbufferEXT
	) {
        shader_support_initialized = true;
    }

	// Ah yes, this was fun.
	return shader_support_initialized;
}

fbo_t CreateFBO(int width, int height) {
	fbo_t ret;
	GLuint fbo, texture, renderbuffer;

	glGenFramebuffersEXT(1, &fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

	glGenRenderbuffersEXT(1, &renderbuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, width, height);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, renderbuffer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	ret.framebuffer = fbo;
	ret.texture = texture;
	ret.renderbuffer = renderbuffer;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	return ret;
}

void DeleteFBO(fbo_t fbo) {
	glDeleteFramebuffersEXT(1, &fbo.framebuffer);
	glDeleteTextures(1, &fbo.texture);
	glDeleteRenderbuffersEXT(1, &fbo.renderbuffer);
}

void CreatePerspectiveMatrix(float *out, float fovy, float aspect, float nearf, float farf, float h) {
	float f = 1.0f / tanf(fovy / 2.0f);
    float nf = 1.0f / (nearf - farf);
    out[0] = f / aspect;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
    out[5] = f;
    out[6] = 0;
    out[7] = 0;
    out[8] = -h;
    out[9] = 0;
    out[10] = (farf + nearf) * nf;
    out[11] = -1;
    out[12] = 0;
    out[13] = 0;
    out[14] = (2.0f * farf * nearf) * nf;
    out[15] = 0;
}





static void VR_Enabled_f (cvar_t *var)
{
	VR_Disable();

	if (!vr_enabled.value) 
		return;

	if( !VR_Enable() )
		vr_enabled.value = 0;
}

static void VR_SuperSample_f (cvar_t *var)
{
	if (!rift_enabled) { return; }

	// Re-init oculus tracker when, if active
	VR_Disable();
	VR_Enable();
}

static void VR_ChromAbr_f (cvar_t *var)
{
	if (!rift_enabled) { return; }

	// Re-init oculus tracker when active
	VR_Disable();
	VR_Enable();
}
static void VR_Prediction_f (cvar_t *var)
{
	if (!rift_enabled) { return; }

	SetOculusPrediction(vr_prediction.value/1000.0f);
	Con_Printf("Motion prediction is set to %.1fms\n",vr_prediction.value);
}

static void VR_DriftCorrect_f (cvar_t *var)
{
	if (!rift_enabled) { return; }

	SetOculusDriftCorrect((int)vr_driftcorrect.value);
}

static void VR_IPD_f (cvar_t *var)
{
	if (!rift_enabled) { return; }

	left_eye.offset = -player_height_units * (vr_ipd.value/(player_height_m * 1000.0)) * 0.5;
	right_eye.offset = -left_eye.offset;
	Con_Printf("Your IPD is set to %.1fmm (SDK default:%.1fmm)\n", vr_ipd.value, hmd_ipd);
}

static void VR_Deadzone_f (cvar_t *var)
{
	// clamp the mouse to a max of 0 - 70 degrees
	
	float value = CLAMP (0.0f, vr_deadzone.value, 70.0f);
	if (value != vr_deadzone.value)
		Cvar_SetValueQuick(&vr_deadzone,value);
}




// ----------------------------------------------------------------------------
// Public vars and functions

float vr_view_offset;
float *vr_projection_matrix = NULL;

void VR_Init()
{
	Cvar_RegisterVariable (&vr_enabled);
	Cvar_SetCallback (&vr_enabled, VR_Enabled_f);
	Cvar_RegisterVariable (&vr_supersample);
	Cvar_SetCallback (&vr_supersample, VR_SuperSample_f);
	Cvar_RegisterVariable (&vr_prediction);
	Cvar_SetCallback (&vr_prediction, VR_Prediction_f);
	Cvar_RegisterVariable (&vr_driftcorrect);
	Cvar_SetCallback (&vr_driftcorrect, VR_DriftCorrect_f);
	Cvar_RegisterVariable (&vr_crosshair);
	Cvar_RegisterVariable (&vr_chromabr);
	Cvar_SetCallback (&vr_chromabr, VR_ChromAbr_f);
	Cvar_RegisterVariable (&vr_aimmode);
	Cvar_RegisterVariable (&vr_ipd);
	Cvar_SetCallback (&vr_ipd, VR_IPD_f);
	Cvar_RegisterVariable (&vr_deadzone);
	Cvar_SetCallback (&vr_deadzone, VR_Deadzone_f);
}

qboolean VR_Enable()
{
	vr_hmd_settings_t hmd;

	qboolean sdkInitialized = false;
	float aspect, r, h;
	float *dk, *chrm;
	float dist_scale;
	float fovy;

	float ss = vr_supersample.value;

	// convert milliseconds to seconds
	float prediction = vr_prediction.value / 1000.0f;
	int driftcorrection = (int) vr_driftcorrect.value;

	sdkInitialized = InitOculusSDK();

	if (!sdkInitialized) 
	{
		Con_Printf("Failed to Initialize Oculus SDK");
		return false;
	}

	GetOculusDeviceInfo(&hmd);

	shader_support = InitShaderExtension();   
    if (!shader_support) 
	{
		Con_Printf("Failed to get OpenGL Extensions");
        return false;
    }
	
	if (vr_chromabr.value)
		lens_warp.shader = &lens_warp_shader_chrm;
	else 
		lens_warp.shader = &lens_warp_shader_norm;


	rift_enabled = CompileShaderProgram(lens_warp.shader);
	if (!rift_enabled) 
	{
		lens_warp.shader = NULL;
		Con_Printf("Failed to Compile Shaders");
		return false;
	}
	

	hmd_ipd = hmd.interpupillary_distance;

	// Calculate lens distortion and fov
	aspect = hmd.h_resolution / (2.0f * hmd.v_resolution);
	h = 1.0f - (2.0f * hmd.lens_separation_distance) / hmd.h_screen_size;
	r = -1.0f - h;

	dk = hmd.distortion_k;
	chrm = hmd.chrom_abr;
	dist_scale = (dk[0] + dk[1] * pow(r,2) + dk[2] * pow(r,4) + dk[3] * pow(r,6));
	fovy = 2 * atan2(hmd.v_screen_size * dist_scale, 2 * hmd.eye_to_screen_distance);
	viewport_fov_y = fovy * 180 / M_PI;
	viewport_fov_x = viewport_fov_y * aspect;

	// Set up eyes
	left_eye.lens_shift = h;
	left_eye.fbo = CreateFBO(glwidth * left_eye.viewport.width * ss, glheight * left_eye.viewport.height * ss);
	CreatePerspectiveMatrix(left_eye.projection_matrix, fovy, aspect, 4, gl_farclip.value, h);

	right_eye.lens_shift = -h;
	right_eye.fbo = CreateFBO(glwidth * right_eye.viewport.width * ss, glheight * right_eye.viewport.height * ss);
	CreatePerspectiveMatrix(right_eye.projection_matrix, fovy, aspect, 4, gl_farclip.value, -h);

	
	// Get uniform location and set some values
	glUseProgramObjectARB(lens_warp.shader->program);
	lens_warp.uniform.scale = glGetUniformLocationARB(lens_warp.shader->program, "scale");
	lens_warp.uniform.scale_in = glGetUniformLocationARB(lens_warp.shader->program, "scaleIn");
	lens_warp.uniform.lens_center = glGetUniformLocationARB(lens_warp.shader->program, "lensCenter");
	lens_warp.uniform.hmd_warp_param = glGetUniformLocationARB(lens_warp.shader->program, "hmdWarpParam");
	lens_warp.uniform.chrom_ab_param = glGetUniformLocationARB(lens_warp.shader->program, "chromAbParam");

	glUniform4fARB(lens_warp.uniform.chrom_ab_param,chrm[0],chrm[1],chrm[2],chrm[3]);
	glUniform4fARB(lens_warp.uniform.hmd_warp_param, dk[0], dk[1], dk[2], dk[3]);
	glUniform2fARB(lens_warp.uniform.scale_in, 1.0f, 1.0f/aspect);
	glUniform2fARB(lens_warp.uniform.scale, 1.0f/dist_scale, 1.0f * aspect/dist_scale);
	glUseProgramObjectARB(0);

	VR_IPD_f(&vr_ipd);
	VR_Prediction_f(&vr_prediction);
	VR_DriftCorrect_f(&vr_driftcorrect);

	return true;
}

void VR_Disable()
{
	if (rift_enabled) 
	{
		DestroyShaderProgram(lens_warp.shader);
		lens_warp.shader = NULL;
		DeleteFBO(left_eye.fbo);
		DeleteFBO(right_eye.fbo);
	}
	ReleaseOculusSDK();
	rift_enabled = false;

	vid.recalc_refdef = true;
}

static void RenderScreenForEye(hmd_eye_t *eye)
{
	// Remember the current vrect.width and vieworg; we have to modify it here
	// for each eye
	int oldwidth = r_refdef.vrect.width;
	int oldheight = r_refdef.vrect.height;
	int oldglheight = glheight;
	int oldglwidth = glwidth;

	float ss = vr_supersample.value;

	r_refdef.vrect.width *= eye->viewport.width * ss;
	r_refdef.vrect.height *= eye->viewport.height * ss;
	glwidth *= ss;
	glheight *= ss;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, eye->fbo.framebuffer);
	glClear(GL_DEPTH_BUFFER_BIT);


	vr_projection_matrix = eye->projection_matrix;
	vr_view_offset = eye->offset;

	srand((int) (cl.time * 1000)); //sync random stuff between eyes

	r_refdef.fov_x = viewport_fov_x;
	r_refdef.fov_y = viewport_fov_y;

	// Draw everything
	SCR_UpdateScreenContent ();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	r_refdef.vrect.width = oldwidth;
	r_refdef.vrect.height = oldheight;

	glwidth = oldglwidth;
	glheight = oldglheight;

	vr_projection_matrix = NULL;
	vr_view_offset = 0;
}

static void RenderEyeOnScreen(hmd_eye_t *eye)
{
	glViewport(
		(glwidth-glx) * eye->viewport.left,
		(glheight-gly) * eye->viewport.top,
		glwidth * eye->viewport.width,
		glheight * eye->viewport.height
	);

	glUniform2fARB(lens_warp.uniform.lens_center, eye->lens_shift, 0);
	glBindTexture(GL_TEXTURE_2D, eye->fbo.texture);
	
	glBegin(GL_QUADS);
	glTexCoord2f (0, 0); glVertex2f (-1, -1);
	glTexCoord2f (1, 0); glVertex2f (1, -1);
	glTexCoord2f (1, 1); glVertex2f (1, 1);
	glTexCoord2f (0, 1); glVertex2f (-1, 1);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

void VR_UpdateScreenContent()
{
	static float lastYaw, lastPitch,lastAimYaw;
	vec3_t orientation;

	// Get current orientation of the HMD
	GetOculusView(orientation);

	switch( (int)vr_aimmode.value )
	{
		// 1: (Default) Head Aiming; View YAW is mouse+head, PITCH is head
		default:
		case HMD_AIMMODE_HEAD_MYAW:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] = orientation[PITCH];
			cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - lastYaw;
			break;
		
		// 2: Head Aiming; View YAW and PITCH is mouse+head
		case HMD_AIMMODE_HEAD_MYAW_MPITCH:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH] - lastPitch;
			cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - lastYaw;
			break;
		
		// 3: Mouse Aiming; View YAW is mouse+head, PITCH is head
		case HMD_AIMMODE_MOUSE_MYAW:
			cl.viewangles[PITCH] = orientation[PITCH];
			cl.viewangles[YAW]   = cl.aimangles[YAW] + orientation[YAW];
			break;
		
		// 4: Mouse Aiming; View YAW and PITCH is mouse+head
		case HMD_AIMMODE_MOUSE_MYAW_MPITCH:
			cl.viewangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH];
			cl.viewangles[YAW]   = cl.aimangles[YAW] + orientation[YAW];
			break;
		
		case HMD_AIMMODE_BLENDED:
			{
				float diffHMDYaw = orientation[YAW] - lastYaw;
				float diffHMDPitch = orientation[PITCH] - lastPitch;
				float diffAimYaw = cl.aimangles[YAW] - lastAimYaw;
				float diffPitch = cl.viewangles[PITCH] - cl.aimangles[PITCH];
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

	lastPitch = orientation[PITCH];
	lastYaw = orientation[YAW];
	lastAimYaw = cl.aimangles[YAW];
	
	VectorCopy (cl.viewangles, r_refdef.viewangles);
	VectorCopy (cl.aimangles, r_refdef.aimangles);

	// Render the scene for each eye into their FBOs
	RenderScreenForEye(&left_eye);
	RenderScreenForEye(&right_eye);


	// Render the views onto the screen with the lens warp shader
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glUseProgramObjectARB(lens_warp.shader->program);
	RenderEyeOnScreen(&left_eye);
	RenderEyeOnScreen(&right_eye);
	glUseProgramObjectARB(0);
}

void VR_AddOrientationToViewAngles(vec3_t angles)
{
	vec3_t orientation;

	// Get current orientation of the HMD
	GetOculusView(orientation);

	angles[PITCH] = angles[PITCH] + orientation[PITCH]; 
	angles[YAW] = angles[YAW] + orientation[YAW]; 
	angles[ROLL] = orientation[ROLL];
}

void VR_ShowCrosshair ()
{
	vec3_t forward, up, right;
	vec3_t start, end, impact;
	float ss;
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
	VectorMA (start, 4096, forward, end);
	TraceLine (start, end, impact); // todo - trace to nearest entity

	ss = vr_supersample.value;

	switch((int) vr_crosshair.value)
	{	
		// point crosshair
	default:
	case HMD_CROSSHAIR_POINT:
		glEnable(GL_POINT_SMOOTH);
		glColor4f (1, 0, 0, 0.5);
		glPointSize( 3.0 * glheight / (800.0 * ss) );

		glBegin(GL_POINTS);
		glVertex3f (impact[0], impact[1], impact[2]);
		glEnd();
		glDisable(GL_POINT_SMOOTH);
		break;

		// laser crosshair
	case HMD_CROSSHAIR_LINE:
		glColor4f (1, 0, 0, 0.4);
		glLineWidth( 2.0 * glheight / (800.0 * ss) );
		glBegin (GL_LINES);
		glVertex3f (start[0], start[1], start[2]);
		glVertex3f (impact[0], impact[1], impact[2]);
		glEnd ();
		break;
		// point crosshair at infinity
	case HMD_CROSSHAIR_POINT_INF:
		glEnable(GL_POINT_SMOOTH);
		glColor4f (1, 0, 0, 0.5);
		glPointSize( 3.0 * glheight / (800.0 * ss) );
		glBegin(GL_POINTS);
		glVertex3f (end[0], end[1], end[2]);
		glEnd();
		glDisable(GL_POINT_SMOOTH);
		break;
		// allow the crosshair to be totally disabled
	case HMD_CROSSHAIR_NONE:
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
	float scale_hud = 0.04;

	glPushMatrix();
	glDisable (GL_DEPTH_TEST); // prevents drawing sprites on sprites from interferring with one another

	VectorCopy(cl.aimangles, sbar_angles)

	if (vr_aimmode.value == HMD_AIMMODE_HEAD_MYAW || vr_aimmode.value == HMD_AIMMODE_HEAD_MYAW_MPITCH)
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

void VR_ResetOrientation()
{
	ResetOculusOrientation();
}
