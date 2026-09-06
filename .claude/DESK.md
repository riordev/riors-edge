# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Direction (owner, 2026-09-05)
No playtest until the core loop has more oomph. The next blocks are the ones that change what a trigger pull, a shield and a kill feel like: flat damage that is really the weapon's, a boss that fights back with a shield you break, and a Niagara pass with a sound for every verb. Content plumbing waits behind them.

## Cycle 9 — a boss that fights back (O198, FIELD-3; one build)
- [ ] The Warden's front is a shield pool of 15 % of its bearer's max health: frontal hits deplete it, it breaks with a tell and stays broken for the fight, the rear stays unarmoured. `FrontalArmor` mitigation is replaced, not stacked. The boss inherits the rule. Pinned on the shipped numbers: the level-1 rifle breaks a Warden's front in N rounds; the boss's in the O18 band.
- [ ] Boss grammar as a pure header: telegraph → punish window → phase gate → add wave → arena change; the shield break is the first punish window.

## Cycle 10 — the Niagara and sound pass (GLASS-1, O179, O190, O193)
- [ ] Muzzle, impact, cast moment, death: the four `NS_<Moment>` slots filled (owner asset or Fab pack), verb-colour law kept. The muzzle flash returns here.
- [ ] The sixth verb: `PlayPlayerDeath`, `player_death.wav` slot → synth fallback, one call site in the fatal-hit branch; the O193 beat's one low sound.
- [ ] Hit feedback re-photographed: the "feedback needs to be better" frame from Part One-B, before and after.
- [ ] Delete the drift (H1): wall-ride out of the specs, dead gameplay tags out of the ini; grep-empty.

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

## Cycle 13 — LEDGER-3 and the at-cap band (one build; O200)
- [ ] 29 new affix rows as data (Breadth: every row rolls on ≥1 slot, none MorePercent; per slot ≥2 offensive, ≥1 conditional, ≥1 weapon line, ≥1 ability line). Conditional flat Added Damage prefixes are legal data-only. Then re-measure `PowerBand.AtCap` on the ruled basis; the pin is deleted when it reads inside the band, never widened.
## Cycle 14 — the stash screen (one build; over LEDGER's landed plumbing)
- [ ] Part One-X is stale: `StashCapacity` 70, `StashItems`, deposit and withdraw with the Anchor gate and O182, the crash-window reconcile and three pins all exist; only the tests call them. Append `Stash` to `EBreakerMenuScreen`, `BuildStashScreen` in its own TU (ALL / WEAPONS / ARMOUR; counter n/70; backpack n/25; MOVE TO STASH / TAKE TO BACKPACK), entered from an Anchor interactable on the F path, `-BreakerCaptureMenu=STASH`. Delete Part One-X's premise in the same commit. No MATERIALS tab: no material item exists.

## Cycle 15 — menus, inventory and trees, value pass (one build; GLASS)
- [ ] Main and pause plate 640 wide, item rows, focus rail, slide-in; settings row, slider and toggle values with every drawn row mapped to a live `UBreakerGameSettings` field (`Settings.Screen.ControlValues`) and rows with no field not drawn; class cards 360×200 with Swift's O176 open slot drawn; the create rail NAME / BODY / VOICE / FACE (BODY renamed from MODEL, still painted until the save fields exist); the forge tab strip keeps five verbs and says RIFTGLASS (O206); the pause plate carries XP and level and the combat rail goes (O210); the dialogue plate with speaker and role split from the one display string.
- [ ] Inventory and trees: the world at 40 % behind menus, tab rail-on-active, equipment rows 64 with the dashed empty frame gone, the limit chip 44 tall, two-level zoom snap, node ladder colours (spent bone, reachable panel-1 with 2 px border-high), compare marks and the rarity tally as drawn geometry, the tier badge split out of the affix string. Frame-identical in structure before and after (`-BreakerCaptureMenu=INVENTORY|SKILLTREES`).
- Then GLASS-3: `BuildInventoryScreen` and `BuildSkillTreesScreen` move to their own TUs unchanged (one build, identical frames), and each re-layout is its own cycle: the tri-zone inventory with the card rail; the card's REWRITE / SIGNATURE → prefixes → suffixes order (O67's forfeit needs LEDGER-4's field); the core wheel (needs the 11-node atlas as content under O103; `UI/BreakerCoreWheelMath.h` pinned against `06-core-wheel-geometry.json`, which is authoring input, not a runtime file); the doctrine lattice (links between spent nodes only, O204; edges and positions in progression.json).

## Cycle 16 — two more bosses (FIELD-3)
- [ ] Second boss: add-clear under pressure. Third: mobility and sustain. `Combat.PowerCurve.BossBand` holds for all three; O31 asserted per boss. One build each.

## Cycle 17 — the rest of DATA-4 (one library per commit; O186, O195)
- [ ] Quests → `Data/quests.json` (`BreakerQuestContent.cpp` becomes a loader; 4 quests, the flag registry, `ValidateQuestContent` runs on the file).
- [ ] Dialogue → `Data/dialogue.json` (`BreakerNPC.cpp` MakeXDialogue become loaders; ~26 nodes, ~70 choices; the lexicon test reads the file).
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

### Cycle 8
- [x] The source damage pools carry the flat as its own factor: the aggregator exposes its three lanes, `FillSourcePools` reads them instead of dividing the composed value, and a target rider composes `(Base + Flat)(1 + (Inc + R)/100)·More`. The defect was live (Cadence's guaranteed Added Damage under ten shipped rider rows) and small. `Pools.FlatOrder` pins the rider case; no existing float moved.
- [x] The Stage 1 design export lives at `Assets/design/` (eight sections); four censuses filed it as Cycles 10, 11, 13 and 14, the plumbing list, and sixteen questions. Five superseded canvases retired from `Assets/ui-reference/`.

### Cycle 7
- [x] O196: the pipeline already composes (Base + ΣFlat) × (1 + ΣInc) × ΠMore and `Offense.AddedDamage` is a Flat row as a fraction of the weapon's item-level base. The one defect, the Overflow rule's flat-into-Increased rewrite, is deleted; the enum value stays for saves and resolves to no rule; the row is a Prefix. `Pools.FlatOrder` pins the order and the no-second-curve contribution. A second, subtler site in the damage library's pool split is the next block's first item.
- [x] O199: a zero-max shield draws nothing on the vitals row; no max is drawn. `HUD.VitalsReadout` pins it.
- [x] The Gloves `Core.MoveSpeed` test roll is on Boots; the inert Sidearm lean row is gone; the export still matches byte for byte.

### Cycle 6
- [x] Affixes are data: `Data/affixes.json` (27 slice / 8 aberrant / 5 anomalous / 3 downside / 1 elemental, 26 leans, caps) through the first runtime loader `Data/BreakerDataFile`; the library is a loader with a validator, signatures unchanged; the commandlet's export matches the file byte for byte; status.py reads it; Data/ is staged.
- [x] Movement Speed is a Boots-only affix with a cap of 25 % (O192), landed as a data-only commit with no build.
- [x] The quartermaster's stock is a row: `Data/class-kits.json`, five classes, read by `GetFallbackClassDefinition`; the catalogue partition test is the identical-output pin.





