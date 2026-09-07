# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Cycle — held input, six Caster nodes and a distinct Unwritten
- [ ] Release held combat inputs on menu/death, wire death confirmation keys and correct dialogue wrapping.
- [ ] Wire Close, Debt, Bloodprice, Patience, Drain and Variance to purchased ranks and editable tuning.
- [ ] Author a reachable named Unwritten that trades single-target critical damage for visible crowd forks.
Remaining repair work includes the other silent nodes, campaign doctrine reachability, ability/weapon balance, broader clipping, arms/weapon/weakpoint art, audible combat polish and the full rarity identities.

## Playtest queue (owner, 2026-09-07)
Continue through the entire repair list in tested batches without stopping after each commit for a playtest. After repairs, execute the content phase below (owner instruction, 2026-09-07).
- (ui, npcs, systems) Reduce menu density and clipping; repair NPC interaction flow, ability assignment and point spending; present Core as the authored tree rather than a node cloud; update the dev sandbox. Finish silent nodes and Caster progression.
- (maps, story, endgame loop) Finish Fernhall → Rift → reward → return, expand Fernhall into meaningful combat areas, make Anchor a hub, and author one memorable mission. Validate the complete Rift loop before expanding multiplayer/MMO scope.
- (abilities, build diversity) Improve ability builds versus weapons; investigate intermittent Rot damage, ticks and numbers and Caster Cleave range. Provide an easy ability/ultimate/base-stat tuning surface.
- (movement, weapons) Reduce sprint bob/sway; disallow sprinting while shooting; remove dash except innate Swift; give scoped/unscoped fire a tradeoff. Add base health regeneration after four seconds out of combat (owner suggests 1–2; magnitude to tune).
- (loot & economy) Fix gear behavior and show base weapon damage. Evaluate Standard 3–4, Uncommon 4–5, Exceptional 4–6 affixes with existing tier caps. Aberrants should have quirky unique rules; Unwrittens should have strong build-defining exotic perks; the no-More restriction does not apply to these two categories. A well-rolled and treated Exceptional should outperform them on some axes.
- (visuals, sound, enemies) Improve arms and weapon models, integrate weakpoints, add subtle enemy names, and make one weapon/ability/enemy/boss visually polished. Add audible combat feedback. Replace colored ability borders with placeholder icons, radial cooldown recovery and numeric timers; hide irrelevant Riftglass. Smooth the death screen and mouse handoff.

## Repair dependencies found in runtime review
- Sequence requires three distinct applications, but Caster has only Bleed and Poison. Void is not an applied status. A third functional status or a revised node contract is required; do not fake the third tag.
- Doctrine progression awards only 2/8. Remaining earned benchmarks need the authored Breach/Field Marshal, Survivor/Erased Earth, and alternate-self finale gameplay. Mission validation must support the separate Act III finale and stable encounter identities before extending content; replaying Fernhall must not counterfeit later completion.
- Ability assignment repeats the catalogue three times and jumps to the top after a click, while feedback sits below every list. Next UI slice selects one slot and pins wrapped feedback; also close equip modals on Escape and wrap NPC bench refusals.

## Content phase after the repair list (owner, 2026-09-07)
- [ ] Author more distinct Fernhall spaces and improve layout/content density.
- [ ] Shape Rift runs minute by minute with finished encounters and deliberate enemy combinations.
- [ ] Strengthen distinct class/build identities and author standout build-defining loot.
- [ ] Unify asset packs through a coherent visual/audio identity and polished effects for existing combat.
- [ ] Add mission/quest beats and one or two signature bosses or activities that define the game.
Prioritize content and encounter composition over additional enemy framework.
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
- Inventory equipped titles, affix names, rarity sublines and base damage fit the inspected1920x1080GEARDAMAGE capture after button padding/alignment repair. Other menus/resolutions remain in the clipping queue.
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

## Filing notes
Every note the owner writes carries one of these tags so the queue reads by category: AI (behaviour, not roster) · bosses · animation · VFX/hit feedback · networking/party/social · loot & economy · encounter/level tooling · content authoring pipeline · onboarding/first hour · endgame loop · performance budget · telemetry · accessibility/input · persistence · systems · core gameplay · weapons · abilities · classes · maps · enemies · story · build diversity · fun interactions · visuals · sound · movement · ui · npcs.

## Owner only
- Fab mannequin/GASP, Ultimate Modular Women, Sonniss extract (arms, anims, real audio all wait on these)
- Four Niagara systems at `/Game/Breaker/FX/NS_<Moment>` with a `Color` user parameter, or a free Fab VFX pack placed there

## Done (last three cycles; older is git)

### Held input, six Caster nodes and Refractor — 599 / 5 / 0
- [x] Menus/death clear fire, aim, sprint and slide intent. Death confirmation uses Slate, preserves focused Return/navigation and prevents duplicate travel. Wider death buttons and wrapped NPC speaker text inspected in actual Anchor captures.
- [x] Purchased Close, Debt, Bloodprice, Patience, Drain and Variance now affect combat/resource events, with editable O2 tuning. Runtime purchases and actual Siphon interruption tested; campaign entitlement remains 2/8.
- [x] Refractor is a reachable named Unwritten necklace: each hitscan pellet's first successful crit-eligible enemy hit may fork to two visible targets for 60% normal nonweak damage, trading away hitscan critical strikes. Real roll/equip/damage/exclusion/O104 tests; no recursive forks or weapon statuses. Generic OnHit listeners still see proc-zero fork damage. Projectile weapons unchanged.
- [x] All inventory rule cards now print their canonical perk/forfeit descriptions. Refractor card inspected with full text and affixes; no claim runtime beams photographed. Special-item damage permission reflected in intent docs; rarity migration remains open.
- [x] Final build, canonical census and all604tests complete:599passed/5existing expected/0unexpected. Corrected isolated-world context/reflected-event fixtures and expanded exact legendary population assertions; no damage bands/pins widened. STATE unchanged under Python quarantine.


### Caster resource nodes, sandbox and real Rift travel — 594 / 5 / 0
- [x] Purchased Seep, FollowThrough, Attrition and Payment now pay through actual status, melee, death and detonation events. New editable resource tuning documents O2 placeholders; lethal final DoT ticks pay once. Campaign still grants only two doctrine points; deeper runtime fixtures do not claim shipped reachability.
- [x] Cycle rank two displays the next status with owner-only replicated state and immediate purchase/respec updates. Sandbox has Character/Gear/Stats tabs, strict repeat seeds and explicit full-backpack refusal. Five UI captures inspected.
- [x] Added reachable ability debt/ledge affixes without weapon leakage. Honest rolled ability/weapon multiplier parity remains 0.550 at level 50 and 0.365 at level 120; balance work remains open.
- [x] Isolated real Anchor → Fernhall → authored Rift → Fernhall → Anchor probe passed: 24 kills, exact completion purse, final 2015 XP / 211 Riftglass retained. Accelerated kills validate integration, not combat feel.
- [x] Final build and all 599 tests complete: 594 passed / 5 existing expected / 0 unexpected. Canonical C++ census and PowerShell reconciliation; STATE unchanged. No test pins widened.


### Fernhall combat pockets, enemy names and DoT scaling — 588 / 5 / 0
- [x] Ordinary Fernhall now spawns 11 finite enemies across three supported combat pockets, including an elite, Lattice, Warden and Skirmisher. Existing kill quests and XP receive real combat credit; repeated startup does not duplicate spawns. Rift wave behavior remains separate.
- [x] Flat damage now multiplies the combined delivery/DoT Increased bucket. Tests cover both delivery lanes, frozen snapshots, removal and uncaptured baseline fallback without changing the More ceiling.
- [x] Seven authored enemy-type names use the strings table and existing range/focus/occlusion rules. Ground-level Fernhall capture inspected with SKITTER above its health bar. Capture-only tour targeting and velocity reset added; static capture does not validate full traversal.
- [x] Final build and all 593 tests complete: 588 passed / 5 existing expected / 0 unexpected. Runtime fixture checks supported spawn capsules, finite roster, separation, actual kills and quest progress. PowerShell reconciliation under Python quarantine; STATE unchanged. Full cross-map loading remains outstanding.


