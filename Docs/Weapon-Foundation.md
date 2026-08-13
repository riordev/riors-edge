# Weapon foundation

The first weapon pipeline is a server-authoritative hitscan prototype with a Data Asset definition and a built-in fallback rifle so clean clones can fire without editor-authored content.

## Playable archetypes

The player carries two weapons. Keys 1-2 select the equipped primary and secondary slots; the current gym loadout is rifle / scattergun. All three clean-clone fallback definitions remain available to the future loadout and pickup systems, but only two may be equipped at once. These remain prototype values rather than balance commitments.

### Rifle

- 24 base physical damage.
- 600 rounds per minute, automatic.
- 30-round magazine and 120 reserve.
- 1.8-second reload.
- 1.2-degree hip spread and 0.25-degree aimed spread.
- 1.75x weak-point multiplier.
- Full damage through 20m, linear falloff to 55% at 60m.
- 120m maximum trace range.

### Scattergun

- Eight deterministic pellets at 10 base physical damage each.
- 85 rounds per minute, semi-automatic.
- Eight-round magazine and 40 reserve.
- Wide close-range spread and aggressive falloff after 8m.

### Marksman

- 72 base physical damage and 2.0x weak-point multiplier.
- 150 rounds per minute, semi-automatic.
- Eight-round magazine and 40 reserve.
- Very tight aimed spread and strong long-range retention.

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

Feel is not verifiable by automation. The tests prove the maths — accumulation,
clamping, monotone recovery to exactly zero, compensation credit, the ADS
difference, bloom growth and decay, spring return to rest — and prove nothing
about whether it feels good.

## Round presentation (tracers, muzzle origin, impacts, rocket)

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

**Depth trade-off.** The tracer stayed on the HUD canvas, so it does not
depth-sort: a streak composites over whatever is in front of it. This is safe
here and only here, because every point on the streak lies on a line from
(almost) the camera to a point the camera's own trace reached unobstructed —
the muzzle is ~20 cm off the camera, so only geometry inside that sliver can
wrongly occlude. The alternative (pooled world primitives) buys correct sorting
at the cost of a pool, a material, and a per-frame transform update for a thing
that lives 0.2 s. If a streak is ever seen through a wall, that is the trade
being paid, and the fix is to move the layer into the world, not to tweak it.

`ABreakerRocketProjectile` got the matching pass: casing, nose cone, two fins
and an emissive exhaust bloom from `/Engine/BasicShapes` primitives with
dynamic material instances, coloured from the same orange tokens, plus a point
light and a roll on the warhead. On detonation the rocket becomes its own
explosion — collision and movement off, casing hidden, the exhaust bloom scaled
to two thirds of the damage radius, the light flared — and dies
`ExplosionFlashSeconds` later. No second actor, no pool, and clients see it
through the existing cosmetic multicast.

Nothing in this section has been PLAYTESTED. `RiorsEdge.Weapons.TracerFlight`
proves the flight maths, the basis construction and the inverse-depth
thickness; it proves nothing about whether a round now looks like a round.

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
