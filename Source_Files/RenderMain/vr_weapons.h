#pragma once

#include "InfoTree.h"
#include <string>
#include <vector>

// MML tag: <vr_weapons> — VR-only weapon customizations.
// Contains <left_handed_weapon>, <vr_casing>, and <vr_spread> entries.
// (Formerly <vr_sprites> / vr_handedness.mml; renamed as the scope broadened
//  from sprite handedness to VR weapon tuning as a whole.)
//
// <left_handed_weapon> marks a weapon as naturally left-handed in the PC original.
// The VR renderer flips all sprites for that weapon when the player is right-handed.
//
// <vr_casing> sets a per-weapon forward spawn offset for shell casing billboards (in metres).
// Default is 0 (spawn at controller position). Use a positive value to push casings further
// in front of the muzzle.
//
// <vr_spread> scales a weapon's projectile spread (theta_error) when firing in VR.
// The PC original gives some weapons deliberate inaccuracy (e.g. the .44 Magnum) to
// simulate the difficulty of aiming a real handgun; in VR the controller already provides
// free 1:1 aim, so that baked-in spread makes such weapons feel useless. A scale < 1.0
// tightens the spread (0.5 = half the spread), 1.0 = unchanged (default), 0.0 = pinpoint.
//
// <no_two_handed_weapon> disables the two-handed (bring-the-hands-together) grip for a weapon.
// Pistol-style one-handers (magnum, fusion pistol) don't read well two-handed, so this keeps them
// single-handed even when the hands are close.
//
// <two_handed_offset> rotates a weapon's MODEL and AIM (together) while the two-handed grip is
// engaged, to correct grips that don't lie along the barrel. Two independent Euler angles, degrees:
//   pitch  tilts the muzzle up(+)/down(-) — e.g. the pistol reads best pointing ~30 up two-handed.
//   yaw    swings the muzzle sideways — e.g. the flamethrower's second handle sits 90 to the side,
//          so the inter-hand vector is 90 off the barrel and must be yawed back onto it.
// The yaw (the horizontal offset) is AUTO-MIRRORED for left-handed players, because the model is
// X-flipped and the off-hand handle ends up on the opposite side. Pitch is symmetric and never
// flips. If the sign points the wrong way on your model, just negate the value in MML.
//
// Attributes (all elements use name OR index to identify the weapon):
//   name="pistol"          engine weapon name string (see mapping in .cpp)
//   index="N"              engine weapon-type constant (0=fist, 1=pistol, 3=assault_rifle, ...)
//   fwd_offset="0.08"      (vr_casing)        metres forward along weapon aim direction (default 0)
//   scale="0.5"            (vr_spread)        spread multiplier applied in VR (default 1.0)
//   pitch="30" yaw="0"     (two_handed_offset) degrees (default 0); see above
//
// Example:
//   <vr_weapons>
//     <left_handed_weapon name="missile_launcher"/>
//     <vr_casing name="assault_rifle" fwd_offset="0.08"/>
//     <vr_spread name="pistol" scale="0.5"/>
//     <two_handed_offset name="pistol" pitch="30"/>
//     <two_handed_offset name="flamethrower" yaw="90"/>
//   </vr_weapons>
void reset_mml_vr_weapons();
void parse_mml_vr_weapons(const InfoTree& root);

// Returns true if weapon_type (engine index) was declared left-handed in MML.
bool VR_IsWeaponNaturallyLeftHanded(short weapon_type);

// Returns true if weapon_type had two-handed grip disabled in MML (<no_two_handed_weapon>).
bool VR_IsWeaponTwoHandedDisabled(short weapon_type);

// Returns the casing spawn forward offset in metres (0 if not configured in MML).
float VR_GetWeaponCasingFwdOffset(short weapon_type);

// Returns the VR spread multiplier for weapon_type (1.0 if not configured in MML).
float VR_GetWeaponSpreadScale(short weapon_type);

// Writes the two-handed model/aim angle offset for weapon_type (degrees) into *pitch/*yaw.
// Both are set to 0 when the weapon has no <two_handed_offset> entry. The caller pushes these to
// the VR layer every frame (VR_SetTwoHandedOffset), which applies the handedness mirroring.
void VR_GetWeaponTwoHandedOffset(short weapon_type, float* pitch_deg, float* yaw_deg);

// ---------------------------------------------------------------------------------------------
// PHYSICAL WEAPONS (experimental mode) -- see docs/VR_PHYSICAL_WEAPONS.md
//
// Aleph One's model loader has no MD3 tag/attachment support (Model3D.h), so every point where a
// hand, a magazine or a muzzle meets a weapon has to be authored by hand in MML. These are
// MODEL-LOCAL offsets in METRES, expressed in the weapon model's own frame:
//
//     +X = right      +Y = up      +Z = forward, down the barrel
//
// They are transformed to world space by the same anchor + basis the weapon model itself is drawn
// with (render.cpp render_vr_weapon_sprites_3d), so a point authored here lands exactly on the
// rendered mesh. Phase 0 ships a gizmo renderer + in-headset live tuner to author these values;
// see VR_GetAttachPoint / the <vr_holster_anchor> block below.
// ---------------------------------------------------------------------------------------------

enum   // model-local attach points, per weapon
{
	VR_ATTACH_GRIP,       // where the dominant hand holds it (usually the model origin)
	VR_ATTACH_FOREGRIP,   // where the off hand grabs for a two-handed hold
	VR_ATTACH_MAGWELL,    // where a magazine is inserted; uses orientation + angle_tolerance
	VR_ATTACH_MUZZLE,     // barrel exit, for muzzle flash / shot origin
	VR_ATTACH_MAG_GRIP,   // on the MAGAZINE model: where the hand holds it
	VR_ATTACH_MAGWELL2,   // a SECOND feed for the weapon's second-trigger ammo (the rifle's grenade launcher)
	VR_ATTACH_COUNT
};

enum   // body-relative holster anchors. Eight, which is exactly the number of holsterable
{      // weapons in the largest scenario (fist and ball are never holstered).
	VR_HOLSTER_RIGHT_HIP,   VR_HOLSTER_LEFT_HIP,
	VR_HOLSTER_RIGHT_THIGH, VR_HOLSTER_LEFT_THIGH,
	VR_HOLSTER_RIGHT_CHEST, VR_HOLSTER_LEFT_CHEST,
	VR_HOLSTER_RIGHT_BACK,  VR_HOLSTER_LEFT_BACK,
	// Ammo pouches. Not weapon holsters -- these DISPENSE magazines, always for the weapon in your
	// PRIMARY hand, so there is no pouch per ammo type (Marathon has nine). LEFT_POUCH = the primary
	// ammo pouch, RIGHT_POUCH = the secondary ammo pouch (the assault rifle's grenades); MML calls them
	// primary_pouch / secondary_pouch. Both sit on the off-hand side, since the free hand reaches in.
	VR_HOLSTER_LEFT_POUCH,  VR_HOLSTER_RIGHT_POUCH,
	// Centre-line anchors. These have no left/right sense, so they never mirror and never flip with
	// handedness -- for things carried across the body, like a rifle slung over the chest.
	VR_HOLSTER_FRONT_CENTER, VR_HOLSTER_BACK_CENTER,
	VR_HOLSTER_COUNT,
	VR_HOLSTER_NONE = -1
};

// True for the centre-line anchors, which have no side and so never mirror or flip.
bool VR_HolsterSlotIsCenter(int slot);

// The anchor on the opposite side of the body. Slots are laid out in right/left pairs, so a
// two-fisted weapon (magnum, shotgun) carried in pairs hangs on BOTH hips or BOTH sides of the
// chest -- grab the left one with your left hand, the right one with your right.
int VR_GetMirrorHolsterSlot(int slot);

// True for the ammo-pouch anchors, which dispense magazines rather than holding a weapon.
bool VR_HolsterSlotIsPouch(int slot);

struct vr_attach_point
{
	float x, y, z;              // model-local position, metres
	float pitch, yaw, roll;     // orientation, degrees (magwell insertion direction)
	float radius;               // grab/seat proximity radius, metres
	float angle_tolerance;      // degrees of misalignment tolerated when seating (magwell)
	bool  defined;              // false = not authored in MML; callers must fall back
	float travel;               // magwells: metres from the ENTRY point to the seated point. A magazine
	                            // slides along that line going in or out, free only beyond the entry.
	                            // 0 = it seats the moment it reaches the well.
	// Magwells: the magazine that goes in here, full size in metres (0 = the default 0.035 x 0.13 x 0.06).
	// width = across the gun, length = along the insertion line, depth = fore-aft. mag_round = a cylinder
	// (grenade, canister) of diameter `width`. Pitch/yaw above tilt the insertion line (a raked pistol grip);
	// roll turns the magazine about it.
	// mag_drum = a drum: a cylinder whose round faces look down the barrel (axis = the gun's forward),
	// `depth` thick fore-aft, `length` tall along the insertion line and `width` across (equal = circular).
	float mag_width, mag_length, mag_depth;
	bool  mag_round;
	bool  mag_drum;
	// Magwells: pressing the magazine release makes the magazine DROP a few centimetres out of the well
	// (pistol-style). Off = it stays seated until pulled or flicked out.
	bool  drop_on_release;
};

// Body-relative holster anchor. Offsets are METRES FROM THE HEAD in body space
// (+X right, +Y up, +Z forward), because the port tracks the head, not the torso.
struct vr_holster_anchor
{
	float x, y, z;
	float radius;               // grab proximity radius, metres
	// How the weapon hangs, as body-space unit directions (right-handed reference like the position):
	// fwd = where the barrel points, up = where the top of the gun faces. Only meaningful when
	// `oriented`; otherwise the slot's default hang applies (VR_GetHolsterAnchorForPlayer fills it in).
	float fwd[3];
	float up[3];
	bool  oriented;
};

// Per-weapon throw tuning (<vr_throw>).
struct vr_throw_params
{
	float damage;               // contact damage of a thrown weapon (0 = inert)
	float tumble;               // spin rate multiplier in flight (1.0 = default)
	float catch_radius;         // how close a hand must be to catch it mid-air, metres
};

// Model-local attach point for a weapon. Always returns a valid pointer; check ->defined.
const vr_attach_point* VR_GetAttachPoint(short weapon_type, int which);

// Mutable access, for the in-headset live tuner (Phase 0). Creates the entry if absent.
vr_attach_point* VR_GetAttachPointMutable(short weapon_type, int which);

// Body-relative holster anchor geometry. Always valid; defaults are built in.
const vr_holster_anchor* VR_GetHolsterAnchor(int slot);
vr_holster_anchor*       VR_GetHolsterAnchorMutable(int slot);

// Anchor adjusted for this player: authored values assume a 1.65 m standing eye height, and the
// vertical offset scales with the wearer measured height so anchors land on the same body landmark
// whoever is playing. Use this everywhere an anchor is POSITIONED; the raw accessors above are for
// the tuner, which edits the reference value.
void VR_GetHolsterAnchorForPlayer(int slot, float eyeHeightM, vr_holster_anchor* out);

// Which holster slot a weapon lives on, or VR_HOLSTER_NONE if unassigned/not holsterable.
int  VR_GetWeaponHolsterSlot(short weapon_type);
// Inverse: which weapon is assigned to a slot, or NONE.
short VR_GetHolsterSlotWeapon(int slot);

// Throw tuning for a weapon (built-in defaults when not authored).
const vr_throw_params* VR_GetWeaponThrowParams(short weapon_type);

// The item type used as this weapon's magazine model. NONE when the weapon takes no magazine;
// defaults to the weapon's own ammunition_type when <vr_mag_model> is absent.
short VR_GetWeaponMagItem(short weapon_type);

// Human-readable names, for gizmo labels and the MML dump.
const char* VR_AttachPointName(int which);
const char* VR_HolsterSlotName(int slot);
// The slot's name as THIS player sees it. Slots are authored for a right-hander (right_* = gun-hand side)
// and mirrored for left-handers, so for a lefty "right_hip" is physically the LEFT hip. Use this for
// anything shown to the player; keep VR_HolsterSlotName (the right-handed reference) for MML.
const char* VR_HolsterSlotDisplayName(int slot);
// Engine weapon-type constant -> the name= string used in MML ("assault_rifle"), or nullptr.
const char* VR_WeaponMMLName(short weapon_type);

// Holster grab hand-off. The VR render layer detects "hand gripped near an occupied holster" (it
// owns the controller poses) and posts the weapon here; the input tick (vbl.cpp) takes it and issues
// an ordinary hotkey weapon select through the action-flag stream. Keeping weapon selection on that
// one path means holster draws behave exactly like a number key, netplay included.
void  VR_SetHolsterGrabRequest(short weapon_type);
bool  VR_TakeHolsterGrabRequest(short* weapon_type);

// Which physical hand is holding the drawn weapon (0 = left, 1 = right), or -1 when empty-handed.
// Set when a weapon is grabbed off a holster: the hand that TAKES a weapon owns it, so that hand
// aims it and that hand trigger fires it, whichever hand the player configured as dominant.
void VR_SetHeldWeaponHand(int hand);
int  VR_GetHeldWeaponHand(void);

// ---- magazines -------------------------------------------------------------------------------
// A hand can hold a magazine instead of a weapon: reach into an ammo pouch and grip. The item type
// is what the engine will consume when the mag is seated -- but nothing is deducted at grab time,
// only on a successful seat, so grabbing a mag and dropping it cannot duplicate ammo.
void  VR_SetHandMagazine(int hand, short item_type);   // NONE = empty hand
short VR_GetHandMagazine(int hand);

// Manual-reload handshake. Seating a magazine posts a request here; reload_weapon() takes it and
// then runs the engine ordinary reload states, so ammo accounting, sounds and animation all stay
// engine-side. Without a pending request an empty weapon simply stays empty and clicks.
// The request names the WEAPON the mag went into and the AMMO it carries, so a mag seated in the
// second hand's gun cannot be consumed by the primary's reload, and a bullet mag cannot refill the
// assault rifle's grenade launcher.
// rounds: how many rounds the seated magazine holds, or -1 for a fresh one from a pouch (full, and it
// uses up one magazine from the ammo count). A magazine taken out of a gun keeps its rounds, and putting
// it back restores exactly those, taking nothing from the ammo count.
// trigger: which of the weapon's triggers this reload is for, or -1 for all of them. A dual pair is ONE
// weapon with a trigger per hand, so flicking one shotgun must name its own trigger or both reload.
// Requests for the same weapon in the same tick merge (both hands flicking reloads both).
void VR_RequestManualReload(short weapon_type, short ammo_item, short rounds, short trigger = -1);
// triggerMask: bit t set = trigger t requested.
bool VR_TakeManualReloadRequest(short weapon_type, short* ammo_item, short* rounds, short* triggerMask);

// True when this weapon must be reloaded by hand: physical weapons is on and the weapon either takes
// magazines or reloads by spinning. Nothing auto-reloads in this mode -- an empty gun stays empty and
// dry-fires until you reload it.
bool VR_WeaponNeedsManualReload(short weapon_type);

// The magwell a magazine seats into: the authored <vr_magwell>, or until one is placed, a default at
// the grip (where a pistol's magazine goes anyway) with a generous radius and no approach-angle check,
// so every magazine weapon can be reloaded by hand from day one. Always returns a point.
const vr_attach_point* VR_GetMagwellOrDefault(short weapon_type);

// Where second-trigger ammo goes in -- the assault rifle's grenades load into the launcher, not the main
// magwell. The authored <vr_magwell2>, or a default under the front of the barrel until one is placed.
// Nothing ever comes OUT of it: grenades have no eject, and flicking the gun never touches it.
const vr_attach_point* VR_GetMagwell2OrDefault(short weapon_type);

// Weapons reloaded by a GESTURE instead of a magazine (<vr_spin_reload name=.../>): the shotgun's
// Terminator-2 flip-cock. Spin the gun a full turn in your hand and it reloads. The pouches never
// hand out magazines for these.
bool VR_WeaponUsesSpinReload(short weapon_type);

// Magazine state per weapon. Ejecting LOOSENS the magazine: the rounds left in it go (the engine has no
// partial magazines) and it hangs in the magwell until pulled out by the free hand or shaken free with a
// hard flick. A new magazine only seats once the old one is OUT. Weapons start with their mag IN.
enum { VR_MAG_IN, VR_MAG_LOOSE, VR_MAG_OUT };
// well: 0 = the main magwell, 1 = the second one (the rifle's grenade launcher).
int  VR_GetMagState(short weapon_type, int well = 0, int copy = 0);   // copy: which gun of a two-fisted pair
void VR_SetMagState(short weapon_type, int state, int well = 0, int copy = 0);
void VR_SwapMagCopies(short weapon_type);    // a pair's two guns traded trigger slots
void VR_ResetMagStates(void);

// Rounds in the magazine a hand is holding: -1 = a fresh one from a pouch (full), otherwise the count
// it had when it came out of a gun. VR_SetHandMagazine resets it to -1; set it after.
void  VR_SetHandMagazineRounds(int hand, short rounds);
short VR_GetHandMagazineRounds(int hand);

// Rounds held by a weapon's LOOSENED magazine -- stored on eject, so pulling the mag keeps them.
void  VR_SetLooseMagRounds(short weapon_type, short rounds, int well = 0, int copy = 0);
short VR_GetLooseMagRounds(short weapon_type, int well = 0, int copy = 0);

// Direct-load weapons (<vr_direct_load name=.../>, the rocket launcher): nothing comes out -- a new load
// goes straight in once it is empty, and the magazine release does nothing.
bool VR_WeaponDirectLoad(short weapon_type);

// The two-handed grab spot: the authored <vr_foregrip>, or a default just forward of and below the
// grip (a support hand cupping it) until one is placed. Two-handing only engages with the support
// hand inside this point's radius. Always returns a point.
const vr_attach_point* VR_GetForegripOrDefault(short weapon_type);

// Holster & pouch SETUP mode (entered from the in-game menu): body anchors are dragged by hand.
void VR_SetHolsterSetupMode(bool on);
bool VR_GetHolsterSetupMode(void);

// Write an anchor back from a position measured on THIS player (body frame, metres from the head) --
// the inverse of VR_GetHolsterAnchorForPlayer: undoes the stature scaling and the left-handed mirror, so
// the stored value stays a right-handed reference that is correct for everyone.
void VR_SetHolsterAnchorFromPlayer(int slot, float eyeHeightM, float bx, float by, float bz,
                                   const float* fwd = nullptr, const float* up = nullptr);

// The player's own anchor positions (set up in-headset), kept apart from the MML/built-in defaults so
// loading a game -- which resets all MML state -- does not wipe them. Saved in the preferences.
bool VR_GetHolsterOverride(int slot, vr_holster_anchor* out);
void VR_SetHolsterOverride(int slot, const vr_holster_anchor& a);
void VR_ClearHolsterOverrides(void);          // "reset holsters to default"
int  VR_HolsterSlotFromName(const char* name);  // MML/prefs slot name -> slot, VR_HOLSTER_NONE if unknown

// Magazine sounds, per weapon and well (<vr_mag_sounds name=... [well=0|1] release= remove= insert=> in a
// plugin's MML; file paths relative to that plugin). Weapon handling mode only. The insert sound REPLACES
// the weapon's own reload sound when one is configured.
enum { VR_MAG_SOUND_RELEASE, VR_MAG_SOUND_REMOVE, VR_MAG_SOUND_INSERT, VR_MAG_SOUND_COUNT };
bool VR_HasMagSound(short weapon_type, int well, int event);

// Part-used magazines. The engine counts ammo in whole magazines (items[ammo]); these remember how many
// rounds the part-used ones among them hold. A magazine dropped in the world (an ammo pickup we spawned)
// carries its rounds until picked up again.
void  VR_PushPartialMag(short ammo_item, short rounds);
bool  VR_PopPartialMag(short ammo_item, short* rounds);        // false: none
int   VR_PartialMagCount(short ammo_item);
void  VR_RegisterDroppedMag(short object_index, short rounds);
bool  VR_TakeDroppedMag(short object_index, short* rounds);    // false: not one of ours
bool VR_PlayMagSound(short weapon_type, int well, int event);   // false if none configured / no audio

// MAGWELL TUNING mode (entered from the in-game menu, persisted): the sticks move and resize the held
// gun's magwells and two-handed grab spot instead of moving the player.
void VR_SetMagwellTuneMode(bool on);
bool VR_GetMagwellTuneMode(void);          // false unless physical weapons are active

// The player's own tuned attach points (magwells, grab spots), kept apart from the MML defaults exactly
// like the holster overrides: they win over MML in VR_GetAttachPoint and are saved in the preferences.
void VR_SetAttachOverride(short weapon_type, int which, const vr_attach_point& pt);
void VR_ClearAttachOverrides(void);        // "reset magwells to default"
struct vr_attach_override { short weapon_type; int which; vr_attach_point pt; };
std::vector<vr_attach_override> VR_GetAttachOverrides(void);
int  VR_AttachTypeFromLabel(const char* label);   // "vr_magwell" etc -> VR_ATTACH_*, -1 if unknown
const char* VR_AttachTypeLabel(int which);
short VR_WeaponFromMMLName(const char* name);     // NONE if unknown

// Serialise the CURRENT in-memory attach-point / holster-anchor state back out as a <vr_weapons>
// MML block, so the live tuner can dump tuned values straight into vr_weapons.mml.
// Returns the block as a string.
std::string VR_DumpAttachPointsMML();
