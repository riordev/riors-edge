# Weapon foundation

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

The first weapon pipeline is a server-authoritative hitscan prototype with a Data Asset definition and a built-in fallback rifle so clean clones can fire without editor-authored content.

## Playable archetypes

The player carries two weapons. Keys 1-2 select the equipped primary and
secondary slots; the gym's default loadout is Rifle / Shotgun. **Eight**
clean-clone fallback archetypes exist, and they are named by exactly one table
(`BreakerWeaponArchetypeNames` in `Weapons/BreakerWeaponArchetype.h`) that
serves the HUD, the loadout screen and item cards — a gun named in three places
gets renamed in two:

| Enum | Display name | Older names still in this document |
|---|---|---|
| `Rifle` | Rifle | — |
| `SMG` | SMG | — |
| `Sniper` | Sniper | — |
| `Shotgun` | Shotgun | — |
| `Rocket` | Rocket Launcher | — |
| `BurstRifle` | Burst Rifle | **Volley** |
| `Machinegun` | Machinegun | **Bulwark** |
| `Sidearm` | Sidearm | **Mark** |

The three O27 breadth additions were designed under the codenames Volley,
Bulwark and Mark and shipped under the plain names; the design sections below
keep the codenames in their headings because that is what the reasoning was
written against, but **the code, the UI and every other document use the
right-hand column.**

**Reconciliation note (O40 pass).** The code enum `EBreakerWeaponArchetype` is
canonical: **Rifle, SMG, Sniper, Shotgun, Rocket** (the original five, which is
what the vertical slice ships against — see `Vertical-Slice.md`), plus
**BurstRifle, Machinegun, Sidearm** from the O27 breadth pass. This document's
prior **Scattergun** and **Marksman** naming (below) has been aligned to the
code names **Shotgun** and **Sniper**. This is a code-alignment fix made during
the O40 reconciliation pass, not a numbered ruling in its own right.

All eight are available to the loadout screen and to weapon drops, but only two
may be equipped at once. These remain prototype values rather than balance
commitments.

`EBreakerWeaponArchetype` is stored as a uint8 in `UBreakerSaveGame` and
replicated as one, so new archetypes are **appended, never inserted**:
renumbering the existing eight would silently rearm every saved loadout with a
different gun. The prototype table in `BreakerWeaponComponent.cpp` is indexed by
the enum and carries a `static_assert` on the count, because a missing row is an
out-of-bounds read on first equip rather than a compile error.

### The table, at a glance

Every number is `O2 PLACEHOLDER`, per pellet where pellets exist, and measured
at item level 1 (the anchor of the damage curve below).

| | Damage | Weak pt | RPM | Mag / reserve | Pellets | Hip / ADS spread | Falloff (m) | Floor | Reload | Swap-in |
|---|---|---|---|---|---|---|---|---|---|---|
| Rifle | 24 | 1.75x | 600 auto | 30 / 120 | 1 | 1.2 / 0.25 | 28 - 70 | 0.72 | 1.8 s | 0.50 s |
| SMG | 13 | 1.5x | 900 auto | 35 / 175 | 1 | 2.0 / 0.9 | 18 - 45 | 0.58 | 1.5 s | 0.35 s |
| Sniper | 72 | 2.0x | 150 semi | 8 / 40 | 1 | 2.0 / 0.05 | 50 - 110 | 0.88 | 2.3 s | 0.70 s |
| Shotgun | 10 | 1.35x | 85 semi | 8 / 40 | **8** | 4.5 / 3.0 | 11 - 28 | 0.40 | 2.2 s | 0.50 s |
| Rocket Launcher | 90 | — | 55 semi | 4 / 16 | projectile | 0.6 / 0.2 | rifle curve | 0.72 | 2.8 s | 0.80 s |
| Burst Rifle | 29 | 1.9x | 720 in burst | 27 / 108 | 1 | 1.6 / 0.12 | 36 - 85 | 0.80 | 2.0 s | 0.55 s |
| Machinegun | 11 | 1.4x | 700 auto | **120** / 300 | 1 | 3.2 / 0.8 | 22 - 90 | 0.55 | **4.2 s** | **0.95 s** |
| Sidearm | 21 | 1.8x | 420 semi | 14 / 210 | 1 | 1.1 / 0.30 | 16 - 40 | 0.52 | 1.1 s | **0.18 s** |

Two entries are inherited rather than authored and are worth knowing before
tuning: the **Rocket Launcher never overrides its falloff**, so it uses the
rifle curve, and the **Shotgun never overrides `SwapInDuration`**, so it sits at
the rifle's 0.50 s despite swap tempo being an explicit axis in the table.
`ArmorPenetration` is 0.0 on every archetype — nothing in the game penetrates
armour today.

### Rifle

- 24 base physical damage.
- 600 rounds per minute, automatic.
- 30-round magazine and 120 reserve.
- 1.8-second reload.
- 1.2-degree hip spread and 0.25-degree aimed spread.
- 1.75x weak-point multiplier.
- Full damage through 28m, linear falloff to 72% at 70m.
- 120m maximum trace range.

### Shotgun

- Eight deterministic pellets at 10 base physical damage each.
- 85 rounds per minute, semi-automatic.
- Eight-round magazine and 40 reserve.
- Wide close-range spread and aggressive falloff after 11m, bottoming at 40%
  by 28m. Still by far the steepest curve in the table.

### Sniper

- 72 base physical damage and 2.0x weak-point multiplier.
- 150 rounds per minute, semi-automatic.
- Eight-round magazine and 40 reserve.
- Very tight aimed spread and strong long-range retention.

### Volley (burst rifle) [O27]

- 29 base damage, 1.9x weak point. **Three-round bursts** at 720 RPM inside the
  burst, then a **0.34 s cycle gap you cannot shorten**.
- 27-round magazine — exactly nine bursts, so ammunition is counted in bursts
  and a reload never strands the player mid-burst. 108 reserve, 2.0 s reload.
- Falloff between the rifle and the sniper (36 m to 85 m, floor 0.80).
- **Niche: cadence and discipline.** No other archetype's DPS is bounded by a
  cycle rather than by the trigger. Per round it out-hits the rifle; sustained
  it lands *under* the rifle, and that gap is the price of the accuracy — a
  test asserts it, because a burst weapon that also wins DPS deletes the rifle.
  Its recoil is the most learnable pattern in the game: a near-pure vertical
  ladder (vertical kick is 17x the horizontal) that settles inside its own
  cycle gap, so every burst starts from the same place and pre-aiming down the
  ladder is a real skill rather than a tax.

### Bulwark (machinegun) [O27]

- 11 base damage — the lowest in the table — 1.4x weak point, 700 RPM.
- **120-round magazine**, four SMG magazines in one trigger pull, against a
  **4.2 s reload** and the slowest swap-in in the game (0.95 s).
- 300 reserve: deep in rounds, *shallow in magazines* (2.5), which is the
  actual constraint.
- Long shallow falloff (22 m to 90 m, floor 0.55): it reaches further than the
  rifle but gives ground the whole way, which makes it a suppression weapon
  rather than a long-range one.
- **Niche: ammunition economy and sustained fire.** The only weapon that can
  hold a lane through an entire wave without reloading, and the most helpless
  if it has to reload mid-fight. Its recoil is deliberately *unrideable past a
  point*: the longest climb ramp in the table (22 shots to full) with the
  highest ceilings, and the largest `MaxBloomDegrees` of any archetype — the
  shotgun included — so held fire, not the kick, is what punishes. Highest
  sustained DPS in the table and only while planted.

### Mark (sidearm) [O27]

- 21 base damage, 1.8x weak point, 420 RPM semi-automatic — trigger-limited,
  so its DPS ceiling is the player's click rate and their aim.
- **0.18 s swap-in** (rifle 0.5, Bulwark 0.95), 1.1 s reload, 14-round
  magazine, and the deepest reserve in the table in magazines (15).
- **Niche: tempo.** It exists to make the *swap* a decision and to be the
  answer to a dry primary, not a second primary. It pairs directly with the
  existing swap-tempo layer and the Secondary "damage on swap-in" affix design.
  The smallest kick in the table with by far the fastest settle
  (`RecoveryConstantDegreesPerSecond` 30, delay 0.03 s), so the pattern is not
  a pattern at all: it is back on target before a fast trigger finger gets
  there.

`RiorsEdge.Weapons.ArchetypeBreadth` pins each niche as a RELATIONSHIP rather
than a value (O2 freezes the numbers), and asserts that no two archetypes share
a cadence/magazine/pellet/spread/burst/swap fingerprint — a stat re-roll of an
existing gun is exactly what O27 says not to add. All values are O2 PLACEHOLDER
and **none of the three has been playtested**.

**Burst cadence is a new mechanic**, not a re-skin:
`UBreakerWeaponDefinition::ShotsPerBurst` / `BurstCycleSeconds` (1 / 0 = inert,
which is every pre-existing archetype). Burst weapons run a one-shot timer
chain rather than the repeating timer the other automatics use, because their
interval alternates; non-burst weapons keep the repeating timer untouched so
their cadence cannot shed a callback's latency per shot.

~~**Known content gap:** `ApplyWeaponPresentation` has no case for the three new
archetypes.~~ **CLOSED.** The viewmodel moved into
`Characters/BreakerViewmodelRig.{h,cpp}`, a per-archetype layout table with a
part pool, and all eight archetypes have their own silhouette: the Burst Rifle
carries a tall optic on a riser, the Machinegun a drum and a bipod, the Sidearm
the smallest silhouette in the game. Still blockout geometry, but the player can
now tell which gun they are holding.

The composite placeholder model changes proportions per archetype and provides muzzle flash, procedural kick, ADS alignment, ammunition state, and hit/weak-point feedback. Authored meshes, animation, VFX, and audio remain Blueprint presentation work.

## Weapon feel (recoil, bloom, viewmodel kick)

The mechanical half of "it should feel like a weapon" lives in
`Source/RiorsEdge/Weapons/BreakerWeaponFeel.{h,cpp}`: pure maths, no world, no
timers, fully unit-tested. `UBreakerWeaponComponent` owns the state and the
application; `FBreakerRecoilProfile` owns the rules. Muzzle flash, audio, and
animation remain Blueprint presentation and are deliberately not faked here.

Two invariants, both enforced in code and asserted in tests:

1. **Recoil moves the aim, never the bullet relative to the aim.** The trace
   already follows the controller's view rotation, so kicking that rotation
   moves crosshair and round together. The kick is applied *after* the trace
   resolves, so the shot lands where the player was aiming when they pulled and
   the kick moves the aim for the shot after it.
2. **The pattern is deterministic given a seed**, exactly like the existing
   spread cone. The learnable component is a sine over `HorizontalPatternPeriod`
   shots; the unlearnable component is a small seeded jitter on top.

What the layer does:

- **Recoil.** Per-shot vertical climb with a horizontal pattern, ramping while
  the trigger is held (`ClimbRampShots`/`ClimbRampMultiplier`), clamped by
  `MaxVertical/HorizontalDegrees`, then settling back toward the original aim
  after `RecoveryDelaySeconds` — proportional interpolation with a constant
  floor so it lands on zero rather than asymptoting. `RecoveryFraction` below
  1.0 makes part of every kick permanent.
- **Player compensation.** Aim movement that opposes the accumulated kick
  spends the recovery budget instead of being undone by it, so pulling down
  mid-burst is not punished by the settle shoving the view down afterwards.
- **First-shot accuracy and bloom.** After `BurstResetSeconds` of trigger rest
  the burst index and bloom reset; shot 0 uses
  `BaseSpread * FirstShotSpreadMultiplier` (0.0 = dead accurate) and later
  shots use `BaseSpread + Bloom`. The shotgun keeps `1.0` because its pellet
  cone is its identity.
- **ADS.** Tightens the weapon on four axes at once: the definition's aimed
  spread, `AimRecoilMultiplier`, `AimBloomMultiplier`, and
  `AimViewmodelMultiplier`. Every archetype's aimed kick is strictly smaller
  than its hip kick, and a test enforces it.
- **Viewmodel kick.** The placeholder mesh is displaced instantly (back,
  lateral following the recoil sign, muzzle up) and returned by a substepped
  spring, so it is frame-rate independent. `ABreakerCharacter::Tick` samples
  `GetViewmodelLocationOffset()`/`GetViewmodelRotationOffset()` onto the mesh;
  the old timed snap is gone.

Per-archetype character lives in the archetype table in
`BreakerWeaponComponent.cpp` beside cadence, spread, falloff and damage: the
SMG buzzes and wanders sideways, the rifle climbs learnably, the sniper and
shotgun shove once and settle slowly, the rocket is the heaviest and slowest.
All values are O2 PLACEHOLDER.

Tuning order, highest leverage first:

1. `UBreakerWeaponComponent::RecoilScale` on the component instance — one dial
   over every weapon's kick. `bRecoilEnabled`/`bViewmodelKickEnabled` A/B the
   whole layer.
2. `RecoilOverrides` (a per-archetype map on the component instance) — the way
   to retune one weapon in the editor with no recompile and no code change. An
   entry here beats the definition asset, which beats the fallback table.
3. Inside a profile: `VerticalKickDegrees` for how hard it hits,
   `RecoveryInterpSpeed`/`RecoveryConstantDegreesPerSecond` for how fast it
   settles, `BloomPerShotDegrees`/`MaxBloomDegrees` for how badly held fire
   punishes, `ViewmodelKickUnits`/`ViewmodelKickPitchDegrees` for how much the
   mesh moves.

One knob per playtest complaint, highest leverage first:

| Complaint | Turn this first | Default | Effect of raising it |
|---|---|---|---|
| Weak points feel stingy | `WeakPointToleranceCm` (component) | 14 | Wider forgiveness halo. 0 = old exact test |
| Falloff is too high | `MinimumFalloffMultiplier` (per definition) | rifle 0.72 | Raises the floor without moving where the curve starts |
| Hip fire is the worse option | `AimMoveSpreadMultiplier` (per profile) | 2.2 | Widens aimed movement further, growing hip fire's band |
| ADS still has no cost | `AimMoveSpeedMultiplier` (per profile) | rifle 0.72 | LOWERING it slows an aimed player further. **Live** — the movement component reads it on the grounded cap |
| The gun feels slow / fast | `Weapon.FireRate` affix, or the archetype's `RoundsPerMinute` | — | Cadence is a real stat now; see the fire rate section |
| A spread reads as one round | `MaxSpreadStreaks` (renderer) | 4 | More sub-streaks per blast, out of a 12-slot pool |

Feel is not verifiable by automation. The tests prove the maths — accumulation,
clamping, monotone recovery to exactly zero, compensation credit, the ADS
difference, bloom growth and decay, spring return to rest — and prove nothing
about whether it feels good.

## Range, weak points, and the hip/ADS trade

Owner playtest report, three complaints that interact:

> "hip firing feels worse than ads"
> "weakpoints dont feel forgiving as they should and dmg fall off is too high"

### Weak points: a world-space forgiveness halo

The enemy weak point is a 20 cm sphere at the head (`ABreakerEnemy`, 18 cm on
`ABreakerTargetDummy`) and the shot is a zero-radius line, so acceptance was a
binary the player could not feel the edges of: the round that clipped the ear
and the round that missed the shoulder read identically, and no amount of
aiming better told you which one you had got. Worse, the body box
(42 cm half-width) sits in FRONT of the head sphere in the 58-62 cm band where
they overlap, so the bottom of the head was shadowed by the torso and could not
be hit at all from the front.

`FBreakerWeaponMath::IsWithinWeakPointTolerance` widens acceptance to
`Radius + WeakPointToleranceCm` measured as the closest approach of the shot
ray to the weak point's centre, clamped to the forward half of the ray. The
halo is **world space**, so the generosity is the same physical size at 5 m and
at 50 m — aiming does not get easier by walking backwards, and the reward for a
near-miss is legible instead of random. It also fixes the torso-shadow bug for
free, because a chest-height round that passes 18 cm under the head centre is
now inside even the untoleranced radius.

Two rules keep it forgiveness rather than aim assist:

1. **It never creates a hit.** Only a round that already hit that actor may be
   upgraded. A shot that misses the enemy entirely, or hits the wall in front
   of their head, is unchanged.
2. **It never outlives the weak point.** A weak point whose collision is off
   (a corpse) is skipped.

`WeakPointToleranceCm` is O2 PLACEHOLDER **14**, on the component instance, and
**0 restores the exact old geometric test**. At 14 the head's effective radius
goes 20 -> 34 cm, about 2.9x the projected area.

**This raises DPS and therefore contaminates TTK.** The measured session landed
36.5% of hits on weak points, an average rifle multiplier of
`1 + 0.365 x 0.75 = 1.274`. The ceiling if every hit became a weak point is
1.75, i.e. **+37%**; a realistic move to a 50-60% rate is **+8% to +14%**
average damage per hit. Subtract this before ruling on the trash re-anchor, or
set the tolerance to 0 for the measuring run.

### Falloff: softer severity, same shape, same archetype ordering

The curves were tuned for a small arena. The gym is now an open field whose
ranged enemy holds a 9-19 m band, so the ordinary fight happens past where the
old curves started biting. Only the severity moved; the mechanic and the
archetype spread are intact and `RiorsEdge.Weapons.ArchetypeFalloff` pins the
ordering (shotgun steepest, then SMG, rifle, sniper) rather than the values.

| Archetype | Start (cm) | End (cm) | Floor | Severity (lost per m) |
|---|---|---|---|---|
| Rifle | 2000 -> **2800** | 6000 -> **7000** | 0.55 -> **0.72** | 1.13% -> **0.67%** |
| SMG | 1200 -> **1800** | 3500 -> **4500** | 0.40 -> **0.58** | 2.61% -> **1.56%** |
| Sniper | 3500 -> **5000** | 9000 -> **11000** | 0.70 -> **0.88** | 0.55% -> **0.20%** |
| Shotgun | 800 -> **1100** | 2500 -> **2800** | 0.25 -> **0.40** | 4.41% -> **3.53%** |

Effective damage multiplier, old -> new:

| Range | Rifle | Shotgun | SMG | Sniper |
|---|---|---|---|---|
| 9 m | 1.00 -> 1.00 (0%) | 0.956 -> 1.00 (+4.6%) | 1.00 -> 1.00 (0%) | 1.00 -> 1.00 (0%) |
| 15 m | 1.00 -> 1.00 (0%) | 0.691 -> 0.859 (+24.3%) | 0.922 -> 1.00 (+8.5%) | 1.00 -> 1.00 (0%) |
| 19 m | 1.00 -> 1.00 (0%) | 0.515 -> 0.718 (+39.4%) | 0.817 -> 0.984 (+20.4%) | 1.00 -> 1.00 (0%) |
| 25 m | 0.944 -> 1.00 (+6.0%) | 0.250 -> 0.506 (+102%) | 0.661 -> 0.891 (+34.8%) | 1.00 -> 1.00 (0%) |
| 40 m | 0.775 -> 0.920 (+18.7%) | 0.250 -> 0.400 (+60%) | 0.400 -> 0.658 (+64.5%) | 1.00 -> 1.00 (0%) |
| 60 m | 0.550 -> 0.787 (+43.0%) | 0.250 -> 0.400 (+60%) | 0.400 -> 0.580 (+45%) | 0.918 -> 1.00 (+8.9%) |
| 90 m | 0.550 -> 0.720 (+30.9%) | — | — | 0.700 -> 0.920 (+31.4%) |

**The rifle's effective DPS is unchanged across the entire 9-19 m engagement
band** — it never fell off there — so a rifle-measured trash/elite TTK inside
20 m carries NO contamination from this change. The secondary is where the
band actually hurt.

### Hip fire: giving ADS a bill instead of buffing hip accuracy

ADS tightened aimed spread, recoil, bloom and viewmodel all at once and cost
nothing, so hip fire was simply the worse option at every range and there was
no decision. Hip fire's accuracy is deliberately NOT buffed — that would delete
the decision rather than create one. Instead ADS now pays twice:

- **Time.** `AimInSeconds` (rifle 0.20, SMG 0.14, shotgun 0.16, rocket 0.30,
  sniper 0.38). `FBreakerWeaponFeel::ProfileAtAimAlpha` interpolates every aim
  benefit from its hip value (1.0) toward the authored one, and the base cone
  lerps hip -> aimed alongside it. A round snapped off the instant the button
  goes down is a hip shot in every respect. **Releasing aim is instant**, so
  the fast-to-first-shot option is always one release away. The alpha rides on
  `FBreakerShotResult::AimAlpha` so remote machines reproduce a partial-ADS
  kick exactly.
- **Mobility.** `MoveSpreadDegrees` adds cone scaled by ground speed over
  `MoveSpreadReferenceSpeed` (600 cm/s, near walk rather than sprint, so
  "planted" means planted), and `AimMoveSpreadMultiplier` above 1.0 makes an
  aimed moving shot **wider** than a hip moving shot. Movement is not forgiven
  by first-shot accuracy: standing still is what buys the perfect shot.

The resulting decision, rifle, at walking speed: planted, ADS wins everything.
Moving, the first hip shot is 0.35 degrees against ADS's 0.77, so hip fire owns
the moving opener while ADS still wins sustained fire. Per archetype the sniper
must be planted (1.10 / 3.0x), the shotgun barely cares (0.25 / 2.0x), the SMG
is the one legitimate run-and-gun ADS weapon (0.30 / 1.8x).

### The ADS movement-speed penalty: CLOSED END TO END

The third item on the ADS bill is movement SPEED. It was recorded here as a
two-sided gap — one half a weapon-authoring question, the other a movement
question, owned by different layers. **Both halves are now built and the trade
is charged in play.**

**Side one — the weapon publishes the penalty. BUILT.**

- `FBreakerRecoilProfile::AimMoveSpeedMultiplier` (EditAnywhere, O2
  PLACEHOLDER) is the ground-speed scale while fully sighted, authored per
  archetype because "how much does sighting this weapon root you" is exactly
  the kind of thing that should separate an SMG from a sniper.
- `FBreakerWeaponFeel::AimMoveSpeedMultiplier(Profile, AimAlpha)` composes it
  against live ADS progress, so the penalty arrives at exactly the pace every
  other ADS benefit does — tapping aim and running does not bolt the player to
  the floor for a frame. It is **clamped to 1.0 at the top**: this is a penalty
  channel, and an archetype that authored 1.3 would make sighting a weapon
  *faster* than hip firing it, inverting the whole trade.
- `UBreakerWeaponComponent::GetAimMoveSpeedMultiplier()` is the query the
  movement layer consumes; `GetArchetypeAimMoveSpeedMultiplier()` is the
  authored fully-sighted value for UI and tuning.

| Archetype | Aimed speed | Why |
|---|---|---|
| Mark (sidearm) | 0.92 | A sidearm you cannot move with is not a sidearm |
| SMG | 0.88 | Stays the one legitimate run-and-gun ADS weapon |
| Shotgun | 0.85 | Strafing into contact is its whole job |
| Rifle | 0.72 | The baseline |
| Volley | 0.70 | Between the rifle and the sniper, like everything else about it |
| Rocket Launcher | 0.65 | Mass |
| Sniper | 0.50 | Scoped and moving is nearly standing still, which is the point |
| Bulwark | 0.45 | Most rooted in the table: its answer to being rushed is to keep firing |

Setting every one of these to 1.0 reproduces today's behaviour exactly, which
is the A/B that makes this safe to land before the other side exists.

**Side two — something has to READ it. BUILT.**
`UBreakerCharacterMovementComponent::GetAimSpeedMultiplier()` finds the owner's
weapon component **per call** (never cached, so a mid-movement weapon swap
cannot apply the outgoing archetype's penalty) and clamps the published value
to [0,1] a second time — two independent clamps on the same invariant, because
an archetype authored above 1.0 by mistake would turn aiming into a speed BUFF
and invert the whole trade rather than merely mistuning it.

It is applied to the **grounded speed cap only**. Sliding and the boosted-speed
ceiling deliberately have no opinion: whether an aimed slide is slowed is a
movement-feel ruling, not a weapon one. Note the one asymmetry that follows —
`TryRedirect`'s minimum-speed floor does **not** include the aim term, so while
fully sighted with a rooted archetype the grounded cap can sit below the speed a
redirect requires.

`RiorsEdge.Weapons.AimMoveSpeed` proves the ramp, the clamp, the 1.0-is-a-no-op
property and the archetype ordering; `RiorsEdge.Movement.AimSpeedPenalty` proves
the movement layer consumes it.

**Three code comments still say this is unbuilt** — in
`BreakerWeaponComponent.h`, `BreakerWeaponComponent.cpp` and
`BreakerWeaponFeel.h`. They are stale; the consumer and its test both exist.

All values above are O2 PLACEHOLDER and **none of this has been playtested**.
`RiorsEdge.Weapons.ArchetypeFalloff`, `.WeakPointTolerance` and `.HipFireTrade`
prove the maths and the archetype ordering; they prove nothing about feel.

## Round presentation, first pass (tracers, muzzle origin, impacts, rocket)

**Largely superseded** — read the second-pass section below for what actually
ships. The muzzle-origin accessor and the projectile-weapon exemption survive
intact; the canvas drawing, the 9 m streak, the six-spoke star and the rocket
build described here do not. Kept because the second pass is only legible next
to the trade this one recorded.

Owner report: "the bullet projectiles look a bit strange." Four causes were
real, one was not.

**Real.** (1) The streak was drawn at full length from trace start to impact on
the frame it was fired, so it read as a static beam, not a round in flight.
(2) It started at the CAMERA, because `FBreakerShotResult::TraceStart` is the
view point — at close range and while strafing the line came out of the middle
of the player's face. (3) Thickness was a constant 1.25 spec px whatever the
range, so an 80 m shot was as fat as a 2 m one. (4) It was gold, which
FIELDPLATE reserves for reward and weak points; weapon/heat is the orange
family. Two further faults were found while reading: the rocket launcher also
emitted a hitscan streak (`FireProjectile` fills a `FBreakerShotResult` too),
so every rocket was shadowed by a ghost round; and the impact cross was drawn
at the moment of firing rather than when the round arrived.

**Not real.** The 0.12 s fade was not the problem — the streak never moved, so
there was nothing for the fade to sell.

What it is now:

- `Source/RiorsEdge/UI/BreakerTracerMath.h` is a pure, testable header holding
  the flight maths. A shot is recorded once; the HUD replays it as a short
  segment (`LengthCm` 900) travelling muzzle-to-impact. Flight time is clamped
  into `[MinFlightSeconds 0.05, MaxFlightSeconds 0.22]` by trimming the
  effective speed, so a point-blank shotgun round still occupies ~3 frames and
  a cross-field sniper round does not float. All O2 PLACEHOLDER.
- Thickness comes from a world radius (`TracerRadiusCm` 2.2) projected at the
  round's own depth, clamped to [0.9, 5.0] spec px.
- Colour is `BreakerUI::Orange` over a wider `OrangeDeep` glow. Never teal.
- `UBreakerWeaponComponent::GetVisualMuzzleLocation()` is a new **presentation
  only** accessor: view point plus a camera-space `MuzzleViewOffset`
  (hip 95/18/-18 cm, ADS 95/2/-6 cm), matching the placeholder weapon assembly
  on `ABreakerCharacter`. **The trace still starts at the camera.** The feel
  layer's tested invariant — recoil moves the aim, the round follows the aim,
  the round lands on the crosshair — is untouched. Visual origin and trace
  origin differing is standard practice; they converge at the impact, which is
  the only place they must agree.
- The impact cross is gone. In its place is a six-spoke star drawn at a world
  radius in the plane perpendicular to the round's travel, so it sits on the
  surface and shrinks with distance. It starts when the round ARRIVES.
  `FBreakerShotResult` carries no impact normal and adding one would touch the
  shot contract a recoil layer just landed in, so travel direction stands in
  for the normal; for anything but a glancing shot the two are within a few
  degrees.
- Projectile weapons no longer record a HUD tracer at all.

**Depth trade-off, and how it was paid.** The first pass kept the tracer on
the HUD canvas and wrote the trade down: a canvas line does not depth-sort, so
it composites over whatever is in front of it, and the stated fix if that ever
showed was to move the layer into the world. It showed.

## Round presentation, second pass (world-space tracers)

Owner report on the first pass: "projectiles are ugly and weird." The
parameters from the first pass were mostly right; the APPROACH was not.

**Diagnosis.** A canvas stroke composites over the world, so a round could draw
in front of the pillar it should have been behind; it held a screen-space
width, so it never foreshortened when you shot along it; and at 9 m long it was
not a round at all, it was a rod. Three more things were wrong on their own
terms: every single round left a streak, so held automatic fire stacked two and
a half overlapping streaks at all times and read as one continuous beam; the
impact was a six-spoke star, which is a symbol rather than an event; and the
rocket was five bright shapes rolling at 540 deg/s, which is a spinning toy.

What it is now:

- **The round is a world primitive.** `Source/RiorsEdge/UI/BreakerTracerRenderer.{h,cpp}`
  is a client-side, non-replicated, lazily-spawned actor holding a fixed pool
  of `UStaticMeshComponent`s: 12 tracers x (head + trail) and 24 impact sparks,
  all `CreateDefaultSubobject`ed once. Nothing is spawned or destroyed per
  bullet. Slots recycle round-robin.
- **It depth-sorts.** The material is `/Engine/EngineMaterials/EmissiveMeshMaterial`
  — unlit, additive. Additive translucency still depth-TESTS against the opaque
  scene, so a wall occludes a round correctly, and unlit means the round reads
  as light rather than as a painted stick. Its parameters were MEASURED, not
  guessed (a throwaway automation probe over `GetAll*ParameterInfo`): vector
  `Color`, texture `LinearColor`. That texture defaults to a WHITE GRID, so
  `Source/RiorsEdge/UI/BreakerGlowMaterial.h` overrides it with
  `WhiteSquareTexture`; without that override every tracer would have graph
  paper printed on it. Additive surfaces have no alpha, so a fade is a multiply
  toward black.
- **Shape: a bright dash with a faint trail, not a rod.** `LengthCm` 900 -> 240,
  split into a `HeadLengthCm` 55 bright head (`BreakerUI::Orange`, intensity
  3.2) and a thinner, dimmer trail (`OrangeDeep`, 0.55). Two primitives,
  because one stretched box cannot be bright at one end without a material that
  does not exist yet. `SpeedCms` 26000 -> 30000, flight band
  [0.045, 0.20], `MinimumTravelCm` 60 -> 120.
- **World thickness with a screen floor.** `TracerThicknessCm` keeps the
  authored 2.6 cm up close and widens the round in world space only as far as
  needed to hold ~1.4 px at 1080p, so a round at 80 m does not strobe out of
  existence between frames. `WorldRadiusToPixels` is gone with the canvas.
- **Not every round traces.** `TracerRoundsPerTracer` returns 3 above 300 RPM
  and 1 below it: rifle/SMG trace one round in three, sniper and other slow
  weapons trace every round (a bolt-action skipping two shots in three would
  read as broken, not restrained). Round 0 always traces, so the first round of
  an engagement is the one that teaches where the gun points.
- **The impact is a point flash.** The six-spoke star is gone. It is now a
  small additive sphere that pops to `ImpactRadiusCm` 13 and collapses over
  `ImpactSeconds` 0.10 — decay, not growth, because a bullet strike is not a
  shockwave. Having no orientation, it cannot be oriented wrongly, which was
  the star's whole problem given that `FBreakerShotResult` carries no impact
  normal. The flash fires on EVERY hit, traced or not: hit confirmation is
  feedback, tracer density is decoration.
- **Pellet weapons got no streak.** `FBreakerShotResult` carried one impact
  point for a whole spread, so the shotgun previously drew a single line for
  eight pellets — a lie about where they went, and its own inconsistency. It
  got the impact flash only. **CLOSED by the per-pellet pass below**; the
  section is left as written because the trade it records is why that pass
  exists.
- Projectile weapons still record no tracer at all.

`ABreakerRocketProjectile` was rebuilt on the same honesty. The old build had
an orange casing, a cone nose, a cross of fins, a sphere on the back, and a
540 deg/s roll — the body was the same colour as its own flame so nothing
separated object from thrust, fins on a 50 cm primitive at 3000 cm/s are never
resolved by the eye, and a rolling warhead is not a thing. Now: a DARK body
(`Panel20` casing, `BorderEmphasis` nose) carrying the only bright element, an
unlit additive flame stretched along -X that grows backward from the nozzle.
Fins and roll deleted. The single animation is an `ExhaustFlickerHz` 26 /
`ExhaustFlickerAmount` 0.22 two-term flicker of the flame's length and
brightness, phased off world time so two rockets are never in sync. The point
light drops 3200/420 cm -> 2000/340 cm. Detonation is unchanged in structure:
collision and movement off, casing hidden, the flame scaled uniform to two
thirds of the damage radius at `FireballIntensity` 6, the light flared, actor
dead `ExplosionFlashSeconds` later. No second actor, no pool, clients see it
through the existing cosmetic multicast.

**Knobs.** `BreakerHUD::FTracerFlight` (speed, streak length, head length,
flight band, minimum travel) and `BreakerHUD::FTracerLook` (thickness, trail
thickness scale, screen-width floor, head/trail/impact intensities, impact
seconds and radius) in `BreakerTracerMath.h`; `TracerRoundsPerTracer`'s 300 RPM
threshold and 3-round cadence in the same header; pool sizes in
`BreakerTracerRenderer.h`; `ExhaustLengthCm` / `ExhaustFlickerAmount` /
`ExhaustFlickerHz` / `ExplosionFlashSeconds` as EditAnywhere on the rocket.
Every one of them is O2 PLACEHOLDER.

Nothing in this section has been PLAYTESTED, and this is the second attempt at
a purely visual complaint, so say it plainly: automation cannot see the screen.
`RiorsEdge.Weapons.TracerFlight` was updated rather than trimmed — it lost the
`ImpactBasis` and `WorldRadiusToPixels` assertions because those functions no
longer exist, and gained coverage of the head/trail split, the shortened
streak, the screen-width floor, and tracer cadence. It proves none of these
things look good.

## Per-pellet impacts, and how a spread shares the tracer pool

### The contract change, and why it is additive

`FBreakerShotResult` carried ONE impact for a whole spread, so the shot
contract could not answer "where did the shotgun actually land". Two
consequences were recorded in the code: the tracer renderer had nothing
per-pellet to draw, so the shotgun deliberately drew no streak at all; and any
consumer wanting a per-pellet reading had to guess.

It now carries `TArray<FBreakerPelletImpact> Pellets` — one entry per pellet of
the trigger pull, **in fire order, hits and misses alike**. A missed pellet is
still recorded, because where a pellet went when it missed is exactly the
information a cone is made of. A single-projectile weapon records exactly one
entry, so no consumer needs an "is this a shotgun" branch; a projectile weapon
records none, because it put a real actor in the world instead.

**Back compatibility is the point of the design, not a footnote.** Every
pre-existing field keeps its exact previous semantics:

| Field | Meaning, unchanged | Equivalent in the new record |
|---|---|---|
| `bHit` | any pellet landed | `GetLandedPelletCount() > 0` |
| `bWeakPoint` | OR across the spread | OR of `Pellets[i].bWeakPoint` |
| `ImpactPoint` / `TraceEnd` / `HitActor` | the LAST pellet that landed | last `Pellets[i]` with `bHit` |
| `DamageResult` | the spread's summed damage | unchanged, still summed in the loop |

So the HUD damage numbers, the Mana component's per-shot generation and the
playtest telemetry are untouched by this change, and a replicated shot from a
build without the array behaves exactly like a projectile shot.
`RiorsEdge.Weapons.PerPelletImpacts` states each of those equations as a test.
The per-pellet record is filled BEFORE each trace and completed after, so every
path through the loop — miss, no combat component, early continue — still
leaves exactly one entry per pellet. A spread with a hole in it would silently
drop a tracer, which is the failure this whole change exists to remove.

### How a spread shares a fixed pool

`ABreakerTracerRenderer` is a fixed pool: **12 tracer slots** and 24 impact
sparks, `CreateDefaultSubobject`ed once, recycled oldest-first. Round-robin
eviction is graceful for single rounds and is NOT graceful for a spread — half
a cone vanishing mid-flight is worse than no cone — and a definition may author
up to 32 pellets, so "one streak per pellet" would consume the pool in a single
trigger pull.

**The policy: a per-spread budget, an even subsample, and thinner streaks.**

- `MaxSpreadStreaks` = **4** slots of 12. At least two whole spreads fit at
  once and no spread can ever evict itself. At the shotgun's 85 RPM (0.7 s
  between shells) against the 0.20 s flight ceiling, two is already unreachable
  in practice.
- The subsample is **even and inclusive of both ends**, so streak 0 is always
  pellet 0 and the last streak is always the last pellet: the drawn cone is
  exactly as wide as the real one and never narrower. Indices are strictly
  increasing, so no slot is wasted redrawing a pellet.
- `SpreadThicknessScale` = **0.55**. Four full-thickness streaks read as four
  rifle rounds fired at once; thinner sub-streaks read as shot. The scale is
  applied to the *authored* thickness, so the screen-width floor still holds
  and the far half of a cone cannot strobe out of existence.
- **Impact flashes follow the pellets that actually landed**, not the streak
  subsample, because hit confirmation is feedback the player acts on while
  tracer density is decoration. Budgeted at `MaxSpreadSparks` = 8 of 24, so a
  32-pellet definition cannot wrap the spark pool inside one trigger pull.
- **Spreads are exempt from the tracer cadence.** A shell is one event;
  skipping two shells in three would read as the gun misfiring.

The selection maths is pure and header-inline (`BreakerHUD::SpreadStreakCount`
/ `SpreadStreakPellet` in `BreakerTracerRenderer.h`), because whether the pool
overflows is arithmetic and is therefore the one part of a visual change
automation can genuinely prove. `RiorsEdge.Weapons.TracerSpreadPool` walks
every legal pellet count 1-32 and asserts the budget holds, every streak
indexes a real pellet, no two streaks share one, and both ends are always
drawn.

Every value above is O2 PLACEHOLDER. **None of it has been playtested, and
automation cannot see whether a spread of four thin streaks actually reads as a
shotgun blast on screen** — that exact blind spot has already let two visual
passes ship.

## Base damage scales with item level [O27, Power-Curve.md §3]

Until this pass `Source/RiorsEdge/Weapons/` contained **no reference to
`ItemLevel` at all**. Base damage was an archetype constant, so an item level 1
weapon and an item level 50 weapon hit identically and item level moved only
affix tier values. That is the multiplicand every affix, node and crit
multiplies, and it is the single largest reason full level 50 gear did not feel
significant.

The curve, implemented as a pure function beside the rest of the weapon maths
(`FBreakerWeaponMath::ItemLevelDamageScalar` / `WeaponBaseDamage`):

    WeaponBase(ilvl) = ArchetypeBase * (1 + w)^(ilvl - 1)

- **`w` = 0.09 per level** (`UBreakerWeaponComponent::ItemLevelDamageGrowth`,
  EditAnywhere, O2 PLACEHOLDER). It is chosen equal to the monster health
  growth `g` from the same document, which the Combat layer owns. With `w == g`
  a BASELINE build's shots-to-kill is level-invariant, so the base curve keeps
  the game playable at every level and every bit of *felt* progression comes
  from the multiplier band. If `w < g` the content outruns the player and
  baseline TTK climbs; if `w > g` baseline TTK falls with level and the build
  layers have nothing left to add. `RiorsEdge.Weapons.ItemLevelTracksMonsterHealth`
  states all three cases so a retune of either curve cannot drift silently.
- **Item level 1 is the anchor.** The scalar is exactly 1.0 there, so every
  authored archetype number keeps meaning exactly what it meant before and no
  previously measured TTK moves.
- **One shared exponent, so the archetype table keeps its shape.** A sniper
  out-hits an SMG per shot at level 1 and at level 50 by the same ratio;
  `RiorsEdge.Weapons.ArchetypeOrderingAcrossLevels` pins it at every level.

**Every damage path uses it**, not only hitscan: the pellet loop (per pellet,
before falloff), the rocket projectile's payload, and the Bleed DoT's base per
tick. The item level is resolved ONCE per trigger pull and passed down, so a
shotgun blast cannot straddle an equipment change.

### How item level reaches the weapon

`UBreakerWeaponComponent::GetEquippedItemLevel()` reads the owner's
`UBreakerEquipmentComponent` through its existing const `GetEquippedItem`
accessor — no change to `Items/` was required. Weapon loadout slot 1 reads the
`Primary` equipment slot and slot 2 reads `Secondary`.

**That correspondence used to be the ONLY link the two layers had**, and the
archetype question — *which of the eight guns is this Primary item?* — was left
open here as a design question. **It is answered.**
`FBreakerItemInstance::WeaponArchetype` is drawn on a weapon drop before its
affixes (from the same deterministic stream, uniformly across archetypes, so a
seed still reproduces an item exactly), and
`UBreakerWeaponComponent::SyncArchetypesToEquipment` — bound to
`OnEquipmentChanged` — arms it. The loadout screen still works; an equipped item
simply overrides it. Weapon drops also carry per-archetype affix leans, which
are weights and never filters; the table is in `Docs/Item-Foundation.md`.

**The item level ceiling is 120** (`FBreakerWeaponMath::MaxSupportedItemLevel`),
and it must equal `UBreakerAffixLibrary::MaxItemLevel` — `RiorsEdge.Items
.TierLadder` pins that they agree, because a weapon clamping lower than the item
system rolls would cap base damage while the affixes on the same item kept
climbing. Note that no enemy currently drops above item level 50; that clamp is
on `ABreakerEnemy` and is recorded in `Docs/Design/Power-Curve.md`.

### An unequipped weapon is item level 1

`UnequippedItemLevel` (EditAnywhere, O2 PLACEHOLDER 1) is what a weapon with no
item in its slot represents. 1 is deliberate: the scalar is then exactly 1.0,
so the zero-setup convention — a clean clone with no loadout firing the
code-driven fallback archetypes — plays on the archetype numbers precisely as
authored. Any other choice would silently rebalance the gym and every TTK
measurement taken in it.

### What this pass did NOT change

The multiplier layers are untouched: the single additive Increased bucket, the
More product and crit all still compose in `UBreakerAttributeSet` and the
damage library exactly as before. Fire rate, magazine and reserve were also
unchanged by it — fire rate became a real stat in a later pass, below.

## Fire rate is a real stat with a live consumer

`Weapon.FireRate` (the affix the SMG leans toward, per the owner's "smg fire
rate") is not a card number. The chain is:

    EBreakerStatTarget::FireRate
      -> EBreakerAggregatedAttribute::FireRateMultiplier   (base 1.0, replicated)
      -> UBreakerWeaponComponent::GetFireRateMultiplier()   (floored at 0.05)
      -> GetEffectiveRoundsPerMinute(Definition)

**Every fire-timing call site runs through `GetEffectiveRoundsPerMinute`** —
the automatic timer, the burst chain's in-burst interval, its end-of-burst
interval, and `CanFire`'s cadence gate. That last part is the whole point: a
cadence stat that applied to some timing sites and not others is how a weapon
ends up firing faster while its burst gap stays at the old rate.

Two deliberate exclusions:

- **`BurstCycleSeconds` is not scaled.** The end-of-burst wait is
  `max(BurstCycleSeconds, FireInterval(effective RPM))`, so fire rate can shrink
  the in-burst interval and never the cycle gap. The Burst Rifle's identity is a
  cadence you cannot shorten.
- **The multiplier is floored at 0.05** because a zero turns the fire interval
  into an infinity and hangs the weapon.

One thing does read the raw, unmultiplied RPM: the tracer cadence
(`TracerRoundsPerTracer` in the HUD). That is cosmetic — a fire-rate affix does
not change how many rounds leave a visible streak.

It rolls on the two weapon slots only, because fire rate is a property of the
gun. The **CADENCE** legendary bends this: half of Fire Rate also becomes
Increased Damage, which is the first reason in the game to stack cadence past
the point the gun already feels fast.

## Runtime flow

1. Input starts or stops the trigger on the weapon component.
2. The server validates cadence, ammunition, and reload state.
3. A deterministic cone direction is generated from the shot sequence.
4. A dedicated `WeaponTrace` channel performs the hit test.
5. Components tagged `WeakPoint` set the weak-point flag.
6. Range falloff and source GAS attributes build a shared damage request.
7. The target's combat component resolves armour, shields, health, and death.
8. A multicast cosmetic event provides trace/impact data to Blueprint presentation.

The current server accepts the owning controller's view point. Before competitive networking, add rewind/lag compensation and stricter view validation. Solo remains the primary development target.

## Data boundary

`UBreakerWeaponDefinition` owns immutable base content. A future generated item instance will reference a definition and carry rolled affixes separately. Do not mutate definition assets at runtime and do not encode affix rolls into Blueprint subclasses.

## Target dummy

`ABreakerTargetDummy` includes GAS attributes, combat resolution, a body hitbox, a tagged weak-point sphere, replicated damage state, and delayed cleanup after death. A Blueprint child can add meshes, damage numbers, reset behavior, and presentation without changing combat rules.

## FOR THE OWNER — open in this layer (2026-08-14)

1. **The Rocket Launcher is not on the shared projectile base.**
   `Combat/BreakerProjectileBase` was built to be the one reusable replicated
   projectile and `ABreakerRocketProjectile` still derives from `AActor`,
   duplicating collision, movement, lifetime and multicast plumbing, and
   missing the carried-status list. The base's own header says the fold was
   deferred because the explosion and the detonation-flash lifetime would make
   it a behaviour change disguised as a refactor. Either it folds (and the
   explosion is re-expressed deliberately) or the base stops claiming to be the
   one projectile.
2. **The rocket's blast query is a full-world actor scan.** `Explode` calls
   `GetAllActorsOfClass(AActor::StaticClass())` and filters by distance, rather
   than an overlap query, and measures to actor origins rather than to the
   nearest point of a collider. Correct today at gym scale; it is the shape that
   does not survive a real level.
3. **Two archetypes inherit values they probably should author**: the Rocket
   Launcher's falloff curve and the Shotgun's swap-in duration are both the
   rifle's, unmarked. Swap tempo is an explicit axis of the table, so the
   shotgun's is the one worth a decision.
4. **O13's rocket self-damage design is unimplemented.** The rocket skips its
   own instigator entirely — no self-damage, no self-knockback. O13 rules
   "strong self-damage reduction, full self-knockback control, never immunity",
   and immunity is what ships.
5. **`ArmorPenetration` is authored 0 on all eight archetypes.** The field is
   consumed (as a flat subtraction inside the mitigation curve), so it works;
   nothing uses it. Either it becomes an archetype axis or a stat, or it is
   noise on the definition.
