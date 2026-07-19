# VR Netcode — 2-Machine Test Plan (Phase 1b verification)

Goal: confirm the VR pose wire is delivering correct data **before** any Phase-2 sim behavior is built on
it. You watch the debug lines (and the log) prove that each player's head + gun aim crosses the wire.

## What you're verifying
For every *other* player, colored world-space lines are drawn from their eye:
- **green** = head view direction
- **red** = dominant-hand gun aim
- **blue** = off-hand gun aim
- **yellow tick** = eye position (drops when they physically crouch)

Pass = a **VR** player's lines track their real head/guns; a **flat PC** player's red/blue rays point
straight along their facing (graceful fallback, no hand aim), and both move as that player moves.

## Setup (direct play, no server)
The Quest already forces direct play (`use_remote_hub=false`). On the **PC**, in the network game setup,
make sure **"Use Dedicated Server" is OFF**.

Decide who hosts — the **host's** `use_vr_netcode` preference decides the game mode (Quest defaults ON,
PC defaults OFF; PC toggle is Network prefs → lobby tab → "Host with VR Netcode Extension").

- **Quest hosts (recommended):** it defaults to VR-on. On the Quest: start a network game and **Gather**.
  On the PC: **Join by Address** using the Quest's IP (same LAN → it may also appear in the join list).
- **PC hosts:** turn the pref ON on the PC first, then Gather; Quest joins by the PC's IP.

Ports if not same-LAN: forward **UDP + TCP 4226** on the host.

## Confirm the protocol activated (log)
Both machines log once at game start (grep `VRNET`):
- On device: `adb shell cat '/sdcard/Android/data/org.alephone/files/Aleph One Log.txt'` → look for
  `VRNET StarGameProtocol::Sync: VR netcode ACTIVE (isServer=1 ...)` on the host, `ACTIVE (isServer=0 ...)`
  on the joiner.
- PC: `Aleph One Log.txt` in the app's log dir → same lines.
- If a machine says **inactive** while the host says ACTIVE → the capability handshake didn't agree
  (investigate `kVR` advertise/capture in network.cpp).
- Also expect throttled `VRNET rx player N: dom_yaw=… lean=… eyeZ=…` on the receiver (~1/sec) — nonzero,
  changing values as the remote player moves = the block is live.

## The actual check
- **PC watching Quest** (primary): with the PC as a spectator/second player, look at the Quest player's
  green/red/blue lines. Move your head and point the controllers on the Quest — the lines on the PC
  screen should follow in real time. Physically crouch → the yellow eye tick + line origins drop.
- **Quest watching PC:** the PC player's rays should point along wherever the PC is facing/aiming.
- **Quest ↔ Quest:** each sees the other's real head/gun pose.

## Expected fallback / negative checks
- Host with the pref **OFF** → no lines, no `VRNET` activation log, and a **stock/old** client can still
  join (legacy game). This is the compatibility path.
- Host with pref **ON** → a stock/old build is refused at join ("hosting with the VR netcode extension…").

## If the lines are wrong (triage)
- **No lines at all:** protocol not active (check the log), or not an OpenGL renderer on PC, or
  `debug_show_net_vr_pose` was turned off. Also confirm >1 player actually in the game.
- **Lines at the wrong position but right direction:** `camera_location` vs eye mismatch — the origin is
  the simulated eye; lean/eye-Z are added from the block (already world units).
- **Direction wrong / mirrored:** angle→direction convention in `render_net_vr_pose` (Marathon 512-circle;
  yaw uses `cos/sin`, pitch is signed) or the capture quantization in `vr_capture_local_net_block`.
- **Right for VR player, flat for PC player:** that's **correct** — the PC emits default (facing) aim.

## Phase 2 stage-1 test — BULLETS (tick-aligned channel)
Once the debug lines are trusted, this verifies the *simulation* now reads the synced block (per-hand
aim, shot origin, dual fire) identically on every client — the real anti-desync goal.

Setup is the same (host with "VR Netcode Extension" ON). Then, with a PC + Quest (or two Quests):
- **Aim:** on the Quest, point a controller well off from where your head/body faces and fire. On the
  **PC screen**, the tracer/impact should come from the hand's aim direction (matching the red/blue debug
  ray), NOT the body facing. Both machines must show the bullet hitting the same spot.
- **Dual pistols:** hold two pistols, aim them in two different directions, fire both. Each should shoot
  along its own hand on every client, firing independently (no alternating stagger).
- **Shot origin:** lean/duck and fire — the shot should originate from your physical (leaned) head
  position on the other screen, and be blocked if you lean into a wall (origin is wall-clamped).
- **Flat PC player:** their shots go straight along their facing (graceful fallback) on every client.
- **Desync watch:** play a few minutes with shooting. No "players in different spots" / rubber-banding.
  If anything desyncs, toggle "VR Netcode Extension" **OFF** on the host → legacy path (known-good).

Confirm activation first via the `VRNET StarGameProtocol::Sync: VR netcode ACTIVE` log on both machines.

## After bullets pass — Phase 2 stage-2
The remaining consumers read the SAME synced block, so they're quick follow-ons: full analog strafe,
underwater-crouch oxygen for all participants, MML weapon spread determinism, body-facing decouple.
Then delete the `!game_is_networked` gates and the binary-strafe fold. Until then those stay on the
current netgame behavior.
