# Damage Pipeline — canonical resolution spec

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
flat reduction, facing armour) plus the boss cap. They compose as:

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
- Tier 1 engineering hook: an automated test must assert the composed
  product of all active Mores never exceeds the ceiling constant.

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

## 7. Current code vs this spec

`UBreakerDamageLibrary::ResolveDamage` today implements steps 1-4, 5
(without facing or composition — armour + penetration only), 7, 8, 9.
Missing: facing armour selection, flat-reduction/bypass/boss-cap
composition, and the entire element resistance step. The proc coefficient
field exists on the request but no consumer enforces the law in section 3
yet. All of that is Tier 1+ work, sequenced behind wave-mode measurement.
