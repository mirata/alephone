/*
 *  Rasterizer_Shader.cpp
 *  Created by Clemens Unterkofler on 1/20/09.
 *  for Aleph One
 *
 *  http://www.gnu.org/licenses/gpl.html
 */

#include "OGL_Headers.h"

#include <iostream>

#include "Rasterizer_Shader.h"

#include "lightsource.h"
#include "media.h"
#include "player.h"
#include "weapons.h"
#include "AnimatedTextures.h"
#include "OGL_Faders.h"
#include "OGL_FBO.h"
#include "vr_openxr.h"
#include "OGL_Textures.h"
#include "OGL_Shader.h"
#include "ChaseCam.h"
#include "preferences.h"
#include "fades.h"
#include "screen.h"

#ifdef HAVE_OPENGL

#define MAXIMUM_VERTICES_PER_WORLD_POLYGON (MAXIMUM_VERTICES_PER_POLYGON+4)

const float FixedAngleToDegrees = 360.0/(float(FIXED_ONE)*float(FULL_CIRCLE));

const GLdouble kViewBaseMatrix[16] = {
	0,	0,	-1,	0,
	1,	0,	0,	0,
	0,	1,	0,	0,
	0,	0,	0,	1
};

const GLdouble kViewBaseMatrixInverse[16] = {
	0,	1,	0,	0,
	0,	0,	1,	0,
	-1,	0,	0,	0,
	0,	0,	0,	1
};

Rasterizer_Shader_Class::Rasterizer_Shader_Class() = default;
Rasterizer_Shader_Class::~Rasterizer_Shader_Class() = default;

void Rasterizer_Shader_Class::SetView(view_data& view) {
	OGL_SetView(view);
	
	if (view.screen_width != view_width || view.screen_height != view_height) {
		view_width = view.screen_width;
		view_height = view.screen_height;
		swapper.reset();
		swapper.reset(new FBOSwapper(view_width * MainScreenPixelScale(), view_height * MainScreenPixelScale(), false));
	}
	
	float aspect = view.screen_width / float(view.screen_height);
	float deg2rad = 8.0 * atan(1.0) / 360.0;
	float xtan, ytan;
	if (View_FOV_FixHorizontalNotVertical()) {
		xtan = tan(view.field_of_view * deg2rad / 2.0);
		ytan = xtan / aspect;
	} else {
		ytan = tan(view.field_of_view * deg2rad / 2.0) / 2.0;
		xtan = ytan * aspect;
	}
	
	// Adjust for view distortion during teleport effect
	ytan *= view.real_world_to_screen_y / double(view.world_to_screen_y);
	xtan *= view.real_world_to_screen_x / double(view.world_to_screen_x);

	double yaw = view.virtual_yaw * FixedAngleToDegrees;
	double pitch = view.virtual_pitch * FixedAngleToDegrees;
	pitch = (pitch > 180.0 ? pitch - 360.0 : pitch);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float nearVal = 64.0;
	float farVal = 128.0 * 1024.0;
	float x = xtan * nearVal;
	float y = ytan * nearVal;
	float yoff = view.mimic_sw_perspective ? tan(pitch * deg2rad) * nearVal : 0;
	glFrustum(-x, x, -y + yoff, y + yoff, nearVal, farVal);

	glMatrixMode(GL_MODELVIEW);

	// Build the classic view matrix and capture it as the landscape UV matrix for all variants.
	// The vertex shaders use landscapeInverseMatrix (not gl_ModelViewMatrix) for relDir so that
	// in VR we can supply a head-tracking-free version below without affecting gl_Position.
	glLoadMatrixd(kViewBaseMatrix);
	if (!view.mimic_sw_perspective)
		glRotated(pitch, 0.0, 1.0, 0.0);
	glRotated(-yaw, 0.0, 0.0, 1.0);
	glTranslated(-view.origin.x, -view.origin.y, -view.origin.z);

	{
		GLfloat landscapeMatrix[16];
		glGetFloatv(GL_MODELVIEW_MATRIX, landscapeMatrix);
		Shader *ls[] = {
			Shader::get(Shader::S_Landscape),
			Shader::get(Shader::S_LandscapeBloom),
			Shader::get(Shader::S_LandscapeInfravision),
			Shader::get(Shader::S_LandscapeSphere),
			Shader::get(Shader::S_LandscapeSphereBloom),
			Shader::get(Shader::S_LandscapeSphereInfravision),
		};
		for (auto s : ls) { s->enable(); s->setMatrix4(Shader::U_LandscapeInverseMatrix, landscapeMatrix); }
		Shader::disable();
	}

#if defined(__ANDROID__)
	if (VR_IsActive())
	{
		const int eye = VR_CurrentEye();
		const float WUperMetre = VR_Settings()->worldScaleWUM;
		// Effective eye-height reference: measured standing height (nominal before first recenter) minus the
		// Height Adjust pref. MUST match the reference used by VR_GetEyeZOffset() so the render camera and the
		// visibility-tree eye stay coupled (render eye Z = camZ + (headY - VR_EyeHeightM())*W == camZ +
		// VR_GetEyeZOffset()).
		const float eyeHeightM = VR_EyeHeightM();

		float vrProj[16];
		VR_GetEyeProjection(eye, vrProj, 0.05f, (128.0f * 1024.0f) / WUperMetre);

		// Mirror the flat renderer's teleport/fold distortion: world_to_screen_x/y are modified
		// by update_render_effect() relative to their "real" values during _render_effect_fold_in/out.
		// The flat path scales xtan/ytan by real/modified before glFrustum; apply the same ratio to
		// the VR projection matrix entries directly. proj[0]/proj[5] are the FOV scale factors;
		// proj[8]/proj[9] are the per-eye centre offsets — scaling them together preserves the IPD
		// convergence direction while changing the apparent FOV.
		if (view.effect != NONE &&
		    view.real_world_to_screen_x != 0 && view.real_world_to_screen_y != 0 &&
		    VR_Settings()->teleportDistortion)
		{
			const float sx = float(view.world_to_screen_x) / float(view.real_world_to_screen_x);
			const float sy = float(view.world_to_screen_y) / float(view.real_world_to_screen_y);
			vrProj[0] *= sx;  vrProj[8]  *= sx;
			vrProj[5] *= sy;  vrProj[9]  *= sy;
		}

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(vrProj);

		// Get VR eye matrix now; we need it for the landscape UV eye position.
		float vrView[16];   // eyeFromStage, metres
		VR_GetEyeViewMetres(eye, vrView);

		// Override landscape UV matrix: body-yaw rotation only, translate from the actual VR eye
		// position (not body centre). For close walls, the IPD/head-translation offset relative to
		// the body is significant — using body origin causes incorrect angular UV near walls.
		// vrView = [R|t] column-major (eye-from-stage, metres).
		// Eye pos in stage space = -R^T * t; convert to Marathon WU via inv(zUpToYUp) * WUperMetre:
		//   world.x = -stage.x * WUM, world.y = -stage.z * WUM, world.z = stage.y * WUM
		{
			float esx = -(vrView[0]*vrView[12] + vrView[4]*vrView[13] + vrView[8]*vrView[14]);
			float esy = -(vrView[1]*vrView[12] + vrView[5]*vrView[13] + vrView[9]*vrView[14]);
			float esz = -(vrView[2]*vrView[12] + vrView[6]*vrView[13] + vrView[10]*vrView[14]);
			float eyeX = -esx * WUperMetre;
			float eyeY = -esz * WUperMetre;
			float eyeZ =  esy * WUperMetre;
			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixd(kViewBaseMatrix);
			glRotated(-yaw, 0.0, 0.0, 1.0);

			glTranslated(-eyeX, -eyeY, -eyeZ);
			GLfloat vrLandscapeMatrix[16];
			glGetFloatv(GL_MODELVIEW_MATRIX, vrLandscapeMatrix);

			// bodyYawFromEye = kViewBase * zUpToYUp^T * mat3(vrView)^T  (no body yaw).
			// Yaw is in the rendering matrix so eyeDir already encodes snap-turn; adding yaw
			// here would double-count it. Analytical: col j = (-vrView[8+j], vrView[4+j], vrView[j]).
			float bodyYawFromEye[9] = {
				-vrView[8],  vrView[4],  vrView[0],
				-vrView[9],  vrView[5],  vrView[1],
				-vrView[10], vrView[6],  vrView[2]
			};

			// Inverse-projection: eye.x = ndc.x/vrProj[0] + vrProj[8]/vrProj[0]
			float projXScale = 1.0f / vrProj[0];
			float projYScale = 1.0f / vrProj[5];
			float projXOff   = vrProj[8]  / vrProj[0];
			float projYOff   = vrProj[9]  / vrProj[5];

			GLint vp[4];
			glGetIntegerv(GL_VIEWPORT, vp);

			Shader *ls[] = {
				Shader::get(Shader::S_Landscape),
				Shader::get(Shader::S_LandscapeBloom),
				Shader::get(Shader::S_LandscapeInfravision),
				Shader::get(Shader::S_LandscapeSphere),
				Shader::get(Shader::S_LandscapeSphereBloom),
				Shader::get(Shader::S_LandscapeSphereInfravision),
			};
			for (auto s : ls) {
				s->enable();
				s->setMatrix4(Shader::U_LandscapeInverseMatrix, vrLandscapeMatrix);
				s->setFloat(Shader::U_VrMode, 1.0f);
				s->setMatrix3(Shader::U_BodyYawFromEye, bodyYawFromEye);
				s->setFloat(Shader::U_ProjXScale, projXScale);
				s->setFloat(Shader::U_ProjYScale, projYScale);
				s->setFloat(Shader::U_ProjXOff,   projXOff);
				s->setFloat(Shader::U_ProjYOff,   projYOff);
				s->setFloat(Shader::U_VpX, (float)vp[0]);
				s->setFloat(Shader::U_VpY, (float)vp[1]);
				s->setFloat(Shader::U_VpW, (float)vp[2]);
				s->setFloat(Shader::U_VpH, (float)vp[3]);
			}
			Shader::disable();
		}
		// world(Marathon, Z-up, left-handed yaw) -> stage(Y-up): (x,y,z) -> (-x, z, -y). The negated
		// X makes this det=-1 to match the engine's kViewBaseMatrix handedness (else the world is
		// mirrored). With det=-1 the engine's glFrontFace(GL_CW) culling is correct (no shim flip).
		static const float zUpToYUp[16] = { -1,0,0,0,  0,0,-1,0,  0,1,0,0,  0,0,0,1 };
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(vrView);
		glScalef(1.0f / WUperMetre, 1.0f / WUperMetre, 1.0f / WUperMetre);
		glMultMatrixf(zUpToYUp);
		glRotated(-yaw, 0.0, 0.0, 1.0);
		// view.origin (int16) drives the visibility tree + wall clamp in integer map space, but its 1-WU
		// (~2mm) quantisation -- and the nonlinear clamp near walls -- makes close walls snap between a
		// couple of positions as the HMD dithers while leaning. screen.cpp apply_vr_view_offsets publishes
		// a CONTINUOUS float render camera (live body+lean plus a low-passed wall-clamp pushback); use it
		// for the horizontal camera so the render is smooth while the vis tree keeps the integer origin.
		// Z is made continuous just below by subtracting the float eye-Z offset (head height rides in via
		// vrView). Falls back to the int16 origin if the float camera hasn't been published yet.
		// Continuous float render camera (all 3 axes) published by apply_vr_view_offsets. X/Y = body+lean
		// with the wall-clamp pushback low-passed; Z = interpolated BODY height only. The live head height
		// (lean + duck) rides in entirely via vrView above, so we do NOT re-add eye-Z here -- that keeps
		// the vertical continuous and free of the cross-call int16 read mismatch that snapped 1 WU. The
		// int16 view.origin still drives the visibility tree/clamp; it's just not the render camera.
		// Falls back to the int16 origin (minus eyeHeight) if the float camera hasn't been published yet.
		double camX = view.origin.x, camY = view.origin.y, camZ = view.origin.z;
		{
			float rcx = 0.0f, rcy = 0.0f, rcz = 0.0f;
			if (VR_GetRenderCamera(&rcx, &rcy, &rcz)) { camX = rcx; camY = rcy; camZ = rcz; }
		}
		glTranslated(-camX, -camY, -(camZ - eyeHeightM * WUperMetre));
	}
#endif
}

void Rasterizer_Shader_Class::setupGL()
{
	view_width = 0;
	view_height = 0;
	swapper.reset();
	
	smear_the_void = false;
	OGL_ConfigureData& ConfigureData = Get_OGL_ConfigureData();
	if (!TEST_FLAG(ConfigureData.Flags,OGL_Flag_VoidColor))
		smear_the_void = true;
}

void Rasterizer_Shader_Class::Begin()
{
	Rasterizer_OGL_Class::Begin();
#if defined(__ANDROID__)
	if (VR_IsActive()) {
		// Render the world straight into the eye swapchain FBO (bound + cleared by VR_BeginEye) at
		// eye resolution. Skip the FBOSwapper: its size/aspect differ from the eye (causing the
		// stretch/smear) and it blits to the screen, not the eye buffer. (No gamma/bloom in VR yet.)
		glViewport(0, 0, VR_EyeWidth(), VR_EyeHeight());
		return;
	}
#endif
	swapper->activate();
	if (smear_the_void)
		swapper->current_contents().draw_full();
}

void Rasterizer_Shader_Class::End()
{
#if defined(__ANDROID__)
	if (VR_IsActive()) {
		// The world is already in the bound eye FBO; no swapper resolve / gamma / screen blit.
		// (Brightness is applied per-fragment in the shader via a1_Brightness, set by the shim.)
		Rasterizer_OGL_Class::End();
		return;
	}
#endif
	swapper->deactivate();
	swapper->swap();
	
	float gamma_adj = get_actual_gamma_adjust(graphics_preferences->screen_mode.gamma_level);
	if (gamma_adj < 0.99f || gamma_adj > 1.01f) {
		Shader *s = Shader::get(Shader::S_Gamma);
		s->enable();
		s->setFloat(Shader::U_GammaAdjust, gamma_adj);
	}
#if defined(__ANDROID__)
	// VR: the FBOSwapper's deactivate left the pbuffer/default FB bound; the final composite must
	// target the bound eye swapchain framebuffer at eye resolution.
	if (VR_IsActive()) {
		glBindFramebuffer(GL_FRAMEBUFFER, VR_CurrentEyeFramebuffer());
		glViewport(0, 0, VR_EyeWidth(), VR_EyeHeight());
	}
#endif
	swapper->draw();
	Shader::disable();
	
	SetForeground();
	glColor3f(0, 0, 0);
	OGL_RenderFrame(0, 0, view_width, view_height, 1);
	
	Rasterizer_OGL_Class::End();
}

#endif