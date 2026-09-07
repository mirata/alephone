/*
 *  vr_openxr.cpp  --  OpenXR (Meta Quest) VR backend for Aleph One. See vr_openxr.h.
 *
 *  SDL-hosted: binds OpenXR to SDL's EGL context rather than creating its own (the QuestZDoom/TBXR
 *  approach owns the activity+thread+EGL, which collides with Aleph One's SDL platform layer).
 *  Adapted from TBXR_Common.cpp (DrBeef), restructured to run on SDL's context + main thread.
 *
 *  Increment 1: bring up session + swapchains + the stereo frame loop and render a head-tracked
 *  colored room to both eyes. This proves the whole immersive pipeline (session lifecycle, per-eye
 *  swapchain FBOs, projection/view matrices, head tracking, compositor submit) before the Marathon
 *  scene is wired in per-eye (Increment 2).
 */

#if defined(__ANDROID__)

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <android/log.h>
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <cmath>

#include "vr_openxr.h"
#include "world.h"           // world_point3d (sprite-view-origin API)
#include "Logging.h"         // logWarning -> persistent on-device log (panel-placement diagnostic)
#include "sdl_fonts.h"       // font_info (on-screen keyboard label rendering)
#include "screen_drawing.h"  // draw_text / text_width
#include "sdl_dialogs.h"     // get_theme_font / DEFAULT_WIDGET
// NB: do NOT #include "Console.h" here -- it drags in preferences.h -> OGL_Setup.h -> OGL_Headers.h ->
// gl_es_compat.h, whose `#define glEnable a1es_enable` (etc.) macros would hijack this file's
// shader-based GL calls into fixed-function emulation and BLACK-OUT all VR rendering. Get the console
// line through the thin C accessor VR_GetConsoleLine() (declared in vr_openxr.h, defined in Console.cpp).

#define A1VR_LOG(...) __android_log_print(ANDROID_LOG_INFO, "A1VR", __VA_ARGS__)
#define XR_CHECK(call) do { XrResult _r = (call); if (XR_FAILED(_r)) A1VR_LOG("%s -> %d", #call, _r); } while (0)

// EXT_sRGB_write_control: lets us store our already-sRGB content verbatim into the sRGB eye swapchain
// (without GL re-encoding linear->sRGB, which over-brightens the legacy renderer's output + the 2D UI).
#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

namespace {

constexpr int kEyes = 2;

XrInstance  s_instance = XR_NULL_HANDLE;
XrSystemId  s_systemId = XR_NULL_SYSTEM_ID;
bool        s_active   = false;          // instance + system created

// Session-level state (created lazily once the GL context exists).
XrSession      s_session = XR_NULL_HANDLE;

// XR_FB_display_refresh_rate. Enabled only when the runtime advertises it -- asking xrCreateInstance
// for an unavailable extension makes the whole call fail, so it must be probed first.
bool s_hasRefreshRateExt = false;
PFN_xrEnumerateDisplayRefreshRatesFB s_pfnEnumRates = nullptr;
PFN_xrGetDisplayRefreshRateFB        s_pfnGetRate   = nullptr;
PFN_xrRequestDisplayRefreshRateFB    s_pfnReqRate   = nullptr;
XrSpace        s_stageSpace = XR_NULL_HANDLE;   // world reference (metres)
XrSpace        s_headSpace  = XR_NULL_HANDLE;   // VIEW space (for locating eyes)
XrSessionState s_sessionState = XR_SESSION_STATE_UNKNOWN;
bool           s_sessionRunning = false;
bool           s_sessionCreated = false;

XrViewConfigurationView s_viewConfig[kEyes] = {};
int s_eyeW = 0, s_eyeH = 0;

struct EyeFB {
	XrSwapchain                  swapchain = XR_NULL_HANDLE;
	uint32_t                     imageCount = 0;
	XrSwapchainImageOpenGLESKHR* images = nullptr;
	GLuint*                      fbos = nullptr;
	GLuint                       depth = 0;   // shared depth renderbuffer (eye-sized)
};
EyeFB s_eye[kEyes];

XrFrameState s_frameState = {};
XrView       s_views[kEyes] = {};
XrPosef      s_stageFromHead = {};
bool         s_headPoseValid = false;   // s_stageFromHead orientation has been located at least once

// Room-scale head tracking: s_lastHead* is the "absorbed" head origin (stage metres, horizontal) --
// the point the player body has been moved up to. The render uses the head/eye position RELATIVE to
// this (the residual), and each physics tick the residual is fed to the body and this advances to the
// current head, so the body follows the head without the render double-counting the movement.
float        s_lastHeadX = 0, s_lastHeadZ = 0;
bool         s_headLatchInit = false;
float        s_headMoveX = 0, s_headMoveY = 0;   // latched per-tick body delta, world units (map x,y)
// The head stage position at the PREVIOUS tick's latch (s_lastHead is the CURRENT tick's). The render
// interpolates BETWEEN these two to build a head reference that lags the live head exactly as the
// interpolated body lags -- subtracting it from the live head removes the interpolation lag so head-walk
// appears once and continuously (see VR_GetHeadOffsetInterp). Seeded to s_lastHead so t-lerp starts flat.
float        s_prevHeadX = 0, s_prevHeadZ = 0;
// Vertical recenter reference (stage metres): the player's MEASURED standing eye height, captured at
// recenter alongside the horizontal reference. QuestZDoom-style -- vertical placement is relative to the
// player's own stance rather than a fixed eyeHeightM assumption vs the absolute stage floor. This makes
// the in-game height immune to per-headset floor-calibration errors and body-height differences (which
// otherwise shrank/grew players who differed from the 1.6 m assumption), and works even if STAGE falls
// back to LOCAL (no floor at all), since nothing depends on the absolute floor any more. 0 until measured.
float        s_standingEyeHeightM   = 0.0f;
bool         s_standingEyeHeightInit = false;
// Set when the runtime recenters our world reference space -- i.e. the user HELD the Meta/Quest button
// (a system recenter). That button is reserved by the OS so we can't read it as an input, but it raises
// XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING, which we treat as a recenter request (as QuestZDoom's
// TBXR does). Two independent one-shot flags: the gameplay flag is consumed by the game tick (vbl.cpp) to
// run the same recenter as the controller Recenter action (yaw-to-facing + height/lean recapture); the
// panel flag is consumed by the 2D-screen path to re-anchor the world-locked menu/terminal/keyboard panel
// to wherever the player is now facing. Our recenter recaptures from the CURRENT head pose, so it works
// regardless of which reference space (STAGE or LOCAL) the runtime actually moved -- the event is only the
// trigger.
bool         s_systemRecenterPending = false;
bool         s_panelRecenterPending  = false;
// The player's in-game eye height above the floor (world units), fed in from the physics each tick. With
// the measured standing eye height it sets a LIFE-SIZE world scale (worldScaleWUM = this / standingHeight)
// so the player is rendered at their true height and reaching the real floor lands on the game floor.
// Nominal (0.6*WORLD_ONE) until the physics reports the real value for the running scenario.
float        s_gameEyeHeightWU = 614.0f;

// Local player's actual collision radius (world units), fed live from the physics model. Drives the
// head-lean deadzone (a fraction of it); 0 until physics reports it, which just means no give yet.
float        s_playerRadiusWU = 0.0f;

// Engine-owned headless EGL context (no window surface).
EGLDisplay s_eglDpy  = EGL_NO_DISPLAY;
EGLContext s_eglCtx  = EGL_NO_CONTEXT;
EGLSurface s_eglSurf = EGL_NO_SURFACE;
bool       s_eglReady = false;

int        s_srgbWriteControl = -1;   // -1 unknown, 0 absent, 1 present (EXT_sRGB_write_control)

bool hasGLExtension(const char* name) {
	GLint n = 0; glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (GLint i = 0; i < n; ++i) {
		const char* e = (const char*)glGetStringi(GL_EXTENSIONS, i);
		if (e && std::strcmp(e, name) == 0) return true;
	}
	return false;
}

// --- test-scene GL objects ---
GLuint s_prog = 0, s_vao = 0, s_vbo = 0;
GLint  s_uProj = -1, s_uView = -1;
int    s_vertCount = 0;

// ------------------------------------------------------------------ math ----
// Column-major 4x4, GL convention.
void mat_identity(float* m) { std::memset(m, 0, 16 * sizeof(float)); m[0]=m[5]=m[10]=m[15]=1.0f; }

void mat_mul(float* out, const float* a, const float* b) {
	float r[16];
	for (int c = 0; c < 4; ++c)
		for (int row = 0; row < 4; ++row) {
			float s = 0;
			for (int k = 0; k < 4; ++k) s += a[k*4+row] * b[c*4+k];
			r[c*4+row] = s;
		}
	std::memcpy(out, r, sizeof r);
}

// OpenXR off-axis projection (tangent-based) -> column-major GL matrix, clip z in [-1,1].
void proj_from_fov(float* m, const XrFovf& fov, float n, float f) {
	const float l = std::tan(fov.angleLeft),  r = std::tan(fov.angleRight);
	const float d = std::tan(fov.angleDown),  u = std::tan(fov.angleUp);
	const float w = r - l, h = u - d;
	std::memset(m, 0, 16 * sizeof(float));
	m[0]  = 2.0f / w;
	m[5]  = 2.0f / h;
	m[8]  = (r + l) / w;
	m[9]  = (u + d) / h;
	m[10] = -(f + n) / (f - n);
	m[11] = -1.0f;
	m[14] = -(2.0f * f * n) / (f - n);
}

// Rigid transform from an XrPosef (quat + translation) -> column-major matrix.
void mat_from_pose(float* m, const XrPosef& p) {
	const float x = p.orientation.x, y = p.orientation.y, z = p.orientation.z, w = p.orientation.w;
	const float xx=x*x, yy=y*y, zz=z*z, xy=x*y, xz=x*z, yz=y*z, wx=w*x, wy=w*y, wz=w*z;
	m[0]=1-2*(yy+zz); m[1]=2*(xy+wz);   m[2]=2*(xz-wy);   m[3]=0;
	m[4]=2*(xy-wz);   m[5]=1-2*(xx+zz); m[6]=2*(yz+wx);   m[7]=0;
	m[8]=2*(xz+wy);   m[9]=2*(yz-wx);   m[10]=1-2*(xx+yy);m[11]=0;
	m[12]=p.position.x; m[13]=p.position.y; m[14]=p.position.z; m[15]=1;
}

// Inverse of a rigid (rotation+translation) matrix.
void mat_rigid_inverse(float* out, const float* m) {
	// transpose rotation
	out[0]=m[0]; out[1]=m[4]; out[2]=m[8];  out[3]=0;
	out[4]=m[1]; out[5]=m[5]; out[6]=m[9];  out[7]=0;
	out[8]=m[2]; out[9]=m[6]; out[10]=m[10];out[11]=0;
	// -R^T * t
	const float tx=m[12], ty=m[13], tz=m[14];
	out[12] = -(out[0]*tx + out[4]*ty + out[8]*tz);
	out[13] = -(out[1]*tx + out[5]*ty + out[9]*tz);
	out[14] = -(out[2]*tx + out[6]*ty + out[10]*tz);
	out[15] = 1;
}

XrPosef pose_mul(const XrPosef& a, const XrPosef& b) {
	// a * b for quaternion+translation poses
	XrPosef o;
	const auto& qa = a.orientation; const auto& qb = b.orientation;
	o.orientation.w = qa.w*qb.w - qa.x*qb.x - qa.y*qb.y - qa.z*qb.z;
	o.orientation.x = qa.w*qb.x + qa.x*qb.w + qa.y*qb.z - qa.z*qb.y;
	o.orientation.y = qa.w*qb.y - qa.x*qb.z + qa.y*qb.w + qa.z*qb.x;
	o.orientation.z = qa.w*qb.z + qa.x*qb.y - qa.y*qb.x + qa.z*qb.w;
	// rotate b.position by a.orientation, add a.position
	float m[16]; mat_from_pose(m, a);
	const float px=b.position.x, py=b.position.y, pz=b.position.z;
	o.position.x = m[0]*px + m[4]*py + m[8]*pz  + a.position.x;
	o.position.y = m[1]*px + m[5]*py + m[9]*pz  + a.position.y;
	o.position.z = m[2]*px + m[6]*py + m[10]*pz + a.position.z;
	return o;
}

// ----------------------------------------------------------- test scene ----
GLuint compile(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) { char log[512]; glGetShaderInfoLog(s, sizeof log, nullptr, log); A1VR_LOG("test shader: %s", log); }
	return s;
}

void buildTestScene() {
	if (s_prog) return;
	static const char* vs =
		"#version 300 es\n"
		"layout(location=0) in vec3 aPos;\n"
		"layout(location=1) in vec3 aColor;\n"
		"uniform mat4 uProj; uniform mat4 uView;\n"
		"out vec3 vColor;\n"
		"void main(){ gl_Position = uProj * uView * vec4(aPos,1.0); vColor = aColor; }\n";
	static const char* fs =
		"#version 300 es\n"
		"precision mediump float;\n"
		"in vec3 vColor; out vec4 o;\n"
		"void main(){ o = vec4(vColor,1.0); }\n";
	GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
	s_prog = glCreateProgram();
	glAttachShader(s_prog, v); glAttachShader(s_prog, f); glLinkProgram(s_prog);
	glDeleteShader(v); glDeleteShader(f);
	s_uProj = glGetUniformLocation(s_prog, "uProj");
	s_uView = glGetUniformLocation(s_prog, "uView");

	// A 6m room (metres, Y up) centred on the player at eye height ~1.6m, each wall a distinct
	// colour so head rotation is unambiguous; floor a grid-ish dark, ceiling light.
	const float R = 3.0f, F = 0.0f, C = 3.0f;     // half-extent, floor y, ceiling y
	auto quad = [&](float* p, const float* a, const float* b, const float* c, const float* d, float r, float g, float bl) {
		const float verts[6][3] = { {a[0],a[1],a[2]},{b[0],b[1],b[2]},{c[0],c[1],c[2]},
									{a[0],a[1],a[2]},{c[0],c[1],c[2]},{d[0],d[1],d[2]} };
		for (int i = 0; i < 6; ++i) { *p++=verts[i][0]; *p++=verts[i][1]; *p++=verts[i][2]; *p++=r; *p++=g; *p++=bl; }
	};
	float data[6 * 6 * 6]; float* p = data;
	const float fnl[3]={-R,F,-R}, fnr[3]={ R,F,-R}, ffr[3]={ R,F, R}, ffl[3]={-R,F, R};
	const float cnl[3]={-R,C,-R}, cnr[3]={ R,C,-R}, cfr[3]={ R,C, R}, cfl[3]={-R,C, R};
	quad(p+0*36,  fnl,fnr,ffr,ffl, 0.12f,0.12f,0.16f);  // floor
	quad(p+1*36,  cnl,cfl,cfr,cnr, 0.55f,0.55f,0.60f);  // ceiling
	quad(p+2*36,  fnl,cnl,cnr,fnr, 0.80f,0.20f,0.20f);  // -Z wall (red, front)
	quad(p+3*36,  ffr,cfr,cfl,ffl, 0.20f,0.80f,0.20f);  // +Z wall (green, behind)
	quad(p+4*36,  ffl,cfl,cnl,fnl, 0.20f,0.30f,0.85f);  // -X wall (blue, left)
	quad(p+5*36,  fnr,cnr,cfr,ffr, 0.85f,0.75f,0.20f);  // +X wall (yellow, right)
	s_vertCount = 6 * 6;

	glGenVertexArrays(1, &s_vao);
	glGenBuffers(1, &s_vbo);
	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
	glBindVertexArray(0);
}

void drawTestScene(const float* proj, const float* view) {
	buildTestScene();
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);   // we're inside the room
	glUseProgram(s_prog);
	glUniformMatrix4fv(s_uProj, 1, GL_FALSE, proj);
	glUniformMatrix4fv(s_uView, 1, GL_FALSE, view);
	glBindVertexArray(s_vao);
	glDrawArrays(GL_TRIANGLES, 0, s_vertCount);
	glBindVertexArray(0);
}

// --------------------------------------------------------- session setup ----
EGLConfig currentEglConfig(EGLDisplay dpy, EGLContext ctx) {
	EGLint cfgId = 0;
	eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &cfgId);
	EGLConfig cfg = nullptr; EGLint n = 0;
	const EGLint attribs[] = { EGL_CONFIG_ID, cfgId, EGL_NONE };
	eglChooseConfig(dpy, attribs, &cfg, 1, &n);
	return cfg;
}

bool createSwapchains() {
	uint32_t count = 0;
	for (int i = 0; i < kEyes; ++i) s_viewConfig[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	XR_CHECK(xrEnumerateViewConfigurationViews(s_instance, s_systemId,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, kEyes, &count, s_viewConfig));
	s_eyeW = s_viewConfig[0].recommendedImageRectWidth;
	s_eyeH = s_viewConfig[0].recommendedImageRectHeight;
	A1VR_LOG("eye resolution %dx%d", s_eyeW, s_eyeH);

	// Pick an sRGB colour format (GL_SRGB8_ALPHA8), matching TBXR/QuestZDoom. The Quest compositor
	// interprets the swapchain in this colour space; handing it a linear RGBA8 made the world far too
	// bright (the runtime applied an extra linear->sRGB brighten). With an sRGB swapchain the
	// compositor samples correctly. Fall back to RGBA8 only if sRGB isn't offered.
	uint32_t fmtCount = 0;
	xrEnumerateSwapchainFormats(s_session, 0, &fmtCount, nullptr);
	int64_t* fmts = new int64_t[fmtCount];
	xrEnumerateSwapchainFormats(s_session, fmtCount, &fmtCount, fmts);
	int64_t chosen = fmts[0];
	bool haveSrgb = false, haveRgba8 = false;
	for (uint32_t i = 0; i < fmtCount; ++i) {
		if (fmts[i] == GL_SRGB8_ALPHA8) haveSrgb = true;
		if (fmts[i] == GL_RGBA8)        haveRgba8 = true;
	}
	if (haveSrgb)       chosen = GL_SRGB8_ALPHA8;
	else if (haveRgba8) chosen = GL_RGBA8;
	delete[] fmts;

	for (int e = 0; e < kEyes; ++e) {
		XrSwapchainCreateInfo ci = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
		ci.format = chosen;
		ci.sampleCount = 1;
		ci.width = s_eyeW; ci.height = s_eyeH;
		ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
		if (XR_FAILED(xrCreateSwapchain(s_session, &ci, &s_eye[e].swapchain))) { A1VR_LOG("xrCreateSwapchain eye %d failed", e); return false; }

		xrEnumerateSwapchainImages(s_eye[e].swapchain, 0, &s_eye[e].imageCount, nullptr);
		s_eye[e].images = new XrSwapchainImageOpenGLESKHR[s_eye[e].imageCount];
		for (uint32_t i = 0; i < s_eye[e].imageCount; ++i) s_eye[e].images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
		xrEnumerateSwapchainImages(s_eye[e].swapchain, s_eye[e].imageCount, &s_eye[e].imageCount,
								   (XrSwapchainImageBaseHeader*)s_eye[e].images);

		glGenRenderbuffers(1, &s_eye[e].depth);
		glBindRenderbuffer(GL_RENDERBUFFER, s_eye[e].depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, s_eyeW, s_eyeH);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		s_eye[e].fbos = new GLuint[s_eye[e].imageCount];
		glGenFramebuffers(s_eye[e].imageCount, s_eye[e].fbos);
		for (uint32_t i = 0; i < s_eye[e].imageCount; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, s_eye[e].fbos[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_eye[e].images[i].image, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_eye[e].depth);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				A1VR_LOG("eye %d fbo %u incomplete", e, i);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	return true;
}

void initActions();   // defined below (input action set); attached once the session exists

bool startSession() {
	if (s_sessionCreated) return true;

	EGLDisplay dpy = eglGetCurrentDisplay();
	EGLContext ctx = eglGetCurrentContext();
	if (dpy == EGL_NO_DISPLAY || ctx == EGL_NO_CONTEXT) {
		// SDL's GL context isn't current yet (window not created); try again next frame.
		return false;
	}

	// OpenXR requires the graphics requirements call before creating the session.
	PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnReq = nullptr;
	xrGetInstanceProcAddr(s_instance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnReq);
	if (pfnReq) {
		XrGraphicsRequirementsOpenGLESKHR req = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR };
		pfnReq(s_instance, s_systemId, &req);
	}

	XrGraphicsBindingOpenGLESAndroidKHR gb = { XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR };
	gb.display = dpy;
	gb.config  = currentEglConfig(dpy, ctx);
	gb.context = ctx;

	XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO };
	sci.next = &gb;
	sci.systemId = s_systemId;
	if (XR_FAILED(xrCreateSession(s_instance, &sci, &s_session))) { A1VR_LOG("xrCreateSession failed"); return false; }

	// Display refresh rate. Quest hands every app 72 Hz; anything higher has to be asked for.
	if (s_hasRefreshRateExt) {
		xrGetInstanceProcAddr(s_instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)&s_pfnEnumRates);
		xrGetInstanceProcAddr(s_instance, "xrGetDisplayRefreshRateFB",       (PFN_xrVoidFunction*)&s_pfnGetRate);
		xrGetInstanceProcAddr(s_instance, "xrRequestDisplayRefreshRateFB",   (PFN_xrVoidFunction*)&s_pfnReqRate);
		float rates[32];
		const int n = VR_GetRefreshRates(rates, 32);
		char buf[256]; int off = 0;
		for (int i = 0; i < n && off < (int)sizeof(buf) - 8; ++i)
			off += std::snprintf(buf + off, sizeof(buf) - off, "%s%.0f", i ? "/" : "", rates[i]);
		if (!n) std::snprintf(buf, sizeof buf, "(none reported)");
		A1VR_LOG("VR: display refresh rates supported: %s Hz; current %.0f Hz", buf, VR_GetRefreshRate());
		// Always run at the highest rate the runtime offers. Quest hands every app 72 Hz by default;
		// there is deliberately no preference for this.
		float want = 0.0f;
		for (int i = 0; i < n; ++i) if (rates[i] > want) want = rates[i];
		if (want > 0.0f)
			VR_SetRefreshRate(want);
	}

	XrReferenceSpaceCreateInfo head = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	head.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	head.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(xrCreateReferenceSpace(s_session, &head, &s_headSpace));

	// Prefer STAGE (floor-level world); fall back to LOCAL.
	XrReferenceSpaceCreateInfo stage = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	stage.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	stage.poseInReferenceSpace.orientation.w = 1.0f;
	if (XR_FAILED(xrCreateReferenceSpace(s_session, &stage, &s_stageSpace))) {
		stage.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		XR_CHECK(xrCreateReferenceSpace(s_session, &stage, &s_stageSpace));
		A1VR_LOG("using LOCAL space (no stage)");
	}

	if (!createSwapchains()) return false;
	initActions();
	s_sessionCreated = true;
	A1VR_LOG("session + swapchains created");
	return true;
}

void pollEvents() {
	XrEventDataBuffer ev = { XR_TYPE_EVENT_DATA_BUFFER };
	while (xrPollEvent(s_instance, &ev) == XR_SUCCESS) {
		if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
			auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
			s_sessionState = ssc->state;
			A1VR_LOG("session state -> %d", s_sessionState);
			if (s_sessionState == XR_SESSION_STATE_READY) {
				XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO };
				bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				XR_CHECK(xrBeginSession(s_session, &bi));
				s_sessionRunning = true;
			} else if (s_sessionState == XR_SESSION_STATE_STOPPING) {
				XR_CHECK(xrEndSession(s_session));
				s_sessionRunning = false;
			}
		} else if (ev.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
			// The runtime re-centered our world reference (Meta/Quest button hold, or a system recenter).
			// Treat it as a recenter request: gameplay recenters (yaw/height/lean) via the game tick, and the
			// world-locked 2D panel re-anchors to the player's new facing. Not filtered by referenceSpaceType
			// -- our offset-based recenter recaptures from the current head pose regardless of which space
			// moved (matches QuestZDoom's TBXR handling).
			auto* rc = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&ev);
			A1VR_LOG("reference space change (recenter): space type %d", rc->referenceSpaceType);
			s_systemRecenterPending = true;
			s_panelRecenterPending  = true;
		}
		ev.type = XR_TYPE_EVENT_DATA_BUFFER;
	}
}

} // namespace

// ============================================================================
extern "C" bool VR_InitOpenXR(void)
{
	if (s_active) return true;

	JNIEnv* env       = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
	jobject actLocal  = static_cast<jobject>(SDL_AndroidGetActivity());
	JavaVM* vm        = nullptr;
	if (env) env->GetJavaVM(&vm);
	if (!vm || !actLocal) { A1VR_LOG("VR_InitOpenXR: no JavaVM/activity"); return false; }

	// SDL_AndroidGetActivity returns a LOCAL ref; hold a GLOBAL ref for the runtime's lifetime.
	// IMPORTANT: this must be the actual Activity (not the application Context) -- the runtime uses
	// XrInstanceCreateInfoAndroidKHR.applicationActivity to track the activity lifecycle; passing the
	// app context instead leaves the session stuck in XR_SESSION_STATE_IDLE.
	static jobject s_activityGlobal = nullptr;
	if (!s_activityGlobal) s_activityGlobal = env->NewGlobalRef(actLocal);
	jobject activity = s_activityGlobal;

	PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
	xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
						  reinterpret_cast<PFN_xrVoidFunction*>(&xrInitializeLoaderKHR));
	if (xrInitializeLoaderKHR) {
		XrLoaderInitInfoAndroidKHR lii{};
		lii.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
		lii.applicationVM = vm;
		lii.applicationContext = activity;
		XrResult lr = xrInitializeLoaderKHR(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&lii));
		if (XR_FAILED(lr)) A1VR_LOG("VR_InitOpenXR: xrInitializeLoaderKHR failed (%d)", lr);
	} else {
		A1VR_LOG("VR_InitOpenXR: xrInitializeLoaderKHR not found");
	}

	// Probe for optional extensions FIRST: xrCreateInstance fails outright if asked for one the
	// runtime doesn't have, so an unconditional request would take the whole VR mode down on any
	// headset/runtime lacking it.
	s_hasRefreshRateExt = false;
	{
		uint32_t extCount = 0;
		if (XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr)) && extCount) {
			std::vector<XrExtensionProperties> props(extCount, { XR_TYPE_EXTENSION_PROPERTIES });
			if (XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, props.data()))) {
				for (uint32_t i = 0; i < extCount; ++i)
					if (!std::strcmp(props[i].extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))
						s_hasRefreshRateExt = true;
			}
		}
	}
	A1VR_LOG("VR_InitOpenXR: %s %s", XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
		s_hasRefreshRateExt ? "available" : "NOT available (locked to the runtime default rate)");

	std::vector<const char*> extensions = {
		XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
		XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
	};
	if (s_hasRefreshRateExt)
		extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
	XrInstanceCreateInfoAndroidKHR androidInfo{};
	androidInfo.type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR;
	androidInfo.applicationVM = vm;
	androidInfo.applicationActivity = activity;

	XrInstanceCreateInfo ici{};
	ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
	ici.next = &androidInfo;
	std::strcpy(ici.applicationInfo.applicationName, "Aleph One");
	std::strcpy(ici.applicationInfo.engineName, "Aleph One");
	ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	ici.enabledExtensionCount = (uint32_t)extensions.size();
	ici.enabledExtensionNames = extensions.data();

	if (XR_FAILED(xrCreateInstance(&ici, &s_instance))) {
		A1VR_LOG("VR_InitOpenXR: xrCreateInstance failed (runtime unavailable?)");
		return false;
	}
	XrInstanceProperties ip{}; ip.type = XR_TYPE_INSTANCE_PROPERTIES;
	xrGetInstanceProperties(s_instance, &ip);
	A1VR_LOG("VR_InitOpenXR: runtime '%s'", ip.runtimeName);

	XrSystemGetInfo sgi{}; sgi.type = XR_TYPE_SYSTEM_GET_INFO;
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	if (XR_FAILED(xrGetSystem(s_instance, &sgi, &s_systemId))) { A1VR_LOG("VR_InitOpenXR: xrGetSystem failed"); return false; }

	s_active = true;
	A1VR_LOG("VR_InitOpenXR: OK");
	return true;
}

extern "C" bool VR_IsActive(void) { return s_active; }

// ---- Display refresh rate -----------------------------------------------------------------------
// All three degrade to "unsupported" (0 / false) when the runtime lacks XR_FB_display_refresh_rate,
// in which case the app simply stays on whatever the runtime picked (72 Hz on Quest).
extern "C" int VR_GetRefreshRates(float* out, int maxCount)
{
	if (!s_pfnEnumRates || s_session == XR_NULL_HANDLE || !out || maxCount <= 0) return 0;
	uint32_t count = 0;
	if (XR_FAILED(s_pfnEnumRates(s_session, 0, &count, nullptr)) || !count) return 0;
	std::vector<float> rates(count, 0.0f);
	if (XR_FAILED(s_pfnEnumRates(s_session, count, &count, rates.data()))) return 0;
	const int n = (int)count < maxCount ? (int)count : maxCount;
	for (int i = 0; i < n; ++i) out[i] = rates[i];
	return n;
}

extern "C" float VR_GetRefreshRate(void)
{
	if (!s_pfnGetRate || s_session == XR_NULL_HANDLE) return 0.0f;
	float hz = 0.0f;
	if (XR_FAILED(s_pfnGetRate(s_session, &hz))) return 0.0f;
	return hz;
}

extern "C" bool VR_SetRefreshRate(float hz)
{
	if (!s_pfnReqRate || s_session == XR_NULL_HANDLE || hz <= 0.0f) return false;
	const XrResult r = s_pfnReqRate(s_session, hz);
	if (XR_FAILED(r)) {
		A1VR_LOG("VR: display refresh rate %.0f Hz REFUSED (%d) -- staying at %.0f Hz", hz, r, VR_GetRefreshRate());
		return false;
	}
	A1VR_LOG("VR: display refresh rate set to %.0f Hz", hz);
	return true;
}

namespace {
	vr_settings_t s_settings = {
		/* screenDistanceM */ 2.5f,
		/* screenHeightM   */ 2.0f,
		/* worldScaleWUM   */ 512.0f,
		/* heightAdjustM   */ 0.0f,
		/* snapTurn        */ 1,      // snap by default (most comfortable); smooth is a prefs toggle
		/* turnDegrees     */ 30.0f,  // snap: degrees per flick
		/* smoothTurnSpeed */ 90.0f,  // smooth: degrees/sec at full deflection (180 deg turn in 2 s)
		/* brightness      */ 1.0f,   // neutral; real fix is the sRGB write-control below
		/* roomScale       */ 0,      // OFF: body-follows-head conflicts with Marathon's tick/interpolation
		                            //      (camera lurches per tick). Stable 6DOF head view + stick for now.
		/* dominantHand    */ 0,      // right-handed
		/* switchSticks    */ 0,
		/* aimPitchAdjust  */ -20.0f, // aim pose sits ~20deg above a held-gun barrel; tilt down
		/* hudDistanceM    */ 1.0f,   // head-locked HUD plane distance
		/* hudSizeM        */ 1.15f,  // head-locked HUD plane height (width follows natural aspect) -- Huge tier
		/* hudTiltDeg      */ 30.0f,  // degrees the HUD bottom-anchor is pitched down from horizontal
		/* hudTextScale    */ 1.4f,   // global Lua HUD text multiplier (1.0 = unchanged; >1 enlarges HUD text) -- Large tier
		/* mapPlayerUp     */ 1,      // overhead map rotation: 0=north-up, 1=player-facing-up (default on for VR)
		/* teleportDistortion */ 1,   // horizontal-stretch/vertical-compress warp on teleport (may cause nausea)
		/* showLaserSight   */ 0,
		/* showAimGizmos    */ 0,     // controller aim diagnostic gizmos (OFF; debug-only, flip to 1 to show)
		/* buttonAction     */ {      // default button map (matches the old hardcoded stopgap)
			VR_ACT_ACTION_USE,       //   A          -> Action / Use
			VR_ACT_NONE,             //   B          -> unbound (screenshot promo feature disabled)
			VR_ACT_PREV_WEAPON,      //   X          -> Previous Weapon
			VR_ACT_NEXT_WEAPON,      //   Y          -> Next Weapon
			VR_ACT_RUN,              //   Move-click -> Run (toggle)
			VR_ACT_TOGGLE_MAP,       //   Turn-click -> Toggle Map
		},
		/* punchWithFists   */ 1,      // thrust a fist to punch (in addition to the trigger)
		/* punchSpeed       */ 1.6f,   // m/s forward controller speed that triggers a punch = "Medium"
		                            //   punch strength in VR CONTROLS (Low 1.1 / Medium 1.6 / High 2.4)
		/* leanGiveFraction */ 0.80f,  // head may lean 80% of the player's collision radius before the body
		                            //   follows (against-a-wall "give"; 0 = strict 1:1)
		/* panelCurvature   */ 0.0f,  // menu/terminal panel bend; flat by default (see vr_settings_t)
	};

	// Locomotion yaw offset (snap/smooth turn), in Marathon angle units (512 = full circle).
	float s_yawOffset = 0.0f;
	bool  s_turnArmed = true;   // snap-turn edge latch (re-armed when the stick recentres)
	const float kAngleUnitsPerDeg = 512.0f / 360.0f;

	// SMOOTH turning is integrated per RENDERED FRAME, not per 30 Hz tick: the eye matrices read
	// s_yawOffset live, so integrating at tick rate would step the whole world ~4 degrees at a time on a
	// 72 Hz display -- exactly the judder that makes smooth turning nauseating. The tick (vbl.cpp) owns
	// the "is turning allowed right now" gating (in game, console keyboard down, ...) and just LATCHES
	// the stick here; VR_BeginFrame integrates it with the real frame dt. If the latch stops being
	// refreshed (menu, keyboard up, game paused) it goes stale and the rotation stops on its own.
	float s_smoothTurnStick = 0.0f;
	int   s_smoothTurnAge   = 9999;      // frames since the last tick latch
	const int kSmoothTurnMaxAge = 6;     // ~80 ms at 72 Hz; ticks are 30 Hz so 2-3 frames is normal
	XrTime s_lastFrameTime = 0;          // predicted display time of the previous frame (for frame dt)

	// Advance the smooth turn by one frame. No-op in snap mode, or when the tick latch has gone stale.
	void advanceSmoothTurn(float dt)
	{
		if (s_settings.snapTurn) return;
		if (s_smoothTurnAge > kSmoothTurnMaxAge) return;
		++s_smoothTurnAge;

		const float dead = 0.2f;
		const float x = s_smoothTurnStick;
		const float mag = x < 0 ? -x : x;
		if (mag <= dead) return;
		// Rescale past the deadzone so a just-off-centre push starts from zero (not from 20% speed) and
		// full deflection still gives the configured speed.
		float t = (mag - dead) / (1.0f - dead);
		if (t > 1.0f) t = 1.0f;
		const float degrees = (x < 0 ? -t : t) * s_settings.smoothTurnSpeed * dt;
		s_yawOffset += degrees * kAngleUnitsPerDeg;
		while (s_yawOffset >= 512.0f) s_yawOffset -= 512.0f;
		while (s_yawOffset <  0.0f)   s_yawOffset += 512.0f;
	}

	// Yaw recenter request (set at level entry): make the player face `s_yawRecenterTarget` when the
	// head is neutral, regardless of where the headset is pointing at spawn.
	bool s_yawRecenterPending = false;
	int  s_yawRecenterTarget  = 0;
}

extern "C" void VR_RequestYawRecenter(int facingAngleUnits)
{
	s_yawRecenterTarget = facingAngleUnits;
	s_yawRecenterPending = true;
}

// If a yaw recenter is pending, return its target facing (angle units) and clear it; else false.
extern "C" bool VR_TakeYawRecenter(int* targetAngleUnits)
{
	if (!s_yawRecenterPending) return false;
	s_yawRecenterPending = false;
	if (targetAngleUnits) *targetAngleUnits = s_yawRecenterTarget;
	return true;
}
extern "C" vr_settings_t* VR_Settings(void) { return &s_settings; }

extern "C" float VR_GetYawOffset(void)        { return s_yawOffset; }
extern "C" void  VR_SetYawOffset(float a)     { s_yawOffset = a; }

extern "C" void VR_UpdateTurn(float stickX, float dt)
{
	// Right stick turns WITHOUT physically turning your body. Snap right (+X) = turn the view right,
	// which in Marathon's CCW angle convention means DECREASING yaw. (Flip the sign here if turning
	// goes the wrong way on device.)
	(void)dt;   // smooth turning integrates on the frame clock (advanceSmoothTurn), not this tick's dt
	if (s_settings.snapTurn) {
		s_smoothTurnStick = 0.0f;
		const float thresh = 0.6f, release = 0.3f;
		if (s_turnArmed && stickX > thresh)  { s_yawOffset += s_settings.turnDegrees * kAngleUnitsPerDeg; s_turnArmed = false; }
		else if (s_turnArmed && stickX < -thresh) { s_yawOffset -= s_settings.turnDegrees * kAngleUnitsPerDeg; s_turnArmed = false; }
		else if (stickX > -release && stickX < release) s_turnArmed = true;
	} else {
		// Smooth turn: just latch the stick (and keep the latch fresh). VR_BeginFrame does the actual
		// rotation once per rendered frame so the motion is display-smooth, not 30 Hz-stepped.
		s_smoothTurnStick = stickX;
		s_smoothTurnAge   = 0;
		s_turnArmed       = true;   // so switching back to snap mid-push doesn't fire a stale snap
	}
	// keep bounded to one turn for numeric tidiness
	while (s_yawOffset >= 512.0f) s_yawOffset -= 512.0f;
	while (s_yawOffset <  0.0f)   s_yawOffset += 512.0f;
}

extern "C" bool VR_GetEyeResolution(int* w, int* h)
{
	if (s_eyeW <= 0 || s_eyeH <= 0) return false;
	if (w) *w = s_eyeW; if (h) *h = s_eyeH;
	return true;
}

extern "C" bool VR_InitEGL(void)
{
	if (s_eglReady) return true;

	s_eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (s_eglDpy == EGL_NO_DISPLAY) { A1VR_LOG("VR_InitEGL: no display"); return false; }
	EGLint major = 0, minor = 0;
	if (!eglInitialize(s_eglDpy, &major, &minor)) { A1VR_LOG("VR_InitEGL: eglInitialize failed"); return false; }

	const EGLint cfgAttribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
		EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLConfig cfg = nullptr; EGLint n = 0;
	if (!eglChooseConfig(s_eglDpy, cfgAttribs, &cfg, 1, &n) || n < 1) { A1VR_LOG("VR_InitEGL: no config"); return false; }

	const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	s_eglCtx = eglCreateContext(s_eglDpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
	if (s_eglCtx == EGL_NO_CONTEXT) { A1VR_LOG("VR_InitEGL: eglCreateContext failed"); return false; }

	const EGLint pbAttribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
	s_eglSurf = eglCreatePbufferSurface(s_eglDpy, cfg, pbAttribs);
	if (s_eglSurf == EGL_NO_SURFACE) { A1VR_LOG("VR_InitEGL: eglCreatePbufferSurface failed"); return false; }

	if (!eglMakeCurrent(s_eglDpy, s_eglSurf, s_eglSurf, s_eglCtx)) { A1VR_LOG("VR_InitEGL: eglMakeCurrent failed"); return false; }

	s_eglReady = true;
	A1VR_LOG("VR_InitEGL: pbuffer GLES3 context current (EGL %d.%d, GL_VERSION=%s)", major, minor,
			 (const char*)glGetString(GL_VERSION));
	return true;
}

// ---- frame-scoped state (between VR_BeginFrame and VR_SubmitFrame) ----
namespace {
	XrCompositionLayerProjectionView s_layerViews[kEyes] = {};
	XrPosef  s_stageFromEye[kEyes] = {};
	bool     s_frameBegun       = false;
	bool     s_frameShouldRender = false;
	uint32_t s_curEyeImageIdx   = 0;
	int      s_curEye           = 0;
	bool     s_needWarmup       = false;   // set on level entry; consumed by the first render_view (texture prewarm)

	// ---- input (OpenXR action set, Touch controllers) ----
	XrActionSet s_actionSet  = XR_NULL_HANDLE;
	// Per-hand sticks/triggers (index 0=left, 1=right) so handedness/stick-switch can be routed in
	// software each frame without re-attaching the action set when the setting changes.
	XrAction    s_stickAction[2]   = { XR_NULL_HANDLE, XR_NULL_HANDLE };
	XrAction    s_stickClickAction[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };   // thumbstick press (L3/R3)
	XrAction    s_triggerAction[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
	XrAction    s_gripAction[2]   = { XR_NULL_HANDLE, XR_NULL_HANDLE };
	XrAction    s_actionAction = XR_NULL_HANDLE;
	XrAction    s_bAction = XR_NULL_HANDLE, s_xAction = XR_NULL_HANDLE, s_yAction = XR_NULL_HANDLE;
	XrAction    s_menuAction = XR_NULL_HANDLE;   // left controller hamburger/menu button -> in-game quit
	XrAction    s_aimAction[2]  = { XR_NULL_HANDLE, XR_NULL_HANDLE };  // 0=left, 1=right, pointing pose
	XrSpace     s_aimSpace[2]  = { XR_NULL_HANDLE, XR_NULL_HANDLE };
	XrPosef     s_aimStage[2]  = {};   // aim pose in stage space, this frame
	bool        s_aimValid[2]  = { false, false };
	XrAction    s_handAction[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE }; // grip/pose — hand body position
	XrSpace     s_handSpace[2]  = { XR_NULL_HANDLE, XR_NULL_HANDLE };
	XrPosef     s_handStage[2]  = {};  // grip pose in stage space, this frame
	bool        s_handValid[2]  = { false, false };
	// Occlusion/staleness diagnostics: whether the runtime is actively TRACKING (vs dead-reckoning an
	// estimate) the grip pose, and the reported linear velocity magnitude (m/s). When two controllers
	// get close (two-handed hold) one can occlude the other from the headset cameras -> the runtime
	// drops the TRACKED bit and dead-reckons -> position drifts until re-acquired.
	bool        s_handTracked[2] = { false, false };
	bool        s_aimTracked[2]  = { false, false };
	float       s_handSpeed[2]   = { 0.0f, 0.0f };
	// Per-hand linear velocity VECTOR in stage space (m/s), from XrSpaceVelocity. Feeds the fist-punch
	// detector, which measures the component along where the controller points (the aim forward).
	float       s_handVel[2][3]  = { {0,0,0}, {0,0,0} };
	// Fist-punch latch: active while a thrust is in progress (rising above punchSpeed, held for a few
	// frames so a 30 Hz action tick catches it, released once the forward speed decays). Re-arms on
	// release so one thrust = one punch.
	bool        s_punchActive[2] = { false, false };
	int         s_punchHold[2]   = { 0, 0 };
	// Provenance window: frames-remaining since a velocity punch was last detected on each hand. Used
	// to tell whether a fist SHOT was started by a physical thrust (suppress the swing animation) vs the
	// trigger (keep it). Longer than the fire pulse so it still reads true when the shot's firing state
	// becomes visible a couple of ticks after the thrust.
	int         s_punchRecent[2] = { 0, 0 };
	// Two-handed hold is LATCHED like QuestZDoom: engaged on the off-hand grip rising edge while the
	// hands are close, released only when the grip is let go -- NOT re-tested against the proximity
	// threshold every frame (that flickers the aim mode near the boundary).
	bool        s_twoHandedLatched = false;
	bool        s_offGripPrev      = false;
	XrAction    s_vibrateAction[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE }; // haptic output, 0=left 1=right
	float       s_vibDuration[2] = { 0.0f, 0.0f };   // pending vibration duration in ms (0=none)
	float       s_vibIntensity[2] = { 0.0f, 0.0f };  // pending vibration amplitude [0,1]
	bool        s_actionsReady = false;
	float       s_stickX[2] = {0,0}, s_stickY[2] = {0,0}, s_trigger[2] = {0,0};   // raw per-hand
	float       s_grip[2] = {0,0};                   // raw per-hand squeeze/grip
	bool        s_stickClick[2] = {false, false};   // raw per-hand thumbstick press
	float       s_moveX = 0, s_moveY = 0, s_turnX = 0, s_turnY = 0;   // routed (handedness/switch-sticks applied)
	bool        s_fire = false, s_altFire = false, s_action = false;
	// Set when the in-game console keyboard closes via its OK key (the trigger that clicked OK is still
	// held); suppresses fire until that trigger is RELEASED, so tapping OK doesn't also shoot. Cleared in
	// VR_BeginFrame once both triggers go low. See VR_InGameKeyboardActive.
	bool        s_kbFireLockout = false;
	bool        s_isDualWield = false;
	bool        s_twoHandedDisabled = false;   // current weapon opts out of two-handed grip (MML)
	float       s_twoHandedPitchDeg = 0.f;     // per-weapon two-handed muzzle pitch offset (MML), degrees
	float       s_twoHandedYawDeg   = 0.f;     // per-weapon two-handed muzzle yaw offset (MML), degrees
	bool        s_offHandHasWeapon = true;
	bool        s_gripAltFireEnabled = true;
	bool        s_bBtn = false, s_xBtn = false, s_yBtn = false;
	bool        s_advancePrev = false;   // edge latch for cutscene-skip key injection
	bool        s_menuBtn = false, s_menuPrev = false;   // menu button + edge latch
	bool        s_menuLatch = false;     // set on menu-button press, consumed by VR_TakeMenuButton

	XrPath path(const char* s) { XrPath p = XR_NULL_PATH; xrStringToPath(s_instance, s, &p); return p; }

	void initActions() {
		if (s_actionsReady) return;

		XrActionSetCreateInfo asci = { XR_TYPE_ACTION_SET_CREATE_INFO };
		std::strcpy(asci.actionSetName, "gameplay");
		std::strcpy(asci.localizedActionSetName, "Gameplay");
		if (XR_FAILED(xrCreateActionSet(s_instance, &asci, &s_actionSet))) { A1VR_LOG("createActionSet failed"); return; }

		auto mkAction = [&](const char* name, XrActionType type, XrAction* out) {
			XrActionCreateInfo aci = { XR_TYPE_ACTION_CREATE_INFO };
			std::strcpy(aci.actionName, name);
			std::strcpy(aci.localizedActionName, name);
			aci.actionType = type;
			xrCreateAction(s_actionSet, &aci, out);
		};
		mkAction("stickleft",  XR_ACTION_TYPE_VECTOR2F_INPUT, &s_stickAction[0]);
		mkAction("stickright", XR_ACTION_TYPE_VECTOR2F_INPUT, &s_stickAction[1]);
		mkAction("stickclickleft",  XR_ACTION_TYPE_BOOLEAN_INPUT, &s_stickClickAction[0]);
		mkAction("stickclickright", XR_ACTION_TYPE_BOOLEAN_INPUT, &s_stickClickAction[1]);
		mkAction("triggerleft",  XR_ACTION_TYPE_FLOAT_INPUT,  &s_triggerAction[0]);
		mkAction("triggerright", XR_ACTION_TYPE_FLOAT_INPUT,  &s_triggerAction[1]);
		mkAction("gripleft",  XR_ACTION_TYPE_FLOAT_INPUT,  &s_gripAction[0]);
		mkAction("gripright", XR_ACTION_TYPE_FLOAT_INPUT,  &s_gripAction[1]);
		mkAction("use", XR_ACTION_TYPE_BOOLEAN_INPUT,   &s_actionAction);
		mkAction("bbtn", XR_ACTION_TYPE_BOOLEAN_INPUT,  &s_bAction);
		mkAction("xbtn", XR_ACTION_TYPE_BOOLEAN_INPUT,  &s_xAction);
		mkAction("ybtn", XR_ACTION_TYPE_BOOLEAN_INPUT,  &s_yAction);
		mkAction("menu", XR_ACTION_TYPE_BOOLEAN_INPUT,  &s_menuAction);
		mkAction("aimleft",  XR_ACTION_TYPE_POSE_INPUT, &s_aimAction[0]);
		mkAction("aimright", XR_ACTION_TYPE_POSE_INPUT, &s_aimAction[1]);
		mkAction("handleftpose",  XR_ACTION_TYPE_POSE_INPUT, &s_handAction[0]);
		mkAction("handrightpose", XR_ACTION_TYPE_POSE_INPUT, &s_handAction[1]);
		mkAction("vibrateleft",  XR_ACTION_TYPE_VIBRATION_OUTPUT, &s_vibrateAction[0]);
		mkAction("vibrateright", XR_ACTION_TYPE_VIBRATION_OUTPUT, &s_vibrateAction[1]);

		XrActionSuggestedBinding binds[] = {
			{ s_stickAction[0],   path("/user/hand/left/input/thumbstick") },
			{ s_stickAction[1],   path("/user/hand/right/input/thumbstick") },
			{ s_stickClickAction[0], path("/user/hand/left/input/thumbstick/click") },
			{ s_stickClickAction[1], path("/user/hand/right/input/thumbstick/click") },
			{ s_triggerAction[1], path("/user/hand/right/input/trigger/value") },
			{ s_triggerAction[0], path("/user/hand/left/input/trigger/value") },
			{ s_gripAction[1],   path("/user/hand/right/input/squeeze/value") },
			{ s_gripAction[0],   path("/user/hand/left/input/squeeze/value") },
			{ s_actionAction,  path("/user/hand/right/input/a/click") },
			{ s_bAction,       path("/user/hand/right/input/b/click") },
			{ s_xAction,       path("/user/hand/left/input/x/click") },
			{ s_yAction,       path("/user/hand/left/input/y/click") },
			{ s_menuAction,    path("/user/hand/left/input/menu/click") },
			{ s_aimAction[0],  path("/user/hand/left/input/aim/pose") },
			{ s_aimAction[1],  path("/user/hand/right/input/aim/pose") },
			{ s_handAction[0], path("/user/hand/left/input/grip/pose") },
			{ s_handAction[1], path("/user/hand/right/input/grip/pose") },
			{ s_vibrateAction[0], path("/user/hand/left/output/haptic") },
			{ s_vibrateAction[1], path("/user/hand/right/output/haptic") },
		};
		XrInteractionProfileSuggestedBinding sb = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		sb.interactionProfile = path("/interaction_profiles/oculus/touch_controller");
		sb.countSuggestedBindings = sizeof(binds) / sizeof(binds[0]);
		sb.suggestedBindings = binds;
		if (XR_FAILED(xrSuggestInteractionProfileBindings(s_instance, &sb))) A1VR_LOG("suggestBindings failed");

		XrSessionActionSetsAttachInfo ai = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
		ai.countActionSets = 1;
		ai.actionSets = &s_actionSet;
		if (XR_FAILED(xrAttachSessionActionSets(s_session, &ai))) { A1VR_LOG("attachActionSets failed"); return; }

		for (int h = 0; h < 2; ++h) {
			XrActionSpaceCreateInfo sci = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
			sci.poseInActionSpace.orientation.w = 1.0f;
			sci.action = s_aimAction[h];
			if (XR_FAILED(xrCreateActionSpace(s_session, &sci, &s_aimSpace[h]))) A1VR_LOG("createAimSpace %d failed", h);
			sci.action = s_handAction[h];
			if (XR_FAILED(xrCreateActionSpace(s_session, &sci, &s_handSpace[h]))) A1VR_LOG("createHandSpace %d failed", h);
		}

		s_actionsReady = true;
		A1VR_LOG("input actions attached");
	}

	void processHaptics() {
		if (!s_actionsReady || !s_sessionRunning) return;
		for (int h = 0; h < 2; ++h) {
			if (s_vibDuration[h] > 0.0f) {
				XrHapticVibration vib = { XR_TYPE_HAPTIC_VIBRATION };
				vib.amplitude = s_vibIntensity[h];
				vib.duration  = (XrDuration)(s_vibDuration[h] * 1e6f); // ms -> ns
				vib.frequency = 3000.0f;
				XrHapticActionInfo hai = { XR_TYPE_HAPTIC_ACTION_INFO };
				hai.action = s_vibrateAction[h];
				xrApplyHapticFeedback(s_session, &hai, (const XrHapticBaseHeader*)&vib);
				s_vibDuration[h] = 0.0f;
			}
		}
	}

	void syncInput() {
		if (!s_actionsReady || !s_sessionRunning) return;
		XrActiveActionSet aas = { s_actionSet, XR_NULL_PATH };
		XrActionsSyncInfo si = { XR_TYPE_ACTIONS_SYNC_INFO };
		si.countActiveActionSets = 1;
		si.activeActionSets = &aas;
		xrSyncActions(s_session, &si);

		XrActionStateGetInfo gi = { XR_TYPE_ACTION_STATE_GET_INFO };
		XrActionStateVector2f v2 = { XR_TYPE_ACTION_STATE_VECTOR2F };
		XrActionStateFloat f = { XR_TYPE_ACTION_STATE_FLOAT };
		XrActionStateBoolean bs = { XR_TYPE_ACTION_STATE_BOOLEAN };
		for (int h = 0; h < 2; ++h) {
			gi.action = s_stickAction[h];   xrGetActionStateVector2f(s_session, &gi, &v2);
			s_stickX[h] = v2.currentState.x; s_stickY[h] = v2.currentState.y;
			gi.action = s_triggerAction[h]; xrGetActionStateFloat(s_session, &gi, &f);
			s_trigger[h] = f.currentState;
			gi.action = s_gripAction[h]; xrGetActionStateFloat(s_session, &gi, &f);
			s_grip[h] = f.currentState;
			gi.action = s_stickClickAction[h]; xrGetActionStateBoolean(s_session, &gi, &bs);
			s_stickClick[h] = bs.currentState;
		}

		// Route the raw per-hand sticks/triggers into the logical move/turn/fire actions. By DEFAULT the
		// OFF hand MOVES (stick) and the DOMINANT hand TURNS (stick) -- for the default right-handed
		// profile that is the conventional left-stick-move / right-stick-turn layout most players expect.
		// FIRING still follows the dominant hand (primary trigger); the off hand secondary-fires.
		// "Switch Thumbsticks" (switchSticks) swaps move<->turn so the DOMINANT hand moves instead (which
		// some left-handed players prefer). (index 0=left, 1=right)
		const int domIdx = VR_Settings()->dominantHand ? 0 : 1;   // left-handed -> dominant is the left hand
		const int offIdx = 1 - domIdx;
		const int moveIdx = VR_Settings()->switchSticks ? domIdx : offIdx;
		const int turnIdx = VR_Settings()->switchSticks ? offIdx : domIdx;
		s_moveX = s_stickX[moveIdx]; s_moveY = s_stickY[moveIdx];
		s_turnX = s_stickX[turnIdx]; s_turnY = s_stickY[turnIdx];
		// Clear the post-console-OK fire lockout once BOTH triggers are released (so the next real pull fires).
		if (s_kbFireLockout && s_trigger[domIdx] <= 0.5f && s_trigger[offIdx] <= 0.5f) s_kbFireLockout = false;
		s_fire    = (s_trigger[domIdx] > 0.5f) && !s_kbFireLockout;
		s_altFire = (s_trigger[offIdx] > 0.5f) && !s_kbFireLockout;

		XrActionStateBoolean b = { XR_TYPE_ACTION_STATE_BOOLEAN };
		gi.action = s_actionAction;  xrGetActionStateBoolean(s_session, &gi, &b); s_action = b.currentState;
		gi.action = s_bAction;       xrGetActionStateBoolean(s_session, &gi, &b); s_bBtn   = b.currentState;
		gi.action = s_xAction;       xrGetActionStateBoolean(s_session, &gi, &b); s_xBtn   = b.currentState;
		gi.action = s_yAction;       xrGetActionStateBoolean(s_session, &gi, &b); s_yBtn   = b.currentState;
		gi.action = s_menuAction;    xrGetActionStateBoolean(s_session, &gi, &b); s_menuBtn = b.currentState;

		// Menu (hamburger) button edge -> request the in-game quit-with-confirmation (consumed in the
		// main loop, which is where the modal confirmation dialog can run).
		if (s_menuBtn && !s_menuPrev) s_menuLatch = true;
		s_menuPrev = s_menuBtn;

		// Cutscene/intro skip: A or X edge -> inject Enter so the chapter screens advance (terminals
		// are handled via action flags in vbl.cpp). Lightweight stopgap until proper VR button mapping.
		const bool advance = s_action || s_xBtn;
		if (advance && !s_advancePrev) {
			SDL_Event e{};
			e.type = SDL_KEYDOWN; e.key.state = SDL_PRESSED;
			e.key.keysym.sym = SDLK_RETURN; e.key.keysym.scancode = SDL_SCANCODE_RETURN;
			SDL_PushEvent(&e);
			e.type = SDL_KEYUP; e.key.state = SDL_RELEASED;
			SDL_PushEvent(&e);
		}
		s_advancePrev = advance;
	}
}

// Two-hand positions in stage space (metres, Y-up): the AIM-pose origins for both hands. This matches
// QuestZDoom's weaponStabilised input exactly -- QZD locates aimSpace[] and feeds those positions into
// (off - dom) (OpenXrInput.cpp TBXR_UpdateControllers + VrInputDefault.cpp). We previously used the
// GRIP pose here, which diverged from the reference and made the inter-hand vector sweep at short
// separation; the aim pose is what QZD proved stable. Returns false if either aim pose is untracked.
// Also reports the inter-hand distance.
static bool twoHandPositions(int domHand, int offHand, float dom[3], float off[3], float* distOut)
{
	if (!s_aimValid[domHand] || !s_aimValid[offHand]) return false;
	const XrPosef& dp = s_aimStage[domHand];
	const XrPosef& op = s_aimStage[offHand];
	dom[0] = dp.position.x; dom[1] = dp.position.y; dom[2] = dp.position.z;
	off[0] = op.position.x; off[1] = op.position.y; off[2] = op.position.z;
	if (distOut) {
		const float dx = dom[0]-off[0], dy = dom[1]-off[1], dz = dom[2]-off[2];
		*distOut = std::sqrt(dx*dx + dy*dy + dz*dz);
	}
	return true;
}

extern "C" bool VR_BeginFrame(void)
{
	// Idempotent within a frame. render_screen (apply_vr_view_offsets) now calls this EARLY so the head
	// pose is freshly located BEFORE the render camera is computed; render_view then calls it again for
	// the per-eye loop and must REUSE the same located pose. Otherwise the camera position comes from the
	// previous frame's pose while the eye matrices (VR_GetEyeViewMetres) come from this frame's -> position
	// lags orientation by a frame and the walls shear/jump when the head TRANSLATES side to side. (A second
	// xrWaitFrame/xrBeginFrame would also be an OpenXR protocol error.) s_frameBegun is cleared in
	// VR_SubmitFrame, which render_view always calls -> the state machine is false->begin->submit->false.
	if (s_frameBegun) return s_frameShouldRender;
	s_frameShouldRender = false;
	if (!s_active) return false;
	if (!startSession()) return false;   // waits for the GL context
	pollEvents();
	if (!s_sessionRunning) return false; // session not running yet -> no frame begun
	processHaptics();
	syncInput();

	XrFrameWaitInfo wfi = { XR_TYPE_FRAME_WAIT_INFO };
	s_frameState = XrFrameState{ XR_TYPE_FRAME_STATE };
	XR_CHECK(xrWaitFrame(s_session, &wfi, &s_frameState));
	XrFrameBeginInfo fbi = { XR_TYPE_FRAME_BEGIN_INFO };
	XR_CHECK(xrBeginFrame(s_session, &fbi));
	s_frameBegun = true;

	// Frame dt from the predicted display times (this is the clock the eyes are actually rendered on),
	// clamped so a hitch/load pause can't dump a huge rotation in one frame.
	{
		float dt = 0.0f;
		if (s_lastFrameTime != 0 && s_frameState.predictedDisplayTime > s_lastFrameTime)
			dt = (float)((double)(s_frameState.predictedDisplayTime - s_lastFrameTime) * 1e-9);
		s_lastFrameTime = s_frameState.predictedDisplayTime;
		if (dt > 0.1f) dt = 0.1f;
		advanceSmoothTurn(dt);
	}

	if (s_frameState.shouldRender) {
		XrSpaceLocation hl = { XR_TYPE_SPACE_LOCATION };
		xrLocateSpace(s_headSpace, s_stageSpace, s_frameState.predictedDisplayTime, &hl);
		s_stageFromHead = hl.pose;
		if (hl.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
			s_headPoseValid = true;
		// Seed the absorbed head origin on the first valid pose so the render starts at ~zero residual
		// (avoids a one-tick position jump when the first VR_LatchHeadMove runs).
		if (!s_headLatchInit && (hl.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
			s_lastHeadX = s_stageFromHead.position.x;
			s_lastHeadZ = s_stageFromHead.position.z;
			s_headLatchInit = true;
		}

		// (No lean-reference drift any more: the render offset is now the RESIDUAL relative to s_lastHead,
		// the absorbed head origin that VR_LatchHeadMove advances every physics tick as the body follows
		// the head. The residual is inherently ~one tick of head motion, so it can't accumulate -- the old
		// unbounded-offset problem the drift patched is gone with the body actually moving.)

		XrViewLocateInfo vli = { XR_TYPE_VIEW_LOCATE_INFO };
		vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		vli.displayTime = s_frameState.predictedDisplayTime;
		vli.space = s_headSpace;
		XrViewState vs = { XR_TYPE_VIEW_STATE };
		uint32_t got = 0;
		for (int e = 0; e < kEyes; ++e) s_views[e].type = XR_TYPE_VIEW;
		xrLocateViews(s_session, &vli, &vs, kEyes, &got, s_views);
		for (int e = 0; e < kEyes; ++e)
			s_stageFromEye[e] = pose_mul(s_stageFromHead, s_views[e].pose);

		// Controller aim poses (direction/orientation) and grip poses (hand position), in stage space.
		for (int h = 0; h < 2; ++h) {
			s_aimValid[h] = false;
			s_aimTracked[h] = false;
			if (s_aimSpace[h] != XR_NULL_HANDLE) {
				XrSpaceLocation al = { XR_TYPE_SPACE_LOCATION };
				xrLocateSpace(s_aimSpace[h], s_stageSpace, s_frameState.predictedDisplayTime, &al);
				if ((al.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) &&
					(al.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
					// Use the runtime pose AS-IS whenever it is valid -- including the dead-reckoned
					// estimate when POSITION_TRACKED is off (brief occlusion). This follows the hand
					// like QuestZDoom instead of freezing at the last tracked spot (which detached the
					// weapon from the hand). When POSITION_VALID drops entirely, we skip the update and
					// s_aimStage keeps its last value -- the runtime has no position to offer at all.
					s_aimStage[h] = al.pose;
					s_aimValid[h] = true;
					s_aimTracked[h] = (al.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0;
				}
			}
			s_handValid[h] = false;
			s_handTracked[h] = false;
			s_handSpeed[h] = 0.0f;
			s_handVel[h][0] = s_handVel[h][1] = s_handVel[h][2] = 0.0f;
			if (s_handSpace[h] != XR_NULL_HANDLE) {
				XrSpaceVelocity hv = { XR_TYPE_SPACE_VELOCITY };
				XrSpaceLocation hl = { XR_TYPE_SPACE_LOCATION };
				hl.next = &hv;
				xrLocateSpace(s_handSpace[h], s_stageSpace, s_frameState.predictedDisplayTime, &hl);
				if ((hl.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
					// Runtime pose as-is while valid (incl. dead-reckoned when not TRACKED) -- follows
					// the hand like QuestZDoom rather than freezing/detaching. See the aim block above.
					s_handStage[h] = hl.pose;
					s_handValid[h] = true;
					s_handTracked[h] = (hl.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0;
					if (hv.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
						const XrVector3f& v = hv.linearVelocity;
						s_handSpeed[h] = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
						s_handVel[h][0] = v.x; s_handVel[h][1] = v.y; s_handVel[h][2] = v.z;
					}
				}
			}
		}

		// Two-handed latch (QuestZDoom model): engage on the off-hand grip RISING edge while the hands
		// are close; release only when the grip is let go. Not re-tested against the proximity threshold
		// every frame, so a noisy/occluded distance estimate near the boundary can't flicker the aim mode.
		{
			const int domHand = s_settings.dominantHand ? 0 : 1;
			const int offHand = 1 - domHand;
			const bool offGrip = s_grip[offHand] > 0.5f;
			if (s_isDualWield) {
				s_twoHandedLatched = false;
			} else if (offGrip && !s_offGripPrev) {
				float dm[3], of[3], dist = 1e9f;
				if (twoHandPositions(domHand, offHand, dm, of, &dist) && dist < 0.5f)
					s_twoHandedLatched = true;
			} else if (!offGrip) {
				s_twoHandedLatched = false;
			}
			s_offGripPrev = offGrip;
		}

		// Fist-punch detection (velocity-driven melee). For each hand, measure the forward speed =
		// component of the controller's linear velocity along where it points (the aim forward, which
		// already includes aimPitchAdjust so a punch fires exactly along the aim ray). A thrust past
		// punchSpeed latches a punch; hysteresis + a short hold means one thrust = one punch and a slow
		// wave of the hand doesn't fire. The actual gating on "fists equipped" happens in the input code.
		{
			const float on  = s_settings.punchSpeed > 0.1f ? s_settings.punchSpeed : 2.0f;
			const float off = on * 0.35f;   // release threshold (hysteresis) so decel re-arms the punch
			for (int h = 0; h < 2; ++h) {
				float ap[3], af[3];
				float vf = 0.0f;
				if (VR_GetAimPoseStage(h, ap, af))
					vf = s_handVel[h][0]*af[0] + s_handVel[h][1]*af[1] + s_handVel[h][2]*af[2];
				if (!s_punchActive[h]) {
					if (vf > on) { s_punchActive[h] = true; s_punchHold[h] = 6; s_punchRecent[h] = 18; }
				} else {
					s_punchRecent[h] = 18;   // keep the provenance window fresh for the whole thrust
					if (s_punchHold[h] > 0) --s_punchHold[h];
					if (vf < off && s_punchHold[h] == 0) s_punchActive[h] = false;
				}
				if (s_punchRecent[h] > 0) --s_punchRecent[h];   // ~200ms window (18 frames)
			}
		}

		s_frameShouldRender = true;
	}
	return s_frameShouldRender;
}

extern "C" void VR_BeginEye(int eye)
{
	s_curEye = eye;
	EyeFB& fb = s_eye[eye];
	XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	xrAcquireSwapchainImage(fb.swapchain, &ai, &s_curEyeImageIdx);
	XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	wi.timeout = XR_INFINITE_DURATION;
	xrWaitSwapchainImage(fb.swapchain, &wi);

	glBindFramebuffer(GL_FRAMEBUFFER, fb.fbos[s_curEyeImageIdx]);
	glDisable(GL_SCISSOR_TEST);   // the engine's 2D view scissor must not clip the full-eye render

	// The eye swapchain is sRGB so the compositor interprets it correctly, but the engine already
	// outputs sRGB-encoded colour; disable the GL linear->sRGB write conversion so our bytes are
	// stored verbatim (else the whole image -- world AND the 2D UI panel -- is over-brightened).
	if (s_srgbWriteControl < 0) {
		s_srgbWriteControl = hasGLExtension("GL_EXT_sRGB_write_control") ? 1 : 0;
		A1VR_LOG("EXT_sRGB_write_control: %s", s_srgbWriteControl ? "present (disabling sRGB encode)" : "ABSENT");
	}
	if (s_srgbWriteControl == 1)
		glDisable(GL_FRAMEBUFFER_SRGB_EXT);

	glViewport(0, 0, s_eyeW, s_eyeH);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

extern "C" unsigned VR_CurrentEyeFramebuffer(void) { return s_eye[s_curEye].fbos[s_curEyeImageIdx]; }

extern "C" void VR_GetEyeProjection(int eye, float* proj16, float zn, float zf)
{
	proj_from_fov(proj16, s_views[eye].fov, zn, zf);
}

extern "C" void VR_GetEyeViewMetres(int eye, float* view16)
{
	// Use the eye position RELATIVE to the absorbed head origin (horizontal only) so the render shows
	// only the residual head movement since the last tick -- the body carries the rest. Without this
	// the body-follows-head locomotion would double-count and movement would feel ~2x.
	// The head's HORIZONTAL position is applied to the VIEW ORIGIN engine-side (VR_GetHeadOffset,
	// clamped against walls), so here the camera keeps only the per-eye IPD separation horizontally
	// (eye relative to head). Keep full vertical (duck) + full orientation. This is render-side only
	// (never the physics position), so it can't fly.
	XrPosef p = s_stageFromEye[eye];
	p.position.x = s_stageFromEye[eye].position.x - s_stageFromHead.position.x;
	p.position.z = s_stageFromEye[eye].position.z - s_stageFromHead.position.z;
	float eyeM[16]; mat_from_pose(eyeM, p);
	mat_rigid_inverse(view16, eyeM);
}

extern "C" void VR_FinishEye(int eye)
{
	EyeFB& fb = s_eye[eye];
	glBindFramebuffer(GL_FRAMEBUFFER, fb.fbos[s_curEyeImageIdx]);
	const GLenum disc[1] = { GL_DEPTH_ATTACHMENT };
	glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, disc);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	xrReleaseSwapchainImage(fb.swapchain, &ri);

	s_layerViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	s_layerViews[eye].pose = s_stageFromEye[eye];
	s_layerViews[eye].fov  = s_views[eye].fov;
	s_layerViews[eye].subImage.swapchain = fb.swapchain;
	s_layerViews[eye].subImage.imageRect.offset = { 0, 0 };
	s_layerViews[eye].subImage.imageRect.extent = { s_eyeW, s_eyeH };
	s_layerViews[eye].subImage.imageArrayIndex = 0;
}

// ---- Level-load texture prewarm ----------------------------------------------------------------
// The FIRST render_view after a level load lazy-uploads EVERY visible sprite/landscape/model texture
// during that frame's first eye render (~2 s), and it does so INSIDE a begun OpenXR frame with the eye
// swapchain image acquired -- so the compositor sees a frame held open for ~2 s and shows the torn
// "one eye is a flat plane, the other is missing" image. Fix: on that first frame, do the upload as a
// throwaway render into the (non-swapchain) screen-layer FBO with NO OpenXR frame begun, so a clean
// loading frame stays on screen the whole time; the next frame then renders both eyes fast (cache warm).
extern "C" void VR_RequestLevelWarmup(void) { s_needWarmup = true; }
extern "C" bool VR_ConsumeLevelWarmup(void) { bool v = s_needWarmup; s_needWarmup = false; return v; }
// VR_BeginWarmupEye / VR_EndWarmup are defined below, after the screen-layer FBO (their scratch target).

extern "C" void VR_SubmitFrame(void)
{
	if (!s_frameBegun) return;
	XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	layer.space = s_stageSpace;
	layer.viewCount = kEyes;
	layer.views = s_layerViews;
	const XrCompositionLayerBaseHeader* layers[1] = { (XrCompositionLayerBaseHeader*)&layer };

	XrFrameEndInfo fei = { XR_TYPE_FRAME_END_INFO };
	fei.displayTime = s_frameState.predictedDisplayTime;
	fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	fei.layerCount = s_frameShouldRender ? 1 : 0;
	fei.layers     = s_frameShouldRender ? layers : nullptr;
	XR_CHECK(xrEndFrame(s_session, &fei));
	s_frameBegun = false;
}

extern "C" int VR_EyeWidth(void)  { return s_eyeW; }
extern "C" int VR_EyeHeight(void) { return s_eyeH; }
extern "C" int VR_CurrentEye(void) { return s_curEye; }

extern "C" bool VR_GetHmdYawPitch(float* yawAngleUnits, float* pitchAngleUnits)
{
	if (yawAngleUnits)   *yawAngleUnits   = 0;
	if (pitchAngleUnits) *pitchAngleUnits = 0;
	if (!s_headPoseValid) return false;

	// Head-forward (head space -Z) rotated into stage space (Y-up, -Z forward) by the HMD quaternion.
	const XrQuaternionf& q = s_stageFromHead.orientation;
	const float fx = -2.0f * (q.x * q.z + q.w * q.y);
	const float fy = -2.0f * (q.y * q.z - q.w * q.x);
	const float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));

	const float kUnitsPerRad = 512.0f / (2.0f * 3.14159265358979f);   // NUMBER_OF_ANGLES / 2pi
	// World-frame azimuth contribution to ADD to the body yaw, derived so view->yaw matches the
	// per-eye modelview's camera direction exactly: modelview maps head-forward (fx,fy,fz) through
	// M^-1 (zUpToYUp inverse: stage(sx,sy,sz)->world(-sx,-sz,sy)) giving world dir (-fx,-fz,fy), whose
	// Marathon azimuth is atan2(j,i) = atan2(-fz,-fx). (Using atan2(fx,-fz) before put the visibility
	// cone ~90 deg off the camera even at neutral -> the world looked back-to-front.)
	const float yawRad   = std::atan2(-fz, -fx);
	float s = fy; if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
	const float pitchRad = std::asin(s);                             // + = looking up (world elevation)

	if (yawAngleUnits)   *yawAngleUnits   = yawRad   * kUnitsPerRad;
	if (pitchAngleUnits) *pitchAngleUnits = pitchRad * kUnitsPerRad;
	return true;
}

extern "C" void VR_GetMove(float* x, float* y) { if (x) *x = s_moveX; if (y) *y = s_moveY; }
extern "C" void VR_GetTurn(float* x)           { if (x) *x = s_turnX; }
extern "C" void VR_GetTurnY(float* y)          { if (y) *y = s_turnY; }

// Deadzoned + rescaled thumbstick so partial deflection gives proportionally slower movement.
extern "C" void VR_GetAnalogMove(float* strafe, float* forward)
{
	const float dead = 0.15f;
	auto curve = [&](float v) -> float {
		const float a = std::fabs(v);
		if (a <= dead) return 0.0f;
		const float t = (a - dead) / (1.0f - dead);   // just past deadzone -> ~0, full -> 1
		return (v < 0 ? -t : t);
	};
	if (strafe)  *strafe  = curve(s_moveX);
	if (forward) *forward = curve(s_moveY);
}

// Capture the head's physical movement since the last call as a world-space body delta (map x,y),
// so the engine can move the player to follow the head (with collision). Call once per real tick.
extern "C" void VR_LatchHeadMove(void)
{
	const float hx = s_stageFromHead.position.x;
	const float hz = s_stageFromHead.position.z;
	if (!s_headLatchInit || !s_headPoseValid) {
		s_lastHeadX = hx; s_lastHeadZ = hz;
		s_prevHeadX = hx; s_prevHeadZ = hz;
		s_headLatchInit = s_headPoseValid;
		s_headMoveX = s_headMoveY = 0;
		return;
	}
	const float W  = s_settings.worldScaleWUM;

	// LEAN GIVE (deadzone). s_lastHead is the body's room-space ANCHOR, not the raw head. The head may
	// lean up to L metres from the anchor with the body FROZEN (a pure head lean); only motion beyond L
	// advances the anchor and drives the body. L is a fraction of the player's ACTUAL collision radius
	// (fed live from the physics model via VR_SetPlayerRadiusWU -- never hardcoded) converted to metres
	// through the world scale, so the give tracks the physics radius and stays the same share of the
	// body's footprint at any world scale.
	//
	// This kills the against-a-wall RATCHET: pressed at a terminal, leaning the head in then back out
	// stays inside L, so the anchor (and the body) never move -- instead of the wall eating the lean-IN
	// while the unopposed lean-OUT walks the body away from the wall a little each cycle. Fitting through
	// tight gaps and pushing right up to walls are UNCHANGED: the body's radius and collision are
	// untouched; the anchor merely trails the head by <=L along the SAME path (a time lag, not a lateral
	// change), so the collision capsule still threads exactly the same geometry.
	float L = 0.0f;
	if (s_playerRadiusWU > 0.0f && W > 0.0f)
		L = s_settings.leanGiveFraction * (s_playerRadiusWU / W);

	// Chase the anchor toward the head, leaving it L behind; within L it stays put (body frozen).
	float dhx = hx - s_lastHeadX;
	float dhz = hz - s_lastHeadZ;
	const float dist = std::sqrt(dhx * dhx + dhz * dhz);
	float mx = 0.0f, mz = 0.0f;   // anchor advance this tick == the body's stage-space move
	if (dist > L) {
		const float k = (dist - L) / dist;
		mx = dhx * k; mz = dhz * k;
	}

	// Safety clamp (catastrophe guard only -- normal lean/walk is well under this). ~0.15 m/tick.
	const float kMax = 0.15f;
	if (mx >  kMax) mx =  kMax; else if (mx < -kMax) mx = -kMax;
	if (mz >  kMax) mz =  kMax; else if (mz < -kMax) mz = -kMax;

	// Roll the anchor: prev -> render interp reference; advance by EXACTLY the emitted body move so the
	// render residual (head - anchor, VR_GetHeadOffset) and the body stay in lockstep.
	s_prevHeadX = s_lastHeadX; s_prevHeadZ = s_lastHeadZ;
	s_lastHeadX += mx; s_lastHeadZ += mz;

	// World body delta = Rz(yawOffset) * (-mx, -mz) * worldScale (matches the render's head-offset:
	// head_world = origin + Rz(yawOffset) * (-hx, -hz) * W). Stage deltas keep snap-turns from
	// registering as movement (a snap rotates the play space, not the head's physical position).
	const float yr = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);
	const float c  = std::cos(yr), s = std::sin(yr);
	s_headMoveX = W * (-c * mx + s * mz);
	s_headMoveY = W * (-s * mx - c * mz);

	// Diagnostic (throttled): head-vs-anchor distance + lean radius + resulting body delta (world units).
	static int s_dbg = 0;
	if ((s_dbg++ % 15) == 0)
		A1VR_LOG("headmove: pos=(%.3f,%.3f) lean=%.3f/L=%.3f -> move=(%.4f,%.4f) body=(%.2f,%.2f) wu",
			hx, hz, dist, L, mx, mz, s_headMoveX, s_headMoveY);
}

extern "C" void VR_GetHeadMove(float* x, float* y) { if (x) *x = s_headMoveX; if (y) *y = s_headMoveY; }

// Vertical offset of the actual HMD eye from the neutral standing eye height, in Marathon world
// units (negative when seated/ducking). The engine adds this to view->origin.z so the visibility
// tree clips floors/ceilings at the TRUE eye height (fixes step-top floors vanishing when seated);
// the Rasterizer subtracts it back so the rendered camera is unchanged.

// Pick a LIFE-SIZE world scale: WU-per-metre so the game's fixed eye-height-above-floor (s_gameEyeHeightWU)
// maps to the player's measured standing eye height. Then eye_world_Z = camZ + (headY - H)*W places the eye
// at camZ (= game eye height E above the floor) when standing, and E/W == H, so the floor sits H metres
// below the eye -- reaching the real floor lands the hand on the game floor and head tracking stays 1:1.
// Only once a standing height is measured; clamped so a bad/miscalibrated measurement can't wildly
// mis-scale the world. Runs whenever the physics feeds E or the player recenters.
static void recomputeWorldScale()
{
	if (!s_standingEyeHeightInit || s_standingEyeHeightM < 0.5f) return;
	float w = s_gameEyeHeightWU / s_standingEyeHeightM;
	if (w < 330.0f) w = 330.0f; else if (w > 560.0f) w = 560.0f;   // ~1.1 m .. ~1.86 m standing at E=614
	if (w != s_settings.worldScaleWUM) {
		A1VR_LOG("life-size world scale: E=%.0f WU / H=%.3f m -> worldScaleWUM=%.1f (was %.1f)",
			s_gameEyeHeightWU, s_standingEyeHeightM, w, s_settings.worldScaleWUM);
		s_settings.worldScaleWUM = w;
	}
}

// Fed by the physics each tick: the player's in-game eye height above the floor (world units) for the
// running scenario. Updates the life-size world scale.
extern "C" void VR_SetGameEyeHeightWU(float eyeHeightWU)
{
	if (eyeHeightWU > 1.0f) { s_gameEyeHeightWU = eyeHeightWU; recomputeWorldScale(); }
}

// Fed by the physics each tick: the local player's actual collision radius (world units) from the running
// physics model. Used (scaled by leanGiveFraction) as the head-lean deadzone -- see VR_LatchHeadMove.
extern "C" void VR_SetPlayerRadiusWU(float radiusWU)
{
	if (radiusWU > 0.0f) s_playerRadiusWU = radiusWU;
}

// Effective eye-height REFERENCE (metres) used for vertical placement: eye_world_Z = camZ + (headY -
// VR_EyeHeightM())*W. Once the player has recentered (auto on the first valid pose and at every level
// entry) the baseline is their ACTUAL measured standing head height; before that a nominal fallback.
// The Height Adjust preference then shifts it: subtracting heightAdjustM from the reference raises the
// placed camera by heightAdjustM*worldScale, so +adjust = taller in-game, -adjust = shorter.
extern "C" float VR_EyeHeightM(void)
{
	const float kNominalEyeHeightM = 1.6f;   // used only in the brief window before the first recenter
	const float baseline = s_standingEyeHeightInit ? s_standingEyeHeightM : kNominalEyeHeightM;
	return baseline - s_settings.heightAdjustM;
}

extern "C" float VR_GetEyeZOffset(void)
{
	if (!s_headPoseValid) return 0.0f;
	return (s_stageFromHead.position.y - VR_EyeHeightM()) * s_settings.worldScaleWUM;
}

extern "C" void VR_RecenterHead(void)
{
	if (s_headPoseValid) {
		// Re-anchor the absorbed head origin to the current head so the render residual (VR_GetHeadOffset)
		// and the per-tick body delta (VR_GetHeadMove) both start from 0 here -- i.e. "you are standing
		// where you are now." The body doesn't teleport; only the head/body mapping is re-zeroed.
		s_lastHeadX = s_stageFromHead.position.x; s_lastHeadZ = s_stageFromHead.position.z;
		s_headLatchInit = true;
		// Capture the current (standing) eye height as the vertical reference, so the player is placed at
		// the Marathon eye height right now and duck/lean read as deltas below it.
		s_standingEyeHeightM = s_stageFromHead.position.y; s_standingEyeHeightInit = true;
		recomputeWorldScale();   // life-size scale now that we know the standing height
	}
}

// True (once) after the runtime recentered our reference space (Meta/Quest button hold). The game tick
// consumes this to run the same recenter as the controller Recenter action. Clears on read.
extern "C" bool VR_TakeSystemRecenter(void)
{
	if (!s_systemRecenterPending) return false;
	s_systemRecenterPending = false;
	return true;
}

// Screenshot one-shot flag (VR_ACT_SCREENSHOT): requested from the input tick, consumed once by the
// render eye-loop. See vr_openxr.h.
static bool s_screenshotRequested = false;
extern "C" void VR_RequestScreenshot(void) { s_screenshotRequested = true; }
extern "C" bool VR_TakeScreenshotIfRequested(void)
{
	if (!s_screenshotRequested) return false;
	s_screenshotRequested = false;
	return true;
}

// Shared head-centre reference for discrete sprite-view selection (see vr_openxr.h /
// get_object_shape_and_transfer_mode). Set to base_origin each stereo frame so BOTH eyes bucket the
// N/NE/E/... sprite from the same point -> no per-eye sprite mismatch at the 45deg view boundaries.
static bool          s_spriteViewOriginValid = false;
static world_point3d s_spriteViewOrigin = {0, 0, 0};
extern "C" void VR_SetSpriteViewOrigin(const world_point3d* o)
{
	if (o) { s_spriteViewOrigin = *o; s_spriteViewOriginValid = true; }
	else   { s_spriteViewOriginValid = false; }
}
extern "C" bool VR_GetSpriteViewOrigin(world_point3d* out)
{
	if (!s_spriteViewOriginValid || !out) return false;
	*out = s_spriteViewOrigin;
	return true;
}

// Continuous float render-camera position, written per render frame by the engine (apply_vr_view_offsets).
static float s_renderCamX = 0.f, s_renderCamY = 0.f, s_renderCamZ = 0.f;
static bool  s_renderCamValid = false;
extern "C" void VR_SetRenderCamera(float wx, float wy, float wz) { s_renderCamX = wx; s_renderCamY = wy; s_renderCamZ = wz; s_renderCamValid = true; }
extern "C" bool VR_GetRenderCamera(float* wx, float* wy, float* wz)
{
	if (!s_renderCamValid) return false;
	if (wx) *wx = s_renderCamX; if (wy) *wy = s_renderCamY; if (wz) *wz = s_renderCamZ;
	return true;
}

// The head's HORIZONTAL position relative to the recenter reference, mapped to Marathon world units
// (map x,y). The engine applies this to the VIEW ORIGIN (clamped against walls) so leaning/walking
// moves the camera + visibility origin -- render-side only, never the physics position.
extern "C" void VR_GetHeadOffset(float* wx, float* wy)
{
	if (wx) *wx = 0; if (wy) *wy = 0;
	if (!s_headPoseValid) return;
	// Capture the standing eye height on the first valid pose if a recenter hasn't already (life-size scale).
	if (!s_standingEyeHeightInit) {
		s_standingEyeHeightM = s_stageFromHead.position.y; s_standingEyeHeightInit = true;
		recomputeWorldScale();
	}
	// s_lastHead is seeded by the frame loop on the first valid pose; until then the residual is 0.
	if (!s_headLatchInit) return;

	// RESIDUAL: the head's horizontal position relative to s_lastHead, the "absorbed" origin the body has
	// been moved up to (advanced every physics tick by VR_LatchHeadMove). Between ticks this grows with
	// live head motion so the render camera tracks the head smoothly at HMD rate; at each tick the body
	// absorbs it (with wall collision) and this returns toward 0. The body carries the bulk of any walk,
	// so this offset stays small -- no lean cap is needed (and a cap would fight real room-scale walking).
	// Applied to the VIEW ORIGIN engine-side, which independently wall-clamps it (screen.cpp), so peeking
	// still can't see through walls.
	const float dhx = s_stageFromHead.position.x - s_lastHeadX;
	const float dhz = s_stageFromHead.position.z - s_lastHeadZ;
	const float W  = s_settings.worldScaleWUM;
	const float yr = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);
	const float c  = std::cos(yr), s = std::sin(yr);
	float ox = W * (-c * dhx + s * dhz);
	float oy = W * (-s * dhx - c * dhz);

	// Safety clamp only (catastrophe guard for a bad pose / a tick that was skipped for many frames, e.g.
	// returning from a menu). Well beyond a single tick of real head motion; never engages in normal play.
	const float maxResidual = 1.5f * W;
	const float mag = std::sqrt(ox * ox + oy * oy);
	if (mag > maxResidual) { const float k = maxResidual / mag; ox *= k; oy *= k; }

	if (wx) *wx = ox;
	if (wy) *wy = oy;
}

// Render-camera head offset for the INTERPOLATED body (screen.cpp). The render camera is built as
// interpolated_body + this offset. The interpolated body LAGS the live head by up to one tick (it lerps
// the previous/current tick snapshots), so subtracting a head reference lerped the SAME way removes that
// lag: offset = map(live_head - lerp(prevTickHead, curTickHead, t)). With the body snapshots carried at
// sub-WU precision (interpolated_world.cpp) the tick-committed head-follow in the body cancels this
// reference exactly, leaving interpolated_body + offset == (interpolated non-head body) + LIVE head --
// continuous, no int16 stepping, no per-tick sawtooth. `t` is the same heartbeat_fraction the body used.
extern "C" void VR_GetHeadOffsetInterp(float t, float* wx, float* wy)
{
	if (wx) *wx = 0; if (wy) *wy = 0;
	if (!s_headPoseValid || !s_headLatchInit) return;
	if (t < 0.f) t = 0.f; else if (t > 1.f) t = 1.f;
	const float refX = s_prevHeadX + (s_lastHeadX - s_prevHeadX) * t;
	const float refZ = s_prevHeadZ + (s_lastHeadZ - s_prevHeadZ) * t;
	const float dhx = s_stageFromHead.position.x - refX;
	const float dhz = s_stageFromHead.position.z - refZ;
	const float W  = s_settings.worldScaleWUM;
	const float yr = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);
	const float c  = std::cos(yr), s = std::sin(yr);
	float ox = W * (-c * dhx + s * dhz);
	float oy = W * (-s * dhx - c * dhz);
	const float maxResidual = 1.5f * W;   // catastrophe guard only (skipped ticks / bad pose)
	const float mag = std::sqrt(ox * ox + oy * oy);
	if (mag > maxResidual) { const float k = maxResidual / mag; ox *= k; oy *= k; }
	if (wx) *wx = ox;
	if (wy) *wy = oy;
}
// Live lockout check (not just the s_fire latched in VR_BeginFrame): the keyboard-close that sets the
// lockout happens LATER in the same tick (vbl's VR_InGameKeyboardActive, before it reads VR_GetFire),
// so the latched s_fire is still the pre-lockout value on the transition frame -- re-test here.
extern "C" bool VR_GetFire(void)               { return s_fire && !s_kbFireLockout; }
extern "C" bool VR_GetSecondaryFire(void)
{
	if (s_kbFireLockout) return false;   // don't fire on the trigger that just clicked the console OK key
	const int domIdx = s_settings.dominantHand ? 0 : 1;
	const int offIdx = 1 - domIdx;
	// Off-hand trigger always fires secondary. In dual-wield this fires the second weapon;
	// for single weapons it allows weapon-mode transitions (e.g. raising the second pistol
	// when switching to dual). The original "dual-only" restriction broke the raise mechanic.
	if (s_trigger[offIdx] > 0.5f) return true;
	// Dominant grip fires alt-fire only for weapons with a distinct secondary (assault rifle
	// grenades, shotgun double-barrel). Suppressed for pistol/fist where secondary == primary.
	if (!s_gripAltFireEnabled) return false;
	return s_grip[domIdx] > 0.5f;
}
extern "C" bool VR_GetPrimaryPunch(void)
{
	if (!s_settings.punchWithFists) return false;
	const int domIdx = s_settings.dominantHand ? 0 : 1;   // dominant hand = primary weapon / first fist
	return s_punchActive[domIdx];
}
extern "C" bool VR_GetSecondaryPunch(void)
{
	if (!s_settings.punchWithFists) return false;
	const int offIdx = s_settings.dominantHand ? 1 : 0;   // off-hand = secondary trigger / second fist
	return s_punchActive[offIdx];
}
// Whether a velocity punch happened on the primary/off hand within the last ~200 ms. Used to tag a
// fist shot's provenance (thrust vs trigger) at the moment its firing animation begins.
extern "C" bool VR_PrimaryPunchRecent(void)
{
	if (!s_settings.punchWithFists) return false;
	const int domIdx = s_settings.dominantHand ? 0 : 1;
	return s_punchRecent[domIdx] > 0;
}
extern "C" bool VR_SecondaryPunchRecent(void)
{
	if (!s_settings.punchWithFists) return false;
	const int offIdx = s_settings.dominantHand ? 1 : 0;
	return s_punchRecent[offIdx] > 0;
}
extern "C" bool VR_GetAction(void)             { return s_action; }
extern "C" bool VR_GetAdvance(void)            { return s_action || s_xBtn; }   // A or X
extern "C" bool VR_GetBack(void)               { return s_yBtn || s_bBtn; }     // Y or B
extern "C" bool VR_GetButtonX(void)            { return s_xBtn; }
extern "C" bool VR_GetButtonY(void)            { return s_yBtn; }
// Press of the thumbstick you MOVE with (routed by handedness/switch-sticks, like the move stick).
extern "C" bool VR_GetMoveStickClick(void)
{
	const int domIdx = s_settings.dominantHand ? 0 : 1;
	const int offIdx = 1 - domIdx;
	const int moveIdx = s_settings.switchSticks ? domIdx : offIdx;
	return s_stickClick[moveIdx];
}
// Press of the thumbstick you TURN with (the opposite hand from the move stick).
extern "C" bool VR_GetTurnStickClick(void)
{
	const int domIdx = s_settings.dominantHand ? 0 : 1;
	const int offIdx = 1 - domIdx;
	const int turnIdx = s_settings.switchSticks ? offIdx : domIdx;   // opposite of moveIdx
	return s_stickClick[turnIdx];
}

// True while any bindable button mapped to `action` is held. Buttons are indexed by VR_BTN_*; the
// two stick-clicks are role-based (follow handedness) via the accessors above. Multiple buttons can
// share an action -- the states are OR'd.
extern "C" bool VR_ActionHeld(int action)
{
	if (action <= VR_ACT_NONE || action >= VR_ACT_COUNT) return false;
	const bool btn[VR_BTN_COUNT] = {
		s_action,                 // VR_BTN_A
		s_bBtn,                   // VR_BTN_B
		s_xBtn,                   // VR_BTN_X
		s_yBtn,                   // VR_BTN_Y
		VR_GetMoveStickClick(),   // VR_BTN_MOVE_CLICK
		VR_GetTurnStickClick(),   // VR_BTN_TURN_CLICK
	};
	for (int i = 0; i < VR_BTN_COUNT; ++i)
		if (s_settings.buttonAction[i] == action && btn[i]) return true;
	return false;
}

namespace {
	bool s_worldFramePresented = false;
	// World-locked 2D panel (placed in stage space when a menu/terminal is shown). Vectors in stage m.
	float s_panelC[3] = {0,0,0}, s_panelR[3] = {1,0,0}, s_panelU[3] = {0,1,0}, s_panelN[3] = {0,0,1};
	float s_panelHalfW = 1, s_panelHalfH = 1;
	bool  s_panelPlaced = false;
	// True while the 2D panel is showing a terminal rather than a menu. Pushed from screen.cpp.
	bool  s_panelIsTerminal = false;
	// Per-hand pointer result (computed in VR_PresentScreenLayer): each controller's ray hit on the panel.
	struct PtrHit { bool active; int x, y; float u, v; };
	PtrHit s_ptr[2] = {};
	int s_ptrClickHand = -1;  // hand locked for the current click (-1 = none)
}
extern "C" void VR_MarkWorldFramePresented(void) { s_worldFramePresented = true; s_panelPlaced = false; /* re-place the 2D panel next time a menu is shown */ }

// Reuses the system-recenter path, which VR_PresentScreenLayer already handles: it clears both the
// panel and the keyboard placement (the keyboard is positioned relative to the panel, so a resized
// panel must drag it along).
extern "C" void VR_InvalidatePanelPlacement(void) { s_panelRecenterPending = true; }

extern "C" void VR_SetTerminalActive(int active) { s_panelIsTerminal = (active != 0); }
extern "C" bool VR_TakeWorldFramePresented(void) { bool v = s_worldFramePresented; s_worldFramePresented = false; return v; }

// Menu pointer: the screen-layer pixel the dominant (or any) controller ray is hitting (false = no hit).
// s_ptrClickHand is locked at DOWN and held through UP so both events come from the same cursor position.
// VR_GetPointerScreen must be called before VR_GetPointerClick each frame (screen.cpp does this).
extern "C" bool VR_GetPointerScreen(int* x, int* y) {
	int dom = s_settings.dominantHand ? 0 : 1, off = 1 - dom;
	// If no click in progress, check whether one is starting now.
	if (s_ptrClickHand < 0) {
		const int order[2] = {dom, off};
		for (int i = 0; i < 2; ++i) {
			int h = order[i];
			if (s_ptr[h].active && s_trigger[h] > 0.5f) { s_ptrClickHand = h; break; }
		}
	}
	// Use locked click hand for consistent DOWN/UP position; VR_GetPointerClick clears lock on release.
	if (s_ptrClickHand >= 0 && s_ptr[s_ptrClickHand].active) {
		if (x) *x = s_ptr[s_ptrClickHand].x; if (y) *y = s_ptr[s_ptrClickHand].y;
		return true;
	}
	// Normal hover: prefer dominant hand, fall back to off-hand.
	const PtrHit& h = s_ptr[dom].active ? s_ptr[dom] : s_ptr[off];
	if (!h.active) return false;
	if (x) *x = h.x; if (y) *y = h.y;
	return true;
}
extern "C" bool VR_GetPointerClick(void) {
	if (s_ptrClickHand >= 0) {
		if (s_ptr[s_ptrClickHand].active && s_trigger[s_ptrClickHand] > 0.5f) return true;
		s_ptrClickHand = -1;  // trigger released: unlock hand
	}
	return false;
}
extern "C" bool VR_GetPointerGrip(void) {
	for (int h = 0; h < 2; ++h) if (s_ptr[h].active && s_grip[h] > 0.5f) return true;
	return false;
}

// ---------------------------------------------------------- screen layer ----
namespace {
	constexpr int kScreenW = 1280, kScreenH = 1024;   // matches change_screen_mode's VR vmode default
	GLuint s_screenFBO = 0, s_screenTex = 0, s_screenDepth = 0;
	GLuint s_quadProg = 0, s_quadVAO = 0, s_quadVBO = 0;
	GLint  s_quadTexLoc = -1, s_quadMVPLoc = -1, s_quadPhiLoc = -1, s_quadRLoc = -1;
	// A curve needs geometry to bend, and the flat quad is only two triangles, so the menu/terminal
	// panel gets its own horizontally-tessellated strip. Only x is subdivided -- a vertical cylinder
	// does not bend in y.
	GLuint s_panelStripVAO = 0, s_panelStripVBO = 0;
	enum { kPanelStripCols = 48, kPanelStripVerts = kPanelStripCols * 6 };
	GLuint s_curProg = 0; GLint s_curMVPLoc = -1, s_curColorLoc = -1;   // pointer cursor (solid colour)

	void ensureScreenLayer() {
		if (s_screenFBO) return;
		glGenTextures(1, &s_screenTex);
		glBindTexture(GL_TEXTURE_2D, s_screenTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kScreenW, kScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenRenderbuffers(1, &s_screenDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, s_screenDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kScreenW, kScreenH);
		glGenFramebuffers(1, &s_screenFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, s_screenFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_screenTex, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_screenDepth);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			A1VR_LOG("screen layer FBO incomplete");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// The panel is drawn flat, or bent around a vertical cylinder for a curved-monitor look.
		// The bend is applied here rather than baked into the vertex data, so the model matrix, the
		// UVs, and the flat users of this program (the HUD and loading screens) are all untouched --
		// they simply leave uCurvePhi at 0. aPos.x spans [-1,1] across the panel, and the model
		// matrix scales x by halfWidth and z by 1 (the unit panel normal, which points at the
		// viewer), so emitting R*sin(theta)/halfWidth and R*(1-cos(theta)) puts each vertex on a
		// cylinder of radius R whose axis runs through centre + R*normal: concave toward you.
		static const char* vs =
			"#version 300 es\n"
			"layout(location=0) in vec2 aPos;\n"
			"uniform mat4 uMVP;\n"
			"uniform float uCurvePhi;\n"
			"uniform float uCurveR;\n"
			"out vec2 vUV;\n"
			"void main(){\n"
			"  vUV = aPos*0.5+0.5;\n"
			"  vec3 p = vec3(aPos, 0.0);\n"
			"  if (uCurvePhi > 0.0001) {\n"
			"    float th = aPos.x * uCurvePhi;\n"
			"    p.x = sin(th) / uCurvePhi;\n"
			"    p.z = uCurveR * (1.0 - cos(th));\n"
			"  }\n"
			"  gl_Position = uMVP * vec4(p, 1.0);\n"
			"}\n";
		static const char* fs =
			"#version 300 es\n"
			"precision mediump float;\n"
			"in vec2 vUV; out vec4 o; uniform sampler2D uTex;\n"
			"void main(){ o = texture(uTex, vUV); }\n";
		GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
		s_quadProg = glCreateProgram();
		glAttachShader(s_quadProg, v); glAttachShader(s_quadProg, f); glLinkProgram(s_quadProg);
		glDeleteShader(v); glDeleteShader(f);
		s_quadTexLoc = glGetUniformLocation(s_quadProg, "uTex");
		s_quadMVPLoc = glGetUniformLocation(s_quadProg, "uMVP");
		s_quadPhiLoc = glGetUniformLocation(s_quadProg, "uCurvePhi");
		s_quadRLoc   = glGetUniformLocation(s_quadProg, "uCurveR");
		const float quad[] = { -1,-1,  1,-1,  1,1,  -1,-1,  1,1,  -1,1 };
		glGenVertexArrays(1, &s_quadVAO);
		glGenBuffers(1, &s_quadVBO);
		glBindVertexArray(s_quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindVertexArray(0);

		{
			std::vector<float> strip;
			strip.reserve(kPanelStripVerts * 2);
			for (int i = 0; i < kPanelStripCols; ++i) {
				const float x0 = -1.0f + 2.0f * (float)i / (float)kPanelStripCols;
				const float x1 = -1.0f + 2.0f * (float)(i + 1) / (float)kPanelStripCols;
				const float q[12] = { x0,-1, x1,-1, x1,1,  x0,-1, x1,1, x0,1 };
				strip.insert(strip.end(), q, q + 12);
			}
			glGenVertexArrays(1, &s_panelStripVAO);
			glGenBuffers(1, &s_panelStripVBO);
			glBindVertexArray(s_panelStripVAO);
			glBindBuffer(GL_ARRAY_BUFFER, s_panelStripVBO);
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(strip.size()*sizeof(float)), strip.data(), GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
			glBindVertexArray(0);
		}

		// Cursor program: a circle (reuses the same [-1,1] quad VAO; fragment discards outside unit circle).
		static const char* cvs =
			"#version 300 es\n"
			"layout(location=0) in vec2 aPos;\n"
			"out vec2 vPos;\n"
			"uniform mat4 uMVP;\n"
			"void main(){ vPos = aPos; gl_Position = uMVP * vec4(aPos,0.0,1.0); }\n";
		static const char* cfs =
			"#version 300 es\n"
			"precision mediump float;\n"
			"in vec2 vPos;\n"
			"uniform vec4 uColor; out vec4 o;\n"
			"void main(){ if(length(vPos)>1.0) discard; o = uColor; }\n";
		GLuint cv = compile(GL_VERTEX_SHADER, cvs), cf = compile(GL_FRAGMENT_SHADER, cfs);
		s_curProg = glCreateProgram();
		glAttachShader(s_curProg, cv); glAttachShader(s_curProg, cf); glLinkProgram(s_curProg);
		glDeleteShader(cv); glDeleteShader(cf);
		s_curMVPLoc   = glGetUniformLocation(s_curProg, "uMVP");
		s_curColorLoc = glGetUniformLocation(s_curProg, "uColor");
	}
}

// -------------------------------------------------------------- HUD layer ----
// The engine's 2D HUD (Lua plugin HUD or the classic OGL HUD) is drawn into this transparent RGBA
// offscreen each frame; VR_PresentHudEye then composites it head-locked, in front of the world, into
// each eye. Unlike the menu screen-layer (world-locked, opaque), the HUD follows the head and is
// alpha-blended so only the HUD elements occlude the scene.
namespace {
	constexpr int kHudW = 1280, kHudH = 1024;
	GLuint s_hudFBO = 0, s_hudTex = 0;

	void ensureHudLayer() {
		if (s_hudFBO) return;
		ensureScreenLayer();   // shares s_quadProg / s_quadVAO for the textured-quad draw
		glGenTextures(1, &s_hudTex);
		glBindTexture(GL_TEXTURE_2D, s_hudTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kHudW, kHudH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenFramebuffers(1, &s_hudFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, s_hudFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_hudTex, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			A1VR_LOG("HUD layer FBO incomplete");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

extern "C" unsigned VR_HudLayerFramebuffer(void) { ensureHudLayer(); return s_hudFBO; }
extern "C" int VR_HudLayerWidth(void)  { return kHudW; }
extern "C" int VR_HudLayerHeight(void) { return kHudH; }

// Composite the head-locked HUD plane into the currently-bound eye `eye`. Called inside the per-eye
// world-render loop, after the world+sprites, before VR_FinishEye. The plane rides with the head
// pose; its BOTTOM edge is anchored in the lower field of view (pitched down) and it grows UPWARD as
// hudSizeM increases (a "dashboard" -- so cranking the size doesn't run it off the bottom/sides).
// Width is capped to a comfortable horizontal FOV so wide HUDs don't spill past the binocular region.
extern "C" void VR_PresentHudEye(int eye)
{
	if (!s_active || !s_headPoseValid) return;
	ensureHudLayer();

	// Head basis in stage space: right/up/forward from the located head pose.
	float hm[16]; mat_from_pose(hm, s_stageFromHead);
	const float hp[3] = { s_stageFromHead.position.x, s_stageFromHead.position.y, s_stageFromHead.position.z };
	float R[3] = {  hm[0],  hm[1],  hm[2] };   // local +X (right)
	float U[3] = {  hm[4],  hm[5],  hm[6] };   // local +Y (up)
	float F[3] = { -hm[8], -hm[9], -hm[10] };  // local -Z (forward)

	// hudDistanceM places the panel and drives ONLY the stereo depth (vergence). Apparent (angular) size
	// is held constant as distance changes because the panel's physical extent scales with D (see hh/hw
	// below): move it closer/further and the ONLY thing that changes is the per-eye disparity, i.e. how
	// deep it sits. Apparent size is set independently by the HUD Size setting.
	const float D = s_settings.hudDistanceM;
	// Bottom-anchor: pitch the forward direction DOWN by hudTiltDeg so the HUD bottom sits in the lower
	// field of view. 0° = eye level, 30° = lower-dashboard look. Configurable in VR Options.
	const float pitch = s_settings.hudTiltDeg * (3.14159265358979f / 180.0f);
	const float cp = std::cos(pitch), sp = std::sin(pitch);
	const float Fb[3]     = { F[0]*cp - U[0]*sp, F[1]*cp - U[1]*sp, F[2]*cp - U[2]*sp };
	const float bottom[3] = { hp[0]+Fb[0]*D, hp[1]+Fb[1]*D, hp[2]+Fb[2]*D };  // bottom-centre anchor

	// Half-height scales with D so apparent size is distance-independent: hudSizeM is the panel height as
	// a FRACTION of the HUD distance (angular size = 2*atan(hudSizeM/2)). Bigger hudSizeM = bigger on
	// screen; distance no longer affects it.
	// Uniform trim so the corner readouts (health / oxygen / ammo) pull in toward centre where they're
	// easier to read, instead of sitting out in the periphery. This scales BOTH axes equally -> no
	// squash; the HUD art keeps its aspect and just sits a touch smaller/more central. 1.0 = untrimmed.
	const float kHudTrim = 0.88f;
	const float hh     = 0.5f * s_settings.hudSizeM * D * kHudTrim;  // half-height; grows up from anchor
	const float aspect = (float)kHudW/(float)kHudH;
	const float hw     = hh * aspect;                        // natural aspect (no cap = no squash)
	const float C[3]   = { bottom[0]+U[0]*hh, bottom[1]+U[1]*hh, bottom[2]+U[2]*hh };

	float model[16] = {
		R[0]*hw, R[1]*hw, R[2]*hw, 0,
		U[0]*hh, U[1]*hh, U[2]*hh, 0,
		-F[0],   -F[1],   -F[2],   0,   // plane normal faces the head (upright billboard)
		C[0],    C[1],    C[2],    1 };

	float proj[16];
	VR_GetEyeProjection(eye, proj, 0.05f, 50.0f);
	float stageFromEye[16]; mat_from_pose(stageFromEye, s_stageFromEye[eye]);
	float eyeFromStage[16]; mat_rigid_inverse(eyeFromStage, stageFromEye);
	float vp[16];  mat_mul(vp, proj, eyeFromStage);
	float mvp[16]; mat_mul(mvp, vp, model);

	// The world rasterizer leaves a view scissor + (possibly) a reduced viewport active; reset both so
	// the HUD quad isn't clipped to the world's view rect (this was chopping the HUD's left side).
	glViewport(0, 0, s_eyeW, s_eyeH);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // HUD pixels (alpha>0) over the world
	glUseProgram(s_quadProg);
	glUniformMatrix4fv(s_quadMVPLoc, 1, GL_FALSE, mvp);
	glUniform1f(s_quadPhiLoc, 0.0f);   // the head-locked HUD stays flat
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_hudTex);
	glUniform1i(s_quadTexLoc, 0);
	glBindVertexArray(s_quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	glDisable(GL_BLEND);
}

// ------------------------------------------------------------ Map layer ----
// When the player opens the overhead map in VR, it is rendered into this RGBA FBO; VR_PresentMapEye
// then composites it as a head-locked floating panel over the world each eye.  Unlike the desktop
// path (map replaces the world), in VR the world keeps rendering underneath the map overlay.
namespace {
	constexpr int kMapW = 1280, kMapH = 1024;
	GLuint s_mapFBO = 0, s_mapTex = 0;
	bool   s_vrMapActive      = false;
	bool   s_vrMapTranslucent = false;   // true=screen blend (black→transparent); false=solid panel

	void ensureMapLayer() {
		if (s_mapFBO) return;
		ensureScreenLayer();   // shares s_quadProg / s_quadVAO / s_quadMVPLoc / s_quadTexLoc
		glGenTextures(1, &s_mapTex);
		glBindTexture(GL_TEXTURE_2D, s_mapTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMapW, kMapH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenFramebuffers(1, &s_mapFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, s_mapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_mapTex, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			A1VR_LOG("Map layer FBO incomplete");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

extern "C" void     VR_SetMapActive(bool active, bool translucent) { s_vrMapActive = active; s_vrMapTranslucent = translucent; }
extern "C" unsigned VR_MapLayerFramebuffer(void) { ensureMapLayer(); return s_mapFBO; }
extern "C" int      VR_MapLayerWidth(void)  { return kMapW; }
extern "C" int      VR_MapLayerHeight(void) { return kMapH; }

// Composite the head-locked map panel into the currently-bound eye buffer.  Called inside the
// per-eye loop (render.cpp) after VR_PresentHudEye.  Only draws when the map is open.
extern "C" void VR_PresentMapEye(int eye)
{
	if (!s_active || !s_headPoseValid || !s_vrMapActive || !s_mapFBO) return;

	glViewport(0, 0, s_eyeW, s_eyeH);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CULL_FACE);

	if (!s_vrMapTranslucent)
	{
		// Solid mode: wipe the eye entirely to black so the world is invisible, then draw the
		// map panel opaquely over it (matches the desktop experience of the map replacing the world).
		glDisable(GL_DEPTH_TEST);
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	// Head basis in stage space.
	float hm[16]; mat_from_pose(hm, s_stageFromHead);
	const float hp[3] = { s_stageFromHead.position.x, s_stageFromHead.position.y, s_stageFromHead.position.z };
	const float R[3] = {  hm[0],  hm[1],  hm[2] };
	const float U[3] = {  hm[4],  hm[5],  hm[6] };
	const float F[3] = { -hm[8], -hm[9], -hm[10] };

	// Centre the panel 5° below the horizon at 2.5 m -- comfortable reading distance.
	const float D = 2.5f;
	const float tilt = 5.0f * (3.14159265358979f / 180.0f);
	const float ct = std::cos(tilt), st = std::sin(tilt);
	const float Fd[3] = { F[0]*ct - U[0]*st, F[1]*ct - U[1]*st, F[2]*ct - U[2]*st };
	const float C[3]  = { hp[0]+Fd[0]*D, hp[1]+Fd[1]*D, hp[2]+Fd[2]*D };

	// Panel half-extents: 0.8 m half-height, aspect-correct width from kMapW/kMapH.
	const float hh = 0.8f;
	const float hw = hh * (float)kMapW / (float)kMapH;

	float model[16] = {
		R[0]*hw, R[1]*hw, R[2]*hw, 0,
		U[0]*hh, U[1]*hh, U[2]*hh, 0,
		-F[0],   -F[1],   -F[2],   0,
		C[0],    C[1],    C[2],    1 };

	float proj[16];
	VR_GetEyeProjection(eye, proj, 0.05f, 50.0f);
	float stageFromEye[16]; mat_from_pose(stageFromEye, s_stageFromEye[eye]);
	float eyeFromStage[16]; mat_rigid_inverse(eyeFromStage, stageFromEye);
	float vp[16];  mat_mul(vp, proj, eyeFromStage);
	float mvp[16]; mat_mul(mvp, vp, model);

	glDisable(GL_DEPTH_TEST);
	glUseProgram(s_quadProg);
	glUniform1f(s_quadPhiLoc, 0.0f);   // flat: only the menu/terminal panel curves
	glUniformMatrix4fv(s_quadMVPLoc, 1, GL_FALSE, mvp);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_mapTex);
	glUniform1i(s_quadTexLoc, 0);
	glBindVertexArray(s_quadVAO);

	if (s_vrMapTranslucent)
	{
		// Screen blend: GL_ONE + GL_ONE_MINUS_SRC_COLOR = Photoshop Screen.
		// Black map pixels (0,0,0) leave the world untouched; coloured map lines brighten the world.
		// This means only the coloured lines/icons float visibly over the scene.
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisable(GL_BLEND);
	}
	else
	{
		// Solid mode: plain opaque draw (world was already wiped above).
		glDisable(GL_BLEND);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBindVertexArray(0);
}

// ----------------------------------------------------------- dim pass ----
namespace {
	GLuint s_dimProg = 0, s_dimVAO = 0, s_dimVBO = 0;
	GLint  s_dimColorLoc = -1;

	void ensureDim() {
		if (s_dimProg) return;
		static const char* vs =
			"#version 300 es\n"
			"layout(location=0) in vec2 aPos;\n"
			"void main(){ gl_Position = vec4(aPos,0.0,1.0); }\n";
		static const char* fs =
			"#version 300 es\n"
			"precision mediump float;\n"
			"uniform vec4 uColor; out vec4 o;\n"
			"void main(){ o = uColor; }\n";
		GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
		s_dimProg = glCreateProgram();
		glAttachShader(s_dimProg, v); glAttachShader(s_dimProg, f); glLinkProgram(s_dimProg);
		glDeleteShader(v); glDeleteShader(f);
		s_dimColorLoc = glGetUniformLocation(s_dimProg, "uColor");
		const float quad[] = { -1,-1,  1,-1,  1,1,  -1,-1,  1,1,  -1,1 };
		glGenVertexArrays(1, &s_dimVAO);
		glGenBuffers(1, &s_dimVBO);
		glBindVertexArray(s_dimVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_dimVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindVertexArray(0);
	}
}

extern "C" void VR_Vibrate(int hand, float durationMs, float amplitude)
{
	if (hand < 0 || hand > 1) return;
	if (s_vibDuration[hand] > 0.0f) return; // already queued for this frame
	s_vibDuration[hand]  = durationMs;
	s_vibIntensity[hand] = amplitude;
}

extern "C" void VR_DimCurrentEye(void)
{
	const float b = s_settings.brightness;
	if (b >= 0.999f) return;
	ensureDim();
	glBindFramebuffer(GL_FRAMEBUFFER, s_eye[s_curEye].fbos[s_curEyeImageIdx]);
	glViewport(0, 0, s_eyeW, s_eyeH);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);   // result = dst * srcColor  -> multiply the eye by (b,b,b)
	glUseProgram(s_dimProg);
	glUniform4f(s_dimColorLoc, b, b, b, 1.0f);
	glBindVertexArray(s_dimVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	glDisable(GL_BLEND);
}

extern "C" unsigned VR_ScreenLayerFramebuffer(void) { ensureScreenLayer(); return s_screenFBO; }
extern "C" int VR_ScreenLayerWidth(void)  { return kScreenW; }
extern "C" int VR_ScreenLayerHeight(void) { return kScreenH; }

// Bind the scratch (screen-layer) FBO as the render target for a level-load warmup pass. Sets the
// current eye so Rasterizer SetView picks up eye 0's (stale but valid) projection/view. No swapchain is
// acquired and no OpenXR frame is begun, so nothing is held against the compositor while the ~2 s of
// lazy texture uploads runs. The rendered result is discarded -- only the texture uploads matter.
extern "C" void VR_BeginWarmupEye(int eye)
{
	s_curEye = eye;
	ensureScreenLayer();
	glBindFramebuffer(GL_FRAMEBUFFER, s_screenFBO);
	glDisable(GL_SCISSOR_TEST);
	glViewport(0, 0, kScreenW, kScreenH);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

extern "C" void VR_EndWarmup(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

namespace {
	void vsub(const float* a, const float* b, float* o){ o[0]=a[0]-b[0]; o[1]=a[1]-b[1]; o[2]=a[2]-b[2]; }
	float vdot(const float* a, const float* b){ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
	void vcross(const float* a, const float* b, float* o){ o[0]=a[1]*b[2]-a[2]*b[1]; o[1]=a[2]*b[0]-a[0]*b[2]; o[2]=a[0]*b[1]-a[1]*b[0]; }
	void vnorm(float* v){ float l=std::sqrt(vdot(v,v)); if(l>1e-6f){ v[0]/=l; v[1]/=l; v[2]/=l; } }
	void poseFwd(const XrPosef& p, float* d){ float m[16]; mat_from_pose(m,p); d[0]=-m[8]; d[1]=-m[9]; d[2]=-m[10]; }

	// Place the 2D panel fixed in stage space, in front of the head's horizontal facing (so it stays
	// put while you point at it / look around). Called once each time a 2D screen is (re)entered.
	void placePanel() {
		float hp[3] = { s_stageFromHead.position.x, s_stageFromHead.position.y, s_stageFromHead.position.z };
		float hf[3]; poseFwd(s_stageFromHead, hf); hf[1]=0; vnorm(hf);
		if (hf[0]==0 && hf[2]==0) { hf[2]=-1; }
		const float D = s_settings.screenDistanceM;
		s_panelC[0]=hp[0]+hf[0]*D; s_panelC[1]=hp[1]; s_panelC[2]=hp[2]+hf[2]*D;
		s_panelN[0]=-hf[0]; s_panelN[1]=0; s_panelN[2]=-hf[2];           // faces the head
		const float up[3]={0,1,0};
		vcross(up, s_panelN, s_panelR); vnorm(s_panelR);
		vcross(s_panelN, s_panelR, s_panelU); vnorm(s_panelU);
		s_panelHalfH = 0.5f*s_settings.screenHeightM;
		s_panelHalfW = s_panelHalfH * (float)kScreenW/(float)kScreenH;
		s_panelPlaced = true;
		// DIAG (menu-out-of-alignment): persistent log of the head pose the panel was placed from and the
		// resulting panel centre. If the menu is still off after this, the numbers show whether the head
		// stage-pose was sane (e.g. corrupted by crossing the guardian) or the placement math is at fault.
		// SHIP-DISABLED (store test build): writes to the on-device log file; re-enable to keep debugging.
		//logWarning("VRpanel place head=(%.2f,%.2f,%.2f) fwdH=(%.2f,%.2f) C=(%.2f,%.2f,%.2f) D=%.2f hh=%.2f hw=%.2f",
		//	hp[0], hp[1], hp[2], hf[0], hf[2], s_panelC[0], s_panelC[1], s_panelC[2], D, s_panelHalfH, s_panelHalfW);
	}

	// Intersect each controller aim ray with the panel independently; both can hit at once.
	// Arc half-angle of the panel's curve, radians. 0 = flat. curvature = distance/radius, so the
	// arc half-angle is halfWidth/R = halfWidth*curvature/distance.
	float panelCurvePhi() {
		const float c = s_settings.panelCurvature;
		if (c <= 0.001f || s_settings.screenDistanceM <= 0.01f) return 0.0f;
		return s_panelHalfW * c / s_settings.screenDistanceM;
	}
	float panelCurveR() {
		const float phi = panelCurvePhi();
		return (phi > 0.0001f) ? (s_panelHalfW / phi) : 0.0f;
	}

	// Panel-local (u,v) in [-1,1] -> stage position, following the curve so the cursor and the
	// hit-test sit on the same surface the shader draws.
	void panelPointAt(float u, float v, float outp[3]) {
		const float phi = panelCurvePhi();
		float along, depth;
		if (phi > 0.0001f) {
			const float R = s_panelHalfW / phi, th = u * phi;
			along = R * std::sin(th);
			depth = R * (1.0f - std::cos(th));
		} else {
			along = u * s_panelHalfW;
			depth = 0.0f;
		}
		for (int i = 0; i < 3; ++i)
			outp[i] = s_panelC[i] + along*s_panelR[i] + (v*s_panelHalfH)*s_panelU[i] + depth*s_panelN[i];
	}

	void updatePointer() {
		for (int h = 0; h < 2; ++h) {
			s_ptr[h].active = false;
			if (!s_aimValid[h]) continue;
			float O[3] = { s_aimStage[h].position.x, s_aimStage[h].position.y, s_aimStage[h].position.z };
			float Dd[3]; poseFwd(s_aimStage[h], Dd);
			float u, v;
			const float phi = panelCurvePhi();
			if (phi > 0.0001f) {
				// Curved: intersect the cylinder the shader bends the panel onto, so the pointer
				// lands where the image actually is. Axis is vertical (panelUp) through
				// centre + R*normal; solve in the plane perpendicular to the axis.
				const float R = s_panelHalfW / phi;
				float A[3]; for (int i=0;i<3;++i) A[i] = s_panelC[i] + R*s_panelN[i];
				float oa[3]; vsub(O, A, oa);
				const float oaU = vdot(oa, s_panelU), dU = vdot(Dd, s_panelU);
				float o2[3], d2[3];
				for (int i=0;i<3;++i) { o2[i] = oa[i] - oaU*s_panelU[i]; d2[i] = Dd[i] - dU*s_panelU[i]; }
				const float a = vdot(d2,d2), b = 2.0f*vdot(o2,d2), c = vdot(o2,o2) - R*R;
				if (a < 1e-8f) continue;
				const float disc = b*b - 4.0f*a*c;
				if (disc < 0.0f) continue;
				const float sq = std::sqrt(disc);
				const float t = (-b + sq) / (2.0f*a);       // far root: the viewer is inside the cylinder
				if (t <= 0.0f) continue;
				float hit[3] = { O[0]+Dd[0]*t, O[1]+Dd[1]*t, O[2]+Dd[2]*t };
				float ha[3]; vsub(hit, A, ha);
				// P - A = R*sin(th)*right - R*cos(th)*normal
				const float th = std::atan2(vdot(ha, s_panelR), -vdot(ha, s_panelN));
				u = th / phi;
				v = vdot(ha, s_panelU) / s_panelHalfH;
			} else {
				float denom = vdot(Dd, s_panelN);
				if (denom > -1e-4f) continue;
				float oc[3]; vsub(s_panelC, O, oc);
				float t = vdot(oc, s_panelN)/denom;
				if (t <= 0) continue;
				float hit[3] = { O[0]+Dd[0]*t, O[1]+Dd[1]*t, O[2]+Dd[2]*t };
				float loc[3]; vsub(hit, s_panelC, loc);
				u = vdot(loc, s_panelR)/s_panelHalfW;
				v = vdot(loc, s_panelU)/s_panelHalfH;
			}
			if (u<-1||u>1||v<-1||v>1) continue;
			s_ptr[h].active = true; s_ptr[h].u = u; s_ptr[h].v = v;
			s_ptr[h].x = (int)((u*0.5f+0.5f) * kScreenW);
			s_ptr[h].y = (int)((1.0f-(v*0.5f+0.5f)) * kScreenH);
		}
	}
}

extern "C" bool VR_GetAimPoseStage(int hand, float pos3[3], float fwd3[3])
{
	if (hand < 0 || hand > 1 || !s_aimValid[hand]) return false;
	// Position from grip pose (physical hand location); fall back to aim if grip unavailable.
	const XrPosef& pp = s_handValid[hand] ? s_handStage[hand] : s_aimStage[hand];
	pos3[0] = pp.position.x;
	pos3[1] = pp.position.y;
	pos3[2] = pp.position.z;
	// Forward = the pose's -Z, pitched by aimPitchAdjust about the pose's local right axis (the OpenXR
	// aim pose points higher than a held-gun barrel; negative tilts down). m: col0=right, col1=up,
	// col2=back. fwd = sin(th)*up - cos(th)*back  (th=0 -> -back -> plain poseFwd).
	float m[16]; mat_from_pose(m, s_aimStage[hand]);
	const float th = s_settings.aimPitchAdjust * (3.14159265358979f / 180.0f);
	const float st = std::sin(th), ct = std::cos(th);
	fwd3[0] = st*m[4] - ct*m[8];
	fwd3[1] = st*m[5] - ct*m[9];
	fwd3[2] = st*m[6] - ct*m[10];
	return true;
}

// Returns the raw aim-pose right (+X, col0) and up (+Y, col1) vectors in stage space.
// Does NOT apply aimPitchAdjust — those axes track the true physical controller orientation.
extern "C" bool VR_GetAimOrientStage(int hand, float right3[3], float up3[3])
{
	if (hand < 0 || hand > 1 || !s_aimValid[hand]) return false;
	float m[16]; mat_from_pose(m, s_aimStage[hand]);
	right3[0] = m[0]; right3[1] = m[1]; right3[2] = m[2];  // col0
	up3[0]    = m[4]; up3[1]    = m[5]; up3[2]    = m[6];  // col1
	return true;
}

extern "C" void VR_SetIsDualWield(bool dual) { s_isDualWield = dual; }
extern "C" void VR_SetTwoHandedDisabled(bool disabled) { s_twoHandedDisabled = disabled; }
extern "C" void VR_SetTwoHandedOffset(float pitch_deg, float yaw_deg) { s_twoHandedPitchDeg = pitch_deg; s_twoHandedYawDeg = yaw_deg; }
extern "C" void VR_SetOffHandHasWeapon(bool has) { s_offHandHasWeapon = has; }
extern "C" void VR_SetGripAltFireEnabled(bool en) { s_gripAltFireEnabled = en; }

extern "C" bool VR_IsTwoHandedActive()
{
	if (s_isDualWield) return false;   // dual-wield uses both hands independently
	if (s_twoHandedDisabled) return false;   // weapon opted out of two-handed grip (MML no_two_handed_weapon)
	if (!s_twoHandedLatched) return false;   // latched on grip edge; see VR_BeginFrame
	const int domHand = s_settings.dominantHand ? 0 : 1;
	const int offHand = 1 - domHand;
	return s_aimValid[domHand] && s_aimValid[offHand];
}

// Two-handed aim = the pure inter-hand vector (off-hand - dominant), normalized -- QuestZDoom's
// weaponStabilised path (VrInputDefault.cpp): dominant hand is the rear/trigger grip, off-hand is
// forward on the barrel, so (off - dom) IS the barrel forward. Full range, no hemisphere guard (that
// caused a 180 "faces backwards" flip when the dominant controller rotated past perpendicular) and no
// separation switch. Positions come from twoHandPositions, which (matching QZD) uses the AIM-pose
// origins. Falls back to the dominant aim forward only when the hand positions are unavailable.
// Per-weapon two-handed angle offset (MML <two_handed_offset>). Rotates the two-handed forward by the
// configured pitch (about the frame's right axis -> muzzle up/down) and yaw (about world-up -> sideways
// swing), so grips that don't lie along the barrel still aim/model correctly (pistol points up, the
// flamethrower's 90-side handle swings back onto the barrel). The yaw is mirrored for left-handed
// players (the model is X-flipped, so the off-hand handle sits on the opposite side); pitch is
// symmetric and never flips. Identity when both offsets are 0, so unconfigured weapons are unchanged.
static void applyTwoHandedOffset(float fwd[3])
{
	if (s_twoHandedPitchDeg == 0.f && s_twoHandedYawDeg == 0.f) return;

	const float d2r = 3.14159265358979f / 180.0f;
	const float pitch = s_twoHandedPitchDeg * d2r;
	// Left-handed: the horizontal (side) offset mirrors with the X-flipped model.
	const float yaw   = (s_settings.dominantHand ? -s_twoHandedYawDeg : s_twoHandedYawDeg) * d2r;

	auto cross = [](const float a[3], const float b[3], float o[3]) {
		o[0]=a[1]*b[2]-a[2]*b[1]; o[1]=a[2]*b[0]-a[0]*b[2]; o[2]=a[0]*b[1]-a[1]*b[0];
	};
	auto norm = [](float v[3]) {
		const float l = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
		if (l > 1e-6f) { v[0]/=l; v[1]/=l; v[2]/=l; }
	};
	// Rotate v about unit axis k by angle (Rodrigues).
	auto rot = [&](float v[3], const float k[3], float ang) {
		const float c = std::cos(ang), s = std::sin(ang);
		float kv[3]; cross(k, v, kv);
		const float kd = (k[0]*v[0]+k[1]*v[1]+k[2]*v[2]) * (1.0f - c);
		v[0] = v[0]*c + kv[0]*s + k[0]*kd;
		v[1] = v[1]*c + kv[1]*s + k[1]*kd;
		v[2] = v[2]*c + kv[2]*s + k[2]*kd;
	};

	const float up0[3] = { 0.f, 1.f, 0.f };   // stage up (Y-up)
	float right[3]; cross(fwd, up0, right);
	if (std::sqrt(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]) < 1e-4f) {
		// Forward ~vertical -> cross with up degenerates; fall back to stage X for a stable pitch axis.
		right[0]=1.f; right[1]=0.f; right[2]=0.f;
	} else {
		norm(right);
	}
	// Yaw about world-up first (carry the right axis), then pitch about the (updated) right axis.
	rot(fwd,   up0, yaw);
	rot(right, up0, yaw);
	rot(fwd,   right, pitch);
	norm(fwd);
}

extern "C" bool VR_GetTwoHandedFwdStage(float fwd3[3])
{
	const int domHand = s_settings.dominantHand ? 0 : 1;
	const int offHand = 1 - domHand;

	float dm[3], of[3];
	if (twoHandPositions(domHand, offHand, dm, of, nullptr)) {
		float ih[3] = { of[0]-dm[0], of[1]-dm[1], of[2]-dm[2] };
		const float l = std::sqrt(ih[0]*ih[0] + ih[1]*ih[1] + ih[2]*ih[2]);
		if (l > 1e-5f) {
			fwd3[0] = ih[0]/l; fwd3[1] = ih[1]/l; fwd3[2] = ih[2]/l;
			applyTwoHandedOffset(fwd3);
			return true;
		}
	}
	// No usable inter-hand vector -> dominant controller's own aim forward.
	float domPos[3];
	if (!VR_GetAimPoseStage(domHand, domPos, fwd3)) return false;
	applyTwoHandedOffset(fwd3);
	return true;
}

// Diagnostic accessor: per-hand grip tracking state + linear speed (m/s), for logging occlusion /
// dead-reckoning during two-handed holds. Returns false for an invalid hand index.
extern "C" bool VR_GetHandTracking(int hand, bool* tracked, float* speed)
{
	if (hand < 0 || hand > 1) return false;
	if (tracked) *tracked = s_handTracked[hand];
	if (speed)   *speed   = s_handSpeed[hand];
	return true;
}

extern "C" bool VR_GetHeadPosStage(float pos3[3])
{
	if (!s_headPoseValid) return false;
	pos3[0] = s_stageFromHead.position.x;
	pos3[1] = s_stageFromHead.position.y;
	pos3[2] = s_stageFromHead.position.z;
	return true;
}

// Shared helper: stage-space aim forward -> Marathon-world unit vector.
static bool aimStageToWorld(int hand, float dir[3])
{
	float pos[3], fwd[3];
	if (!VR_GetAimPoseStage(hand, pos, fwd)) return false;   // fwd already has aimPitchAdjust applied
	// stage(metres,Y-up) forward -> world(WU,Z-up): Z^T maps (sx,sy,sz)->(-sx,-sz,sy), then Rz(yawOffset).
	const float zx = -fwd[0], zy = -fwd[2], zz = fwd[1];
	const float yr = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);   // 512 = Marathon FULL_CIRCLE
	const float cw = std::cos(yr), sw = std::sin(yr);
	dir[0] = zx*cw - zy*sw;
	dir[1] = zx*sw + zy*cw;
	dir[2] = zz;
	const float l = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
	if (l > 1e-6f) { dir[0]/=l; dir[1]/=l; dir[2]/=l; }
	return true;
}

// The DOMINANT controller's aim direction as a Marathon-world unit vector (x/y horizontal, z up) --
// the same mapping the aim-debug ray uses, so weapons fire exactly where the ray points. Returns
// false if the controller pose isn't tracked. dir = Rz(yawOffset) * Z^T * aimForward(stage).
extern "C" bool VR_GetWeaponAim(float dir[3])
{
	// Two-handed steadying: aim along the inter-hand vector instead of single controller
	if (VR_IsTwoHandedActive()) {
		float fwd[3];
		if (VR_GetTwoHandedFwdStage(fwd)) {
			const float zx = -fwd[0], zy = -fwd[2], zz = fwd[1];
			const float yr = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);
			const float cw = std::cos(yr), sw = std::sin(yr);
			dir[0] = zx*cw - zy*sw;
			dir[1] = zx*sw + zy*cw;
			dir[2] = zz;
			const float l = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
			if (l > 1e-6f) { dir[0]/=l; dir[1]/=l; dir[2]/=l; }
			return true;
		}
	}
	const int hand = s_settings.dominantHand ? 0 : 1;   // dominant hand holds the weapon
	return aimStageToWorld(hand, dir);
}

// Off-hand (non-dominant) aim direction -- same transform as VR_GetWeaponAim but for the other
// controller, so the secondary weapon fires along the left gun when dual-wielding.
extern "C" bool VR_GetSecondaryWeaponAim(float dir[3])
{
	const int hand = s_settings.dominantHand ? 1 : 0;   // off-hand (opposite of dominant)
	return aimStageToWorld(hand, dir);
}

// Offset from the head to a controller, in world units and game axes. Mirrors the transform
// render_vr_aim_reticle uses for the laser sight (controller pos - head pos, scaled by worldScale and
// rotated by the yaw offset), so the laser and the bullet start from the same point instead of
// running as parallel rays from the hand and the eye.
static bool handOffsetFromHead(int hand, float* wx, float* wy, float* wz)
{
	if (wx) *wx = 0.0f; if (wy) *wy = 0.0f; if (wz) *wz = 0.0f;
	float hp[3], ps[3], fs[3];
	if (!VR_GetHeadPosStage(hp)) return false;
	if (!VR_GetAimPoseStage(hand, ps, fs)) return false;
	const float dx = ps[0] - hp[0];
	const float dy = ps[1] - hp[1];
	const float dz = ps[2] - hp[2];
	const float W  = s_settings.worldScaleWUM;
	const float yr = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);
	const float c  = std::cos(yr), sn = std::sin(yr);
	if (wx) *wx = W * (-c  * dx + sn * dz);
	if (wy) *wy = W * (-sn * dx - c  * dz);
	if (wz) *wz = W * dy;
	return true;
}

extern "C" bool VR_GetWeaponOriginOffset(float* wx, float* wy, float* wz)
{
	// Dominant hand -- matches VR_GetWeaponAim, including while two-handed (the model and the grip
	// still sit on the dominant controller; only the aim DIRECTION comes from the inter-hand vector).
	return handOffsetFromHead(s_settings.dominantHand ? 0 : 1, wx, wy, wz);
}

extern "C" bool VR_GetSecondaryWeaponOriginOffset(float* wx, float* wy, float* wz)
{
	return handOffsetFromHead(s_settings.dominantHand ? 1 : 0, wx, wy, wz);
}

// Menu (hamburger) button press, consumed once. The main loop turns this into the in-game quit dialog.
extern "C" bool VR_TakeMenuButton(void) { bool v = s_menuLatch; s_menuLatch = false; return v; }

// True only while the OpenXR session is FOCUSED (the app is the active immersive app). Drops when the
// user presses the Meta/home button and goes to the system overlay -> the engine pauses on that.
extern "C" bool VR_HasFocus(void) { return s_sessionState == XR_SESSION_STATE_FOCUSED; }

// ----------------------------------------------------- on-screen keyboard ----
// A second world-locked quad, placed below the menu panel and reclined ~30 degrees ("almost
// billboarded"), that lets the controller laser type into dialog text fields (menus/preferences).
// Keys are clickable regions; a press synthesizes the same SDL_TEXTINPUT / SDL_KEYDOWN events a
// physical keyboard would, which the dialog text-entry widgets already consume. Shown whenever SDL
// text input is active (i.e. a w_text_entry is focused) during a 2D menu frame. Not drawn in-game
// (VR_PresentScreenLayer only runs for menu/loading frames), so this is strictly menu-time input.
namespace {
	enum { KB_ALPHA = 0, KB_NUMERIC = 1, KB_IP = 2 };
	enum KbAct { KA_CHAR, KA_BKSP, KA_ENTER, KA_SHIFT, KA_SYMBOLS, KA_LEFT, KA_RIGHT, KA_SPACE, KA_HIDE };
	struct KbKey { float x, y, w, h; KbAct act; char lo, hi; const char* label; };

	constexpr int kKbW = 1024, kKbH = 512;   // keyboard texture size (px)
	// --- placement tunables (code-only for v1; tweak + rebuild) ---
	constexpr float kKbReclineDeg = 30.0f;   // lay-back from the menu's vertical plane
	constexpr float kKbGapM       = 0.12f;   // gap below the menu panel's bottom edge (metres)
	constexpr float kKbForwardM   = 0.30f;   // brought toward the user from the menu plane (metres)
	constexpr float kKbWidthScale = 1.0f;    // keyboard width relative to the menu panel width

	int    s_kbMode = KB_ALPHA;
	bool   s_kbShift = false, s_kbSymbols = false;
	KbKey  s_kbKeys[64];
	int    s_kbKeyCount = 0;
	int    s_kbLayoutMode = -1; bool s_kbLayoutShift = false, s_kbLayoutSymbols = false;
	int    s_kbHover = -1;
	bool   s_kbDirty = true;
	bool   s_kbTrigPrev = false;

	GLuint       s_kbTex  = 0;
	SDL_Surface* s_kbSurf = nullptr;

	float s_kbC[3], s_kbR[3], s_kbU[3], s_kbN[3];
	float s_kbHalfW = 0.5f, s_kbHalfH = 0.25f;
	bool  s_kbPlaced = false;

	struct KbPtr { bool active; float u, v; int key; };
	KbPtr s_kbPtr[2] = {};

	// Visibility follows SDL's text-input state, which the dialog layer manages across every transition:
	// focus a text field -> SDL_StartTextInput; switch/leave a field or close the dialog
	// (dialog::finish) -> SDL_StopTextInput. VR_SetKeyboardInputHint picks the layout on focus.
	//
	// We AND that with an explicit focus flag as a second guard: SDL text input can be left stale by a
	// dialog that pre-activates its own text field (network gather/join/lobby chat), and if that ever
	// slips through, requiring BOTH signals keeps a stale flag from resurrecting the keyboard over a
	// menu that has no focused field. s_kbFocusActive is set on focus, cleared on blur / explicit hide.
	bool s_kbFocusActive = false;
	bool kbVisible() { return s_kbFocusActive && SDL_IsTextInputActive() == SDL_TRUE; }

	void kbAddKey(float x, float y, float w, float h, KbAct act, char lo, char hi, const char* label) {
		if (s_kbKeyCount < (int)(sizeof s_kbKeys / sizeof s_kbKeys[0]))
			s_kbKeys[s_kbKeyCount++] = KbKey{ x, y, w, h, act, lo, hi, label };
	}
	void kbAddCharRow(const char* chars, float y, float rh, float startX, float cw) {
		for (int i = 0; chars[i]; ++i) {
			char c = chars[i];
			char up = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
			kbAddKey(startX + i * cw, y, cw, rh, KA_CHAR, c, up, nullptr);
		}
	}
	void kbBuildLayout() {
		s_kbKeyCount = 0;
		if (s_kbMode == KB_ALPHA) {
			const float rh = 1.0f / 5.0f;
			kbAddCharRow("1234567890", 0*rh, rh, 0.0f, 0.1f);
			if (!s_kbSymbols) {
				kbAddCharRow("qwertyuiop", 1*rh, rh, 0.0f,  0.1f);
				kbAddCharRow("asdfghjkl",  2*rh, rh, 0.05f, 0.1f);
				kbAddKey(0.0f,  3*rh, 0.15f, rh, KA_SHIFT, 0, 0, s_kbShift ? "SHFT" : "shft");
				kbAddCharRow("zxcvbnm", 3*rh, rh, 0.15f, 0.1f);
				kbAddKey(0.85f, 3*rh, 0.15f, rh, KA_BKSP, 0, 0, "<-");
			} else {
				kbAddCharRow("@#$_&-+()/", 1*rh, rh, 0.0f,  0.1f);
				kbAddCharRow("*\"':;!?\\|", 2*rh, rh, 0.05f, 0.1f);
				kbAddKey(0.0f, 3*rh, 0.15f, rh, KA_CHAR, '=', '=', nullptr);
				kbAddCharRow("<>[]{}~", 3*rh, rh, 0.15f, 0.1f);
				kbAddKey(0.85f, 3*rh, 0.15f, rh, KA_BKSP, 0, 0, "<-");
			}
			const float y4 = 4*rh;
			kbAddKey(0.00f, y4, 0.15f, rh, KA_SYMBOLS, 0, 0, s_kbSymbols ? "ABC" : "?123");
			kbAddKey(0.15f, y4, 0.15f, rh, KA_HIDE,  0, 0, "hide");
			kbAddKey(0.30f, y4, 0.30f, rh, KA_SPACE, ' ', ' ', "space");
			kbAddKey(0.60f, y4, 0.10f, rh, KA_CHAR, '.', '.', nullptr);
			kbAddKey(0.70f, y4, 0.10f, rh, KA_LEFT,  0, 0, "<");
			kbAddKey(0.80f, y4, 0.10f, rh, KA_RIGHT, 0, 0, ">");
			kbAddKey(0.90f, y4, 0.10f, rh, KA_ENTER, 0, 0, "OK");
		} else {
			const float rh = 1.0f / 5.0f, cw = 0.25f;   // 5 rows: 3 digit rows, an entry row, a hide bar
			kbAddCharRow("123", 0*rh, rh, 0.0f, cw); kbAddKey(0.75f, 0*rh, cw, rh, KA_BKSP,  0, 0, "<-");
			kbAddCharRow("456", 1*rh, rh, 0.0f, cw); kbAddKey(0.75f, 1*rh, cw, rh, KA_LEFT,  0, 0, "<");
			kbAddCharRow("789", 2*rh, rh, 0.0f, cw); kbAddKey(0.75f, 2*rh, cw, rh, KA_RIGHT, 0, 0, ">");
			if (s_kbMode == KB_IP) {
				kbAddKey(0.00f, 3*rh, cw, rh, KA_CHAR, '.', '.', nullptr);
				kbAddKey(0.25f, 3*rh, cw, rh, KA_CHAR, '0', '0', nullptr);
				kbAddKey(0.50f, 3*rh, cw, rh, KA_CHAR, ':', ':', nullptr);
				kbAddKey(0.75f, 3*rh, cw, rh, KA_ENTER, 0, 0, "OK");
			} else {
				kbAddKey(0.00f, 3*rh, 0.75f, rh, KA_CHAR, '0', '0', nullptr);
				kbAddKey(0.75f, 3*rh, 0.25f, rh, KA_ENTER, 0, 0, "OK");
			}
			kbAddKey(0.0f, 4*rh, 1.0f, rh, KA_HIDE, 0, 0, "hide keyboard");
		}
	}
	void kbEnsureLayout() {
		if (s_kbLayoutMode == s_kbMode && s_kbLayoutShift == s_kbShift && s_kbLayoutSymbols == s_kbSymbols)
			return;
		kbBuildLayout();
		s_kbLayoutMode = s_kbMode; s_kbLayoutShift = s_kbShift; s_kbLayoutSymbols = s_kbSymbols;
		s_kbDirty = true;
	}
	void kbEnsureTex() {
		if (s_kbTex) return;
		glGenTextures(1, &s_kbTex);
		glBindTexture(GL_TEXTURE_2D, s_kbTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kKbW, kKbH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	// Redraw the key faces into s_kbSurf and upload to s_kbTex. Called only when the layout, shift/
	// symbol state, or the hovered key changes -- not per frame.
	void kbRebuild() {
		if (!s_kbSurf)
			s_kbSurf = SDL_CreateRGBSurfaceWithFormat(0, kKbW, kKbH, 32, SDL_PIXELFORMAT_ABGR8888);
		if (!s_kbSurf) return;
		SDL_PixelFormat* fmt = s_kbSurf->format;
		uint16 style = 0;
		font_info* font = get_theme_font(DEFAULT_WIDGET, style);
		SDL_FillRect(s_kbSurf, nullptr, SDL_MapRGBA(fmt, 18, 20, 26, 235));
		for (int i = 0; i < s_kbKeyCount; ++i) {
			const KbKey& k = s_kbKeys[i];
			SDL_Rect r{ (int)(k.x*kKbW) + 3, (int)(k.y*kKbH) + 3, (int)(k.w*kKbW) - 6, (int)(k.h*kKbH) - 6 };
			const bool hot = (i == s_kbHover);
			const bool special = (k.act != KA_CHAR);
			Uint32 fill = hot     ? SDL_MapRGBA(fmt, 95, 130, 190, 255)
			            : special ? SDL_MapRGBA(fmt, 48, 52, 62, 255)
			                      : SDL_MapRGBA(fmt, 70, 74, 88, 255);
			SDL_FillRect(s_kbSurf, &r, fill);
			char cbuf[2];
			const char* text;
			if (k.act == KA_CHAR) { cbuf[0] = s_kbShift ? k.hi : k.lo; cbuf[1] = 0; text = cbuf; }
			else                    text = k.label ? k.label : "";
			if (font && text[0]) {
				uint16 tw = text_width(text, font, style);
				int tx = r.x + (r.w - tw) / 2;
				int ty = r.y + (r.h + font->get_ascent() - font->get_descent()) / 2;
				draw_text(s_kbSurf, text, tx, ty, SDL_MapRGB(fmt, 235, 236, 242), font, style);
			}
		}
		kbEnsureTex();
		glBindTexture(GL_TEXTURE_2D, s_kbTex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		// GL texel row 0 is the BOTTOM; our SDL surface has row 0 at the TOP, and the quad samples it
		// with the same UVs as the (OGL_Blitter-flipped) menu -- so upload the rows reversed, otherwise
		// the whole keyboard renders vertically flipped (numbers at the bottom, glyphs upside down).
		const int pitch = s_kbSurf->pitch;
		static unsigned char* flip = nullptr;
		if (!flip) flip = (unsigned char*)malloc((size_t)pitch * kKbH);
		if (flip) {
			const unsigned char* src = (const unsigned char*)s_kbSurf->pixels;
			for (int y = 0; y < kKbH; ++y)
				memcpy(flip + (size_t)y * pitch, src + (size_t)(kKbH - 1 - y) * pitch, pitch);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kKbW, kKbH, GL_RGBA, GL_UNSIGNED_BYTE, flip);
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kKbW, kKbH, GL_RGBA, GL_UNSIGNED_BYTE, s_kbSurf->pixels);
		}
		s_kbDirty = false;
	}
	// Place the keyboard below the menu panel, reclined about the menu's right axis so it faces up
	// toward the head. Anchored to the menu bottom edge, brought toward the user; the top edge stays
	// put as recline changes. Done once per dialog (persists across field focus changes).
	void kbPlace() {
		if (!s_panelPlaced) return;
		const float th = kKbReclineDeg * (3.14159265358979f / 180.0f);
		const float ct = std::cos(th), st = std::sin(th);
		s_kbHalfW = s_panelHalfW * kKbWidthScale;
		s_kbHalfH = s_kbHalfW * (float)kKbH / (float)kKbW;
		for (int i = 0; i < 3; ++i) {
			s_kbR[i] = s_panelR[i];
			s_kbU[i] = s_panelU[i]*ct - s_panelN[i]*st;   // reclined up
			s_kbN[i] = s_panelN[i]*ct + s_panelU[i]*st;   // reclined normal (tilts up toward head)
		}
		vnorm(s_kbU); vnorm(s_kbN);
		float top[3];
		for (int i = 0; i < 3; ++i)
			top[i] = s_panelC[i] - s_panelU[i]*(s_panelHalfH + kKbGapM) + s_panelN[i]*kKbForwardM;
		for (int i = 0; i < 3; ++i)
			s_kbC[i] = top[i] - s_kbU[i]*s_kbHalfH;
		s_kbPlaced = true;
	}
	// Ray-cast one controller onto the keyboard plane. Returns true if it lands within the panel;
	// outputs panel-local u,v in [-1,1] (for the cursor) and the key index under it (-1 if none).
	bool kbHitPanel(int h, float* uOut, float* vOut, int* keyOut) {
		if (!s_aimValid[h]) return false;
		float O[3] = { s_aimStage[h].position.x, s_aimStage[h].position.y, s_aimStage[h].position.z };
		float Dd[3]; poseFwd(s_aimStage[h], Dd);
		float denom = vdot(Dd, s_kbN);
		if (denom > -1e-4f) return false;
		float oc[3]; vsub(s_kbC, O, oc);
		float t = vdot(oc, s_kbN) / denom;
		if (t <= 0) return false;
		float hit[3] = { O[0]+Dd[0]*t, O[1]+Dd[1]*t, O[2]+Dd[2]*t };
		float loc[3]; vsub(hit, s_kbC, loc);
		float u = vdot(loc, s_kbR) / s_kbHalfW;
		float v = vdot(loc, s_kbU) / s_kbHalfH;
		if (u < -1 || u > 1 || v < -1 || v > 1) return false;
		*uOut = u; *vOut = v;
		float px = u*0.5f + 0.5f, py = 1.0f - (v*0.5f + 0.5f);
		int key = -1;
		for (int i = 0; i < s_kbKeyCount; ++i) {
			const KbKey& k = s_kbKeys[i];
			if (px >= k.x && px < k.x+k.w && py >= k.y && py < k.y+k.h) { key = i; break; }
		}
		*keyOut = key;
		return true;
	}
	void kbPushKey(SDL_Keycode sym) {
		SDL_Event d{}; d.type = SDL_KEYDOWN; d.key.state = SDL_PRESSED; d.key.repeat = 0;
		d.key.keysym.sym = sym; d.key.keysym.scancode = SDL_GetScancodeFromKey(sym); d.key.keysym.mod = KMOD_NONE;
		SDL_PushEvent(&d);
		SDL_Event u{}; u.type = SDL_KEYUP; u.key.state = SDL_RELEASED;
		u.key.keysym.sym = sym; u.key.keysym.scancode = SDL_GetScancodeFromKey(sym); u.key.keysym.mod = KMOD_NONE;
		SDL_PushEvent(&u);
	}
	void kbPress(int i) {
		if (i < 0 || i >= s_kbKeyCount) return;
		VR_PlayKeyClick();   // audible click so you can tell a key registered (esp. the in-game console)
		const KbKey& k = s_kbKeys[i];
		switch (k.act) {
			case KA_CHAR: {
				char c = s_kbShift ? k.hi : k.lo;
				if (!c) break;
				SDL_Event e{}; e.type = SDL_TEXTINPUT; e.text.text[0] = c; e.text.text[1] = 0;
				SDL_PushEvent(&e);
				if (s_kbShift) { s_kbShift = false; }   // shift is one-shot
			} break;
			case KA_SPACE: {
				SDL_Event e{}; e.type = SDL_TEXTINPUT; e.text.text[0] = ' '; e.text.text[1] = 0;
				SDL_PushEvent(&e);
			} break;
			case KA_BKSP:    kbPushKey(SDLK_BACKSPACE); break;
			case KA_ENTER:   kbPushKey(SDLK_RETURN);    break;
			case KA_LEFT:    kbPushKey(SDLK_LEFT);      break;
			case KA_RIGHT:   kbPushKey(SDLK_RIGHT);     break;
			case KA_SHIFT:   s_kbShift = !s_kbShift;    break;
			case KA_SYMBOLS: s_kbSymbols = !s_kbSymbols; s_kbShift = false; break;
			case KA_HIDE:    VR_DismissKeyboardField();  break;   // blur the field -> keyboard hides
		}
	}
	// Per-menu-frame keyboard update: place (once), ray-cast both hands, drive hover/click, and
	// suppress the menu pointer for any hand that is on the keyboard (so key clicks don't leak
	// through to the widgets behind it). Call after updatePointer(), before the eye loop.
	void kbUpdate() {
		if (!kbVisible()) { s_kbPlaced = false; s_kbHover = -1; s_kbTrigPrev = false; s_kbPtr[0].active = s_kbPtr[1].active = false; return; }
		if (!s_kbPlaced) kbPlace();
		if (!s_kbPlaced) return;
		kbEnsureLayout();

		const int dom = s_settings.dominantHand ? 0 : 1, off = 1 - dom;
		const int order[2] = { dom, off };
		int usedHand = -1, newHover = -1;
		for (int h = 0; h < 2; ++h) s_kbPtr[h].active = false;
		for (int oi = 0; oi < 2; ++oi) {
			int h = order[oi];
			float u, v; int key;
			if (kbHitPanel(h, &u, &v, &key)) {
				s_kbPtr[h].active = true; s_kbPtr[h].u = u; s_kbPtr[h].v = v; s_kbPtr[h].key = key;
				s_ptr[h].active = false;   // this hand is on the keyboard: hide it from the menu pointer
				if (usedHand < 0) { usedHand = h; newHover = key; }
			}
		}
		if (newHover != s_kbHover) { s_kbHover = newHover; s_kbDirty = true; }

		const bool trig = (usedHand >= 0) && (s_trigger[usedHand] > 0.5f);
		if (trig && !s_kbTrigPrev && s_kbHover >= 0) kbPress(s_kbHover);
		s_kbTrigPrev = trig;

		if (s_kbDirty) kbRebuild();
	}
	// Draw the keyboard quad + its per-hand cursors into the currently-bound eye. vp = proj*eyeFromStage.
	void kbDrawEye(const float* vp) {
		if (!kbVisible() || !s_kbPlaced || !s_kbTex) return;
		const float hw = s_kbHalfW, hh = s_kbHalfH;
		float model[16] = {
			s_kbR[0]*hw, s_kbR[1]*hw, s_kbR[2]*hw, 0,
			s_kbU[0]*hh, s_kbU[1]*hh, s_kbU[2]*hh, 0,
			s_kbN[0],    s_kbN[1],    s_kbN[2],    0,
			s_kbC[0],    s_kbC[1],    s_kbC[2],    1 };
		float mvp[16]; mat_mul(mvp, vp, model);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(s_quadProg);
		glUniform1f(s_quadPhiLoc, 0.0f);   // flat: only the menu/terminal panel curves
		glUniformMatrix4fv(s_quadMVPLoc, 1, GL_FALSE, mvp);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, s_kbTex);
		glUniform1i(s_quadTexLoc, 0);
		glBindVertexArray(s_quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		// cursors: yellow dot per hand on the keyboard
		const float cs = 0.012f * 1.3f;
		glUseProgram(s_curProg);
		glUniform4f(s_curColorLoc, 0.95f, 0.9f, 0.35f, 0.75f);
		for (int h = 0; h < 2; ++h) {
			if (!s_kbPtr[h].active) continue;
			const float u = s_kbPtr[h].u, v = s_kbPtr[h].v;
			const float cx = s_kbC[0] + u*hw*s_kbR[0] + v*hh*s_kbU[0] + s_kbN[0]*0.01f;
			const float cy = s_kbC[1] + u*hw*s_kbR[1] + v*hh*s_kbU[1] + s_kbN[1]*0.01f;
			const float cz = s_kbC[2] + u*hw*s_kbR[2] + v*hh*s_kbU[2] + s_kbN[2]*0.01f;
			float cm[16] = {
				s_kbR[0]*cs, s_kbR[1]*cs, s_kbR[2]*cs, 0,
				s_kbU[0]*cs, s_kbU[1]*cs, s_kbU[2]*cs, 0,
				s_kbN[0],    s_kbN[1],    s_kbN[2],    0,
				cx,          cy,          cz,          1 };
			float cmvp[16]; mat_mul(cmvp, vp, cm);
			glUniformMatrix4fv(s_curMVPLoc, 1, GL_FALSE, cmvp);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		glDisable(GL_BLEND);
		glBindVertexArray(0);
	}

	// ---- in-game console placement + text strip ----
	// The menu keyboard hangs under the (eye-level, far) menu panel -- too high/far for an in-game
	// console. Place a small UNDRAWN virtual panel low + close in front of the head so the keyboard
	// sits in the lower field of view within easy controller reach. Keyboard geometry then follows via
	// kbPlace (it hangs just under this virtual panel).
	void placeConsolePanel() {
		float hp[3] = { s_stageFromHead.position.x, s_stageFromHead.position.y, s_stageFromHead.position.z };
		float hf[3]; poseFwd(s_stageFromHead, hf); hf[1]=0; vnorm(hf);
		if (hf[0]==0 && hf[2]==0) { hf[2]=-1; }
		const float kConD    = 0.85f;   // metres in front of the head (laser-operated, so reach is moot)
		const float kConDrop = 0.42f;   // metres below eye level (keyboard sits in the lower FOV)
		s_panelC[0]=hp[0]+hf[0]*kConD; s_panelC[1]=hp[1]-kConDrop; s_panelC[2]=hp[2]+hf[2]*kConD;
		s_panelN[0]=-hf[0]; s_panelN[1]=0; s_panelN[2]=-hf[2];      // vertical, faces the head
		const float up[3]={0,1,0};
		vcross(up, s_panelN, s_panelR); vnorm(s_panelR);
		vcross(s_panelN, s_panelR, s_panelU); vnorm(s_panelU);
		// Match the menu keyboard's APPARENT size so the console keyboard doesn't feel smaller than the
		// one in Preferences: the menu panel is (0.5*screenHeightM) tall * aspect wide at screenDistanceM;
		// reproduce that angular half-width at our distance kConD. Scales with the screenHeightM pref.
		const float menuHalfW = 0.5f * s_settings.screenHeightM * (float)kScreenW / (float)kScreenH;
		s_panelHalfW = menuHalfW * (kConD / s_settings.screenDistanceM);
		s_panelHalfH = 0.05f;   // keyboard top hangs just below this (undrawn) panel centre
		s_panelPlaced = true;
	}

	// A thin world-locked strip that shows the console line (Console::displayBuffer). Drawn VERTICAL
	// (panel basis, NOT the keyboard's reclined basis) directly above the keyboard's top edge, so it
	// stays put with the keyboard instead of following the head like the HUD.
	// Smaller texture width => the fixed-size theme font is MAGNIFIED more when the strip is stretched to
	// its world width (font world size = font_px * 2*s_conHalfW / kConW), i.e. the console line reads a lot
	// bigger than the keyboard key labels. kConW must still fit a typical line (proportional font ~9px/char
	// => ~55 chars at 512); longer lines clip on the right.
	constexpr int kConW = 512, kConH = 72;
	GLuint       s_conTex  = 0;
	SDL_Surface* s_conSurf = nullptr;
	float        s_conHalfW = 0.40f, s_conHalfH = 0.05f;

	void conRebuild() {
		if (!s_conSurf) s_conSurf = SDL_CreateRGBSurfaceWithFormat(0, kConW, kConH, 32, SDL_PIXELFORMAT_ABGR8888);
		if (!s_conSurf) return;
		SDL_PixelFormat* fmt = s_conSurf->format;
		uint16 style = 0;
		font_info* font = get_theme_font(DEFAULT_WIDGET, style);
		SDL_FillRect(s_conSurf, nullptr, SDL_MapRGBA(fmt, 18, 20, 26, 220));
		const char* text = VR_GetConsoleLine();
		if (font && text[0]) {
			int ty = (kConH + font->get_ascent() - font->get_descent()) / 2;
			draw_text(s_conSurf, text, 12, ty, SDL_MapRGB(fmt, 235, 236, 242), font, style);
		}
		if (!s_conTex) {
			glGenTextures(1, &s_conTex);
			glBindTexture(GL_TEXTURE_2D, s_conTex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kConW, kConH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, s_conTex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		const int pitch = s_conSurf->pitch;                 // flip rows (GL bottom-origin), like kbRebuild
		static unsigned char* flip = nullptr; static int flipCap = 0;
		if (flipCap < pitch*kConH) { free(flip); flip = (unsigned char*)malloc((size_t)pitch*kConH); flipCap = pitch*kConH; }
		if (flip) {
			const unsigned char* src = (const unsigned char*)s_conSurf->pixels;
			for (int y=0; y<kConH; ++y) memcpy(flip+(size_t)y*pitch, src+(size_t)(kConH-1-y)*pitch, pitch);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kConW, kConH, GL_RGBA, GL_UNSIGNED_BYTE, flip);
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kConW, kConH, GL_RGBA, GL_UNSIGNED_BYTE, s_conSurf->pixels);
		}
		s_conHalfW = s_kbHalfW * 1.15f;   // a touch wider than the keyboard so the bigger text has room
		s_conHalfH = s_conHalfW * (float)kConH / (float)kConW;
	}

	void conDrawEye(const float* vp) {
		if (!s_kbPlaced || !s_conTex) return;
		const float hw = s_conHalfW, hh = s_conHalfH;
		float topKb[3]; for (int i=0;i<3;++i) topKb[i] = s_kbC[i] + s_kbU[i]*s_kbHalfH;   // reclined kb top edge
		float conC[3];  for (int i=0;i<3;++i) conC[i]  = topKb[i] + s_panelU[i]*(0.05f + hh); // vertical, above it
		float model[16] = {
			s_panelR[0]*hw, s_panelR[1]*hw, s_panelR[2]*hw, 0,
			s_panelU[0]*hh, s_panelU[1]*hh, s_panelU[2]*hh, 0,
			s_panelN[0],    s_panelN[1],    s_panelN[2],    0,
			conC[0],        conC[1],        conC[2],        1 };
		float mvp[16]; mat_mul(mvp, vp, model);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(s_quadProg);
		glUniform1f(s_quadPhiLoc, 0.0f);   // flat: only the menu/terminal panel curves
		glUniformMatrix4fv(s_quadMVPLoc, 1, GL_FALSE, mvp);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, s_conTex);
		glUniform1i(s_quadTexLoc, 0);
		glBindVertexArray(s_quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisable(GL_BLEND);
		glBindVertexArray(0);
	}
}

// Report which keyboard layout the focused text field wants (called by the text-entry widget on
// focus). Resets shift/symbols on a mode change so the new field starts clean.
extern "C" void VR_SetKeyboardInputHint(int hint) {
	int m = (hint == VR_KB_NUMERIC) ? KB_NUMERIC : (hint == VR_KB_IP) ? KB_IP : KB_ALPHA;
	if (m != s_kbMode) { s_kbMode = m; s_kbShift = false; s_kbSymbols = false; }
	s_kbFocusActive = true;   // a text field is focused: allow the keyboard to show
}

// Widget blur hook (field lost focus / dialog closed). Clears the explicit focus guard so the keyboard
// hides even if SDL's global text-input flag is momentarily left stale by the dialog layer.
extern "C" void VR_KeyboardDismiss(void) { s_kbFocusActive = false; }

// ---- In-game console keyboard --------------------------------------------------------------------
// The menu keyboard (above) is driven + drawn by VR_PresentScreenLayer, which only runs for 2D menu
// frames. The in-game console needs the SAME keyboard during the 3D frame, so these entry points drive
// and draw it from the render seam (screen.cpp update, render.cpp eye loop). It reuses the whole
// keyboard implementation, anchored to a world-locked panel (placePanel) placed in front of the head.
namespace { bool s_inGameKb = false; bool s_conPanelPlaced = false; }

extern "C" void VR_SetInGameKeyboard(bool on)
{
	if (on) {
		s_inGameKb = true;
		s_kbFocusActive = true;    // paired with the console's SDL text input -> kbVisible() true
		s_kbMode = KB_ALPHA; s_kbShift = false; s_kbSymbols = false;
		s_kbPlaced = false;        // re-place under a fresh anchor
		s_conPanelPlaced = false;  // force placeConsolePanel() once, in front of the current head
	} else {
		s_inGameKb = false;
		s_kbFocusActive = false;
		s_kbPlaced = false;
	}
}

extern "C" bool VR_InGameKeyboardActive(void)
{
	// Self-clear if the console dropped SDL text input (e.g. the keyboard's OK key submitted the line,
	// or the line was entered), so callers stop suppressing game input and drawing the keyboard. If a
	// trigger is still held (it just clicked OK), latch the fire lockout so releasing it doesn't shoot.
	if (s_inGameKb && SDL_IsTextInputActive() != SDL_TRUE) {
		s_inGameKb = false; s_kbFocusActive = false;
		if (s_trigger[0] > 0.5f || s_trigger[1] > 0.5f) s_kbFireLockout = true;
	}
	return s_inGameKb;
}

extern "C" void VR_UpdateInGameKeyboard(void)
{
	if (!VR_InGameKeyboardActive() || !s_headPoseValid) return;
	// Place the panel ONCE (own flag): s_panelPlaced gets reset every world frame by
	// VR_MarkWorldFramePresented (for the menu re-anchor), which would otherwise re-run placeConsolePanel
	// each frame relative to the CURRENT head -> the console strip would follow the head. s_kbPlaced isn't
	// reset, which is why the keyboard itself stayed put but the strip (panel basis) didn't.
	if (!s_conPanelPlaced) { placeConsolePanel(); s_conPanelPlaced = true; }
	kbUpdate();                                // gates on kbVisible() (s_kbFocusActive && SDL text input)
	conRebuild();                              // refresh the console text strip
}

extern "C" void VR_DrawInGameKeyboardEye(int eye)
{
	if (!VR_InGameKeyboardActive() || eye < 0 || eye >= kEyes) return;
	float proj[16];
	VR_GetEyeProjection(eye, proj, 0.05f, 50.0f);
	float stageFromEye[16]; mat_from_pose(stageFromEye, s_stageFromEye[eye]);
	float eyeFromStage[16]; mat_rigid_inverse(eyeFromStage, stageFromEye);
	float vp[16]; mat_mul(vp, proj, eyeFromStage);
	kbDrawEye(vp);
	conDrawEye(vp);   // console text strip, vertical, above the keyboard
}

extern "C" void VR_PresentScreenLayer(void)
{
	if (!s_active) return;
	ensureScreenLayer();
	const bool render = VR_BeginFrame();
	if (render) {
		// System recenter (Meta/Quest button hold) while a 2D screen is up: re-anchor the world-locked
		// panel -- and the keyboard, which is placed relative to it -- to the player's new facing.
		if (s_panelRecenterPending) { s_panelPlaced = false; s_kbPlaced = false; s_panelRecenterPending = false; }
		if (!s_panelPlaced && s_headPoseValid) placePanel();
		updatePointer();   // ray-cast both controllers onto the world-locked panel
		kbUpdate();        // on-screen keyboard: place/hover/click + suppress menu pointer when on it

		// DIAG (menu-out-of-alignment): ~1/sec persistent log of the LIVE head-vs-panel relationship while
		// the menu is shown. Decisive: when the menu looks off, "off=Ndeg" tells whether the head is actually
		// pointed AT the panel (~0 -> a render/pose bug: panel drawn wrong despite being in front) or well
		// off it (large -> a placement/position bug), and the head/panel coords show if the stage pose is
		// sane or corrupted (e.g. by crossing the guardian). Remove once root-caused.
		// SHIP-DISABLED (store test build): this ran ~1/sec and wrote to the on-device log file the whole
		// time a menu was open. Re-enable (uncomment the block) to resume debugging panel alignment.
		/*
		if (s_headPoseValid) {
			static int s_pdbg = 0;
			if ((s_pdbg++ % 45) == 0) {
				const float hpx=s_stageFromHead.position.x, hpy=s_stageFromHead.position.y, hpz=s_stageFromHead.position.z;
				float hf[3]; poseFwd(s_stageFromHead, hf); hf[1]=0;
				const float hlen=std::sqrt(hf[0]*hf[0]+hf[2]*hf[2]);
				const float tx=s_panelC[0]-hpx, tz=s_panelC[2]-hpz;
				const float dist=std::sqrt(tx*tx+tz*tz);
				float cang = (hlen>1e-4f && dist>1e-4f) ? (hf[0]*tx+hf[2]*tz)/(hlen*dist) : 2.0f;
				float angDeg = (cang>1.5f) ? -1.0f : std::acos(cang<-1.f?-1.f:(cang>1.f?1.f:cang))*57.29578f;
				logWarning("VRpanel disp head=(%.2f,%.2f,%.2f) fwdH=(%.2f,%.2f) C=(%.2f,%.2f,%.2f) dist=%.2f dY=%.2f off=%.0fdeg placed=%d",
					hpx,hpy,hpz, hf[0],hf[2], s_panelC[0],s_panelC[1],s_panelC[2], dist, s_panelC[1]-hpy, angDeg, (int)s_panelPlaced);
			}
		}
		*/

		// World-locked panel: local quad (-1..1 XY) -> stage via panelRight/Up/Center.
		const float hw = s_panelHalfW, hh = s_panelHalfH;
		float model[16] = {
			s_panelR[0]*hw, s_panelR[1]*hw, s_panelR[2]*hw, 0,
			s_panelU[0]*hh, s_panelU[1]*hh, s_panelU[2]*hh, 0,
			s_panelN[0],    s_panelN[1],    s_panelN[2],    0,
			s_panelC[0],    s_panelC[1],    s_panelC[2],    1 };
		// Cursor quads: one per active hand, a hair in front of the panel.
		const float cs = 0.012f * 1.3f;
		float curModel[2][16] = {};
		for (int h = 0; h < 2; ++h) {
			if (!s_ptr[h].active) continue;
			const float u = s_ptr[h].u, v = s_ptr[h].v;
			float cp[3]; panelPointAt(u, v, cp);          // follows the curve, so it sits on the image
			const float cx = cp[0] + s_panelN[0]*0.01f;
			const float cy = cp[1] + s_panelN[1]*0.01f;
			const float cz = cp[2] + s_panelN[2]*0.01f;
			curModel[h][0] = s_panelR[0]*cs; curModel[h][1] = s_panelR[1]*cs; curModel[h][2] = s_panelR[2]*cs; curModel[h][3] = 0;
			curModel[h][4] = s_panelU[0]*cs; curModel[h][5] = s_panelU[1]*cs; curModel[h][6] = s_panelU[2]*cs; curModel[h][7] = 0;
			curModel[h][8] = s_panelN[0];    curModel[h][9] = s_panelN[1];    curModel[h][10]= s_panelN[2];    curModel[h][11]= 0;
			curModel[h][12]= cx;              curModel[h][13]= cy;              curModel[h][14]= cz;             curModel[h][15]= 1;
		}
		const int domIdx = s_settings.dominantHand ? 0 : 1;

		for (int e = 0; e < kEyes; ++e) {
			VR_BeginEye(e);
			float proj[16];
			VR_GetEyeProjection(e, proj, 0.05f, 50.0f);
			float stageFromEye[16]; mat_from_pose(stageFromEye, s_stageFromEye[e]);
			float eyeFromStage[16]; mat_rigid_inverse(eyeFromStage, stageFromEye);
			float vp[16]; mat_mul(vp, proj, eyeFromStage);   // proj * eyeFromStage

			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			// panel
			float mvp[16]; mat_mul(mvp, vp, model);
			glUseProgram(s_quadProg);
			glUniformMatrix4fv(s_quadMVPLoc, 1, GL_FALSE, mvp);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, s_screenTex);
			glUniform1i(s_quadTexLoc, 0);
			// Curved: the tessellated strip has the vertices the bend needs; flat: the plain quad.
			const float phi = panelCurvePhi();
			glUniform1f(s_quadPhiLoc, phi);
			glUniform1f(s_quadRLoc,   panelCurveR());
			if (phi > 0.0001f) {
				glBindVertexArray(s_panelStripVAO);
				glDrawArrays(GL_TRIANGLES, 0, kPanelStripVerts);
			} else {
				glBindVertexArray(s_quadVAO);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
			// cursors: grey translucent circles for both hands
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glUseProgram(s_curProg);
			// Bind explicitly: this used to inherit whatever the panel draw left bound, which was
			// always the 6-vertex quad. The curved panel binds a 288-vertex strip instead, so the
			// cursor was drawing that strip's first 6 vertices -- a sliver at the panel's left edge.
			glBindVertexArray(s_quadVAO);
			glUniform4f(s_curColorLoc, 0.5f, 0.5f, 0.5f, 0.5f);
			for (int h = 0; h < 2; ++h) {
				if (s_panelIsTerminal) break;   // nothing to click on a terminal; dots just cover text
				if (!s_ptr[h].active) continue;
				float cmvp[16]; mat_mul(cmvp, vp, curModel[h]);
				glUniformMatrix4fv(s_curMVPLoc, 1, GL_FALSE, cmvp);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
			glDisable(GL_BLEND);
			glBindVertexArray(0);
			// on-screen keyboard: its own reclined quad below the menu, drawn over the world
			kbDrawEye(vp);
			VR_FinishEye(e);
		}
	}
	VR_SubmitFrame();
}

// Submit a clean BOTH-EYES frame during a blocking operation (single-player level load, which pumps
// no frames of its own -- no progress dialog in solo play). Without this the compositor stops getting
// xrWaitFrame/xrEndFrame for the whole load and freezes on the last submitted frame; worse, an extended
// stall makes Meta's compositor degrade the frozen image to a single eye (the reported "only the right
// eye is visible"). Each VR_BeginEye clears its eye FBO to black and VR_FinishEye records that eye's
// layer view, so this always submits a symmetric stereo pair -> the frozen loading image stays clean
// in BOTH eyes and head-tracks. Call it at load checkpoints (see goto_level) to keep the compositor fed.
extern "C" void VR_RenderLoadingFrame(void)
{
	if (!s_active) return;
	const bool render = VR_BeginFrame();
	if (render) {
		for (int e = 0; e < kEyes; ++e) {
			VR_BeginEye(e);    // acquires the swapchain image, binds the eye FBO, clears it to black
			VR_FinishEye(e);   // sets s_layerViews[e] from this frame's head pose + releases the image
		}
	}
	VR_SubmitFrame();   // no-ops if no frame was begun (session not running yet)
}

// Per-frame fallback for non-3D frames (menus/loading): render the test room to both eyes.
extern "C" bool VR_RenderTestFrame(void)
{
	if (!s_active) return false;
	const bool render = VR_BeginFrame();
	if (render) {
		for (int e = 0; e < kEyes; ++e) {
			VR_BeginEye(e);
			float proj[16], view[16];
			VR_GetEyeProjection(e, proj, 0.05f, 100.0f);
			VR_GetEyeViewMetres(e, view);
			drawTestScene(proj, view);
			VR_FinishEye(e);
		}
	}
	VR_SubmitFrame();   // no-ops if no frame was begun (session not running)
	return s_active;
}

extern "C" void VR_GetEyeIPDOffsetWU(int eye, float* wx, float* wy)
{
    if (wx) *wx = 0; if (wy) *wy = 0;
    if (!s_headPoseValid || eye < 0 || eye >= kEyes) return;
    const float dhx = s_stageFromEye[eye].position.x - s_stageFromHead.position.x;
    const float dhz = s_stageFromEye[eye].position.z - s_stageFromHead.position.z;
    const float W   = s_settings.worldScaleWUM;
    const float yr  = s_yawOffset * (2.0f * 3.14159265358979f / 512.0f);
    const float c   = std::cos(yr), s = std::sin(yr);
    if (wx) *wx = W * (-c * dhx + s * dhz);
    if (wy) *wy = W * (-s * dhx - c * dhz);
}

#else // !__ANDROID__

extern "C" bool VR_InitOpenXR(void)     { return false; }
extern "C" bool VR_IsActive(void)       { return false; }
extern "C" vr_settings_t* VR_Settings(void) {
	static vr_settings_t s = { 1, 2.5f, 2.0f, 512.0f, 0.0f, 1, 30.0f, 90.0f, 1.0f, 0, 0, 0, -20.0f, 0.8f, 0.55f, 30.0f, 0, 0, 0 };
	return &s;
}
extern "C" float VR_GetYawOffset(void)   { return 0.0f; }
extern "C" void  VR_UpdateTurn(float, float) {}
extern "C" void  VR_SetYawOffset(float)  {}
extern "C" void  VR_RequestYawRecenter(int) {}
extern "C" bool  VR_TakeYawRecenter(int*) { return false; }
extern "C" bool  VR_TakeSystemRecenter(void) { return false; }
extern "C" void  VR_DimCurrentEye(void)  {}
extern "C" bool VR_RenderTestFrame(void){ return false; }
extern "C" void VR_RenderLoadingFrame(void) {}
extern "C" void VR_RequestLevelWarmup(void) {}
extern "C" bool VR_ConsumeLevelWarmup(void) { return false; }
extern "C" void VR_BeginWarmupEye(int)  {}
extern "C" void VR_EndWarmup(void)      {}
extern "C" bool VR_InitEGL(void)        { return false; }
extern "C" bool VR_GetEyeResolution(int* w, int* h) { (void)w; (void)h; return false; }
extern "C" bool VR_GetHmdYawPitch(float* y, float* p) { if (y) *y = 0; if (p) *p = 0; return false; }
extern "C" void VR_GetMove(float* x, float* y) { if (x) *x = 0; if (y) *y = 0; }
extern "C" void VR_GetTurn(float* x)           { if (x) *x = 0; }
extern "C" void VR_GetTurnY(float* y)          { if (y) *y = 0; }
extern "C" void VR_GetAnalogMove(float* s, float* f) { if (s) *s = 0; if (f) *f = 0; }
extern "C" void VR_LatchHeadMove(void) {}
extern "C" void VR_GetHeadMove(float* x, float* y) { if (x) *x = 0; if (y) *y = 0; }
extern "C" void VR_RecenterHead(void) {}
extern "C" void VR_GetHeadOffset(float* x, float* y) { if (x) *x = 0; if (y) *y = 0; }
extern "C" void VR_SetRenderCamera(float, float, float) {}
extern "C" bool VR_GetRenderCamera(float*, float*, float*) { return false; }
extern "C" float VR_GetEyeZOffset(void) { return 0.0f; }
extern "C" float VR_EyeHeightM(void) { return 0.0f; }
extern "C" void VR_SetGameEyeHeightWU(float) {}
extern "C" void VR_SetPlayerRadiusWU(float) {}
extern "C" bool VR_GetFire(void)               { return false; }
extern "C" bool VR_GetSecondaryFire(void)      { return false; }
extern "C" bool VR_GetPrimaryPunch(void)       { return false; }
extern "C" bool VR_GetSecondaryPunch(void)     { return false; }
extern "C" bool VR_PrimaryPunchRecent(void)    { return false; }
extern "C" bool VR_SecondaryPunchRecent(void)  { return false; }
extern "C" bool VR_GetAction(void)             { return false; }
extern "C" bool VR_GetAdvance(void)            { return false; }
extern "C" bool VR_GetBack(void)               { return false; }
extern "C" bool VR_GetButtonX(void)            { return false; }
extern "C" bool VR_GetButtonY(void)            { return false; }
extern "C" bool VR_GetMoveStickClick(void)     { return false; }
extern "C" bool VR_GetTurnStickClick(void)       { return false; }
extern "C" void VR_SetMapActive(bool, bool)      {}
extern "C" unsigned VR_MapLayerFramebuffer(void) { return 0; }
extern "C" int  VR_MapLayerWidth(void)         { return 0; }
extern "C" int  VR_MapLayerHeight(void)        { return 0; }
extern "C" void VR_PresentMapEye(int)          {}
extern "C" bool VR_BeginFrame(void)     { return false; }
extern "C" void VR_BeginEye(int)        {}
extern "C" void VR_FinishEye(int)       {}
extern "C" void VR_GetEyeProjection(int, float*, float, float) {}
extern "C" void VR_GetEyeViewMetres(int, float*) {}
extern "C" void VR_SubmitFrame(void)    {}
extern "C" int  VR_EyeWidth(void)       { return 0; }
extern "C" int  VR_EyeHeight(void)      { return 0; }
extern "C" unsigned VR_CurrentEyeFramebuffer(void) { return 0; }
extern "C" int  VR_CurrentEye(void)     { return 0; }
extern "C" void VR_MarkWorldFramePresented(void) {}
extern "C" void VR_InvalidatePanelPlacement(void) {}
extern "C" void VR_SetTerminalActive(int) {}
extern "C" int   VR_GetRefreshRates(float*, int) { return 0; }
extern "C" float VR_GetRefreshRate(void)         { return 0.0f; }
extern "C" bool  VR_SetRefreshRate(float)        { return false; }
extern "C" bool VR_TakeWorldFramePresented(void) { return false; }
extern "C" unsigned VR_ScreenLayerFramebuffer(void) { return 0; }
extern "C" void VR_PresentScreenLayer(void) {}
extern "C" bool VR_GetPointerScreen(int* x, int* y) { (void)x; (void)y; return false; }
extern "C" bool VR_GetPointerClick(void) { return false; }
extern "C" bool VR_GetPointerGrip(void) { return false; }
extern "C" bool VR_GetAimPoseStage(int, float*, float*) { return false; }
extern "C" bool VR_GetAimOrientStage(int, float*, float*) { return false; }
extern "C" void VR_SetIsDualWield(bool) {}
extern "C" void VR_SetTwoHandedDisabled(bool) {}
extern "C" void VR_SetTwoHandedOffset(float, float) {}
extern "C" void VR_SetSpriteViewOrigin(const world_point3d*) {}
extern "C" bool VR_GetSpriteViewOrigin(world_point3d*) { return false; }
extern "C" void VR_SetOffHandHasWeapon(bool) {}
extern "C" void VR_SetGripAltFireEnabled(bool) {}
extern "C" bool VR_IsTwoHandedActive() { return false; }
extern "C" bool VR_GetTwoHandedFwdStage(float*) { return false; }
extern "C" bool VR_GetHandTracking(int, bool*, float*) { return false; }
extern "C" bool VR_GetHeadPosStage(float*) { return false; }
extern "C" bool VR_GetWeaponOriginOffset(float*, float*, float*) { return false; }
extern "C" bool VR_GetSecondaryWeaponOriginOffset(float*, float*, float*) { return false; }
extern "C" bool VR_GetWeaponAim(float*) { return false; }
extern "C" bool VR_GetSecondaryWeaponAim(float*) { return false; }
extern "C" bool VR_TakeMenuButton(void) { return false; }
extern "C" bool VR_HasFocus(void) { return false; }
extern "C" int  VR_ScreenLayerWidth(void)  { return 0; }
extern "C" int  VR_ScreenLayerHeight(void) { return 0; }
extern "C" void VR_GetEyeIPDOffsetWU(int, float* wx, float* wy) { if (wx) *wx = 0; if (wy) *wy = 0; }

#endif
