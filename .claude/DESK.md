# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Cycle — Progression and combat (step 3)
- [x] Repair Cascade silent echoes and cancel queued echoes when its originating ultimate ends; include targets spawned during the window.
- [x] Implement Long Dark's zone-expiry pause during its originating Unmake window; actual purchased/paid casts retain damage and membership and release on expiry, cancel, death and respec without later rearming.
- [x] Split Caster elemental selections across one hit and one budget; preflight one reaction, preserve proc safeguards and single Sympathetic bonus. Resonance delivery is repaired.
- [x] Actual Siphon unlock appends Void once; starter three remain, existing unlocks reconstruct, and a legal eight-point paid Fracture path delivers Entropy/Void together.
- [x] Replace Hold's percentage substitute with an expiring per-hit cap: base 25% maximum health, solo Wall 12.5%, editable O2 tuning; paid base/purchased Wall prove large/small hits and cancellation/expiry. Vein remains uncapped.
- [x] Add ordinary conditional ability-power affixes for low resource, depleted resource and completed ledge traversal; actual rolls/equips and live condition changes increase ability damage without weapon spill.
- [x] Replace Interposition's solo shield trickle with temporary headroom behind the owned Anchor; paid cast checks rear/front/range, repeated entry and destruction without free shield.
- [x] Detonation second ultimate press releases its paid stored damage once; expiry remains a fallback, cancellation/death/keystone loss discard it.
- [ ] Finish remaining live solo node gaps.
- [ ] Finish ability-versus-weapon balance with sustained actual delivery; retain existing parity findings until their conditions are resolved.
- [x] Repair Cleave wall-only occlusion and reject Bleed on avoided/lethal hits; actual paid casts cover aligned enemies, wall, dodge, purchased parry and lethal melee income.
- [x] Reject hitscan Bleed and pierce Poison payloads on avoided hits; actual purchased Threshold/Pierce/Chain and paid shots preserve accepted originals/copies. Live aim checks confirm stationary first-shot accuracy, increased moving-ADS spread, movement cost and release recovery.

## Completed element implementation (steps 1–2, O221–O225)
- [x] Add explicit element identity, accepted-hit buildup and chassis-scaled thresholds; Entropy applies Rot with damage snapshotted from the applying hit.
- [x] Route an actual weapon conversion affix and Caster Rot through the same damage/buildup path. Resistance changes buildup only; Bleed and Poison remain physical.
- [x] Retire legacy Status.Void armour/healing reduction. Siphon keeps damage/healing and Sequence earns its third status through actual Entropy.
- [x] Show enemy/player Entropy buildup and active Rot timers; inspect actual combat captures at 1920x1080 and 1280x720.
- [x] Give Vestige melee an authored Entropy share and family buildup resistance; restore Fracture's third position as real Entropy hits. Add the missing native player status receiver.
- [x] Support Attunement converts real Cadence/Metronome recipients' weapons to Entropy, with rank-two linger, independent ownership and fire-time projectile snapshots.
- [x] Support Sympathetic adds damage-independent buildup through actual maintained buffs and gives each attacker's protected buildup its own gradual fade. Weapons and Entropy ability casts snapshot the effect.
- [x] Keep Rot damage numbers separate from physical ticks and add bounded activation VFX/audio; inspect 1080p and 720p combat captures.
- [x] Repair real projectile collision and aimed Rot placement; tune Caster burst damage and verify every zone lifetime tick against shipped trash/veteran chassis.
- [x] Validate Entropy implementation in sustained paid casting and authored encounters. Fracture continues paying and dealing damage during seconds50-60; purchased Patience increases post-opening casts against matched controls. Live ability/converted-weapon delivery and reward-loop checks pass. This is implementation coverage, not human balance acceptance; fixed progression parity remains in step3.
  - Entry now opens with4melee/1Lattice rather than12melee. Actual first-wave AI delivery runs use normal starting resources: converted and guaranteed starter rifles kill4/5 before death at11.95s; paid Fracture/Rot kills4/5 before death at24.60s and activates Rot. This basic driver approaches and aims but does not evade; results prove delivery and threat, not player skill, successful clears or balance. Negative Mana(-10.30) is legal Overcast above the authored-20floor. Entry is free despite area5; naturally earned Fernhall progression and full-run balance remain unmeasured. Buffed traveling Fracture now proves Sympathetic snapshot and timed fade; audible mix still needs playtest evidence.
- [x] Finish Void implementation: delayed Erased, paid Siphon, ordinary weapon conversion and family attack delivery validated. Altered applies Void; Vestige authored attacks consistently apply Entropy. Human balance acceptance remains separate.
- [x] Build Rift kernel and Breach delivery: accepted-hit Unstable pays once and sweeps actual enemy/player capsules against walls and ledges; status, damage and audio feedback are wired.
- [x] Finish Rift weapon conversion through actual rolled/equipped Primary hitscan and rocket fire-time snapshots.
- [x] Implement Collapse, Wither and Tear, consuming only the first status's unpaid damage; accepted-hit, callback, hitch, shortened-duration and cancellation tests pass.
O221–O225 are recorded in DECISIONS and combat intent. Element magnitudes remain O2 tuning in Data/elements.json. Element implementation is validated; human balance and audible-mix acceptance remain part of the eventual playtest.
## Current owner direction
Finish steps 1-6 before the owner playtests: Entropy, remaining elements/reactions, progression/combat, loot, interface, then Fernhall/Rift content. Step 7's full combat visual/audio identity pass follows later; Anchor and Fernhall environment presentation belongs in step 6 and is prioritized before the additional destinations. Step 6 includes two additional larger persistent destinations alongside Fernhall, campaign traversal and return Rift activities, and an in-game destination map. Ordinary enemies retain fixed regional level ranges; tougher return Rifts supply later challenge. Automated checks and inspected captures validate implementation; human balance acceptance remains separate.

## Playtest queue (owner, 2026-09-07)
The owner will playtest after the remaining Entropy pass is finished. Continue its remaining encounter tuning and presentation work through tested batches, then report for playtest; do not stop after each intermediate checkpoint.
- (ui, npcs, systems) Reduce menu density and clipping; repair NPC interaction flow, ability assignment and point spending; present Core as the authored tree rather than a node cloud; update the dev sandbox. Finish silent nodes and Caster progression.
- (maps, story, endgame loop) Finish Fernhall → Rift → reward → return, expand Fernhall into meaningful combat areas, make Anchor a hub, and author one memorable mission. Validate the complete Rift loop before expanding multiplayer/MMO scope.
- (abilities, build diversity) Improve ability builds versus weapons; investigate intermittent Rot damage, ticks and numbers and Caster Cleave range. Provide an easy ability/ultimate/base-stat tuning surface.
- (movement, weapons) Reduce sprint bob/sway; disallow sprinting while shooting; remove dash except innate Swift; give scoped/unscoped fire a tradeoff. Add base health regeneration after four seconds out of combat (owner suggests 1–2; magnitude to tune).
- (loot & economy) Fix gear behavior and show base weapon damage. Evaluate Standard 3–4, Uncommon 4–5, Exceptional 4–6 affixes with existing tier caps. Aberrants should have quirky unique rules; Unwrittens should have strong build-defining exotic perks; the no-More restriction does not apply to these two categories. A well-rolled and treated Exceptional should outperform them on some axes.
- (visuals, sound, enemies) Improve arms and weapon models, integrate weakpoints, add subtle enemy names, and make one weapon/ability/enemy/boss visually polished. Add audible combat feedback. Replace colored ability borders with placeholder icons, radial cooldown recovery and numeric timers; hide irrelevant Riftglass. Smooth the death screen and mouse handoff.

## Repair dependencies found in runtime review
- Resonance is an untyped Elemental status-count detonation; its purchased preservation keeps earned Rot at half remaining duration/budget without triggering Wither. Tank incoming resource uses the actual hit proc coefficient, including proc-zero reactions.
- Step3: split-budget Fracture preserves adjacent elements and one direct hit; Cascade skips elemental entries. Actual Siphon unlock appends Void through progression, including existing unlocks.
- Support Attunement and Sympathetic deliver Entropy through actual maintained buffs; Void/Rift choices wait for their element pipelines. Tank Kinetic Recovery consumes actual owned blast landings and protects against real fall harm and stagger.
- Doctrine progression now has all four authored benchmarks across three acts and pays 8/8 through the physical finale at level50. The real mission probe verifies actions/reloads but uses an explicit XP fixture at the final gate; normal campaign leveling pace remains unvalidated.
- New special loot now respects its final affix budget. Existing saved items remain unchanged; a migration still needs to be designed.

## Content phase after the repair list (owner, 2026-09-07)
- [ ] Visually flesh out Anchor first: lived-in market street, suppression-pylon landmark, distinct stash/Forge/quartermaster/command/gate spaces, functional verticality, warm interior/cold exterior lighting and environmental detail. Inspect actual arrival and service-route captures.
- [ ] Visually flesh out Fernhall before adding destinations: recognizable districts, terrain/building silhouettes, material and lighting coherence, landmarks, environmental detail and readable encounter routes. Preserve combat sightlines and traversal; inspect actual route and combat captures.
- [ ] Author more distinct Fernhall spaces and improve layout/content density.
- [ ] Add two larger persistent destinations alongside Fernhall, each with connected districts, distinct landmarks, deliberate encounter compositions and a campaign route; choose final names and silhouettes against the existing setting/assets during content authoring.
- [ ] Give each destination fixed regional level ranges and discoverable Rift sites that support later, harder return activities; preserve destination access after its campaign beats.
- [ ] Build an in-game map of discovered destinations/districts, connecting routes, player location, campaign objectives and Rift entrances with difficulty and tracked destination; validate its markers against actual world locations and saved discovery.
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
- (fun interactions) Provoke-as-a-status still needs a threat ruling; elemental semantics are defined by O222–O225.

## Plumbing the design asks for, in the order that unblocks the most
- Not to build without a system and a ruling: subtitles, text scale, reduce-flash, Forge Attune-to-rift, a MATERIALS tab, pad glyphs, the gamepad toggle, the class-select yard with 3D figures (waits on O14 models).

## Then, in this order, each sized when it reaches the top
1. Implement Entropy, Void, Rift, then Collapse/Wither/Tear under O222–O225. Provoke-as-a-status waits on the threat question.
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
- Siphon cycle: real ability-token unlock appends Void/Erased once, including unlock-before-component creation; unchanged sync does not broadcast. Starter three and authored entries remain intact. Legal eight-point MS7 and two paid casts deliver Entropy/Void through the real Impact seam without cursor overrides. Build/focus pass; full suite700started/697passed/3known/0unexpected. Natural flight and campaign acquisition are not claimed; STATE unchanged.
- Element shares: ordered normalized selections reuse one direct damage/crit result; first preexisting reaction suppresses all new buildup. Duplicate Entropy pays one flat bonus; application/consume callback recursion cannot react to sibling-created statuses. Actual purchased Fracture with an authored two-element fixture delivers both. Build/focused checks pass; full suite699started/696passed/3known/0unexpected. Shipping cycle remains three pending real Siphon unlock wiring; STATE unchanged.
- Detonation: real Ultimate slot second press releases the active ledger without another payment or activation notification. Cleanup precedes callbacks; death and keystone removal cancel immediately. Paid legal eight-point test covers actual damage, exact mitigated payout, no recost/replay, cancellation and death. Build and focus pass; full suite698started/695passed/3known/0unexpected through quarantine auditor. STATE unchanged.
- Interposition: solo rear field adds 10% maximum-health shield capacity within4m, grants no shield, and releases on exit, Anchor destruction, death or progression loss. Gear changes preserve their underlying capacity. Build and paid Anchor runtime pass; full suite697started/694passed/3known/0unexpected through quarantine auditor. STATE unchanged; human feel remains unvalidated.
- Cascade/Grit: actual eight-point purchased Cascade and paid Fracture/Unmake prove physical echo across elemental cycle positions, late-spawn targets, proc0/no Chain, and cancel/death/respec before queued delivery. Tank damage callback now honors real proc0/.25/1 with proportional generation. Build/suite690started/687passed/3known/0unexpected. Fixture restores campaign entitlement and uses explicit projectile Impact; does not claim campaign traversal or natural flight. Long Dark zone pause remains next.
- Reactions: Collapse/Wither/Tear spend one consumed unpaid budget; original-applier credit, proc0, no new buildup or chains. Rot ticks claim payment before callbacks; shortening cancels future budget permanently. Build/suite688started/685passed/3known/0unexpected. Inspected all7frames across three reaction types at1080p/720p: labels separate from each other; original hit still overlaps enemy nameplate, retained for step5. Overlapping activation effects and audible mix need later polish/playtest.
- Rift conversion: ordinary Primary prefix, real rifle threshold activation and marker suppression, wrong-slot isolation and launched rocket snapshot after gear replacement. Build/suite687started/684passed/3known/0unexpected. Entropy/Void/Rift each have real ability and weapon delivery; reactions remain unbuilt. Existing enemy families cover Entropy/Void; no new Rift family authored.
- Rift kernel/Breach: full suite686started/683passed/3known/0unexpected; actual paid Breach, callback cancellation/order, lethal single payout, native floor-backed activation and capsule wall/ledge tests pass. Opened both1080p and720p frames: UNSTABLE timer/damage and player RIFT60% meter readable; transient enemy-plate overlap remains step5. Screenshots do not validate movement feel or audible mix. Rift weapon conversion and reactions remain next.
- **Void weapons and family attacks:** Ordinary Primary conversion rolls route hitscan/rockets through one explicit strongest-share selection; actual ammunition earns Erased and world ticks pay once, rockets retain launch identity after gear swaps. Altered attacks carry Void and Vestige attacks Entropy across authored melee/projectile/Warden paths; modifier hazards remain separate. Skirmisher rounds now configure before BeginPlay. Runtime review exposed player capsules being counted as cover: Lattice/Skirmisher now query actual WorldStatic objects, with open/wall regressions. Build passed;683declared/started,680passed,3known,0unexpected via quarantine auditor. No new art-quality claim; projectile geometry checked through actual begun components. STATE unchanged.
- **Erased kernel and Siphon delivery:** Void earns a finite applying-hit snapshot and pays it once after its delay. Reapplication cannot enlarge/refresh it; consume, cleanse and death cancel unpaid damage. Siphon builds Void through actual paid channel hits; Zonework recognizes unpaid Erased. Separate meters/countdowns and activation/burst cues; capture-discovered Rot/Erased number overlap repaired with measured animated bounds. Final1080p/720p frames inspected: independent numbers and meters, transient numbers can still cross enemy status plates. Sustained Fracture/Patience checks pass without tuning/parity pin changes. Build passed;681declared/started,678passed,3known,0unexpected via quarantine auditor after extending volume coverage to all9voices. STATE unchanged. Void weapons/family delivery, Rift and reactions remain next.
- **Siphon damage-family repair:** Restored Elemental classification accidentally removed alongside the retired Void debuff. Actual paid channel ticks now verify equal damage before/after legitimately rolled Physical DR gear, matching leech, and a reduced physical control hit. No Erased effect or retired armour/healing debuff added. Build passed;678declared/started,675passed,3known,0unexpected via quarantine auditor. STATE unchanged under host-Python quarantine.
