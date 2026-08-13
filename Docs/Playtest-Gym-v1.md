# Playtest Gym v1

The existing First Person map now becomes a zero-setup combat and movement test whenever `BreakerGameMode` is active.

## What appears at runtime

- Four recycling diagnostic targets: health, shield, armour, and lateral movement.
- Three recycling chase/attack enemies using the same GAS damage pipeline as the player and targets.
- Two LATTICE ranged enemies (`ABreakerRangedEnemy`), spawned wide on either
  flank of the melee pack so their fire lanes cross the ground route the
  chasers push you down. See "The ranged archetype" below.
- Runtime movement facilities: mantle steps, dash markers, gap platforms, parallel wall-ride lanes, a flat slide lane, and a downhill slide lane.
- A code-driven crosshair and debug HUD showing movement state, horizontal speed, health, shields, ammunition, and reload state.
- Red body-hit feedback, gold weak-point feedback, and applied damage numbers.
- A deliberately simple first-person placeholder weapon block with a procedural kick and muzzle flash. It exists only to communicate viewmodel position and firing state until proper art and animation replace it.

The targets use engine basic-shape meshes, so this setup does not depend on new binary assets or machine-specific editor work.

## Controls

- Move: WASD
- Look: Mouse
- Sprint toggle: Left Shift
- Dash: Q
- Slide: C or Left Ctrl
- Jump / wall jump: Space
- Fire: Left mouse
- Aim: Right mouse
- Reload: R
- Equip primary / secondary weapon: 1 / 2. The current gym loadout is rifle / scattergun.
- Escape: open the pause menu; Escape backs out of Settings or Loadout and resumes from the pause root.

The game opens on a title menu rather than immediately dropping the player into the gym. Title and pause menus expose the two-slot loadout and saved sensitivity, FOV, and invert-look settings.

The combat HUD is grouped along the bottom-left: movement and vitals, active weapon/ammunition, then placeholder slots for two class abilities and one ultimate. The ability slots communicate the intended shipping layout but have no gameplay bindings yet.
- Reset player, ammunition, targets, and session stats: F1
- Copy a structured session report to the clipboard: F2
- Toggle diagnostics: F3
- FOV: left/right bracket
- Mouse sensitivity: fixed equal X/Y baseline for this pass

FOV persists locally between launches. Diagnostics show frame rate, session accuracy, weak-point rate, damage, reload count, target types/distances, and a brief impact marker. The copied report is formatted for pasting into the Notion Signals database.

## The ranged archetype — LATTICE

Implements Encounter-Design §2.2. Until this landed, every enemy in the gym
was a melee chaser whose three gears all ended in contact, so nothing asked
the player to take cover, strafe, or care about distance, and nothing had an
attack that was visible in flight.

**How you meet it.** No new key. It is already in the world:

- **On spawn / after F1 reset** — two of them, mossy grey-green humanoids with
  a violet emitter at chest height, standing ~32m out on the left and right
  flanks of the melee pack, roughly 15m to either side of the pack's line.
  Walk toward the encounter and they will already be holding station.
- **In wave mode (F4)** — from wave 2 onward, `Wave/2` of them capped at 3
  (Encounter-Design §5.3's hard cap on converging projectile sources), placed
  a ring further out than the melee packs. They come **out of** the melee
  budget, not on top of it, so pack density is unchanged.
- Its state label prints over its head like every other enemy:
  `CLOSING` / `HOLDING` / `FALLING BACK` / `AIMING` / `REPOSITION`.

**What it does.** It holds a preferred band (900–1900cm) instead of closing:
advances when you are too far, backs off *faster than it advances* when you
crowd it, and strafes — reversing direction on a cadence — while it fires. It
never has a contact attack; the base melee path is disabled on it.

**The shot.** A real replicated projectile actor (`ABreakerEnemyProjectile`),
not a hitscan with a cosmetic streak. A ~42cm violet-magenta orb at 1100 cm/s
carrying its own point light, flying in a straight line with no gravity and no
splash. Player sprint is 950 cm/s, so it crosses the band in 0.8–1.7s: long
enough to watch it come and sidestep it. Violet, not teal — the object-chroma
law reserves saturated teal for rift objects and suppression hardware.

**The telegraph.** A 0.85s wind-up before every shot. The chest emitter grows
from 0.16 to 0.38 scale, ramps from near-black violet to hot violet, and its
point light blooms on a squared curve so the last third of the wind-up is
where it really lights. The enemy also drops to 30% move speed while
committing, so the tell is readable from its feet as well as its emitter. Per
Encounter-Design §0, this window is tuned for **passive** defence (O1) and is
not to be shortened — the answer is repositioning, not a button.

**Leading — the deliberate choice.** It uses a **partial lead**
(`LeadFraction` 0.35). Encounter-Design §2.2 specifies zero lead, which makes
a moving player literally unhittable from the front and reads as harmless; a
full lead means only a direction change beats it. 0.35 aims at where a player
holding a straight sprint line would be, so keeping a lane gets punished and
**any** change of direction — strafe, dash, slide, jump — beats it outright.
Set `LeadFraction` to 0 to recover the document's pure Lattice.

**Tuning knobs**, all `EditAnywhere` on `ABreakerRangedEnemy` and all
**O2 PLACEHOLDER**:

| Category | Knobs |
|---|---|
| `Enemy\|Ranged\|Band` | `MinEngagementDistance` 900, `MaxEngagementDistance` 1900, `BandHysteresis` 150, `AdvanceSpeedMultiplier` 1.25, `RetreatSpeedMultiplier` 1.35, `StrafeSpeedMultiplier` 0.90, `StrafeReverseSeconds` 2.1 |
| `Enemy\|Ranged\|Fire` | `WindupSeconds` 0.85, `WindupMoveScale` 0.30, `ShotCooldown` 2.4, `ProjectileSpeed` 1100, `LeadFraction` 0.35, `ProjectilesPerVolley` 1, `VolleySpreadDegrees` 7, `ProjectileClass` |
| `Enemy\|Ranged\|Telegraph` | `TelegraphIdleColor`, `TelegraphHotColor`, `TelegraphLightIntensity` 2600, `TelegraphIdleScale` 0.16, `TelegraphHotScale` 0.38 |
| Inherited chassis | `DetectionRange` 3200, `MoveSpeed` 320, `AttackDamage` 16 (the projectile's damage), health inherited at 220 |
| `ABreakerEnemyProjectile` | `MaximumLifetime` 6, `OrbColor`, `GlowIntensity` 2400, `GlowRadius` 700, and the collision sphere radius / visual scale in the constructor |

**Health is deliberately the base 220, not §2.2's 1.6x.** Trash and elite
health are mid-re-anchor and awaiting an owner ruling; adding a 1.6x enemy now
would push the measured TTK further past the O18 target. Apply the 1.6x ratio
when the re-anchor lands.

**Known gap:** the playtest report's TTK sampling does not separate ranged
kills from melee kills — both feed the same trash bucket. `Playtest/` is not
owned by this change; adding an archetype dimension is a follow-up.

## First feedback pass

Test each item for several minutes and record **too weak / good / too strong**, plus one sentence describing why:

1. Walk acceleration and stopping precision.
2. Sprint speed relative to arena scale.
3. Dash force, direction control, momentum retention, collision/stop cancellation, and four-second cooldown.
4. Slide entry reliability, duration, slope response, and exit control.
5. Wall-ride activation consistency, 0.85-second maximum, gravity, and wall jump.
6. Hip-fire spread versus aimed spread.
7. Rifle damage cadence, reload length, and magazine size.
8. Weak-point readability and hit feedback.
9. Damage falloff between the near, middle, and far targets.
10. Any motion discomfort, camera obstruction, collision snag, or input failure.

Do not tune from a single run. First verify the controls and target behavior, restart once, then perform the real feedback pass.
