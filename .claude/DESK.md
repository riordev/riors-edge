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

## Cycle 11 — the HUD to the design system, pass 1 (one build)
- [ ] Tokens: the olive palette, three borders, the text ramp, `System` bone split from `VerbMove` cyan (O179 keeps cyan on the movement verb; chrome moves to bone), accents with dims, teal-2 and rift-hot, the rarity ramp, rails 4/2, gutter 40. Fonts are their own editor-run commit; Source Sans 3 and Sometype Mono are not staged.
- [ ] Drawing on data the HUD holds: vitals bottom-left 480 wide with the max after the value (O199), the shield layer and the health chip (drain 0, hatch 400 ms, recover 600 ms); near-death 8→16 px frame and brackets under 20 %; abilities bottom-centre 64/88/64 with READY / COOLDOWN drain / LOCKED hatch; weapon bottom-right with the ammo rail at rest and the name only on swap; crosshair four ticks with spread 16→40 and ADS collapse; hit / kill / weak-point markers 80 / 200 / 320 ms; the damage-number timeline; zone name and `mm:ss` countdown top-left; quest one-liner; a hatch primitive.
- [ ] Deletions the export orders: the minimap (O155 crossing declared: the `EnemyBlips` consumer goes, the producer stays), weapon name at rest, ability digits, resource state words, the screen-top encounter row, the armour meter. The XP rail leaves when the pause plate carries it (O210, Cycle 15). Sizes and the aggregation window stay at the play measurements (O207); crit stays orange (O208).
- [ ] Pins: pure timelines in `UI/BreakerHUDMath.h` (crosshair, chip, pulse, swap slide, damage numbers, countdown, tally) and `HUD.ShippedTokens`; `-BreakerCaptureHUD` with forced reload, swap and cooldown states.
- Pass 2, own block: the interact/loot plate; a reload-fraction accessor in Weapons/; an ability verb on the definition for rail colours; wave cells need an encounter total (O120); the four non-Swift resource treatments; menu chrome to bone (GLASS-3).

## Cycle 12 — nameplates to the design system, pass 1, and the death beat's numbers (one build)
- [ ] `Combat/BreakerEnemyBarMath.h`: scale 1.0 at ≤12 m to 0.5 at ≥35 m, draw to 60 m, borders never scale, per-rank widths 96/160, pixel floors; the chip hatch on a per-enemy shown-health map (declared as a shared HUD member beside O155's four); the shield as a 2 px line along the top of the fill; band dividers off (state untouched, the O135 pin survives); Elite as a 2 px ellipse at the feet and the coloured modifier disc and light retired (O203); Champion diamonds off both bar ends, Trash none; gold edge and gold labels gone; the ten modifier shapes above the bar at every range, words and letters gone. Pin `Combat.EnemyBarMath`; photograph the bar probe at 12 m and 35 m.
- [ ] Death beat to spec.md's numbers (all O2): weapon lower 0.0–0.3 as its own field, camera drop 60 cm, hard cut at 0.8 (fade tail 0), black to 2.0, fade to 2.4. The test asserts the shipped timeline field by field.
- Pass 2, own block (ruled: O156, O82, O203): the boss bar world-space with phase marks off the authored gates and pips; the occlusion trace and the >60 m rules; names from the O195 table; the active-modifier underline; the death screen and the rift retry; banners to the design with a priority queue.

## Cycle 15 — menus, inventory and trees, value pass (one build; GLASS)
- [ ] Main and pause plate 640 wide, item rows, focus rail, slide-in; settings row, slider and toggle values with every drawn row mapped to a live `UBreakerGameSettings` field (`Settings.Screen.ControlValues`) and rows with no field not drawn; class cards 360×200 with Swift's O176 open slot drawn; the create rail NAME / BODY / VOICE / FACE (BODY renamed from MODEL, still painted until the save fields exist); the forge tab strip keeps five verbs and says RIFTGLASS (O206); the pause plate carries XP and level and the combat rail goes (O210); the dialogue plate with speaker and role split from the one display string.
- [ ] Inventory and trees: the world at 40 % behind menus, tab rail-on-active, equipment rows 64 with the dashed empty frame gone, the limit chip 44 tall, two-level zoom snap, node ladder colours (spent bone, reachable panel-1 with 2 px border-high), compare marks and the rarity tally as drawn geometry, the tier badge split out of the affix string. Frame-identical in structure before and after (`-BreakerCaptureMenu=INVENTORY|SKILLTREES`).
- Then GLASS-3: `BuildInventoryScreen` and `BuildSkillTreesScreen` move to their own TUs unchanged (one build, identical frames), and each re-layout is its own cycle: the tri-zone inventory with the card rail; the card's REWRITE / SIGNATURE → prefixes → suffixes order (O67's forfeit needs LEDGER-4's field); the core wheel (needs the 11-node atlas as content under O103; `UI/BreakerCoreWheelMath.h` pinned against `06-core-wheel-geometry.json`, which is authoring input, not a runtime file); the doctrine lattice (links between spent nodes only, O204; edges and positions in progression.json).

## Cycle 16 — the story-mission schema (doc + data, no build)
- [ ] `Data/missions.json` (the act-one draft: 13 beats over the four quests, boss id empty until named), `Save/BreakerMissionContent` loader + validator (ids unique; every flag registered; every npc/node, rift, quest id resolves; beats ordered; Σ doctrine points == 8; every corePoint a known source), census export, `Data.Missions.Fresh` and `Missions.PointBudgets`. Then the three seams: an arrival flag on travel, rift completion → flag, and the flag-driven doctrine entitlement replacing the level benchmarks (`LevelPointEntitlement` moves with it).

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

### Cycles 9, 10, 13, 14, 16 (schema), 17 (quests, dialogue) — one build, one suite, 534 / 3 / 0
- [x] O198: the Warden's front is a pool of 15 % of max health that breaks for the fight; FrontalArmor is gone; the boss inherits it. Level-1 rifle: Warden front in 5 rounds (0.4 s), boss front in 37 rounds (3.6 s), inside O18's 15 % of the band. Boss grammar as a pure header: telegraph → punish window → phase gate → add wave → arena change derived from the shipped numbers; the front break opens the first punish window (2.5 s, O2). `Warden.FrontBreaks`, `Boss.Grammar`.
- [x] O190 recipe at `Content/Breaker/FX/README.md`; the four NS_ assets stay Owner-only and the code side was done in Cycle 2. O193 sixth verb `PlayPlayerDeath` on the beat's cut (0.8 s), synth 110→55 Hz. H1: wall-ride out of the specs; the tag ini holds only the three requested `Status.*` tags.
- [x] LEDGER-3: 56 slice rows (29 new, all conditional lines against existing plumbing); AtCap measured on a rolled best-in-slot (O200): 6.38× against 8–10×, pin kept and rewritten to the basis. `RolledBestInSlotIsGameLegal`.
- [x] The stash screen over LEDGER's landed plumbing: an Anchor stash point on the F path, 10×7 / 5×5, ALL / WEAPONS / ARMOUR, Necklace under ARMOUR, painted verbs outside the Anchor and at the caps; `-BreakerCaptureMenu=STASH` photographed. Part One-X deleted from ORDERS.
- [x] Story missions: the schema is a spec section (beat vocabulary, one flag registry, doctrine points two per main-story benchmark by Unlock beats: O43 and O86 amended); the data and loader are the next Cycle 16 item.
- [x] Quests and dialogue are data: `Data/quests.json` (4 quests, 5 objectives, 20 flags) and `Data/dialogue.json` (Kess and the Quartermaster, 24 nodes, 59 choices, 9 entries), loaders with validators, exports byte-identical, every deleted literal accounted for. Two libraries in one commit because they share the census export TU.



### Cycle 6
- [x] Affixes are data: `Data/affixes.json` (27 slice / 8 aberrant / 5 anomalous / 3 downside / 1 elemental, 26 leans, caps) through the first runtime loader `Data/BreakerDataFile`; the library is a loader with a validator, signatures unchanged; the commandlet's export matches the file byte for byte; status.py reads it; Data/ is staged.
- [x] Movement Speed is a Boots-only affix with a cap of 25 % (O192), landed as a data-only commit with no build.
- [x] The quartermaster's stock is a row: `Data/class-kits.json`, five classes, read by `GetFallbackClassDefinition`; the catalogue partition test is the identical-output pin.





