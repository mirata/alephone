// Stub implementations of the VR API for non-Android (Windows/desktop) builds.
// On Android the real implementation lives in vr_openxr.cpp.
#include "vr_openxr.h"

extern "C" bool VR_InitOpenXR(void)     { return false; }
extern "C" bool VR_IsActive(void)       { return false; }
extern "C" vr_settings_t* VR_Settings(void) {
	static vr_settings_t s = { 1, 2.5f, 2.0f, 512.0f, 0.0f, 1, 30.0f, 1.0f, 0, 0, 0, -20.0f, 0.8f, 0.55f, 30.0f, 0 };
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
extern "C" bool VR_InitEGL(void)        { return false; }
extern "C" bool VR_GetEyeResolution(int* w, int* h) { (void)w; (void)h; return false; }
extern "C" bool VR_GetHmdYawPitch(float* y, float* p) { if (y) *y = 0; if (p) *p = 0; return false; }
extern "C" void VR_GetMove(float* x, float* y) { if (x) *x = 0; if (y) *y = 0; }
extern "C" void VR_GetTurn(float* x)           { if (x) *x = 0; }
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
extern "C" bool VR_GetFire(void)               { return false; }
extern "C" bool VR_GetSecondaryFire(void)      { return false; }
extern "C" bool VR_GetAction(void)             { return false; }
extern "C" bool VR_GetAdvance(void)            { return false; }
extern "C" bool VR_GetBack(void)               { return false; }
extern "C" bool VR_GetButtonX(void)            { return false; }
extern "C" bool VR_GetButtonY(void)            { return false; }
extern "C" bool VR_GetMoveStickClick(void)     { return false; }
extern "C" bool VR_GetTurnStickClick(void)     { return false; }
extern "C" bool VR_ActionHeld(int)             { return false; }
extern "C" void VR_SetMapActive(bool, bool)    {}
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
extern "C" bool VR_TakeWorldFramePresented(void) { return false; }
extern "C" unsigned VR_ScreenLayerFramebuffer(void) { return 0; }
extern "C" void VR_PresentScreenLayer(void) {}
extern "C" bool VR_GetPointerScreen(int* x, int* y) { (void)x; (void)y; return false; }
extern "C" bool VR_GetPointerClick(void) { return false; }
extern "C" bool VR_GetPointerGrip(void) { return false; }
extern "C" void VR_SetKeyboardInputHint(int) {}
extern "C" void VR_KeyboardDismiss(void) {}
extern "C" bool VR_GetAimPoseStage(int, float*, float*) { return false; }
extern "C" bool VR_GetAimOrientStage(int, float*, float*) { return false; }
extern "C" void VR_SetIsDualWield(bool) {}
extern "C" void VR_SetOffHandHasWeapon(bool) {}
extern "C" void VR_SetGripAltFireEnabled(bool) {}
extern "C" bool VR_IsTwoHandedActive() { return false; }
extern "C" bool VR_GetTwoHandedFwdStage(float*) { return false; }
extern "C" bool VR_GetHeadPosStage(float*) { return false; }
extern "C" bool VR_GetWeaponAim(float*) { return false; }
extern "C" bool VR_GetSecondaryWeaponAim(float*) { return false; }
extern "C" bool VR_TakeMenuButton(void) { return false; }
extern "C" bool VR_HasFocus(void) { return false; }
extern "C" int  VR_ScreenLayerWidth(void)  { return 0; }
extern "C" int  VR_ScreenLayerHeight(void) { return 0; }
extern "C" unsigned VR_HudLayerFramebuffer(void) { return 0; }
extern "C" int  VR_HudLayerWidth(void)  { return 0; }
extern "C" int  VR_HudLayerHeight(void) { return 0; }
extern "C" void VR_PresentHudEye(int)   {}
extern "C" void VR_RenderLoadingFrame(void) {}
