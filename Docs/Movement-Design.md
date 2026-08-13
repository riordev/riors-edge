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
- Wall ride: implemented baseline with 0.85-second maximum, minimum 700 cm/s, reduced gravity, no passive speed gain, loss-of-contact exit, and a controlled wall jump.
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
