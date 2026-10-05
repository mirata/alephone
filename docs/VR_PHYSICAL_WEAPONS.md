# VR Physical Weapons (experimental mode)

Onward-style physical weapon handling for the Quest VR port. Your whole loadout lives
on your body; you grab weapons off it with the grip, throw them with real physics,
catch them mid-air, and reload by physically seating magazines. Weapon cycling is
gone — **your body is the weapon selector.**

Status: **partly implemented and on device** -- see section 0 for the live state. Decisions signed off 2026-09-26.
See `VR_PORT_PLAN.md` for port state, `VR_NETCODE.md` for the wire protocol this mode
deliberately stays out of for now.

---

## 0. Living status and backlog

**This section is the working list.** Daniel's feedback gets logged here as it arrives, so it survives
between sessions without either of us re-reading the whole history. Newest round at the top of each
list. Everything below it is the original design and is partly out of date -- where they disagree, this
section wins.

### One switch

Everything is behind a single preference: **VR CONTROLS -> "VR Weapon Handling (experimental)"**
(`experimentalWeapons`, XML `physical_weapons`), tested only through `VR_PhysicalWeaponsActive()`, which
also forces it off in netgames and films. The attach-point gizmos + in-headset tuner are a CODE-ONLY
switch (`showAttachGizmos` default in vr_openxr.cpp) and additionally require the main option.

Gating audit (2026-09-27): every engine hook in weapons.cpp checks `VR_PhysicalWeaponsActive()` directly or
via `VR_WeaponNeedsManualReload()` / `vr_get_offhand_weapon()`; vr_openxr.cpp input changes sit in
physical-only branches; every render.cpp pass returns early without it. Found and fixed one gap: weapon
cycling had never been disabled in this mode.

### Built and on device (awaiting feel feedback)

- Holsters on the body, following a torso yaw derived from the head (35 deg leash), per-slot hang.
- Grip a holster to draw; the grabbing hand owns the gun (aim, trigger, haptics). Release = back on the body.
- True second-hand weapon (different weapon each hand); HUD shows the primary hand's weapon.
- Dual magnums from both hips; releasing either hand puts that pistol back.
- Fists always shown on both hands.
- Ammo: primary pouch (amber) + secondary pouch (red, rifle grenades), off-hand side. Pouches show what
  they are for even when you are out (drawn dark).
- Magazines carry their rounds. Mag release is a HOLD: loosens the mag; while held it can be pulled or
  flicked (downward snap, or a wrist roll that slings it off sideways); release with the mag still in
  and it clicks back home. New mags only seat once the old one is out. Magwell ring: red / yellow / green.
- Thrown mags keep hand velocity, land at your feet.
- No auto-reload, dry-fire click once per pull, no auto-switch, empty guns can be picked up.
- Two-handing only at the weapon's grab spot. Rifle: support trigger fires grenades while two-handed.
  Fusion pistol: tap fires, hold charges.
- Shotgun: upward wrist-flick reload (replaced the 270 deg spin, 2026-09-27).
- Start empty-handed on new game / load / respawn.
- Y and B are forced to magazine release in this mode (normal bindings untouched when it is off).
- Exit button opens a weapon-mode menu: HOLSTER & POUCH SETUP / QUIT GAME / CANCEL. In setup every anchor
  shows as a box; grip one and drag it. Leaving setup writes all anchors to the log as MML
  (`adb logcat -d -s A1VR | Select-String MML`), converted back to right-handed reference values.
- A pulled magazine has to leave the magwell before it can be seated again (fixes instant re-seat).
- Pouch magazines are billboarded toward the head.
- Round 11: setup mode names say the PHYSICAL side (slots are authored right-handed and mirrored for
  lefties -- that is a naming issue, not the early facing bug, which was the room-yaw one); magazines on
  the floor / in pouches depth-test (the quad had depth OFF, so they drew over the guns); shotgun slots
  moved outboard of the pistols; shotgun reload is now an UPWARD WRIST FLICK (>250 deg/s muzzle-up,
  35 deg); both fists are always drawn by the hand-visuals rule (the engine's empty-handed fists came
  out reversed and are skipped in this mode).
- Round 12: fists mirror by (hand==right) XOR the scenario <left_handed_weapon fist> flag -- the same
  fist.md3 ships in all three scenarios; only M1 sets the flag. M2 confirmed on device, M1 UNVERIFIED; shotgun flick-reload runs the
  engine's own reload SEQUENCE (reload_weapon, authorised by s_vr_reload_authorised) so its reload
  animation plays; back slots grab from anywhere above their anchor within 40 cm horizontally.
- Round 15: assault rifle has a SECOND magwell (VR_ATTACH_MAGWELL2, MML <vr_magwell2>, default under the
  front of the barrel): grenade mags seat there, never in the main well; nothing ever comes out of it.
- OPEN BUG: SMG + shotgun (second hand) -- flicking the shotgun appeared to reload but it would not fire.
  Not reproduced from code. DIAGNOSTIC logging added (VRRELOAD in the on-device log: flick detection,
  the service's live triggers/state/rounds, reload_weapon result, and each dry-fire's trigger). Next
  repro -> read the log before changing anything.
- Round 14: holster setup PERSISTS (per-slot player overrides kept apart from the MML defaults -- which a
  game load resets -- saved as <holster_anchor> children of <vr> in the preferences; written on leaving
  setup). Menu gains RESET HOLSTERS TO DEFAULT. Holsters have ROTATION: stored as barrel/top directions in
  body space (fx fy fz / ux uy uz, right-handed, mirrored for lefties). In setup a grabbed box follows the
  hand's full pose (position + rotation); boxes are drawn oriented, with the weapon hanging in them.
- Round 13: reload requests carry a TRIGGER mask. A dual pair is one weapon with a trigger per hand
  (trigger 0 = primary hand), so a flick names its own trigger and only that shotgun reloads/animates.
  Same-tick requests merge (both hands flicking reloads both).

### Open -- Daniel's list (add new items here)

- Verify M1 fists (needs `deploy.ps1 -Scenario "Marathon"`; the device currently has M2 data).
- NEXT: Daniel positions holsters/pouches in setup mode -> I read them and make them the defaults.
  Source: his preferences (`adb shell run-as org.alephone cat "files/Marathon 2 Preferences"`, the
  `<holster_anchor>` children of `<vr>`) or the MML dumped to the log on leaving setup. Both are already
  right-handed reference values (he is a lefty -- no manual mirroring). Copy position into
  `kDefaultHolsters` and the hang (fx..uz) into `default_holster_orient` (vr_weapons.cpp).
- Give-all-weapons cheat RE-ENABLED for setup (player.cpp) -- re-comment before release.
- Grenade pouch was unreachable (clashed with the pistols, too far): moved closer/central 2026-09-27,
  final placement via setup mode.
- 2026-09-27 batch -- all built and deployed (Infinity data), none yet felt in-headset:
  - [x] Shotgun flick straight after a shot: one pull empties it, the flick was landing during the
        firing/recovery state and being dropped. Now held until the gun is ready (up to ~0.6 s).
  - [x] Daniel's setup baked in as defaults (thighs, chest pair, front centre), pairs averaged as mirror
        images, hang vectors squared up. HE MUST PRESS 'RESET HOLSTERS TO DEFAULT' to see them -- his saved
        overrides (the pre-symmetry versions) still take precedence until cleared.
  - [x] AR grenades: holding mag release loosens BOTH wells; grenades are pulled out of the second well
        (orange ring + dangling grenade), never flicked. A new grenade mag needs the old one out.
  - [x] AR steadying grab spot moved forward and enlarged (z 0.32, radius 0.18).
  - [x] Explicit editable entries in every scenario's vr_weapons.mml: magwells for all magazine weapons,
        the rifle's grenade well, foregrips for the two-handed guns.
  - [x] Rocket launcher: <vr_direct_load> -- no removal, reloads straight in once empty.
  - [x] Magazine SLIDE PATH: <vr_magwell ... travel=> -- a magazine engages at the entry point and slides
        on the line to the seat as you push (pulled back / yanked off the line it comes away); a loose one
        is slid OUT along the same line and is free only past the entry. Seeded on pistols, SMG, rifle.
  - [x] TEMPORARY: magazines drawn as an oriented wireframe box (3.5 x 13 x 6 cm) instead of the sprite,
        to make positioning readable -- cyan in hand, green on a well's path, orange loose, grey dropped;
        drawn through the gun. Switch: s_vrMagAsBox in render.cpp. Pouches keep their sprites. Box size is
        one global default for now (per-weapon size would go on <vr_mag_model>).
  - [ ] 2026-09-28 "slide ammo does not work" -> reworked, awaiting device test:
        - the mag is held by its base, sticking UP out of the fist (top 10 cm above the grip; kMagInFistM).
        - contact is measured from the mag's TOP to the mouth (path entry, else seat), not the hand; the mag
          axis must be within max(angle_tolerance, 55) deg of the insertion line, then it snaps into line.
          (Old bug: the hand position drove capture and t, so the drawn mag and the logic disagreed.)
        - on the path the slide follows the hand's motion ALONG the line relative to where it engaged
          (no jump); comes away if pulled 40% of travel back past the mouth or dragged 10 cm off the line.
        - releasing grip while green (touching) or while on the path: the well takes it and the mag runs
          home in 0.10 s (kAutoSeatSeconds), then loads. Releasing anywhere else still throws it.
        - Daniel: "much better".
  - [ ] 2026-09-28 per-well magazine size/shape/angle -> done, awaiting device test:
        - <vr_magwell>/<vr_magwell2> take mag_width / mag_length / mag_depth (m) and mag_round="1" (cylinder,
          diameter = width). The box is drawn squared to the (tilted) insertion line and turned by roll.
          Hand/dropped mags look their size up from the held gun's well for that ammo, else the first weapon
          that uses it. Contact (mag top) uses the well's length.
        - Seeded: pistol 2.5x12x3.5 pitch -15 (raked grip; NEGATIVE pitch = leans forward going in), fusion
          3x8x5 pitch -15, AR 4x20x9, AR grenades round 5.5 dia x 7, SMG 3x16x5, flamethrower round 7x14,
          rocket round 8x25. All guesses.
        - Loose mags are grabbed at the middle of their body (reach >= 0.6 x length).
  - [ ] 2026-09-28 bug: pulling a mag out, moving the hand away before it cleared the slide made it "snap
        away" -> fixed, awaiting test. A mag can no longer skip the slide: coming out it runs along the
        path no faster than kSlideOutSeconds (0.12 s full travel) whatever the hand does, then swings into
        the hand over kHandoffSeconds (0.15 s, stored hand-relative) instead of jumping. Going in, once
        any of it is inside (t > 0.02) pulling back turns into a removal (slides out); only at the mouth
        can it be pulled away / dragged off the line (also with the swing).
  - [ ] 2026-09-28 MAGWELL TUNING mode -> built, awaiting device test. Menu: MAGWELL TUNING / EXIT MAGWELL
        TUNING and RESET MAGWELLS TO DEFAULT. The mode persists (pref magwell_tune_mode). While on:
        locomotion/turn/fire/mapped actions frozen (vbl.cpp kbActive); mag release still works so a tuned
        well can be tried at once. Held gun (primary hand) only.
        - left stick X = forward/back along the gun (right = toward muzzle), Y = up/down; left click = next
          spot (main magwell -> second well if the weapon has distinct secondary ammo -> two-handed grab
          spot unless no_two_handed). Spin-reload weapons have no wells.
        - right stick pages (right click cycles): SIZE (X width, Y length), DEPTH+ANGLE (Y depth, X pitch),
          PATH+REACH (Y slide travel, X capture radius). Grab spot: Y reach radius, X sideways.
        - rates 6 cm/s, 30 deg/s at full stick, squared with a 0.15 deadzone. HUD line via screen_printf.
        - drawn: seated mag box + path + capture ring per well, sphere for the grab spot; selected =
          pulsing magenta, others dim.
        - edits = player overrides (s_attachOverrides, win in VR_GetAttachPoint, survive level loads),
          saved as <attach_override> in prefs 1.5 s after the last change, dumped as MML to the log
          (VRATTACH) on leaving the mode -> bake into vr_weapons.mml like the holsters.
  - [ ] 2026-09-28 drum magazines -> built, awaiting test: new mag_drum="1" = a cylinder lying ACROSS the
        gun (axis = gun right), mag_width thick, mag_length tall x mag_depth fore-aft. Rifle grenades
        5 x 9 x 9 cm drum, SMG 4.5 x 13 x 13 cm drum. mag_round (upright) stays for flamethrower/rocket.
        Shape always comes from the MML (a tuning override never changes it).
  - [ ] 2026-09-28 round (awaiting test):
        - drum turned to FACE FRONT: axis = gun forward; mag_depth = thickness, mag_length tall, mag_width
          across. Grenades 9 x 9 x 5 cm, SMG 13 x 13 x 4.5 cm.
        - frame-rate: tuner no longer prints while a stick moves (prints once on release / selection) and
          no longer autosaves mid-tune (saves on leaving the mode or quitting from the menu). VRRELOAD
          diagnostic logs gated off (kVRReloadDiag = false in weapons.cpp + render.cpp).
        - mag release: the loosened mag slides from the seat down to its loose position over 90 ms
          (not if it was left loose by a hand letting go mid-pull: s_vrLooseNoDrop).
        - holster <-> hand swing (render only, kWeaponSwingMs 110 ms smoothstep): each end snapshots where it
          last drew each weapon per frame (s_holsterSnap[slot], s_handSnap[hand][weapon]); a weapon newly
          appearing at one end blends in from the other end's snapshot if that end dropped it within 3
          frames. Live pose (magwells/aim) is untouched. Known: a left-hand model is drawn mirrored, the
          holstered one is not, so the mirror still pops at the swap.
  - [ ] 2026-09-28 holstered weapons mirrored like the hand that holds them (hand 0 = left mirrored
        unless <left_handed_weapon>, XOR the shape's X-mirror bit). Hand = the one that last put it back
        (s_holsterLastHand), else the body side it hangs on. Cross-draw with the other hand still pops.
  - [ ] 2026-09-28 holster visual overload -> ghosts + reveal + highlight, awaiting device test
        (Daniel picked options 1+2+3 + ghost style):
        - every holstered weapon is a see-through cool-tinted GHOST (alpha kHolsterGhostAlpha 0.22, no
          depth write) until revealed; solid when revealed. Setup mode: all solid.
        - reveal by reach: any hand fades a slot in from 15 cm outside its grab radius to solid at it
          (same distance rule as grabbing, incl. over-the-shoulder back slots).
        - reveal by looking down: head pitch 25 -> 35 deg down fades everything in.
        - highlight: the slot a FREE hand's grip would draw (shared vr_nearest_holster_slot, minus nearer
          steadying grip / pouch, as the grab) glows cyan and is solid; entering it = 12 ms tick on that hand.
        - swinging weapons draw solid. Fade smoothing ~70 ms. Tint via new OGL_SetVRWeaponModelTint
          (colour = shade*mul + add, alpha); ghost forces GL blend on in the morph path.
        - Sprite fallback (weapons without a model) is not ghosted.
  - [x] 2026-09-28 Daniel's tuned magwells / grab spots / mag sizes baked into Marathon 2 + Infinity
        vr_weapons.mml (from the VRATTACH dump): pistol, fusion, AR main + grenade drum, rocket (magwell
        pitch 80 + foregrip), SMG drum. Pouch positions (+ orientation) baked into kDefaultHolsters (shared,
        so M1 gets them too). M1's weapons still to tune (Daniel will do it).
  - [ ] 2026-09-28 magazine approach assist, awaiting test (render only): approaching a well that will take
        it, the in-hand mag eases toward the insertion line + the well's orientation from 2.2x the capture
        radius, up to 85% at 0.6x (vr_mag_assist); on engaging it settles from its last drawn pose onto
        the path over kEngageSettleMs 80 ms. Back-to-front holds aim for the nearer half-turn.
  - [ ] 2026-09-28 DUAL-WIELD RELOAD fixed, awaiting test. Root causes: (a) a magnum pair is one engine
        weapon, trigger 0 = primary hand, trigger 1 = other; letting go ALWAYS lowers trigger 1, so when the
        primary hand let go the gun left in hand took the other gun's rounds, and one gun of the pair could
        never be reloaded; (b) magazine state was keyed per weapon TYPE (one state for both magnums);
        (c) eject emptied every trigger of that ammo (both magnums, incl. the holstered one); (d) wells,
        pouches, flick-out and loose mags only ever looked at the PRIMARY hand's gun.
        Fix: vr_swap_pair_triggers (trigger data + mag copies trade places when the primary hand lets go of
        a pair); mag state/loose rounds keyed (weapon, copy, well), copy = the pair gun's trigger;
        vr_hand_gun(hand) = the gun in a hand; eject/reseat per CONTROLLER (VR_ActionHeldOnHand: X/Y left,
        A/B right; empty hand's button falls back to the other hand's gun) and only that gun's trigger;
        wells/pouches/assist/ring = the gun in the OTHER hand, whichever it is; magazine reload honours the
        trigger mask (a pair's mag loads only its own gun); flick-out + loose mags per gun hand.
  - [ ] 2026-09-28 holster OUTLINES (Daniel: ghosts were always opaque; asked for outline-until-hover).
        Ghost failures: the reach reveal used the over-the-shoulder rule (back weapons lit whenever a hand
        was raised) and resting hands sit at the hips; non-MD3 models may ignore alpha. Now: eye depth
        buffers are D24S8 (fallback to D24 if the FBO is incomplete; VR_EyeHasStencil), stencil cleared
        with the eye. Holstered weapon = outline only (OGL_RenderVRWeaponModelOutline: stencil-mark the
        model, then the model grown kOutlineWidthM 4 mm per axis about its bbox centre in flat colour
        (morph shader mode 3) where not marked). Outline brightness by plain distance (full at radius,
        zero at radius + 20 cm). SOLID only on hover (+ glow + tick), look-down, swing, setup. No stencil =>
        faint ghost fallback. Cost: 2 extra draws per holstered weapon.
  - [ ] 2026-09-28 round, awaiting test:
        - outlines also when looking down (look-down now only brightens outlines); the hovered weapon is
          fully opaque (no tint) with a wider brighter rim (kHoverOutlineWidthM 7 mm).
        - release "flashing": after letting go the engine keeps the gun as current weapon until the fist
          switch lands, so it drew in the hand AND on the holster and the swing never started. The hand
          now only draws a gun vr_hand_gun says it holds.
        - rifle mag (2 cm slide): mouth pull-off is now absolute (max(40% travel, 4 cm), was 8 mm); a
          removal pushed back in now seats at t >= 0.97 (jitter used to turn an insertion into a removal
          capped part-way, stuck); must-clear after pulling out is 1.1x radius (was 1.6x); approach angle
          floor 70 deg (was 55).
  - [ ] 2026-09-28: holstered weapons are now INVISIBLE unless hovered (solid + thin 3 mm bright rim) or
        looked down at (outlines only, 4 mm). Proximity no longer shows anything. Hips (pistols) moved in
        3 cm and back 2 cm (x 0.19, z 0.08); chest pair (shotguns) in 3 cm and back 2 cm (x 0.335, z -0.001).
        Needs RESET HOLSTERS TO DEFAULT if those slots were ever moved in setup.
  - [ ] 2026-09-28: hover snaps to full opacity (no fading anywhere); look-down outlines fainter + thinner
        (alpha 0.4, 2.5 mm). HOLSTER SETUP now shows each weapon as its outline (white + wider while being
        moved) instead of a debug box, and each pouch as a magazine-shaped box turned the way the pouch is
        set (white while moved; amber primary, red secondary). Slots with no weapon still get the old box.
        Pistol hips further in/back: x 0.175, z 0.06.
  - [x] 2026-09-28 Daniel's new holster setup baked into kDefaultHolsters EXACTLY as set (from the
        VRATTACH "holster setup" dump): hips, thighs, chest pair, both pouches, all oriented. Pairs are no
        longer mirror-symmetric (his choice this time). Backs + front centre unchanged.
  - [ ] 2026-09-28 dropped / flicked / thrown mags now tumble (render only): each has an orientation and a
        spin -- flick-out = the wrist roll about the barrel + end-over-end tumble; throw/drop from the hand
        = end-over-end about the horizontal axis across the throw (2 + 7 rad/s per m/s) + a little random
        wobble. Integrated per frame (handedness kept, room->world is a reflection); on touching the floor
        it lands flat on its broad side, long axis level. vr_spawn_falling_mag now takes top + sides + spin.
  - NOTE: deploy.ps1 installs the old APK even when the native build FAILS -- always check the build line.
  - [ ] 2026-09-28 round, awaiting test:
        - flick-out harder: out-of-well speed 2.4 m/s, wrist roll 900 deg/s (was 1.1 / 450; then 1.35 / 540, 1.7 / 680 -- both still too easy).
        - SHAKE: any forceful shake now ejects a loose main mag -- the gun hand's acceleration (velocity
          sampled ~30 ms apart): |a . ins| > 35 m/s^2 (jolt along the magazine, e.g. raise then snap down and
          stop) or |a| > 55 m/s^2 (any violent jolt). Speed and roll checks kept alongside.
        - new <vr_magwell drop_on_release="1">: only those magazines drop 4 cm out when the release is
          pressed; all others stay seated (loose, but where they are) until pulled or flicked out. Set for
          pistol + plasma_pistol in all three scenarios, and the M1 assault_rifle main well. Taken from the
          MML even over a tuning override (like the shape flags).
        - a removal only re-seats on a push-in after it has come at least 15% out (s_slideMinT), since a
          non-dropping mag starts its pull-out fully seated.
        - mag tumble toned down: toss spin 1 + 3 rad/s per m/s (was 2 + 7), wobble ~40%, only 40% of the
          wrist roll carries into a flicked mag, and air drag on the spin (-150%/s).
  - [x] 2026-09-28 Daniel's Marathon 1 magwell tuning baked into Marathon/Scripts/vr_weapons.mml (pistol,
        fusion, rifle main pitch -20.9 travel 11 cm drop_on_release, rifle grenade drum 20 cm x 17.6 cm).
        Shotgun + SMG entries removed from M1 (it has neither); SMG entries removed from M2.
  - [ ] 2026-09-28 drums/canisters, awaiting test: drum = mag_drum on the well (M1 + M2 rifle grenades,
        Infinity SMG + grenades). Round faces are always round: vr_mag_dims_of forces drum width = length
        (their average) and canister depth = width. Tuner SIZE page: drum -> Y = diameter (both), X =
        thickness; canister -> X = diameter, Y = length. Drum in the hand: first tried a quarter turn about
        the insertion axis (WRONG axis, Daniel); then a quarter turn about the hand's RIGHT with its top
        along +forward (right axis, but upside down); now the other way -- top along the hand's BACKWARD
        (-forward), face toward the hand's up -- still wrong. FINAL: MEASURED. VRMAGPOSE diagnostic logged the
        drum's axes in the hand frame while Daniel pulled the M1 grenade drum out with his natural grip
        (right hand, 2 pulls, consistent). Averaged + squared up: top (-0.884,0.121,0.462), right
        (-0.362,0.478,-0.800), face (0.320,0.870,0.375), top offset (-5.9,0.9,12.3) cm for a 20 cm drum
        (scaled by drum size). Left hand = x mirrored. kDrumTop/Right/Face/TopOffM in render.cpp; the
        insertion axis in the hand (contact test, angle check) uses the same hold. VRMAGPOSE logging still
        ON (kVRMagPoseDiag) -- turn off once confirmed.
  - [x] 2026-09-28 M1 fists were mirrored wrong: the fist flip honoured M1's <left_handed_weapon
        name="fist">, which describes the stock 2D sprite. Same fist.md3 everywhere, so the flip is now
        simply (hand == 1) in all scenarios.
  - [x] 2026-09-28 holster defaults are now PER SCENARIO where they differ: Daniel's Infinity setup (hips +
        thighs changed) written as <vr_holster_anchor> in the Infinity and M2 vr_weapons.mml (M2 without
        left_thigh, the SMG slot). kDefaultHolsters (C++) = the M1 layout; unlisted slots fall back to it.
        NOTE: in-headset setup overrides are saved GLOBALLY in the prefs and win over every scenario's
        defaults -- RESET HOLSTERS TO DEFAULT to see each scenario's own layout.
  - [ ] 2026-09-28 weapon-mode reload sound: a seated magazine whose trigger has no reloading_sound
        (fusion pistol, rifle grenades, rocket launcher) plays the magnum's reload sound instead
        (vr_fallback_reload_sound, both the fresh-mag and same-mag paths). Weapon-handling mode only.
  - [ ] 2026-10-04 MAGAZINE MESHES (first model: Daniel's new M2/Infinity assault rifle), awaiting test.
        Blender export: SomaZ id Tech 3 exporter, Surfaces = "From Objects" (one MD3 surface per object),
        local space OFF. Surfaces: assaultrifle (gun), assaultrifle_mag, assaultrifle_grenade,
        assaultrifle_bullet1..3. ("From Materials" merged everything into one surface; the legacy exporter
        merged then split at 999 verts with misleading names -- both unusable.)
        Engine: MD3_Loader keeps each surface's name + index slice (Model3D::MD3Surfaces);
        a1ffDrawMorphMeshRanges draws index slices; OGL_SetVRWeaponModelParts / OGL_SetVRWeaponModelMatrix /
        OGL_VRWeaponModelMatrix in OGL_Render. render.cpp: surfaces classified by suffix (_mag = well 0,
        _grenade/_mag2 = well 1, _bulletN = top rounds, else gun). The held / cached / holstered gun draws
        only parts that are IN (bullets by loaded rounds: >=3 all, then 2, 1, 0). Magazines on the path,
        in the hand, loose and falling are drawn from the gun model (frame 0) via vr_mag_part_matrix: gun
        model -> gun space -> SEATED magazine frame (vr_fill_well_geometry on a unit pose) -> the
        magazine's actual pose; falls back to the debug box when the model has no such part. Normal mode
        untouched (whole model, reload animation). Falling mags remember weapon/well/rounds/hand.
  - [x] 2026-10-04 magazine meshes: Daniel "works great". Approach assist turned OFF (kVRMagApproachAssist):
        with a real mesh the gradual alignment read as the mag sliding around in the hand. Now it stays as
        held until it engages the well, then snaps into line over kEngageSettleMs (80 ms).
  - [ ] 2026-10-04 "grenades grabbed out of the hand from far away": engage distance is now min(well radius,
        kEngageReachM 4 cm) from the mag top to the mouth (radius stays the ring's guide size), and the
        approach angle must be within max(tol, 70) capped at 100 deg -- wells set to "any angle" (180, e.g.
        rifle grenades) no longer accept a mag swung round from upside down. Awaiting test.
  - [ ] 2026-10-04 MAGAZINE SOUNDS, awaiting test. <vr_mag_sounds name=.. [well=0|1] release= remove= insert=>
        inside a <vr_weapons> block in a plugin MML (paths resolve relative to the plugin -- plugin MML is
        parsed with the plugin folder on the search path). First use: M2 "VR Models" plugin vr_models.mml,
        assault_rifle, sounds/mag-release|mag-remove|mag-insert.wav (16-bit mono 22 kHz). Decoded on first
        play (ExternalSoundHeader::LoadExternal), played 2D via OpenALManager. Weapon handling mode only.
        release = vr_eject_magazine (main well's sound, else the second's); remove = slid off the path end,
        pulled straight out (no path), shaken out; insert = seated (REPLACES the weapon's reloading_sound:
        put_rounds_into_weapon muted via s_vr_mute_reload_sound) and a loose mag clicking back home -- the
        latter ONLY for drop_on_release wells (a mag that never moved makes no sound when the button is let
        go; Daniel heard "insert" on every button release with the M2 rifle).
        Infinity's plugin does not have these yet (copy sounds/ + the block when its rifle model is updated).
        2026-10-04: M2 plugin also has plasma_pistol (all three) and pistol + smg (release + remove only --
        their seating keeps the weapon's own reload sound). M2 has no SMG; the line is there for Infinity.
  - [ ] 2026-10-04 GRIP-RELATIVE HOLD, awaiting test: rifle grenades are grabbed palm-up and the drum slid
        through the hand into the fixed (palm-down, top-anchored) drum hold. Now a magazine taken OUT OF A
        GUN keeps its exact pose relative to the hand at that moment (s_holdValid/Top/R/U/F, hand-local):
        captured where it leaves the path (pose at t = 0), or next frame after a straight pull from a
        no-path well (vr_capture_hold_later). The top position, insertion axis (contact test) and drawn pose
        all use it; the handoff swing becomes zero. Cleared when the hand lets go; pouch magazines use the
        default holds. Debug guides (target ring + line, orange loose ring) hidden for any magazine drawn as
        a real mesh (vr_mag_has_mesh) -- Daniel wants to see the rifle without them.
  - [ ] 2026-10-04: pouches show the gun's magazine MESH (standing in the pouch, pouch rotation, dark when
        empty) when the model has one; sprite otherwise. Thrown mags now spin with the hand's measured rotation
        rate (s_handOmega: frame-to-frame axis change, smoothed) + half the toss tumble -- the earlier
        toning-down had left throws with only a weak speed-based spin. Flick-outs unchanged.
  - [ ] 2026-10-04 "jerky when examining a gun up close": checked -- NOT the int16 bug (render camera =
        float interpolated body + live head offset; weapon = camera + hand offset, all float). Diagnostic ON:
        kVRJitterDiag, logcat tag A1VRJIT, one line per frame per hand (us clock, raw hand + head stage pos,
        render camera, weapon anchor + forward). Read after Daniel rotates a gun slowly up close.
  - [ ] 2026-10-04 JITTER = FRAME RATE. A1VRJIT trace: weapon placement tracks the controller exactly
        (relative motion ratio 1.000, p95 1.006) -- not precision, not our transform. But frames were 27.9 ms
        median (~36 fps). VrApi during play: FPS 35/120, GPU% 0.98-0.99, App(GPU) ~20 ms, CPU% ~0.18 =>
        GPU-BOUND (pause menu: 120 fps, App 4.5 ms). Prime suspect: the eye D24S8 stencil added for the
        outlines (packed depth+stencil under MSAA render-to-texture may not stay in Adreno tile memory).
        TEST BUILD: s_eyeStencil = false -> still GPU-bound (40 fps, App 17.5 ms): NOT the cause, stencil back
        ON. REAL CAUSE: Daniel's M2 prefs had VR Resolution Scale 2.0 (4x pixels) + 4x MSAA. Daniel's "jitters
        back and forth between 2 points" = hand-held object at 1/3 refresh: each frame shown 3x, head is
        reprojected but the gun's pose is a ~30 ms prediction, off by hand tremor alternately (~0.5 deg
        zig-zag, continuous steps -- no quantisation). Advice: Resolution Scale 1.0-1.3, MSAA 2x.
        CONFIRMED by Daniel after lowering the settings ("that seemed to improve it"). kVRJitterDiag and
        kVRMagPoseDiag both turned OFF.
  - [ ] 2026-10-04 drum hold anchored by its BOTTOM: Daniel's palm-up grenade pulls (VRMAGPOSE, 5 consistent)
        kept the measured drum angle but put the top a drum length + ~3.5 cm from the hand along its axis;
        the default hold had it ~7 cm (top-anchored). Now top = hand + kDrumTop * (length + kDrumPalmGapM).
  - [ ] 2026-10-04 round, awaiting test:
        - BUG pistol/fusion still fired with the mag out: vr_eject_magazine only zeroed rounds if the trigger
          was IDLE (eject mid-shot left them). Now always zeroed, AND vr_enforce_empty_wells (every tick, in
          the reload service, both passes): a trigger whose well (per pair copy) is not IN has 0 rounds.
        - PART-USED MAGAZINES: items[] counts whole mags; vr_weapons keeps per-ammo lists of part-used
          rounds (VR_Push/PopPartialMag). Pouches hand out FULL mags first, then a part-used one (which
          leaves the inventory at grab time and is carried like any mag with rounds).
        - DROPPED MAGS -> PICKUPS: a landed mag that would vanish (3 s) becomes its ammo item via new_item at
          its spot (find_new_object_polygon from the player); part-used ones registered by object index
          (VR_RegisterDroppedMag) and picked up part-used again (items.cpp get_item hook). Empty mags just
          vanish. A fresh pouch mag is taken from the inventory as it becomes the pickup (net zero).
        - PUT BACK IN A POUCH: letting go within 1.25x a pouch's radius when that pouch takes this ammo ->
          back into the inventory (vr_add_magazine_to_inventory, same max as pickups; a part-used one keeps
          its rounds; fresh never left; empty is gone). Inventory full -> it drops instead.
        - Daniel: the pickup "was there for a split second then vanished" -- the engine auto-takes any item
          within MAXIMUM_ARM_REACH (3/4 WU ~1.5 m), so a mag at your feet was picked straight back up. Now a
          landed mag WITH rounds waits as the magazine while the player is within 1.15x that reach and
          becomes the pickup once you move away; evicting the oldest of the 8 kept also converts it first.
        - Not saved: the part-used lists and dropped-mag rounds are not in save games (a part-used mag in
          inventory becomes a full one after a load). Ammo pickups stay sprites for now (Daniel's call).
  - [ ] MD3 PLAN (proposed 2026-09-28, not started) -- see "Models: split magazines and posed hands" below.
  - [ ] 2026-09-28 bug: holding mag release and gripping the grenades two-handed the rifle instead ->
        fixed, awaiting test: the steadying grip is suppressed while that hand holds a magazine or is
        within 1.25x reach of a loosened magazine (the grenade well sits by the AR grab spot).
  - Found + fixed on the way: setup-drag hand frame was not square (aim fwd includes the aim-pitch
    offset) -> saved hang vectors were 1.29/0.75 long; XML comments contained illegal double hyphens.
- *(Daniel has more items queued -- log them here as they come.)*

### Open -- known gaps and follow-ups

- Tune: flick thresholds (1.1 m/s out-speed, 450 deg/s roll), charge hold (250 ms), shotgun flick (250 deg/s over 35 deg), default
  grab spot and default magwell -- all first guesses.
- Place real magwell / foregrip points per weapon with the tuner (only the assault rifle has any).
- Grenades use the rifle's main magwell point; a separate launcher insertion point is a small follow-up.
- Dropped / flicked magazines are lost (no floor pickup).
- Dual-wield sprite routing still follows primary->holding hand; a mismatch would show as swapped pistols.
- Unverified: whether a level change also resets you to empty-handed.
- Holster orientation is per-slot in code; an MML attribute would let it be tuned.
- Throwing / catching weapons and the fusion cook-off: deprioritised 2026-09-26.
- Netplay fold: the mode is single-player only by design for now.

### Decisions changed since sign-off

- Alt-fire on stick clicks -> replaced by support-hand trigger (two-handed weapons with a second mode)
  and tap/hold (charge weapons).
- "Manual-only, never softlock, auto-reload fallback" -> NO auto-reload at all; unplaced weapons get a
  default magwell at the grip instead, so nothing softlocks.
- Eject button -> a HOLD that loosens rather than ejects.
- Two preferences -> one.

---

## 1. The interaction model

1. Every weapon you carry is **visible on your body** — pistols on the hips, long guns
   on the chest and over the shoulders. Fixed positions, so they become muscle memory.
2. Move a hand near one and squeeze **grip** to take it. Empty hand = fist.
3. **The hand that takes a weapon owns it** — that hand aims it and that hand trigger
   fires it, whichever hand is configured dominant.
4. **Let go and it returns to its holster.** No dropping, no throwing.
5. **Except** when the other grip is still closed: the weapon stays in play and the
   remaining hand becomes the pivot it hangs from, so a two-handed hold survives
   releasing the main handle and you can re-grip it.

Throwing, catching and ground pickup are **deprioritised** (2026-09-26). They pulled in a
thrown-projectile type, suppressing `swipe_nearby_items()`, mid-air catch tests against
30 Hz int16 projectile positions, and world items that can be lost in geometry — a lot of
machinery for a mechanic that release-returns-to-holster covers more cheaply. The design
notes for them are kept in §8 in case they come back.

Signed-off decisions:

| Question | Decision |
|---|---|
| Per-hand weapons | **True per-hand.** Two independent weapon state machines. |
| Grip | **Hold-to-grip** by default, with a `gripMode` pref for toggle. |
| Reload | **Manual-only, never softlock.** Auto-reload falls back when a weapon has no magwell authored yet. |
| Scope | **Single-player first**, behind `experimentalWeapons`. Forced off in netgames and films. |
| Weapon selection | **All carried weapons on the body. Next/prev weapon cycling disabled.** |
| Alt-fire | **Physical stick click**, per hand. Map moves to a face button. |
| Throw damage | **Light damage / stagger**, MML-tunable per weapon. |
| Charged fusion throw | **Allowed — it cooks off as a grenade where it lands.** |

The SP-only gate is what makes true per-hand affordable: the experimental path does not
have to preserve netgame, film or savegame compatibility.

### Preferences and the single-player gate

Two prefs, both in `vr_settings_t` and persisted in the `<vr>` block:

| Pref | XML attr | UI | Meaning |
|---|---|---|---|
| `experimentalWeapons` | `physical_weapons` | VR CONTROLS -> Physical Weapons -> "Enable" | Master switch for the whole mode. Off by default. |
| `showAttachGizmos` | `attach_gizmos` | VR CONTROLS -> Physical Weapons -> "Attach Point Gizmos" | Phase 0 authoring overlay. Independent of the master switch, so attach points can be placed before any behaviour exists. |

**Never test `experimentalWeapons` directly.** Every physical-weapons path calls
`VR_PhysicalWeaponsActive()`, which is the pref AND a session check: off in netgames
(`game_is_networked`) and off during film playback or demos
(`get_game_controller()`). Per-hand weapon state has no wire or film representation, so
a client running it would desync immediately and a recorded film would replay wrong.
Keeping that condition in one function is what stops the restriction being bypassed by
accident later.

---

## 2. What the engine already gives us

All verified in source.

| Need | Existing primitive |
|---|---|
| Draw a weapon model anywhere in world space | `OGL_RenderVRWeaponModel()` — arbitrary position + right/up/forward basis (`OGL_Render.h:101`) |
| Throw with physics, bounce, land as a pickup | `_projectile_ball` (see below) |
| Select a specific weapon, netplay-safe | `encode_hotkey_sequence()` → `ready_weapon()` (`vbl.cpp:1221`, `weapons.cpp:939`) |
| Manual reload hook | One gate in `reload_weapon()` (`weapons.cpp:2378`) |
| Per-weapon persistent trigger state | `player_weapon_data.weapons[]` — already per-weapon, already serialised |
| Per-hand physical stick click | `s_stickClick[2]` — already stored raw per hand (`vr_openxr.cpp:999`) |
| World-space debug lines | The GL idiom in `render_vr_aim_gizmos()` (`render.cpp:637`) |

### Throwing is nearly free

`_projectile_ball` (`projectile_definitions.h:652`) is already defined with exactly the
behaviour we want:

```
_becomes_item_on_detonation | _doubly_affected_by_gravity
| _rebounds_from_floor | _persistent_and_virulent
speed = 0          /* all velocity comes from the initial vector */
```

Gravity, bouncing, settling and landing as a retrievable item are all present, and
`speed = 0` means the throw velocity comes purely from the hand. We clone the
definition per throw class to shed the skull shape, ball sounds and its 40 damage, and
render the weapon's own model on the object.

### The reload state machine

Entered from exactly one place, requiring `state == _weapon_idle && rounds_loaded == 0`:

```
_weapon_idle (empty)
  -> _weapon_awaiting_reload    (await_reload_ticks)
  -> _weapon_waiting_to_load    (loading_ticks)     <- rounds_loaded = rounds_per_magazine
  -> _weapon_finishing_reload   (finish_loading_ticks)
  -> _weapon_raising            (ready_ticks)
  -> _weapon_idle
```

Ammo is a shared pool: `player->items[trigger_definition->ammunition_type]`.

---

## 3. Obstacles

1. **Both grips are already bound.** Off-hand grip = two-handed latch
   (`vr_openxr.cpp:1344`); dominant grip = alt-fire (`vr_openxr.cpp:1849`). Both move.
2. **No model attachment points exist.** `Model3D.h` has no MD3 tag support — nothing in
   the format or loader carries a magwell, foregrip or muzzle. Every attach point must
   be authored as a model-local offset in MML. *This is why Phase 0 comes first.*
3. **Items auto-pickup.** `swipe_nearby_items()` (`items.cpp:489`) grabs anything within
   `MAXIMUM_ARM_REACH` as you walk. Must be suppressed; pickup becomes deliberate.
4. **Two of the same weapon.** One `weapon_data` per weapon type, holding two triggers.
   Two of the same weapon only works for `_twofisted_pistol_class` (magnum, shotgun,
   fists), where the engine already models it as trigger 0 + 1. Two assault rifles or
   two fusion pistols is not expressible. Accepted.
5. **The engine auto-switches weapons in six places.** With cycling disabled and the body
   as the only selector, every `select_next_best_weapon()` call site
   (`weapons.cpp:441, 990, 2506, 2528, 2565, 2598`) plus `should_switch_to_weapon()`
   (`:569`) and `check_player_weapons_for_environment_change()` must be gated. A gun that
   runs dry must stay in your hand and click empty — which is also what manual reload
   requires, so these agree.

---

## 4. Architecture: the active-weapon slot

`update_player_weapons()` and its helpers are written around "the current weapon",
reached through four accessors that each read `player_weapons->current_weapon`:

```
get_player_current_weapon()      -> &weapons[current_weapon]
get_current_weapon_definition()
get_player_trigger_data()        -> get_trigger_data(.., current_weapon, ..)
get_player_trigger_definition()
```

Make those read an **active slot** and the state machine becomes re-entrant over hands.
A hand slot is a `(weapon_type, trigger)` pair, which covers every case uniformly:

- two *different* weapons → each slot uses trigger 0 of its own weapon
- dual magnums/shotguns/fists → both slots use trigger 0 and 1 of the *same* weapon,
  which is exactly the existing `_twofisted_pistol_class` mechanism
- empty hand → `weapon_type = _weapon_fist`, so punching is just holding nothing

```c
struct hand_slot {
    short weapon_type;   // _weapon_fist when empty
    short trigger;       // which trigger_data within that weapon
};
```

Appended to `player_weapon_data`, **not serialised** (same pattern as the existing
`hotkey` / `hotkey_sequence` fields, `player.h:415`). `pack_player_weapon_data()` writes
field-by-field and asserts a fixed 472 bytes, so it stays untouched. On load, slot 0
derives from `current_weapon`, slot 1 = fist.

```c
static short get_active_weapon(short player_index) {
    auto pw = get_player_weapon_data(player_index);
    if (!vr_per_hand_active()) return pw->current_weapon;   // stock path, bit-identical
    return pw->hand[pw->active_slot].weapon_type;
}
```

`update_player_weapons()` splits into an extracted `update_one_weapon()` (the existing
body, verbatim) plus a driver:

```c
if (vr_per_hand_active()) {
    for (int slot = 0; slot < 2; ++slot) {
        pw->active_slot = slot;
        update_one_weapon(player_index, action_flags_for_slot(slot));
    }
    pw->active_slot = 0;
} else {
    update_one_weapon(player_index, action_flags);          // unchanged
}
```

### Two kinds of "dominant hand"

Catching a weapon changes which hand is primary. But `dominantHand` today is a single
static pref that routes the weapon **and** the locomotion sticks **and** the menu laser.
If catching a gun flipped it, your movement stick would jump hands mid-firefight. Split
it:

| Concept | Kind | Drives |
|---|---|---|
| `dominantHand` | pref, static | locomotion sticks, menu pointer, UI |
| `primaryWeaponHand` | dynamic, follows the gun | weapon primary, HUD ordering, haptics routing |

### Work inventory for the core

`current_weapon` refs by function (105 total, all in `weapons.cpp`):

| Function | Refs | Treatment |
|---|---|---|
| `idle_weapon` | 11 | → `get_active_weapon()` |
| `fire_weapon` | 10 | → `get_active_weapon()`; per-hand aim + haptics routing |
| `update_player_weapons` | 7 | split into driver + `update_one_weapon` |
| `should_switch_to_weapon` | 7 | gated off in this mode |
| `change_to_desired_weapon` | 6 | per-slot |
| `get_player_weapon_mode_and_type` | 5 | must report both slots |
| `weapon_works_in_current_environment` | 4 | → `get_active_weapon()` |
| the four accessors | 11 | the actual change |
| remainder (~24 fns, 1–3 each) | ~45 | mechanical |

Outside `weapons.cpp`: two read-only Lua pushes (`lua_player.cpp:1325`,
`lua_hud_objects.cpp:1689`) and one HUD site reading `desired_weapon`
(`HUDRenderer.cpp:607`). Minimal.

### Display information

`get_weapon_display_information()` gains a **`hand_slot` field** on
`weapon_display_information`, so the VR renderer is told which hand a sprite belongs to
rather than inferring it from screen position. This retires the fragile
`_position_center` heuristic in `render_vr_weapon_sprites_3d()` (`render.cpp:1027`) and
the `vr_hud_display_trigger()` lefty swap hack.

`get_interpolated_weapon_display_information()` (`interpolated_world.cpp`) needs the
same treatment — easy to miss.

---

## 5. Input scheme

| Input | Stock | Experimental mode |
|---|---|---|
| Trigger (each hand) | Fire | Fire that hand's weapon |
| Grip (each hand) | Two-handed latch / alt-fire | **Hold weapon or magazine**, grab, catch |
| **Physical L3 / R3** | Run / Map (role-based) | **Alt-fire for that hand** |
| Two-handed | Hands within 0.5 m | Off-hand grip **near the weapon's foregrip point** |
| Next / Prev weapon | X / Y | **Disabled** |
| Toggle map | Turn-stick click | Moves to a face button |
| Run | Move-stick click | Moves to a face button |
| Eject magazine | n/a | New bindable `VR_ACT_EJECT_MAG` |

Disabling weapon cycling frees exactly the two face buttons that Run and Map need.
Proposed defaults (all still user-remappable): A = Action/Use, B = Run, X = Toggle Map,
Y = Eject Magazine, L3/R3 = per-hand alt-fire.

Two notes:

- **Alt-fire must use physical L3/R3, not the current role-based stick clicks.** Today
  `VR_GetMoveStickClick()` / `VR_GetTurnStickClick()` resolve through handedness and
  `switchSticks`, so alt-fire would follow your *move* hand rather than the hand holding
  the gun. The raw state is already there — `s_stickClick[2]` (`vr_openxr.cpp:999`) — so
  this is just a new accessor, no OpenXR rebinding.
- Proximity-based two-handing is a real upgrade on the "hands within 0.5 m" latch: it's
  per-weapon correct, and it makes the `<two_handed_offset>` MML kludge unnecessary,
  because the foregrip point *is* the grip location.

New `VR_ACT_*` values append to the end of the enum in `vr_openxr.h:41` — persisted by
index, so **append only**.

---

## 6. Body holsters

Eight fixed anchors, which is exactly the number of holsterable weapons in the largest
scenario (Infinity: magnum, fusion pistol, assault rifle, missile launcher,
flamethrower, alien shotgun, shotgun, SMG — fist and ball are not holstered).

| Anchor | Typical occupant |
|---|---|
| `right_hip`, `left_hip` | pistols — magnum, fusion pistol |
| `right_thigh`, `left_thigh` | compact — SMG |
| `right_chest`, `left_chest` | shotgun, alien shotgun |
| `right_back`, `left_back` | over-shoulder long guns — AR, missile launcher, flamethrower |
| `left_pouch`, `right_pouch` | **ammo pouches** — dispense magazines, hold no weapon |

The two pouches are not holsters. Reaching into one gives you a magazine for whatever
weapon is in your *other* hand, so there is no need for a pouch per ammo type (Marathon
has nine): left dispenses that weapon primary ammo, right its secondary — which in
practice only the assault rifle grenade launcher needs. The mag is drawn from the shared
pool, `player->items[ammunition_type]`.

One weapon per anchor, assigned in MML per scenario. Fixed positions beat compact ones:
the entire value of body holsters is that your hand learns where each gun lives.

**Reachability is a real risk.** With cycling gone, an unreachable anchor means an
unusable weapon — for a seated player, an odd room-scale body offset, or a mis-tuned
anchor. Mitigations, in order: Phase 0's live tuning makes anchors adjustable in-headset;
anchor offsets scale with the measured standing eye height; and a hidden "summon
weapon" bind stays available as an escape hatch.

Holster anchoring uses head yaw for now (head yaw *is* the player's facing in this
port), so holsters rotate as you look around. A smoothed torso yaw is more correct but
adds state — revisit on device.

---

## 7. MML schema

Attach points are model-local offsets in metres in the weapon model's own frame
(+Z forward down the barrel, +Y up, +X right), extending the existing `<vr_weapons>`
block parsed by `vr_weapons.cpp`.

```xml
<vr_weapons>
  <vr_grip      name="assault_rifle" x="0"    y="0"     z="0"/>
  <vr_foregrip  name="assault_rifle" x="0"    y="-0.02" z="0.25" radius="0.12"/>
  <vr_magwell   name="assault_rifle" x="0"    y="-0.05" z="0.08"
                pitch="0" yaw="0" roll="0" radius="0.08" angle_tolerance="35"/>
  <vr_muzzle    name="assault_rifle" x="0"    y="0.01"  z="0.42"/>
  <vr_holster   name="assault_rifle" slot="right_back"/>
  <vr_mag_model name="assault_rifle" item="assault_rifle_magazine"
                grip_x="0" grip_y="0" grip_z="-0.04"/>
  <vr_throw     name="assault_rifle" damage="12" tumble="1.0" catch_radius="0.18"/>
</vr_weapons>
```

`<vr_mag_model>` defaults to the weapon's own `ammunition_type` item shape
(`get_item_shape()`), so most weapons need no explicit entry.

---

## 8. Phases

### Phase 0 — Debug + authoring scaffolding **(first, and load-bearing)**

Nothing downstream is tunable until attach points can be seen and placed.

1. MML schema above + `vr_weapons.cpp` parsing and accessors.
2. `render_vr_attach_gizmos()` in `render.cpp`, gated on a `showAttachGizmos` flag,
   reusing the GL idiom from `render_vr_aim_gizmos()` (`glUseProgram(0)`, `GL_LINES`,
   depth test off):
   - grip — white cross
   - foregrip — cyan ring at its radius
   - magwell — orange axes + an arrow along the insertion direction
   - muzzle — red arrow down the barrel
   - holster anchors — wireframe boxes on the body, coloured by occupancy
   - held magazine — its own origin + axes
3. **Live tuning mode.** Point at a gizmo, nudge the offset with the thumbstick, dump the
   resulting MML block to the A1VR log and to a file on device.

   Item 3 is the highest-leverage thing in this plan. There are six attach points across
   eight weapons across three scenarios, plus eight body anchors. Without in-headset
   tuning every one costs a build/deploy/"a bit left"/build round-trip, and I can't wear
   the headset to shortcut it.

### Phase 1 — Holster visuals + grip-to-draw (vertical slice) **[done 2026-09-26]**

Deliberately moved ahead of the engine core. The per-hand rewrite is large, invisible and gives
nothing to react to, and the body anchors cannot be judged until a weapon is actually hanging on
one. So: draw every carried weapon at its holster and let a grip near one select it, on top of the
**existing single-weapon engine**.

- `render_vr_holstered_weapons()` draws each carried weapon at its anchor, skipping the one in
  hand. Needs a weapon shape resolved without it being current, hence the new
  `get_weapon_idle_shape()` / `get_weapon_item_type()` in weapons.cpp — every existing path into
  the shape tables goes through `get_player_current_weapon()`. Falls back to a billboard sprite
  when a weapon has no 3D model, so a holster is never silently empty.
- `vr_update_holster_grabs()` does nearest-occupied-holster-within-radius on a grip rising edge,
  and posts `VR_SetHolsterGrabRequest()`.
- vbl.cpp takes that request and issues `encode_hotkey_sequence(get_weapon_ordering_index(w))` —
  the identical 3-tick action-flag path a number key uses. Weapon selection stays on one code path,
  which is what keeps the netplay door open.

Known rough edges, deliberate for now: holstered weapons hang barrel-down with no per-slot
orientation (over-shoulder slots will look wrong — wants a `<vr_holster>` orientation attribute);
a drawn weapon cannot be stowed again until grip-to-hold lands.

### Phase 2 — Per-hand weapon core (engine)

Section 4, behind the pref, SP-only. **No new VR interaction yet** — verify by forcing
two weapons into the slots from a debug bind. Done when two weapons can be raised, fired,
animated and reloaded independently without disturbing the stock path.

Includes the `dominantHand` / `primaryWeaponHand` split and gating the six
`select_next_best_weapon()` sites.

Risk sites: `check_player_weapons_for_environment_change()` on level transitions;
`discharge_charged_weapons()` on death; the alien shotgun's `_weapon_disappears_after_use`.

### Phase 3 — Grip-to-hold + input re-homing

Hand state machine (`EMPTY` / `WEAPON` / `MAGAZINE`), `gripMode` pref, physical L3/R3
alt-fire, Run and Map rehomed, cycling disabled, two-handing moved onto the foregrip
point.

### Phase 4 — Body holsters (completion)

Eight anchors, draw and stow, holstered weapons rendered as world-space models via
`OGL_RenderVRWeaponModel()`. Grabbing a weapon is what performs the engine-side weapon
select (via `encode_hotkey_sequence()`, keeping the netplay door open).

### Phase 5 — Throw, catch, ground pickup **[DEPRIORITISED 2026-09-26]**

Cloned `_projectile_ball` throw class carrying hand velocity, tumbling the weapon model
in flight, light contact damage per `<vr_throw damage>`. Mid-air catch = per-frame hand
proximity against in-flight throw objects; catching sets `primaryWeaponHand`. Suppress
`swipe_nearby_items()` and require a deliberate grip-grab to pick up.

Catching needs care: projectiles tick at 30 Hz on int16-quantised world points, so a hard
throw covers real distance per tick. Use the interpolated position and a forgiving
`catch_radius`.

### Phase 6 — Magazines and manual reload

The loop:

1. The gun runs dry and **stays dry** — it clicks empty, no auto-reload.
2. **Eject** the spent magazine with the bound button (`VR_ACT_EJECT_MAG`).
3. Reach into an **ammo pouch** on your body and grip: a fresh magazine appears in that hand.
4. Bring it to the weapon **magwell**; within `radius` and `angle_tolerance`, it seats.
5. Seating triggers the engine reload, which does the ammo accounting, sound and animation.

Ejection stays a button rather than a grip-and-pull gesture. On a rifle the magwell and
the foregrip sit centimetres apart, so a grip near there would be ambiguous between
"two-hand the weapon" and "pull the mag" — and getting that wrong mid-firefight is worse
than pressing a button. Revisit once the authored radii show how much room there really is.

**Ammo is decremented on SEAT, not on grab**, through the engine own `reload_weapon()`
path. Grabbing a mag and dropping it therefore costs nothing and cannot duplicate ammo.

Because `reload_weapon()` requires `rounds_loaded == 0`, ejecting a partial magazine
discards the rounds still in it — which is also the honest physical reading, since
Marathon has no concept of a partially filled magazine.


Gate `reload_weapon()` on `VR_ManualReloadPending()`; the VR layer sets a one-shot when a
mag is seated and the engine runs its normal reload states, so ammo accounting, sounds
and animation stay engine-side. Fall back to auto-reload when a weapon has no
`<vr_magwell>` authored — the never-softlock rule, and what lets mags roll out
weapon-by-weapon as models are built.

### Phase 7 — Charged fusion cook-off, polish

Throwing a charging fusion pistol carries the remaining overload ticks onto the thrown
object; on expiry it detonates where it lands and the weapon is destroyed. Then haptics
on grab/seat/catch/throw, sounds, preferences UI, and the additive netplay fold — holster
and mag state is derivable, so only discrete events (draw, throw, catch, seat-mag) travel.

---

## 8b. Models: split magazines and posed hands (proposed 2026-09-28)

What the engine has: MD3 vertex-animated models (keyframes), several models per weapon shape (drawn
together with one transform), a GPU morph between any two keyframes (`a1ffDrawMorphMesh`, frame / next
frame / mix), and a mirrored draw for the left hand. What it lacks: the MD3 loader reads the header but
ignores TAGS (named attachment frames), and there is no skeleton support.

**Magazines: one separate MD3 per magazine, drawn by the magazine code.**
- In Blender, separate the magazine from each gun and export two files; the gun keeps its origin.
- Put the magazine's origin at the CENTRE OF ITS TOP FACE (the end that goes in), +Y along the insertion
  line into the gun, +Z toward the muzzle -- exactly the box's frame, so it drops in where the box is.
- Before moving the origin, note where that point sits in the gun's space: that is the seated `x/y/z` of the
  `<vr_magwell>` (or, later, a tag).
- New MML: `<vr_mag_model ammo=... file=... skin=...>` (per ammo type, so the same mag draws in the pouch,
  the hand, the gun and on the floor). The gun no longer carries the magazine, so an empty well looks empty.
- Gun reload animations that move a magazine stop applying -- fine, reloading is physical now.

**Attach points: MD3 tags (small loader addition).**
- Add tags in Blender (`tag_magwell`, `tag_magwell2`, `tag_foregrip`, `tag_muzzle`, `tag_grip`): each is a
  position + orientation, so the magwell's seat, insertion angle (pitch) and roll come straight from the
  model. The MML/tuner values become overrides. Parsing tags is ~50 lines in MD3_Loader.cpp.

**Hands: one hand MD3 whose keyframes are POSES; blend between them on the GPU.**
- Frames: open, relaxed, fist, pistol grip, rifle grip, trigger-pulled grip, support (foregrip) grip,
  magazine hold, pouch reach. Right hand only; the left is the mirrored draw we already use.
- Each weapon names its grip pose in MML (`<vr_hand_pose weapon=... hold=... support=...>`), so a pistol
  and a rifle hold differently without new code.
- Transitions: morph from the current pose to the new one over ~80-120 ms (the existing two-frame mix).
- Analog: the grip axis blends open -> fist when empty-handed; the trigger axis blends grip -> trigger-pulled.
  Two axes at once needs a 3-4 frame blend; start with one at a time and extend the shader only if needed.
- Why not skeletal: no skeleton support in the engine, and vertex poses on the Quest are cheap and already
  proven (weapon morph work, docs/memory vr-model-gpu-lerp).

Suggested order: (1) the hand MD3 with 4 poses (open, fist, rifle grip, support), (2) one gun split
(pistol) + its mag model, (3) tags, (4) the rest of the guns.

---

## 9. Build gotchas

Any new `.cpp` must be registered in **both** builds:

- **Android:** `android/jni/CMakeLists.txt` globs at configure time with no
  `CONFIGURE_DEPENDS`. Touch it, or you get a deceptive "BUILD SUCCESSFUL" that silently
  skipped the file.
- **Desktop:** add `<ClCompile>` to `VisualStudio/LibAlephOne/LibAlephOne.vcxproj`.
- Any unguarded `VR_*` call needs a stub in `vr_openxr_win.cpp`.

Prefer extending `vr_weapons.{h,cpp}` and `render.cpp` over adding files.

---

## 10. Open questions

1. **Tactical reloads.** `reload_weapon()` requires `rounds_loaded == 0`, so the engine
   cannot swap a partial magazine. Either forbid ejecting a partial mag, or discard the
   remaining rounds on eject.
2. **Does a cooked-off fusion pistol survive?** Proposed: no — it detonates and the item
   is lost, making it a real cost/benefit choice rather than a free grenade.
3. **Two-handing an off-hand weapon.** Does the dominant hand grab the foregrip, and does
   that flip `primaryWeaponHand`?
4. **Holster yaw.** Head yaw now; smoothed torso yaw later if holsters feel like they
   swing away when you look around.
