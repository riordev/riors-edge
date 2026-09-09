# Desk — next playtest

## Working scope

Continue validated blocks until the owner returns. Prioritize playable changes for the next large playtest. Investigate an individual bug for at most five minutes, then record its evidence and park the affected change if unresolved. Independent agents stage on disjoint files; one engine build/suite runs at a time. Every landed block follows BUILD → SUITE → Scripts/status.py → COMMIT → PUSH. Larger changes receive independent review. Never count placeholders or pure-maths tests as complete gameplay.

## Immediate queue

1. Fernhall cache: a console-shaped interactable, unlocked by clearing its nearby pocket, pays loot once. Use yard-relative placement without moving existing sites.
2. O258: retire Skim, make Slipcut the Swift starter, and migrate/remove SkimDiscipline with its consumer and existing loadouts.
3. O247: Provoke keeps its four-second forced target and grants threat scaled by the existing threat lane.
4. O250: a broken Warden front stays broken through revival.
5. O251: occlusion hides the entire enemy plate; inspect rendered frames.
6. O248: grandfather existing over-budget special items; constrain new rolls without removing owned affixes.
7. Kit expansion under O252–O260: verify the registered roster, author reachable abilities/passives with real consumers, and reconcile counts before claiming the 55-entry target. Carom, Coup, Backstep, Pyre, Riftlance, Recall, Bulwark and Overwatch are named candidates. Rover, Barrel and Wildcard require additional mechanics. Check proposed names against Core/doctrine IDs.
8. O246: remove duplicated compiled ability defaults only with a replacement schema/completeness test for Data/abilities.json.
9. Derive world Core Point trigger coverage from actual mission references instead of bTriggerBuilt declarations.

## Seven-step acceptance checklist

| Step | Present evidence | Remaining acceptance work |
|---|---|---|
| 1 Entropy | Finite funding, original critical samples, paid duration and persistence tests landed | Actual encounter clarity, audible feedback and sustained ability/converted-weapon play |
| 2 Elements/reactions | Entropy, Void, Rift and bounded reactions have native delivery tests | Regression check current builds, readable combined feedback and complete encounter loop |
| 3 Progression/combat | 187 Core nodes; 371 total nodes; no measured silent nodes; Caster doctrines12/24; allocation parity0.93 | Audit actual consumers/acquisition, remaining kit programme, solo-node gaps and gameplay balance |
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

Owner authorizes adding multiplayer when feasible. Audit current listen-server join, ownership, replication, interactions, combat and save isolation first. Implement bounded cooperative slices with two-client evidence; record unsupported paths. Do not advertise a working multiplayer mode from RPC declarations alone. Trading, account services and MMO infrastructure are outside the immediate playtest priority.

## Known issues to time-box

- Warden engaged movement bypasses arrival spacing and can walk through the player.
- Ranged STEER transitions reset movement and appear to stutter; walk animation is missing.
- Volatile corpse has no visible fuse after O203 retired disc/light. Add readable placeholder feedback before final Niagara art.
- Damage numbers can overlap enemy plates. Hover, focus recovery and comfort require real interaction checks beyond static captures.
- Remaining Afterimage window/ability consumers, Open Wound's authored lane substitute, rocket continuation and ally-threat semantics need source review; old desk claims are not evidence of current absence.
- Three generation entries remain uncalled; one aggregation lane and one target remain empty/unrouted. Consult current STATE for exact identities.
- Other low-priority cleanup only after verifying use: empty modifier test helper; unused HUD dimensions; stale movement comments; duplicated boss timings; slot-invalid fixtures; inert affix leans; menu string-table migration.

## Decisions and measurements requiring explicit care

- DropChanceReachesEveryRank measures probability while O249 converts capped excess into rarity value. Keep the enumerated finding until its replacement measurement is explicitly settled; do not change pins for green results.
- Prolific1.51 remains enumerated with its deletion condition; the eight-Unwritten fixture is not a legal loadout.
- At-cap variance4.05 is below8–10. Ability allocation parity is now0.93 after mirrored fixture allocations and Arc critical access; it is not a native sustained-throughput measurement.
- Core offers429/65=6.6x; closed22-wedge ring is active. Do not reopen retired hub-entry or2.63x questions.
- Potential future ultimate redesign (Unmake as a decisive event) and skill-level stacking cap need explicit design treatment; retain current rules meanwhile.
- Anchor13's name alone does not establish twelve other settlements.

## Latest validated baseline

Pulled main1e369b4. Remote report:837 passing,3 expected failures,0 unexpected. Local build and fresh census/suite validated:837 passing,3 expected failures,0 unexpected. Completed historical work lives in git; this queue lists pending work rather than replaying earlier sessions.

## Landed this pass

- Desk reduced from640 to104 lines: removed stale completed work and contradictory questions, retained active gaps and seven-step acceptance criteria. Rebuilt pulled code and regenerated its source witness;837 passing,3 expected failures,0 unexpected.
