# Decisions Ledger — append-only

## How to use this file

This is the **only** ruling ledger (O28). It is append-only: rulings are never
edited or deleted, only superseded by a later O-number that says so.

- **To check whether something is decided**, search this file for the topic. If
  it is not here, it is not ruled — no matter what any other document implies.
- **Superseded rulings stay put.** O25 supersedes the air-jump line in the
  locked-decisions paragraph; O27 supersedes the reading of O18 as a global
  invariant; O28 supersedes the master sheet's standing. The original text
  remains so the reasoning is auditable.
- **Pending questions live at the bottom** under "Owner choices currently
  pending". A question there has been presented and not answered.

| Range | Theme |
|---|---|
| O1-O23 | The design sprint: stamina deletion, elements, budgets, TTK targets |
| O24 | World aesthetic (overgrown Earth) |
| O25-O26 | Jump kit; movement priority |
| O27 | The power curve — content scaling, the build band, choices over accumulation |
| O28 | Documentation authority (this section) |


THE canonical decisions log. O-numbers are permanent IDs. Every entry gets
a date, a ruling, and the files it was propagated to. Design docs reference
O-numbers instead of restating rulings; a reversal touches this file first,
then its propagation list. These rulings supersede conflicting text
anywhere else in Docs/ and the master sheet import. Never rewrite an entry;
append a superseding one.

## 2026-08-12 — O1..O17 (Design-Overview §7 list)

Propagated 2026-08-12 (same-day directive pass) to: Master-Sheet-Import.txt,
Layer-Ownership.md, Character-Progression-Architecture.md,
Item-Foundation.md, Core-Constellations.md, Game-Modes.md,
Encounter-Design.md, XP-And-Pacing.md, UI-UX-Spec.md, Save-Architecture.md,
Art-And-Modelling-Plan.md, Damage-Pipeline.md (new), and code (stamina
removal, wave-mode instrument, passive block/dodge — commits c86cbe2,
7766649).

| # | Decision |
|---|----------|
| O1 | **Stamina pool removed entirely.** Block/dodge are passive chance layers (ratified). Parry, when built, uses its own short cooldown. |
| O2 | **Measure before authoring.** Wave-mode instrumentation ships before any further value authoring; numbers stay placeholder until it reports. |
| O3 | **More multipliers multiply as an unordered product; a build may hold 2–3 Mores total (hard cap 3).** Trees may author them only on branch keystones and constellation Convergence/Keystone nodes. |
| O4 | **300–400 hours to a finished build, BUT builds must feel viable and playable by mid-campaign (~level 25).** The long chase is optimization, not viability. Breadth of options and creative expression is an explicit product goal — err toward more viable builds, not higher ceilings. |
| O5 | **Per-element resistances, applied after armour and before shields.** THE ELEMENTS ARE: **RIFT, TIME, and VOID** — not fire/ice/lightning. All Ignite/Chill/Shock naming in the affix tables and the Elements constellation is placeholder and will be re-flavored onto Rift/Time/Void when the resistance model is implemented. |
| O6 | **Hybrid item level ratified.** ZoneLevel on the rift/zone Data Asset + tier bonus, ±1 variance; enemy-level fallback keeps the gym working. |
| O7 | **XP doc's 15-source Core Point list is canon** (with rift-archetype first-clears). |
| O8 | **The endgame farm content type is named "Frontier."** |
| O9 | **Enemy taxonomy: three fields** — Archetype (behavior), Rank (Standard/Veteran/Champion/Boss), Modifiers (0–3). Rename approved. |
| O10 | **Tick interval is part of the DoT snapshot; discrete tick intervals** so stacking has visibly diminishing steps. |
| O11 | **Aberrant: up to 3 equipped, global limit.** Each Aberrant carries 1–2 unique modifier affixes that help define builds; the owner will name and design the specific modifiers later. |
| O12 | **Scalar crafting currencies** (3–4, tiered), not item-derived materials. |
| O13 | **Rocket: strong self-damage reduction, full self-knockback control, never immunity.** Rocket-jumping is tolerated, never required. |
| O14 | **Player is "a person, lightly" — body, face, minimal voice.** TWO player models are planned: Human and Effigy (Effigy later; both are real, not cosmetic-only afterthoughts). Character art must plan for both. |
| O15 | **Branch nodes freely mixed with investment gates.** No mutually exclusive tiers. |
| O16 | **No hardcore/permadeath.** |
| O17 | **Account-wide stash: characters are builds, gear is an account asset.** Deliberate product position. |

## 2026-08-12 — O18..O23 (post-propagation rulings)

| # | Decision |
|---|----------|
| O18 | **TTK/TTD seed targets** (the designer inputs wave mode measures divergence from): trash mobs a little under 1 second, scaling exponentially with enemy difficulty; rare/elite enemies ~3 seconds; boss encounters 20–45 seconds unless a special enemy. **TTD: 4–5 seconds with no resources/sustain; substantially higher with resources and sustain invested.** Stat chassis solves backwards from these. |
| O19 | **Element collisions ruled.** Void: Void Whisperer IS the Void-element specialist — coupling intentional; Multispell's separation becomes "rotates all three" vs "masters one"; C2's mono-element restriction clause is retired as redundant. Rift: Rift-element damage gets a distinct hotter/whiter cyan; teal reservation intact; rule: **saturated teal is a property of objects, not of damage.** Time is renamed — **the elements are Rift / Entropy / Void.** Sweep all references. |
| O20 | **K10 SLIPSTREAM's refund clause and the stamina-built Bulwark nodes are REDESIGN items, not recost items** — separate bucket; O1 removed their trigger, new numbers cannot fix them. |
| O21 | **The three [O3-VIOLATION] nodes are promoted, not cut**: Fixate, Necrosis, and Reflex move to their constellation's Convergence or Keystone tier. Execute now. |
| O22 | **Replication position is owner-authored, due before 0c closes** (this week). If combat is server-authoritative, the proc-coefficient law and the More ceiling assertion live server-side; Damage-Pipeline.md carries a pointer note until the page lands. |
| O23 | **XP §5.1's Veteran 3.0x XP multiplier is flagged over-rewarding** (~a third) against the canonical 2.0x-health chassis. Frozen under O2; on wave mode's report list so it is not carried forward silently. |

## 2026-08-12 — O24 (world aesthetic)

| # | Decision |
|---|----------|
| O24 | **World aesthetic: overgrown Earth.** Nature has reclaimed the ground — vegetation over ruins — with slight sci-fi styling and futuristic tech scattered through it (functional, weathered, out of place). This is the environmental read for Anchors, rift approaches, and erased Earths alike; art direction pillars in Art-And-Modelling-Plan.md compose with it. |

Propagated 2026-08-12 to: Core-Constellations.md, Class-Kits.md,
Art-And-Modelling-Plan.md, UI-UX-Spec.md, XP-And-Pacing.md,
Master-Sheet-Import.txt, Game-Modes.md, Encounter-Design.md,
Damage-Pipeline.md, and the wave-mode report (target seed lines).

## 2026-08-13 — O25, O26 (jump kit, movement scope)

| # | Decision |
|---|----------|
| O25 | **Two jumps are base kit for everyone. Swift innately unlocks a third jump later** — innate to the class, not a tree purchase. This SUPERSEDES the earlier locked line that air jump is one of only two tree-granted verbs: `JumpMaxCount = 2` in `ABreakerCharacter` is correct and stays, and the double jump is no longer a code/design contradiction. Swift's third jump is unimplemented; it is a class-innate unlock, so it belongs with the Swift kit and not in the Core tree. Parry remains tree-granted. |
| O26 | **Movement drops in priority.** It is a big part of the game and a system worth messing with, but it is not the centre of the design and does not get further dedicated passes for now. The weight pass stands with gravity eased back; further movement work is opportunistic, not scheduled. |

Implementation notes tied to these rulings:
- O25: `JumpMaxCount = 2` retained; CONTEXT.md's locked-decisions line corrected.
  Swift's third jump is NOT built.
- O26: gravity eased 1.60 -> 1.45 the same day, per the owner's "feels too
  heavy" playtest; the rest of the weight pass is unchanged.

## 2026-08-13 — O27 (the power curve)

| # | Decision |
|---|----------|
| O27 | **Monsters are CONTENT-scaled to area level, never player-scaled.** Area level drives monster health and damage, and drives the item level of what they drop, so rising item level corresponds to gameplay instead of being cosmetic. The model is PoE's: a legible spreadsheet underneath, geometric monster scaling, and player power that outruns it when the player builds well. **Trash exists to be trivialized** — an optimized character roughly 40 hours past level 50 should delete trash on contact, and difficulty lives entirely in elites, bosses, and monsters carrying modifiers. Hitting 50 must be satisfying with decent power; optimized 50 must feel great. **Choices beat accumulation** — per-point accumulation is cut back and the power moves into node choices — and every avenue (affixes, nodes, weapons) needs significantly more options than the slice currently has. |

Implementation notes tied to this ruling:
- O27 supersedes the reading of O18 as a global invariant. TTK targets are a
  statement about an ON-LEVEL character with a BASELINE build in ON-LEVEL
  content; they are the ratio of two authored curves at one point, not a
  constant to be held true at every point of progression.
- The full architecture, the curve identity, and the tuning dials are in
  `Docs/Design/Power-Curve.md`.
- Three structural gaps this ruling exposes, all confirmed in code:
  `EnemyLevel` existed but drove only loot item level, never monster stats;
  monster health was the literal constant 220 at every level; and `Weapons/`
  contained no reference to `ItemLevel` at all, so weapon base damage was an
  archetype constant and item level touched only affix tier values.

## 2026-08-13 — O28 (documentation authority)

| # | Decision |
|---|----------|
| O28 | **This file is the only ledger, and the authority chain is three links, not four.** `Master-Sheet-Import.txt` is a raw import and is **superseded** — it is historical source material, not law; where it disagrees with this file, this file wins and the master sheet is not to be cited as authority again. **`CONTEXT.md`'s next-actions list is the operative plan.** `Roadmap.md`'s five milestones and `Design-Overview.md` §6's Tier 0-6 build order are both retired to historical: they describe a plan the project no longer follows, which is playtest -> report -> fix. Design-Overview §7 remains the *question* list and must be reconciled against this ledger rather than answering questions on its own. Every design document carries a `Last reconciled against:` marker so drift is visible instead of silent. |

Implementation notes tied to this ruling:
- O28: authority chain is now `Decisions.md` -> `Design-Overview.md` (map, not law)
  -> the per-domain docs. `Master-Sheet-Import.txt` retains its content for
  history and loses its standing.

## Owner choices currently pending (presented, not ruled)

- **TTK re-anchor** — RULED by O27. It is no longer a single retune: trash
  health becomes a curve in area level, and the target is stated for a
  baseline build in on-level content. Do not pick a new constant.
- **Movement's multiplicative gear x tree composition** — the last instance
  of the bug class fixed everywhere else. Conforming to the one-additive-
  bucket rule changes movement FEEL (+20/+20 becomes x1.40, not x1.44),
  which is why it is a ruling and not a fix. Related: the composed MoveSpeed
  attribute has no gameplay consumer at all.
- **Subclass commitment** — the branch strip browses; committing needs a
  branch field, a permanence-or-Forge rule, save versioning, and a decision
  on whether unselected branches become unpurchasable. That last part
  collides with O15 (branches freely mixed, no mutually exclusive tiers).
- **Swift's third jump (O25)** — unimplemented; needs a kit design for when
  it unlocks and whether it costs anything.


- **"Frontier pack" name collision** — Game-Modes §4.3's top pack tier now
  collides with the Frontier content type (GAP [O8]). HELD per owner.
- **Sealed / Bare modifier scope** vs Support and Leech generation
  (Game-Modes Class C audit).
- **REDESIGN bucket [O20]** — K10 SLIPSTREAM refund clause; stamina-built
  Bulwark nodes (B4 cooldown shape, B7 Riposte, B9 Guard Doctrine).
  Redesign, not recost; owner-led.
- **Dodge resource refund** — survives as base kit, tree rewrite, or dies
  (Item-Foundation GAP [O1]).
- **Rift-archetype first-clear point budget** — 8 (Game-Modes) vs 2 (XP §7)
  within the 15-point canon list.
- **XP band-to-Rank collapse** — two bands each under Champion and Boss.
- **Networking/replication position** — owner writing it this week, due
  before 0c closes (O22).

Implementation notes tied to these rulings:
- O1: `Stamina`/`MaxStamina` removed from the attribute set and combat
  component in the same commit as this file.
- O2: wave mode + time-to-kill instrumentation in the gym (same commit).
- O5: `EBreakerDamageFamily::Elemental` stays as the pipeline family; the
  Rift/Time/Void split arrives with the resistance model.

## 2026-08-14 — O29, O30, O31 (endgame gear depth, the archetype axes, content shape)

| # | Decision |
|---|----------|
| O29 | **THE ENDGAME POWER SOURCE IS GEAR DEPTH.** Item level runs to **120**, past the character cap of 50 and past the area-level ceiling, which is what makes "all endgame character power comes from gear" actually function rather than merely be stated. Affix modifiers stay **within tier ranges** — the tier ladder simply widens from T8..T-1 to **T12..T-1** — and values are **tuned up significantly across the whole ladder, with a materially bigger jump between the high tiers.** The T-band is therefore no longer linear: the curve is back-loaded so a top-tier roll is an event rather than one more step. This is the answer to the 74x gap recorded at the end of `Power-Curve.md`. A paragon-style post-cap tree is REJECTED — it collides with the locked no-post-cap-character-power rule and is pure accumulation against O27. |
| O30 | **The Core tree is open to redesign, organised around the axes a build is actually built on.** Owner's taxonomy, to be scouted and costed rather than implemented blind — **GUNS**: ailment/element, poison, bleed, flat damage, crit, fire rate, movement. **ABILITIES**: stacking, multispell, cooldown reduction, AoE, poison, bleed, flat damage, crit. **MINIONS**: drones, turrets, deployables generally. Weapon archetypes should fit some axes more naturally than others. Stated design intent: three knobs (two trees plus gear) becomes **several** knobs, and subclasses exist to solidify and empower identities the axes already create rather than to introduce separate ones. |
| O31 | **Content shape: a cross between Destiny and Path of Exile.** Raids are **puzzles rewarded for team play** — many distinct encounters that force players into different situations. Builds may excel in some situations and be weak in others, but **every build must be able to make an impact and feel player power**. No encounter may have a build that cannot participate. |

Implementation notes tied to these rulings:
- O29: item level 120 moves `MaxSupportedItemLevel`, `BestTierForItemLevel`,
  `ValueForTier`, `TierCapForRarity` and `GetDropItemLevel`'s clamp together.
  `RiorsEdge.Combat.PowerCurve.EndgameClamp` asserts the endgame gap is still
  OPEN and is EXPECTED TO FAIL once this lands — that failure is the signal to
  delete the test and rewrite `Power-Curve.md` §1 to describe what now carries
  endgame power.
- O29: affix tier values are save-relevant only through `Tier` and `Value` on a
  rolled affix, both of which are already stored per-item, so widening the
  ladder does not invalidate existing saves — but every item rolled before it
  keeps the values it rolled, and will read as weak. That is correct and should
  not be migrated.
- O30: `EBreakerBuildCondition` is movement-only today, which is why no node can
  key off combat or status state. Several axes in the taxonomy (ailment, crit,
  stacking) need it widened before they can be authored honestly.
- O30: minions/deployables do not exist in any form. The Gunsmith kit designs
  them; nothing is built.

## 2026-08-14 — O32 (legendary cadence)

| # | Decision |
|---|----------|
| O32 | **Legendary drop rate stays where it is; the legendary POOL grows instead.** The measured ~57 hours per legendary at area level 50 is mostly an artefact of there being three legendaries covering three of eight slots — the same arithmetic with a full set gives ~21 hours, which is a reasonable cadence for a named item against O4's 300–400 hour horizon. So `LegendaryChanceWithinAnomalous` is NOT raised. Authoring more legendaries is the fix, and the effective wait falls on its own as the pool fills. For playtesting a legendary before the pool is full, use the existing dev grant rather than bending the drop rate. |

Implementation notes tied to this ruling:
- O32: legendary and Anomalous are different axes and the docs must stop
  conflating them. **Anomalous is a RARITY** (the fifth tier; gates affix count
  and tier ceiling, and carries one ROLLED rule rewrite from a generic pool of
  four). **Legendary is a separate field** (`FBreakerItemInstance::LegendaryId`)
  naming a specific authored item with a fixed slot, guaranteed affixes and a
  HAND-AUTHORED rule. Every legendary rolls at Anomalous rarity; most Anomalous
  drops are not legendaries.
- O32: the three authored legendaries occupy Boots, Primary and Waist.
  Helmet, Body Armour, Gloves, Necklace and Secondary have none, which is what
  makes the current wait 57 hours rather than 21.

