# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Cycle 1 — the regressions (one build)
- [ ] Land `lane/batch1-field` on main: five FIELD commits, the revive-gait fix among them. Keep NAV-1's `Drive` block in `BreakerEnemy.cpp` and FIELD's revive path.
- [ ] Enemies face the wrong way (gym reads "everyone looking left"). `SetActorRotation(Facing)` is intact in Tick; find why the mesh forward and actor forward disagree since NAV-1. `Breaker.Nav.Probe` gains a facing assertion (< 15°) so it cannot recur.
- [ ] Muzzle fallback is a giant orange sphere. `BreakerEffectMomentMath.h` Muzzle fallback → 15–25 cm disc, 60 ms, at the visual muzzle. Death fallback untouched (owner: it works).
- [ ] Sprint viewmodel bob/sway spikes. Third-mode amplitude and frequency halved as the start.
- [ ] Waves auto-advance: clear → visible countdown (8 s; rest waves 20 s) → next. F4 skips the countdown.
- [ ] O192 first numbers: walk −15 %, sprint −10 %, `AirControl` 0.55 → 0.35, boost 1.4 → 1.15; Momentum thresholds re-derived as fractions of the base.

## Cycle 2 — what a death and a fight look like
- [ ] O193 death beat: weapon lowers → camera drops/tilts and desaturates ~0.8 s → one low sound → black ~1.2 s → fade-in at tileset start, input on first visible frame. `HandlePlayerDeath` is the site.
- [ ] Health bar reads `0 —— 0` while dead; should read `0 —— 100`.
- [ ] O194 rank law: Elite +15 % scale + halo; Champion +30 % + two diamonds; ELITE leaves the label; CLOSING/HELD leave the screen.
- [ ] Gun forward axis: eight meshes checked through `NamedMeshPath`; muzzle is the far end; photographed at rest.
- [ ] Lighting settles before the fade-in ends: hold the fade until Lumen's surface cache is warm on runtime-spawned geometry; travel flicker is the same fix.

## Cycle 3 — the voice
- [ ] O195 string table: every player-facing `TEXT("…")` in `UI/` and `Game/` → `Data/strings.json`; TILESET / BANKED / SETTLED never reach the screen.
- [ ] Menu checklist: every screen photographed; hover/press states, transitions, type hierarchy, density, faded-disabled — a list the owner marks.
- [ ] Per-archetype weapon fire: `weapon_fire_<archetype>.wav` → `weapon_fire.wav` → synth.
- [ ] Movement Speed affix on Boots (O192), after affixes are data.

## Later (infrastructure only when it unblocks a felt item this week)
- NAV-2 cover on the nav · DATA-2 affixes to data · FIELD-3 boss grammar · GROUND-4 functional tests · GLASS-3 the 11K-line split · NAV cover/squad · Anomalies

## Owner only
- Fab mannequin/GASP, Ultimate Modular Women, Sonniss extract (arms, anims, real audio all wait on these)
- Four Niagara systems at `/Game/Breaker/FX/NS_<Moment>` with a `Color` user parameter, or a free Fab VFX pack placed there

## Done (last three cycles; older is git)
