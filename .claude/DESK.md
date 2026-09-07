# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Playtest queue (owner, 2026-09-07)
Continue through this queue in tested batches without stopping after each commit for a playtest (owner instruction, 2026-09-07).
- (ui, npcs, systems) Reduce menu density and clipping; repair NPC interaction flow, ability assignment and point spending; present Core as the authored tree rather than a node cloud; update the dev sandbox. Finish silent nodes and Caster progression.
- (maps, story, endgame loop) Finish Fernhall → Rift → reward → return, expand Fernhall into meaningful combat areas, make Anchor a hub, and author one memorable mission. Validate the complete Rift loop before expanding multiplayer/MMO scope.
- (abilities, build diversity) Improve ability builds versus weapons; investigate intermittent Rot damage, ticks and numbers and Caster Cleave range. Provide an easy ability/ultimate/base-stat tuning surface.
- (movement, weapons) Reduce sprint bob/sway; disallow sprinting while shooting; remove dash except innate Swift; give scoped/unscoped fire a tradeoff. Add base health regeneration after four seconds out of combat (owner suggests 1–2; magnitude to tune).
- (loot & economy) Fix gear behavior and show base weapon damage. Evaluate Standard 3–4, Uncommon 4–5, Exceptional 4–6 affixes with existing tier caps. Aberrants should have quirky unique rules; Unwrittens should have strong build-defining exotic perks; the no-More restriction does not apply to these two categories. A well-rolled and treated Exceptional should outperform them on some axes.
- (visuals, sound, enemies) Improve arms and weapon models, integrate weakpoints, add subtle enemy names, and make one weapon/ability/enemy/boss visually polished. Add audible combat feedback. Replace colored ability borders with placeholder icons, radial cooldown recovery and numeric timers; hide irrelevant Riftglass. Smooth the death screen and mouse handoff.

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
- (enemies) The sheet's teal boss name has no string source: no strings table and no display name per family. `bar.boss` is a `Data/strings.json` row; a per-family display name is still a `displayName` on the family, when ruled.
- (movement) Crouch: the player has no crouch verb on its input; the slide is the crouch and the design's keybind sheet shows "L CTRL · HOLD CROUCH" as the slide key. A separate crouch verb, or crouch = the slide key held? Until ruled, the HOLD|TOGGLE rows cover sprint and aim only.
- (weapons) Momentum on the gun is built as tighten-only: the cone shrinks to 0.6x at a full bar (O2 PLACEHOLDER) and tracers brighten to 1.75x; an empty bar is baseline (O92). If "follows the bar" meant something else, say what.
- (enemies) A Volatile blast hits enemies at the player's number (O217): 9x chassis damage kills every trash body inside the inner radius. Keep one number, or rule an enemy fraction after a playtest.
- (abilities) Class numbers ride `Data/abilities.json` and are written onto the class defaults at load; the compiled members stay as the failed-load fallback. Delete the compiled defaults later (as cycle 17 deleted the registry's costs), or keep them?
- (fun interactions) Elements: the spec names Rift, Entropy and Void (O19) with one reaction per pair; code has only a Void damage tag and the Bleed/Poison ailments. Does Void become an applied status (the ini once said healing and armour reduction) or stay a damage tag? Do enemies deal elemental damage at all? What are the three reaction cells and the interval? Do Bleed and Poison join the matrix or stay the physical lane beside it? May Provoke become a status an enemy reads as its target (a threat primitive by another name; "no class-pair specials" is honoured)?

## Plumbing the design asks for, in the order that unblocks the most
- Not to build without a system and a ruling: subtitles, text scale, reduce-flash, Forge Attune-to-rift, a MATERIALS tab, pad glyphs, the gamepad toggle, the class-select yard with 3D figures (waits on O14 models).

## Then, in this order, each sized when it reaches the top
1. The reaction matrix (Rift, Entropy, Void; O19) waits on its cells being named; Provoke-as-a-status waits on the threat question. The pierce-spread rule is live in `Data/statuses.json`.
2. Rift interiors (GROUND-1): three to five room shapes measured against the gap rules, feeding the wave solver.
3. Ten authored legendaries with printed forfeits (LEDGER-4, O66/O67).
4. Anomalies as the first real endgame (GROUND-3): key → run → payout as a functional test.
5. Two-seat listen-server smoke every cycle (O185), then Dungeons, then Raids.

## The voice (O195) — slots in whenever a cycle has room
- [ ] O195 string table, the menu series: `BreakerMenu.cpp` by screen (title/pause/settings, inventory, trees, …), one build each, through `BreakerStrings::Get`; the HUD, loading, stash and bar slice is live in `Data/strings.json`. `BANKED FROM THE FIGHT · SPENT IN LUMPS` on the class cards is the first menu row to move.
- [ ] Menu checklist: every screen photographed; hover/press states, transitions, type hierarchy, density, faded-disabled — a list the owner marks.
- [ ] Per-archetype weapon fire: `weapon_fire_<archetype>.wav` → `weapon_fire.wav` → synth.

## Later (infrastructure only when it unblocks a felt item this week)
- Inventory captures still show item-title truncation and the item-level label under the discard button; the new equipment requirement footer and refusal line fit. Fix the card header with the menu clipping pass.
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

### Connected Core, cooldown icons, Rot feedback and inventory headers — 581 / 5 / 0
- [x] Core overview draws all117nodes171edges12entries in5sectors; focused constellations retain actual purchase handlers and clear paths. Opaque board, fitted overview, readable labels, accurate focused purchase count and unclipped Back. Graph geometry and hit targets tested; CORE/COREPRECISION captures inspected.
- [x] Neutral ability tiles use glyph icons, clockwise radial recovery and numeric cooldown; plain DoT numbers brighter at20px. Forced half-cooldown capture inspected; existing preview banners are fixture content, not a completed gameplay run.
- [x] Rot applies poison on entry and ticks2.5damage/.5s per stack, preserving5baseDPS while advancing initial feedback/stack ramp. Runtime entry/noinstantdamage/exit-tail test passes. Inventory separates level/discard row from full-width title; equipment refusal capture inspected.
- [x] Final build clean; all586tests completed:581passed/5existing expected/0unexpected. PowerShell reconciliation under Python quarantine; STATE unchanged. Static captures do not establish hands-on feel or multiplayer correctness.
### Recovery, movement, Cleave/DoT reliability and audio routing — 578 / 5 / 0
- [x] Living authority players recover 2 HP/s after four seconds without dealing/taking damage; no passive proc payouts. Real registered player tick, incoming interruption, dead/non-player exclusion and frame crossing tested. Sprint stride blends 360→720 cm, halving full-speed cadence while retaining walking. Dash entry and gear cooldown display are Swift-only; map reachability and client dash networking still need gameplay validation.
- [x] Cleave no longer treats its target as an occluder; range 300→450 cm (O2). World tests cover actual enemy collision, intervening wall and out-of-range refusal. Statuses pay elapsed ticks only within lifetime, safely re-find after damage callbacks; zone lifetime and cylindrical broad-phase bounds corrected. Rot startup delay/feel and damage-number clarity remain queued.
- [x] Master × effects reaches all six combat voices; live sliders and test cue wired, music labeled unavailable. Anchor permanent Riftglass removed. Anchor and audio-settings 1920×1080 captures inspected; no claim of verified speaker output, motion feel or multiplayer play.
- [x] Data/README.md documents editable ability/ultimate numerics and restart workflow. Numeric test now verifies actual runtime defaults against JSON, allowing data-only tuning; compiled values remain failed-load fallback.
- [x] Build clean; all 583 declarations completed, 578 passed / five pre-existing expected reds / zero unexpected. Intermediate fixture registration/player-state and stale expectation failures corrected before final run. PowerShell reconciliation used under Python quarantine; generated STATE unchanged. No pins widened.
### Loadout choices, dialogue keys and sprint/fire exclusion — 571 / 5 / 0
- [x] Core markers use the component's real adjacency/class gate. Full refusals wrap; small node labels stay short. This does not replace the constellation-card layout with the requested tree.
- [x] Abilities can exchange compatible occupied slots or move into empty ones. Contextual grant resolution keeps a cleared starter slot from duplicating the ability moved elsewhere; tests cover fresh Caster swaps and fresh Swift moves, raw state and resolved grants.
- [x] Dialogue top-row/numpad choices share the mouse dispatcher and ignore key repeats. Vendor Escape returns to dialogue rather than the pause menu.
- [x] New fire cancels sprint; new sprint cancels held fire, with a CanFire guard. Sprint travels with saved moves; correction replay preserves current intent without replaying weapon RPCs. Component tests cover both input orders and saved flags; real multiplayer corrections and dialogue key input still need hands-on validation.
- [x] Build and suite pass, 576 declarations accounted for through the local PowerShell audit. A test-prefix collision was detected and corrected before accepting the full run. Python status generation remains unavailable under the machine rule; STATE is unchanged. One helper Python editing invocation violated that rule and was disclosed; subsequent work used PowerShell/apply_patch.
