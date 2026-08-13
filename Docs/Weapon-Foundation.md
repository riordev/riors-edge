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
- **Pellet weapons get no streak.** `FBreakerShotResult` carries one impact
  point for a whole spread, so the shotgun previously drew a single line for
  eight pellets — a lie about where they went, and its own inconsistency. It
  now gets the impact flash only. **Known gap, deliberately not forced from the
  HUD:** doing this properly needs `FBreakerShotResult` to carry per-pellet
  impacts, which is the weapon layer's contract to change.
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
