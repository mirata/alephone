#pragma once

#include "InfoTree.h"

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
