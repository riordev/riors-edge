# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Direction (owner, 2026-09-06)
Push hard on today's build; the owner is out and reads the report. Cycles run in parallel on disjoint files, one build, one suite, one commit per cycle. The affix system is updated as Cycle 13 lands. A story-mission schema is drafted now so the campaign can be fleshed out. The owner's frame to check every system against: characters pick a doctrine (sub-class) a couple of levels in and unlock doctrine points through the main story quest; a Core tree every class shares scales on its own; the doctrine tree is class-specific. A current inventory of abilities, ultimates, doctrines, affixes and their scaling is owed, with each marked live, stub or unreachable.

## Direction (owner, 2026-09-05)
No playtest until the core loop has more oomph. The next blocks are the ones that change what a trigger pull, a shield and a kill feel like: flat damage that is really the weapon's, a boss that fights back with a shield you break, and a Niagara pass with a sound for every verb. Content plumbing waits behind them.

## Story questions for the owner
- (story) Which body ends Act I? The deleted campaign table made it a Vestige mass, "The Holdfast", with no Altered before Act II; the only boss built is the Field Marshal, an Act II Altered commander. The act-one draft's Boss beat carries an empty boss id until you name it.
- (story) Is doctrine commitment gated on the first Kess beat (the KessSalvage turn-in), or merely sited there?
- (story) Command's giver is "Commander Aluko" as a placeholder in the story source; under O209 a giver with no dialogue row is not an NPC. Name, or leave Act I to the Quartermaster and Kess?

## Cycle 11b — HUD pass 2 (one build)
- [ ] The interact/loot plate (40 tall, rarity rail and tally, key tile); a reload-fraction accessor in Weapons/ so the ammo rail shows progress; an ability verb on the definition for rail colours; wave cells need an encounter total (O120); the four non-Swift resource treatments; menu chrome to bone (`Sys` already is System; the remaining `Cyan` chrome reads in the menu, the stash screen and the loading screen); the kit-tile names on the class cards clip ("SIDEAR", "ANCHOF") — the fit helper serves them; the XP rail leaves the combat HUD (O210, the pause plate carries it).

## Cycle 12b — nameplates pass 2 (one build)
- [ ] The boss bar world-space 640×24 with phase marks off the authored gates and pips (O156); the occlusion trace and the >60 m rules (Champion contact dot, boss bar at floor); names from the O195 table; the active-modifier underline; banners to the design with a priority queue; the death screen and the rift retry (O82 amended).

## Cycle 16 — the mission seams (one build)
- [ ] The three mission seams: an arrival flag written on travel (`Mission.<id>.Arrived`), rift completion → the quest flag the Boss beat names, and the flag-driven doctrine entitlement replacing the level benchmarks (`LevelPointEntitlement` moves with it). Then the mission tracker reads the current beat.

## Cycle 16b — two more bosses (FIELD-3)
- [ ] Second boss: add-clear under pressure. Third: mobility and sustain. `Combat.PowerCurve.BossBand` holds for all three; O31 asserted per boss. One build each.

## Cycle 17 — the ability numerics to data (O186)
- [ ] Abilities: only the numerics (`ResourceCost`, `CooldownSeconds`, `WindowDuration`, variant numbers) → `Data/abilities.json`; the registry, `AbilityClass` and tags stay a DataAsset (O186). The ~40 per-ability `O2 PLACEHOLDER` constants in the ability .cpp files are the real no-compile target and need their own item.

## Plumbing the design asks for, in the order that unblocks the most
1. Character save fields Model / Face / Voice (LEDGER, save version bump, O14): three create-rail controls and the class-select figures.
2. `ViewBobScale` and `ScreenShakeScale` on settings with their two existing consumers (KIT reads, GLASS writes).
3. `DamageNumberScale` and `NameplateScale` on settings; the nameplate scale touches an O155 member.
4. Sprint / crouch / aim HOLD|TOGGLE in Movement/ and input, before any row is drawn.
5. The ADS sensitivity consumer in the aim path (the row exists and is labelled a stub).
6. Swap picker (O205): `EquipItem(Item, DisplaceId)` and a candidate list with a pre-focus from the component (LEDGER), then the modal, pinned by `UI.EquipLimit.SwapPicker`.
7. NPC rail colour and hold-to-leave for Kess and the Quartermaster (O209), landing in `Data/dialogue.json`.
- Not to build without a system and a ruling: subtitles, text scale, reduce-flash, Forge Attune-to-rift, a MATERIALS tab, pad glyphs, the gamepad toggle, the class-select yard with 3D figures (waits on O14 models).

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

## Later (infrastructure only when it unblocks a felt item this week)
- The enemy chip re-arm inside a hold reads `GetSecondsSinceDamage() <= DeltaSeconds`; settled chips are not pruned (they hold the last fraction) and the map is bounded by live enemies in range.
- An occluded, unfocused enemy still draws its bar; only its marks and the BOSS word yield. Say if the bar should yield too.
- The Chevron mark as specified is a corner bracket; BarsVertical sits one unit off centre. Both drawn as the sheet says.
- `BreakerUI::HudEnemyBarWidth/Height` are unread; delete on the next token pass.
- `PrepareEnemyForModifierGrant` in the game-mode tests is an empty stub with live callers.
- `DoctrinePointsPerBenchmark` and the doctrine grant are constexprs the census writes; a JSON getter for `budgets.doctrine` would let the mission validator read one source.
- `CameraRollDegrees` 9 on the death beat is in no spec line (spec.md names the 12° pitch only); keep as O2 or zero it.
- With the modifier disc and light retired (O203), the Volatile corpse's fuse has no visual tell until the Niagara pass; recorded at the site.
- The Warden's front re-arms on every `OnVitalsRestored`, so a Wakeful revive gives a second front and a second punish window. If "gone for the fight" should survive a Wakeful rise, that is a ruling.
- The front break exposes the weak point but does not raise the apparatus; the player who broke the front stands at the front. Whether the break should also run the raise is an owner call. `EnterPhase` clears `bOrderRaiseActive` without closing the apparatus (a gate crossed mid-raise leaves the weak point open).
- `AggregateStats` adds every conditional line's raw value to the HUD's conditional-damage figure regardless of target or bucket; the new conditional Armour, crit and fire-rate rows inflate it.
- Deposit/withdraw return a bool and log the reason; the stash screen re-derives the refusal line. A result enum on the component (LEDGER) is the fix.
- The HUD paints every travel point's overhead prompt in rift teal; the stash point's prompt inherits it (GLASS, the HUD pass).
- `GatherDialogueFlags` / `GatherEntryFlags` in `BreakerQuestContent.cpp` carry no Breaker prefix (pre-existing).
- `player_death.wav` is not in the shipped-samples list until a sample is authored; the synth is the floor.
- The content spec's rule "The campaign is post-slice" now sits beside a Story missions section in present tense. Keep the rule or delete it.
- `Offense.WallRideDamage` is a dead row (its condition is never true); delete it when the leans are next touched.
- The DoT twin of the flat fold: `ComposeDotSourcePower` (`BreakerCombatComponent.cpp` ~:513-518) recovers the Increased bucket from the composed value, so Increased-DoT is not multiplied by the flat. Same fix as the hit path, own item.
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

### Cycles 11, 12, 15, 16 — one build, one suite, 545 / 3 / 0
- [x] The HUD to the design system, pass 1: the olive palette with System bone split from the movement-verb cyan, three borders, the text ramp, rails 4/2, gutter 40; vitals bottom-left with the max after the value (O199) and a health chip; near-death frame and brackets; abilities bottom-centre with READY / drain / hatch; weapon bottom-right with the ammo rail and the name only on swap; a four-tick crosshair with spread and ADS collapse; hit / kill / weak-point markers; zone name and mm:ss countdown; a quest one-liner; the minimap, the screen-top encounter row, the armour meter, ability digits, resource words and the weapon name at rest deleted. Seven pure pins in `UI/BreakerHUDMath.h`.
- [x] Nameplates, pass 1: scale 1.0 at 12 m to 0.5 at 35 m, draw to 60 m, borders never scale, per-rank widths; chip hatch and a 2 px shield line; band dividers off; the Elite ellipse at the feet, Champion diamonds off both bar ends, gold edge and labels gone; the ten modifier marks as system-colour geometry above the bar; the coloured modifier disc and light retired (O203). Death beat to the sheet: weapon lower 0.3 s, drop 60 cm, hard cut at 0.8. `Combat.EnemyBarMath`; photographed on the bar matrix.
- [x] Menus value pass on all eight screens: the 640 plate with the pause XP plate (O210), settings row values, class cards with Swift's open slot (O176), the create rail NAME / BODY / VOICE / FACE, the Forge diamond and RIFTGLASS (O206), the dialogue speaker/role split, inventory rows and drawn compare marks and rarity tally and tier column, the trees' DOCTRINE / CORE counters, two-level zoom and the node ladder (O204). `MenuLayout.SplitSpeakerLine`; the settings walk pins every drawn row to a live field.
- [x] Missions are data: `Data/missions.json` (act one as 20 beats over the four quests, boss unnamed) through a loader with a validator that holds each act to one benchmark and the file to the budget; export byte-identical; `Data.Missions.Fresh`, `Missions.PointBudgets`.




### Cycle 6
- [x] Affixes are data: `Data/affixes.json` (27 slice / 8 aberrant / 5 anomalous / 3 downside / 1 elemental, 26 leans, caps) through the first runtime loader `Data/BreakerDataFile`; the library is a loader with a validator, signatures unchanged; the commandlet's export matches the file byte for byte; status.py reads it; Data/ is staged.
- [x] Movement Speed is a Boots-only affix with a cap of 25 % (O192), landed as a data-only commit with no build.
- [x] The quartermaster's stock is a row: `Data/class-kits.json`, five classes, read by `GetFallbackClassDefinition`; the catalogue partition test is the identical-output pin.





