# Decisions Ledger — append-only

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

## Owner choices currently pending (presented, not ruled)

- **TTK re-anchor [O18/O2]** — session 5 measured melee trash 1.81s vs a
  <1s target and elite 3.01s vs ~3s (ON target). The correction is one
  ratio: trash health ~220 -> ~120 or an equivalent damage raise. Health is
  the recommended lever; it leaves weapon damage as the gear/tree tuning
  surface. Set `WeakPointToleranceCm = 0` for the measuring run.
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
