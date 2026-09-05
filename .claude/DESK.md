# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Cycle 2 — what the owner felt after cycle 1 (one build)
- [ ] Enemies spin hard until inside their effective range, and even then do not walk properly. Facing now reads correctly once close (screenshot: the pack looks at the player). Find what feeds `Facing` while closing — target direction vs path/steer direction alternating (PATH/STEER mode flips in the probe log), an unclamped yaw per tick, or the mover and `SetActorRotation` fighting — and why the gait does not read as walking at range. A yaw-rate cap is the O2 start if the source is legitimate flipping.
- [ ] Shields make the boss, and every shielded enemy, effectively immortal. Find where shield damage routes (`ShieldDamage`, `bShieldBroken`, regen, `bOverhealToShield`) and why health never falls: a regen faster than the rifle, a break that never latches, or damage discarded when the shield is up. Pin the rifle-versus-shield break time on the shipped numbers.
- [ ] Sprint, dash and slide race the bob. `AdvanceBobPhase` advances on raw `GroundSpeed`, so a dash or slide multiplies the footfall rate. Clamp the phase speed to the sprint cap and hold or decay the bob during dash/slide/air rather than racing it. Walking is right now; do not touch the walk numbers.
- [ ] Sprint pose snaps on jump/slide: `SprintFraction` and `SpeedFraction` drop to 0 in one frame when `bGroundedStride` flips (`BreakerCharacter.cpp` ~1039–1057). A one-pole ease on the sprint fraction. Same header as the item above; one hands.
- [ ] Muzzle flash off (owner, 2026-09-05): the Muzzle fallback moment draws nothing until a real flash asset exists. Death fallback untouched. `MuzzleClearsReticle` becomes the assertion that the moment is off; the size question is closed for now.

## Cycle 3 — what a death and a fight look like
- [ ] O193 death beat: weapon lowers → camera drops/tilts and desaturates ~0.8 s → one low sound → black ~1.2 s → fade-in at tileset start, input on first visible frame. `HandlePlayerDeath` is the site.
- [ ] Health bar reads `0 —— 0` while dead; should read `0 —— 100`.
- [ ] O194 rank law: Elite +15 % scale + halo; Champion +30 % + two diamonds; ELITE leaves the label; CLOSING/HELD leave the screen.
- [ ] Gun forward axis: eight meshes checked through `NamedMeshPath`; muzzle is the far end; photographed at rest.
- [ ] Lighting settles before the fade-in ends: hold the fade until Lumen's surface cache is warm on runtime-spawned geometry; travel flicker is the same fix.

## Cycle 4 — the voice
- [ ] O195 string table: every player-facing `TEXT("…")` in `UI/` and `Game/` → `Data/strings.json`; TILESET / BANKED / SETTLED never reach the screen.
- [ ] Menu checklist: every screen photographed; hover/press states, transitions, type hierarchy, density, faded-disabled — a list the owner marks.
- [ ] Per-archetype weapon fire: `weapon_fire_<archetype>.wav` → `weapon_fire.wav` → synth.
- [ ] Movement Speed affix on Boots (O192), after affixes are data.

## Later (infrastructure only when it unblocks a felt item this week)
- NAV-2 cover on the nav · DATA-2 affixes to data · FIELD-3 boss grammar · GROUND-4 functional tests · GLASS-3 the 11K-line split · NAV cover/squad · Anomalies
- `SlideEntrySpeed = 550` is still absolute (0.92 of the 595 walk; was reachable in the top 45 cm/s of a walk only) — a fraction of `WalkSpeed` like the Momentum gates.
- `BreakerGameMode.h` field grammar comments derive `DashRefreshDistance 4400` and `OneJumpGap 700` from a 1100 sprint; the sprint is 990.
- A sprint-only bob frequency needs a `SprintStrideLengthCm` and a lerp; `StrideLengthCm 360` is shared with the walk.

## Owner only
- Fab mannequin/GASP, Ultimate Modular Women, Sonniss extract (arms, anims, real audio all wait on these)
- Four Niagara systems at `/Game/Breaker/FX/NS_<Moment>` with a `Color` user parameter, or a free Fab VFX pack placed there

## Done (last three cycles; older is git)

### Cycle 1
- [x] `lane/batch1-field`: already on main as rebased copies (5d45a6c, 4b10f98, 12147d4, 310eb8a, 7c12c4d); nothing merged.
- [x] Enemies face where the actor faces: the fit reads each rig's left/right bone pair and yaws the mesh forward onto +X; Nav.Probe prints `facing=` and `FACING FAIL` past 15°. NAV-1 was not the cause; the identity yaw was.
- [x] Sprint bob: `SprintBobMultiplier` 1.6 → 1.3, `FullBobSpeed` 600 → 510. No third harmonic exists; the one-frame pose snap is the Cycle 2 item above.
- [x] Waves auto-advance in every mode: clear → `CLEAR — N` countdown (8 s; rest waves 20 s) → next; F4 skips. Boss waves still wait for F4. The rift's standard breather moved 4 → 8 with it.
- [x] O192: walk 700 → 595, sprint 1100 → 990, `AirControl` 0.55 → 0.35, boost 1.4 → 1.15; Momentum gates are fractions of `WalkSpeed` (0.643 / 1.786 / 0.571).
