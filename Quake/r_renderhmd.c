// 2013 Dominic Szablewski - phoboslab.org

#include "quakedef.h"
#include "oculus_sdk.h"


typedef struct {
    GLhandleARB program;
    GLhandleARB vert_shader;
    GLhandleARB frag_shader;
    const char *vert_source;
    const char *frag_source;
} shader_t;


typedef struct {
	float h_resolution;
	float v_resolution;
	float h_screen_size;
	float v_screen_size;
	float interpupillary_distance;
	float lens_separation_distance;
	float eye_to_screen_distance;
	float distortion_k[4];
	float chrom_abr[4];
} hmd_settings_t;


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

static qboolean shader_support;
static qboolean shader_support_initialized;


// Lens Warp Shader

static shader_t *lens_warp_shader = NULL;

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
    "\n"
    // Scales input texture coordinates for distortion.
    // ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
    // larger due to aspect ratio.
    "void main()\n"
    "{\n"
	"	vec2 uv = (vUv*2.0)-1.0;\n" // range from [0,1] to [-1,1]
	"	vec2 theta = (uv-lensCenter)*scaleIn;\n"
	"	float rSq = theta.x*theta.x + theta.y*theta.y;\n"
	"	vec2 theta1 = theta*(hmdWarpParam.x + hmdWarpParam.y*rSq + hmdWarpParam.z*rSq*rSq + hmdWarpParam.w*rSq*rSq*rSq);\n"
    "   \n"
    "   // Detect whether blue texture coordinates are out of range since these will scaled out the furthest.\n"
    "   vec2 thetaBlue = theta1 * (chromAbParam.z + chromAbParam.w * rSq);\n"
    "   vec2 tcBlue = lensCenter + scale * thetaBlue;\n"
	"	tcBlue = (tcBlue+1.0)/2.0;\n" // range from [-1,1] to [0,1]
    "   if (any(bvec2(clamp(tcBlue, vec2(0.0,0.0), vec2(1.0,1.0))-tcBlue)))\n"
    "   {\n"
    "       gl_FragColor = vec4(0.0,0.0,0.0,1.0);\n"
    "       return;\n"
    "   }\n"
    "   \n"
    "   // Now do blue texture lookup.\n"
    "   float blue = texture2D(texture, tcBlue).b;\n"
    "   \n"
    "   // Do green lookup (no scaling).\n"
    "   vec2  tcGreen = lensCenter + scale * theta1;\n"
	"	tcGreen = (tcGreen+1.0)/2.0;\n" // range from [-1,1] to [0,1]
    "   vec4  center = texture2D(texture, tcGreen);\n"
    "   \n"
    "   // Do red scale and lookup.\n"
    "   vec2  thetaRed = theta1 * (chromAbParam.x + chromAbParam.y * rSq);\n"
    "   vec2  tcRed = lensCenter + scale * thetaRed;\n"
	"	tcRed = (tcRed+1.0)/2.0;\n" // range from [-1,1] to [0,1]
    "   float red = texture2D(texture, tcRed).r;\n"
    "   \n"
    "   gl_FragColor = vec4(red, center.g, blue, center.a);\n"
    "}\n"
};

// Uniform locations for the Shader
static struct {
	GLuint scale;
	GLuint scale_in;
	GLuint lens_center;
	GLuint hmd_warp_param;
	GLuint chrom_ab_param;
} lens_warp_shader_uniforms;


// HMD Settings for OculusRift
hmd_settings_t oculus_rift_hmd = {
	1280,    // h_resolution
	800,     // v_resolution
	0.14976, // h_screen_size
	0.0936,  // v_screen_size
	0.064,   // interpupillary_distance
	0.064,   // lens_separation_distance
	0.041,   // eye_to_screen_distance
	{1.0, 0.22, 0.24, 0.0}, // distortion_k
	{0.996, -0.004, 1.014,0.0}
};


static hmd_eye_t left_eye = {0, 0, {0, 0, 0.5, 1}, 0};
static hmd_eye_t right_eye = {0, 0, {0.5, 0, 0.5, 1}, 0};
static float viewport_fov_x;
static float viewport_fov_y;

extern cvar_t r_oculusrift;
extern cvar_t r_oculusrift_supersample;
extern cvar_t r_oculusrift_prediction;
extern cvar_t r_oculusrift_driftcorrect;
extern cvar_t r_oculusrift_crosshair;
extern cvar_t r_oculusrift_chromabr;
extern cvar_t r_oculusrift_aimmode;
extern cvar_t r_oculusrift_showweapon;

extern int glx, gly, glwidth, glheight;
extern void SCR_UpdateScreenContent();
extern void SCR_CalcRefdef();
extern refdef_t r_refdef;


static qboolean CompileShader(GLhandleARB shader, const char *source)
{
    GLint status;

    glShaderSourceARB(shader, 1, &source, NULL);
    glCompileShaderARB(shader);
    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
    if (status == 0) {
        GLint length;
        char *info;

        glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
        info = SDL_stack_alloc(char, length+1);
        glGetInfoLogARB(shader, length, NULL, info);
        Con_Warning("Failed to compile shader:\n%s\n%s", source, info);
        SDL_stack_free(info);
        return false;
    }
	else {
        return true;
    }
}

static qboolean CompileShaderProgram(shader_t *shader)
{
	glGetError();
	if (shader)
	{

		shader->program = glCreateProgramObjectARB();

		shader->vert_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		if (!CompileShader(shader->vert_shader, shader->vert_source)) {
			return SDL_FALSE;
		}

		shader->frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		if (!CompileShader(shader->frag_shader, shader->frag_source)) {
			return SDL_FALSE;
		}

		glAttachObjectARB(shader->program, shader->vert_shader);
		glAttachObjectARB(shader->program, shader->frag_shader);
		glLinkProgramARB(shader->program); 
	}
	return (glGetError() == GL_NO_ERROR);
}

static void DestroyShaderProgram(shader_t *shader)
{
    if (shader_support && shader) {
        glDeleteObjectARB(shader->vert_shader);
        glDeleteObjectARB(shader->frag_shader);
        glDeleteObjectARB(shader->program);
    }
}

static qboolean InitShaderExtension()
{
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

extern cvar_t gl_farclip;
static qboolean rift_enabled;

static const float player_height_units = 56;
static const float player_height_m = 1.75;

qboolean R_InitHMDRenderer(hmd_settings_t *hmd)
{
	qboolean sdkInitialized = false;
	float aspect, r, h;
	float *dk, *chrm;
	float dist_scale, lens_shift;
	float fovy;

	float ss = r_oculusrift_supersample.value;

	// convert milliseconds to seconds
	float prediction = r_oculusrift_prediction.value / 1000.0f;
	int driftcorrection = (int) r_oculusrift_driftcorrect.value;

	shader_support = InitShaderExtension();   

    if (!shader_support) {
		Con_Printf("Failed to get OpenGL Extensions");
        return false;
    }
	
	if (r_oculusrift_chromabr.value)
	{
		lens_warp_shader = &lens_warp_shader_chrm;
	} else {
		lens_warp_shader = &lens_warp_shader_norm;
	}

	rift_enabled = CompileShaderProgram(lens_warp_shader);

	if (!rift_enabled) {
		lens_warp_shader = NULL;
		Con_Printf("Failed to Compile Shaders");
		return false;
	}

	// Calculate lens distortion and fov
	aspect = hmd->h_resolution / (2.0f * hmd->v_resolution);
	r = -1.0f - (4.0f * (hmd->h_screen_size/4.0f - hmd->lens_separation_distance/2.0f) / hmd->h_screen_size);
	h = 4.0f * (hmd->h_screen_size/4.0f - hmd->interpupillary_distance/2.0f) / hmd->h_screen_size;

	dk = hmd->distortion_k;
	chrm = hmd->chrom_abr;
	dist_scale = (dk[0] + dk[1] * pow(r,2) + dk[2] * pow(r,4) + dk[3] * pow(r,6));
	lens_shift = 4 * (hmd->h_screen_size/4 - hmd->lens_separation_distance/2) / hmd->h_screen_size;
	fovy = 2 * atan2(hmd->v_screen_size * dist_scale, 2 * hmd->eye_to_screen_distance);
	viewport_fov_y = fovy * 180 / M_PI;
	viewport_fov_x = viewport_fov_y * aspect;

	// Set up eyes
	left_eye.offset = -player_height_units * (hmd->interpupillary_distance/player_height_m) * 0.5;
	left_eye.lens_shift = lens_shift;
	left_eye.fbo = CreateFBO(glwidth * left_eye.viewport.width * ss, glheight * left_eye.viewport.height * ss);
	CreatePerspectiveMatrix(left_eye.projection_matrix, fovy, aspect, 4, gl_farclip.value, h);

	right_eye.offset = player_height_units * (hmd->interpupillary_distance/player_height_m) * 0.5;
	right_eye.lens_shift = -lens_shift;
	right_eye.fbo = CreateFBO(glwidth * right_eye.viewport.width * ss, glheight * right_eye.viewport.height * ss);
	CreatePerspectiveMatrix(right_eye.projection_matrix, fovy, aspect, 4, gl_farclip.value, -h);

	
	// Get uniform location and set some values
	glUseProgramObjectARB(lens_warp_shader->program);
	lens_warp_shader_uniforms.scale = glGetUniformLocationARB(lens_warp_shader->program, "scale");
	lens_warp_shader_uniforms.scale_in = glGetUniformLocationARB(lens_warp_shader->program, "scaleIn");
	lens_warp_shader_uniforms.lens_center = glGetUniformLocationARB(lens_warp_shader->program, "lensCenter");
	lens_warp_shader_uniforms.hmd_warp_param = glGetUniformLocationARB(lens_warp_shader->program, "hmdWarpParam");
	lens_warp_shader_uniforms.chrom_ab_param = glGetUniformLocationARB(lens_warp_shader->program, "chromAbParam");

	glUniform4fARB(lens_warp_shader_uniforms.chrom_ab_param,chrm[0],chrm[1],chrm[2],chrm[3]);
	glUniform4fARB(lens_warp_shader_uniforms.hmd_warp_param, dk[0], dk[1], dk[2], dk[3]);
	glUniform2fARB(lens_warp_shader_uniforms.scale_in, 1.0f, 1.0f/aspect);
	glUniform2fARB(lens_warp_shader_uniforms.scale, 1.0f/dist_scale, 1.0f * aspect/dist_scale);
	glUseProgramObjectARB(0);

	sdkInitialized = InitOculusSDK();

	if (!sdkInitialized) {
		Con_Printf("Failed to Initialize Oculus SDK");
		return false;
	}

	SetOculusPrediction(prediction);
	SetOculusDriftCorrect(driftcorrection);
	return true;
}

void R_ReleaseHMDRenderer()
{
	if (rift_enabled) {
	
		DestroyShaderProgram(lens_warp_shader);
		lens_warp_shader = NULL;
		DeleteFBO(left_eye.fbo);
		DeleteFBO(right_eye.fbo);
	}
	ReleaseOculusSDK();
	rift_enabled = false;

	vid.recalc_refdef = true;
}

void R_SetHMDPredictionTime()
{
	if (rift_enabled) {
		float prediction = r_oculusrift_prediction.value / 1000.0f;
		SetOculusPrediction(prediction);
	}
}

void R_SetHMDDriftCorrection()
{
	if (rift_enabled) {
		SetOculusDriftCorrect((int) r_oculusrift_driftcorrect.value);
	}
}

extern vec3_t vright;
extern cvar_t r_stereodepth;

float hmd_screen_2d[4] = {0,0,0,0};
float hmd_view_offset;
float *hmd_projection_matrix = NULL;

void RenderScreenForEye(hmd_eye_t *eye)
{
	// Remember the current vrect.width and vieworg; we have to modify it here
	// for each eye
	int oldwidth = r_refdef.vrect.width;
	int oldheight = r_refdef.vrect.height;
	int oldglheight = glheight;
	int oldglwidth = glwidth;

	float ss = r_oculusrift_supersample.value;

	r_refdef.vrect.width *= eye->viewport.width * ss;
	r_refdef.vrect.height *= eye->viewport.height * ss;
	glwidth *= ss;
	glheight *= ss;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, eye->fbo.framebuffer);
	glClear(GL_DEPTH_BUFFER_BIT);


	hmd_projection_matrix = eye->projection_matrix;
	hmd_view_offset = eye->offset;

	srand((int) (cl.time * 1000)); //sync random stuff between eyes

	r_refdef.fov_x = viewport_fov_x;
	r_refdef.fov_y = viewport_fov_y;

	// Cheap hack to make the UI readable in HMD mode
	hmd_screen_2d[0] = r_refdef.vrect.width/2.6 - eye->offset * r_refdef.vrect.width * 0.11;
	hmd_screen_2d[1] = r_refdef.vrect.height/3.5;
	hmd_screen_2d[2] = (r_refdef.vrect.width / 2)/2;
	hmd_screen_2d[3] = (r_refdef.vrect.height / 2);

	// Draw everything
	SCR_UpdateScreenContent ();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	r_refdef.vrect.width = oldwidth;
	r_refdef.vrect.height = oldheight;

	glwidth = oldglwidth;
	glheight = oldglheight;

	hmd_projection_matrix = NULL;
	hmd_view_offset = 0;
}

void RenderEyeOnScreen(hmd_eye_t *eye)
{
	glViewport(
		(glwidth-glx) * eye->viewport.left,
		(glheight-gly) * eye->viewport.top,
		glwidth * eye->viewport.width,
		glheight * eye->viewport.height
	);

	glUniform2fARB(lens_warp_shader_uniforms.lens_center, eye->lens_shift, 0);
	glBindTexture(GL_TEXTURE_2D, eye->fbo.texture);
	
	glBegin(GL_QUADS);
	glTexCoord2f (0, 0); glVertex2f (-1, -1);
	glTexCoord2f (1, 0); glVertex2f (1, -1);
	glTexCoord2f (1, 1); glVertex2f (1, 1);
	glTexCoord2f (0, 1); glVertex2f (-1, 1);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}


static float lastYaw;
extern viddef_t	vid;


void SCR_UpdateHMDScreenContent()
{
	vec3_t orientation;

	// Get current orientation of the HMD
	GetOculusView(orientation);

	if(r_oculusrift_aimmode.value == 1)
	{
		cl.viewangles[PITCH] = cl.aimangles[PITCH] + orientation[PITCH]; 
		cl.viewangles[YAW]   = cl.aimangles[YAW] + orientation[YAW]; 
	}
	else if(r_oculusrift_aimmode.value == 2)
	{
		cl.viewangles[PITCH] = orientation[PITCH];

		cl.aimangles[YAW] = cl.viewangles[YAW] = cl.aimangles[YAW] + orientation[YAW] - lastYaw;

		lastYaw = orientation[YAW];
	}

	cl.viewangles[ROLL]  = orientation[ROLL];

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

	glUseProgramObjectARB(lens_warp_shader->program);
	RenderEyeOnScreen(&left_eye);
	RenderEyeOnScreen(&right_eye);
	glUseProgramObjectARB(0);
}

void V_AddOrientationToViewAngles(vec3_t angles)
{
	vec3_t orientation;

	// Get current orientation of the HMD
	GetOculusView(orientation);

	angles[PITCH] = angles[PITCH] + orientation[PITCH]; 
	angles[YAW] = angles[YAW] + orientation[YAW]; 
	angles[ROLL] = orientation[ROLL];
}

void R_ShowHMDCrosshair ()
{
	vec3_t forward, up, right;
	vec3_t start, end, impact;
	
	if(sv_player && (int)(sv_player->v.weapon) == IT_AXE)
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

	// point crosshair
	if(r_oculusrift_crosshair.value == 1)
	{
		glColor4f (1, 0, 0, 0.5);
		glPointSize( 3.0 );

		glBegin(GL_POINTS);
 
		glVertex3f (impact[0], impact[1], impact[2]);
 
		glEnd();
	}

	// laser crosshair
	else if(r_oculusrift_crosshair.value == 2)
	{ 
		glColor4f (1, 0, 0, 0.4);

		glBegin (GL_LINES);
		glVertex3f (start[0], start[1], start[2]);
		glVertex3f (impact[0], impact[1], impact[2]);
		glEnd ();
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

void HMD_Sbar_Draw()
{	
	vec3_t sbar_angles, forward, right, up, target;
	float scale_hud = 0.04;

	if(!r_oculusrift_showweapon.value)
		return;

	glPushMatrix();
	glDisable (GL_DEPTH_TEST); // prevents drawing sprites on sprites from interferring with one another

	VectorCopy(cl.aimangles, sbar_angles)

	if(r_oculusrift_aimmode.value == 2)
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

