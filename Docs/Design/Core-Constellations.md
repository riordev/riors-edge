# Core Constellations — the six universal trees

Status: design pass 1. Values are placeholder and must be re-anchored after the Playtest Gym TTK pass, consistent with Affixes §3 ("ALL VALUES ARE PLACEHOLDER").

Scope: the six universal Core Tree constellations spent with Core Points — Precision, Volley, Affliction, Elements, Bulwark, Kinesis. Class trees and affixes are out of scope and are governed by `Docs/Layer-Ownership.md`.

---

## 1. Governing constraints

These are inherited, not proposed. Nothing below may violate them.

| Constraint | Source |
|---|---|
| Level cap 50, hard stop; all endgame power from gear | Master Sheet §7.1 |
| ~65 Core Points = two full constellations plus one partial | §7.2, §7.4 |
| Air jump (Kinesis) and Parry (Bulwark) are the ONLY tree-granted verbs | §5.2, §7.6 |
| Dash, slide, wall ride, block, dodge are base kit — trees improve, never unlock | §5.2 |
| Crit is the only multiplier of its kind; no parallel multiplier via nodes | §6.3 |
| Flat sums → one additive Increased bucket → More multipliers reserved for trees/Anomalous | Item-Foundation locked rule |
| A node that reads as a flat percentage is doing the affix layer's job | §2.6, §7.6 |
| Core nodes must not outperform the class branch they overlap | §7.10 risk 2 |
| No cornerstone may be universally superior | §7.10 risk 3 |
| No grapple, no tether | §5.1 |
| Elements is BLOCKED pending a resistance model | §6.1, §3.2 |

**Design consequence taken seriously:** with only two verbs left to grant, every node here is either (a) a *conditional* quality change, (b) a rule rewrite, or (c) one of the two verb grants. Where a node does carry a percentage, it is a **More** multiplier or a conditional gate that gear cannot produce — never a duplicate of an existing affix line.

---

## 2. Shared cost grammar

One grammar across all six constellations, so the UI teaches itself once.

| Node class | Cost | Ranks | Role |
|---|---|---|---|
| **Gateway** | 1 | 1 | Entry. Cheap, always worth the first point. |
| **Minor** | 1 / rank | 3 | Conditional scaling. The only place percentages live. |
| **Notable** | 2 | 1 | A small rule change or a conditional More multiplier. |
| **Link** | 1 | 1 | Pure adjacency. Buys cross-lane access, no effect of its own beyond a token. |
| **Convergence** | 3 | 1 | Requires two of three lanes completed. Combines two lanes' fantasies. |
| **Keystone** | 5 | 1 | Rewrites a rule, with a real cost. Requires 18 points spent in this constellation. |

### 2.1 Per-constellation total

Every constellation contains exactly **11 nodes** totalling **26 points**:

```
Gateway (1)
  ├─ Lane A: Minor 3r (3) → Notable (2)      = 5
  ├─ Lane B: Minor 3r (3) → Notable (2)      = 5
  └─ Lane C: Minor 3r (3) → Notable (2)      = 5
  Link A↔B (1), Link B↔C (1)                 = 2
  Convergence (3)   [requires 2 lanes done]
  Keystone (5)      [requires 18 spent here]
                                     TOTAL   = 26
```

### 2.2 Budget validation — does 65 still mean "two full plus one partial"?

| Spend | Points | Result |
|---|---|---|
| Constellation 1, complete | 26 | 26 |
| Constellation 2, complete | 26 | 52 |
| Constellation 3, partial | 13 | 65 |

13 points into a third constellation buys Gateway + two complete lanes + one link (1+5+5+1 = 12) with one point spare, or Gateway + one lane + Convergence prerequisites-short. It **cannot** reach a third keystone (18-point gate + 5 cost = 23). That is the intended shape: **a character has at most two keystones, ever.** This is the single most important structural decision in the document and it is what keeps the six-way choice real.

Alternative shapes rejected:
- 20-point constellations → 65 buys three full trees, three keystones, and the choice collapses.
- 32-point constellations → 65 buys two full and almost nothing else; the third constellation stops being a decision.

**If node costs change, re-run this table.** Master Sheet §7.4 requires it.

### 2.3 Adjacency rules (all constellations)

1. Gateway has no prerequisite beyond owning ≥1 Core Point.
2. A lane's Notable requires that lane's Minor at rank 3.
3. Links connect adjacent lanes and require either neighbouring Minor at rank 1. They exist so a player can reach a second lane's Notable without fully committing to the first lane's Minor — this is the only way to build a "two-notable, no-keystone" splash.
4. Convergence requires **two** lanes complete (Minor r3 + Notable).
5. Keystone requires **18 points spent in this constellation**, which in practice means Gateway + all three lanes + Convergence (1+15+3 = 19) or Gateway + three lanes + both links (1+15+2 = 18). Two legal routes; the second is cheaper but skips Convergence.
6. Refunds are free at a Forge (§7.8). Deallocation must validate downstream dependents — the progression subsystem already stores IDs and ranks, so a respec is a full rebuild, not a decrement.

---

## 3. PRECISION

**Theme:** the reward for holding an aim. Precision is the anti-movement constellation — everything in it pays out for staying on one target, and its keystone actively punishes target-switching. It is the constellation that makes a Tank or a Gunsmith into a sniper without touching Swift/Marksman's projectile fantasy.

**Owns:** critical hits, weak points, executions, sustained single-target accuracy (§7.6).

**Lanes:** A — Weak Points · B — Streak · C — Execution

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| P0 | **Sightline** | Gateway | 1 | Weak-point hits no longer suffer damage falloff at any range. |
| P1 | **Anatomy** | Minor ×3 | 3 | Weak-point volume on all non-boss enemies is enlarged 8% / 16% / 24%. Geometry, not damage — invisible to the affix layer. |
| P2 | **Called Shot** | Notable | 2 | The first shot fired after leaving ADS-idle for ≥0.6s cannot miss its weak point if the crosshair is on the target's hitbox at all. Internal cooldown 3s. |
| P3 | **Fixate** | Minor ×3 | 3 | Consecutive hits on the same target grant a stacking **More** damage multiplier of 1% / 2% / 3% per stack, max 6 stacks. Resets on target swap or 2.0s without a hit. |
| P4 | **Tunnel Vision** | Notable | 2 | Your streak no longer resets on a miss — only on a target swap or timeout. Streak state is per-attacker/per-target with an explicit timeout (see Progression-Architecture: this needs real state, not a counter on the weapon). |
| P5 | **Coup** | Minor ×3 | 3 | Enemies below 15% / 20% / 25% health take critical hits from you as though your Critical Multiplier were at its snapshot maximum. No new multiplier is introduced — it forces the existing roll high. |
| P6 | **Mercy Rule** | Notable | 2 | Killing an enemy below the Coup threshold refunds the shot's ammunition and does not break your Fixate streak. |
| P7 | **Link — Steady** | Link | 1 | Connects Lane A ↔ B. Weak-point hits add 2 Fixate stacks instead of 1. |
| P8 | **Link — Finisher** | Link | 1 | Connects Lane B ↔ C. Reaching 6 Fixate stacks marks the target as Coup-eligible regardless of its health. |
| P9 | **Marksman's Ledger** | Convergence | 3 | Requires two lanes. Your critical hits against a target you have hit at least 3 consecutive times bypass 50% of that target's Armour. Bypass, not reduction — it does not stack with class armour-shred into negative armour. |
| **P10** | **FIXATION** | **Keystone** | **5** | **Rewrite:** after 6 consecutive hits on a single target, your Critical Chance against *that* target is treated as 100%. Against **every other target**, your Critical Chance is treated as **0%** until the streak ends. Streak ends on target swap or 2.0s without a hit. |

**Why the keystone is not universally superior:** it deletes crit entirely in add-clear, trash waves, and any multi-target fight — which is most of the game's minute-to-minute. It is a boss/elite keystone. A Volley or Affliction build would be actively worse for taking it. That is the intended shape of every keystone here.

**Balance notes.** Fixate is a **More** multiplier and is therefore legal per the locked aggregation rule (More reserved for trees/Anomalous), but at 6 stacks × 3% it is only 1.19x — deliberately modest, because it multiplies against everything. Coup must read the *snapshot* critical result for DoTs (§6.4) or it will retroactively upgrade running bleeds.

**CONFLICT — none.** Precision introduces no second multiplier of crit's kind; FIXATION manipulates Critical Chance, which §6.3 names as one of the two sources of truth.

---

## 4. VOLLEY

**Theme:** the gun as a machine rather than an aim. Volley cares about rounds leaving the barrel — count, cadence, magazine, ricochet — and never about where they land. It is the add-clear constellation and the natural partner to Affliction.

**Owns:** projectile count, cadence, magazine/reload behavior, ricochet, independent projectile outcomes (§7.6).

**Lanes:** A — Cadence · B — Magazine · C — Ricochet

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| V0 | **Trigger Discipline** | Gateway | 1 | Your first three shots after a reload have no recoil accumulation. |
| V1 | **Cyclic** | Minor ×3 | 3 | Firing continuously without releasing the trigger grants +4% / +8% / +12% fire rate after 1.0s of sustained fire, decaying over 0.5s on release. Conditional and self-cancelling — gear cannot produce it. |
| V2 | **Runaway** | Notable | 2 | Cyclic's ramp no longer decays on release. It decays only on reload or weapon swap. |
| V3 | **Deep Well** | Minor ×3 | 3 | The last 20% / 30% / 40% of your magazine fires without consuming ammunition reserve on kill. Rewards emptying the magazine rather than tap-reloading. |
| V4 | **Last Round** | Notable | 2 | The final round in any magazine fires as a full Multishot volley at your current Multishot count, each projectile at full damage. **Proc coefficient of the extra projectiles is 0** (see Balance). |
| V5 | **Carom** | Minor ×3 | 3 | Ricochets seek the nearest enemy within 6m / 9m / 12m instead of reflecting off geometry angle. Behavioural, not a chance increase — Ricochet Chance stays on the affix layer. |
| V6 | **Crossfire** | Notable | 2 | A ricochet that hits a target already hit by the originating shot deals double its own damage. Bounded: the ricochet's damage, not the shot's, and it cannot chain a second time. |
| V7 | **Link — Feed** | Link | 1 | Connects A ↔ B. Kills while Cyclic is at maximum return 1 round to the magazine directly. |
| V8 | **Link — Spray** | Link | 1 | Connects B ↔ C. Rounds fired from the Deep Well portion of the magazine gain +1 Ricochet bounce. |
| V9 | **Barrage Doctrine** | Convergence | 3 | Requires two lanes. Multishot projectiles no longer share a spread cone; they fan deterministically across the cone at even intervals. Authority-side generation, seeded per shot — see Progression-Architecture note on deterministic extra projectiles. |
| **V10** | **PERPETUAL** | **Keystone** | **5** | **Rewrite:** you can no longer reload. Your magazine refills at 10% of capacity per second while not firing, beginning 0.8s after your last shot. All "on reload" effects — including the Guaranteed Crit After Reload affix and Volley's own Last Round — are disabled. |

**Why the keystone is not universally superior:** it deletes burst. A player who empties a magazine in two seconds gets it back in ten. It suits high-fire-rate sustained builds and is a straightforward downgrade for a Sniper or Rocket loadout. It also disables one of the two Precision-adjacent affix lines, which is exactly the anti-synergy that keeps keystone pairs from stacking.

**Balance notes — the proc coefficient problem.** Volley is the single largest multiplicative-explosion risk in the game (§7.10 risk 1). Ruling for this constellation, EXTENDS the master sheet:

> **Multishot-generated projectiles carry proc coefficient 0 for status application, ailment application, on-hit affixes, and Core node triggers. They carry proc coefficient 1 for damage only.**

Without this, Volley × Affliction × Elements is unbounded. Ricochets carry proc coefficient 0.5 and cannot generate further ricochets.

**EXTENDS** — §3.5 states Multishot "should multiply with nothing." Barrage Doctrine changes the *geometry* of multishot rather than its count, which is compatible, but the deterministic fan needs the authority-side generation path named in Progression-Architecture before V9 can ship.

---

## 5. AFFLICTION

**Theme:** damage you have already dealt. Affliction is patient — it wants the target to be dying before it fires the next shot, and it wants that dying to be contagious. Physical only: Bleed and Poison. It ships before Elements because it needs no resistance model (§3.7, §6.2).

**Owns:** physical damage over time and spread (§7.6).

**Lanes:** A — Application · B — Duration · C — Spread

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| A0 | **Open Wound** | Gateway | 1 | Weak-point hits always apply one stack of Bleed, ignoring Bleed Chance. |
| A1 | **Deepen** | Minor ×3 | 3 | Your Bleed and Poison stack caps increase by 1 / 2 / 3. Stack cap is tree-owned; stack *magnitude* stays on gear. |
| A2 | **Saturate** | Notable | 2 | Applying a stack to a target already at its cap refreshes every existing stack's duration instead of being discarded. Converts overkill application into uptime. |
| A3 | **Slow Bleed** | Minor ×3 | 3 | Your physical DoTs tick 15% / 25% / 35% slower and each tick deals proportionally more, to the same total. Fewer, larger ticks: better against armour rounding, worse against short-lived targets, and it deliberately fights the Tick Frequency affix rather than stacking with it. |
| A4 | **Necrosis** | Notable | 2 | A target at maximum Bleed or Poison stacks takes 15% **More** damage from your direct weapon hits. The bridge that makes Affliction worth taking alongside a shooting constellation. |
| A5 | **Vector** | Minor ×3 | 3 | When an afflicted enemy dies, its remaining ailment duration transfers to the nearest enemy within 4m / 7m / 10m at 50% magnitude. One hop. |
| A6 | **Contagion** | Notable | 2 | Vector's transfer no longer requires death — it occurs on a 4s interval from any afflicted target. Ancestry depth is capped at **2**; a second-generation transfer cannot transfer again. |
| A7 | **Link — Rot** | Link | 1 | Connects A ↔ B. Poison stacks no longer expire individually; the whole application expires at once. |
| A8 | **Link — Carrier** | Link | 1 | Connects B ↔ C. Transferred ailments inherit your Slow Bleed tick rate rather than the source's. |
| A9 | **Epidemiology** | Convergence | 3 | Requires two lanes. Transferred ailments copy a **normalized status payload** — fixed magnitude derived from the original snapshot, no re-roll, no fresh kill-proc. Explicitly *not* a fresh application. |
| **A10** | **TERMINAL** | **Keystone** | **5** | **Rewrite:** your physical DoTs no longer deal damage over time. Each application banks its full remaining damage silently; the entire banked total is delivered as a single instance when the target dies, when the ailment expires, or when you land a weak-point hit on that target. Banked damage cannot critically strike (the crit was already resolved at snapshot) and does not benefit from Tick Frequency. |

**Why the keystone is not universally superior:** it removes all pressure damage. Nothing dies while you are reloading, and against a target you cannot finish, you have dealt zero damage. It is a burst-detonation build, not a sustain build. It also makes Contagion far weaker (there is no tick to spread), so it competes with the constellation's own Lane C.

**Balance notes.** A9 is the guard against Progression-Architecture's stated Contagion failure mode: recursive spreads treated as fresh applications. Ancestry depth 2 plus normalized payload plus proc coefficient 0 on transfers is the full mitigation set; all three are required, not alternatives.

Slow Bleed intentionally *conflicts* with the Tick Frequency affix rather than multiplying it — §3.7 names Tick Frequency "the most dangerous affix in the game," and a tree node that stacks with it would compound the problem.

---

## 6. ELEMENTS — **BLOCKED**

> **BLOCKED.** Elements cannot ship until an elemental resistance model exists in the damage pipeline. Master Sheet §6.1: "there is no elemental resistance step. Combat-Foundation has Armor only. This blocks every elemental affix and the Elements constellation." §3.2 blocks Elemental Damage Reduction; §3.7 blocks Ignite, Chill, and Shock.
>
> Every node below carries an explicit dependency tag. **No node in this constellation may be authored as a Data Asset until its tag is cleared.**

**Dependency tags used:**
- `[ELEM-RES]` — requires the elemental resistance model (one stat vs per-element is itself open, §6.7).
- `[ELEM-BUILDUP]` — requires a threshold/buildup track on the status component (the component records buildup already per Progression-Architecture, but no elemental status writes to it).
- `[ELEM-MATRIX]` — requires `UElementReactionDataAsset` and a reaction resolution order.
- `[ELEM-PIPE]` — requires a resistance step inserted into the §6.1 damage resolution order between armour mitigation and shield routing.

**Theme:** Elements does not deal elemental damage — it *conducts* it. Every node is about buildup, thresholds, and what happens when two elements meet. This is deliberate: it keeps Elements from being "the damage constellation for Casters" and preserves Multispell's identity (§7.10 risk 2).

**Lanes:** A — Buildup · B — Reaction · C — Conduction

| # | Node | Class | Cost | Effect | Tags |
|---|---|---|---|---|---|
| E0 | **Conductive** | Gateway | 1 | Elemental buildup on a target decays 50% slower. | `[ELEM-BUILDUP]` |
| E1 | **Charge Up** | Minor ×3 | 3 | Elemental buildup you apply is increased 10% / 20% / 30%. Buildup, not damage — the one legal percentage here because buildup has no affix line. | `[ELEM-BUILDUP]` |
| E2 | **Threshold** | Notable | 2 | Reaching a status threshold no longer consumes the buildup bar; it resets to 50% instead of 0. | `[ELEM-BUILDUP]` |
| E3 | **Catalyst** | Minor ×3 | 3 | Reactions you trigger have their internal cooldown reduced by 15% / 25% / 35%. | `[ELEM-MATRIX]` |
| E4 | **Second Order** | Notable | 2 | A reaction leaves its *first* element on the target instead of consuming both. Enables deliberate reaction chains without the accidental double-trigger that Progression-Architecture warns against — because the *second* element is always the one consumed, resolution order stays deterministic. | `[ELEM-MATRIX]` |
| E5 | **Penetrance** | Minor ×3 | 3 | Your elemental damage ignores 10% / 18% / 25% of the target's elemental resistance. | `[ELEM-RES]` `[ELEM-PIPE]` |
| E6 | **Insulator's Bane** | Notable | 2 | Against a target whose resistance to your element is its *highest* resistance, your buildup rate is doubled. Punishes the enemy's strength rather than seeking its weakness — the anti-optimal-target rule. | `[ELEM-RES]` |
| E7 | **Link — Arc** | Link | 1 | Connects A ↔ B. Buildup applied within 1.0s of a reaction is doubled. | `[ELEM-BUILDUP]` `[ELEM-MATRIX]` |
| E8 | **Link — Ground** | Link | 1 | Connects B ↔ C. Reactions apply a flat elemental resistance reduction of 10 for 4s. Flat, not percentage — it cannot drive resistance negative when stacked. | `[ELEM-RES]` `[ELEM-MATRIX]` |
| E9 | **Reaction Chain** | Convergence | 3 | Requires two lanes. A reaction can trigger a second reaction on the same target, once, if a third element is present. Hard depth cap of 2. Proc coefficient of the second reaction is 0. | `[ELEM-MATRIX]` |
| **E10** | **RESONANCE** | **Keystone** | **5** | **Rewrite:** you may carry only one element, chosen at a Forge. All elemental damage you deal converts to that element. In exchange, applying *any* other source's element to a target you have already built up instantly fills that target's buildup to threshold and triggers the reaction at maximum magnitude. | `[ELEM-RES]` `[ELEM-MATRIX]` `[ELEM-BUILDUP]` |

**Why the keystone is not universally superior:** mono-element is a severe restriction in a game whose enemy families are meant to have differing resistances, and RESONANCE's payoff depends on a *second* element arriving from somewhere — a class ability, a weapon, or an ally. Solo, that is a real constraint; solo is the primary balance target.

**CONFLICT — Multispell.** RESONANCE is close to Caster/Multispell's stated fantasy ("sequencing different elements to create reactions"). Two mitigations are proposed and one must be chosen:
1. RESONANCE's mono-element restriction is precisely what Multispell does *not* have, so the constellation is a worse version of the class for a Caster and a genuine option for everyone else. (Preferred.)
2. Multispell gains an explicit exemption node making it the only source that can carry two elements under RESONANCE.

Recorded as unresolved in Open Questions.

**Implementation prerequisite, restated:** §6.1's damage resolution order has seven steps and no resistance step. Elements requires an eighth. Adding it after step 4 (armour mitigation) and before step 5 (shield routing) is the least disruptive insertion point — but that ordering choice is a real decision and is not made here.

---

## 7. BULWARK

**Theme:** the refusal to be moved. Bulwark deepens the universal Block and grants Parry. Its keystone converts randomness into certainty — which is the single most valuable thing you can give an encounter designer.

**Owns:** armour, mitigation, stamina efficiency, and parry (§7.6). **Does not grant Block** (§7.6).

> **CONFLICT — Block/Dodge model.** This document treats **Block as a passive chance to reduce incoming damage** and **Dodge as a passive chance to fully evade**, per the current design direction and consistent with the affix lines `Block %` and `Dodge %` in §3.8. Master Sheet §7.7 and Progression-Architecture describe block and dodge as *stamina-spending player actions* sharing a 100-point pool, and `UBreakerCombatComponent` currently implements them that way (frontal-only block stance, dodge negation window with resource refund). **These two models cannot both be true.** Nodes below are written for the passive-chance model; nodes that reference stamina are marked `[STAMINA-DEP]` and are only meaningful if the action model survives. Resolution required before authoring.

**Lanes:** A — Guard · B — Parry · C — Armour

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| B0 | **Set Stance** | Gateway | 1 | Your Block chance applies to a 180° frontal arc rather than a 120° one. |
| B1 | **Bracing** | Minor ×3 | 3 | A successful Block reduces the damage of the *next* incoming hit within 1.5s by an additional 10% / 15% / 20%. Chains while you are being focused. |
| B2 | **Deflect** | Notable | 2 | A successful Block returns 25% of the *prevented* damage to the attacker if it is within 15m. Prevented, not dealt — it cannot be farmed by tanking a hit you would have survived anyway. |
| B3 | **Read** | Minor ×3 | 3 | **Grants nothing yet.** Increases the Parry window by 0.04s / 0.08s / 0.12s. Purchasable before B4, at which point it is inert — intentional, so a player planning toward Parry can pre-invest. |
| B4 | **PARRY** | Notable | 2 | **VERB GRANT.** Unlocks Parry: a timed defensive input with a base 0.18s window. A successful Parry fully negates the incoming hit, staggers the attacker for 1.2s, and refunds its own cost. Parry is the *only* verb this constellation grants and the only defensive verb in the game. |
| B5 | **Weight** | Minor ×3 | 3 | Your Armour is not reduced by armour-shredding enemy effects below 60% / 75% / 90% of its value. A floor, not a bonus. |
| B6 | **Unyielding** | Notable | 2 | You cannot be staggered or knocked back while your shield is intact. Removes a category of damage-taken interruption rather than reducing a number. |
| B7 | **Link — Riposte** | Link | 1 | Connects A ↔ B. A successful Block within 0.5s of a failed Parry re-opens the Parry window once. |
| B8 | **Link — Plate** | Link | 1 | Connects B ↔ C. A successful Parry grants 20 Armour for 6s, stacking to 5. |
| B9 | **Guard Doctrine** | Convergence | 3 | Requires two lanes. Parrying refunds the full stamina cost of your last three defensive actions. `[STAMINA-DEP]` — under the passive model this becomes: parrying grants a guaranteed Block on your next two incoming hits. |
| **B10** | **AEGIS** | **Keystone** | **5** | **Rewrite:** your Block no longer rolls. It becomes a guaranteed frontal damage reduction equal to **half** its total rolled chance-weighted value, applied to every frontal hit. In exchange, your Dodge chance is set to **0** from all sources. |

**Why the keystone is not universally superior:** halving the value in exchange for certainty is a net *loss* of expected mitigation. It buys predictability, which is worth a great deal to a player who dies to spikes and nothing at all to a player who never gets hit. Zeroing Dodge makes Bulwark and Kinesis genuinely exclusive at the keystone tier — you cannot hold AEGIS and SLIPSTREAM.

**Balance notes — the invulnerability loop.** §7.10 risk 4 names parry refunds, dodge refunds, leech, overshields, and block combining into permanent safety. Guards applied here:
- Parry refunds its own cost only (B4). It does not generate stamina.
- Guard Doctrine's refund is bounded to three actions and gated behind a 3-point Convergence with a two-lane prerequisite.
- Deflect scales on *prevented* damage, so it cannot be farmed against trivial hits.
- AEGIS zeroes Dodge, which structurally forbids stacking the two defensive keystones.
- No node in Bulwark grants health, shield capacity, or regeneration. Those are affix-layer quantities (§3.8) and duplicating them here would be the affix layer's job.

---

## 8. KINESIS

**Theme:** you are hardest to hit when you are hardest to predict. Kinesis is deliberately the *least* raw-speed constellation in the document — Master Sheet §7.10 risk 2 forbids it from making non-Swift characters the best movers, and risk 7 forbids balancing encounters around it. So Kinesis owns **evasion quality and aerial capability**, and hands raw traversal speed to Swift/Kinetic and to boots affixes.

**Owns:** dodge quality, movement efficiency, slide handling, aerial (§7.6). **Does not grant** dodge, slide, dash, or wall ride.

**Lanes:** A — Evasion · B — Aerial · C — Slide

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| K0 | **Light Footing** | Gateway | 1 | Your Dodge chance is not reduced while airborne or sliding. (Baseline applies a penalty to evasion outside neutral stance.) |
| K1 | **Reflex** | Minor ×3 | 3 | After a successful Dodge, your Dodge chance against the next hit within 1.2s is increased by 10% / 18% / 25% of its own value. A **More** multiplier on a conditional window, not a flat addition to the affix bucket. |
| K2 | **Phantom Step** | Notable | 2 | A successful Dodge grants 0.25s of full i-frames. **I-frame duration is tree-only** (§3.8, Layer-Ownership) — this is the only node in the game that grants it, and no affix may. Internal cooldown 2.0s so it cannot be chained through a burst. |
| K3 | **Loft** | Minor ×3 | 3 | **Grants nothing yet.** Air jump preserves 10% / 20% / 30% more horizontal speed. Inert until K4, deliberately, mirroring Bulwark's Read node. |
| K4 | **AIR JUMP** | Notable | 2 | **VERB GRANT.** Unlocks a single mid-air jump, refreshed on landing, on wall contact, and on a successful Dodge. Air jump is the *only* verb this constellation grants. Per §3.3, "Additional Air Jump" is explicitly not an affix; `Air Jump Speed Retention %` on gear scales it for players who own it. |
| K5 | **Carve** | Minor ×3 | 3 | You may steer a slide 15° / 25° / 35° further from your entry vector without cancelling it. Control, not speed — §5.4 forbids self-acceleration beyond sprint and forbids wall riding generating speed. |
| K6 | **Slipstream** | Notable | 2 | Ending a slide by jumping preserves the slide's full speed into the jump instead of the standard reduction. Preserves; does not generate. Explicitly does **not** remove the combat tradeoffs of sliding (reduced accuracy, fixed camera height) — Progression-Architecture asks for exactly this restraint. |
| K7 | **Link — Weave** | Link | 1 | Connects A ↔ B. A successful Dodge while airborne refreshes your air jump immediately rather than on landing. |
| K8 | **Link — Chain** | Link | 1 | Connects B ↔ C. Landing from an air jump directly into a slide skips the slide's minimum-speed requirement. |
| K9 | **Kinetic Doctrine** | Convergence | 3 | Requires two lanes. Enemy attacks that miss you due to Dodge do not break your slide, your wall ride, or your air jump refresh state. Evasion stops costing tempo. |
| **K10** | **SLIPSTREAM** | **Keystone** | **5** | **Rewrite:** your Dodge chance is doubled while airborne or sliding, and a successful Dodge in either state refunds your air jump. While grounded and moving below sprint speed, your Dodge chance is **0**. |

**Why the keystone is not universally superior:** it deletes evasion for any player who fights from cover, from a stance, or from a stationary firing position — which includes most of Precision and all of Bulwark. It is a build tax that pays only if you are already committed to constant motion. It also cannot coexist with AEGIS, which zeroes Dodge outright.

**Balance notes — identity theft.** Kinesis contains no raw movement speed, no dash cooldown reduction, no dash charges, no wall-ride duration. All four exist as affixes (§3.3) or belong to Swift/Kinetic. What Kinesis owns that neither can produce: i-frames (K2), the air jump verb (K4), slide steering (K5), and evasion-preserves-state (K9). None of those is expressible as a percentage on an item, which is the test in Layer-Ownership.

**Movement tax check (§7.10 risk 7).** Every Kinesis node improves an action the player already has. A character with zero Kinesis points can still dodge, slide, dash, and wall ride. Only air jump is gated, and no encounter may require it — that is a level-design constraint that should be written into the arena checklist, not assumed.

---

## 9. Cross-constellation interaction matrix

Which pairs a ~65-point character can actually complete (two keystones maximum):

| Pair | Verdict | Note |
|---|---|---|
| Precision + Affliction | Strong | TERMINAL's detonation on weak-point hit is Precision's exact output. Intended flagship pair. Watch banked-damage burst against bosses. |
| Precision + Volley | Weak | FIXATION wants one target; PERPETUAL wants sustained multi-target. Deliberately anti-synergistic. |
| Volley + Affliction | Strong | Highest add-clear in the game. **Gated entirely by the proc-coefficient-0 rule in §4.** Without it, this pair is the multiplicative explosion. |
| Volley + Elements | Blocked / dangerous | Same recursion risk one layer worse. Do not enable until proc coefficients and the reaction matrix both exist. |
| Bulwark + Kinesis | **Impossible at keystone tier** | AEGIS zeroes Dodge; SLIPSTREAM is a Dodge keystone. A player may take one keystone and splash the other constellation's lanes. Intended. |
| Bulwark + Precision | Strong | Stationary, certain, single-target. The "gun platform" build. |
| Kinesis + Volley | Strong | Airborne spray. Pairs with the `Accuracy While Airborne` marquee affix (§3.4). |
| Elements + anything | BLOCKED | — |

**Mandatory-cornerstone audit (§7.10 risk 3).** Each keystone imposes a real, named loss: FIXATION loses all off-target crit; PERPETUAL loses burst and all on-reload effects; TERMINAL loses all pressure damage; RESONANCE loses element choice; AEGIS loses half its mitigation value and all Dodge; SLIPSTREAM loses grounded evasion. No keystone is a strict upgrade over not taking it.

---

## 10. Vertical-slice subset

Per CONTEXT.md and §7.9: the slice ships approximately **15 skill nodes** with a **Core Point cap of 10** on a compressed curve. This is a pipeline test, not a balance statement.

**Selection principle:** prove one of each *kind* of node — Gateway, ranked Minor, rule-rewriting Notable, verb grant, and prerequisite gating — across an offensive, a defensive, and a mobility constellation, exactly as Progression-Architecture's prototyping order step 3 asks. Elements contributes **zero** nodes; it is blocked and nothing about it can be validated.

### 10.1 The fifteen

| # | Node | Constellation | Cost | Ranks | Proves |
|---|---|---|---|---|---|
| 1 | Sightline | Precision | 1 | 1 | Gateway; passive GE grant |
| 2 | Fixate | Precision | 1/rank | 3 | Ranked Minor; **More** multiplier bucket; per-target streak state |
| 3 | Tunnel Vision | Precision | 2 | 1 | Notable with a prerequisite; rule change on existing state |
| 4 | Trigger Discipline | Volley | 1 | 1 | Gateway in a second tree; recoil-system hook |
| 5 | Cyclic | Volley | 1/rank | 3 | Conditional ramp with decay; timer-driven GE |
| 6 | Last Round | Volley | 2 | 1 | Multishot hook; **proc coefficient 0 enforcement** |
| 7 | Open Wound | Affliction | 1 | 1 | Gateway; forces status application ignoring chance |
| 8 | Deepen | Affliction | 1/rank | 3 | Stack-cap modification on `UBreakerStatusComponent` |
| 9 | Set Stance | Bulwark | 1 | 1 | Gateway; block arc modification |
| 10 | Read | Bulwark | 1/rank | 3 | **Inert-until-prerequisite node** — validates a node that grants nothing yet |
| 11 | **PARRY** | Bulwark | 2 | 1 | **VERB GRANT.** Ability grant + input binding + stagger event |
| 12 | Light Footing | Kinesis | 1 | 1 | Gateway; conditional dodge modifier |
| 13 | Phantom Step | Kinesis | 2 | 1 | **I-frame grant with internal cooldown** — the tree-only quantity |
| 14 | Loft | Kinesis | 1/rank | 3 | Second inert-until-prerequisite node; scales a verb the player may not own |
| 15 | **AIR JUMP** | Kinesis | 2 | 1 | **VERB GRANT.** Movement-component verb + refresh conditions |

**Total purchasable:** 24 points across 15 nodes. **Slice cap: 10.** A slice player can buy roughly two Gateways, one full Minor, one Notable, and one verb grant — enough to feel a decision and nowhere near enough to buy everything. That gap is the point.

### 10.2 What the slice deliberately omits

- **All six keystones.** They gate on 18 points spent; the cap is 10. Shipping a keystone in the slice would require breaking its own gate.
- **All Convergence nodes.** Two-lane prerequisites cannot be satisfied under a 10-point cap.
- **All Link nodes.** Cross-lane adjacency is untestable when no lane completes.
- **Every Elements node.** BLOCKED.

### 10.3 Slice acceptance criteria

1. All 15 nodes load from Data Assets. No node ID, cost, rank count, or effect magnitude is hardcoded in C++ (§7.9).
2. The 10-point cap comes from the progression curve Data Asset and can be changed to 65 without a recompile.
3. Allocation is rejected when: points are insufficient; a prerequisite is unmet; a rank cap is exceeded; or a node's constellation-spend gate is unmet.
4. Deallocation at a Forge refunds every point and correctly revokes granted abilities, effects, and tags — including the two verbs.
5. **Read at rank 3 with Parry unpurchased produces no observable effect and no error.** Likewise Loft without Air Jump. This is the inert-node test and it is the one most likely to be silently broken.
6. Parry and Air Jump survive a save/load round trip: the saved build stores IDs and ranks only, and the verbs are re-granted on load (§7.9, CONTEXT.md save format).
7. Fixate's **More** multiplier lands in the More bucket, not the additive Increased bucket. Verifiable in the damage log: a character with +40% Increased weapon damage and 6 Fixate stacks deals `base × 1.40 × 1.18`, not `base × 1.58`.
8. Last Round's extra projectiles deal full damage and apply **zero** statuses. Verifiable against a target dummy with a 100% Bleed Chance weapon: one bleed application per trigger pull, not one per pellet.
9. Phantom Step's i-frames respect their 2.0s internal cooldown under sustained fire from three gym enemies.
10. Air Jump refreshes on landing, on wall contact, and on a successful Dodge — and on nothing else. Specifically it must not refresh on dash.
11. No slice node's effect is reproducible by any affix in the slice affix pool (§3.14). Manual audit, recorded in the slice sign-off.
12. Respec, allocate, respec, reload — final state matches a fresh allocation of the same build exactly.

---

## 11. Summary of labels applied

**EXTENDS**
- Multishot-generated projectiles carry proc coefficient 0 for all status/on-hit/node triggers, coefficient 1 for damage. Ricochets carry 0.5 and cannot chain. (§4 Balance notes — extends §3.5 and §7.10 risk 1.)
- Affliction transfer ancestry is capped at depth 2 with a normalized payload. (§5, A9.)
- Elements reactions cap at chain depth 2 with proc coefficient 0 on the second reaction. (§6, E9.)
- Per-constellation total of 26 points, producing a hard maximum of two keystones per character. (§2.2 — extends §7.4's "two full plus one partial" into a specific number.)
- Bulwark's Read and Kinesis's Loft are purchasable while inert. (§7, §8 — a node pattern the master sheet does not describe.)

**CONFLICT**
- Block/Dodge model: passive chance (this document, §3.8 affix lines) vs. stamina-spending player action (§7.7, Progression-Architecture, shipped `UBreakerCombatComponent`). Unresolved. (§7 preamble.)
- Elements RESONANCE overlaps Caster/Multispell's stated fantasy. (§6.)

**BLOCKED**
- The entire Elements constellation, all 11 nodes, four dependency tags. (§6.)
- Any Bulwark node touching the stamina pool, pending the stamina cap decision (§3.9, §7.7).

---

## 12. OPEN QUESTIONS

1. **Is Block/Dodge a passive chance or a stamina-spending action?** This is the highest-priority question in the document. Bulwark and Kinesis are written for the passive model; §7.7, Progression-Architecture, and the shipped combat component implement the action model. AEGIS and SLIPSTREAM are both meaningless under the wrong one. Nothing in either constellation can be authored as a Data Asset until this resolves.

2. **Is 26 points per constellation the right size?** It is derived from "two full plus one partial" and produces a hard cap of two keystones per character. If playtesting says two keystones is too many or too few, the correct lever is constellation size, not the point schedule — but §7.4 requires re-validating the ratio either way.

3. **Elemental resistance: one stat or per-element?** (§6.7) This determines whether Penetrance and Insulator's Bane are even coherent nodes. Per-element makes Insulator's Bane meaningful and RESONANCE's mono-element restriction genuinely costly; a single stat makes both nearly inert.

4. **Where does the resistance step sit in the §6.1 damage order?** Proposed between armour mitigation (4) and shield routing (5), but not decided. Affects whether elemental DoTs can be made shield-bypassing later.

5. **Does RESONANCE need an explicit Multispell exemption, or is mono-element restriction sufficient separation?** (§7.10 risk 2.)

6. **Are lanes freely mixed, or mutually exclusive at the Convergence tier?** (§7.11) This document assumes freely mixed with Links as the cost of mixing. Mutual exclusivity would be a different and more opinionated tree.

7. **What input does Parry use, and how does it relate to Block's input?** Progression-Architecture asks the same question for Dodge and rejects double-tap. If Block is passive, Parry needs a dedicated key of its own — a defensive input on a character with no other defensive input is unusual and may need a design pass of its own.

8. **Does Fixate's More multiplier snapshot into DoTs?** §6.4 says DoTs snapshot source power at application. A 6-stack Fixate bleed applied at maximum streak would tick at 1.19x for its full life, including after the streak breaks. Consistent with the contract, possibly not intended.

9. **Can TERMINAL's banked damage be capped against bosses without disabling the build?** §7.10 risk 5. A single delivered instance is exactly the shape a boss cap is designed to catch.

10. **What are the ~15 world-content sources for non-level Core Points?** (§7.11) If they are all endgame-gated, the two-full-plus-one-partial shape does not exist until well after level 50 and the third constellation is a post-campaign feature rather than a build decision.

11. **Should keystones be respec-free like everything else?** §7.8 says Core Point respec is free unless playtesting establishes a reason for friction. A keystone that rewrites a rule is the most likely place for that reason to appear.

12. **Does Slipstream's preserved slide speed cross the chained-slide threshold named in §5.4?** It preserves rather than generates, so it should not — but the threshold itself is undefined, which is also an open item in §3.15.
