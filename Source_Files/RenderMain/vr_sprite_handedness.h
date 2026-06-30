#pragma once

#include "InfoTree.h"

// MML tag: <vr_sprites> containing <left_handed_weapon> and <vr_casing> entries.
//
// <left_handed_weapon> marks a weapon as naturally left-handed in the PC original.
// The VR renderer flips all sprites for that weapon when the player is right-handed.
//
// <vr_casing> sets a per-weapon forward spawn offset for shell casing billboards (in metres).
// Default is 0 (spawn at controller position). Use a positive value to push casings further
// in front of the muzzle. Attributes (use name OR index):
//   name="assault_rifle"   engine weapon name string (see mapping in .cpp)
//   index="N"              engine weapon-type constant (0=fist, 3=assault_rifle, ...)
//   fwd_offset="0.08"      metres forward along weapon aim direction (default 0)
//
// Example:
//   <vr_sprites>
//     <left_handed_weapon name="missile_launcher"/>
//     <vr_casing name="assault_rifle" fwd_offset="0.08"/>
//   </vr_sprites>
void reset_mml_vr_sprites();
void parse_mml_vr_sprites(const InfoTree& root);

// Returns true if weapon_type (engine index) was declared left-handed in MML.
bool VR_IsWeaponNaturallyLeftHanded(short weapon_type);

// Returns the casing spawn forward offset in metres (0 if not configured in MML).
float VR_GetWeaponCasingFwdOffset(short weapon_type);
