# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Open questions for the owner
- (bosses) At area level 1 a rear-only boss kill is 241 rifle rounds against 150 carried; the fight's ammo comes from add kills and the supply crate. Is a boss meant to be sustainable on one loadout, or paced by add-kill ammo? `PowerCurve.GymEntry` prints the gap every run.
- (bosses) The "shield" on the boss is the Warden's frontal armour slab (90 armour, 47 % mitigation; the rear is unarmoured). O175 keeps it as the puzzle you flank. Should the front feel breakable instead?
- (ui) The vitals row is `[shield current] —— [health current]`, no max drawn by design (HUD v2). `0 —— 0` dead is true. Do you want a max shown (`current / max`, a design reversal), and should a zero-max shield draw its empty track and `0` at all? Today it does, which is what made the row read as current/max.
- (sound) O193 as written has no sound; your desk line has one low sound. If you want it, add "one low sound" to O193's line and the sixth verb lands (`player_death.wav` slot → synth fallback) with its one call site in the HUD's fatal-hit branch.

## Cycle 7 — the rest of DATA-4 (one library per commit)
- [ ] Quests → `Data/quests.json` (`BreakerQuestContent.cpp` becomes a loader; 4 quests, the flag registry, `ValidateQuestContent` runs on the file).
- [ ] Dialogue → `Data/dialogue.json` (`BreakerNPC.cpp` MakeXDialogue become loaders; ~26 nodes, ~70 choices; the lexicon test reads the file).
- [ ] Abilities: only the numerics (`ResourceCost`, `CooldownSeconds`, `WindowDuration`, variant numbers) → `Data/abilities.json`; the registry, `AbilityClass` and tags stay a DataAsset (O186). The ~40 per-ability `O2 PLACEHOLDER` constants in the ability .cpp files are the real no-compile target and need their own item.
- Owner question (content pipeline): dialogue and quest text are keyed rows. One home per string: `quests.json`/`dialogue.json` own their text, `Data/strings.json` owns UI chrome only. Amend the O195 line to "every player-facing `TEXT` in `UI/` and `Game/` not already a dialogue or quest row"?
- ORDERS Part Five DATA-4 says "→ DataAssets"; O186 says JSON. Edit that line on your seat.

## Cycle 8 — the flat-damage ruling lands (R1)
- Owner rules R1 first: weapons and gear carry a true flat Added Damage fed by an affix and a weapon base line, or the Flat term leaves `power-and-scaling.md`. Then LEDGER-3 (affix pool 28 → 56) and the at-cap band re-measured (`PowerBand.AtCap` is expected red today).

## Cycle 9 — the boss grammar and two more bosses (FIELD-3)
- [ ] Telegraph → punish window → phase gate → add wave → arena change as a pure header; the shield is the first punish window, not a wall.
- [ ] Second boss: add-clear under pressure. Third: mobility and sustain. `Combat.PowerCurve.BossBand` holds for all three; O31 asserted per boss.

## Cycle 10 — the Niagara pass (GLASS-1, O179, O190)
- [ ] Muzzle, impact, cast moment, death: the four `NS_<Moment>` slots filled (owner asset or Fab pack), verb-colour law kept. The muzzle flash comes back here and nowhere earlier.
- [ ] Delete the drift (H1): wall-ride out of the specs, dead gameplay tags out of the ini; grep-empty.

## Then, in this order, each sized when it reaches the top
1. Elements and the reaction matrix; statuses as the cross-class combo language (KIT-3: Rot spread by pierce, Provoke grouping for MineCluster; no class-pair specials).
2. Volatile enemies as player weapons; Momentum reads off the gun (KIT-2: spread and tracer brightness follow the bar).
3. Rift interiors (GROUND-1): three to five room shapes measured against the gap rules, feeding the wave solver.
4. Ten authored legendaries with printed forfeits (LEDGER-4, O66/O67).
5. Anomalies as the first real endgame (GROUND-3): key → run → payout as a functional test.
6. Two-seat listen-server smoke every cycle (O185), then Dungeons, then Raids.

## The voice (O195) — slots in whenever a cycle has room
- [ ] O195 string table: every player-facing `TEXT("…")` in `UI/` and `Game/` → `Data/strings.json`; TILESET / BANKED / SETTLED never reach the screen.
- [ ] Menu checklist: every screen photographed; hover/press states, transitions, type hierarchy, density, faded-disabled — a list the owner marks.
- [ ] Per-archetype weapon fire: `weapon_fire_<archetype>.wav` → `weapon_fire.wav` → synth.
- [ ] Movement Speed affix on Boots (O192), after affixes are data.

## Later (infrastructure only when it unblocks a felt item this week)
- After the Boots-only MoveSpeed row: the Gloves `Core.MoveSpeed` roll in `Items.Equipment.AttributeContribution` (~BreakerItemTests.cpp:425) grants a slot a line it cannot roll; move it to Boots in the next build. The Sidearm lean on `Core.MoveSpeed` in `affixes.json` is inert (leans apply on weapon slots); delete the row when the leans are next touched.
- The death beat's black is a camera fade and its teleport lands at the end of black; `HoldBlack`/`ReleaseBlack` on the game instance are the seam to move the teleport to the start of black and reveal through the arrival gate. Felt only if the respawn frame reads cold.
- The boot's first front-end frame is still uncovered; only travels get the cover.
- The split copy's 0.7 scale has no pin: `ConfigureAsSplitCopy` writes health through GAS, which no test outside a world can call. It waits on the map-loading functional tests (GROUND-4).
- The Warden overrides the base engaged tick wholesale: no arrival ring, no arrival angle, so two Wardens stack on one line and it walks through the player between sweeps (`BreakerWardenEnemy.cpp` TickEngagedBehaviour).
- Nav.Probe Squad's LATTICE FAIL reads the band without `BandHysteresis` (150 cm); a moving pawn can print an honest fail at the band edge.
- Ranged enemies have no walk sequence and move in ref pose; STEER frames call StopMovement and rebuild velocity from zero each flip (the stutter at range). Both recorded at the site in `BreakerEnemy.cpp` Tick.
- `BossBand`'s 20/45 s constants are function-local; `PromotedBossSecondsFloor/Ceiling` duplicate them. One line in BossBand to share the pair.
- NAV-2 cover on the nav · DATA-2 affixes to data · FIELD-3 boss grammar · GROUND-4 functional tests · GLASS-3 the 11K-line split · NAV cover/squad · Anomalies
- `SlideEntrySpeed = 550` is still absolute (0.92 of the 595 walk; was reachable in the top 45 cm/s of a walk only) — a fraction of `WalkSpeed` like the Momentum gates.
- `BreakerGameMode.h` field grammar comments derive `DashRefreshDistance 4400` and `OneJumpGap 700` from a 1100 sprint; the sprint is 990.
- A sprint-only bob frequency needs a `SprintStrideLengthCm` and a lerp; `StrideLengthCm 360` is shared with the walk.

## Filing notes
Every note the owner writes carries one of these tags so the queue reads by category: AI (behaviour, not roster) · bosses · animation · VFX/hit feedback · networking/party/social · loot & economy · encounter/level tooling · content authoring pipeline · onboarding/first hour · endgame loop · performance budget · telemetry · accessibility/input · persistence · systems · core gameplay · weapons · abilities · classes · maps · enemies · story · build diversity · fun interactions · visuals · sound · movement · ui · npcs.

## Owner only
- Fab mannequin/GASP, Ultimate Modular Women, Sonniss extract (arms, anims, real audio all wait on these)
- Four Niagara systems at `/Game/Breaker/FX/NS_<Moment>` with a `Color` user parameter, or a free Fab VFX pack placed there

## Done (last three cycles; older is git)

### Cycle 6
- [x] Affixes are data: `Data/affixes.json` (27 slice / 8 aberrant / 5 anomalous / 3 downside / 1 elemental, 26 leans, caps) through the first runtime loader `Data/BreakerDataFile`; the library is a loader with a validator, signatures unchanged; the commandlet's export matches the file byte for byte; status.py reads it; Data/ is staged.
- [x] Movement Speed is a Boots-only affix with a cap of 25 % (O192), landed as a data-only commit with no build.
- [x] The quartermaster's stock is a row: `Data/class-kits.json`, five classes, read by `GetFallbackClassDefinition`; the catalogue partition test is the identical-output pin.

### Cycle 5
- [x] Every travel arrives under cover: a briefing pane for a rift, plain black otherwise, and the cover lifts only when the world is loaded AND 24 frames AND 0.9 s have passed, then a 0.35 s ramp (all O2, live as CVars). A 12 s watchdog lifts a cover whose travel the engine refused. `-BreakerCaptureArrival` photographs the first revealed frame and one second later: mean luminance 67 and 67, identical. Pinned by `Arrival.SettleHold`.

### Cycle 4
- [x] O193 death beat: weapon lowers into the holster pose, camera drops 18 cm and rolls 9° / pitches −12° while the world desaturates over 0.8 s, black 1.2 s with the HUD hidden, teleport under black, fade-in 0.4 s with input on from the first visible frame. All numbers O2 in `Game/BreakerDeathBeatMath.h`; pinned by `DeathBeatTimeline`. No sound (needs the sixth verb; question above). Motion: owed your eyes.
- [x] O194 rank law: Trash / Elite / Champion stand at 1.0 / 1.15 / 1.30 through `ApplyChassis`, so every promotion and demotion lands the same absolute scale; Elite wears a drawn ring halo, Champion two diamonds; ELITE leaves the label; the state word leaves the shipped bar (F3 still prints it). Photographed on the four-rank bar matrix. Split copies shrink through the chassis base scale.
- [x] Gun forward axis: already landed by 65247d4 with three tests; photographed across the weapon cycle — six named guns barrel-forward, Shotgun and Rocket on primitives (no candidate in the pack). The rule is the thin end, not the far end.
- [x] Health bar `0 —— 0` dead: found-not-built. The row is shield current / health current, no max by design; `0 —— 0` is true. Two questions above.



