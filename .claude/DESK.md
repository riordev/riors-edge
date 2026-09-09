# Desk â€” next playtest

## Working scope

Continue validated blocks until the owner returns. Prioritize playable changes for the next large playtest. Investigate an individual bug for at most five minutes, then record its evidence and park the affected change if unresolved. Independent agents stage on disjoint files; one engine build/suite runs at a time. Every landed block follows BUILD â†’ SUITE â†’ Scripts/status.py â†’ COMMIT â†’ PUSH. Larger changes receive independent review. Never count placeholders or pure-maths tests as complete gameplay.

## Immediate queue

1. Cooperative sandbox landed with real host/guest fire and replicated health/death evidence. Owner slot metadata and guest health/input respawn transport now verified; remaining acceptance: predicted ability effects/costs, drop contention, rejoin, human presentation/movement feel and eventual campaign support.
2. Red Basin/Station Zero prototypes landed; next content acceptance is varied mission beats and real return Rifts. Refine exposed boundaries and repeated cover. Anchor surroundings/work-area dressing landed but remains blockout art.
3. Volatile countdown and ability-menu wrapping/empty-label fixes landed with rendered checks. Continue interactive focus/death flow and other menu screens.
4. Finish remaining Afterimage consumers and real-clock Rot feedback checks. Cache prompt now shares opening eligibility and stays visible at the console; final prop art remains.
5. Kit expansion under O252â€“O260: current34 registered entries are11 actives and10 passives short of55. Carom, Coup, Backstep, Pyre, Riftlance, Recall, Bulwark and Overwatch are named candidates without sufficient authored mechanics in the current docs. Rover/Barrel need new mechanics; Wildcard has O257 but needs Forge/foreign-grant/resource infrastructure. Do not fill the count with clones or call existing innate nodes slot passives.
6. World Core Point coverage is already derived from mission Unlock.CorePoint references; verify remaining uncovered sources against actual flag delivery before changing content.

## Seven-step acceptance checklist

| Step | Present evidence | Remaining acceptance work |
|---|---|---|
| 1 Entropy | Finite funding, original critical samples, paid duration and persistence tests landed | Actual encounter clarity, audible feedback and sustained ability/converted-weapon play |
| 2 Elements/reactions | Entropy, Void, Rift and bounded reactions have native delivery tests | Regression check current builds, readable combined feedback and complete encounter loop |
| 3 Progression/combat | 187 Core nodes; 370 total nodes; no measured silent nodes; Caster doctrines12/24; allocation parity0.93 | Audit actual consumers/acquisition, remaining kit programme, solo-node gaps and gameplay balance |
| 4 Loot | New special budgets, overflow conversion, Unwritten rename and skill-level affixes landed | Authored perks/legendaries, real legal-loadout comparison and UI clarity; legacy-item archive/migration protection now tested |
| 5 Interface | Core wheel and paid purchase/assignment paths exist | Occlusion, clipping across menus/resolutions, NPC/equipment/death focus flows and dev sandbox |
| 6 Fernhall/Rifts | Five outdoor pockets/17 enemies, Watchkeeper contract, campaign flags and local map exist | Further authored spaces, varied objectives, distinct Rift interiors, reward/return and natural leveling; guarded physical caches now validated |
| 7 Visual/audio | Placeholder presentation and combat feedback exist | Anchor/Fernhall identity, weapons/arms/weakpoints, coherent sound/VFX and signature encounters; inspect captures, do not claim final art |

## Destinations and missions

- Anchor 13: main hub; Fernhall Approach: overgrown industrial with a cityscape.
- Red Basin: scorched farmland/crater. Station Zero: overrun research hub. Port Meridian: destroyed airport/terminal. Broken Coast: ocean/flat landscape. Shatterpoint: city sprawl.
- Improve Anchor/Fernhall routes and landmarks; author playable placeholder destinations/tilesets and varied objectives using available assets. Ordinary regional difficulty stays fixed; return Rifts provide harder encounters.
- Existing local-map site IDs contain coordinates: replace with stable identity and preserve discovery before relocating existing sites.
- Rift interior currently reuses the yard. BuildCoverField generates gym-specific sections; FernhallFieldParams validates authored layouts and is not a compatible generation recipe. Use a genuinely band-aware generator or independently composed interiors; retain RiftGeneratedField as a measured probe.
- O262: command post consumes a key carrying area level/modifiers and determines layout. Consumable entry and death budget land together. A key, device and tileset are not complete until enter â†’ fight â†’ reward â†’ return works.
- Add mission variety only with a real objective consumer. Current ordinary quest progress supports kills and the specific residue collection path; generic collection needs implementation.

## Multiplayer

Opt-in fixed-Fernhall cooperative sandbox exists: distinct transient Swift profiles, client geometry, authored late-guest start, no persistent writes, scoped friendly-fire prevention. Real two-process guest fire/server damage/death/client health transport passed; see Docs/reports/coop-combat-2026-09-09.md and Scripts/coop-combat-verify.ps1. Owner slot IDs/real GAS specs and actual guest death/server-timer respawn/input restoration are verified. Full campaign/progression, predicted ability effects/costs, loot contention, rejoin and human presentation/feel remain unvalidated. Trading, account services and MMO infrastructure stay outside this playtest slice.

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
- At-cap variance4.05 is below8â€“10. Ability allocation parity is now0.93 after mirrored fixture allocations and Arc critical access; it is not a native sustained-throughput measurement.
- Native starter diagnostic: Cleave tempo change raises seconds10â€“20 ratio from0.535/0.550 to about0.73/0.74 at levels1/20; starter parity remains open. Earned Fracture has independent Ability-pool evidence. Rifles exhaust initial ammunition during20â€“30s, so late totals cannot establish parity. Measure accepted direct versus periodic damage before another coefficient adjustment; see Docs/reports/cleave-tempo-2026-09-09.md.
- Core offers429/65=6.6x; closed22-wedge ring is active. Do not reopen retired hub-entry or2.63x questions.
- Potential future ultimate redesign (Unmake as a decisive event) and skill-level stacking cap need explicit design treatment; retain current rules meanwhile.
- Anchor13's name alone does not establish twelve other settlements.

## Latest validated baseline

Current local build/full suite:854 passing,3 expected failures,0 unexpected; native census remains current for unchanged authoring. Completed historical work lives in git; this queue lists pending work rather than replaying earlier sessions.

## Recent delivered changes

- Full22-wedge Core remains active. Paid Afterimage Hold/Sightline and Survivor Core-point delivery have runtime coverage.
- Warden revival, Provoke/Standing Order, Swift starter migration, numeric ability authority and existing-item migration protection landed.
- Fernhall guarded physical cache; Red Basin/Station Zero fixed-region prototype districts, local maps and real return gates landed.
- Anchor gatehouses/service-yard dressing, boss/Volatile occlusion cues, ability-menu wrapping, empty-label removal and cache prompt clarity were inspected in rendered captures.
- Cleave cost/tempo improved native sustained output; direct/periodic split is next before coefficient tuning.
- Cooperative sandbox now verifies owner metadata, actual weapon damage, guest lethal/server respawn and client input restoration with isolated saves. Optional early screenshot capture is parked; no rendered death acceptance claim.

Detailed historical receipts: Docs/reports/playtest-pass-ledger-2026-09-09.md. Individual capture/measurement reports remain under Docs/reports.
