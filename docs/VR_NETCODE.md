# VR Netcode Extension — Design Spec

> ## ✅ LIVE STATUS — 2026-07-19 (Phase 1 + 1b landed on `questvr` HEAD; builds + deploys)
> Phase 1 (the transport pipe) and Phase 1b (debug orientation lines) are **implemented on the working
> tree** (no longer stashed) and the Android build succeeds + deploys. One deliberate **deviation from
> §2/§8 below**, chosen to match Daniel's spoken model ("a preference to enable the new protocol; if I
> host with it on, the new stuff takes effect; if not, legacy; a connecting client checks the version"):
>
> - **Activation is preference-gated per game, NOT an unconditional `kStar` bump.** A new capability
>   **`Capabilities::kVR` (=1)** is advertised by every build. A host pref **`network_preferences->
>   use_vr_netcode`** (Quest default ON, desktop default off; toggle in Network prefs → lobby tab
>   "Host with VR Netcode Extension") decides. When the gatherer hosts with it on it advertises `kVR=1`;
>   a joiner reads that (`sGathererAdvertisedVR`) and activates; a stock/older client that can't speak
>   `kVR` is **refused only then** (capabilities_indicate_player_is_gatherable). Host off → advertises
>   `kVR=0` → wire is byte-identical to stock, so **legacy games stay open to any client**. `kStar` is
>   left at 6. This gives the "legacy when off / graceful desktop fallback" the plain kStar bump could not.
> - **Single source of truth:** `NetVRNetcodeActive(isServer)` (network.cpp) → `vr_net_set_active()` in
>   `StarGameProtocol::Sync` → `vr_net_is_active()`, read by spoke (emit), hub (relay) and renderer.
> - **Landed files:** `Network/vr_net.{h,cpp}` (NEW; block + BE serialization + activation flag);
>   `network_star.h` `kVRPoseMessageType`; `network_capabilities.{h,cpp}` `kVR`; `network.{cpp,h}`
>   (advertise/capture/refuse/accessor); `StarGameProtocol.cpp` (activation); `network_star_hub.cpp` +
>   `network_star_spoke.cpp` (store/relay/emit, gated on `vr_net_is_active()`); `vbl.cpp`
>   `vr_capture_local_net_block()`; `render.cpp` `render_net_vr_pose()` (Phase 1b, Android eye loop);
>   `vr_openxr.{h,cpp}` `showNetPose` toggle (default 1); `preferences.{h,cpp}` pref + UI. Build reg:
>   `Makefile.am` (Linux), `LibAlephOne.vcxproj` (Windows), Android CMake auto-globs. `VRNET rx …`
>   throttled log is the secondary pipe proof.
>
> ### NOT done (deferred until the debug lines are verified on 2 machines — per the movement-revert lesson)
> - **On-device PC↔Quest / Quest↔Quest test.** Watch the debug lines (green head ray, red dominant-gun,
>   blue off-hand ray, yellow eye tick) drawn on the Quest for the OTHER player, and/or the `VRNET rx`
>   log. A flat PC player's rays point along their facing (graceful fallback); a VR player's track their
>   real head/guns. **Verify this before wiring any sim behavior.**
> - **Phase 2 (sim consumption).** The `!game_is_networked` gates in `physics.cpp`/`weapons.cpp`/
>   `devices.cpp` and the binary-strafe `_absolute_position_mode` fold in `vbl.cpp` are **still the live
>   netgame behavior** — the wire block is captured/sent/drawn but does NOT yet drive movement, aim, fire
>   timing, or spread. That rewrite (full analog strafe, per-hand fire, positioned shots, MML spread
>   determinism, body-facing decouple) is Phase 2/3, to start once Phase 1b is confirmed.
> - **Debug lines render on BOTH the Quest AND the flat PC** (`render_net_vr_pose` is cross-platform under
>   `#ifdef HAVE_OPENGL`, called from the VR eye loop and the flat main view; gated on `OGL_IsActive()` +
>   `game_is_networked` + `vr_net_is_active()`, toggle `debug_show_net_vr_pose`). PC-renders-the-VR-player
>   is spec §5a's primary validation — watch the PC screen while moving on the Quest. Desktop compile
>   verified (LibAlephOne, Debug x64, 2026-07-19).
> - **Connectivity:** direct play only (Quest forces `use_remote_hub=false`). PC↔Quest same-LAN Gather+join;
>   Quest↔Quest host + port-forward UDP/TCP 4226 + join-by-IP.
>
> ---

Status: **Phase 1 + 1b implemented (2026-07-19)**; §2/§8 amended by the LIVE STATUS box above (preference-gated `kVR`, not an unconditional `kStar` bump). Decisions §0 and the wire format §4 stand; the "delete every `!game_is_networked` gate" work (§5/§6) is Phase 2, still pending debug-line verification.

## 0. Decisions locked
1. **Body facing is decoupled from the head.** The avatar's body yaw is driven by the **turn stick** (snap/smooth), not raw head yaw. Head and hands move freely without re-orienting the body. This fixes the "avatar twitches when I tilt my head" problem.
2. **Physical pose (lean / crouch / room-scale step) is view + shot-origin only — NO hitbox/collision effect** (for now). You cannot physically dodge or take cover; your collision capsule stays at the standard standing position.
3. **VR weapon spread reduction is read from synced scenario MML**, never from a per-player local preference (it feeds `global_random()`, so it must be identical on every client).

## 1. Goal & core principle
Make the VR experience **byte-identical in single-player and netgame** — full analog strafe, per-hand aim, independent dual-pistol fire, physically-positioned shots — with zero behavioral gating.

**Principle:** anything that affects the simulation must come from (a) synced per-tick wire data, (b) synced scenario/config data, or (c) a fixed protocol rule. **Never** from a local preference or a live device read gated to the local player. Every current desync is a violation of this; every fix below restores it.

Aleph One is deterministic lockstep — every client re-simulates every player from the shared per-tick stream. So the fix is not "send corrections," it's "put the VR inputs into the stream so all clients simulate identically."

## 1a. Connecting for a direct test (no server) — IMPORTANT
The stock default is `use_remote_hub = true` ("Use Dedicated Server", `preferences.cpp:4740`): hosting routes through a community relay server discovered via the metaserver. If that infra is unreachable you get *"Impossible to establish a connection with an available remote server…"* — which is what happened on the first attempt. Direct/LAN play needs **no** server, but it is **not** the default; you must turn the relay off.
- **Quest:** forced off in the build via `vr_force_direct_networking()` (preferences.cpp, called from shell.cpp next to `vr_force_optimal_sound`) — sets `use_remote_hub = false` and `advertise_on_metaserver = false` every launch. So the headset always gathers as its own local hub. No action needed on the Quest.
- **PC:** uncheck **"Use Dedicated Server"** in the network game setup (it defaults on on desktop too).
- **To connect:** one machine **Gathers** (hosts → becomes the local hub); the other **Joins**. Same LAN → the gathered game appears in the join list via SSLP discovery. Different networks → the joiner uses **Join by Address** with the host's IP, and the host port-forwards **UDP 4226** (`DEFAULT_GAME_PORT`).
- This is orthogonal to the VR protocol work — a pre-existing config default, not a netcode bug.

## 2. Compatibility & enforcement
- **No new infrastructure.** Verified: the version handshake is pure peer-to-peer (protocol-ID string + capability-map exchange, `network.cpp`); direct games use LAN discovery (SSLP) or direct-IP join with the gatherer's own machine as hub; the metaserver is optional discovery only. Two peers on LAN or direct IP need nothing external.
- **Enforcement is already built.** The gatherer's capability check (`network.cpp:398–476`) refuses joiners whose capabilities are missing/older and sends the standard *"The gatherer is using a newer version… you will not appear in the list of available players."* message.
- **Mechanism:** bump **`Capabilities::kStarVersion`** (6 → 7) in `network_capabilities.{h,cpp}`. A stock/old client (kStar ≤ 6) is auto-refused. This IS the "block PC players until they update" behavior Daniel asked for — no fallback path (all participants must run the VR-aware build).

## 3. The VR model (post-decoupling)
- **Body facing (`player->facing`)** — driven by the **turn stick**, carried on the **existing** `_absolute_yaw_mode` action-flag path (unchanged wire; only the input source changes from head to stick). Stable; drives movement reference + avatar orientation.
- **Head orientation** — free look. Sent in the new VR block. **Cosmetic only** (own view is rendered locally; other clients use it for pose visualization). Does NOT drive facing or aim.
- **Hands (dominant + off-hand aim)** — sent in the VR block. **Sim-critical**: each weapon fires along its hand's aim.
- **Locomotion** — left stick move is **relative to body facing** (classic twin-stick VR comfort model): turn with the stick, strafe/advance relative to the body, look and aim freely.
- **`player->elevation`** — see Open Points (§9); not sim-critical once aim is hand-driven.

## 4. Per-tick VR wire block (per VR player)
Appended to that player's per-tick data in the star stream, present only when the player advertised VR. Quantized; bandwidth is a non-issue (~12 bytes/tick × 30 tps ≈ 360 B/s per VR player).

| Field | Bits (approx) | Class | Used for |
|---|---|---|---|
| forward analog | 8 signed | sim | forward/back velocity (retires the `_absolute_position_mode` hack) |
| strafe analog | 8 signed | sim | perpendicular velocity — **real analog strafe** |
| dominant aim yaw | 9 | sim | primary-weapon fire direction |
| dominant aim pitch | 8 signed | sim | primary-weapon fire elevation |
| off-hand aim yaw | 9 | sim | secondary / off-hand fire direction |
| off-hand aim pitch | 8 signed | sim | secondary fire elevation |
| lean offset x | 8 signed | sim | shot origin (physical lean) |
| lean offset y | 8 signed | sim | shot origin (physical lean) |
| eye-Z (crouch) | 8 | sim | shot origin height + crouch pose viz |
| head yaw | 8 | cosmetic | pose viz (torso/debug line) |
| head pitch | 8 signed | cosmetic | pose viz |

Exact bit packing + a bandwidth/precision note finalized at implementation. Angles use Marathon's 512-unit circle; quantization must be deterministic (same rounding on encode/decode).

## 5. Unified simulation path (removes all gating)
Introduce a per-player `vr_input` struct holding the decoded block. It is populated:
- for **your own** player: from live controller/HMD reads (as today), and also encoded into the outgoing stream;
- for **remote** players: from the received wire block;
- in **single-player**: only the local fill exists.

Every VR-affected sim computation reads from `vr_input[player]` — **the same code path in SP and netgame**. The `!game_is_networked` branches disappear. SP and netgame diverge only in *where the struct is filled from*, never in behavior.

## 5a. Mixed VR / non-VR players (and the PC↔VR test rig)
A client built from this repo is **VR-aware regardless of hardware** — a flat PC build has the full VR simulation + serialization code, it just has no headset. So:
- **PC (non-VR) client:** consumes every VR player's block and simulates them correctly (hand aim, lean, etc.); emits only **default/trivial** VR data for itself (aim = its `facing`/`elevation`, lean = 0, eye-Z = 0). Its own shots/movement use the classic path.
- **VR client:** consumes and emits real blocks.
- A per-player **`is_vr` flag** is set at join and carried in the synced topology, so every client knows which players expect VR behavior. Sim branches **per player**: `if (player.is_vr) use vr_input[player] else classic facing/action_flags`. This is per-player capability branching (correct + necessary for mixed games), NOT the `!game_is_networked` per-game gating we're deleting — a given player behaves identically in SP and netgame.
- **Primary validation:** Daniel connects his flat **PC build** to his **VR build**. The PC screen renders the debug orientation lines (§7) for the VR player straight from the received block — if they track the VR player's real head/gun, the wire is correct. This is why Phase 1 ships the pipe + debug lines before any behavior.

Phase-1 simplification: the block may be sent for **all** players (PC fills defaults) to avoid conditional payload presence while proving the pipe; Phase 2 can make it VR-only-presence once the `is_vr` flag drives it.

## 6. Behavior-by-behavior
- **Movement (A1/A2):** forward + strafe velocity set from `vr_input` analog values on all clients. Full analog both axes. Delete the local `VR_GetAnalogMove` scaling in `physics.cpp`, the `_absolute_position_mode` encoding in `vbl.cpp`, and the `vr_debug_force_wire_movement` toggle.
- **Per-hand aim / bullet angle (B1):** `calculate_weapon_origin_and_vector` uses `vr_input[player]` dominant/off-hand aim for the fire vector — no `local_player_index`/`!game_is_networked` gate. Two pistols fire in two directions correctly on every client.
- **Dual-pistol fire timing (B2):** remove the `!game_is_networked` on the stagger bypass (`weapons.cpp:2567`) — with all clients VR-aware and triggers already synced via `action_flags`, independent per-hand firing is deterministic. The compromise is gone.
- **Bullet origin (B3):** shot origin = `camera_location` + `vr_input` lean(x,y) + eye-Z, on all clients. Shots emit from your real physical position. (Hitbox unchanged per Decision 2.)
- **Spread (B4):** `VR_GetWeaponSpreadScale` replaced by a value read from **synced scenario MML**, applied identically on all clients → no RNG-stream desync. Remove the `!game_is_networked` gate.
- **Fist/velocity punch (B5):** unchanged — detection stays local (produces a synced trigger flag), damage uses synced velocity, direction reuses B1.

## 7. Verification tooling (build these EARLY — invisible protocol needs visible proof)
- **Debug orientation lines (temporary VR pref).** For each network player, draw world-space lines from their position along their **head** direction and each **gun** aim, straight from the received `vr_input`. This is the primary correctness check — you can literally see whether remote pose/aim matches reality. Gated behind a temporary `vr_debug_show_net_pose` pref.
- **Torso-follows-pose avatar.** If the player sprite renders torso and legs separably, offset the torso by the synced eye-Z (and lean) so a physically-crouching VR player visibly hunches on other screens. Crude, possibly goofy, but a great end-to-end proof the pose data survives the wire. (Confirm sprite structure supports it during implementation; fall back to the debug lines if not.)
- Keep the existing `vr_debug_force_wire_movement` toggle only until Phase 2 lands, then remove.

## 8. Implementation phases
1. **Pipe + proof.** `kStar` bump + capability enforcement; serialize/deserialize a *minimal* VR block with per-player storage; ship the **debug orientation lines**. Goal: prove data crosses before hanging behavior on it.
   - **Transport choice:** Phase 1 carries the block in the packet's existing **type-tagged message section** (new `kVRPoseMessageType`, handled by the `process_messages` loop that runs on both spoke and hub) — deliberately **clear of the intricate tick-flag queues**, so there's near-zero risk to existing netplay. It's "latest pose per player," which is all the cosmetic debug lines need. Phase 2 moves sim-critical data to the **tick-aligned** flag-parallel path (a block per player per tick, consumed deterministically with that tick's flags) — required because a shot at tick T must use the aim at tick T identically on every client.
   - **Landed:** `vr_net.{h,cpp}` (block + BE serialization); `kStarVersion` 6→7 (old clients auto-refused); `kVRPoseMessageType`; `vr_capture_local_net_block` (vbl.cpp); desktop build fixed (vr_openxr_win.cpp `VR_RenderLoadingFrame` stub, `vr_net.cpp` added to LibAlephOne.vcxproj, OGL_Render `a1ffStaticMode` guarded). **Full data path wired + Android-deployed:** spoke `send_packet` emits `[kVRPose][localIndex][block]`; hub `process_messages` stores per-sender + `send_packets` relays every player's block (space-guarded); spoke `handle_vr_pose_message` stores per-player + `vr_net_get_player_block` accessor + a throttled `logWarning("VRNET rx ...")` proof. **Both peers must be built from this branch** (a client without the handler hits the `process_messages` default case, doesn't consume the message payload → stream desync).
   - **Remaining (Phase 1b):** the visual debug orientation lines (render hook) behind a temp `vr_debug_show_net_pose` pref. Until then, the `VRNET rx` log is the pipe proof.
2. **Sim consumers.** Wire the full block; route all VR shot/movement math through `vr_input`; delete every `!game_is_networked` gate and the movement hack/toggle. Full analog strafe + per-hand aim + independent dual fire + positioned shots.
3. **Decouple + pose.** Switch body facing to stick-turn; head becomes cosmetic; add torso-follows-pose avatar. Fixes the head twitch.
4. **Polish.** MML spread wiring, remove temporary debug prefs/toggles, finalize bit packing, device + 2-machine netplay test.

## 9. Open / deferred points
- **`player->elevation` source.** Not sim-critical once aim is hand-driven. Default: leave on the existing head-pitch `_absolute_pitch` path for now (least change); revisit if vertical avatar twitch is objectionable (could neutralize, or drive from dominant-hand pitch).
- **Sprite torso/leg separability** — verify before committing to the torso-crouch viz.
- **Physical-pose-affects-hitbox** — explicitly deferred (Decision 2). Revisit later if physical dodging/cover is wanted; it's a larger sim change (variable capsule position/height + abuse surface).
- **Bit packing / bandwidth-vs-precision** — finalize in Phase 1.
