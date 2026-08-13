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
| `FallGravityMultiplier` | — (1.0) | **1.80** | A symmetric arc is the classic floaty read; a real-feeling jump falls faster than it rises. |
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
`FallGravityMultiplier` (1.80 → 2.10) first, then `GravityScale`, then lower
`JumpZVelocity`. **Too heavy** → lower `GravityScale` (1.60 → 1.45) first, then
`FallGravityMultiplier`, then raise `LandingMinimumSpeedScale` toward 1.0.

None of this has been playtested. It is verified by automation and by the
arithmetic above only; floatiness is a feeling and the numbers cannot confirm
it landed.

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

## Base kit

Every character has walk, sprint, jump, crouch, dash, slide, wall ride, wall jump, block, and dodge from level one. No class or constellation unlocks these actions.

Air jump is the exception and remains a tree unlock.

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
