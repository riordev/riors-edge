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

## Owner choices currently pending (presented, not ruled)

- **2b-1 Void vs Void Whisperer** — rename the Caster subclass, or make it
  explicitly the Void-element specialist (accepting the coupling).
- **2b-2 Rift element vs reserved teal** — distinct stated shade for Rift
  damage VFX, or narrow the teal reservation to rift phenomena only.
  ([OWNER-CHOICE] block lives in Art-And-Modelling-Plan.md Pillar 3.)
- **2b-3 Time element vs Swift** — declare them unrelated systems, or
  rename the element.
- **"Frontier pack" name collision** — Game-Modes §4.3's top pack tier now
  collides with the Frontier content type (GAP [O8]).
- **Sealed / Bare modifier scope** vs Support and Leech generation
  (Game-Modes Class C audit).
- **SLIPSTREAM's dodge→air-jump refund clause** — not cleanly expressible
  under passive dodge (Core-Constellations K10).
- **Dodge resource refund** — survives as base kit, tree rewrite, or dies
  (Item-Foundation GAP [O1]).
- **Rift-archetype first-clear point budget** — 8 (Game-Modes) vs 2 (XP §7)
  within the 15-point canon list.
- **XP band-to-Rank collapse** — two bands each under Champion and Boss.
- **Networking/replication position** (raised by the directive §3): one
  page ruling on server authority before any Tier 1 code.
- **Target TTK/TTD seed figures** for wave mode (directive §4): the
  instrument reports divergence from a target only the owner can set.

Implementation notes tied to these rulings:
- O1: `Stamina`/`MaxStamina` removed from the attribute set and combat
  component in the same commit as this file.
- O2: wave mode + time-to-kill instrumentation in the gym (same commit).
- O5: `EBreakerDamageFamily::Elemental` stays as the pipeline family; the
  Rift/Time/Void split arrives with the resistance model.
