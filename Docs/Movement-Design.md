# Movement design

## Product intent

Movement gives players expressive positioning, recovery, and route choice without becoming the entire challenge or invalidating weapons, enemies, and arenas. A player using ordinary run, sprint, jump, and cover should remain viable. Advanced movement rewards awareness without being mandatory for baseline combat effectiveness.

## Current baseline

- Walk: 700 cm/s.
- Sprint: 1100 cm/s.
- Jump: 700 cm/s with 1.35 gravity scale for a compact arc. Jumping from a slide always performs a normal jump—never a mantle—while preserving the exact horizontal slide velocity and returning to the sprinting air state.
- Air control: moderate CMC control plus a Source-inspired steering assist that rotates existing horizontal momentum toward input without adding speed or permitting free reversals.
- Dash: available on ground or in air whenever the player is not sliding, with a four-second cooldown. It redirects momentum with a 1500 cm/s floor plus a 200 cm/s bonus and preserves earned speed while movement input continues. Releasing movement or colliding clears the boosted ceiling; a 4200 cm/s safety cap remains.
- Slide: available from 550 cm/s, eases its small deterministic entry push across 0.35 seconds, gives that boost at most once per 1.2 seconds, and never uses the entry boost to exceed sprint speed plus 120 cm/s. This prevents crouch-spam speed generation while allowing downhill momentum. It carries momentum and ends after one second or when released/slowed; holding slide while airborne queues one slide for landing; downhill surfaces add restrained acceleration.
- Wall ride: implemented baseline with 0.85-second maximum, minimum 700 cm/s, reduced gravity, no passive speed gain, loss-of-contact exit, and a controlled wall jump.
- Mantle: pressing jump at a clear 35-150 cm ledge smoothly lifts the capsule over it in 0.20 seconds; tall walls and obstructed landing space reject the attempt.
- Grapple: excluded.

All numbers are initial editor-test values, not promises. Tune them in the context of aiming, incoming attacks, encounter distances, and readable enemy behavior.

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
