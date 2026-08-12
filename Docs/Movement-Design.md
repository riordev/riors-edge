# Movement design

## Product intent

Movement gives players expressive positioning, recovery, and route choice without becoming the entire challenge or invalidating weapons, enemies, and arenas. A player using ordinary run, sprint, jump, and cover should remain viable. Advanced movement rewards awareness without being mandatory for baseline combat effectiveness.

## Current baseline

- Walk: 650 cm/s.
- Sprint: 950 cm/s.
- Jump: 700 cm/s with slightly increased gravity.
- Air control: moderate and forgiving, but not Quake-style speed generation.
- Dash: redirects momentum with a 1250 cm/s floor plus a small bonus; 2.5-second prototype cooldown.
- Slide: requires 750 cm/s, adds a small deterministic entry push, then brakes naturally; downhill surfaces add restrained acceleration.
- Wall ride: implemented baseline with 0.85-second maximum, minimum 700 cm/s, reduced gravity, no passive speed gain, loss-of-contact exit, and a controlled wall jump.
- Grapple: excluded.

All numbers are initial editor-test values, not promises. Tune them in the context of aiming, incoming attacks, encounter distances, and readable enemy behavior.

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
