/*
 *  vr_net.cpp -- see vr_net.h. Serialization for the per-tick VR input block, plus the
 *  per-game activation flag shared by the spoke, hub and renderer.
 */

#include "vr_net.h"
#include "AStream.h"

#include <string.h>

// Per-game activation (see vr_net.h). Defaults off; StarGameProtocol::Sync sets it at game start
// from the negotiated gatherer kStar version. Single-player never turns it on.
static bool sVRNetActive = false;

void vr_net_set_active(bool active)
{
	sVRNetActive = active;
}

bool vr_net_is_active(void)
{
	return sVRNetActive;
}

void vr_block_clear(vr_block& b)
{
	memset(&b, 0, sizeof(b));
}

// Sim-side per-player current-tick block (see vr_net.h). MAXIMUM_NUMBER_OF_NETWORK_PLAYERS is the wire
// cap; the sim uses MAXIMUM_NUMBER_OF_PLAYERS which is the same value, but keep this self-contained.
enum { kVRSimMaxPlayers = 8 };
static vr_block sSimBlocks[kVRSimMaxPlayers];
static bool     sSimBlocksInit = false;

void vr_net_set_sim_block(int player_index, const vr_block& b)
{
	if (player_index < 0 || player_index >= kVRSimMaxPlayers) return;
	sSimBlocks[player_index] = b;
}

const vr_block& vr_net_get_sim_block(int player_index)
{
	static vr_block neutral;
	if (!sSimBlocksInit) { for (int i = 0; i < kVRSimMaxPlayers; ++i) vr_block_clear(sSimBlocks[i]); vr_block_clear(neutral); sSimBlocksInit = true; }
	if (player_index < 0 || player_index >= kVRSimMaxPlayers) return neutral;
	return sSimBlocks[player_index];
}

AIStream& operator>>(AIStream& s, vr_block& b)
{
	s >> b.forward
	  >> b.strafe
	  >> b.dom_yaw
	  >> b.dom_pitch
	  >> b.off_yaw
	  >> b.off_pitch
	  >> b.lean_x
	  >> b.lean_y
	  >> b.eye_z
	  >> b.head_yaw
	  >> b.head_pitch;
	return s;
}

AOStream& operator<<(AOStream& s, const vr_block& b)
{
	s << b.forward
	  << b.strafe
	  << b.dom_yaw
	  << b.dom_pitch
	  << b.off_yaw
	  << b.off_pitch
	  << b.lean_x
	  << b.lean_y
	  << b.eye_z
	  << b.head_yaw
	  << b.head_pitch;
	return s;
}
