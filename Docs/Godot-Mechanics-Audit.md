# Godot mechanics audit

Source reviewed: `riors-arena-v0.1.132-src.zip`. Its internal files identify it as Rior's Arena v0.1.128/114-era source. The lightweight extracted reference is ignored by Git under `References/GodotArena`.

This records behavior supported by source code. It is not a recommendation to copy the Godot architecture line for line.

## Executive finding

The prototype's identity is a momentum economy, not simply a high run speed. Ordinary movement builds and preserves speed; deliberate boosts temporarily unlock a higher ceiling. Sliding, wallrunning, jumping, dashing, grappling, weapon knockback, and perks participate in that velocity economy.

The Unreal character therefore needs a custom `UCharacterMovementComponent` subclass. Isolated `LaunchCharacter` calls on the stock component would reproduce the silhouettes but not the interactions. However, the Godot values are evidence rather than direct targets: the Unreal game is deliberately moving toward more grounded, combat-supporting motion.

## Confirmed movement model

Godot uses meters and seconds; Unreal equivalents multiply linear values by 100 for centimeters.

| Behavior | Godot value or rule | Initial Unreal equivalent |
|---|---:|---:|
| Walk target speed | 13 m/s | 1300 cm/s |
| Ground acceleration | 110 m/s² | 11000 cm/s² |
| Ground friction | Source-style coefficient 10 | Custom friction calculation |
| Stop speed | 3 m/s | 300 cm/s |
| Gravity | 24 m/s² | 2400 cm/s² |
| Jump velocity | 9.5 m/s | 950 cm/s |
| Jump buffer | 150 ms | 0.15 s |
| Coyote time | 100 ms | 0.10 s |
| Air acceleration | Quake projection-cap model | Custom air acceleration |
| Air projection cap | 1 m/s | 100 cm/s |
| Direct air steering | Preserves speed; cannot reverse freely | Custom steering at 4.2 rate |
| Self-sustained soft cap | 35 m/s | 3500 cm/s |
| Boosted hard cap | 42 m/s | 4200 cm/s |
| Excess-speed decay | 14 m/s² | 1400 cm/s² |

Ground friction drops immediately after landing and is reduced above ordinary walk speed, allowing bunny-hop momentum to survive brief floor contact.

Air movement caps only the velocity projection along the requested direction. Sideways strafing and turning can therefore add speed beyond the nominal air cap. A second steering pass rotates horizontal velocity toward input without changing magnitude and refuses free reversals.

Required Unreal consequence: use `UBreakerCharacterMovementComponent : UCharacterMovementComponent` with explicit movement state and velocity integration. Keep replication and prediction inside the movement component rather than distributing velocity rules across character, ability, and animation classes.

## Jumping

- 150 ms input buffer and 100 ms coyote window.
- Ground jumps preserve horizontal speed.
- Default kit supports two jumps.
- An air jump resets vertical velocity and redirects horizontal velocity 35% toward input while retaining at least walk-speed magnitude.
- Starting a wallrun refreshes one jump.
- Dash refreshes the complete jump allowance.
- Landing resets jumps and wallrun count.

This is core traversal, not a perk. Unreal should represent remaining jumps and movement resources in predicted movement state; GAS can modify maximum values.

## Slide and crouch

- Crouch shortens the capsule and smoothly lowers the camera.
- Grounded crouch above 7.5 m/s enters slide; holding crouch while landing does the same.
- Slide minimum continuation speed is 6.5 m/s.
- Slide has 0.75 seconds of friction grace.
- Flat-ground friction begins after grace and ramps with time, preventing permanent flat slides.
- Downhill acceleration is 26 m/s²; downhill slides avoid flat-ground friction.
- Jumping exits slide without discarding horizontal speed.
- A timed slide cancel exists during the final 110 ms of grace and requires at least 12 m/s.
- Successful cancels add 1.9 m/s; chained payouts decay to 60%, 36%, and so on.
- Cancel cooldown is 2.4 seconds and an audio cue announces the valid timing window.

Preserve capsule transition, landing-to-slide, grace, slope acceleration, speed retention, and timed cancel. Reconsider the prototype's random slide-start boost: randomness in a precision movement loop can weaken learnability and may fit better as a future skill or item effect.

## Wallrun and wall jump

- Begins only while airborne, faster than 5 m/s, with movement input and a near-vertical attachable surface.
- Crouch, ground contact, grapple, or the post-walljump cooldown prevents wallrun.
- Base duration is 2.2 seconds.
- Wall gravity is 5 m/s² and falling speed is limited.
- Horizontal velocity projects onto the wall tangent and gains up to 2 m/s per second, capped at 21 m/s before other modifiers.
- A gentle inward force maintains contact.
- Curved wall normals are smoothed; changes above 50 degrees break the run.
- Two wallruns are available before ground contact resets them.
- Ending a wall restores the last tangent velocity if collision resolution removed too much speed.
- Wall jump adds 7 m/s away, 2.5 m/s view-forward, and 8.8 m/s upward.
- Reattachment cooldown is 250 ms.
- An instant wall-bounce is possible by jumping on contact without riding the wall.
- Camera rolls about 11 degrees away from the wall.

Wallrun should be an Unreal custom movement mode. Surface eligibility should use a trace channel, physical surface, or tag comparable to Godot's `no_attach` metadata.

## Dash

- Dash is an ability with two base charges and seven-second recharge.
- Movement input selects direction; otherwise camera-forward is used.
- Dash redirects horizontal velocity rather than adding a blind impulse.
- Output speed is `max(current speed, 21 m/s) + 2 m/s`; it never intentionally brakes a faster player.
- Vertical velocity rises to at least 1.5 m/s.
- Dash refreshes air jumps and temporarily opens the boosted speed ceiling.
- Presentation includes sound, six-degree FOV kick, and a trail.

The current Unreal `TryDash` is placeholder behavior and should be replaced with this charge-based model before tuning. GAS should own charges, recharge, restrictions, tags, and cues; the movement component should execute the predicted velocity change.

## Grapple — reference only, excluded from Unreal scope

- World-only attach trace with 31.9 m range; surfaces can disallow attachment.
- Maximum attached duration is 3.5 seconds.
- Detaches within 3 m, on death, timeout, or release.
- Applies a steady 26 m/s² pull toward the anchor.
- It is intentionally not a rigid rope: the first 120 ms has no outward constraint.
- Afterward, 1.5 m/s of outward radial drift is free; only excess outward velocity is damped, by 50%.
- Air steering remains available while attached.
- Release adds 4.5 m/s upward and temporarily opens the boosted ceiling.
- Cooldown is seven seconds and begins on meaningful hooked release.
- Jump releases the hook; grapple and wallrun are mutually exclusive.

This mechanic is not planned for the Unreal game. Retain this section only as historical reference; do not scaffold, implement, or reserve an ability slot for grapple without a new explicit product decision.

## Camera and feedback

- Base scene FOV is 92 degrees.
- Speed widens FOV above walk speed using a player setting; ADS overrides it.
- FOV interpolates rather than snapping; dash adds a temporary six-degree kick.
- Wallrun adds directional roll with hysteresis.
- Camera height interpolates between standing and crouched positions.
- Recoil is immediate view kick followed by spring-like repayment.
- Footsteps are distance-driven; slide cancel has an audio timing tell.

These are mechanic-readability requirements, not optional polish.

## Combat structure

The data-driven weapon system supports hitscan, projectile, bolt, burst, automatic, charge, shotgun, melee, damage/headshots, RPM, ammunition/reload, hip and ADS spread, falloff/range, recoil, ADS movement, splash, self-damage, knockback/rocket jumping, homing, ally healing, harpoon pull, slow, marking, and other special flags.

Do not port all 38 weapons. First build a reusable pipeline and three representatives:

1. Hitscan rifle or sidearm for baseline aiming and damage.
2. Shotgun or burst weapon for multi-shot behavior.
3. Rocket or harpoon weapon for movement interaction.

Weapon rules belong in C++, content in Data Assets, damage/status in GAS, and presentation in Blueprint.

## Multiplayer lesson

Godot fully simulates movement on the owning peer and interpolates remotes. Unreal should use `UCharacterMovementComponent` prediction instead of recreating that network layer. Abilities and combat outcomes remain server-authoritative while local movement abilities support prediction.

## Deferred systems

Perks, loadouts, attachments, progression, abilities, bots, modes, and Steam multiplayer are evidence of the design space, not current scope.

Affixes will work fundamentally differently in the Unreal ARPG. Do not infer them from Godot attachments or perks. Affix architecture needs a separate design covering roll sources, scopes, stacking, tags, stat evaluation, proc rules, persistence, and build-defining effects.

## Revised implementation order

1. Add `UBreakerCharacterMovementComponent` and move locomotion velocity rules out of `ABreakerCharacter`.
2. Implement ground friction/acceleration, landing grace, jump buffer, coyote time, air acceleration/steering, and two-tier speed caps.
3. Implement deterministic slide, slope behavior, and slide-hop; add timed cancel after fundamentals stabilize.
4. Add predicted air-jump and wallrun resources.
5. Add wallrun/walljump as a custom movement mode.
6. Replace placeholder dash with a charge-based GAS ability calling the movement component.
7. Add camera/FOV/roll/audio feedback alongside each mechanic.
8. Build a traversal course and record completion times and speed traces.
9. Begin combat as soon as grounded movement, dash, and slide are coherent; wall riding may mature alongside the combat sandbox.

## Product decisions still needed

- The exact 13/35/42 m/s scale is rejected; begin with grounded shooter values and tune through combat playtests.
- Universal double jump, skill-tree unlock, or discipline-specific?
- Keep the 110 ms slide-cancel test, widen it, or make it build-dependent?
- Universal dash, or an ability with charges?
- Grapple is excluded from current scope.

The movement component should support these choices without hard-coding progression ownership.
