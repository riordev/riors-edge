# Desk — what the owner will feel next time he plays

One file, one queue. Ordered by what changes the next play session, not by
system. A cycle takes the top block, lands it in ONE build and ONE suite,
pushes, and stops so the owner can play. His notes go straight in here.
Nothing in this file is a ruling; rulings are in Docs/DECISIONS.md.

## Cycle — earned Act II progression and Swift movement nodes
- [x] Connect the authored wounded-contact investigation to a dedicated finite Fernhall enemy and real death credit.
- [x] Open the Breach through the Quartermaster job; stage its armoured line, flanker, combined formation and actual Field Marshal.
- [x] Award the next two doctrine points only through the completed Act II return/turn-in.
- [x] Wire Read the Room airborne credit and Landing momentum through actual movement and purchases.
- [x] Review, build, census, run the full suite and validate the playable flow before landing.
Remaining repair work includes the other silent nodes, later campaign doctrine reachability, ability/weapon balance, broader clipping, arms/weapon/weakpoint art, audible combat polish and the full rarity identities.

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
- Doctrine progression now awards 4/8 through Acts I and II. Remaining earned benchmarks need Survivor/Erased Earth and alternate-self finale gameplay. Mission validation must support the separate Act III finale and stable encounter identities before extending content; replaying Fernhall must not counterfeit later completion.
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
### Earned Act II progression and Swift movement — 611 / 5 / 0
- [x] Dedicated wounded Drudge investigation, actual-death objective, Quartermaster report/orders, restored Breach entrance and four-wave Field Marshal job now earn the next two Doctrine points. Stable encounter identity and verified active-boss death prevent developer completion from awarding story progress.
- [x] Swift Read the Room and Landing use purchased ranks and actual movement. Ground-only credit refill, continuous fall distance, teleport/traversal resets and once-per-landing gains are tested.
- [x] Fixed two real integration failures: offer-node gates hid their own dialogue choices, and silently restored journals did not refresh the physical entrance. Added visible-choice and same-journal save-restore regressions.
- [x] Final build/census and616tests:611passed/5known/0unexpected. Fresh isolated fully earned Act I-II run passed111kills, actual travel/contact/Marshal/return and exactly4Doctrine points. Damage accelerated; no combat-feel or Slate-input claim. Contact and Marshal frames inspected; Marshal solid shield panel remains visual repair. STATE unchanged.

### Two-target Lead, special loot budgets and Rift identity — 604 / 5 / 0
- [x] Purchased Lead retains two independently timed targets and renders both diamonds; weapon hits honor both, Ledger refunds once per cast, Mark Economy preserves lifetime/refund identity. Actual GAS/weapon tests plus shipped enemy geometry; live Fernhall capture shows both marks.
- [x] Lead now targets the weapon collision channel and rejects scenery/dead combat targets. Live capture found the mismatch hidden by the old synthetic fixture. Capture probe purchases nodes and casts normally under isolated saves.
- [x] Special loot reserves signatures, special lines and paired downsides inside final count/category budgets. Ordinary rolls and existing saved items unchanged. New slot/level/seed coverage preserves all four legendary identities; Refractor card inspected with five affixes.
- [x] Rift boss/story lookup uses stable EncounterId carried through travel/retry, never display copy. Renames, duplicate display names and missing IDs tested. Real round trip passed again:24kills,2015XP,231Riftglass retained; accelerated integration, not combat-feel validation.
- [x] Final build and609tests complete:604passed/5existing expected/0unexpected. Strict pool-membership test fixed without widening counts; real enemy fixture registers its existing ASC attributes. STATE unchanged. Read-only MeshAudit confirms both imported mannequins mix arm/body weights in both sections; no arms replacement claimed.
### Ability picker and five Caster nodes — 602 / 5 / 0
- [x] One selected ability catalogue, equipped slot summaries and pinned feedback replace the repeated lists. Actual Anchor capture inspected with all six Caster choices and unlock refusal fitting. Escape dismisses equip modals; bench refusal text wraps.
- [x] Standing Water pays one live-occupied Rot stream; Zonework reconciles extra flat strip across overlapping zones; Wellspring follows intentional self-ground placement and refreshes one following zone. Mobile rim now follows actor transform; movement/overlap verified in runtime tests, no rendered mobile-ring capture yet.
- [x] Chain copies the newest accepted status without recursive spread/double duration scaling and excludes player teammates. Momentum Transfer uses successful Closequarter arrival and only the matching next eligible melee hit. Seven new ability numbers are editable; Chain reach remains editor-only.
- [x] Build and all607tests complete:602passed/5existing expected/0unexpected. New fixtures respect actual prerequisites, eight doctrine points, resource costs and GAS activation. Canonical census clean; STATE unchanged under quarantine. No pins widened.
- [x] Rifle-arm prototype photographed and reverted because its torso intruded into the camera; art item remains open.
