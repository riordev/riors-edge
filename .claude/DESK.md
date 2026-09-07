# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Direction (owner, 2026-09-06)
Push hard on today's build; the owner is out and reads the report. Cycles run in parallel on disjoint files, one build, one suite, one commit per cycle. The affix system is updated as Cycle 13 lands. A story-mission schema is drafted now so the campaign can be fleshed out. The owner's frame to check every system against: characters pick a doctrine (sub-class) a couple of levels in and unlock doctrine points through the main story quest; a Core tree every class shares scales on its own; the doctrine tree is class-specific. A current inventory of abilities, ultimates, doctrines, affixes and their scaling is owed, with each marked live, stub or unreachable.

## Direction (owner, 2026-09-05)
No playtest until the core loop has more oomph. The next blocks are the ones that change what a trigger pull, a shield and a kill feel like: flat damage that is really the weapon's, a boss that fights back with a shield you break, and a Niagara pass with a sound for every verb. Content plumbing waits behind them.

## Open questions for the owner
- (systems) O213's level N for the free Core respec. Landed as 30, O2 PLACEHOLDER.
- (systems) With the travel ring gone, Core's offered-to-spendable falls to 2.63x under a 3.0 floor the pin file itself calls seeded-not-derived. Rule the floor at 2.6, or order Core authoring?
- (systems) "The hub is the only start" is built as twelve inner-rim entries on a virtual centre. If you meant a real centre node, that is content for a later cycle.
- (bosses) The third boss ("mobility and sustain") has no story name and no act; the Act III climax is a meeting, not a fight. Name it and its act, or drop the third.
- (persistence) The amended O82 spends the endgame death budget at runtime; O122 says the two rules ship together or the limit is a loading screen, and `BreakerRiftDefinition.h` states O122's side. Every rift is Campaign today, so the decrement is wired but unreachable. Which stands?
- (enemies) The sheet's teal boss name has no string source: no strings table and no display name per family. The bar says BOSS until O195's table or a `displayName` per family lands.
- (movement) Crouch: the player has no crouch verb on its input; the slide is the crouch and the design's keybind sheet shows "L CTRL · HOLD CROUCH" as the slide key. A separate crouch verb, or crouch = the slide key held? Until ruled, the HOLD|TOGGLE rows cover sprint and aim only.
- (fun interactions) Elements: the spec names Rift, Entropy and Void (O19) with one reaction per pair; code has only a Void damage tag and the Bleed/Poison ailments. Does Void become an applied status (the ini once said healing and armour reduction) or stay a damage tag? Do enemies deal elemental damage at all? What are the three reaction cells and the interval? Do Bleed and Poison join the matrix or stay the physical lane beside it? May Provoke become a status an enemy reads as its target (a threat primitive by another name; "no class-pair specials" is honoured)?

## Cycle 12c — the death screen and the rift retry (KIT, GROUND, GLASS; one build)
- [ ] `SpendDeath`/`CanRetryRift` pure on the rift tier; the game mode's `EndgameDeathsRemaining` from `SoloEndgameDeathBudget`; `RetryRift` = the rift entry travel through the cover, `ReturnToAnchor` = the hub travel; the death beat's Black phase opens `EBreakerMenuScreen::Death` (appended last) in its own TU on the stash pattern when in a rift instance, campaign and gym unchanged (O82); RETRY THE RIFT gold focused, RETURN TO ANCHOR panel, DEATHS REMAINING n OF m with tally cells, terminal variant RETURN alone, boss line "The encounter resets."; the input handoff at Black is the playtest risk. Pin `Game.DeathBudget`.

## Plumbing the design asks for, in the order that unblocks the most
1. Swap picker (O205): `EquipItem(Item, DisplaceId)` and a candidate list with a pre-focus from the component (LEDGER), then the modal, pinned by `UI.EquipLimit.SwapPicker`.
2. NPC rail colour and hold-to-leave for Kess and the Quartermaster (O209), landing in `Data/dialogue.json`.
- Not to build without a system and a ruling: subtitles, text scale, reduce-flash, Forge Attune-to-rift, a MATERIALS tab, pad glyphs, the gamepad toggle, the class-select yard with 3D figures (waits on O14 models).

## Then, in this order, each sized when it reaches the top
1. The reaction matrix (Rift, Entropy, Void; O19) waits on its cells being named; Provoke-as-a-status waits on the threat question. The pierce-spread rule is live in `Data/statuses.json`.
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
- Two expected reds carry the campaign's absence: `KeystoneAtShippedBudget` and `NodePurchaseFlow` assert doctrine purchases at the full pool, and the shipped entitlement is two of eight until acts two, three and the finale carry their Unlock beats. Both pinned with that delete condition; never widened.
- Arrival and rift-completion flags are set only when the beat is current (the kill-counter rule). If arrival should count unconditionally, drop the flag-set argument on the two seams.
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

### Round three — one build, one suite, 560 / 5 / 0
- [x] Cycle 16c, the Core wheel under O211–O213: the travel ring is gone (117 Core nodes, 171 edges, no rank without a magnitude), five sectors as data on every constellation, dark constellations sealed on the board, class gates off the Core, `RespecCore` free until level N (30, O2 PLACEHOLDER) and Riftglass after.
- [x] Cycle 16b, the Holdfast: Act I's boss with the add-gate grammar (`AddGate` beat, gated advance, incoming multiplier while adds live), spawned by name from the mission's Boss beat and `-BreakerBossOnStart=Holdfast`; `BossBand` holds for both bosses. The third boss is found-not-built: it has no story name (Open questions).
- [x] Cycle 17, the ability numerics to `Data/abilities.json` (35 rows, 20 variants), applied over the C++ registry at load; the census exports the file byte-for-byte.
- [x] Statuses, first rule: `Data/statuses.json` with `spreadsOnPierce`; Poison spreads through any pierce at the depth-2 normalised payload, Bleed does not.
- [x] Plumbing 1–5: Model / Voice / Face on the save (version 9) and the create rail; `ViewBobScale`, `ScreenShakeScale`, `DamageNumberScale`, larger nameplates; sprint and aim HOLD|TOGGLE in input; ADS sensitivity on the look gain. Crouch has no toggle verb yet (Open questions).
- [x] Cycle 16d, the tracker line reads the current beat: SPEAK TO, the destination, the objective counter, RETURN TO.
- [x] Cycle 12b, nameplates pass 2: the boss bar 640×24 with phase marks and pips, the occlusion trace, the beyond-60 m rules, the active-modifier underline; banners on a priority queue to the sheet's three regions. The name string source stays open (O195).
- [x] Cycle 11b, HUD pass 2: the interact plate, the reload fraction on the ammo rail, verb-coloured ability rails, wave cells with the encounter total, the four resource treatments, menu chrome to bone, kit tiles fit, the XP rail off the combat HUD (O210), the ACCESSIBILITY pane with six rows.

### Cycle 16 — the mission seams, 544 / 5 / 0
- [x] A mission plays end to end in code: the character sets the mission's arrival flag when it arrives at a Travel beat's destination, the game mode sets the Boss beat's flag when its rift completes, and doctrine points are paid from the journal by reached Unlock beats instead of character levels (`SettleDoctrineEntitlement`; `DoctrineBenchmarkLevels` is gone; the counter keeps its name and a paid character keeps its points). `Missions.Progress` walks act one's twenty beats. Two tests that raised a character to 50 for the full doctrine pool are expected red until the other three acts exist.

### Cycles 11, 12, 15, 16 — one build, one suite, 545 / 3 / 0
- [x] The HUD to the design system, pass 1: the olive palette with System bone split from the movement-verb cyan, three borders, the text ramp, rails 4/2, gutter 40; vitals bottom-left with the max after the value (O199) and a health chip; near-death frame and brackets; abilities bottom-centre with READY / drain / hatch; weapon bottom-right with the ammo rail and the name only on swap; a four-tick crosshair with spread and ADS collapse; hit / kill / weak-point markers; zone name and mm:ss countdown; a quest one-liner; the minimap, the screen-top encounter row, the armour meter, ability digits, resource words and the weapon name at rest deleted. Seven pure pins in `UI/BreakerHUDMath.h`.
- [x] Nameplates, pass 1: scale 1.0 at 12 m to 0.5 at 35 m, draw to 60 m, borders never scale, per-rank widths; chip hatch and a 2 px shield line; band dividers off; the Elite ellipse at the feet, Champion diamonds off both bar ends, gold edge and labels gone; the ten modifier marks as system-colour geometry above the bar; the coloured modifier disc and light retired (O203). Death beat to the sheet: weapon lower 0.3 s, drop 60 cm, hard cut at 0.8. `Combat.EnemyBarMath`; photographed on the bar matrix.
- [x] Menus value pass on all eight screens: the 640 plate with the pause XP plate (O210), settings row values, class cards with Swift's open slot (O176), the create rail NAME / BODY / VOICE / FACE, the Forge diamond and RIFTGLASS (O206), the dialogue speaker/role split, inventory rows and drawn compare marks and rarity tally and tier column, the trees' DOCTRINE / CORE counters, two-level zoom and the node ladder (O204). `MenuLayout.SplitSpeakerLine`; the settings walk pins every drawn row to a live field.
- [x] Missions are data: `Data/missions.json` (act one as 20 beats over the four quests, boss unnamed) through a loader with a validator that holds each act to one benchmark and the file to the budget; export byte-identical; `Data.Missions.Fresh`, `Missions.PointBudgets`.
