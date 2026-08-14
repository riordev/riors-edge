# Movement design

## Product intent

Movement gives players expressive positioning, recovery, and route choice without becoming the entire challenge or invalidating weapons, enemies, and arenas. A player using ordinary run, sprint, jump, and cover should remain viable. Advanced movement rewards awareness without being mandatory for baseline combat effectiveness.

## Current baseline

- Walk: 700 cm/s.
- Sprint: 1100 cm/s.
- Jump: 700 cm/s with 1.60 gravity scale, an asymmetric fall curve, a jump cut on release, and a terminal velocity — see "Weight pass" below. Jumping from a slide always performs a normal jump—never a mantle—while preserving the exact horizontal slide velocity and returning to the sprinting air state.
- Air control: moderate CMC control plus a Source-inspired steering assist that rotates existing horizontal momentum toward input without adding speed or permitting free reversals.
- Dash: available on ground or in air whenever the player is not sliding, with a four-second cooldown. It redirects momentum with a 1500 cm/s floor plus a 200 cm/s bonus and preserves earned speed while movement input continues. Releasing movement or colliding clears the boosted ceiling; a 4200 cm/s safety cap remains.
- Slide: available from 550 cm/s, eases its small deterministic entry push across 0.35 seconds, gives that boost at most once per 1.2 seconds, and never uses the entry boost to exceed sprint speed plus 120 cm/s. This prevents crouch-spam speed generation while allowing downhill momentum. It carries momentum and ends after one second or when released/slowed; holding slide while airborne queues one slide for landing; downhill surfaces add restrained acceleration.
- Wall ride: implemented baseline with 0.85-second maximum, minimum **450 cm/s** (was 700 — see "Wall ride was dead" below), reduced gravity, no passive speed gain, loss-of-contact exit, and a controlled wall jump. The wall jump keeps its own 700 cm/s exit floor and hands back one air jump.
- Mantle: pressing jump at a clear 35-150 cm ledge smoothly lifts the capsule over it in 0.20 seconds; tall walls and obstructed landing space reject the attempt.
- Grapple: excluded.

All numbers are initial editor-test values, not promises. Tune them in the context of aiming, incoming attacks, encounter distances, and readable enemy behavior.

## Weight pass (owner report: "movement should be less floaty")

The character hung in the air. The response is mass, not new verbs: no verb
was added, removed, or re-scoped, and dash, slide, wall ride and the redirect
hook are untouched. Every value below is `EditAnywhere` on
`UBreakerCharacterMovementComponent` under the **Weight** category (or under
the engine's own Character Movement categories for the two inherited ones), so
the whole pass can be A/B'd and reverted in the editor without a rebuild.

| Value | Old | New | Why |
|---|---:|---:|---|
| `GravityScale` | 1.35 | **1.60** | Single strongest lever. Shortens the rise and lowers the apex at the same time. |
| `FallGravityMultiplier` | — (1.0) | **1.55** | A symmetric arc is the classic floaty read; a real-feeling jump falls faster than it rises. Was 1.80; eased once — see "Gravity, fourth report" below. |
| `ApexGravityMultiplier` | — (1.0) | **1.50** | Time spent near zero vertical velocity is felt directly as hang. Blended into 1.0 upward and into the fall multiplier downward, so the curve is continuous. |
| `ApexBandSpeed` | — | **220 cm/s** | Half-width of the apex band. |
| `MaxFallSpeed` | — (volume 4000) | **2400 cm/s** | Terminal velocity, so the heavier fall does not turn a long drop into a bullet. |
| `JumpCutMultiplier` | — (1.0) | **0.55** | Variable jump height: releasing jump mid-rise scales the remaining rise. Authority over the arc reads as control rather than drift. |
| `JumpCutMinimumRiseSpeed` | — | **50 cm/s** | Below this a cut is not worth the discontinuity. |
| `JumpHoldWindow` | — (0) | **0.60 s** | Written onto `ACharacter::JumpMaxHoldTime` at BeginPlay purely so the movement layer can see a key release; the engine's own hold-to-rise is suppressed in `DoJump`. Must exceed the ~0.45 s rise. |
| `LandingHeavyFallSpeed` | — | **950 cm/s** | Landing threshold. A full-height jump lands at ~937 cm/s, so routine jumping is never taxed. |
| `LandingMaxFallSpeed` / `LandingMinimumSpeedScale` | — | **2400 / 0.78** | Real falls cost horizontal speed on arrival, ramped, so an arrival has consequence instead of being weightless. |
| `BrakingDecelerationWalking` | 1800 | **2400** | Floaty is often really "slow to stop" rather than anything airborne. |
| `GroundFriction` | 7.5 | **8.5** | Same reason: a released stick plants instead of skating. |
| `JumpZVelocity` | 700 | *unchanged* | Gravity already lowers the apex ~16%; cutting the impulse too would compound past 25% and could strand authored ledges and wall-ride approaches. |
| `AirControl` / `AirSteerRate` | 0.55 / 4.2 | *unchanged* | Already restrained per the Godot audit. Reducing air authority reads as *more* drift, not less — and `AirSteerRate` is the gear/tree air-control consumption point. |
| `MaxAcceleration` | 4200 | *unchanged* | Already twice the engine default; more reads as twitchy, not heavy. |

Resulting arc, flat ground, no gear or tree multipliers:

| | Old | New |
|---|---:|---:|
| Rise time | 0.53 s | 0.45 s |
| Apex height | 185 cm | 156 cm |
| Fall time | 0.53 s | 0.34 s |
| Total airtime | 1.06 s | 0.79 s |
| Landing speed | 700 cm/s | ~937 cm/s |

Exemptions, all deliberate:

- **Wall ride** is exempt from the fall curve entirely (`NewFallVelocity`
  returns early while riding). It keeps its own `WallRideGravityScale` and its
  duration cap, so it stays short, situational and speed-neutral.
- **Dash** clears the jump cut when it fires, so an air dash's vertical floor
  is never cut. Slide jumps and wall jumps never arm the cut at all (only a
  real `DoJump` arms it), so their impulses are fixed by design.
- **A queued slide owns its landing**: the landing speed cost is skipped when a
  slide is queued or running, or it would push the player under
  `SlideEntrySpeed` and silently eat the slide.

The curve, the terminal velocity, the cut and the landing ramp are pure static
functions on the component and are covered by `RiorsEdge.Movement.Weight`.

To revert to exactly the old feel: `GravityScale` 1.35, `FallGravityMultiplier`
1.0, `ApexGravityMultiplier` 1.0, `MaxFallSpeed` 0, `JumpCutMultiplier` 1.0,
`JumpHoldWindow` 0, `LandingMinimumSpeedScale` 1.0, `BrakingDecelerationWalking`
1800, `GroundFriction` 7.5.

Tuning order if it is still wrong: **still floaty** → raise
`FallGravityMultiplier` first, then `GravityScale`, then lower `JumpZVelocity`.
**Too heavy** → lower `GravityScale` first, then `FallGravityMultiplier`, then
raise `LandingMinimumSpeedScale` toward 1.0. Both of the first two steps of the
"too heavy" branch have now been taken, in that order, one per report:
`GravityScale` 1.60 → 1.45 → 1.38, then `FallGravityMultiplier` 1.80 → 1.55.
**Exactly one value moves per report**, which is what makes the next report
attributable.

None of this has been playtested. It is verified by automation and by the
arithmetic above only; floatiness is a feeling and the numbers cannot confirm
it landed.

## Gravity, fourth report (owner: "gravity needs to be tuned down just a little bit... needs to make the character slightly more floaty")

Four reports now, and they are not contradictory — they are about **different
halves of the arc**. 1.35 read floaty. 1.60 and 1.45 both read too heavy, and
the heaviness is the RISE, which is paid on every single jump. `GravityScale`
was therefore walked back to 1.38, a hair above the 1.35 the project started
at, and left there. The DESCENT was never eased: it still ran at 1.80x on top
of that rise.

So this report moves the descent. **`FallGravityMultiplier` 1.80 → 1.55, and
nothing else** — the tuning order below named this dial for exactly this
report, and O26 makes this an owner request executed, not a movement pass
opened. Moving `GravityScale` as well would put the rise under its original
value and make the fifth report unattributable.

| | Before (1.80) | After (1.55) |
|---|---:|---:|
| Rise time | 0.518 s | **0.518 s** (untouched) |
| Apex height | 181 cm | **181 cm** (untouched) |
| Fall time | 0.386 s | **0.416 s** |
| Total airtime | 0.903 s | **0.933 s** |
| Landing speed | 939 cm/s | **871 cm/s** |

Apex is untouched **by construction**: nothing on the rise moved, so no ledge,
gap or wall-ride approach in the field changes reach. The whole delta is +7.8%
descent time, +3.3% airtime, and a landing that arrives 68 cm/s softer — still
under `LandingHeavyFallSpeed` (950), so routine jumping is still untaxed, now
with more margin. `ApexGravityMultiplier` stays 1.50, which keeps the invariant
that the apex is never heavier than a settled fall.

Arithmetic uses the same flat model as the weight-pass table (`g = 980 ×
GravityScale`, apex band ignored), so the two are comparable.

**If it is still too heavy, the next dial is `LandingMinimumSpeedScale` → 1.0**,
which deletes the landing speed cost outright. After that, `GravityScale`.
Nobody has playtested this; the numbers cannot say whether it feels right.

## Wall ride was dead (owner: "wall riding doesnt work but jumping does and its awkward")

**Root cause.** `WallRideMinimumSpeed` was 700 cm/s — *exactly* `WalkSpeed`, which
is the hard airborne horizontal ceiling whenever the sprint toggle is off. The
gate is read in `TickComponent` **after** the capsule has already touched the
wall, and UE's `PhysFalling` rewrites horizontal velocity from the wall-slid
displacement on a blocking hit, so what the gate actually measures is the
*along-wall* component that survives contact, not the approach speed:

| Approach | Speed | Along-wall at 30° | at 45° | at 60° |
|---|---:|---:|---:|---:|
| Walking | 700 | 606 | 495 | 350 |
| Sprinting | 1100 | 953 | 778 | 550 |

Against a 700 gate a walking player could **never** enter — every row is below
it — and a sprinting player lost it past roughly a 50° approach. Every other
entry gate in the component sits strictly *below* the speed it gates (slide
enters at 550, under the 700 walk speed); the wall ride was the one set at
100% of its own ceiling.

**Why the wall JUMP also disappeared.** `TryWallJump()` returns false unless
`bWallRiding` is already true, and `ABreakerCharacter::HandleJumpInput` tries it
first. With the ride dead there is no other path to a wall jump at all, so what
the owner felt next to a wall was `ACharacter::Jump`'s plain second jump (O25
base kit) — a vertical hop with no wall push. That is exactly the "awkward".

**The fix, split by kind:**

| Change | Kind | Why |
|---|---|---|
| `WallRideMinimumSpeed` 700 → **450** | bug fix | Restores the invariant that an entry gate sits below the speed it gates, and sizes it for post-contact along-wall speed. |
| Entry rule extracted to the pure static `CanBeginWallRide(...)` | bug fix | The verb broke silently once; the rule now has a name and a regression test. |
| `WallRideJumpMinimumSpeed` = **700**, new | bug fix | The wall jump used to borrow the entry gate as its exit floor, so lowering the gate would have silently weakened every wall jump. Two jobs, two values. |
| `bWallJumpRefreshesAirJump` = true | **feel** | Wall jump and the O25 second jump share one key and one budget; both jumps were usually spent by the time a player reached a wall, so a wall jump threw them off with nothing left. The count is *clamped* to `JumpMaxCount - 1`, never cleared, so the baseline is still two and wall-to-wall traversal chains instead of stranding. Set false to restore the old behaviour. |

Deliberately **not** touched: gravity, the fall curve, jump impulse, air control,
air steer rate, the landing cost, the jump-hold window, `WallRideMaxDuration`,
`WallRideGravityScale`, the trace, and the speed-neutrality rule. The weight pass
shortened total airtime from 1.06 s to ~0.79 s, which narrows the window in which
a ride can start, but it is not the blocker and O26 puts it out of scope.

Covered by `RiorsEdge.Movement.WallRideEntry`.

## Dash feedback (owner: "cant really feel it or see it since your speed just jumps up")

The dash rule is correct and unreadable. A dash is instantaneous and its entire
effect is a velocity change; in first person over open ground there is almost no
optical flow to read it from. **No dash value was changed** — the fix is
presentation, added the same way the landing weight already does it.

- `UBreakerCharacterMovementComponent::OnDashStarted(FVector DashDirection, float DashSpeed)`
  — a `BlueprintAssignable` delegate broadcast from `TryDash` after the velocity
  is committed. Same shape and same contract as `OnLandingImpact`: presentation
  binds to it, C++ stays ignorant of cameras, and any listener can be replaced
  without touching movement.
- `ABreakerCharacter` consumes it with two camera cues, both `EditAnywhere` under
  **Camera|Dash Feedback**, both honouring the guardrail below that camera roll
  and FOV changes stay subtle and configurable:
  - **FOV punch** (`DashFOVPunch` 12°, `DashFOVPunchAttack` 0.05 s,
    `DashFOVPunchRecovery` 0.30 s, quadratic ease-out). The genre-standard speed
    cue: a wider frustum multiplies the peripheral motion the eye uses to judge
    speed. Answers *how much*. Scaled by actual dash speed against
    `DashFeedbackReferenceSpeed` (1700) and clamped to
    `DashFeedbackMinimumScale`/`MaximumScale`, so a dash that carried momentum in
    reads harder than a standing one.
  - **Camera roll** (`DashCameraRoll` 5°, signed by `dot(dashDir, actorRight)`).
    Answers *which way* — a forward dash rolls not at all, a strafe dash rolls
    fully. FOV alone cannot carry direction. Applied through the controller's
    control-rotation roll, because the camera runs `bUsePawnControlRotation` and
    re-derives its world rotation every frame, which would discard a relative
    roll. Roll about the view axis leaves aim, and therefore every weapon trace,
    untouched; `bUseControllerRotationRoll` is false so the capsule never tilts.
- The player's FOV setting now lives in `BaseFieldOfView` and the punch is a pure
  offset on top of it. `GetCurrentFOV()` reports the *setting*, so a punch in
  flight can never be read back by the settings screen or persisted by
  `SavePlaytestSettings`. A playtest reset drops any punch in flight.

**What a HUD would need to finish this** (UI/ is owned elsewhere and was not
touched): bind `OnDashStarted` for the event, and read
`ABreakerCharacter::GetDashFeedbackAlpha()` (0 at rest, 1 at the punch peak,
decaying) and `GetLastDashDirection()` to drive a radial speed-line burst — the
existing `DrawSkimBurst` in `ABreakerPlaytestHUD` is already the right visual
vocabulary and already the right shape. Riding the character's envelope instead
of starting a second timer keeps the burst in sync with the camera.

Whether any of this *feels* good is unverified: automation can prove the envelope
and the plumbing, not the read.

## Gear x tree movement composition is now ADDITIVE — this is a FELT CHANGE

**Movement gets slightly slower at high investment. Judge it in a playtest.**

The locked aggregation rule (`Source/RiorsEdge/Attributes/BreakerAttributeAggregation.h`,
and the "Unified attribute application" section of `Docs/Item-Foundation.md`) is:
flat sums first, **all** Increased percentages form **one additive bucket per
stat**, More multipliers reserved for trees and Anomalous. Damage was conformed
when `GearWeaponDamageMultiplier` was deleted. Movement was the last violation:
`UBreakerCharacterMovementComponent` read the gear multiplier and the tree
multiplier separately and *multiplied* them, so +20% boots and +20% tree read
**x1.44** where the rule says **x1.40**.

Nothing was retuned. The composition changed, which means a heavily invested
character is now measurably slower than they were yesterday, and a character
with investment in only one layer is **bit-identical**.

### Before / after at representative investment

Sprint 1100 cm/s, walk 700 cm/s, dash cooldown 4.0 s, air steer rate 4.2.

| Gear | Tree | Composed | Old (x) | New (x) | Sprint old | Sprint new | Δ |
|---:|---:|---|---:|---:|---:|---:|---:|
| +0% | +0% | x1.00 | 1.000 | 1.000 | 1100 | 1100 | — |
| +8% | +0% | x1.08 | 1.080 | 1.080 | 1188 | 1188 | — |
| +0% | +12% | x1.12 | 1.120 | 1.120 | 1232 | 1232 | — |
| +8% | +12% | x1.20 | 1.210 | 1.200 | 1330 | 1320 | **−0.8%** |
| +20% | +20% | x1.40 | 1.440 | 1.400 | 1584 | 1540 | **−2.8%** |
| +30% | +24% | x1.54 | 1.612 | 1.540 | 1773 | 1694 | **−4.5%** |

Same arithmetic, same four stats:

| Stat | At +20% gear / +20% tree | Old | New | Δ |
|---|---|---:|---:|---:|
| Walk ceiling | 700 x mult | 1008 cm/s | 980 cm/s | −2.8% |
| Sprint ceiling | 1100 x mult | 1584 cm/s | 1540 cm/s | −2.8% |
| Slide ceiling | 1100 x mult | 1584 cm/s | 1540 cm/s | −2.8% |
| Air steer rate | 4.2 x mult | 6.05 | 5.88 | −2.8% |
| Dash cooldown | 4.0 s ÷ reduction | 3.39 s | 3.39 s | **unchanged** |

Dash cooldown is unchanged *today* only because no `EBreakerNodeStatTarget`
authors dash cooldown, so gear is the only bidder and one bucket equals one
layer. It is conformed anyway: the moment a node adds a dash line it is additive
from day one instead of repeating this bug.

**Only one layer invested? Nothing changed.** The whole delta lives at the
intersection, which is exactly where the rule was being broken.

### How it is implemented

Not by fixing the multiplication in place — by deleting the composition from the
movement layer entirely, in the precedent the damage pass set:

- `EBreakerAggregatedAttribute` gained `SlideSpeedMultiplier`,
  `AirControlMultiplier` and `DashCooldownReduction` (all base 1.0, all
  replicated on `UBreakerAttributeSet`). `MoveSpeed` already existed.
- Gear (`UBreakerEquipmentComponent::AggregateStats`) and the tree
  (`UBreakerProgressionComponent::AggregateStats`) each bid raw percentages into
  those attributes, so the additive bucket is structural rather than a
  convention the movement layer has to remember.
- `UBreakerCharacterMovementComponent` reads the composed attributes:
  `GetComposedMoveSpeedMultiplier` / `SlideSpeed` / `AirControl` /
  `DashCooldownMultiplier`. The private `GearMoveSpeedMultiplier()` and friends
  are gone.
- `DashCooldownReduction` is a **divisor** (x1.20 == a 20% shorter cooldown).
  That shape is what lets it live in an additive bucket at all; an attribute
  holding the cooldown in seconds could not be shared by two layers additively.

**To revert to the old feel exactly** there is no switch — it is a rule, not a
tunable. Reverting means putting the multiplication back in
`GetComposed*Multiplier`, which re-breaks the locked rule. If the new numbers
feel bad, retune the *content* (affix ranges, node percentages) instead.

Covered by `RiorsEdge.Movement.AdditiveComposition` (the arithmetic, including
the exact table above) and `RiorsEdge.Movement.ComposedAttributes` (both layers
through the real attribute set with real gear and a real node).

Not playtested. Automation proves the maths; whether a 2.8% loss at heavy
investment is felt at all is exactly what it cannot see.

## The composed MoveSpeed attribute now has a consumer

`UBreakerAttributeSet::MoveSpeed` was written by the aggregator and read by
**nobody** — the movement component computed its own speed. That is the
"attribute that lies to the player" failure mode, the same one `DamageMultiplier`
had before the damage pass. It is now the single source of truth for move-speed
composition, consumed by `GetComposedMoveSpeedMultiplier`.

One wrinkle worth stating rather than hiding: `MoveSpeed` is a **speed in cm/s**
while the other three are multipliers. `WalkSpeed` is authored `EditAnywhere` on
the movement component, so an attribute-set constant for it (it was 650, against
a real walk speed of 700) goes stale the moment the owner retunes it — a
composed attribute that disagrees with the speed the character actually walks at
is the same class of lie. The movement component therefore **publishes** its
`WalkSpeed` as the attribute's base
(`UBreakerAttributeSet::SetAggregatedAttributeBase`) once the ability system has
registered the set, and reads back `composed / base` as the multiplier. After
that the attribute genuinely is the character's current walk speed.

**SlideSpeed / AirControl / DashCooldown reached the attribute set at all for
the first time here** — they are routed in, not documented away.

The publish is polled from `TickComponent` rather than done in `BeginPlay`,
because the attribute set is registered with the ability system in
`ABreakerCharacter::BeginPlay`, which can run *after* the movement component's;
a one-shot there would silently find nothing. It costs one branch per frame
after the first success.

## Swift's third jump (O25)

O25: "TWO JUMPS are base kit for everyone and Swift innately unlocks a third
later — innate to the class, not a tree purchase." Before this the count was the
constant `JumpMaxCount = 2` on `ABreakerCharacter` and the third jump did not
exist in any form.

**What is delivered is the MECHANISM.** The threshold is a placeholder awaiting
an owner ruling — CONTEXT.md lists "when it unlocks and whether it is free" as
open — and it is flagged `O2 PLACEHOLDER` at the code.

| Tunable | Default | Kind |
|---|---:|---|
| `BaseJumpCount` | **2** | O25 base kit, every class, every level. Written onto `ACharacter::JumpMaxCount` at runtime, so a Blueprint override is deliberately overwritten: O25 is a rule, not a per-Blueprint preference. |
| `bSwiftThirdJumpEnabled` | **true** | Master switch. False restores exactly the pre-O25 behaviour. |
| `SwiftThirdJumpUnlockLevel` | **1** | **O2 PLACEHOLDER, still awaiting a ruling.** Free — no resource cost, no cooldown, no point spend. Was **20**, which made the feature unreachable; see "The third jump was unreachable" below. |
| `SwiftThirdJumpRedirectAlpha` | **0.55** | **O2 PLACEHOLDER.** How far the third jump turns horizontal velocity onto input. 0 makes it identical to the second jump. |

**Rules, all tested by `RiorsEdge.Movement.JumpGrant`:**

- Two jumps for every class at every level. Nobody ever drops below two.
- Three only for Swift, only at or past the unlock level. The threshold is
  inclusive: level 19 is two, level 20 is three.
- The grant reads the **permanent** class from `UBreakerProgressionComponent`,
  recomputes on `OnProgressionChanged`, **and** polls the live state from
  `TickComponent` as a backstop, in the precedent of
  `UBreakerManaComponent::AdvanceLoop`. `DevForceClass` *does* broadcast today —
  that was checked, not assumed — but an illegal third jump that outlives its
  class must not depend on every future writer of the progression state
  remembering to broadcast.
- A swap **away** from Swift returns the character to two immediately, and
  clamps `JumpCurrentCount` so a jump already banked against the larger budget
  cannot survive the swap.

**Why it feels like Swift's and not like a repeat.** The third jump buys a
*course correction*, not altitude and not speed: `BlendHorizontalVelocity`
rotates horizontal velocity partway onto the current input direction with its
magnitude **preserved exactly**, and vertical velocity is untouched. With no
input it is a plain jump that keeps all of its momentum. Redirection is Swift's
verb already (Skim is the same idea) and Master 5.4 forbids self-acceleration,
so a speed-preserving turn is the restrained version. O26 says movement gets no
further dedicated passes, so this executes O25 and adds nothing else.

### The third jump was unreachable (owner: "i never could do a 3rd jump")

**The grant was never broken.** Every link in the chain was checked, not
assumed, and every one of them was correct: the permanent class is read from
`UBreakerProgressionComponent`, `OnProgressionChanged` is bound (late-bound
from `RefreshJumpGrant`, because component BeginPlay order is not guaranteed),
the tick poll re-runs it as a backstop, `DevForceClass` does broadcast, and a
swap away from Swift clamps `JumpCurrentCount` so a jump banked against three
cannot survive onto a budget of two.

What was wrong is one level up. The gate read
`FBreakerProgressionState::CharacterLevel`, and **nothing in the project writes
that field.** It is declared with a default of 1, there is no XP loop, no
level-up path, and a repository-wide search for an assignment to it returns the
declaration and nothing else. Against a threshold of 20 the condition
`CharacterLevel >= UnlockLevel` was therefore false for every character that
has ever existed. The feature was not late — it was **disabled, behind a
plausible-looking excuse**, and the excuse is what let it pass review, pass a
green suite, and reach a playtest.

The repair is not a smaller number picked to clear today's level. It is the
rule: **a gate must key off something that actually moves, and until an XP loop
exists nothing does — so the gate defaults to reachable.** Raise it again on
the day `CharacterLevel` starts moving.

Three things now make that failure unable to recur silently:

- `SwiftThirdJumpUnlockLevel` defaults to **1**.
- `RefreshJumpGrant` logs a **warning, once**, if a Swift character's gate sits
  above the level it observes, naming the missing XP loop.
- **`RiorsEdge.Movement.JumpGrantMatrix`** asserts the *shipped configuration*
  against a **default-constructed `FBreakerProgressionState`** — the state
  every character in the gym, in a playtest and in a fresh save actually runs
  in — rather than against a hypothetical level. Swift gets three, every other
  class gets exactly two, a `DevForceClass` swap moves the budget in both
  directions, and the banked-jump clamp never hands out a jump. The existing
  `RiorsEdge.Movement.JumpGrant` proves the *rule* and passed the entire time
  the feature was dead; that is precisely the gap this closes.

The jump budget is also logged whenever it changes, so an owner can confirm the
third jump from the log instead of by failing to perform it.

Never playtested; automation proves the matrix and the speed-preservation
guardrail, not the feel.

## Base kit

Every character has walk, sprint, jump, crouch, dash, slide, wall ride, wall jump, block, and dodge from level one. No class or constellation unlocks these actions.

**Two jumps are base kit for every class (O25).** The earlier line here — "air
jump is the exception and remains a tree unlock" — was superseded by O25 and is
deleted. Swift innately unlocks a *third* jump later; see "Swift's third jump"
below. Parry is the only tree-granted verb left.

Trees and affixes scale these actions. Affixes own raw percentages and stamina economy; trees own rule changes and quality such as i-frame duration and parry. See `Docs/Layer-Ownership.md`.

## Guardrails

- Ordinary forward movement must not self-accelerate beyond sprint speed.
- Wall riding preserves flow but must not generate speed.
- Dash should solve a positioning problem, not become the fastest way to travel everywhere.
- Sliding should have a clear beginning and end on flat ground.
- Advanced movement cannot be required to land routine weapon shots or avoid every baseline enemy attack.
- Camera roll, FOV changes, and shake must be subtle and configurable.
- Enemy and level design should offer movement opportunities without punishing players who use conventional routes.

## Testing questions

- Can a new player fight effectively using only standard FPS controls?
- Does advanced movement create tactical choices rather than mandatory repetition?
- Can the player track targets while sprinting, sliding, and immediately after dashing?
- Do rooms retain meaningful cover and distance when traversal abilities are available?
- Is movement still readable from an enemy or multiplayer observer perspective?
