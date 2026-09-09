# Desk — next playtest

## Working scope

Continue validated blocks until the owner returns. Prioritize playable changes for the next large playtest. Investigate an individual bug for at most five minutes, then record its evidence and park the affected change if unresolved. Independent agents stage on disjoint files; one engine build/suite runs at a time. Every landed block follows BUILD → SUITE → Scripts/status.py → COMMIT → PUSH. Larger changes receive independent review. Never count placeholders or pure-maths tests as complete gameplay.

## Immediate queue

1. Fernhall cache gameplay and rendered placement landed; live prompt/focus and final prop art remain.
2. O258 landed: Slipcut is Swift's starter; v10 migrates Skim slots and refunds retired purchases without duplication.
3. O247 landed: Provoke forces its target for four seconds and grants lasting scaled threat; paid Standing Order cancellation verified.
4. O250 landed: a broken Warden front stays broken through revival.
5. O251 landed: static cover hides the entire enemy plate including bosses; open/blocked frames inspected.
6. O248 verified: native archive, v9→v10/repeated migration, equipped/backpack restore and paid Attune retain historical over-budget affixes. No destructive normalization needed.
7. Kit expansion under O252–O260: verify the registered roster, author reachable abilities/passives with real consumers, and reconcile counts before claiming the 55-entry target. Carom, Coup, Backstep, Pyre, Riftlance, Recall, Bulwark and Overwatch are named candidates. Rover, Barrel and Wildcard require additional mechanics. Check proposed names against Core/doctrine IDs.
8. O246 landed: Data alone authors146 numeric keys; native instance initialization applies them explicitly, schema and instance/CDO agreement verified.
9. World Core Point coverage is already derived from mission Unlock.CorePoint references; verify remaining uncovered sources against actual flag delivery before changing content.

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

Current local build/census/full suite:842 passing,3 expected failures,0 unexpected. Completed historical work lives in git; this queue lists pending work rather than replaying earlier sessions.

## Landed this pass

- Desk reduced from640 to104 lines: removed stale completed work and contradictory questions, retained active gaps and seven-step acceptance criteria. Rebuilt pulled code and regenerated its source witness;837 passing,3 expected failures,0 unexpected.

- Warden front revival: build and full suite838 passing/3 expected/0 unexpected; actual Wakeful revival and follow-up frontal damage tested. Broken front stays depleted; fresh Wardens still arm.

- Fernhall cache: real pocket clearance, authoritative range/LOS, one physical ordinary loot roll, one-shot/reentrant claims and pickup validated. No empty prompt or map marker. Build/full suite839 passing,3 expected,0 unexpected; rendered placement pending.

- Provoke: real forced target and editable threat grant, range/death/safe-zone eligibility, stronger later competitor and paid Standing Order route pass. Test competitor moved outside native melee reach while retaining real Grit proximity. Build/census/full suite840 passing,3 expected,0 unexpected. Two new numeric keys counted explicitly; no balance pin changed.

- Swift starter: Slipcut replaces Skim; v10 preserves equipped order, refunds paid Slipcut/SkimDiscipline once and leaves frozen historical migrations intact. Paid starter, HardStop and save coverage pass. Four stale inventory/version assertions updated;841 passing,3 expected,0 unexpected. Registered abilities35→34 and total nodes371→370 reflect the explicit retirement; kit expansion remains open.

- Enemy plates: bosses now obey the same static-cover rule. Native occlusion and full suite pass842/3/0; rendered1920×1080 open/blocked frames confirm full plate visibility/removal. Cache placement also inspected. Capture boot-map race avoided with explicit map launch; weak core ticker fixes paused setup. See Docs/reports/plate-cache-capture-2026-09-09.md; rough art/poses and live interaction remain open.

- Legacy special items: strengthened existing native Forge test checks exact stored line order/identity/tier/value/category after serialization, repeated current migration, equipment restore, then paid Attune. Nine-line historical items survive. Build/full suite842/3/0; production preservation already existed, so no item rewrite or tuning change.

- Ability numeric authority: removed143 duplicated initializers; Data schema replaces compiled-default freshness. First suite exposed native instances retaining zeros despite patched CDOs; fixed at PostInitProperties and added all-row instance checks, independently reviewed. Rebuilt full suite842/3/0. No status.py binding existed to rename; measurement pins unchanged. Custom serialized Blueprint overrides are outside the registered native roster guarantee.
