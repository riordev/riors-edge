# Desk — next playtest

## Working scope

Continue validated blocks until the owner returns. Prioritize playable changes for the next large playtest. Investigate an individual bug for at most five minutes, then record its evidence and park the affected change if unresolved. Independent agents stage on disjoint files; one engine build/suite runs at a time. Every landed block follows BUILD → SUITE → Scripts/status.py → COMMIT → PUSH. Larger changes receive independent review. Never count placeholders or pure-maths tests as complete gameplay.

## Immediate queue

1. Cooperative sandbox landed with real host/guest fire and replicated health/death evidence. Owner slot metadata and guest health/input respawn transport now verified; real Slipcut prediction/payment/server cadence and same-key cleanup now verified; remaining acceptance: other abilities/classes, latency/rejection bank convergence, drop contention, rejoin, human presentation/movement feel and eventual campaign support.
2. Red Basin/Station Zero/Port Meridian/Broken Coast/Shatterpoint prototypes and Station priority hunt landed; Red Basin recovery/extraction, Broken Coast uplink transmission and Port Meridian ground-crew escort also landed; next content acceptance is real return Rifts, route pacing and region scenery. Refine exposed boundaries and repeated cover. Anchor surroundings/work-area dressing landed but remains blockout art.
3. Volatile countdown and ability-menu wrapping/empty-label fixes landed with rendered checks. Inventory destruction prose now wraps within its plate and failed single-item removal reports failure. Continue interactive focus/death flow and other menu screens.
4. Finish remaining Afterimage consumers and real-clock Rot feedback checks. Cache prompt now shares opening eligibility and stays visible at the console; final prop art remains.
5. Kit expansion under O252–O260: current34 registered entries are11 actives and10 passives short of55. Carom, Coup, Backstep, Pyre, Riftlance, Recall, Bulwark and Overwatch are named candidates without sufficient authored mechanics in the current docs. Rover/Barrel need new mechanics; Wildcard has O257 but needs Forge/foreign-grant/resource infrastructure. Do not fill the count with clones or call existing innate nodes slot passives.
6. World Core Point coverage counts mission Unlock.CorePoint references. Four of15 sources are authored; the remaining11 are unmapped content, not eleven broken wallet writes. Startup and flag delivery already settle grants. Assign real source/objective identities before adding any grant; do not award a named source from a merely similar mission.

## Seven-step acceptance checklist

| Step | Present evidence | Remaining acceptance work |
|---|---|---|
| 1 Entropy | Finite funding, original critical samples, paid duration and persistence tests landed | Actual encounter clarity, audible feedback and sustained ability/converted-weapon play |
| 2 Elements/reactions | Entropy, Void, Rift and bounded reactions have native delivery tests | Regression check current builds, readable combined feedback and complete encounter loop |
| 3 Progression/combat | 187 Core nodes; 370 total nodes; no measured silent nodes; Caster doctrines12/24; allocation parity0.93 | Audit actual consumers/acquisition, remaining kit programme, solo-node gaps and gameplay balance |
| 4 Loot | New special budgets, overflow conversion, Unwritten rename and skill-level affixes landed | Authored perks/legendaries, real legal-loadout comparison and UI clarity; legacy-item archive/migration protection now tested |
| 5 Interface | Core wheel and paid purchase/assignment paths exist | Occlusion, clipping across menus/resolutions, NPC/equipment/death focus flows and dev sandbox |
| 6 Fernhall/Rifts | Five outdoor pockets/17 enemies, Watchkeeper contract, campaign flags and local map exist | Further authored spaces, distinct Rift interiors, reward/return and natural leveling; guarded physical caches now validated |
| 7 Visual/audio | Placeholder presentation and combat feedback exist | Anchor/Fernhall identity, weapons/arms/weakpoints, coherent sound/VFX and signature encounters; inspect captures, do not claim final art |

## Destinations and missions

- Anchor 13: main hub; Fernhall Approach: overgrown industrial with a cityscape.
- Red Basin: scorched farmland/crater. Station Zero: overrun research hub. Port Meridian: destroyed airport/terminal. Broken Coast: ocean/flat landscape. Shatterpoint: city sprawl.
- Improve Anchor/Fernhall routes and landmarks; author playable placeholder destinations/tilesets and varied objectives using available assets. Ordinary regional difficulty stays fixed; return Rifts provide harder encounters.
- Existing local-map site IDs contain coordinates: replace with stable identity and preserve discovery before relocating existing sites.
- Campaign Substation enclosure candidate is parked after five-minute native arrival-floor failure: marker600,0,0 lies inside imported floor bounds, but simple WorldStatic floor query returns no hit. Roof, light positions and structural identity pass. No new interior landed; candidate/diagnostics retained outside checkout.
- Native Rift wave/boss/purse/exit-offer checks now wait through the real breather clock. Complete door entry→cross-map return and combined earned Act1 journal/reward persistence remain unproven as one chain; use actual accept/arrival/death/pickup/turn-in consumers, not restored completion flags.
- Rift interior currently reuses the yard. BuildCoverField generates gym-specific sections; FernhallFieldParams validates authored layouts and is not a compatible generation recipe. Use a genuinely band-aware generator or independently composed interiors; retain RiftGeneratedField as a measured probe.
- Rift seed arithmetic now hashes canonical encounter text rather than process-local FName indices; golden70439488 verified for breach.marshalling/area12/base20260814. Nonempty-ID generated arrangements change once; no persisted run-seed schema exists. This is deterministic input arithmetic, not full topology/transport acceptance.
- O262 acquisition/modifier/failure rulings remain needed: where keys drop, allowed modifier pool/count, and refund versus retained admission after failed travel. Current equipment-shaped item storage cannot safely impersonate a key.
- O262: command post consumes a key carrying area level/modifiers and determines layout. Consumable entry and death budget land together. A key, device and tileset are not complete until enter → fight → reward → return works.
- Add mission variety only with a real objective consumer. Current ordinary quest progress supports kills and the specific residue collection path; generic collection needs implementation.

## Multiplayer

Opt-in fixed-Fernhall cooperative sandbox exists: distinct transient Swift profiles, client geometry, authored late-guest start, no persistent writes, scoped friendly-fire prevention. Real two-process guest fire/server damage/death/client health transport passed; see Docs/reports/coop-combat-2026-09-09.md and Scripts/coop-combat-verify.ps1. Owner slot IDs/real GAS specs and actual guest death/server-timer respawn/input restoration are verified. Real Slipcut local/server payment and prediction-key cleanup are verified. Full campaign/progression, other abilities/classes, exact bank convergence under latency/rejection, loot contention, rejoin and human presentation/feel remain unvalidated. Trading, account services and MMO infrastructure stay outside this playtest slice.

## Known issues to time-box

- Warden engaged movement bypasses arrival spacing and can walk through the player. Arrival-ring candidate is parked after five-minute native fixture limit: naked100HP target dies to native level-one124.51 slam before later spacing checks; needs a legal equipped target and post-BeginPlay range sampling, not health/damage overrides. No production change landed.
- Cleave's reach is read CENTRE TO CENTRE (BreakerMeleeSweep::IsInsideArc), so a body already touching the overlap sphere is refused when its actor origin sits past the range — the swing lands short of what the player sees by both capsule radii. Recorded at the site. The authored 650 carries the difference; fixing it properly means the arc test taking a target radius, which is an API change worth making when a second melee verb exists.
- WalkSpeed moved 595 -> 640 alone; SprintSpeed stayed at 990, narrowing the sprint gain to 1.55x. If the next report is "sprint no longer feels like anything", raise that one dial and nothing else.
- Core overview names landed but the node markers are still small grey squares at the fit zoom, and the harness cannot hover: node detail, tooltip and wheel-zoom legibility remain unverified by anything but the owner's hands.
- CASTER CAST TIME, owner-ruled and NOT BUILT: "mana is the casters cooldown but there should be like a cast time for spells ... they shouldnt be able to appear instantly". No Caster ability has a pre-effect delay; Cleave's AnimationLockSeconds is a POST-effect lock on re-activation, which is not the same mechanism. Needs rulings before any code: does incoming damage interrupt a cast, does movement, is Mana spent at the start or at the landing, and does an interrupted cast refund? A cast bar is a HUD window (O179: windows stay HUD bars) and does not exist. Its own block.
- Landing/slide findings recorded at the site during O264 and deliberately not repaired: bSlideOwnsLanding exempts a merely REQUESTED slide from the landing cost entirely (crouch held in the air pays no toll at any fall speed) when the stated intent is only a floor at SlideEntrySpeed; and GetMaxSpeed while sliding returns max(SprintSpeed * multiplier, Velocity.Size2D()), a cap that reads its own current speed.
- The -BreakerMoveTrace script never lands with BoostedSpeedCeiling ABOVE the resting cap, so O264's magnitude at a high landing is unmeasured. A dash-then-jump leg would close it.
- Ranged stutter remains reported, cause unverified. StopChase already avoids repeated STEER velocity resets; inspect actual mode/velocity transitions before changing navigation ownership. Walk animation remains missing.
- HARNESS FINDING, unfixed: -BreakerCaptureMenu= fires its frames on Lvl_FrontEnd, BEFORE -BreakerAutoPlay finishes travelling ("[BreakerAutoPlay] Skipping the title menu; travelling to Lvl_Anchor" appears, the Anchor world never loads in that run). The plate-capture path waits on IsArrivalCoverUp; the plain menu path does not. Consequence: any menu frame whose CONTENT depends on which map the player is standing in has been photographed against the front end. The Core/doctrine boards do not depend on the map and are unaffected; the local map's hub-side travel list does, and is currently unphotographed.
- ENEMY ARRIVAL AND REPOPULATION, owner-ruled, NOT BUILT and the next system block. Verified: nothing in the project repopulates — no timer, no respawn, no repopulate path outside the gym wave driver. Consequences the owner reported are all one gap: wave enemies materialise on top of the player with no arrival fiction; cleared Fernhall stays cleared until the instance is re-made, which is why the Pattern quest's marked kills cannot be finished in one visit; and non-rift areas have no ambient life. Wants authored spawn points (doors, building mouths, small rifts), brief spawn immunity so emerging enemies cannot be deleted as they appear, and an occasional ambient trickle outside rifts.
- MARKED ENEMIES BEFORE THE QUEST: NOT a crediting bug. UBreakerQuestLibrary::NotifyEnemyKilled already refuses progress unless the quest is Active, and the Pattern objective is elite-gated. The enemies the owner reads as "the quest ones" are the ordinary elite population the quest later refers to. Fix is presentation or authored quest-specific spawns, and it belongs with the arrival block above, not as a separate defect.
- CASTER CAST TIME, owner-ruled: damage INTERRUPTS a cast; Mana is spent on the KEYPRESS. Not yet built. Open: whether an interrupted cast refunds (the keypress ruling implies not — confirm), and whether movement and defensive verbs (Slipcut, Hard Stop) take a cast time at all or stay instant. A new Cast Speed multiplier lane needs a canon row in power-and-scaling.md plus a conformance test before merge. A cast bar is a HUD window (O179).
- GEAR NAMES FROM AFFIX POOLS, owner-ruled: PoE grammar, prefix + base + suffix. Not built. The naming rule is pure and world-free (highest-tier relevant prefix and suffix off the item's own affixes); the inventory de-bloat that follows it shows the name and moves affixes to hover, and HOVER IS THE ONE THING THE HARNESS CANNOT VERIFY.
- RIFT INTERIORS AS DILAPIDATED VARIANTS of existing areas, owner-ruled direction. Answers the parked "reuse the yard vs author new tilesets" question by reusing authored geometry with a decay pass. Expect a lighting and palette difference first; ruined geometry needs art that does not exist.
- DEATH SCREEN INSIDE A RIFT reads as ugly and unintuitive (owner). BreakerDeathScreen.cpp exists with retry-rift and return-to-anchor verbs. Not touched: it has no capture fixture, and the menu capture path photographs the front end (see the harness finding above), so nobody has seen it in its rift state. The fixture comes first.
- Volatile now has a tested outlined countdown; final Niagara presentation and real client cadence/human timing remain.
- Damage numbers can overlap enemy plates. Hover, focus recovery and comfort require real interaction checks beyond static captures.
- Tank.Leech.OpenWound promises gear Life on Hit but Rend reads LifeOnKill because no LifeOnHit target/lane exists. Its accepted-hit tests prove the substitute, not the authored gear source. Core.Affliction.OpenWound correctly supplies Increased DoT. Finish the real gear lane and its delivery policy rather than rename the promise.
- Ignore Me doubles owned-deployable threat, but allied-player doubling lacks an authoritative party relation. Cooperative sandbox identity/friendly-fire protection does not establish party membership.
- Remaining Afterimage consumers require individual source-owned cleanup checks; parked cases below carry exact failures.
- Overhaul Bench Work revoke cleanup is parked after bounded correction: nested death/removal during main ammo settlement skips the old window/delegate teardown even when capacity refunds correctly. Candidate and callback fixtures remain outside checkout; no production change landed.
- Blackout Afterimage is parked: its unkeyed enemy movement overwrite can erase Fleetfoot/other slows, and its shared incoming-damage key lets one caster remove another's effect. Requires source-owned contributions before extending the numerical tail; ordinary marked-hit Charge permission still ends on time.
- Guest Hold/deployable cast audio and HUD pulses lack an owner acceptance receipt. The narrow RPC candidate is parked: GAS activation success can still represent immediately rejected placement, while active-state filtering rejects valid instant casts. No optimistic success cue was added.
- Desktop input probe launched an isolated profile but exposed no targetable game window to the computer-use tool; mouse-focus acceptance remains unverified.
- Three generation entries remain uncalled; one aggregation lane and one target remain empty/unrouted. Consult current STATE for exact identities.
- Other low-priority cleanup only after verifying use: empty modifier test helper; unused HUD dimensions; stale movement comments; duplicated boss timings; slot-invalid fixtures; inert affix leans; menu string-table migration.

- Refractor can waste a fork seat on a protected coop player before damage rejection. Legal real-shot fixture and candidate eligibility repair remain parked after bounded audit.
- Bare Swift coop smoke profile suffered ambient deaths during ordinary movement funding. Network receipts are not a balanced encounter acceptance.
- Caster cooperative expansion is parked after bounded network acceptance failure: metadata/real grants/native isolation passed, but two actual guest Cleave requests were rejected by authority; first run logged player death25ms before request. Swift unchanged rerun passed after an initial rejection. Whole expansion restored; no Caster combat claim. Logs and candidate are retained outside checkout in coop-caster-verification-stage/parked-runtime.

## Decisions and measurements requiring explicit care

- DropChanceReachesEveryRank measures probability while O249 converts capped excess into rarity value. Keep the enumerated finding until its replacement measurement is explicitly settled; do not change pins for green results.
- Prolific1.51 remains enumerated with its deletion condition; the eight-Unwritten fixture is not a legal loadout.
- At-cap variance4.05 is below8–10. Ability allocation parity is now0.93 after mirrored fixture allocations and Arc critical access; it is not a native sustained-throughput measurement.
- Native starter diagnostic: after separating accepted direct and periodic damage, Cleave coefficient1.5→2.4 brings seconds10–20 rifle ratios to0.935/0.962 at levels1/20. This is stationary close-range weapon-derived Cleave, not general Ability-pool parity. Earned Fracture has independent Ability-pool evidence. Rifles exhaust initial ammunition during20–30s, so late totals cannot establish parity; see Docs/reports/cleave-tempo-2026-09-09.md.
- Core offers429/65=6.6x; closed22-wedge ring is active. Do not reopen retired hub-entry or2.63x questions.
- Potential future ultimate redesign (Unmake as a decisive event) and skill-level stacking cap need explicit design treatment; retain current rules meanwhile.
- Gear ResourceOnKill currently pays immediately through the attribute bank outside Mana::GrantMana conditional metering; its suspension runtime test explicitly expects this payment. Should gear kill income remain immediate or join the capped queue? Keep current behavior until ruled; this is not a confirmed defect.
- Anchor13's name alone does not establish twelve other settlements.

## Latest validated baseline

Current local build/full suite:864 passing,3 expected failures,0 unexpected; native census refreshed for54 quest flags and current ability declarations. Completed historical work lives in git; this queue lists pending work rather than replaying earlier sessions.

## Playtest handoff

Current delivered features and suggested route: Docs/reports/next-playtest-2026-09-09.md. Detailed evidence stays in the linked reports and git; this desk carries remaining work.
