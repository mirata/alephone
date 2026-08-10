#include "vr_weapons.h"
#include "weapons.h"
#include <set>
#include <map>
#include <string>

static const std::map<std::string, short> kWeaponNames = {
	{"fist",             _weapon_fist},
	{"pistol",           _weapon_pistol},
	{"plasma_pistol",    _weapon_plasma_pistol},
	{"assault_rifle",    _weapon_assault_rifle},
	{"missile_launcher", _weapon_missile_launcher},
	{"flamethrower",     _weapon_flamethrower},
	{"alien_shotgun",    _weapon_alien_shotgun},
	{"shotgun",          _weapon_shotgun},
	{"ball",             _weapon_ball},
	{"smg",              _weapon_smg},
};

struct TwoHandedOffset { float pitch; float yaw; };

static std::set<short> s_leftHandedWeapons;
static std::set<short> s_noTwoHandedWeapons;
static std::map<short, float> s_casingFwdOffsets;
static std::map<short, float> s_spreadScales;
static std::map<short, TwoHandedOffset> s_twoHandedOffsets;

// Resolve a <...> child's target weapon from its "index" or "name" attribute.
// Returns the engine weapon-type constant, or -1 if unspecified/unknown.
static short resolve_weapon_index(const InfoTree& child)
{
	short index = -1;
	child.read_attr("index", index);
	if (index < 0)
	{
		std::string name;
		if (child.read_attr("name", name))
		{
			auto it = kWeaponNames.find(name);
			if (it != kWeaponNames.end())
				index = it->second;
		}
	}
	return index;
}

void reset_mml_vr_weapons()
{
	s_leftHandedWeapons.clear();
	s_noTwoHandedWeapons.clear();
	s_casingFwdOffsets.clear();
	s_spreadScales.clear();
	s_twoHandedOffsets.clear();
}

void parse_mml_vr_weapons(const InfoTree& root)
{
	for (const InfoTree& child : root.children_named("left_handed_weapon"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0)
			s_leftHandedWeapons.insert(index);
	}
	for (const InfoTree& child : root.children_named("no_two_handed_weapon"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0)
			s_noTwoHandedWeapons.insert(index);
	}
	for (const InfoTree& child : root.children_named("vr_casing"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0)
		{
			float fwd = 0.f;
			child.read_attr("fwd_offset", fwd);
			s_casingFwdOffsets[index] = fwd;
		}
	}
	for (const InfoTree& child : root.children_named("vr_spread"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0)
		{
			float scale = 1.f;
			child.read_attr("scale", scale);
			if (scale < 0.f) scale = 0.f;
			s_spreadScales[index] = scale;
		}
	}
	for (const InfoTree& child : root.children_named("two_handed_offset"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0)
		{
			TwoHandedOffset off = { 0.f, 0.f };
			child.read_attr("pitch", off.pitch);
			child.read_attr("yaw",   off.yaw);
			s_twoHandedOffsets[index] = off;
		}
	}
}

bool VR_IsWeaponNaturallyLeftHanded(short weapon_type)
{
	return s_leftHandedWeapons.count(weapon_type) > 0;
}

bool VR_IsWeaponTwoHandedDisabled(short weapon_type)
{
	return s_noTwoHandedWeapons.count(weapon_type) > 0;
}

float VR_GetWeaponCasingFwdOffset(short weapon_type)
{
	auto it = s_casingFwdOffsets.find(weapon_type);
	return it != s_casingFwdOffsets.end() ? it->second : 0.f;
}

float VR_GetWeaponSpreadScale(short weapon_type)
{
	auto it = s_spreadScales.find(weapon_type);
	return it != s_spreadScales.end() ? it->second : 1.f;
}

void VR_GetWeaponTwoHandedOffset(short weapon_type, float* pitch_deg, float* yaw_deg)
{
	float p = 0.f, y = 0.f;
	auto it = s_twoHandedOffsets.find(weapon_type);
	if (it != s_twoHandedOffsets.end()) { p = it->second.pitch; y = it->second.yaw; }
	if (pitch_deg) *pitch_deg = p;
	if (yaw_deg)   *yaw_deg   = y;
}
