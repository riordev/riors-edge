# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Cycle — Support Metronome and Conduit
- [ ] Apply real per-holder weapon-hit ramps and purchased ally clauses with independent owned cleanup.
- [ ] Make Conduit Downbeat count actual buff recipients and keep flat damage in the weapon lane.
- [ ] Verify actual casts, overlap, proc weights, expiry and death; review, build, census and full suite.
Remaining repairs include Support element nodes, ability balance, rarity identities, other weapon poses and audible combat polish.
## Playtest queue (owner, 2026-09-07)
Continue through the entire repair list in tested batches without stopping after each commit for a playtest. After repairs, execute the content phase below (owner instruction, 2026-09-07).
- (ui, npcs, systems) Reduce menu density and clipping; repair NPC interaction flow, ability assignment and point spending; present Core as the authored tree rather than a node cloud; update the dev sandbox. Finish silent nodes and Caster progression.
- (maps, story, endgame loop) Finish Fernhall → Rift → reward → return, expand Fernhall into meaningful combat areas, make Anchor a hub, and author one memorable mission. Validate the complete Rift loop before expanding multiplayer/MMO scope.
- (abilities, build diversity) Improve ability builds versus weapons; investigate intermittent Rot damage, ticks and numbers and Caster Cleave range. Provide an easy ability/ultimate/base-stat tuning surface.
- (movement, weapons) Reduce sprint bob/sway; disallow sprinting while shooting; remove dash except innate Swift; give scoped/unscoped fire a tradeoff. Add base health regeneration after four seconds out of combat (owner suggests 1–2; magnitude to tune).
- (loot & economy) Fix gear behavior and show base weapon damage. Evaluate Standard 3–4, Uncommon 4–5, Exceptional 4–6 affixes with existing tier caps. Aberrants should have quirky unique rules; Unwrittens should have strong build-defining exotic perks; the no-More restriction does not apply to these two categories. A well-rolled and treated Exceptional should outperform them on some axes.
- (visuals, sound, enemies) Improve arms and weapon models, integrate weakpoints, add subtle enemy names, and make one weapon/ability/enemy/boss visually polished. Add audible combat feedback. Replace colored ability borders with placeholder icons, radial cooldown recovery and numeric timers; hide irrelevant Riftglass. Smooth the death screen and mouse handoff.

## Repair dependencies found in runtime review
- Support's Attunement/Sympathetic require distinct element conversion/buildup consumers. Tank Kinetic Recovery now consumes actual owned blast landings and protects against real fall harm and stagger.
- Doctrine progression now has all four authored benchmarks across three acts and pays 8/8 through the physical finale at level50. The real mission probe verifies actions/reloads but uses an explicit XP fixture at the final gate; normal campaign leveling pace remains unvalidated.
- New special loot now respects its final affix budget. Existing saved items remain unchanged; a migration still needs to be designed.

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
- `KeystoneAtShippedBudget` and `NodePurchaseFlow` now pass through completed campaign journal fixtures at level50; their expected-red entries were retired after the physical mission probe reached eight and survived reload. All original numeric purchase/refund/Core assertions remain unchanged.
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
- **Tank landing and stagger:** Binary timed interruption with resistance and immunity cancels actual enemy windups and interruptible player channels. Ground Zero waits for physical landing and scales from measured fall distance with the purchased Terminal Descent cap. Ordinary falls damage shields/health; owned Breach landings within three seconds preserve takeoff self-damage while granting Kinetic fall protection and 1.5-second stagger immunity. Foreign launches, teleport, death and respec refuse stale protection. Build/census clean; **654 tests, 651 passed, 3 known failures, 0 unexpected**. Physical fall fixtures use real rolled shield gear and retain original resource/damage assertions. Native Rift loop PASS:24kills,2015XP,217Glass. Warden slam delivery inspected, not separately exercised by automation. STATE unchanged under host-Python quarantine.

- **Support Cadence and Section (2026-09-07):** Living player aura and self-first copy, walking-speed footprint and Section sprint follow, stationary Baton, owned overlapping windows and strongest reload/swap tempo. Conducting shortens actual recipient cooldowns; self detach-tail and Downbeat duration survive re-entry and extension. Charge upkeep belongs to the maintaining caster rather than receiving Supports. Exit, expiry, cancellation and death remove only the owning cast. Six editable numeric keys bring the registry to 141. Build/census clean; **651 tests, 648 pass, 3 known, 0 unexpected**. Corrected isolated fixture clocks and purchased prerequisite ranks within the authored eight-point budget. Native Rift loop **PASS: 24 kills, 2015 XP, 211 Glass**. Both captures inspected; opaque fill rejected and replaced by a boundary ring. Actual Conduit Downbeat activation has inspection-only coverage; broader Metronome ally clauses and element nodes remain. STATE unchanged under host-Python quarantine.
- **Caster Void and Sequence:** Void is a timed, non-damaging armour/healing debuff with normal avoidance/immunity, refresh, Chain, consumption and expiry. Real Siphon hits and Fracture's third cycle position apply it. Purchased Sequence rewards three distinct applications on one target, with per-target cooldown, weakest-proc scaling and suspension/death/respec guards; unrelated progression updates preserve a rotation. Status/resource tuning and node numbers are readable data. Build/census clean; **649 tests, 646 passed, 3 known failures, 0 unexpected**. Real status, ability/projectile and Sequence tests pass; native Rift loop passes 24 kills, 2015 XP and 202 Riftglass retained. STATE unchanged under host-Python quarantine.
- **Primary accuracy, Pierce and traversal:** Sustained accuracy reduces accumulated bloom in actual shots and HUD prediction. Pierce uses whole tier steps across drops, Prolific, Temper, Reforge and Attune, excludes rockets and respects total penetration travel. Existing wall-ride item IDs now pay after real completed vault/mantle. Forge uses exact roll bands and wraps long names. Build/census clean (64 ordinary affixes); **646 tests, 643 passed, 3 known failures, 0 unexpected**. Corrected an exhausted endgame test target without changing damage assertions. Forge capture inspected after repairing the observed clipped label; STATE unchanged under host-Python quarantine.
- **Primary magazine and range:** Two ordinary Primary prefixes use the shared tier ladder and real capacity/falloff/travel consumers. Expanded magazines reload from reserve; shrink and inactive-slot changes conserve rounds. Temporary capacity stays with its origin gun, including paid-round refunds after swapping. Fixed equipment callback ordering during swaps and paid conversion. Build/census clean (62 ordinary affixes); **643 tests, 640 passed, 3 known failures, 0 unexpected**. Actual equipped-item/reload/range/rocket tests pass. Native Rift loop passes with 24 kills, 2015 XP and 201 Riftglass retained through return travel. STATE unchanged under host-Python quarantine.
- **First-person rifle and cropped arms (2026-09-07):** Unreal-generated Manny arms preserve both chains, skeleton/materials and one LOD; generation/fresh-process audit passes with34,926 triangles and18,882 render vertices. Removed seven disconnected source vertices without relaxing the weight filter. Gun_Rifle replaces AR_1 with measured forward axis, fitted grips, corrected hip/ADS framing, actual reload-clock sampling/cancellation and owner-only rendering. Shot motion uses the recoil spring; additive fire clip, other archetypes, fixed magazine and imported materials remain unfinished art/animation. Tracers/VFX follow the presented muzzle using a measured datum validated against geometry, with no packaged CPU vertex reads. Build/census clean; **640 tests,637pass,3known,0unexpected**. Native Rift loop **PASS24kills,2015XP,211Glass** with actual travel/reward persistence. Hip/ADS/fire/reload frames inspected; final ADS recaptured alone after three parallel render runs exceeded shared VRAM. STATE unchanged under host-Python quarantine.
- **Earned finale benchmark (2026-09-07):** Researcher briefing/reconstruction, separate Stripped Earth with12finiteVestiges and physical fragment recovery, separate intact Winning Earth with friendly animated player-mesh alternate, actual Anchor return and guarded level50 SEAL/HOLD device. Four unique benchmark IDs pay2each across3acts; atomic choice publishes completion/branch before persistence and rejects repeats/opposite choice. Both endings keep identical rewards/Rift access. Fixed Unreal mesh-default collision overriding decorative NoCollision in both Earth builders. Final build/census clean; **638 tests,635pass,3known,0unexpected**. Retired two campaign purchase expected reds with original budgets preserved. Full native mission run **PASS138kills,eightDoctrine after two map reloads**, `Saved/LoopProbe/3bc0d554e0ef4f74b7ce7472f04e2906/loop.log`; explicitly refused level6 then used XPfixture50, no natural-leveling claim. Earlier photo runs correctly refused fragment recovery after the test player died during screenshot dwell; normal headless mission run passed. Stripped/Won/alternate frames inspected; all environment art remains blockout. STATE unchanged under host-Python quarantine.
- **Earned Survivor rescue (2026-09-07):** Separate Quiet Earth map, garden/settlement/causeway/terrace blockout, physical swept escort, three finite Vestige pockets (15 enemies), visible lucidity countdown and retryable timeout/death. Ordered dialogue → actual extraction → actual Anchor arrival → rescued resident turn-in pays two Exceptional ilvl30 items and +2 Doctrine (six cumulative); dialogue cannot counterfeit extraction. Destination map verification and all-NPC quest validation corrected. Build/census clean; full suite **632 tests, 627 pass, five known failures, zero unexpected**. Fresh isolated `-Survivor -Photos` campaign **PASS,126 actual deaths,six earned Doctrine**, log `Saved/LoopProbe/92c0720783194c05b0d587a27edef5bc/loop.log`; garden/causeway frames inspected. Probe relocates only test player and accelerates damage, not Survivor movement; no combat pacing/input claim. Environment/NPC art remains visibly placeholder. STATE unchanged under host-Python quarantine.
### Swift traversal income and Damage Ramp — 623 / 5 / 0
- [x] Contact now grants brief income after actual vault/mantle completion, with purchased ranks, fractional timing and the existing income/anti-farm caps. Corrected the wall ray that missed50–80cm vaults; real collision tests cover vault/mantle, obstruction, teleport, death and respec within the currently attainable four-point wallet.
- [x] Ordinary Primary Damage Ramp rolls a per-stack Increased weapon bonus with ten-stack cap, once-per-shot hitscan/shotgun accrual and actual rocket impact ordering. Purchased Redline Trigger doubles accrual at Redline. Miss/swap/removal/death and stale projectile tokens reset safely; weapon DoT snapshots share the additive contribution.
- [x] Visible ten-cell stack HUD and per-stack card wording; HUD preview inspected. Later Redline purchase test explicitly uses full authored eight-point wiring fixture, not current campaign reachability.
- [x] Build/census clean (60 ordinary affixes);628 tests complete:623 passed,5 known,0 unexpected. Teleport fixture corrected to land on ground so ordinary airborne income cannot masquerade as Contact; original cancellation assertion preserved. Fresh real default loop passed24kills,2015XP,208Riftglass with reward persistence. STATE unchanged.
### Parry, special damage sources and compact inventory — 621 / 5 / 0
- [x] Purchased Parry is a rebindable V action with a frontal one-hit window, Read extension, successful Counterweight bonus, owner replication and combat recovery timing. Real purchase/input/runtime tests cover exclusions, cooldown, respec and death; ready HUD preview inspected.
- [x] Gear/tree damage More sources share strongest-three selection across weapon, ability, shared and DoT lanes. Reserve Surge rolls an ability payoff while resource-low with the actual paired weapon downside; live equip/direct/DoT/removal and counterfeit rejection are covered.
- [x] Empty inventory preview removed; equipment, totals and three readable backpack cards use the space. Wide-screen sizing converts physical pixels through Unreal's actual UI scale;1920 and1280 frames inspected, footer wrapping repaired.
- [x] Build/census clean;626 tests complete:621 passed,5 known,0 unexpected. Integration caught and fixed erased source records and missing DoT selection; obsolete warning fixture now asserts source rejection with the original numerical ceiling. Historical parity0.542 unchanged; rolled cap0.608/endgame0.738 remain below goal. STATE unchanged.
### Ability flat power and integrated Marshal module — 616 / 5 / 0
- [x] Added Ability Power rolls on ordinary gear and feeds the ability multiplier's Flat bucket; direct damage, captured DoT, weapon isolation and equip/unequip are tested. Item and Forge values print percentage units.
- [x] Marshal's visible command module receives weakpoint hits; inherited head orb stays disabled across body/revive/visibility paths. Exposure is applied at spawn, follows real Orders/front-break windows and lights at rest. Compact ribbed module and telescoping mast replace the disconnected slab; Holdfast presentation remains separate.
- [x] Build/census clean (59 ordinary affixes); 621 tests complete: 616 passed, 5 known, 0 unexpected. Fixed historical parity stays 0.542; unchanged rolled search measures 0.569 cap and 0.732 endgame. The larger ability balance deficit remains open; no pins or search fixtures changed.
- [x] Actual ordinary item card and rear-closed/front-raised Marshal frames inspected. Fresh earned Act I-II loop passed 111 kills and 4 Doctrine. Photo probe waits the authored full order cadence and repositions its live player outside melee on real floor; no damage-feel claim. STATE unchanged.
