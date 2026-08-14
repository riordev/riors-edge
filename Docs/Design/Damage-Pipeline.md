# Damage Pipeline — canonical resolution spec

Last reconciled against: O32

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
3. **Critical roll**, or the previously snapshotted critical result. Crit
   remains the only multiplier of its kind.
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

- GAP [O2/O3]: whether the 1.30x cap extends to constellation Mores is
  unruled. ASSUMING it does, the composed ceiling is 1.30^3 ≈ **2.20x**.
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
- **DRIFT, flagged rather than fixed:** the aggregator computes its ceiling as
  `SingleMoreCeiling^MaxComposedMoreSources` = 1.30³ = **2.197**, precisely so
  the number is derived from the two constants that define it; the combat
  component restates it as a literal **2.20f**. The two clamps therefore differ
  by 0.003 and sit in different stages. Harmless today; it is exactly the
  "third constant that can drift" the aggregator's own comment says it avoids.

**Aberrant exclusion (O3 extension, per directive):** Aberrant signature
affixes may rewrite rules but may NOT author a More multiplier. Anomalous
items remain the only item-layer source of one. Otherwise three equipped
Aberrants × 2 signatures reopens at the item layer exactly what the tree
layer's one-per-keystone rule closed.

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
| 3 Critical | **Built**, including the snapshot path. Crit is still the only multiplier of its kind. |
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
