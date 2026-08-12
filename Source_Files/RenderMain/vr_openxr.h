/*
 *  vr_openxr.h  --  OpenXR (Meta Quest) VR backend for Aleph One. See vr_openxr.cpp.
 *
 *  Phase 2 of the Quest port. SDL-hosted: OpenXR binds to the EGL context SDL already created, and
 *  the engine keeps SDL for the main loop, input, audio and timing. The stereo frame loop replaces
 *  SDL_GL_SwapWindow. Public API is plain C; everything is a no-op off Android.
 */
#ifndef VR_OPENXR_H
#define VR_OPENXR_H

#ifdef __cplusplus
extern "C" {
#endif

struct world_point3d;   // forward decl for the sprite-view-origin API below (defined in world.h)

// Create the OpenXR instance + system. Call once SDL video is initialised (the GL context need not
// exist yet -- the session is created lazily on the first frame). Returns true if VR is available.
bool VR_InitOpenXR(void);

// True once VR_InitOpenXR has succeeded (i.e. we are running as a VR app).
bool VR_IsActive(void);

// ---- Controller button remapping (VR) ----
// The bindable physical inputs. Triggers/grips/stick-directions stay committed to fire/aim/locomotion;
// the left menu (hamburger) button is reserved for Quit and is NOT remappable. The two stick-clicks are
// role-based (they follow handedness / switch-sticks) rather than physical L3/R3, so a lefty's move-stick
// click stays on their move stick. Order is stable -- persisted by index in buttonAction[].
enum {
	VR_BTN_A = 0,          // right controller A
	VR_BTN_B,              // right controller B
	VR_BTN_X,              // left controller X
	VR_BTN_Y,              // left controller Y
	VR_BTN_MOVE_CLICK,     // click of the thumbstick you MOVE with (follows handedness/switch-sticks)
	VR_BTN_TURN_CLICK,     // click of the thumbstick you TURN with (the other stick)
	VR_BTN_COUNT
};
// The in-game action a button can be bound to. Values are the menu order and are PERSISTED by number
// (keep stable; append new actions at the end). VR_ACT_NONE = unbound.
enum {
	VR_ACT_NONE = 0,
	VR_ACT_PRIMARY_FIRE,
	VR_ACT_SECONDARY_FIRE,
	VR_ACT_ACTION_USE,
	VR_ACT_NEXT_WEAPON,
	VR_ACT_PREV_WEAPON,
	VR_ACT_RUN,
	VR_ACT_TOGGLE_MAP,
	VR_ACT_RECENTER,
	VR_ACT_INVENTORY_PREV,   // scroll the inventory panel back one item
	VR_ACT_INVENTORY_NEXT,   // scroll the inventory panel forward one item
	VR_ACT_SCREENSHOT,       // capture a clean single-eye screenshot to the Screenshots folder
	VR_ACT_CONSOLE,          // toggle the in-game console (+ the on-screen keyboard) for chat / lua commands
	VR_ACT_COUNT
};

// ---- Tunable VR comfort/scale settings (the VR preferences menu binds to this) ----
// One global, sane defaults at startup. Centralises the knobs scattered across the render seam so a
// prefs dialog can drive them without touching the renderer. Modeled on QuestZDoom's VR cvars.
typedef struct {
	int   disableBob;       // 1 = suppress camera view-bob (nausea); weapon bob unaffected
	float screenDistanceM;  // distance of the 2D UI panel (menus/terminals) in metres
	float screenHeightM;    // height of the 2D UI panel in metres (width follows its aspect)
	float worldScaleWUM;    // Marathon world-units per metre (bigger = world feels smaller)
	float heightAdjustM;    // manual height nudge (metres of stature; + = taller in-game). Added on top of
	                        // the auto-measured standing eye height; 0 = no adjustment.
	int   snapTurn;         // 1 = snap turning, 0 = smooth (locomotion comfort)
	float turnDegrees;      // snap: degrees per snap flick; smooth: degrees/sec at full deflection
	float brightness;       // world brightness multiply (1=unchanged; <1 dims the over-bright world)
	int   roomScale;        // 1 = body follows the head's physical movement (room-scale); 0 = head is
	                        //     a free 6DOF camera over a static body (no positional locomotion)
	int   dominantHand;     // 0 = right-handed, 1 = left-handed. The dominant hand FIRES the primary weapon
	                        //     (trigger); the off-hand secondary-fires. By default the off-hand MOVES
	                        //     (stick) and the dominant hand TURNS (stick) -> conventional left-move/
	                        //     right-turn for a right-hander (see switchSticks to swap move/turn).
	int   switchSticks;     // 0 (default) = off-hand moves / dominant turns (conventional for right-handers).
	                        //     1 = swap so the DOMINANT hand moves and the off-hand turns.
	float aimPitchAdjust;   // degrees added to the controller aim pitch (the OpenXR aim pose sits higher
	                        //   than a held-gun barrel; negative tilts the ray DOWN). QZD's vr_weaponRotate.
	float hudDistanceM;     // distance of the head-locked HUD plane in metres. Apparent size is held
	                        //   constant across distance (physical extent scales with it), so this drives
	                        //   ONLY the stereo depth/vergence -- how deep the HUD sits.
	float hudSizeM;         // HUD plane height as a FRACTION of hudDistanceM (width follows HUD aspect).
	                        //   Sets the APPARENT (angular) size directly; independent of distance.
	float hudTiltDeg;       // degrees the HUD bottom-anchor is pitched down from horizontal (0 = eye level,
	                        //   30 = lower dashboard look, negative = above horizontal)
	float hudTextScale;     // global multiplier on all Lua HUD text (drives g_lua_hud_font_scale). 1.0 =
	                        //   unchanged; >1 enlarges HUD text (ammo counts etc.) for readability.
	int   mapPlayerUp;      // 1 = rotate the overhead map so the player's facing direction is always at the top
	int   teleportDistortion; // 1 = apply horizontal-stretch/vertical-compress warp during teleport fold effect
	int   showLaserSight;   // 1 = draw the laser-sight dot at the controller aim point; default 0 (off)
	int   showAimGizmos;    // 1 = draw controller diagnostic gizmos (grip markers + aim rays + two-handed
	                        //     aim vector) in world space. CODE-ONLY debug flag: no preferences UI and
	                        //     not persisted -- flip the default in vr_openxr.cpp s_settings to enable.
	                        //     default 0 (off)
	int   buttonAction[VR_BTN_COUNT]; // in-game action (VR_ACT_*) bound to each bindable button (VR_BTN_*)
	int   punchWithFists;   // 1 = thrust a fist forward fast to punch (velocity-driven), in addition to the
	                        //     trigger; each hand punches independently along where it points. default 1
	float punchSpeed;       // forward controller speed (m/s, measured along the aim direction) that triggers
	                        //     a fist punch. lower = easier/twitchier; higher = needs a committed thrust
	float leanGiveFraction; // head-lean "give" as a FRACTION of the player's collision radius: the head may
	                        //     lean this far from the body's room-space anchor before the body starts to
	                        //     follow (pure lean below it). Kills the against-a-wall ratchet and lets you
	                        //     lean into a wall a little. 0 = strict 1:1 (old behaviour). radius is read
	                        //     live from the physics model -- never hardcoded -- so the give tracks it.
} vr_settings_t;

vr_settings_t* VR_Settings(void);

// ---- Locomotion yaw offset (QuestZDoom model) ----
// The HMD yaw IS the player's facing; this offset is the extra rotation added by snap/smooth turning
// the right stick (so you can turn without physically turning your body). The engine drives the
// player's simulation facing to (this offset + head yaw), and the per-eye render rotates by this
// offset only (head yaw rides in via the eye pose). Both in Marathon angle units (512 = full circle).
float VR_GetYawOffset(void);                 // current locomotion yaw offset, angle units
void  VR_UpdateTurn(float rightStickX, float dtSeconds);  // advance snap/smooth turn from the stick
void  VR_SetYawOffset(float angleUnits);     // recenter: pin the offset (e.g. to face a start dir)

// Request the player face `facingAngleUnits` when the head is neutral (called at level entry so the
// start orientation is the level's, not wherever the headset points). Consumed once by the input
// code, which sets the yaw offset accordingly.
void VR_RequestYawRecenter(int facingAngleUnits);
bool VR_TakeYawRecenter(int* targetAngleUnits);

// True (once) when the runtime recentered our reference space (the user held the Meta/Quest button / a
// system recenter). The game tick consumes this to run the same recenter as the controller Recenter
// action (yaw-to-facing + height/lean recapture). Clears on read.
bool VR_TakeSystemRecenter(void);

// Dim the currently-bound eye buffer by VR_Settings()->brightness (a fullscreen multiply pass). Call
// after the world is rendered into the eye FBO. No-op at brightness >= 1.
void VR_DimCurrentEye(void);

// Queue a haptic vibration on the given controller (0=left, 1=right). durationMs is duration in
// milliseconds, amplitude in [0,1]. Silently ignored if a vibration is already queued for that hand
// this frame (prevents automatic-fire from stacking -- the existing vibration carries through).
// Applied at the start of the next frame via xrApplyHapticFeedback. No-op off Android.
void VR_Vibrate(int hand, float durationMs, float amplitude);

// Create and make-current a headless (pbuffer) GLES3 EGL context that the engine renders with.
// Immersive VR apps get no SurfaceView Surface, so SDL can't make a window GL context; we own one
// instead (the QuestZDoom/TBXR approach). Call on the engine/render thread before any GL use.
bool VR_InitEGL(void);

// Eye (per-view) framebuffer resolution chosen by the runtime; valid after the first frame. Used as
// the engine's logical screen size in VR. Returns false until known.
bool VR_GetEyeResolution(int* w, int* h);

// Head orientation from the last located HMD pose, in Marathon angle units (512 = full circle),
// signed (may be negative). yaw is the WORLD-frame azimuth contribution to ADD to the body yaw
// (derived to match the per-eye modelview's camera direction exactly); pitch is elevation (+ = up).
// The engine folds these into view->yaw/pitch (yaw = bodyYaw + this) so the visibility tree, polygon
// sort and sprite billboarding follow the head, consistent with what each eye renders. Returns false
// until a head pose has been located (output 0,0 in that case). Valid after the first VR_BeginFrame.
bool VR_GetHmdYawPitch(float* yawAngleUnits, float* pitchAngleUnits);

// ---- Controller input (OpenXR action set; Touch controllers) ----
// Latched each frame in VR_BeginFrame. Components are normalised [-1,1] / pressed booleans.
void VR_GetMove(float* x, float* y);   // left thumbstick: x=strafe, y=forward(+)

// Analog locomotion + room-scale head tracking (QuestZDoom model). VR_GetAnalogMove returns the
// deadzoned/rescaled left stick (partial deflection -> proportionally slower). VR_LatchHeadMove
// captures the head's physical movement since the last call as a world-space body delta (call once
// per real physics tick); VR_GetHeadMove returns it. The engine adds these to the player's velocity
// and position (with collision) so the body follows the head and the stick gives analog speed.
void VR_GetAnalogMove(float* strafe, float* forward);
void VR_LatchHeadMove(void);
void VR_GetHeadMove(float* x, float* y);

// Head horizontal position relative to the recenter reference, in Marathon world units (map x,y).
// The engine applies this to the VIEW ORIGIN (clamped against walls) so leaning/walking moves the
// camera + visibility origin without touching the physics position (render-side -> can't fly).
// VR_RecenterHead pins the reference to the current head (offset becomes 0 from there).
void VR_GetHeadOffset(float* wx, float* wy);
// Same offset for the render camera built on the INTERPOLATED body: pass the body's heartbeat_fraction t
// so the head reference is lerped across the tick boundary identically, cancelling the interpolation lag
// (removes the head-walk jitter/stepping that a live offset on a lagging int16 body would reintroduce).
void VR_GetHeadOffsetInterp(float t, float* wx, float* wy);
void VR_RecenterHead(void);

// Continuous FLOAT render-camera position (Marathon world units). The engine writes this each render
// frame (screen.cpp apply_vr_view_offsets) so the Rasterizer can place the camera continuously instead
// of at the int16 view.origin (whose 1-WU quantisation + nonlinear wall clamp makes close walls snap
// when you lean). X/Y = smooth interpolated body + head lean with the wall-clamp pushback low-passed.
// Z = the interpolated body height ONLY (no eye-Z): the live head height rides in via the vrView matrix,
// so the rasterizer must NOT re-add eye-Z. Publishing Z here (rather than subtracting an int16 eye-Z in
// the rasterizer) avoids a cross-call read mismatch that caused an occasional 1-WU vertical snap.
// VR_GetRenderCamera returns false until the first write this session.
void VR_SetRenderCamera(float wx, float wy, float wz);
bool VR_GetRenderCamera(float* wx, float* wy, float* wz);

// Vertical eye offset from neutral standing height, in Marathon world units (negative when seated).
// Added to view->origin.z so the visibility tree uses the true eye height; subtracted in the
// Rasterizer so the rendered camera is unchanged.
float VR_GetEyeZOffset(void);

// Effective eye-height reference (metres): the player's measured standing head height once recentered
// (nominal before that), minus the Height Adjust preference. Used as the vertical reference so in-game
// height is relative to the player's own stance (immune to floor-calibration errors and body-height
// differences), with the trim as a manual nudge on top.
float VR_EyeHeightM(void);

// Fed by the physics each tick with the player's in-game eye height above the floor (world units). The VR
// layer uses it with the measured standing height to pick a life-size world scale (worldScaleWUM), so the
// player is rendered at their true height and reaching the real floor lands on the game floor.
void VR_SetGameEyeHeightWU(float eyeHeightWU);

// Fed by the physics each tick with the local player's actual collision radius (world units) from the
// running physics model. The VR head-follow uses it (scaled by leanGiveFraction) as the head-lean
// deadzone, so the "give" is a fraction of the real radius rather than a hardcoded distance.
void VR_SetPlayerRadiusWU(float radiusWU);
void VR_GetTurn(float* x);             // non-dominant thumbstick X: snap/smooth turn
void VR_GetTurnY(float* y);            // non-dominant thumbstick Y: used for map zoom in-game
bool VR_GetFire(void);                 // right trigger
bool VR_GetSecondaryFire(void);        // left trigger

// Fist punch (velocity-driven melee): true for the frame(s) a hand is thrust forward faster than
// VR_Settings()->punchSpeed along where it points. Primary = dominant hand, Secondary = off-hand,
// so dual fists punch independently. Gated by VR_Settings()->punchWithFists; the caller must also
// confirm fists are equipped before routing these into the fire flags. Off Android: always false.
bool VR_GetPrimaryPunch(void);
bool VR_GetSecondaryPunch(void);
// True if a velocity punch occurred on that hand within the last ~200 ms (provenance window). Lets the
// weapon code tell a thrust-started fist shot (suppress the swing animation) from a trigger-started one.
bool VR_PrimaryPunchRecent(void);
bool VR_SecondaryPunchRecent(void);
bool VR_GetAction(void);               // A button (use terminals/switches)
bool VR_GetAdvance(void);              // A or X: advance terminal / skip cutscene
bool VR_GetBack(void);                 // Y or B: terminal page back
bool VR_GetButtonX(void);              // X button alone (in-game: previous weapon)
bool VR_GetButtonY(void);              // Y button alone (in-game: next weapon)
bool VR_GetMoveStickClick(void);       // press of the move thumbstick (in-game: run, honors toggle pref)
bool VR_GetTurnStickClick(void);       // press of the turn thumbstick -- the OPPOSITE hand from move (in-game: toggle overhead map)

// True while ANY bindable button currently mapped to `action` (a VR_ACT_* value) is held down. ORs all
// buttons bound to the same action. The in-game input builder uses this; edge detection for one-shot
// actions (weapon cycle / map / recenter) is done tick-side by the caller.
bool VR_ActionHeld(int action);

// Screenshot request (VR_ACT_SCREENSHOT). The input tick calls VR_RequestScreenshot() on the button
// edge; the render eye-loop calls VR_TakeScreenshotIfRequested() once per frame and, if true, reads
// one eye's framebuffer to a PNG (a clean, undistorted capture -- unlike the compositor grab). A
// simple cross-file one-shot flag.
void VR_RequestScreenshot(void);
bool VR_TakeScreenshotIfRequested(void);

// Increment 1: render one head-tracked stereo test frame (a colored room) to the headset and
// submit it. Drives the OpenXR session lifecycle internally. Returns true if a VR frame was
// presented (the caller should then skip SDL_GL_SwapWindow). Returns false if VR isn't ready yet.
// Used as the per-frame fallback for non-3D frames (menus/loading) in Increment 2+.
bool VR_RenderTestFrame(void);

// Submit a clean both-eyes (black, head-tracked) frame during a blocking load so the compositor keeps
// getting stereo frames and doesn't freeze on a stale/torn image (which Meta degrades to one eye).
// Call at level-load checkpoints -- see goto_level in game_wad.cpp. No-op on desktop / when VR inactive.
void VR_RenderLoadingFrame(void);

// Level-load texture prewarm. VR_RequestLevelWarmup() is called on level entry (enter_screen); the first
// render_view consumes it (VR_ConsumeLevelWarmup) and renders the scene once into the scratch screen-layer
// FBO (VR_BeginWarmupEye/VR_EndWarmup) with NO OpenXR frame begun -- forcing all lazy sprite/landscape/model
// texture uploads while a clean loading frame stays on screen, so the following real frame is fast in BOTH
// eyes. Without this the first frame's left eye stalls ~2 s inside a held frame -> the one-eye artifact.
void VR_RequestLevelWarmup(void);
bool VR_ConsumeLevelWarmup(void);
void VR_BeginWarmupEye(int eye);
void VR_EndWarmup(void);

// ---- Increment 2: per-eye frame loop the engine's render_view drives for the real world ----
// Begin a VR frame: advance the session, wait/begin the OpenXR frame, and locate the head pose +
// per-eye views. Returns true if the world should be rendered this frame (the engine then loops
// the eyes); the frame MUST be closed with VR_SubmitFrame regardless. Returns false if VR isn't
// rendering yet (no frame begun -> don't submit).
bool VR_BeginFrame(void);

// Bind + clear eye `eye`'s swapchain framebuffer and set the viewport. Between this and
// VR_FinishEye the engine renders the world for that eye.
void VR_BeginEye(int eye);
void VR_FinishEye(int eye);

// Per-eye matrices for the bound eye. Projection is an off-axis frustum from the runtime FOV
// (near/far in metres). View is eyeFromStage (metres, Y-up) -- the engine composes the Marathon
// world transform (origin/yaw/scale, Z-up world units) onto it.
void VR_GetEyeProjection(int eye, float* proj16, float zNearMetres, float zFarMetres);
void VR_GetEyeViewMetres(int eye, float* view16);

// Submit the frame to the compositor (xrEndFrame). Pairs with VR_BeginFrame.
void VR_SubmitFrame(void);

int  VR_EyeWidth(void);
int  VR_EyeHeight(void);
unsigned VR_CurrentEyeFramebuffer(void);   // GL FBO id bound for the current eye (for the blit target)
int  VR_CurrentEye(void);                  // eye index of the most recent VR_BeginEye (for SetView)

// One VR frame must be presented per engine frame: render_view presents the world frame and marks
// it; MainScreenSwap presents the menu/loading test frame only if the world frame didn't.
void VR_MarkWorldFramePresented(void);
bool VR_TakeWorldFramePresented(void);     // returns + clears the mark

// Screen layer: the engine renders its 2D UI (menus, terminals, loading) into this offscreen
// framebuffer; VR_PresentScreenLayer then shows it to both eyes as a flat head-locked panel. This
// is how menus/UI become visible+navigable in VR (the real SurfaceView is gone).
unsigned VR_ScreenLayerFramebuffer(void);
void     VR_PresentScreenLayer(void);

// Controller menu pointer: the screen-layer pixel an aiming controller is hitting on the (world-
// locked) 2D panel, and whether to click. The engine injects SDL mouse motion/clicks from these so
// the 2D menus/terminals become interactable. Computed during VR_PresentScreenLayer.
bool VR_GetPointerScreen(int* x, int* y);
bool VR_GetPointerClick(void);
bool VR_GetPointerGrip(void);   // true when the pointing hand's grip/squeeze is held (cheat modifier)
int      VR_ScreenLayerWidth(void);
int      VR_ScreenLayerHeight(void);

// ---- On-screen keyboard (menus/preferences only) ----
// A second world-locked, reclined quad shown below the menu panel whenever an SDL text field is
// focused, so the controller laser can type into dialog text entries (there is no physical keyboard
// on Quest). Keys synthesize the SDL_TEXTINPUT/SDL_KEYDOWN events the entry widgets already consume.
// The input hint selects the layout for the focused field. Reported by the text-entry widget on focus.
enum { VR_KB_ALPHA = 0, VR_KB_NUMERIC = 1, VR_KB_IP = 2 };
void VR_SetKeyboardInputHint(int hint);   // a text field gained focus: show the keyboard with this layout
void VR_KeyboardDismiss(void);            // a text field lost focus / dialog closed: hide the keyboard

// ---- In-game console keyboard (VR) ----
// Unlike the menu keyboard (drawn by VR_PresentScreenLayer, anchored to the world-locked menu panel),
// the in-game console keyboard must be driven + drawn during the 3D frame. Toggled with the bound
// VR_ACT_CONSOLE button (shell.cpp): on -> place a world-locked anchor in front of the head so the
// keyboard machinery has a panel to hang under. Update once per 3D frame; draw in the per-eye loop.
void VR_SetInGameKeyboard(bool on);       // console opened/closed in-game: show/hide the floating keyboard
bool VR_InGameKeyboardActive(void);       // true while the in-game keyboard is up (SDL text input still on)
void VR_UpdateInGameKeyboard(void);       // per-3D-frame: place/hover/click (call before render_view)
void VR_DrawInGameKeyboardEye(int eye);   // per-eye: composite the keyboard into eye `eye` (in the eye loop)
// Current console display line ("" if the console isn't open). Defined in Console.cpp so vr_openxr.cpp
// can render the console text strip WITHOUT including Console.h (which would pull the GLES shim in and
// break VR rendering -- see the note at the top of vr_openxr.cpp).
const char* VR_GetConsoleLine(void);
// Play the UI click sound (keyboard keypress feedback). Defined in shell.cpp so vr_openxr.cpp needn't
// include the sound/interface headers (same shim-include hazard as VR_GetConsoleLine).
void VR_PlayKeyClick(void);
// The on-screen keyboard's "hide" key blurs the focused text field so the keyboard dismisses (and a
// later tap on the field can bring it back). Implemented in the dialog layer, which owns widget focus.
void VR_DismissKeyboardField(void);

// Controller aim pose this frame, in STAGE space (metres, Y-up): origin (pos3) + unit forward (fwd3,
// the controller's -Z). hand 0=left, 1=right. Returns false if the pose isn't tracked. Used to draw
// the 3D-gun aim debug (controller marker + ray + hit dot) and, later, to aim weapons. The engine
// maps stage->Marathon world with the same transform Rasterizer_Shader::SetView uses, anchoring at
// the head (VR_GetHeadPosStage) so the player's absolute position in the play space cancels out.
bool VR_GetAimPoseStage(int hand, float pos3[3], float fwd3[3]);
// Raw pose right (+X) and up (+Y) in stage space — tracks full controller orientation (pitch, roll, yaw).
bool VR_GetAimOrientStage(int hand, float right3[3], float up3[3]);

// Two-handed weapon steadying: true when the off-hand grip is held and both controllers
// are within 0.5 m of each other. Use to override single-hand orientation/aim.
// Inform the VR layer whether dual-wield is active this frame. Two-handed steadying is
// suppressed while dual-wielding (each hand independently holds its own weapon).
void VR_SetIsDualWield(bool dual);
// Inform the VR layer whether the current weapon opted out of two-handed grip (MML
// <no_two_handed_weapon>, e.g. pistol/fusion pistol). When true, VR_IsTwoHandedActive() stays false.
void VR_SetTwoHandedDisabled(bool disabled);
// Per-weapon two-handed angle offset (degrees), pushed each frame from the render seam (0,0 for
// weapons without an MML <two_handed_offset>). Rotates the two-handed forward returned by
// VR_GetTwoHandedFwdStage — so BOTH the weapon model and the firing aim pick it up. pitch tilts the
// muzzle up(+)/down(-); yaw swings it sideways and is auto-mirrored for left-handed players.
void VR_SetTwoHandedOffset(float pitch_deg, float yaw_deg);
// Shared head-centre reference for DISCRETE sprite-view (N/NE/E/...) selection. The stereo eye loop
// sets it to the head-centre origin each frame so both eyes pick the same creature-rotation sprite
// (per-eye origins otherwise straddle the 45deg view boundaries -> different sprite per eye). Pass
// NULL to clear (non-VR/overhead-map paths). get_object_shape_and_transfer_mode consults it.
void VR_SetSpriteViewOrigin(const struct world_point3d* o);
bool VR_GetSpriteViewOrigin(struct world_point3d* out);
// True if the off-hand actually has a weapon sprite this frame (false when a dual-wield type
// is selected but only one weapon remains, so the off-hand trigger is suppressed).
void VR_SetOffHandHasWeapon(bool has);
// Enable/disable the dominant-grip secondary-fire binding. Disable for weapons whose secondary
// fire is identical to primary (pistol, fist) to prevent accidental shots from gripping.
void VR_SetGripAltFireEnabled(bool enabled);
bool VR_IsTwoHandedActive();
// Stage-space unit vector from dominant hand toward non-dominant hand — the two-handed
// weapon forward direction. Valid only when VR_IsTwoHandedActive() returns true.
bool VR_GetTwoHandedFwdStage(float fwd3[3]);

// Diagnostic: per-hand grip pose tracking state (true = actively optically TRACKED, false =
// runtime is dead-reckoning an estimate, e.g. the controller is occluded) and its linear speed
// in m/s. For logging drift during close-hand two-handed holds. Returns false for a bad index.
bool VR_GetHandTracking(int hand, bool* tracked, float* speed);

// Head position this frame in STAGE space (metres, Y-up). The aim-debug uses controller-minus-head so
// it doesn't double-count the head offset (which the renderer applies to view.origin, not the eye matrix).
bool VR_GetHeadPosStage(float pos3[3]);

// The dominant controller's aim direction as a Marathon-world unit vector (x/y horizontal, z up) --
// the same direction the aim-debug ray uses. Weapons fire along this instead of the player facing.
// Returns false if the controller pose isn't tracked.
bool VR_GetWeaponAim(float dir[3]);
// Off-hand (non-dominant) controller aim direction -- same transform as VR_GetWeaponAim but for the
// other hand. Used when the secondary weapon fires so the left gun aims along the left controller.
bool VR_GetSecondaryWeaponAim(float dir[3]);

// HUD layer: the engine draws the 2D HUD (Lua plugin HUD or classic OGL HUD) into this transparent
// offscreen each frame; VR_PresentHudEye then composites it head-locked, in front of the world, into
// each eye (alpha-blended, depth off). hudDistanceM/hudSizeM (VR settings) control its placement so
// VR users can push HUD elements out toward the edges of their field of view.
unsigned VR_HudLayerFramebuffer(void);
int      VR_HudLayerWidth(void);
int      VR_HudLayerHeight(void);
void     VR_PresentHudEye(int eye);

// Map overlay layer: when the player opens the overhead map in VR it is rendered into this offscreen
// FBO; VR_PresentMapEye then composites it head-locked over the world (the world keeps rendering
// underneath -- unlike desktop where the map replaces the world view).
// screen.cpp calls VR_SetMapActive each frame so the compositor knows whether to draw the overlay.
// `translucent` mirrors map_is_translucent(): false = solid panel (world hidden, opaque black fill);
// true = screen-blend map (black becomes transparent, colored lines float over the world).
void     VR_SetMapActive(bool active, bool translucent);
unsigned VR_MapLayerFramebuffer(void);
int      VR_MapLayerWidth(void);
int      VR_MapLayerHeight(void);
void     VR_PresentMapEye(int eye);

// Left-controller menu (hamburger) button press, consumed once -> the main loop opens the in-game
// quit-with-confirmation dialog.
bool VR_TakeMenuButton(void);

// True only while the OpenXR session is FOCUSED. Goes false when the user presses the Meta/home button
// and drops to the system overlay -> the engine pauses the game while unfocused.
bool VR_HasFocus(void);

// Per-eye horizontal offset from the head centre in Marathon world units (IPD separation).
// Add to view->origin before building the per-eye visibility tree so each eye's vis-tree
// is computed from the correct eye position. Restore view->origin before GPU rendering
// (SetView already encodes the IPD via VR_GetEyeViewMetres; do NOT double-count).
void VR_GetEyeIPDOffsetWU(int eye, float* wx, float* wy);

#ifdef __cplusplus
}
#endif

#endif // VR_OPENXR_H
