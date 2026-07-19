/*
 *  vr_net.h -- per-tick VR input block distributed in the star network protocol.
 *
 *  Part of the VR netcode extension (see docs/VR_NETCODE.md). Carries the VR-specific
 *  inputs that classic action_flags cannot express -- analog forward/strafe, per-hand aim,
 *  physical lean/crouch, and (cosmetic) head orientation -- so that every client simulates a
 *  VR player identically, with no single-player-vs-netgame behavior difference.
 *
 *  Activation is per-GAME, decided by the gatherer's "VR netcode" preference and distributed via
 *  the advertised Capabilities::kStar version (7 = VR protocol on, 6 = legacy). Every peer keys
 *  legacy-vs-new off the negotiated version -- a stock/older client that only speaks kStar 6 is
 *  refused when the host turns the protocol on, and when the host leaves it off the wire is
 *  byte-identical to stock so legacy clients can still join. See vr_net_is_active().
 */

#ifndef VR_NET_H
#define VR_NET_H

#include "cseries.h"

class AIStream;
class AOStream;

// [-1,1] analog float <-> int16 scale for forward/strafe.
enum { kVRNetAnalogScale = 1000 };

// One player's VR inputs for one tick. Fixed-width, quantized fields for deterministic
// cross-client decoding. Angles use Marathon's 512-unit circle (see NUMBER_OF_ANGLES).
struct vr_block
{
	int16  forward;    // analog forward/back  [-kVRNetAnalogScale, +kVRNetAnalogScale]
	int16  strafe;     // analog strafe         (+ = right)
	uint16 dom_yaw;    // dominant-hand aim yaw   [0, FULL_CIRCLE)     -- SIM: primary fire dir
	int16  dom_pitch;  // dominant-hand aim pitch (signed)             -- SIM: primary fire elev
	uint16 off_yaw;    // off-hand aim yaw                             -- SIM: secondary fire dir
	int16  off_pitch;  // off-hand aim pitch (signed)                  -- SIM: secondary fire elev
	int16  lean_x;     // physical lean offset, world units            -- SIM: shot origin
	int16  lean_y;     // physical lean offset, world units            -- SIM: shot origin
	int16  eye_z;      // physical crouch/eye-height offset, world u.   -- SIM: shot origin z
	uint16 head_yaw;   // head view yaw                                -- COSMETIC: pose viz
	int16  head_pitch; // head view pitch (signed)                     -- COSMETIC: pose viz
};

// Wire size: 11 fields x 2 bytes. Keep in sync with the (de)serializers.
enum { kVRBlockSerializedLength = 11 * 2 };

// Zero every field (neutral block: no VR input, aim straight ahead once the caller fills yaw).
void vr_block_clear(vr_block& b);

// Deterministic big-endian (de)serialization matching the star packet streams. Declared as free
// operators taking the AStream base refs so they bind to the concrete AIStreamBE/AOStreamBE used
// by the spoke/hub packet code.
AIStream& operator>>(AIStream& s, vr_block& b);
AOStream& operator<<(AOStream& s, const vr_block& b);

// Fill `out` from the LOCAL player's current input: live controller/HMD state when in VR, otherwise
// a neutral block whose aim points along the player's facing/elevation (so a flat client emits
// sensible defaults). Implemented in vbl.cpp (which has the VR getters, angle math, and player
// state). See docs/VR_NETCODE.md.
void vr_capture_local_net_block(vr_block& out);

// Latest VR block received for a given player (Phase 1: latest-pose, message-transported). Returns
// false if none received yet. Implemented in network_star_spoke.cpp. Used by the debug pose viz.
bool vr_net_get_player_block(int player_index, vr_block& out);

// --- Sim-side per-player block (Phase 2) ------------------------------------------------------
// The block delivered in lockstep with the action_flags the sim is CURRENTLY simulating for a player
// (set from ActionQueues::peekVRBlockAtHead in update_players). Every VR-affected sim computation
// reads it via vr_net_get_sim_block(player) so SP and netgame use the identical code path. See
// docs/VR_NETCODE.md. Returns a neutral (zeroed) block for players with no VR input this tick.
void            vr_net_set_sim_block(int player_index, const vr_block& b);
const vr_block& vr_net_get_sim_block(int player_index);

// Latch the block to be stored in the SAME RealActionQueues slot as this player's NEXT enqueued flag
// (SP local input + each netgame player at flag-enqueue time). Implemented in player.cpp (has
// GetRealActionQueues); declared here so the star spoke can call it. Not referenced by the standalone
// hub build (which has no sim), so its definition need not be linked there.
void            vr_net_latch_enqueue_block(int player_index, const vr_block& b);

// Whether THIS game is running the VR netcode extension. Set once at game start (StarGameProtocol::
// Sync) from the negotiated gatherer kStar version, so hub, spokes and the renderer all agree. When
// false the spoke/hub emit and relay NO VR messages (wire stays byte-identical to stock), so a game
// with the host preference off remains legacy-compatible. Implemented in vr_net.cpp.
void vr_net_set_active(bool active);
bool vr_net_is_active(void);

#endif // VR_NET_H
