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
