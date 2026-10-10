#include "vr_weapons.h"
#include "weapons.h"
#include "items.h"
#include "map.h"         // game_is_networked
#include "interface.h"   // get_game_controller / _replay / _demo
#include "vr_openxr.h"   // VR_Settings
#include "FileHandler.h"
#include "ReplacementSounds.h"   // ExternalSoundHeader: decode a sound file
#include "OpenALManager.h"
#include <set>
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <array>
#include <cmath>

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


// ---------------- physical weapons (experimental) -- see docs/VR_PHYSICAL_WEAPONS.md ----------

static const std::map<std::string, int> kAttachNames = {
	{"vr_grip",      VR_ATTACH_GRIP},
	{"vr_foregrip",  VR_ATTACH_FOREGRIP},
	{"vr_magwell",   VR_ATTACH_MAGWELL},
	{"vr_muzzle",    VR_ATTACH_MUZZLE},
	{"vr_magwell2",  VR_ATTACH_MAGWELL2},
};

static const char* kAttachLabels[VR_ATTACH_COUNT] = {
	"vr_grip", "vr_foregrip", "vr_magwell", "vr_muzzle", "vr_mag_grip", "vr_magwell2"
};

static const std::map<std::string, int> kHolsterNames = {
	{"right_hip",   VR_HOLSTER_RIGHT_HIP},   {"left_hip",   VR_HOLSTER_LEFT_HIP},
	{"right_thigh", VR_HOLSTER_RIGHT_THIGH}, {"left_thigh", VR_HOLSTER_LEFT_THIGH},
	{"right_chest", VR_HOLSTER_RIGHT_CHEST}, {"left_chest", VR_HOLSTER_LEFT_CHEST},
	{"right_back",  VR_HOLSTER_RIGHT_BACK},  {"left_back",  VR_HOLSTER_LEFT_BACK},
	{"primary_pouch", VR_HOLSTER_LEFT_POUCH}, {"secondary_pouch", VR_HOLSTER_RIGHT_POUCH},
	{"left_pouch",  VR_HOLSTER_LEFT_POUCH},  {"right_pouch", VR_HOLSTER_RIGHT_POUCH},   // old names
	{"front_center", VR_HOLSTER_FRONT_CENTER}, {"back_center", VR_HOLSTER_BACK_CENTER},
};

static const char* kHolsterLabels[VR_HOLSTER_COUNT] = {
	"right_hip", "left_hip", "right_thigh", "left_thigh",
	"right_chest", "left_chest", "right_back", "left_back",
	"primary_pouch", "secondary_pouch", "front_center", "back_center"
};

// Starting anchor geometry, metres from the head in body space (+X right, +Y up, +Z forward),
// for a roughly 1.65 m standing eye height. These are first guesses meant to be replaced by the
// in-headset live tuner -- reaching for your own hip is the only way to know where it is.
// Offsets are FROM THE HEAD, so they are body landmarks measured down from eye level for the
// kReferenceEyeHeightM standing height: shoulders sit ~0.25 m below the eyes, chest ~0.40,
// waist/hip ~0.68, mid-thigh ~0.88. The first pass at these was a flat ~0.25 m too low and put the
// hips out of comfortable arm reach -- easy to get wrong from outside a headset, which is what the
// live tuner exists for.
static const float kReferenceEyeHeightM = 1.65f;
static const vr_holster_anchor kDefaultHolsters[VR_HOLSTER_COUNT] = {
	// Hips, thighs, chest pair and pouches: Daniel's in-headset setup (2026-09-28), stored exactly as set
	// (right-handed reference; mirrored for left-handed players).
	{  0.183f, -0.776f, -0.235f, 0.16f, {  0.099f, -0.986f, -0.131f }, {  0.083f, -0.123f,  0.989f }, true },  // right_hip
	{ -0.283f, -0.752f, -0.121f, 0.16f, { -0.166f, -0.953f, -0.253f }, {  0.416f, -0.301f,  0.858f }, true },  // left_hip
	{  0.155f, -0.497f, -0.066f, 0.16f, { -0.108f, -0.957f, -0.269f }, { -0.828f, -0.063f,  0.557f }, true },  // right_thigh
	{ -0.205f, -0.486f, -0.038f, 0.16f, { -0.048f, -0.998f, -0.051f }, {  0.929f, -0.063f,  0.365f }, true },  // left_thigh
	{  0.303f, -0.420f,  0.027f, 0.16f, { -0.076f, -0.997f, -0.023f }, { -0.529f,  0.020f,  0.848f }, true },  // right_chest (shotgun)
	{ -0.268f, -0.525f,  0.112f, 0.16f, { -0.073f, -0.995f, -0.075f }, {  0.575f, -0.103f,  0.812f }, true },  // left_chest
	{  0.20f, -0.22f, -0.18f, 0.18f },   // right_back   (reach up and over the shoulder)
	{ -0.20f, -0.22f, -0.18f, 0.18f },   // left_back
	{ -0.097f, -0.420f, 0.018f, 0.11f, { -0.055f, 0.021f, 0.998f }, {  0.058f, 0.998f, -0.018f }, true },  // primary_pouch
	{  0.082f, -0.470f, 0.003f, 0.11f, {  0.382f, -0.019f, 0.924f }, { -0.099f, 0.993f,  0.061f }, true },  // secondary_pouch
	{  0.114f, -0.606f,  0.051f, 0.20f, { -0.493f, -0.870f,  0.031f }, { -0.856f,  0.491f,  0.161f }, true },  // front_center (rifle)
	{  0.00f, -0.30f, -0.20f, 0.20f },   // back_center
};

static const vr_throw_params kDefaultThrow = { 10.f, 1.f, 0.18f };

// weapon_type -> per-attach-point array
static std::map<short, std::array<vr_attach_point, VR_ATTACH_COUNT> > s_attachPoints;
static std::map<short, int>              s_weaponHolsterSlot;
static std::map<short, vr_throw_params>  s_throwParams;
static std::map<short, short>            s_magItems;
static std::set<short>                   s_spinReloadWeapons;
static std::set<short>                   s_directLoadWeapons;
static vr_holster_anchor                 s_holsters[VR_HOLSTER_COUNT];
static bool                              s_holstersInit = false;

static void reset_holsters()
{
	for (int i = 0; i < VR_HOLSTER_COUNT; ++i) s_holsters[i] = kDefaultHolsters[i];
	s_holstersInit = true;
}

static std::array<vr_attach_point, VR_ATTACH_COUNT>& attach_entry(short weapon_type)
{
	auto it = s_attachPoints.find(weapon_type);
	if (it != s_attachPoints.end()) return it->second;
	std::array<vr_attach_point, VR_ATTACH_COUNT> fresh;
	for (int i = 0; i < VR_ATTACH_COUNT; ++i)
		fresh[i] = vr_attach_point{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.12f, 35.f, false };
	return s_attachPoints.emplace(weapon_type, fresh).first->second;
}

static void read_attach_attrs(const InfoTree& child, vr_attach_point& pt)
{
	child.read_attr("x", pt.x);
	child.read_attr("y", pt.y);
	child.read_attr("z", pt.z);
	child.read_attr("pitch", pt.pitch);
	child.read_attr("yaw",   pt.yaw);
	child.read_attr("roll",  pt.roll);
	child.read_attr("radius", pt.radius);
	child.read_attr("angle_tolerance", pt.angle_tolerance);
	child.read_attr("travel", pt.travel);
	child.read_attr("mag_width",  pt.mag_width);
	child.read_attr("mag_length", pt.mag_length);
	child.read_attr("mag_depth",  pt.mag_depth);
	child.read_attr("mag_round",  pt.mag_round);
	child.read_attr("mag_drum",   pt.mag_drum);
	child.read_attr("drop_on_release", pt.drop_on_release);
	{
		std::string hold;
		if (child.read_attr("mag_hold", hold))
			pt.mag_hold = (hold == "palm_up") ? 2 : (hold == "grip") ? 1 : 0;
	}
	pt.defined = true;
}

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

// ---------------- magazine sounds (<vr_mag_sounds>) ------------------------------------------------
// Key: weapon, well (0/1), event. The file is resolved when the MML is parsed -- a plugin's MML is read with
// the plugin's folder on the search path, so its paths are relative to the plugin. Decoded on first use.
struct vr_mag_sound { std::string path; bool loaded = false; Sound sound; };
static std::map<int, vr_mag_sound> s_magSounds;
static int vr_mag_sound_key(short weapon, int well, int event) { return (weapon * 2 + (well ? 1 : 0)) * VR_MAG_SOUND_COUNT + event; }

static std::map<short, std::vector<short> > s_partialMags;     // ammo item -> rounds of each part-used mag
static std::map<short, short>                s_droppedMags;     // map object -> rounds in it
void VR_PushPartialMag(short ammo_item, short rounds) { s_partialMags[ammo_item].push_back(rounds); }
bool VR_PopPartialMag(short ammo_item, short* rounds)
{
	auto it = s_partialMags.find(ammo_item);
	if (it == s_partialMags.end() || it->second.empty()) return false;
	if (rounds) *rounds = it->second.back();
	it->second.pop_back();
	return true;
}
int  VR_PartialMagCount(short ammo_item)
{
	auto it = s_partialMags.find(ammo_item);
	return it == s_partialMags.end() ? 0 : (int)it->second.size();
}
void VR_RegisterDroppedMag(short object_index, short rounds) { s_droppedMags[object_index] = rounds; }
bool VR_TakeDroppedMag(short object_index, short* rounds)
{
	auto it = s_droppedMags.find(object_index);
	if (it == s_droppedMags.end()) return false;
	if (rounds) *rounds = it->second;
	s_droppedMags.erase(it);
	return true;
}

bool VR_HasMagSound(short weapon_type, int well, int event)
{
	return s_magSounds.count(vr_mag_sound_key(weapon_type, well, event)) > 0;
}

bool VR_PlayMagSound(short weapon_type, int well, int event)
{
	if (!VR_PhysicalWeaponsActive()) return false;
	auto it = s_magSounds.find(vr_mag_sound_key(weapon_type, well, event));
	if (it == s_magSounds.end()) return false;
	vr_mag_sound& ms = it->second;
	if (!ms.loaded) {
		ms.loaded = true;   // one try: a file that will not decode stays silent instead of retrying every time
		FileSpecifier file(ms.path.c_str());
		ExternalSoundHeader header;
		ms.sound.data = header.LoadExternal(file);
		ms.sound.header = header;
	}
	if (!ms.sound.data || !OpenALManager::Get()) return false;
	SoundParameters params;          // 2D, unpanned: full volume under the master volume
	params.pitch = 1.f;
	OpenALManager::Get()->PlaySound(ms.sound, params);
	return true;
}

void reset_mml_vr_weapons()
{
	s_magSounds.clear();
	s_leftHandedWeapons.clear();
	s_noTwoHandedWeapons.clear();
	s_casingFwdOffsets.clear();
	s_spreadScales.clear();
	s_twoHandedOffsets.clear();
	s_attachPoints.clear();
	s_weaponHolsterSlot.clear();
	s_throwParams.clear();
	s_magItems.clear();
	s_spinReloadWeapons.clear();
	s_directLoadWeapons.clear();
	reset_holsters();
}

void parse_mml_vr_weapons(const InfoTree& root)
{
	for (const InfoTree& child : root.children_named("vr_mag_sounds"))
	{
		const short index = resolve_weapon_index(child);
		if (index < 0) continue;
		int well = -1;                                   // absent: both wells
		child.read_attr("well", well);
		static const char* kEvents[VR_MAG_SOUND_COUNT] = { "release", "remove", "insert" };
		for (int e = 0; e < VR_MAG_SOUND_COUNT; ++e) {
			std::string rel;
			if (!child.read_attr(kEvents[e], rel) || rel.empty()) continue;
			FileSpecifier file;
			if (!file.SetNameWithPath(rel.c_str())) continue;   // not found: leave the game's own sound
			for (int w = 0; w < 2; ++w) {
				if (well >= 0 && w != well) continue;
				vr_mag_sound ms;
				ms.path = file.GetPath();
				s_magSounds[vr_mag_sound_key(index, w, e)] = ms;
			}
		}
	}
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

	// ---- physical weapons: model-local attach points -------------------------------------
	for (const auto& named : kAttachNames)
	{
		for (const InfoTree& child : root.children_named(named.first))
		{
			short index = resolve_weapon_index(child);
			if (index >= 0)
				read_attach_attrs(child, attach_entry(index)[named.second]);
		}
	}
	// The magazine's own grip point rides on <vr_mag_model>, since it describes the mag, not the gun.
	for (const InfoTree& child : root.children_named("vr_mag_model"))
	{
		short index = resolve_weapon_index(child);
		if (index < 0) continue;
		std::string item;
		if (child.read_attr("item", item))
		{
			// Accept either a raw item index or one of the magazine item names.
			short item_index = -1;
			if (!item.empty() && (isdigit((unsigned char)item[0]) || item[0] == '-'))
				item_index = (short)atoi(item.c_str());
			else
			{
				static const std::map<std::string, short> kMagItems = {
					{"magnum_magazine",         _i_magnum_magazine},
					{"plasma_magazine",         _i_plasma_magazine},
					{"assault_rifle_magazine",  _i_assault_rifle_magazine},
					{"assault_grenade_magazine",_i_assault_grenade_magazine},
					{"missile_launcher_magazine",_i_missile_launcher_magazine},
					{"alien_shotgun_magazine",  _i_alien_shotgun_magazine},
					{"flamethrower_canister",   _i_flamethrower_canister},
					{"shotgun_magazine",        _i_shotgun_magazine},
					{"smg_ammo",                _i_smg_ammo},
				};
				auto mi = kMagItems.find(item);
				if (mi != kMagItems.end()) item_index = mi->second;
			}
			if (item_index >= 0) s_magItems[index] = item_index;
		}
		vr_attach_point& grip = attach_entry(index)[VR_ATTACH_MAG_GRIP];
		child.read_attr("grip_x", grip.x);
		child.read_attr("grip_y", grip.y);
		child.read_attr("grip_z", grip.z);
		child.read_attr("radius", grip.radius);
		grip.defined = true;
	}
	// ---- physical weapons: holster assignment + anchor geometry ---------------------------
	for (const InfoTree& child : root.children_named("vr_holster"))
	{
		short index = resolve_weapon_index(child);
		if (index < 0) continue;
		std::string slot;
		if (child.read_attr("slot", slot))
		{
			auto it = kHolsterNames.find(slot);
			if (it != kHolsterNames.end()) s_weaponHolsterSlot[index] = it->second;
		}
	}
	for (const InfoTree& child : root.children_named("vr_holster_anchor"))
	{
		std::string slot;
		if (!child.read_attr("slot", slot)) continue;
		auto it = kHolsterNames.find(slot);
		if (it == kHolsterNames.end()) continue;
		vr_holster_anchor& a = s_holsters[it->second];
		child.read_attr("x", a.x);
		child.read_attr("y", a.y);
		child.read_attr("z", a.z);
		child.read_attr("radius", a.radius);
		// Optional hang: barrel (fx fy fz) and top-of-gun (ux uy uz) directions, body space.
		if (child.read_attr("fx", a.fwd[0])) {
			child.read_attr("fy", a.fwd[1]); child.read_attr("fz", a.fwd[2]);
			child.read_attr("ux", a.up[0]);  child.read_attr("uy", a.up[1]);  child.read_attr("uz", a.up[2]);
			a.oriented = true;
		}
	}
	for (const InfoTree& child : root.children_named("vr_direct_load"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0) s_directLoadWeapons.insert(index);
	}
	for (const InfoTree& child : root.children_named("vr_spin_reload"))
	{
		short index = resolve_weapon_index(child);
		if (index >= 0) s_spinReloadWeapons.insert(index);
	}
	// ---- physical weapons: throw tuning ----------------------------------------------------
	for (const InfoTree& child : root.children_named("vr_throw"))
	{
		short index = resolve_weapon_index(child);
		if (index < 0) continue;
		vr_throw_params tp = kDefaultThrow;
		child.read_attr("damage",       tp.damage);
		child.read_attr("tumble",       tp.tumble);
		child.read_attr("catch_radius", tp.catch_radius);
		s_throwParams[index] = tp;
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


// ---------------- physical weapons: accessors -------------------------------------------------

static const vr_attach_point kUndefinedPoint = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.12f, 35.f, false };

// The player's tuned points (magwell tuning mode). Survive ResetAllMMLValues; saved in the preferences.
static std::map<std::pair<short, int>, vr_attach_point> s_attachOverrides;
static bool s_magwellTuneMode = false;

void VR_SetMagwellTuneMode(bool on) { s_magwellTuneMode = on; }
bool VR_GetMagwellTuneMode(void)    { return s_magwellTuneMode && VR_PhysicalWeaponsActive(); }

void VR_SetAttachOverride(short weapon_type, int which, const vr_attach_point& pt)
{
	if (which < 0 || which >= VR_ATTACH_COUNT) return;
	vr_attach_point p = pt;
	p.defined = true;
	s_attachOverrides[std::make_pair(weapon_type, which)] = p;
}
void VR_ClearAttachOverrides(void) { s_attachOverrides.clear(); }
std::vector<vr_attach_override> VR_GetAttachOverrides(void)
{
	std::vector<vr_attach_override> out;
	for (const auto& kv : s_attachOverrides) out.push_back({ kv.first.first, kv.first.second, kv.second });
	return out;
}
int VR_AttachTypeFromLabel(const char* label)
{
	for (int i = 0; i < VR_ATTACH_COUNT; ++i) if (label && !strcmp(label, kAttachLabels[i])) return i;
	return -1;
}
const char* VR_AttachTypeLabel(int which)
{
	return (which >= 0 && which < VR_ATTACH_COUNT) ? kAttachLabels[which] : "?";
}
short VR_WeaponFromMMLName(const char* name)
{
	if (!name) return NONE;
	auto it = kWeaponNames.find(name);
	return it != kWeaponNames.end() ? it->second : (short)NONE;
}

const vr_attach_point* VR_GetAttachPoint(short weapon_type, int which)
{
	if (which < 0 || which >= VR_ATTACH_COUNT) return &kUndefinedPoint;
	auto it = s_attachPoints.find(weapon_type);
	auto ov = s_attachOverrides.find(std::make_pair(weapon_type, which));
	if (ov != s_attachOverrides.end()) {
		// The magazine's SHAPE is not tuned in-headset: it always comes from the scenario MML.
		if (it != s_attachPoints.end() && it->second[which].defined) {
			ov->second.mag_round = it->second[which].mag_round;
			ov->second.mag_drum  = it->second[which].mag_drum;
			ov->second.drop_on_release = it->second[which].drop_on_release;
			ov->second.mag_hold = it->second[which].mag_hold;
		}
		return &ov->second;
	}
	if (it == s_attachPoints.end()) return &kUndefinedPoint;
	return &it->second[which];
}

vr_attach_point* VR_GetAttachPointMutable(short weapon_type, int which)
{
	if (which < 0 || which >= VR_ATTACH_COUNT) return nullptr;
	return &attach_entry(weapon_type)[which];
}

// Default hang for a slot, as authored for a RIGHT-handed player, in body space (+X right, +Y up,
// +Z forward): the barrel direction and the direction the top of the gun faces.
//   sides (hips, thighs, chest): barrel down, top of the slide facing forward
//   front centre: a patrol sling -- barrel down toward the off-hand side, the gun's side against you
//   backs: barrel up over the shoulder, top facing back
//   pouches: upright, facing forward
static void default_holster_orient(int slot, float f[3], float u[3])
{
	const float s = 0.70710678f;
	if (VR_HolsterSlotIsPouch(slot))                  { f[0]=0; f[1]=0;  f[2]=1;  u[0]=0;  u[1]=1; u[2]=0;  }
	else if (slot == VR_HOLSTER_FRONT_CENTER)         { f[0]=-s; f[1]=-s; f[2]=0; u[0]=-s; u[1]=s; u[2]=0;  }
	else if (slot == VR_HOLSTER_RIGHT_BACK || slot == VR_HOLSTER_LEFT_BACK || slot == VR_HOLSTER_BACK_CENTER)
	                                                  { f[0]=0; f[1]=1;  f[2]=0;  u[0]=0;  u[1]=0; u[2]=-1; }
	else                                              { f[0]=0; f[1]=-1; f[2]=0;  u[0]=0;  u[1]=0; u[2]=1;  }
}

// Player overrides: anchors set up in-headset. Kept apart from s_holsters (the MML/built-in defaults)
// because loading a game resets all MML state -- an override survives that, is saved in the player's
// preferences, and "reset to default" simply clears it.
static vr_holster_anchor s_holsterOverride[VR_HOLSTER_COUNT];
static bool              s_hasHolsterOverride[VR_HOLSTER_COUNT];

const vr_holster_anchor* VR_GetHolsterAnchor(int slot)
{
	if (!s_holstersInit) reset_holsters();
	if (slot < 0 || slot >= VR_HOLSTER_COUNT) slot = 0;
	if (s_hasHolsterOverride[slot]) return &s_holsterOverride[slot];
	return &s_holsters[slot];
}

bool VR_GetHolsterOverride(int slot, vr_holster_anchor* out)
{
	if (slot < 0 || slot >= VR_HOLSTER_COUNT || !s_hasHolsterOverride[slot]) return false;
	if (out) *out = s_holsterOverride[slot];
	return true;
}

void VR_SetHolsterOverride(int slot, const vr_holster_anchor& a)
{
	if (slot < 0 || slot >= VR_HOLSTER_COUNT) return;
	s_holsterOverride[slot] = a;
	s_hasHolsterOverride[slot] = true;
}

void VR_ClearHolsterOverrides(void)
{
	for (int i = 0; i < VR_HOLSTER_COUNT; ++i) s_hasHolsterOverride[i] = false;
}

int VR_HolsterSlotFromName(const char* name)
{
	if (!name) return VR_HOLSTER_NONE;
	auto it = kHolsterNames.find(name);
	return it != kHolsterNames.end() ? it->second : VR_HOLSTER_NONE;
}

// Anchor geometry adjusted for the player actually wearing the headset. Authored values are
// relative to kReferenceEyeHeightM; a taller or shorter player needs the same BODY LANDMARK, not
// the same absolute drop, so the vertical offset scales with their measured standing height. X and
// Z are left alone -- body width and depth do not track stature closely enough to be worth it.
void VR_GetHolsterAnchorForPlayer(int slot, float eyeHeightM, vr_holster_anchor* out)
{
	if (!out) return;
	*out = *VR_GetHolsterAnchor(slot);
	if (!out->oriented) default_holster_orient(slot, out->fwd, out->up);
	if (eyeHeightM > 0.5f && eyeHeightM < 2.5f)
		out->y *= (eyeHeightM / kReferenceEyeHeightM);
	// Anchors are authored for a RIGHT-handed player, so +X is the dominant side. A left-handed
	// player gets the whole layout mirrored -- position AND how each weapon hangs -- which keeps
	// "primary weapon on your primary hip" true for everyone instead of only for right-handers.
	if (VR_Settings()->dominantHand == 1) {
		out->x      = -out->x;
		out->fwd[0] = -out->fwd[0];
		out->up[0]  = -out->up[0];
	}
}

// The inverse of VR_GetHolsterAnchorForPlayer, into the player's OVERRIDE: undoes the left-handed mirror
// and the stature scaling, so what is stored stays a right-handed reference value.
void VR_SetHolsterAnchorFromPlayer(int slot, float eyeHeightM, float bx, float by, float bz,
                                   const float* fwd, const float* up)
{
	vr_holster_anchor* a = VR_GetHolsterAnchorMutable(slot);
	if (!a) return;
	const bool lefty = (VR_Settings()->dominantHand == 1);
	a->x = lefty ? -bx : bx;
	a->y = (eyeHeightM > 0.5f && eyeHeightM < 2.5f) ? by * (kReferenceEyeHeightM / eyeHeightM) : by;
	a->z = bz;
	if (fwd && up) {
		a->fwd[0] = lefty ? -fwd[0] : fwd[0]; a->fwd[1] = fwd[1]; a->fwd[2] = fwd[2];
		a->up[0]  = lefty ? -up[0]  : up[0];  a->up[1]  = up[1];  a->up[2]  = up[2];
		// Stored as unit vectors (the renderer re-squares them anyway, but the saved values should be clean).
		for (float* v : { a->fwd, a->up }) {
			const float l = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
			if (l > 1e-4f) { v[0] /= l; v[1] /= l; v[2] /= l; }
		}
		a->oriented = true;
	}
}

static bool s_holsterSetupMode = false;
void VR_SetHolsterSetupMode(bool on) { s_holsterSetupMode = on; }
bool VR_GetHolsterSetupMode(void)    { return s_holsterSetupMode && VR_PhysicalWeaponsActive(); }

// Editing an anchor edits the player's OVERRIDE (created from the current default on first touch), so
// the setup mode and the tuner both persist, and "reset to default" undoes either.
vr_holster_anchor* VR_GetHolsterAnchorMutable(int slot)
{
	if (!s_holstersInit) reset_holsters();
	if (slot < 0 || slot >= VR_HOLSTER_COUNT) return nullptr;
	if (!s_hasHolsterOverride[slot]) {
		s_holsterOverride[slot] = s_holsters[slot];
		s_hasHolsterOverride[slot] = true;
	}
	return &s_holsterOverride[slot];
}

int VR_GetWeaponHolsterSlot(short weapon_type)
{
	auto it = s_weaponHolsterSlot.find(weapon_type);
	return it != s_weaponHolsterSlot.end() ? it->second : VR_HOLSTER_NONE;
}

short VR_GetHolsterSlotWeapon(int slot)
{
	for (const auto& kv : s_weaponHolsterSlot)
		if (kv.second == slot) return kv.first;
	return NONE;
}

const vr_throw_params* VR_GetWeaponThrowParams(short weapon_type)
{
	auto it = s_throwParams.find(weapon_type);
	return it != s_throwParams.end() ? &it->second : &kDefaultThrow;
}

short VR_GetWeaponMagItem(short weapon_type)
{
	auto it = s_magItems.find(weapon_type);
	if (it != s_magItems.end()) return it->second;
	// Not authored: fall back to the weapon's own primary ammunition type, which is what a
	// magazine for it would be. NONE for weapons that take no ammunition (the fist).
	// Guard the range: get_player_weapon_ammo_type asserts on the dual-wield PSEUDO-weapon
	// indices (_weapon_doublefisted_pistols and up), which are >= MAXIMUM_NUMBER_OF_WEAPONS.
	if (weapon_type < 0 || weapon_type >= MAXIMUM_NUMBER_OF_WEAPONS) return NONE;
	return get_player_weapon_ammo_type(0, weapon_type, _primary_weapon);
}

// Physical weapons (docs/VR_PHYSICAL_WEAPONS.md) is opt-in AND single-player only. The mode gives
// each hand its own weapon state machine, which has no representation in the netgame wire protocol
// or in recorded films -- a client running it would desync instantly, and a film recorded with it
// would replay wrong. Everything funnels through here so the restriction lives in one place.
//
// Deliberately NOT in vr_openxr.cpp: that file forbids headers which transitively reach
// gl_es_compat.h, because its  macros hijack the shader GL calls and
// black out all VR rendering. This gate needs map.h and interface.h, so it lives here instead --
// vr_weapons.cpp does no GL at all, so the shim is harmless even if it does get pulled in.
extern "C" bool VR_PhysicalWeaponsActive(void)
{
	if (!VR_Settings()->experimentalWeapons) return false;
	if (game_is_networked) return false;
	const short controller = get_game_controller();
	if (controller == _replay || controller == _replay_from_file || controller == _demo) return false;
	return true;
}

static short s_holsterGrabPending = NONE;
static int   s_heldWeaponHand     = -1;

void VR_SetHeldWeaponHand(int hand) { s_heldWeaponHand = (hand == 0 || hand == 1) ? hand : -1; }
int  VR_GetHeldWeaponHand(void)     { return s_heldWeaponHand; }

void VR_SetHolsterGrabRequest(short weapon_type) { s_holsterGrabPending = weapon_type; }

bool VR_TakeHolsterGrabRequest(short* weapon_type)
{
	if (s_holsterGrabPending == NONE) return false;
	if (weapon_type) *weapon_type = s_holsterGrabPending;
	s_holsterGrabPending = NONE;
	return true;
}

static short s_handMag[2]        = { NONE, NONE };
static short s_handMagRounds[2]  = { -1, -1 };          // -1 = fresh (full)
static std::map<short, short> s_looseMagRounds;          // weapon -> rounds in its loosened mag
static std::map<short, int> s_magStates;   // weapon -> VR_MAG_*; absent = IN
// Pending seated magazines, weapon -> (ammo item, rounds). Per weapon so both hands can reload in one tick.
struct pending_reload { short ammo; short rounds; short triggerMask; };
static std::map<short, pending_reload> s_manualReloads;

void  VR_SetHandMagazine(int hand, short item_type)
{
	if (hand != 0 && hand != 1) return;
	s_handMag[hand] = item_type;
	s_handMagRounds[hand] = -1;
}
void  VR_SetHandMagazineRounds(int hand, short rounds) { if (hand==0||hand==1) s_handMagRounds[hand] = rounds; }
short VR_GetHandMagazineRounds(int hand)               { return (hand==0||hand==1) ? s_handMagRounds[hand] : (short)-1; }

// Keys: weapon, which gun of a two-fisted pair (copy: 0 = trigger 0, 1 = trigger 1; always 0 otherwise),
// and which well.
static short vr_mag_key(short weapon_type, int well, int copy)
{
	return short(weapon_type * 4 + (copy ? 2 : 0) + (well ? 1 : 0));
}
void  VR_SetLooseMagRounds(short weapon_type, short rounds, int well, int copy)
{
	s_looseMagRounds[vr_mag_key(weapon_type, well, copy)] = rounds;
}
short VR_GetLooseMagRounds(short weapon_type, int well, int copy)
{
	auto it = s_looseMagRounds.find(vr_mag_key(weapon_type, well, copy));
	return it != s_looseMagRounds.end() ? it->second : (short)0;
}

int VR_GetMagState(short weapon_type, int well, int copy)
{
	auto it = s_magStates.find(vr_mag_key(weapon_type, well, copy));
	return it != s_magStates.end() ? it->second : VR_MAG_IN;
}
void VR_SetMagState(short weapon_type, int state, int well, int copy)
{
	const short key = vr_mag_key(weapon_type, well, copy);
	if (state == VR_MAG_IN) s_magStates.erase(key);
	else s_magStates[key] = state;
}
// A two-fisted pair's guns trade trigger slots (see vr_swap_pair_triggers): their magazines go with them.
void VR_SwapMagCopies(short weapon_type)
{
	for (int well = 0; well < 2; ++well) {
		const int s0 = VR_GetMagState(weapon_type, well, 0), s1 = VR_GetMagState(weapon_type, well, 1);
		const short r0 = VR_GetLooseMagRounds(weapon_type, well, 0), r1 = VR_GetLooseMagRounds(weapon_type, well, 1);
		VR_SetMagState(weapon_type, s1, well, 0); VR_SetMagState(weapon_type, s0, well, 1);
		VR_SetLooseMagRounds(weapon_type, r1, well, 0); VR_SetLooseMagRounds(weapon_type, r0, well, 1);
	}
}
bool VR_WeaponDirectLoad(short weapon_type) { return s_directLoadWeapons.count(weapon_type) > 0; }
void VR_ResetMagStates(void) { s_magStates.clear(); s_looseMagRounds.clear(); s_droppedMags.clear(); }

static unsigned s_handsResetSerial = 0;
void VR_ResetHandsState(void)
{
	s_heldWeaponHand = -1;
	s_holsterGrabPending = NONE;
	for (int h = 0; h < 2; ++h) { s_handMag[h] = NONE; s_handMagRounds[h] = -1; }
	VR_ResetMagStates();          // every weapon starts with its magazine in
	s_manualReloads.clear();
	s_partialMags.clear();
	++s_handsResetSerial;
}
unsigned VR_GetHandsResetSerial(void) { return s_handsResetSerial; }

const vr_attach_point* VR_GetForegripOrDefault(short weapon_type)
{
	const vr_attach_point* fg = VR_GetAttachPoint(weapon_type, VR_ATTACH_FOREGRIP);
	if (fg->defined) return fg;
	// x, y, z, pitch, yaw, roll, radius, angle_tolerance, defined
	static const vr_attach_point kDefaultForegrip = { 0.f, -0.03f, 0.10f, 0.f, 0.f, 0.f, 0.12f, 35.f, true };
	return &kDefaultForegrip;
}
short VR_GetHandMagazine(int hand)                  { return (hand==0||hand==1) ? s_handMag[hand] : (short)NONE; }

void VR_RequestManualReload(short weapon_type, short ammo_item, short rounds, short trigger)
{
	if (weapon_type == NONE) return;
	const short mask = (trigger >= 0) ? short(1 << trigger) : short(0x7fff);
	auto it = s_manualReloads.find(weapon_type);
	if (it != s_manualReloads.end()) { it->second.triggerMask |= mask; return; }   // both hands, one tick
	s_manualReloads[weapon_type] = pending_reload{ ammo_item, rounds, mask };
}
bool VR_TakeManualReloadRequest(short weapon_type, short* ammo_item, short* rounds, short* triggerMask)
{
	auto it = s_manualReloads.find(weapon_type);
	if (it == s_manualReloads.end()) return false;
	if (ammo_item)   *ammo_item   = it->second.ammo;
	if (rounds)      *rounds      = it->second.rounds;
	if (triggerMask) *triggerMask = it->second.triggerMask;
	s_manualReloads.erase(it);
	return true;
}

const vr_attach_point* VR_GetMagwellOrDefault(short weapon_type)
{
	const vr_attach_point* mw = VR_GetAttachPoint(weapon_type, VR_ATTACH_MAGWELL);
	if (mw->defined) return mw;
	// x, y, z, pitch, yaw, roll, radius, angle_tolerance (180 = any approach), defined
	static const vr_attach_point kGripMagwell = { 0.f, -0.06f, 0.f, 0.f, 0.f, 0.f, 0.14f, 180.f, true };
	return &kGripMagwell;
}

const vr_attach_point* VR_GetMagwell2OrDefault(short weapon_type)
{
	const vr_attach_point* mw = VR_GetAttachPoint(weapon_type, VR_ATTACH_MAGWELL2);
	if (mw->defined) return mw;
	// under the front of the barrel, where an underslung launcher loads; any approach angle
	static const vr_attach_point kLauncherWell = { 0.f, -0.05f, 0.22f, 0.f, 0.f, 0.f, 0.12f, 180.f, true };
	return &kLauncherWell;
}

bool VR_WeaponUsesSpinReload(short weapon_type)
{
	return s_spinReloadWeapons.count(weapon_type) > 0;
}

bool VR_WeaponNeedsManualReload(short weapon_type)
{
	if (!VR_PhysicalWeaponsActive()) return false;
	// No magwell authored for this weapon yet -> let the engine reload it automatically. This is the
	// never-softlock rule: an unconfigured weapon must never become unusable once empty.
	return VR_WeaponUsesSpinReload(weapon_type) || weapon_takes_magazines(weapon_type);
}

const char* VR_AttachPointName(int which)
{
	return (which >= 0 && which < VR_ATTACH_COUNT) ? kAttachLabels[which] : "?";
}

bool VR_HolsterSlotIsCenter(int slot)
{
	return slot == VR_HOLSTER_FRONT_CENTER || slot == VR_HOLSTER_BACK_CENTER;
}

int VR_GetMirrorHolsterSlot(int slot)
{
	// The side anchors are ordered in pairs, so the mirror is just the low bit flipped. Centre-line
	// anchors have no opposite side.
	if (slot < 0 || slot >= VR_HOLSTER_COUNT) return VR_HOLSTER_NONE;
	if (VR_HolsterSlotIsCenter(slot)) return VR_HOLSTER_NONE;
	return slot ^ 1;
}

bool VR_HolsterSlotIsPouch(int slot)
{
	return slot == VR_HOLSTER_LEFT_POUCH || slot == VR_HOLSTER_RIGHT_POUCH;
}

const char* VR_HolsterSlotName(int slot)
{
	return (slot >= 0 && slot < VR_HOLSTER_COUNT) ? kHolsterLabels[slot] : "none";
}

const char* VR_HolsterSlotDisplayName(int slot)
{
	if (slot < 0 || slot >= VR_HOLSTER_COUNT) return "none";
	const bool mirrored = VR_Settings()->dominantHand == 1 &&
	                      !VR_HolsterSlotIsCenter(slot) && !VR_HolsterSlotIsPouch(slot);
	return kHolsterLabels[mirrored ? (slot ^ 1) : slot];
}

const char* VR_WeaponMMLName(short weapon_type)
{
	for (const auto& kv : kWeaponNames)
		if (kv.second == weapon_type) return kv.first.c_str();
	return nullptr;
}

// Serialise current in-memory state as a <vr_weapons> block. The live tuner prints this so tuned
// values can be pasted straight into the scenario's vr_weapons.mml -- the whole point of Phase 0
// is that nobody hand-guesses these numbers from outside the headset.
std::string VR_DumpAttachPointsMML()
{
	char line[512];
	std::string out = "<vr_weapons>\n";

	for (int i = 0; i < VR_HOLSTER_COUNT; ++i)
	{
		// Effective values (the player's override, else the default), with the hang written out even when
		// it is only the slot's default, so the block can be pasted as the new defaults verbatim.
		vr_holster_anchor a = *VR_GetHolsterAnchor(i);
		if (!a.oriented) default_holster_orient(i, a.fwd, a.up);
		snprintf(line, sizeof(line),
			"  <vr_holster_anchor slot='%s' x='%.3f' y='%.3f' z='%.3f' radius='%.3f' "
			"fx='%.3f' fy='%.3f' fz='%.3f' ux='%.3f' uy='%.3f' uz='%.3f'/>\n",
			kHolsterLabels[i], a.x, a.y, a.z, a.radius,
			a.fwd[0], a.fwd[1], a.fwd[2], a.up[0], a.up[1], a.up[2]);
		out += line;
	}

	// Effective points: the MML ones with the player's tuned overrides laid over them.
	std::map<short, std::array<vr_attach_point, VR_ATTACH_COUNT> > merged = s_attachPoints;
	for (const auto& ov : s_attachOverrides) {
		if (!merged.count(ov.first.first)) merged[ov.first.first] = attach_entry(ov.first.first);
		merged[ov.first.first][ov.first.second] = ov.second;
	}
	for (const auto& kv : merged)
	{
		const char* wname = VR_WeaponMMLName(kv.first);
		if (!wname) continue;
		for (int i = 0; i < VR_ATTACH_COUNT; ++i)
		{
			const vr_attach_point& pt = kv.second[i];
			if (!pt.defined) continue;			if (i == VR_ATTACH_MAG_GRIP)
			{
				snprintf(line, sizeof(line),
					"  <vr_mag_model name=\"%s\" grip_x=\"%.3f\" grip_y=\"%.3f\" grip_z=\"%.3f\" radius=\"%.3f\"/>\n",
					wname, pt.x, pt.y, pt.z, pt.radius);
			}
			else if (i == VR_ATTACH_MAGWELL || i == VR_ATTACH_MAGWELL2)
			{
				snprintf(line, sizeof(line),
					"  <%s name=\"%s\" x=\"%.3f\" y=\"%.3f\" z=\"%.3f\" "
					"pitch=\"%.1f\" yaw=\"%.1f\" roll=\"%.1f\" radius=\"%.3f\" angle_tolerance=\"%.1f\" travel='%.3f' "
					"mag_width='%.3f' mag_length='%.3f' mag_depth='%.3f' mag_round='%d' mag_drum='%d' drop_on_release='%d'/>\n",
					kAttachLabels[i], wname, pt.x, pt.y, pt.z, pt.pitch, pt.yaw, pt.roll, pt.radius, pt.angle_tolerance, pt.travel,
					pt.mag_width, pt.mag_length, pt.mag_depth, pt.mag_round ? 1 : 0, pt.mag_drum ? 1 : 0,
					pt.drop_on_release ? 1 : 0);
			}
			else
			{
				snprintf(line, sizeof(line),
					"  <%s name=\"%s\" x=\"%.3f\" y=\"%.3f\" z=\"%.3f\" radius=\"%.3f\"/>\n",
					kAttachLabels[i], wname, pt.x, pt.y, pt.z, pt.radius);
			}
			out += line;
		}
		int slot = VR_GetWeaponHolsterSlot(kv.first);
		if (slot != VR_HOLSTER_NONE)
		{
			snprintf(line, sizeof(line), "  <vr_holster name=\"%s\" slot=\"%s\"/>\n",
				wname, kHolsterLabels[slot]);
			out += line;
		}
	}

	out += "</vr_weapons>\n";
	return out;
}
