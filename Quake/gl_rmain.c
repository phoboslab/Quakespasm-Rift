/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2013 Dominic Szablewski

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_main.c

#include "quakedef.h"
#include "r_renderhmd.h"

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

//johnfitz -- rendering statistics
int rs_brushpolys, rs_aliaspolys, rs_skypolys, rs_particles, rs_fogpolys;
int rs_dynamiclightmaps, rs_brushpasses, rs_aliaspasses, rs_skypasses;
float rs_megatexels;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

float r_fovx, r_fovy; //johnfitz -- rendering fov may be different becuase of r_waterwarp and r_stereo

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


cvar_t	r_norefresh = {"r_norefresh","0",CVAR_NONE};
cvar_t	r_drawentities = {"r_drawentities","1",CVAR_NONE};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1",CVAR_NONE};
cvar_t	r_speeds = {"r_speeds","0",CVAR_NONE};
cvar_t	r_fullbright = {"r_fullbright","0",CVAR_NONE};
cvar_t	r_lightmap = {"r_lightmap","0",CVAR_NONE};
cvar_t	r_shadows = {"r_shadows","0",CVAR_ARCHIVE};
cvar_t	r_wateralpha = {"r_wateralpha","1",CVAR_ARCHIVE};
cvar_t	r_dynamic = {"r_dynamic","1",CVAR_ARCHIVE};
cvar_t	r_novis = {"r_novis","0",CVAR_ARCHIVE};

cvar_t	gl_finish = {"gl_finish","0",CVAR_NONE};
cvar_t	gl_clear = {"gl_clear","0",CVAR_NONE};
cvar_t	gl_cull = {"gl_cull","1",CVAR_NONE};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1",CVAR_NONE};
cvar_t	gl_affinemodels = {"gl_affinemodels","0",CVAR_NONE};
cvar_t	gl_polyblend = {"gl_polyblend","1",CVAR_NONE};
cvar_t	gl_flashblend = {"gl_flashblend","0",CVAR_ARCHIVE};
cvar_t	gl_playermip = {"gl_playermip","0",CVAR_NONE};
cvar_t	gl_nocolors = {"gl_nocolors","0",CVAR_NONE};

//johnfitz -- new cvars
cvar_t	r_stereo = {"r_stereo","0",CVAR_NONE};
cvar_t	r_stereodepth = {"r_stereodepth","128",CVAR_NONE};
cvar_t	r_clearcolor = {"r_clearcolor","2",CVAR_ARCHIVE};
cvar_t	r_drawflat = {"r_drawflat","0",CVAR_NONE};
cvar_t	r_flatlightstyles = {"r_flatlightstyles", "0", CVAR_NONE};
cvar_t	gl_fullbrights = {"gl_fullbrights", "1", CVAR_ARCHIVE};
cvar_t	gl_farclip = {"gl_farclip", "16384", CVAR_ARCHIVE};
cvar_t	gl_overbright = {"gl_overbright", "1", CVAR_ARCHIVE};
cvar_t	gl_overbright_models = {"gl_overbright_models", "1", CVAR_ARCHIVE};
cvar_t	r_oldskyleaf = {"r_oldskyleaf", "0", CVAR_NONE};
cvar_t	r_drawworld = {"r_drawworld", "1", CVAR_NONE};
cvar_t	r_showtris = {"r_showtris", "0", CVAR_NONE};
cvar_t	r_showbboxes = {"r_showbboxes", "0", CVAR_NONE};
cvar_t	r_lerpmodels = {"r_lerpmodels", "1", CVAR_NONE};
cvar_t	r_lerpmove = {"r_lerpmove", "1", CVAR_NONE};
cvar_t	r_nolerp_list = {"r_nolerp_list", "progs/flame.mdl,progs/flame2.mdl,progs/braztall.mdl,progs/brazshrt.mdl,progs/longtrch.mdl,progs/flame_pyre.mdl,progs/v_saw.mdl,progs/v_xfist.mdl,progs/h2stuff/newfire.mdl", CVAR_NONE};
extern cvar_t	r_vfog;
//johnfitz

//phoboslab -- cvars for oculus rift
cvar_t  r_oculusrift = {"r_oculusrift", "0", CVAR_NONE};
cvar_t  r_oculusrift_ipd = {"r_oculusrift_ipd","-1",CVAR_NONE};

// cvars that get saved
cvar_t  r_oculusrift_supersample = {"r_oculusrift_supersample", "2", CVAR_ARCHIVE};
cvar_t  r_oculusrift_prediction = {"r_oculusrift_prediction","40", CVAR_ARCHIVE};
cvar_t  r_oculusrift_driftcorrect = {"r_oculusrift_driftcorrect","1", CVAR_ARCHIVE};
cvar_t  r_oculusrift_crosshair = {"r_oculusrift_crosshair","1", CVAR_ARCHIVE};
cvar_t  r_oculusrift_chromabr = {"r_oculusrift_chromabr","1", CVAR_ARCHIVE};
cvar_t  r_oculusrift_aimmode = {"r_oculusrift_aimmode","1", CVAR_ARCHIVE};
cvar_t  r_oculusrift_deadzone = {"r_oculusrift_deadzone","30",CVAR_ARCHIVE};
//phoboslab



cvar_t	gl_zfix = {"gl_zfix", "1", CVAR_ARCHIVE}; // QuakeSpasm z-fighting fix

qboolean r_drawflat_cheatsafe, r_fullbright_cheatsafe, r_lightmap_cheatsafe, r_drawworld_cheatsafe; //johnfitz

/*
=================
R_CullBox -- johnfitz -- replaced with new function from lordhavoc

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (vec3_t emins, vec3_t emaxs)
{
	int i;
	mplane_t *p;
	for (i = 0;i < 4;i++)
	{
		p = frustum + i;
		switch(p->signbits)
		{
		default:
		case 0:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}
/*
===============
R_CullModelForEntity -- johnfitz -- uses correct bounds based on rotation
===============
*/
qboolean R_CullModelForEntity (entity_t *e)
{
	vec3_t mins, maxs;

	if (e->angles[0] || e->angles[2]) //pitch or roll
	{
		VectorAdd (e->origin, e->model->rmins, mins);
		VectorAdd (e->origin, e->model->rmaxs, maxs);
	}
	else if (e->angles[1]) //yaw
	{
		VectorAdd (e->origin, e->model->ymins, mins);
		VectorAdd (e->origin, e->model->ymaxs, maxs);
	}
	else //no rotation
	{
		VectorAdd (e->origin, e->model->mins, mins);
		VectorAdd (e->origin, e->model->maxs, maxs);
	}

	return R_CullBox (mins, maxs);
}

/*
===============
R_RotateForEntity -- johnfitz -- modified to take origin and angles instead of pointer to entity
===============
*/
void R_RotateForEntity (vec3_t origin, vec3_t angles)
{
	glTranslatef (origin[0],  origin[1],  origin[2]);
	glRotatef (angles[1],  0, 0, 1);
	glRotatef (-angles[0],  0, 1, 0);
	glRotatef (angles[2],  1, 0, 0);
}

/*
=============
GL_PolygonOffset -- johnfitz

negative offset moves polygon closer to camera
=============
*/
void GL_PolygonOffset (int offset)
{
	if (offset > 0)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(1, offset);
	}
	else if (offset < 0)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-1, offset);
	}
	else
	{
		glDisable (GL_POLYGON_OFFSET_FILL);
		glDisable (GL_POLYGON_OFFSET_LINE);
	}
}

//==============================================================================
//
// SETUP FRAME
//
//==============================================================================

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

/*
===============
TurnVector -- johnfitz

turn forward towards side on the plane defined by forward and side
if angle = 90, the result will be equal to side
assumes side and forward are perpendicular, and normalized
to turn away from side, use a negative angle
===============
*/
#define DEG2RAD( a ) ( (a) * M_PI_DIV_180 )
void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle)
{
	float scale_forward, scale_side;

	scale_forward = cos( DEG2RAD( angle ) );
	scale_side = sin( DEG2RAD( angle ) );

	out[0] = scale_forward*forward[0] + scale_side*side[0];
	out[1] = scale_forward*forward[1] + scale_side*side[1];
	out[2] = scale_forward*forward[2] + scale_side*side[2];
}

/*
===============
R_SetFrustum -- johnfitz -- rewritten
===============
*/
void R_SetFrustum (float fovx, float fovy)
{
	int		i;

	if (r_stereo.value )
		fovx += 10; //silly hack so that polygons don't drop out becuase of stereo skew
	
	if (r_oculusrift.value )
		fovx += 25; // meh

	TurnVector(frustum[0].normal, vpn, vright, fovx/2 - 90); //left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - fovx/2); //right plane
	TurnVector(frustum[2].normal, vpn, vup, 90 - fovy/2); //bottom plane
	TurnVector(frustum[3].normal, vpn, vup, fovy/2 - 90); //top plane

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal); //FIXME: shouldn't this always be zero?
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
=============
GL_SetFrustum -- johnfitz -- written to replace MYgluPerspective
phoboslab -- fixed for oculus renderer
=============
*/
#define NEARCLIP 4
float frustum_skew = 0.0; //used by r_stereo
void GL_SetFrustum(float fovx, float fovy)
{
	GLfloat xmax, ymax;
	GLfloat aspect = fovx/fovy;

	ymax = NEARCLIP * tan(fovy * M_PI / 360.0);
	xmax = ymax * aspect;

	glFrustum(-xmax + frustum_skew, xmax + frustum_skew, -ymax, ymax, NEARCLIP, gl_farclip.value);
}

/*
=============
R_SetupGL
=============
*/
extern float *hmd_projection_matrix; // phoboslab
void R_SetupGL (void)
{
	//johnfitz -- rewrote this section
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glViewport (glx + r_refdef.vrect.x,
				gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height,
				r_refdef.vrect.width,
				r_refdef.vrect.height);
	//johnfitz

	if (hmd_projection_matrix) {
		glLoadMatrixf(hmd_projection_matrix);
	}
	else {
		GL_SetFrustum (r_fovx, r_fovy); //johnfitz -- use r_fov* vars
	}

//	glCullFace(GL_BACK); //johnfitz -- glquake used CCW with backwards culling -- let's do it right

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear -- johnfitz -- rewritten and gutted
=============
*/
void R_Clear (void)
{
	unsigned int clearbits;

	clearbits = GL_DEPTH_BUFFER_BIT;
	if (gl_clear.value)
		clearbits |= GL_COLOR_BUFFER_BIT;
	glClear (clearbits);
}

/*
===============
R_SetupScene -- johnfitz -- this is the stuff that needs to be done once per eye in stereo mode
===============
*/
void R_SetupScene (void)
{
	R_PushDlights ();
	R_AnimateLight ();
	r_framecount++;
	R_SetupGL ();
}

/*
===============
R_SetupView -- johnfitz -- this is the stuff that needs to be done once per frame, even in stereo mode
===============
*/
void R_SetupView (void)
{
	Fog_SetupFrame (); //johnfitz

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	//johnfitz -- calculate r_fovx and r_fovy here
	r_fovx = r_refdef.fov_x;
	r_fovy = r_refdef.fov_y;
	if (r_waterwarp.value)
	{
		int contents = Mod_PointInLeaf (r_origin, cl.worldmodel)->contents;
		if (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
		{
			//variance is a percentage of width, where width = 2 * tan(fov / 2) otherwise the effect is too dramatic at high FOV and too subtle at low FOV.  what a mess!
			r_fovx = atan(tan(DEG2RAD(r_refdef.fov_x) / 2) * (0.97 + sin(cl.time * 1.5) * 0.03)) * 2 / M_PI_DIV_180;
			r_fovy = atan(tan(DEG2RAD(r_refdef.fov_y) / 2) * (1.03 - sin(cl.time * 1.5) * 0.03)) * 2 / M_PI_DIV_180;
		}
	}
	//johnfitz

	R_SetFrustum (r_fovx, r_fovy); //johnfitz -- use r_fov* vars

	R_MarkSurfaces (); //johnfitz -- create texture chains from PVS

	R_CullSurfaces (); //johnfitz -- do after R_SetFrustum and R_MarkSurfaces

	R_UpdateWarpTextures (); //johnfitz -- do this before R_Clear

	R_Clear ();

	//johnfitz -- cheat-protect some draw modes
	r_drawflat_cheatsafe = r_fullbright_cheatsafe = r_lightmap_cheatsafe = false;
	r_drawworld_cheatsafe = true;
	if (cl.maxclients == 1)
	{
		if (!r_drawworld.value) r_drawworld_cheatsafe = false;

		if (r_drawflat.value) r_drawflat_cheatsafe = true;
		else if (r_fullbright.value || !cl.worldmodel->lightdata) r_fullbright_cheatsafe = true;
		else if (r_lightmap.value) r_lightmap_cheatsafe = true;
	}
	//johnfitz
}

//==============================================================================
//
// RENDER VIEW
//
//==============================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (qboolean alphapass) //johnfitz -- added parameter
{
	int		i;

	if (!r_drawentities.value)
		return;

	//johnfitz -- sprites are not a special case
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		//johnfitz -- if alphapass is true, draw only alpha entites this time
		//if alphapass is false, draw only nonalpha entities this time
		if ((ENTALPHA_DECODE(currententity->alpha) < 1 && !alphapass) ||
			(ENTALPHA_DECODE(currententity->alpha) == 1 && alphapass))
			continue;

		//johnfitz -- chasecam
		if (currententity == &cl_entities[cl.viewentity])
			currententity->angles[0] *= 0.3;
		//johnfitz

		switch (currententity->model->type)
		{
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
		}
	}
}

/*
=============
R_DrawViewModel -- johnfitz -- gutted
=============
*/
void R_DrawViewModel (void)
{
	if (chase_active.value)
		return;

	if(r_oculusrift.value && r_oculusrift_crosshair.value)
		R_ShowHMDCrosshair();

	if (!r_drawviewmodel.value || !r_drawentities.value )
		return;
	
	if (cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	//johnfitz -- this fixes a crash
	if (currententity->model->type != mod_alias)
		return;
	//johnfitz


	// hack the depth range to prevent view model from poking into walls
	
	// JM - turned off this hack because it doesn't look right in 3d
	// also the axe going right into enemies is awesome

	//glDepthRange (0, 0.3);
	R_DrawAliasModel (currententity);
	//glDepthRange (0, 1);
}

/*
================
R_EmitWirePoint -- johnfitz -- draws a wireframe cross shape for point entities
================
*/
void R_EmitWirePoint (vec3_t origin)
{
	int size=8;

	glBegin (GL_LINES);
	glVertex3f (origin[0]-size, origin[1], origin[2]);
	glVertex3f (origin[0]+size, origin[1], origin[2]);
	glVertex3f (origin[0], origin[1]-size, origin[2]);
	glVertex3f (origin[0], origin[1]+size, origin[2]);
	glVertex3f (origin[0], origin[1], origin[2]-size);
	glVertex3f (origin[0], origin[1], origin[2]+size);
	glEnd ();
}

/*
================
R_EmitWireBox -- johnfitz -- draws one axis aligned bounding box
================
*/
void R_EmitWireBox (vec3_t mins, vec3_t maxs)
{
	glBegin (GL_QUAD_STRIP);
	glVertex3f (mins[0], mins[1], mins[2]);
	glVertex3f (mins[0], mins[1], maxs[2]);
	glVertex3f (maxs[0], mins[1], mins[2]);
	glVertex3f (maxs[0], mins[1], maxs[2]);
	glVertex3f (maxs[0], maxs[1], mins[2]);
	glVertex3f (maxs[0], maxs[1], maxs[2]);
	glVertex3f (mins[0], maxs[1], mins[2]);
	glVertex3f (mins[0], maxs[1], maxs[2]);
	glVertex3f (mins[0], mins[1], mins[2]);
	glVertex3f (mins[0], mins[1], maxs[2]);
	glEnd ();
}

/*
================
R_ShowBoundingBoxes -- johnfitz

draw bounding boxes -- the server-side boxes, not the renderer cullboxes
================
*/
void R_ShowBoundingBoxes (void)
{
	extern		edict_t *sv_player;
	vec3_t		mins,maxs;
	edict_t		*ed;
	int			i;

	if (!r_showbboxes.value || cl.maxclients > 1 || !r_drawentities.value || !sv.active)
		return;

	glDisable (GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_PolygonOffset (OFFSET_SHOWTRIS);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_CULL_FACE);
	glColor3f (1,1,1);

	for (i=0, ed=NEXT_EDICT(sv.edicts) ; i<sv.num_edicts ; i++, ed=NEXT_EDICT(ed))
	{
		if (ed == sv_player)
			continue; //don't draw player's own bbox

//		if (r_showbboxes.value != 2)
//			if (!SV_VisibleToClient (sv_player, ed, sv.worldmodel))
//				continue; //don't draw if not in pvs

		if (ed->v.mins[0] == ed->v.maxs[0] && ed->v.mins[1] == ed->v.maxs[1] && ed->v.mins[2] == ed->v.maxs[2])
		{
			//point entity
			R_EmitWirePoint (ed->v.origin);
		}
		else
		{
			//box entity
			VectorAdd (ed->v.mins, ed->v.origin, mins);
			VectorAdd (ed->v.maxs, ed->v.origin, maxs);
			R_EmitWireBox (mins, maxs);
		}
	}

	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_CULL_FACE);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	GL_PolygonOffset (OFFSET_NONE);
	glEnable (GL_DEPTH_TEST);

	Sbar_Changed (); //so we don't get dots collecting on the statusbar
}

/*
================
R_ShowTris -- johnfitz
================
*/
void R_ShowTris (void)
{
	extern cvar_t r_particles;
	int i;

	if (r_showtris.value < 1 || r_showtris.value > 2 || cl.maxclients > 1)
		return;

	if (r_showtris.value == 1)
		glDisable (GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_PolygonOffset (OFFSET_SHOWTRIS);
	glDisable (GL_TEXTURE_2D);
	glColor3f (1,1,1);
//	glEnable (GL_BLEND);
//	glBlendFunc (GL_ONE, GL_ONE);

	if (r_drawworld.value)
	{
		R_DrawTextureChains_ShowTris ();
	}

	if (r_drawentities.value)
	{
		for (i=0 ; i<cl_numvisedicts ; i++)
		{
			currententity = cl_visedicts[i];

			if (currententity == &cl_entities[cl.viewentity]) // chasecam
				currententity->angles[0] *= 0.3;

			switch (currententity->model->type)
			{
			case mod_brush:
				R_DrawBrushModel_ShowTris (currententity);
				break;
			case mod_alias:
				R_DrawAliasModel_ShowTris (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				break;
			}
		}

		// viewmodel
		currententity = &cl.viewent;
		if (r_drawviewmodel.value
			&& !chase_active.value
			&& cl.stats[STAT_HEALTH] > 0
			&& !(cl.items & IT_INVISIBILITY)
			&& currententity->model
			&& currententity->model->type == mod_alias)
		{
			glDepthRange (0, 0.3);
			R_DrawAliasModel_ShowTris (currententity);
			glDepthRange (0, 1);
		}
	}

	if (r_particles.value)
	{
		R_DrawParticles_ShowTris ();
	}

//	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glDisable (GL_BLEND);
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	GL_PolygonOffset (OFFSET_NONE);
	if (r_showtris.value == 1)
		glEnable (GL_DEPTH_TEST);

	Sbar_Changed (); //so we don't get dots collecting on the statusbar
}

/*
================
R_DrawShadows
================
*/
void R_DrawShadows (void)
{
	int i;

	if (!r_shadows.value || !r_drawentities.value || r_drawflat_cheatsafe || r_lightmap_cheatsafe)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity->model->type != mod_alias)
			continue;

		if (currententity == &cl.viewent)
			return;

		GL_DrawAliasShadow (currententity);
	}
}

/*
================
R_RenderScene
================
*/
void R_RenderScene (void)
{
	R_SetupScene (); //johnfitz -- this does everything that should be done once per call to RenderScene

	Fog_EnableGFog (); //johnfitz

	Sky_DrawSky (); //johnfitz

	R_DrawWorld ();

	S_ExtraUpdate (); // don't let sound get messed up if going slow

	R_DrawShadows (); //johnfitz -- render entity shadows

	R_DrawEntitiesOnList (false); //johnfitz -- false means this is the pass for nonalpha entities

	R_DrawTextureChains_Water (); //johnfitz -- drawn here since they might have transparency

	R_DrawEntitiesOnList (true); //johnfitz -- true means this is the pass for alpha entities

	R_RenderDlights (); //triangle fan dlights -- johnfitz -- moved after water

	R_DrawParticles ();

	Fog_DisableGFog (); //johnfitz

	R_DrawViewModel (); //johnfitz -- moved here from R_RenderView

	R_ShowTris (); //johnfitz

	R_ShowBoundingBoxes (); //johnfitz
}

/*
================
R_RenderView
================
*/
extern float hmd_view_offset;
void R_RenderView (void)
{
	double	time1, time2;

	if (r_norefresh.value)
		return;

	if (!cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	time1 = 0; /* avoid compiler warning */
	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_DoubleTime ();

		//johnfitz -- rendering statistics
		rs_brushpolys = rs_aliaspolys = rs_skypolys = rs_particles = rs_fogpolys = rs_megatexels =
		rs_dynamiclightmaps = rs_aliaspasses = rs_skypasses = rs_brushpasses = 0;
	}
	else if (gl_finish.value)
		glFinish ();

	R_SetupView (); //johnfitz -- this does everything that should be done once per frame

	//johnfitz -- stereo rendering -- full of hacky goodness
	if (r_stereo.value)
	{
		float eyesep = CLAMP(-8.0f, r_stereo.value, 8.0f);
		float fdepth = CLAMP(32.0f, r_stereodepth.value, 1024.0f);

		AngleVectors (r_refdef.viewangles, vpn, vright, vup);

		//render left eye (red)
		glColorMask(1, 0, 0, 1);
		VectorMA (r_refdef.vieworg, -0.5f * eyesep, vright, r_refdef.vieworg);
		frustum_skew = 0.5 * eyesep * NEARCLIP / fdepth;
		srand((int) (cl.time * 1000)); //sync random stuff between eyes

		R_RenderScene ();

		//render right eye (cyan)
		glClear (GL_DEPTH_BUFFER_BIT);
		glColorMask(0, 1, 1, 1);
		VectorMA (r_refdef.vieworg, 1.0f * eyesep, vright, r_refdef.vieworg);
		frustum_skew = -frustum_skew;
		srand((int) (cl.time * 1000)); //sync random stuff between eyes

		R_RenderScene ();

		//restore
		glColorMask(1, 1, 1, 1);
		VectorMA (r_refdef.vieworg, -0.5f * eyesep, vright, r_refdef.vieworg);
		frustum_skew = 0.0f;
	}
	else
	{
		VectorMA (r_refdef.vieworg, hmd_view_offset, vright, r_refdef.vieworg);
		R_RenderScene ();
	}
	//johnfitz

	//johnfitz -- modified r_speeds output
	time2 = Sys_DoubleTime ();
	if (r_speeds.value == 2)
		Con_Printf ("%3i ms  %4i/%4i wpoly %4i/%4i epoly %3i lmap %4i/%4i sky %1.1f mtex\n",
					(int)((time2-time1)*1000),
					rs_brushpolys,
					rs_brushpasses,
					rs_aliaspolys,
					rs_aliaspasses,
					rs_dynamiclightmaps,
					rs_skypolys,
					rs_skypasses,
					TexMgr_FrameUsage ());
	else if (r_speeds.value)
		Con_Printf ("%3i ms  %4i wpoly %4i epoly %3i lmap\n",
					(int)((time2-time1)*1000),
					rs_brushpolys,
					rs_aliaspolys,
					rs_dynamiclightmaps);
	//johnfitz
}


