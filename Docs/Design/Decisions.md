# Owner Decisions — 2026-08-12

Rulings on the Design-Overview §7 owner-decision list. These supersede
conflicting text anywhere else in Docs/ and the master sheet import.

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

Implementation notes tied to these rulings:
- O1: `Stamina`/`MaxStamina` removed from the attribute set and combat
  component in the same commit as this file.
- O2: wave mode + time-to-kill instrumentation in the gym (same commit).
- O5: `EBreakerDamageFamily::Elemental` stays as the pipeline family; the
  Rift/Time/Void split arrives with the resistance model.
