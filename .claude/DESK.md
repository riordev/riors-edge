# Desk — next playtest

## Working scope

Continue validated blocks until the owner returns. Prioritize playable changes for the next large playtest. Investigate an individual bug for at most five minutes, then record its evidence and park the affected change if unresolved. Independent agents stage on disjoint files; one engine build/suite runs at a time. Every landed block follows BUILD → SUITE → Scripts/status.py → COMMIT → PUSH. Larger changes receive independent review. Never count placeholders or pure-maths tests as complete gameplay.

## Immediate queue

1. Cooperative sandbox landed with real host/guest fire and replicated health/death evidence. Remaining multiplayer acceptance: abilities, drop contention, respawn/rejoin, human movement feel and eventual campaign support.
2. Red Basin/Station Zero prototypes landed; next content acceptance is varied mission beats and real return Rifts. Refine exposed boundaries, repeated cover and crowded distant supply prompts.
3. Volatile countdown and ability-menu wrapping/empty-label fixes landed with rendered checks. Continue interactive focus/death flow and other menu screens.
4. Finish remaining Afterimage consumers and real-clock Rot feedback checks. Cache prompt now shares opening eligibility and stays visible at the console; final prop art remains.
5. Kit expansion under O252–O260: current34 registered entries are11 actives and10 passives short of55. Carom, Coup, Backstep, Pyre, Riftlance, Recall, Bulwark and Overwatch are named candidates without sufficient authored mechanics in the current docs. Rover/Barrel need new mechanics; Wildcard has O257 but needs Forge/foreign-grant/resource infrastructure. Do not fill the count with clones or call existing innate nodes slot passives.
6. World Core Point coverage is already derived from mission Unlock.CorePoint references; verify remaining uncovered sources against actual flag delivery before changing content.

## Seven-step acceptance checklist

| Step | Present evidence | Remaining acceptance work |
|---|---|---|
| 1 Entropy | Finite funding, original critical samples, paid duration and persistence tests landed | Actual encounter clarity, audible feedback and sustained ability/converted-weapon play |
| 2 Elements/reactions | Entropy, Void, Rift and bounded reactions have native delivery tests | Regression check current builds, readable combined feedback and complete encounter loop |
| 3 Progression/combat | 187 Core nodes; 370 total nodes; no measured silent nodes; Caster doctrines12/24; allocation parity0.93 | Audit actual consumers/acquisition, remaining kit programme, solo-node gaps and gameplay balance |
| 4 Loot | New special budgets, overflow conversion, Unwritten rename and skill-level affixes landed | Legacy-item protection, authored perks/legendaries, real legal-loadout comparison and UI clarity |
| 5 Interface | Core wheel and paid purchase/assignment paths exist | Occlusion, clipping across menus/resolutions, NPC/equipment/death focus flows and dev sandbox |
| 6 Fernhall/Rifts | Five outdoor pockets/17 enemies, Watchkeeper contract, campaign flags and local map exist | Cache, authored spaces, varied mission objectives, distinct Rift interiors, reward/return and natural leveling |
| 7 Visual/audio | Placeholder presentation and combat feedback exist | Anchor/Fernhall identity, weapons/arms/weakpoints, coherent sound/VFX and signature encounters; inspect captures, do not claim final art |

## Destinations and missions

- Anchor 13: main hub; Fernhall Approach: overgrown industrial with a cityscape.
- Red Basin: scorched farmland/crater. Station Zero: overrun research hub. Port Meridian: destroyed airport/terminal. Broken Coast: ocean/flat landscape. Shatterpoint: city sprawl.
- Improve Anchor/Fernhall routes and landmarks; author playable placeholder destinations/tilesets and varied objectives using available assets. Ordinary regional difficulty stays fixed; return Rifts provide harder encounters.
- Existing local-map site IDs contain coordinates: replace with stable identity and preserve discovery before relocating existing sites.
- Rift interior currently reuses the yard. BuildCoverField generates gym-specific sections; FernhallFieldParams validates authored layouts and is not a compatible generation recipe. Use a genuinely band-aware generator or independently composed interiors; retain RiftGeneratedField as a measured probe.
- O262: command post consumes a key carrying area level/modifiers and determines layout. Consumable entry and death budget land together. A key, device and tileset are not complete until enter → fight → reward → return works.
- Add mission variety only with a real objective consumer. Current ordinary quest progress supports kills and the specific residue collection path; generic collection needs implementation.

## Multiplayer

Opt-in fixed-Fernhall cooperative sandbox exists: distinct transient Swift profiles, client geometry, authored late-guest start, no persistent writes, scoped friendly-fire prevention. Real two-process guest fire/server damage/death/client health transport passed; see Docs/reports/coop-combat-2026-09-09.md and Scripts/coop-combat-verify.ps1. Full multiplayer campaign/progression, ability replication, loot contention, respawn/rejoin and feel remain unvalidated. Trading, account services and MMO infrastructure stay outside this playtest slice.

## Known issues to time-box

- Warden engaged movement bypasses arrival spacing and can walk through the player.
- Ranged STEER transitions reset movement and appear to stutter; walk animation is missing.
- Volatile now has a tested outlined countdown; final Niagara presentation and real client cadence/human timing remain.
- Damage numbers can overlap enemy plates. Hover, focus recovery and comfort require real interaction checks beyond static captures.
- Remaining Afterimage window/ability consumers, Open Wound's authored lane substitute, rocket continuation and ally-threat semantics need source review; old desk claims are not evidence of current absence.
- Blackout Afterimage is parked: its unkeyed enemy movement overwrite can erase Fleetfoot/other slows, and its shared incoming-damage key lets one caster remove another's effect. Requires source-owned contributions before extending the numerical tail; ordinary marked-hit Charge permission still ends on time.
- Three generation entries remain uncalled; one aggregation lane and one target remain empty/unrouted. Consult current STATE for exact identities.
- Other low-priority cleanup only after verifying use: empty modifier test helper; unused HUD dimensions; stale movement comments; duplicated boss timings; slot-invalid fixtures; inert affix leans; menu string-table migration.

## Decisions and measurements requiring explicit care

- DropChanceReachesEveryRank measures probability while O249 converts capped excess into rarity value. Keep the enumerated finding until its replacement measurement is explicitly settled; do not change pins for green results.
- Prolific1.51 remains enumerated with its deletion condition; the eight-Unwritten fixture is not a legal loadout.
- At-cap variance4.05 is below8–10. Ability allocation parity is now0.93 after mirrored fixture allocations and Arc critical access; it is not a native sustained-throughput measurement.
- Native starter diagnostic: Cleave/rifle DPS0.535 at level1 and0.550 at level20 during seconds10–20. Earned Fracture at level20 sustains1267 vs rifle1051 in that interval. Rifles run out of initial ammunition during20–30s; late totals cannot establish parity. Diagnose Cleave's rate/resource lane before tuning; see Docs/reports/caster-sustain-2026-09-09.md.
- Core offers429/65=6.6x; closed22-wedge ring is active. Do not reopen retired hub-entry or2.63x questions.
- Potential future ultimate redesign (Unmake as a decisive event) and skill-level stacking cap need explicit design treatment; retain current rules meanwhile.
- Anchor13's name alone does not establish twelve other settlements.

## Latest validated baseline

Current local build/census/full suite:852 passing,3 expected failures,0 unexpected. Completed historical work lives in git; this queue lists pending work rather than replaying earlier sessions.

## Landed this pass

- Desk reduced from640 to104 lines: removed stale completed work and contradictory questions, retained active gaps and seven-step acceptance criteria. Rebuilt pulled code and regenerated its source witness;837 passing,3 expected failures,0 unexpected.

- Warden front revival: build and full suite838 passing/3 expected/0 unexpected; actual Wakeful revival and follow-up frontal damage tested. Broken front stays depleted; fresh Wardens still arm.

- Fernhall cache: real pocket clearance, authoritative range/LOS, one physical ordinary loot roll, one-shot/reentrant claims and pickup validated. No empty prompt or map marker. Build/full suite839 passing,3 expected,0 unexpected; rendered placement pending.

- Provoke: real forced target and editable threat grant, range/death/safe-zone eligibility, stronger later competitor and paid Standing Order route pass. Test competitor moved outside native melee reach while retaining real Grit proximity. Build/census/full suite840 passing,3 expected,0 unexpected. Two new numeric keys counted explicitly; no balance pin changed.

- Swift starter: Slipcut replaces Skim; v10 preserves equipped order, refunds paid Slipcut/SkimDiscipline once and leaves frozen historical migrations intact. Paid starter, HardStop and save coverage pass. Four stale inventory/version assertions updated;841 passing,3 expected,0 unexpected. Registered abilities35→34 and total nodes371→370 reflect the explicit retirement; kit expansion remains open.

- Enemy plates: bosses now obey the same static-cover rule. Native occlusion and full suite pass842/3/0; rendered1920×1080 open/blocked frames confirm full plate visibility/removal. Cache placement also inspected. Capture boot-map race avoided with explicit map launch; weak core ticker fixes paused setup. See Docs/reports/plate-cache-capture-2026-09-09.md; rough art/poses and live interaction remain open.

- Legacy special items: strengthened existing native Forge test checks exact stored line order/identity/tier/value/category after serialization, repeated current migration, equipment restore, then paid Attune. Nine-line historical items survive. Build/full suite842/3/0; production preservation already existed, so no item rewrite or tuning change.

- Ability numeric authority: removed143 duplicated initializers; Data schema replaces compiled-default freshness. First suite exposed native instances retaining zeros despite patched CDOs; fixed at PostInitProperties and added all-row instance checks, independently reviewed. Rebuilt full suite842/3/0. No status.py binding existed to rename; measurement pins unchanged. Custom serialized Blueprint overrides are outside the registered native roster guarantee.

- Hold Afterimage: paid native Core route and equipped resource-funded Hold verify generation's two-second half-contribution tail. Ordinary hit cap ends on time; cancellation, death, paid respec and removing the inactive granted ability revoke the contribution. Independent review caught the removal gap before landing. Build/full suite843/3/0.

- Cooperative combat sandbox: native isolation/environment tests and full suite845/3/0. Real two-process verification passed: two profiles, client floor/lighting, remote displacement, guest normal fire RPC, nine server weapon hits and client220→148→0 health/death. Disposable UserDirs and unchanged save hashes verified. Compile API/macro issues and live-log sharing fixed within the per-bug time box. This is combat-only; progression/travel and remaining multiplayer interactions are not claimed complete.

- Prototype destinations: real Red Basin/Station Zero packages, six fixed-level districts/36 guards/six supply rewards, stable maps and return gates. Safe arrival and native death/respawn, route sweeps, actual cache kill gates/pickups pass;847/3/0. Both four-frame1080 scenery tours inspected; prototype objective verified in two720 frames. Still blockout art and recovery-only objectives; see Docs/reports/prototype-destinations-2026-09-09.md.

- Survivor Core entitlement: canonical one-point Unlock pays actual supported arrival flag; silent restored saves settle at native startup. Wallet updates before synchronous receipt persistence. Independent review and memory archive/reentry/reload tests pass;848/3/0. Uncovered World Core sources12→11; remaining events need real authored delivery, not guessed bindings.

- Volatile warning: real lethal/corpse tick/detonation/reset checks and full suite849/3/0. All six1080 capture frames inspected: open countdown readable, covered and clipped-edge cues fully absent. Static freezes prove geometry only; native tests prove clock, replication transport remains separate. See Docs/reports/volatile-countdown-2026-09-09.md.

- Ability menu/cache usability: full suite849/3/0. Swift1080 and Caster720 rows fit; vacant slots no longer print NONE. Cache focus shares range/cover eligibility, redundant distant labels are removed and its focused prompt anchors at the console. Six successful frames inspected; see Docs/reports/ability-menu-cache-2026-09-09.md. Mouse interaction and other screens still require play.

- Sightline removal: actual purchased grant and nine-point Afterimage route verify full/half pierce and immediate permanent cleanup when the inactive grant is removed. Fixture baseline now includes native traversal-earned Momentum pierce. Build/full suite850/3/0; independently reviewed.

- Rift retry: actual game-mode entry now rejects exhausted endgame allowance without resetting/spending it; campaign retry stays free. Native direct calls verify zero/negative/positive budgets and unchanged instance identity. Test name is a sibling of existing DeathBudget so Unreal runs both. Build/full suite851/3/0. Consumable keys, device and cross-map run remain unfinished.

- Caster sustain/Rot clock: ten native minute-long scenarios preserve legal affixes, earned grants, resource recovery, projectile delivery and reloads. All Rot zones pay12 boundary ticks. HUD test now advances actual world time and rejects indefinite zero-time aggregation. Build/full suite852/3/0; measurements and limits in Docs/reports/caster-sustain-2026-09-09.md. Starter Cleave balance remains open.
