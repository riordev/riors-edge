# Playtest Gym v1

The existing First Person map now becomes a zero-setup combat and movement test whenever `BreakerGameMode` is active.

## What appears at runtime

- Four recycling diagnostic targets: health, shield, armour, and lateral movement.
- A code-driven crosshair and debug HUD showing movement state, horizontal speed, health, shields, ammunition, and reload state.
- Red body-hit feedback, gold weak-point feedback, and applied damage numbers.
- A deliberately simple first-person placeholder weapon block. It exists only to communicate viewmodel position until proper art and animation replace it.

The targets use engine basic-shape meshes, so this setup does not depend on new binary assets or machine-specific editor work.

## Controls

- Move: WASD
- Look: Mouse
- Sprint: Left Shift
- Dash: Q
- Slide: C or Left Ctrl
- Jump / wall jump: Space
- Fire: Left mouse
- Aim: Right mouse
- Reload: R
- Reset player, ammunition, targets, and session stats: F1
- Copy a structured session report to the clipboard: F2
- Toggle diagnostics: F3
- FOV: left/right bracket
- Mouse sensitivity: minus/equals

FOV and sensitivity persist locally between launches. Diagnostics show frame rate, session accuracy, weak-point rate, damage, reload count, target types/distances, and a brief shot trace. The copied report is formatted for pasting into the Notion Signals database.

## First feedback pass

Test each item for several minutes and record **too weak / good / too strong**, plus one sentence describing why:

1. Walk acceleration and stopping precision.
2. Sprint speed relative to arena scale.
3. Dash distance, direction control, and 2.5-second cooldown.
4. Slide entry reliability, duration, slope response, and exit control.
5. Wall-ride activation consistency, 0.85-second maximum, gravity, and wall jump.
6. Hip-fire spread versus aimed spread.
7. Rifle damage cadence, reload length, and magazine size.
8. Weak-point readability and hit feedback.
9. Damage falloff between the near, middle, and far targets.
10. Any motion discomfort, camera obstruction, collision snag, or input failure.

Do not tune from a single run. First verify the controls and target behavior, restart once, then perform the real feedback pass.
