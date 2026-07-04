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
// Attributes (all elements use name OR index to identify the weapon):
//   name="pistol"          engine weapon name string (see mapping in .cpp)
//   index="N"              engine weapon-type constant (0=fist, 1=pistol, 3=assault_rifle, ...)
//   fwd_offset="0.08"      (vr_casing)  metres forward along weapon aim direction (default 0)
//   scale="0.5"            (vr_spread)  spread multiplier applied in VR (default 1.0)
//
// Example:
//   <vr_weapons>
//     <left_handed_weapon name="missile_launcher"/>
//     <vr_casing name="assault_rifle" fwd_offset="0.08"/>
//     <vr_spread name="pistol" scale="0.5"/>
//   </vr_weapons>
void reset_mml_vr_weapons();
void parse_mml_vr_weapons(const InfoTree& root);

// Returns true if weapon_type (engine index) was declared left-handed in MML.
bool VR_IsWeaponNaturallyLeftHanded(short weapon_type);

// Returns the casing spawn forward offset in metres (0 if not configured in MML).
float VR_GetWeaponCasingFwdOffset(short weapon_type);

// Returns the VR spread multiplier for weapon_type (1.0 if not configured in MML).
float VR_GetWeaponSpreadScale(short weapon_type);
