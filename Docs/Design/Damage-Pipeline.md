# Damage Pipeline — canonical resolution spec

> STATUS 2026-08-16: PARTIALLY BUILT — the resolution order is largely implemented, but sign-off is blocked on Replication-Position.md (O22), and the live hit-feedback defects D15–D19 (HANDOFF §6) all sit in this pipeline's domain.

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Status: Tier 0 spec (Directive Task 3). Supersedes Master-Sheet-Import §6.1's
seven steps. Four documents previously guessed at this ordering; they now
reference this file. No balance values are authored here; every number cited
is either an existing ruling or flagged GAP.

## 1. Resolution order

Every damage event — weapon, ability, status tick, hazard, enemy — resolves
in exactly this order:

1. **Base damage and source scaling.** Flat sums first, then the single
   additive Increased bucket per stat, then More multipliers as an
   unordered product (O3). DoTs use their application snapshot, including
   the tick interval (O10).
2. **Weak-point multiplier**, when applicable.
3. **Critical roll**, or the previously snapshotted critical result. **Amended
   by O34** — see §4a: crit and weak point are the two site multipliers; the
   old "only multiplier of its kind" wording is superseded.
4. **Passive dodge roll** (full evasion) then **passive block roll**
   (partial reduction) — O1. Neither applies to damage over time.
   **AS BUILT, a deviation worth knowing:** both are *rolled* here, but the
   block *reduction* is applied after step 5 and after the incoming-damage
   multiplier, so a blocked hit is reduced from the already-mitigated figure
   rather than from the raw one. A dodge returns immediately with zero damage
   and, because `UBreakerCombatComponent` short-circuits, raises neither
   `OnDamageTaken` nor `OnHitDealt` — anything keyed off being hit (the
   Reflective modifier, on-hit affixes) correctly sees nothing.
5. **Facing selects the base armour value** (Encounter-Design §7: armoured
   facings are real armour, not a gimmick), THEN armour composition
   (section 2), THEN the mitigation formula. Physical shield-bypassing
   DoTs take half the result (existing global status rule).
6. **Per-element resistance** — Rift, Entropy, or Void (O5/O19) — applied
   after armour mitigation, before shield routing. True Damage skips 5
   and 6. GAP [O5]: resistance formula and value ranges unauthored (O2
   freeze).
7. **Shield routing**, unless the event explicitly bypasses shields.
8. **Remaining damage to health.**
9. **Shield-break, damage, dodge/block, and death events.**

## 2. Armour composition rule

Three systems write into the armour step (Marksman's Ledger bypass, Rot's
flat reduction, facing armour) plus the boss cap. The **specified** composition:

```
FacingArmour = facing selects the base armour value
AfterFlat    = max(FacingArmour - flat reductions, 0)      // floor at 0 —
                                                           // never negative
AfterBypass  = AfterFlat * (1 - bypass fraction)
Mitigation   = AfterBypass / (AfterBypass + 100), capped at 80%
BossClamp    = on Boss-rank targets, total armour REDUCTION (flat + bypass)
               may not exceed the boss cap (Game-Modes §4.5: 60%)
```

Armour-shred stacking into negative armour is the classic failure; the
explicit floor at 0 forbids it. The boss cap clamps the *reduction*, not
the mitigation, so status builds still function against bosses (Master
§7.10 risk 5).

**AS BUILT (2026-08-14).** Three of the five lines exist and two do not:

| Line | Built? | Where |
|---|---|---|
| Facing selects the armour value | **yes** | `UBreakerDamageLibrary::GetFacingArmorMultiplier` — a 2D dot against the hit's `SourceLocation` (Z deliberately excluded), returning 1.0 in the front arc and `RearArmorMultiplier` behind it. Applied per hit in `UBreakerCombatComponent::ReceiveDamage`, after the flat strippers and before the curve. `RearArcArmorMultiplier` defaults to **1.0, i.e. OFF**; `RearArcCosine` 0.15. The only writer today is the Warden (and therefore the boss), which sets it to **0.0** — a rear or flank hit bypasses its armour entirely. |
| Flat reductions, floored at 0 | **yes** | `PushArmorReduction(Key, Flat)` / `PopArmorReduction`; `GetEffectiveArmor() = max(0, Armor - sum(reductions))`. Keyed, so a re-push replaces rather than stacks. Zones strip armour keyed by zone tag and release it on exit. |
| The mitigation curve | **yes** | `Armor / (Armor + 100)`, capped at **0.80**. Both constants are in `CalculateArmorMitigation`. `TrueDamage` skips the step entirely; a physical shield-bypassing DoT takes half the result. |
| Fractional **bypass** | **no** | There is no bypass-fraction term. `FBreakerDamageRequest::ArmorPenetration` is a **flat subtraction** inside the curve, not a multiplier, and no weapon or ability authors a non-zero value today. A percentage bypass (Marksman's Ledger) would need a new term. |
| The **boss armour cap** | **no** | Nothing clamps total armour reduction on a Boss-rank target. The Field Marshal instead halves its own frontal armour in phase 3 (90 -> 45) and exposes a rear weak point, which is a different mechanism reaching a similar place. |

## 3. Proc coefficient law (E1, promoted)

Governs weapons, classes, and items alike:

- Multishot-generated projectiles carry **coefficient 0** for status
  application, on-hit effects, and node triggers, and **1** for damage.
- Ricochets carry **coefficient 0.5** and cannot chain (a ricochet never
  spawns another ricochet).
- Affliction spread/transfer ancestry caps at **depth 2** with a
  normalized payload (a spread copy carries the original's remaining
  budget, never a fresh full application).
- DoT ticks trigger only effects that declare DoT compatibility (existing
  contract).

## 4. Composed More ceiling

Per O3: a build holds at most **3** More multipliers — one from the class
branch keystone, and at most two from constellations (the 26-point cost
structure makes a third constellation keystone unreachable). Class-Kits
caps its keystone More at 1.30x.

- **RULED by O34** (was GAP [O2/O3]): yes — there is **ONE** More ceiling,
  and it applies across every source without exception: class branch
  keystone, constellation Convergence/Keystone, and **temporary ability
  windows, which ARE Mores and count within the same budget** (this resolves
  Overdrive's self-flagged 4th-More — it now competes for headroom with the
  tree Mores, which is the choice O27 wants). The ceiling is derived from the
  aggregator: 1.30^3 ≈ **2.197**. See §4a for the full canon table.
- ~~Tier 1 engineering hook: an automated test must assert the composed
  product of all active Mores never exceeds the ceiling constant.~~
  **BUILT, and it is a clamp rather than only an assertion.** Two enforcement
  points, in two different stages:
  - `UBreakerProgressionComponent::AggregateStats` selects the strongest
    `MaxDamageMoreSources` (3) of the Mores a build owns and clamps each to
    `SingleMoreCeiling` (1.30x), so a fourth purchase is dead weight rather
    than a quiet nerf to the three the player chose.
  - `FBreakerAttributeAggregator::Compose` clamps the **composed** More product
    **globally across every contributor**, for `DamageMultiplier` and nothing
    else. A layer arriving second cannot buy its way past O3.
  - The outgoing-modifier chain in `UBreakerCombatComponent` clamps separately
    at its own `ComposedMoreCeiling` and warning-logs when it bites.
- **DRIFT — now a ruled fix, not an accepted deviation.** The aggregator
  computes its ceiling as `SingleMoreCeiling^MaxComposedMoreSources` = 1.30³ =
  **2.197**; `UBreakerCombatComponent::ComposedMoreCeiling` still restates a
  separate literal **2.20f** (verified in code as of this reconciliation
  pass — `Source/RiorsEdge/Combat/BreakerCombatComponent.h:132`). **O34 rules
  there is one ceiling, derived from the aggregator, and this separate
  constant is deleted; its product counts against the same 2.197 budget.**
  The 0.003 gap is no longer "harmless today" — it is an open implementation
  GAP against a ruled decision, not an open design question, and belongs on a
  code lane's list.

**Aberrant exclusion (O3 extension, per directive):** Aberrant signature
affixes may rewrite rules but may NOT author a More multiplier. Anomalous
items remain the only item-layer source of one. Otherwise three equipped
Aberrants × 2 signatures reopens at the item layer exactly what the tree
layer's one-per-keystone rule closed.

## 4a. The multiplier canon (O34)

O34 rules that this document carries the canonical list of every lane
permitted to touch outgoing player damage. **A new lane requires a canon row
in the table below plus a conformance test before it may merge** — this
section is a standing discipline, not a one-time cleanup.

| Lane | Owner system | Bucket / cap | Status |
|---|---|---|---|
| **Flat** | Base damage, flat affix lines | Summed first, step 1 | Conforming |
| **Increased (additive)** | Affixes, tree Minor/Notable nodes | One additive percentage bucket per stat, no cap of its own | Conforming |
| **More (budget)** | Class branch keystone (≤1) + Core constellation Convergence/Keystone (≤2) + **temporary ability windows** — O34: ability windows ARE Mores | **ONE ceiling**, unordered product, 1.30³ ≈ 2.197 (O3, amended O34) | Conforming by ruling — see §4's DRIFT note above for the pending code-side cleanup (the combat chain's separate 2.20f constant is not yet deleted) |
| **Crit** | Crit chance / crit multiplier system | Site multiplier, **build-gated** — one of exactly two multipliers permitted at the hit site (O34 amended wording) | Conforming |
| **Weak point** | Aim-skill hit detection | Site multiplier, **skill-gated**, archetype-bounded **[1.0, 2.0]**, explicitly **outside** the O3 More budget (O34) | Conforming by ruling (explicit carve-out) |
| **Distance / edge falloff** | Per-pellet weapon geometry | Geometric falloff curve, evaluated per pellet — not a stat-layer multiplier | Conforming |
| **Fire rate** | Weapon cadence, ability haste, affixes (e.g. Volley's Cyclic) | **Named, watched, currently uncapped** (O34) | Conforming — no cap is ruled for this lane today; flagged so it is watched rather than allowed to drift into a de facto More |
| **Target-conditional Increased (riders)** | Tree node effects whose requirement names a `Target*` condition. Published by `UBreakerProgressionComponent` as a rider table (requirement, stat target, rank-scaled percent), resolved by `UBreakerCombatComponent::ReceiveDamage` — the one site that knows both actors (H3) | Joins the **same additive Increased bucket** as every other Increased line, never a multiplier: the request carries the source split (`SourceIncreasedPercent` / `SourceMoreProduct`, with `SourceDamageMultiplier` kept as the composed convenience) and the target side recomposes `(1 + (Increased + RiderPercent)/100) × MoreProduct` only when a rider fired and the split is present. A target-conditional **MorePercent is not supported by rule** (Hook-And-Condition-Vocabulary §3.3) and is warn-and-dropped like every other unpaid More | Conforming — conformance test `RiorsEdge.Combat.TargetRiders.*` (the §4a toll for this lane). Coverage today: the weapon submission paths (hitscan and rocket) carry the split; ability submissions and DoT ticks are composed-only and cannot fire riders yet — stated, not hidden |
| **DoT composition** | Status / DoT application | **CURRENT:** `DamageOverTimeMultiplier × SourcePower` — multiplies rather than joining the additive Increased bucket | **Deviating, with an open owner Q.** O34, verbatim: "whether Increased Damage and Increased DoT share one additive bucket for DoT ticks, or keep multiplying, is deliberately NOT ruled today. Currently they multiply." Recorded as the one deliberate multiplicative deviation in the canon, pending the owner call. (`Class-Kits.md`'s VW12 node is already blocked on this question.) |

**Amended wording, per O34.** The locked line that "crit is the only
multiplier of its kind" (§1 step 3, §7's AS BUILT table) is superseded. It now
reads: **crit and weak point are the two site multipliers — crit is
build-gated, weak point is skill-gated, and nothing else may multiply at the
site.**

## 5. Replication pointer (O22)

The owner's replication position paper is due before this spec closes. If
combat is ruled server-authoritative, the proc coefficient law (§3) and
the composed More ceiling assertion (§4) are server-side enforcement
points; nothing in §1's ordering changes, but where it executes does.
This section is replaced by a reference to that page when it lands.

## 6. Seed targets (O18)

Wave mode reports divergence from these designer inputs; the stat chassis
is solved backwards from them: trash TTK a little under 1s, scaling
exponentially with difficulty; rare/elite ~3s; boss 20–45s unless special.
Player TTD 4–5s with no resources or sustain, substantially higher with
investment. O23: XP §5.1's Veteran 3.0x XP multiplier is flagged against
the 2.0x-health chassis on the same report.

## 7. Current code vs this spec (2026-08-14)

`UBreakerDamageLibrary::ResolveDamage` implements steps 1-5 and 7-9.
`UBreakerCombatComponent::ReceiveDamage` wraps it and contributes the facing
selection, the flat armour strippers, gear Physical DR and the keyed
incoming-damage multiplier chain before handing the request down.

| Step | State |
|---|---|
| 1 Base and source scaling | **Built.** Flat lane, one additive Increased bucket, More product, all clamped as §4 describes. DoTs snapshot `SourcePower`, crit chance/multiplier, `DamageOverTimeMultiplier` and the tick interval at application (O10); a reapplication adds a stack and refreshes duration but **keeps the original snapshot**. |
| 2 Weak point | **Built**, plus a world-space forgiveness halo (`WeakPointToleranceCm`, 14 cm) so acceptance has a felt edge. Set it to 0 for a measuring run — it inflates damage per hit by 8-14%. |
| 3 Critical | **Built**, including the snapshot path. **Amended by O34** — see §4a: crit and weak point are now the two site multipliers, not crit alone. |
| 4 Dodge / block | **Built.** See the deviation note in §1: the block *reduction* lands after armour. |
| 5 Armour | **Built except fractional bypass and the boss cap** — see §2's as-built table. |
| 6 Element resistance | **NOT BUILT.** `EBreakerDamageFamily` is `Physical / Elemental / TrueDamage` and there is no per-element split, no resistance attribute and no consumer. `EBreakerStatTarget::ElementalDamageReduction` is reserved and deliberately absent from the affix pool, so it lies to nobody. GAP [O5]. |
| 7-9 Shield, health, events | **Built.** Shield routing, shield-break, kill and the attacker-side `OnHitDealt` / `OnKillDealt` pair with `FBreakerHitContext` (a DoT tick credits its applier). |

**Healing resolves through this contract too**, added since this spec was
written: `ResolveHealing(FBreakerHealRequest, FBreakerVitalsState)` mirrors the
damage path — health first, then overheal, then optional overheal-to-shield at
an authored fraction. Overheal is reported at its full value even when part of
it became shield, deliberately, so a future Support Charge generates nothing
from it. `ApplyHealing` **refuses to heal a dead actor**: healing is not
revival. Two affix lines (Health on Kill, Resource on Kill) and one legendary
already route through it rather than writing Health directly.

**§3's proc coefficient law is still UNENFORCED.** `ProcCoefficient` exists on
the request and on the status spec, and exactly one system reads it:
`UBreakerManaComponent::HitGeneration`, where a coefficient of 0 is how a DoT
tick generates no resource. Nothing implements the multishot-0, ricochet-0.5,
depth-2 spread rules — there is no multishot and no ricochet in the code yet,
so the law is unbroken rather than obeyed. It becomes real work the day either
ships.

**§5's replication pointer still stands.** O22 is unanswered; the position paper
is what decides whether §3 and §4 are server-side enforcement points, and it
also decides whether recoil should be client-predicted. Nothing in §1's ordering
depends on the answer — only where it executes.
