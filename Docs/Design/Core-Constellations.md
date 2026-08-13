# Core Constellations — six universal trees authored, FIVE shipping

Status: design pass 1, propagated against `Docs/Design/Decisions.md` (O1–O23, law). Values are placeholder and must be re-anchored after the Playtest Gym TTK pass, consistent with Affixes §3 ("ALL VALUES ARE PLACEHOLDER"). **Per O2, no value in this document may be re-authored until wave-mode instrumentation reports.** Nodes whose cost basis was invalidated by O1 are tagged **NEEDS-RECOST [O1/O2]** and carry their frozen original values until then; **per O20 the subset whose *trigger* O1 deleted is split into a separate REDESIGN [O20] bucket** (§11) — those cannot be closed by any number. **O18's TTK/TTD seed targets are the validation reference for that eventual tuning pass (§10.3, criterion 16).**

Scope: the six universal Core Tree constellations spent with Core Points — Precision, Volley, Affliction, Elements, Bulwark, Kinesis. Class trees and affixes are out of scope and are governed by `Docs/Layer-Ownership.md`.

> **PLAN OF RECORD.** The vertical slice and the first release ship **FIVE** constellations — Precision, Volley, Affliction, Bulwark, Kinesis. **Elements is authored in parallel against the three-element model (O5, renamed by O19: Rift, Entropy, Void)** and slots in when the resistance step lands in the §6.1 damage order. Elements is a designed-but-unshipped sixth, not a cut one.

---

## 1. Governing constraints

These are inherited, not proposed. Nothing below may violate them.

| Constraint | Source |
|---|---|
| Level cap 50, hard stop; all endgame power from gear | Master Sheet §7.1 |
| ~65 Core Points = two full constellations plus one partial | §7.2, §7.4 |
| Air jump (Kinesis) and Parry (Bulwark) are the ONLY tree-granted verbs | §5.2, §7.6 |
| Dash, slide, wall ride, block, dodge are base kit — trees improve, never unlock | §5.2 |
| **No stamina pool exists.** Block and dodge are passive chance layers; Parry has its own short cooldown and is the only defensive input | **O1** |
| **Three elements: Rift, Entropy, Void.** Three reaction pairs, three resist stats | **O5 (renamed by O19)** |
| **Max one More multiplier per constellation**, on Convergence/Keystone only; build-wide hard cap 3 | **O3 (+ extension, §2.4)** |
| Crit is the only multiplier of its kind; no parallel multiplier via nodes | §6.3 |
| Flat sums → one additive Increased bucket → More multipliers reserved for trees/Anomalous | Item-Foundation locked rule |
| More multipliers compose as an **unordered product**; 2–3 per build, hard cap 3 | **O3** |
| **Aberrant signature affixes may NOT author a More.** Anomalous is the only item-layer More source | **O3 + O11 extension** |
| A node that reads as a flat percentage is doing the affix layer's job | §2.6, §7.6 |
| Core nodes must not outperform the class branch they overlap | §7.10 risk 2 |
| No cornerstone may be universally superior | §7.10 risk 3 |
| No grapple, no tether | §5.1 |
| Elements is BLOCKED pending a resistance model | §6.1, §3.2 |

**Design consequence taken seriously:** with only two verbs left to grant, every node here is either (a) a *conditional* quality change, (b) a rule rewrite, or (c) one of the two verb grants. Where a node does carry a percentage, it is a **More** multiplier or a conditional gate that gear cannot produce — never a duplicate of an existing affix line. **Under the O3 extension (§2.4) the "or a More multiplier" half of that sentence is now restricted to one node per constellation, at Convergence or Keystone tier.** Three existing nodes were out of compliance; **O21 promoted all three to their constellation's Convergence tier and this pass executes it (§2.4).**

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

13 points into a third constellation buys Gateway + two complete lanes + one link (1+5+5+1 = 12) with one point spare, or Gateway + one lane + Convergence prerequisites-short. It **cannot** reach a third keystone (18-point gate + 5 cost = 23). That is the intended shape: **a character has at most two keystones, ever.** This is the single most important structural decision in the document and it is what keeps the constellation choice real (six-way as authored, **five-way at ship** — see §2.2a).

Alternative shapes rejected:
- 20-point constellations → 65 buys three full trees, three keystones, and the choice collapses.
- 32-point constellations → 65 buys two full and almost nothing else; the third constellation stops being a decision.

**If node costs change, re-run this table.** Master Sheet §7.4 requires it.

#### 2.2a Re-validation against FIVE live constellations (O5 plan of record)

Arithmetic only — no value is authored or changed here. Per-constellation size is unchanged at 26; only the number of live constellations changes from 6 to 5.

| Quantity | Six live | Five live |
|---|---|---|
| Total points on offer | 6 × 26 = **156** | 5 × 26 = **130** |
| Budget | 65 | 65 |
| Share of the tree a character can buy | 65 / 156 = **41.7%** | 65 / 130 = **50.0%** |
| 26 + 26 + 13 = 65 | holds | holds |
| Third-keystone cost (18-point gate + 5) = 23 > 13 | unreachable | unreachable |

**Structural conclusion HOLDS: a character has at most two keystones, ever.** The two-keystone cap is a function of constellation *size* (26) and the keystone gate (18 + 5 = 23 > 13 remaining), not of how many constellations exist. Removing one constellation from the shipping set changes neither term.

**What did change — one item, flagged not resolved:** the budget now buys exactly **half** the live tree rather than ~42% of it. "Two full plus one partial" is still literally true, but with five trees the untouched remainder falls from three-and-a-partial to two-and-a-partial, so the *opportunity cost* of each choice is lower and the "six-way choice" language throughout this document is now a **five-way choice** at ship. Whether 50% is an acceptable share is an owner call and a candidate lever for Open Question 2; it is not decided here.

### 2.3 Adjacency rules (all constellations)

1. Gateway has no prerequisite beyond owning ≥1 Core Point.
2. A lane's Notable requires that lane's Minor at rank 3.
3. Links connect adjacent lanes and require either neighbouring Minor at rank 1. They exist so a player can reach a second lane's Notable without fully committing to the first lane's Minor — this is the only way to build a "two-notable, no-keystone" splash.
4. Convergence requires **two** lanes complete (Minor r3 + Notable).
5. Keystone requires **18 points spent in this constellation**, which in practice means Gateway + all three lanes + Convergence (1+15+3 = 19) or Gateway + three lanes + both links (1+15+2 = 18). Two legal routes; the second is cheaper but skips Convergence.
6. Refunds are free at a Forge (§7.8). Deallocation must validate downstream dependents — the progression subsystem already stores IDs and ranks, so a respec is a full rebuild, not a decrement.

### 2.4 More-multiplier rule (O3, extended)

O3 is law: **More multipliers multiply as an unordered product**, a build may hold **2–3 total (hard cap 3)**, and trees may author them only on branch keystones and constellation Convergence/Keystone nodes.

Extension applied throughout this document:

1. **At most ONE More multiplier per constellation**, and it must sit on that constellation's **Convergence or Keystone** node. Minors, Notables, Gateways, and Links may not author a More.
2. **Composition is an unordered product.** `base × More₁ × More₂ × More₃` — no priority, no ordering rule, no bucket-of-Mores summed first. Order-independence is the property that must be true in the damage log; if it is not, the aggregation is wrong.
3. **The two-keystone cap (§2.2) plus one-More-per-constellation means a character can hold at most 2 tree-sourced Mores**, leaving exactly one slot for the item layer under O3's hard cap of 3.
4. **Item layer: `Anomalous` is the ONLY item-layer source of a More.** Per O11 an Aberrant carries 1–2 unique modifier affixes that help define builds — **those signature affixes may NOT author a More multiplier.** With up to 3 Aberrants equipped globally, allowing signature Mores would put 3–6 Mores on the item layer alone and break O3's hard cap outright. Aberrant signature affixes express identity through rule changes, conditional gates, and behavioural rewrites — the same grammar the trees use.

**Compliance status — RESOLVED by O21 (promotion, not cut).** Three authored nodes predated this extension and violated rule 1. O21 rules that they are **promoted**, and this pass executes it:

| Node | Was | Now | Cost | Displaced |
|---|---|---|---|---|
| **P3 Fixate** (Precision) | Lane B Minor ×3 | **Convergence** | 3 → 3 (unchanged) | P9 Marksman's Ledger → Lane B Minor ×3, 3 → 3 |
| **A4 Necrosis** (Affliction) | Lane B Notable | **Convergence** | 2 → 3 **COST-MOVED [O21]** | A9 Epidemiology → Lane B Notable, 3 → 2 **COST-MOVED [O21]** |
| **K1 Reflex** (Kinesis) | Lane A Minor ×3 | **Convergence** | 3 → 3 (unchanged) | K9 Kinetic Doctrine → Lane A Minor ×3, 3 → 3 |

Each constellation's Convergence tier was **occupied**, not empty — the §2.4/§9 "compliant More slot is currently unused" note meant unused *for a More*, not vacant. So each promotion is a **minimal swap** within the same lane: the promoted node takes the Convergence slot, the sitting Convergence node takes the vacated lane slot. All three constellations still hold **11 nodes / 26 points** (see the arithmetic line in §3, §5, §8). Only Affliction touched costs, and its two moves cancel exactly (+1/−1); both use existing grammar costs. **No new numeric value is introduced anywhere in this pass.** Effect text is verbatim in all six affected nodes.

**GAP [O21] carried, not resolved:** three swaps place a 3-rank magnitude ladder into a 1-rank tier (P3, K1) or a 1-rank rule change into a 3-rank Minor slot (P9, K9, and A9's dropped two-lane prerequisite). Reconciling rank counts would author values; O2 forbids it. Flagged on each node.

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
| P3 | **Fixate** | **Convergence** | **3** | Consecutive hits on the same target grant a stacking **More** damage multiplier of 1% / 2% / 3% per stack, max 6 stacks. Resets on target swap or 2.0s without a hit. **PROMOTED [O21]** — moved from Lane B Minor to Precision's Convergence tier; it is now Precision's single compliant More (§2.4 rule 1). Effect text verbatim; tier and position changed, cost unchanged (Minor 3 → Convergence 3). Requires two lanes, per §2.3 rule 4. **GAP [O21]:** the 1/2/3% ladder is a three-rank magnitude and Convergence is a 1-rank class — the ladder is carried verbatim and frozen under O2; collapsing it to a single magnitude would author a value and is not done here. |
| P4 | **Tunnel Vision** | Notable | 2 | Your streak no longer resets on a miss — only on a target swap or timeout. Streak state is per-attacker/per-target with an explicit timeout (see Progression-Architecture: this needs real state, not a counter on the weapon). |
| P5 | **Coup** | Minor ×3 | 3 | Enemies below 15% / 20% / 25% health take critical hits from you as though your Critical Multiplier were at its snapshot maximum. No new multiplier is introduced — it forces the existing roll high. |
| P6 | **Mercy Rule** | Notable | 2 | Killing an enemy below the Coup threshold refunds the shot's ammunition and does not break your Fixate streak. |
| P7 | **Link — Steady** | Link | 1 | Connects Lane A ↔ B. Weak-point hits add 2 Fixate stacks instead of 1. |
| P8 | **Link — Finisher** | Link | 1 | Connects Lane B ↔ C. Reaching 6 Fixate stacks marks the target as Coup-eligible regardless of its health. |
| P9 | **Marksman's Ledger** | **Minor ×3** | **3** | **MOVED [O21]** — displaced into Lane B's Minor slot by Fixate's promotion; effect verbatim, cost unchanged (Convergence 3 → Minor 3). Your critical hits against a target you have hit at least 3 consecutive times bypass 50% of that target's Armour. Bypass, not reduction — it does not stack with class armour-shred into negative armour. **GAP [O21]:** a single-rank rule change now sits in a 3-rank Minor slot; it has no rank ladder and none is invented here (O2). |
| **P10** | **FIXATION** | **Keystone** | **5** | **Rewrite:** after 6 consecutive hits on a single target, your Critical Chance against *that* target is treated as 100%. Against **every other target**, your Critical Chance is treated as **0%** until the streak ends. Streak ends on target swap or 2.0s without a hit. |

**Why the keystone is not universally superior:** it deletes crit entirely in add-clear, trash waves, and any multi-target fight — which is most of the game's minute-to-minute. It is a boss/elite keystone. A Volley or Affliction build would be actively worse for taking it. That is the intended shape of every keystone here.

**Constellation arithmetic after the O21 swap:** 11 nodes, 1+3+2+3+2+3+2+1+1+3+5 = **26 points**. Unchanged — the swap was cost-neutral (3↔3), so no node here is COST-MOVED.

**Balance notes.** Fixate is a **More** multiplier. It is legal against the locked *aggregation* rule (More reserved for trees/Anomalous) and, **since PROMOTED [O21] to Convergence, is now compliant with the O3 extension in §2.4** — it is Precision's one More, on the one tier permitted to carry it. At 6 stacks × 3% it is only 1.19x — deliberately modest, because it multiplies against everything. Coup must read the *snapshot* critical result for DoTs (§6.4) or it will retroactively upgrade running bleeds.

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
| A4 | **Necrosis** | **Convergence** | **3** | A target at maximum Bleed or Poison stacks takes 15% **More** damage from your direct weapon hits. The bridge that makes Affliction worth taking alongside a shooting constellation. **PROMOTED [O21]** — moved from Lane B Notable to Affliction's Convergence tier; now Affliction's single compliant More (§2.4 rule 1). Effect text verbatim. **COST-MOVED [O21]:** 2 → 3, the existing Notable→Convergence grammar cost; no new number invented. Requires two lanes, per §2.3 rule 4. |
| A5 | **Vector** | Minor ×3 | 3 | When an afflicted enemy dies, its remaining ailment duration transfers to the nearest enemy within 4m / 7m / 10m at 50% magnitude. One hop. |
| A6 | **Contagion** | Notable | 2 | Vector's transfer no longer requires death — it occurs on a 4s interval from any afflicted target. Ancestry depth is capped at **2**; a second-generation transfer cannot transfer again. |
| A7 | **Link — Rot** | Link | 1 | Connects A ↔ B. Poison stacks no longer expire individually; the whole application expires at once. |
| A8 | **Link — Carrier** | Link | 1 | Connects B ↔ C. Transferred ailments inherit your Slow Bleed tick rate rather than the source's. |
| A9 | **Epidemiology** | **Notable** | **2** | **MOVED [O21]** — displaced into Lane B's Notable slot by Necrosis's promotion; effect verbatim, two-lane prerequisite dropped with the tier (it now requires Lane B's Minor at rank 3, per §2.3 rule 2). **COST-MOVED [O21]:** 3 → 2, the existing Convergence→Notable grammar cost. Transferred ailments copy a **normalized status payload** — fixed magnitude derived from the original snapshot, no re-roll, no fresh kill-proc. Explicitly *not* a fresh application. **GAP [O21]:** A9 is the sole guard against the Contagion recursion failure mode (see Balance notes) and is now cheaper and reachable without completing two lanes; whether that weakens the mitigation set is an owner call, flagged not resolved. |
| **A10** | **TERMINAL** | **Keystone** | **5** | **Rewrite:** your physical DoTs no longer deal damage over time. Each application banks its full remaining damage silently; the entire banked total is delivered as a single instance when the target dies, when the ailment expires, or when you land a weak-point hit on that target. Banked damage cannot critically strike (the crit was already resolved at snapshot) and does not benefit from Tick Frequency. |

**Why the keystone is not universally superior:** it removes all pressure damage. Nothing dies while you are reloading, and against a target you cannot finish, you have dealt zero damage. It is a burst-detonation build, not a sustain build. It also makes Contagion far weaker (there is no tick to spread), so it competes with the constellation's own Lane C.

**Constellation arithmetic after the O21 swap:** 11 nodes, 1+3+2+3+3+3+2+1+1+2+5 = **26 points**. Holds — the two COST-MOVED nodes (A4 +1, A9 −1) cancel exactly.

**Balance notes.** A9 is the guard against Progression-Architecture's stated Contagion failure mode: recursive spreads treated as fresh applications. Ancestry depth 2 plus normalized payload plus proc coefficient 0 on transfers is the full mitigation set; all three are required, not alternatives.

Slow Bleed intentionally *conflicts* with the Tick Frequency affix rather than multiplying it — §3.7 names Tick Frequency "the most dangerous affix in the game," and a tree node that stacks with it would compound the problem.

---

## 6. ELEMENTS — **THREE ELEMENTS: RIFT · ENTROPY · VOID** (O5 + O19) — **BLOCKED on the resistance step**

> **O5 IS LAW; O19 RENAMES.** The elements are **RIFT, ENTROPY, and VOID** — not fire/ice/lightning. **O19: the element formerly called "Time" is named Entropy; every reference in this document, including reaction-pair names, is swept accordingly.** All Ignite/Chill/Shock naming in the affix tables and in earlier drafts of this constellation is placeholder and is re-flavored onto Rift/Entropy/Void when the resistance model lands. `EBreakerDamageFamily::Elemental` stays the pipeline family; the three-way split arrives with the resistance model. Note Decisions.md's O5 implementation note still writes "the Rift/Time/Void split" — that is pre-O19 wording in an append-only ledger and O19 supersedes it; **GAP [O19]:** any code identifier authored as `Time` must land as `Entropy`.
>
> **Structural consequence of three elements rather than four:**
> - **THREE reaction pairs**, not six. The unordered pairs are **Rift×Entropy, Rift×Void, Entropy×Void** (O19 naming). A four-element set would have given six pairs; a three-element set gives three. The `UElementReactionDataAsset` matrix is a 3×3 with three off-diagonal entries, and the whole reaction table is small enough to be hand-authored and hand-balanced — which is a design advantage, not a shortfall.
> - **THREE resist stats**, three affix lines, three UI glyphs. One resist attribute per element on the attribute set, one suffix family per element in the affix pool, one glyph per element in the damage-number and tooltip vocabulary. Enemy resistance profiles are three numbers.
> - E9 **Reaction Chain** requires "a third element present" — with exactly three elements that means *all three* must be on the target, which is the maximum possible state rather than one of several. This tightens E9 from a common case to the ceiling case. Flagged, not re-authored.
>
> **BLOCKED (unchanged).** Elements still cannot ship until an elemental resistance step exists in the damage pipeline. Master Sheet §6.1: "there is no elemental resistance step. Combat-Foundation has Armor only." §3.2 blocks Elemental Damage Reduction; §3.7 blocks the three status effects under whatever names they take. O5 fixes *which* elements exist; it does not build the pipeline.
>
> **PLAN OF RECORD.** The slice and the first release ship **five** constellations without Elements. **Elements is authored in parallel against the three-element model** and slots in when the resistance step lands. Authoring may proceed on paper; **no node may be authored as a Data Asset until its dependency tag is cleared.**
>
> **[REMAP — O5]** tags below mark nodes whose identity depended on a fourth element or on the old element names. No new mechanics are invented for them here.

**Dependency tags used:**
- `[ELEM-RES]` — requires the elemental resistance model (one stat vs per-element is itself open, §6.7).
- `[ELEM-BUILDUP]` — requires a threshold/buildup track on the status component (the component records buildup already per Progression-Architecture, but no elemental status writes to it).
- `[ELEM-MATRIX]` — requires `UElementReactionDataAsset` and a reaction resolution order.
- `[ELEM-PIPE]` — requires a resistance step inserted into the §6.1 damage resolution order between armour mitigation and shield routing.

**Theme:** Elements does not deal elemental damage — it *conducts* it. That theme survives O5 intact: conduction, buildup, and thresholds are element-count-agnostic. What O5 changes is the *size of the reaction surface* — three pairs to conduct between, not six. Every node is about buildup, thresholds, and what happens when two elements meet. This is deliberate: it keeps Elements from being "the damage constellation for Casters" and preserves Multispell's identity (§7.10 risk 2).

**Lanes:** A — Buildup · B — Reaction · C — Conduction

| # | Node | Class | Cost | Effect | Tags |
|---|---|---|---|---|---|
| E0 | **Conductive** | Gateway | 1 | Elemental buildup on a target decays 50% slower. | `[ELEM-BUILDUP]` |
| E1 | **Charge Up** | Minor ×3 | 3 | Elemental buildup you apply is increased 10% / 20% / 30%. Buildup, not damage — the one legal percentage here because buildup has no affix line. | `[ELEM-BUILDUP]` |
| E2 | **Threshold** | Notable | 2 | Reaching a status threshold no longer consumes the buildup bar; it resets to 50% instead of 0. | `[ELEM-BUILDUP]` |
| E3 | **Catalyst** | Minor ×3 | 3 | Reactions you trigger have their internal cooldown reduced by 15% / 25% / 35%. | `[ELEM-MATRIX]` |
| E4 | **Second Order** | Notable | 2 | A reaction leaves its *first* element on the target instead of consuming both. Enables deliberate reaction chains without the accidental double-trigger that Progression-Architecture warns against — because the *second* element is always the one consumed, resolution order stays deterministic. | `[ELEM-MATRIX]` **[REMAP — O5]** — "leaves its first element" was written against a larger reaction surface; with three pairs, the retained element has only two possible follow-on partners. Behaviour unchanged, reach narrowed. |
| E5 | **Penetrance** | Minor ×3 | 3 | Your elemental damage ignores 10% / 18% / 25% of the target's elemental resistance. | `[ELEM-RES]` `[ELEM-PIPE]` **[REMAP — O5]** — "elemental resistance" now resolves against one of three named resist stats (Rift/Entropy/Void); whether it penetrates the matching resist only or all three is Open Question 3 and is not decided here. |
| E6 | **Insulator's Bane** | Notable | 2 | Against a target whose resistance to your element is its *highest* resistance, your buildup rate is doubled. Punishes the enemy's strength rather than seeking its weakness — the anti-optimal-target rule. | `[ELEM-RES]` **[REMAP — O5]** — "its *highest* resistance" is now a 1-in-3 condition rather than 1-in-4, so the node fires roughly a third of the time against a random profile instead of a quarter. Frequency changed by O5; no value re-authored (O2). |
| E7 | **Link — Arc** | Link | 1 | Connects A ↔ B. Buildup applied within 1.0s of a reaction is doubled. | `[ELEM-BUILDUP]` `[ELEM-MATRIX]` |
| E8 | **Link — Ground** | Link | 1 | Connects B ↔ C. Reactions apply a flat elemental resistance reduction of 10 for 4s. Flat, not percentage — it cannot drive resistance negative when stacked. | `[ELEM-RES]` `[ELEM-MATRIX]` **[REMAP — O5]** — "a flat elemental resistance reduction" must now name *which* of the three resists it reduces (the reaction's own elements, or all three). Owner choice; not made here. |
| E9 | **Reaction Chain** | Convergence | 3 | Requires two lanes. A reaction can trigger a second reaction on the same target, once, if a third element is present. Hard depth cap of 2. Proc coefficient of the second reaction is 0. | `[ELEM-MATRIX]` **[REMAP — O5]** — with exactly three elements, "if a third element is present" means **all three** are on the target. The node's trigger condition is now the maximum achievable element state rather than a mid-range one; it fires far less often than authored. Structural, not a value change. Also the constellation's **compliant O3 More slot** (§2.4) if a More is ever wanted here. |
| **E10** | **RESONANCE** | **Keystone** | **5** | **Rewrite:** you may carry only one element, chosen at a Forge. All elemental damage you deal converts to that element. In exchange, applying *any* other source's element to a target you have already built up instantly fills that target's buildup to threshold and triggers the reaction at maximum magnitude. **O19 framing:** RESONANCE is the "**masters one**" pole; Multispell is the "**rotates all three**" pole. That opposition — not a restriction clause — is the separation. The rewrite text above is unchanged; no new mechanic is introduced. | `[ELEM-RES]` `[ELEM-MATRIX]` `[ELEM-BUILDUP]` **[REMAP — O5]** — "you may carry only one element" now means one of **three** (Rift, Entropy, Void), giving up two rather than three. The restriction is materially *cheaper* under O5 than as authored. **FLAG [O19]:** the cost argument for this keystone was already weakened by the drop from four elements to three, and the retirement of the mono-element separation clause removes its remaining load-bearing justification — **re-examine E10's cost basis.** Not redesigned here; no value authored (O2). |

**Why the keystone is not universally superior:** **[REMAP — O5] + [FLAG — O19] — this argument is weakened twice over and needs re-examination.** O19 retires the mono-element separation clause (below), which was the same clause this paragraph spends as RESONANCE's cost; with the clause retired the cost argument has to be re-derived rather than restated. Flagged for the owner; not redesigned here. Mono-element is a severe restriction in a game whose enemy families are meant to have differing resistances — but under O5 the player forgoes two elements, not three, and a three-element resist profile has fewer places to hide a hard counter. The claim below is retained as written pending an owner re-read; it is not re-argued here. Mono-element is a restriction in a game whose enemy families are meant to have differing resistances, and RESONANCE's payoff depends on a *second* element arriving from somewhere — a class ability, a weapon, or an ally. Solo, that is a real constraint; solo is the primary balance target.

**CONFLICT — Multispell. RESOLVED by O19.** RESONANCE is close to Caster/Multispell's stated fantasy ("sequencing different elements to create reactions"). The two previously proposed mitigations were:

1. ~~RESONANCE's mono-element restriction is precisely what Multispell does *not* have, so the constellation is a worse version of the class for a Caster and a genuine option for everyone else.~~ **RETIRED [O19]: separation is now "rotates all three" vs "masters one".** The mono-element *restriction clause* is retired as redundant — the separation no longer rests on a restriction one side lacks, but on two opposed fantasies: Multispell **rotates all three** elements; RESONANCE **masters one**. Also note Void Whisperer IS the Void-element specialist (O19) — that coupling is intentional and is not a collision.
2. ~~Multispell gains an explicit exemption node making it the only source that can carry two elements under RESONANCE.~~ Not needed under the O19 framing; no exemption node is authored.

No new mechanics are introduced by this retirement. **FLAG [O19]:** E10's cost argument leaned on the retired clause and must be re-examined (see E10 and §6's not-universally-superior note) — flagged, not redesigned.

**Implementation prerequisite, restated:** §6.1's damage resolution order has seven steps and no resistance step. Elements requires an eighth. Adding it after step 4 (armour mitigation) and before step 5 (shield routing) is the least disruptive insertion point — but that ordering choice is a real decision and is not made here.

---

## 7. BULWARK

**Theme:** the refusal to be moved. Bulwark deepens the universal Block and grants Parry. Its keystone converts randomness into certainty — which is the single most valuable thing you can give an encounter designer.

**Owns:** armour, mitigation, **Parry cooldown economy**, and parry (§7.6). **Does not grant Block** (§7.6). *("Stamina efficiency" removed from this ownership line — O1 deleted the stat it referred to.)*

> **RESOLVED by O1 — the CONFLICT is closed.** The stamina pool is **deleted entirely**. `Stamina`/`MaxStamina` are removed from the attribute set and the combat component. **Block is a passive chance to reduce incoming damage; Dodge is a passive chance to fully evade** — ratified, consistent with the `Block %` and `Dodge %` affix lines in §3.8. Master Sheet §7.7, Progression-Architecture, and the shipped `UBreakerCombatComponent` action model (frontal block stance, dodge negation window with resource refund) are **superseded**; Decisions.md overrides them.
>
> **Parry is the sole exception and the sole defensive input.** It is a timed input on **its own short cooldown** — not a stamina spend, not a shared resource, not a charge pool held in common with block or dodge. Parry's cooldown is Bulwark-local and is the only defensive resource in the game.
>
> **Propagation consequence.** Nodes previously tagged `[STAMINA-DEP]` were costed against a spend-and-refund economy that no longer exists. Each was retagged **NEEDS-RECOST [O1/O2]**; **O20 then moved Bulwark's three (B4, B7, B9) into the REDESIGN [O20] bucket** — new numbers cannot fix a node whose trigger no longer exists. Either way: the node's name and intent are preserved, its effect is re-expressed against the Parry cooldown, and **its numbers are frozen exactly as authored** until O2's instrumentation reports. No replacement value is authored in this pass.

**Refund grammar under O1.** "Refund" no longer means returning stamina. Where a Bulwark node refunded a cost, the only cost left to refund is **Parry cooldown time** — either resetting it, reducing it, or granting a free Parry that does not consume it. Which of those three, and by how much, is a re-cost and an owner choice. Every affected node states the shape and stops.

**Lanes:** A — Guard · B — Parry · C — Armour

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| B0 | **Set Stance** | Gateway | 1 | Your Block chance applies to a 180° frontal arc rather than a 120° one. |
| B1 | **Bracing** | Minor ×3 | 3 | A successful Block reduces the damage of the *next* incoming hit within 1.5s by an additional 10% / 15% / 20%. Chains while you are being focused. |
| B2 | **Deflect** | Notable | 2 | A successful Block returns 25% of the *prevented* damage to the attacker if it is within 15m. Prevented, not dealt — it cannot be farmed by tanking a hit you would have survived anyway. |
| B3 | **Read** | Minor ×3 | 3 | **Grants nothing yet.** Increases the Parry window by 0.04s / 0.08s / 0.12s. Purchasable before B4, at which point it is inert — intentional, so a player planning toward Parry can pre-invest. |
| B4 | **PARRY** | Notable | 2 | **VERB GRANT.** Unlocks Parry: a timed defensive input with a base 0.18s window. A successful Parry fully negates the incoming hit and staggers the attacker for 1.2s. **REDESIGN [O20]** *(was NEEDS-RECOST [O1/O2] — O20 rules that the stamina-built Bulwark nodes are redesign items, not recost items: O1 removed the trigger and new numbers cannot fix them).* Parry now runs on **its own short cooldown** (O1) — no stamina, no shared pool. "Refunds its own cost" re-expresses as: **a successful Parry does not put Parry on cooldown** (a miss does). The cooldown duration itself is unauthored — value frozen/absent under O2. Parry is the *only* verb this constellation grants, the only defensive verb in the game, and under O1 **the only defensive input of any kind**, since block and dodge are now passive. |
| B5 | **Weight** | Minor ×3 | 3 | Your Armour is not reduced by armour-shredding enemy effects below 60% / 75% / 90% of its value. A floor, not a bonus. |
| B6 | **Unyielding** | Notable | 2 | You cannot be staggered or knocked back while your shield is intact. Removes a category of damage-taken interruption rather than reducing a number. |
| B7 | **Link — Riposte** | Link | 1 | Connects A ↔ B. A successful Block within 0.5s of a failed Parry re-opens the Parry window once. **REDESIGN [O20]** *(was NEEDS-RECOST [O1/O2])* — a failed Parry now starts Parry's cooldown (B4), so "re-opens the window" is really "bypasses the cooldown once." Coherent under O1, but its cost basis is the cooldown length, which is unauthored. Block is passive here, so the trigger is a passive proc, not a player action — the node fires on chance, not on skill. Flagged; frozen. |
| B8 | **Link — Plate** | Link | 1 | Connects B ↔ C. A successful Parry grants 20 Armour for 6s, stacking to 5. |
| B9 | **Guard Doctrine** | Convergence | 3 | Requires two lanes. **REDESIGN [O20]** *(was NEEDS-RECOST [O1/O2], originally `[STAMINA-DEP]`)* — O20: the authored mechanism has no subject under O1, so this is owner-led redesign, not a re-cost. The authored effect — "Parrying refunds the full stamina cost of your last three defensive actions" — is void: there is no stamina and there are no other defensive actions. **Intent preserved:** a successful Parry converts into a window of guaranteed defence. **Re-expressed against the passive model:** a successful Parry grants a **guaranteed Block on your next two incoming hits** (Block no longer rolls for those two hits). The count "two" is inherited from the prior passive-model note and is **frozen, not authored** — it and any cooldown interaction are a re-cost pending O2. |
| **B10** | **AEGIS** | **Keystone** | **5** | **Rewrite:** your Block no longer rolls. It becomes a guaranteed frontal damage reduction equal to **half** its total rolled chance-weighted value, applied to every frontal hit. In exchange, your Dodge chance is set to **0** from all sources. **Re-expressed against the O1 passive model — and it survives cleanly.** AEGIS assumed no action: it operates on Block *chance*, which under O1 is exactly what Block is. "No longer rolls" is now literally a conversion of a passive chance layer into a flat layer, with no stance, no input, and no spend anywhere in it. Dodge zeroing likewise acts on a passive chance. **The `1/2` conversion factor is a frozen O2 value and is not re-authored**; the *shape* (certainty bought at a fraction of expected value) is intact. Bulwark's compliant O3 More slot (§2.4) if one is ever wanted. |

**Why the keystone is not universally superior:** halving the value in exchange for certainty is a net *loss* of expected mitigation. It buys predictability, which is worth a great deal to a player who dies to spikes and nothing at all to a player who never gets hit. Zeroing Dodge makes Bulwark and Kinesis genuinely exclusive at the keystone tier — you cannot hold AEGIS and SLIPSTREAM.

**Balance notes — the invulnerability loop.** §7.10 risk 4 names parry refunds, dodge refunds, leech, overshields, and block combining into permanent safety. **O1 removes most of this risk surface by construction:** with no stamina pool there is no resource to refund into, and with block and dodge passive there are no defensive actions to chain. The loop now has exactly one input — Parry — and one resource — Parry's cooldown. Guards applied here:
- Parry does not put itself on cooldown when it succeeds (B4); a miss does. It generates no resource of any kind. **REDESIGN [O20]** — a perfect player therefore has Parry available continuously, and whether that is acceptable is a re-cost question the frozen cooldown value cannot yet answer.
- Guard Doctrine (B9) is bounded to two guaranteed Blocks and gated behind a 3-point Convergence with a two-lane prerequisite. **REDESIGN [O20].**
- **Riposte (B7) is the one remaining loop risk under O1:** passive Block procs can bypass Parry's cooldown, and Parry's cooldown is the only defensive resource left. Bounded to "once" per failed Parry, but the frequency is now chance-driven rather than skill-gated. Flagged for the re-cost pass.
- Deflect scales on *prevented* damage, so it cannot be farmed against trivial hits.
- AEGIS zeroes Dodge, which structurally forbids stacking the two defensive keystones.
- No node in Bulwark grants health, shield capacity, or regeneration. Those are affix-layer quantities (§3.8) and duplicating them here would be the affix layer's job.

---

## 8. KINESIS

**Theme:** you are hardest to hit when you are hardest to predict. Kinesis is deliberately the *least* raw-speed constellation in the document — Master Sheet §7.10 risk 2 forbids it from making non-Swift characters the best movers, and risk 7 forbids balancing encounters around it. So Kinesis owns **evasion quality and aerial capability**, and hands raw traversal speed to Swift/Kinetic and to boots affixes.

**Owns:** dodge quality, movement efficiency, slide handling, aerial (§7.6). **Does not grant** dodge, slide, dash, or wall ride.

> **O1 propagation.** Kinesis holds no stamina references and needs no retagging — but its whole Lane A now sits on a **passive** Dodge. Read every "a successful Dodge" below as *"when a passive Dodge roll succeeds against an incoming hit,"* never as *"when the player presses dodge."* This is a real change in feel: Kinesis's evasion payoffs now fire on chance rather than on input, so their frequency is governed by Dodge % from gear rather than by player timing. Nodes are re-expressed in place; no value is authored.

**Lanes:** A — Evasion · B — Aerial · C — Slide

| # | Node | Class | Cost | Effect |
|---|---|---|---|---|
| K0 | **Light Footing** | Gateway | 1 | Your Dodge chance is not reduced while airborne or sliding. (Baseline applies a penalty to evasion outside neutral stance.) |
| K1 | **Reflex** | **Convergence** | **3** | After a successful Dodge, your Dodge chance against the next hit within 1.2s is increased by 10% / 18% / 25% of its own value. A **More** multiplier on a conditional window, not a flat addition to the affix bucket. **PROMOTED [O21]** — moved from Lane A Minor to Kinesis's Convergence tier; now Kinesis's single compliant More (§2.4 rule 1). Effect verbatim; cost unchanged (Minor 3 → Convergence 3). Requires two lanes, per §2.3 rule 4. **GAP [O21]:** same rank-ladder problem as P3 — the 10/18/25% ladder is carried verbatim into a 1-rank tier and frozen under O2. Note this More scales Dodge *chance*, not damage, and under O1 Dodge is a passive chance layer — whether a chance-space More counts against O3's build-wide cap of 3 is an unanswered question, logged in Open Questions. |
| K2 | **Phantom Step** | Notable | 2 | A successful Dodge grants 0.25s of full i-frames. **I-frame duration is tree-only** (§3.8, Layer-Ownership) — this is the only node in the game that grants it, and no affix may. Internal cooldown 2.0s so it cannot be chained through a burst. **NEEDS-RECOST [O1/O2]** — under O1 the trigger is a passive proc, so i-frames are granted by luck rather than by input. The 2.0s internal cooldown is now the *only* thing bounding i-frame uptime and it was costed against a player-initiated dodge; value frozen. |
| K3 | **Loft** | Minor ×3 | 3 | **Grants nothing yet.** Air jump preserves 10% / 20% / 30% more horizontal speed. Inert until K4, deliberately, mirroring Bulwark's Read node. |
| K4 | **AIR JUMP** | Notable | 2 | **VERB GRANT.** Unlocks a single mid-air jump, refreshed on landing, on wall contact, and on a successful Dodge. Air jump is the *only* verb this constellation grants. Per §3.3, "Additional Air Jump" is explicitly not an affix; `Air Jump Speed Retention %` on gear scales it for players who own it. |
| K5 | **Carve** | Minor ×3 | 3 | You may steer a slide 15° / 25° / 35° further from your entry vector without cancelling it. Control, not speed — §5.4 forbids self-acceleration beyond sprint and forbids wall riding generating speed. |
| K6 | **Slipstream** | Notable | 2 | Ending a slide by jumping preserves the slide's full speed into the jump instead of the standard reduction. Preserves; does not generate. Explicitly does **not** remove the combat tradeoffs of sliding (reduced accuracy, fixed camera height) — Progression-Architecture asks for exactly this restraint. |
| K7 | **Link — Weave** | Link | 1 | Connects A ↔ B. A successful Dodge while airborne refreshes your air jump immediately rather than on landing. **NEEDS-RECOST [O1/O2]** — same passive-trigger problem as K10's refund clause. **GAP [O20]:** the note "the two must be costed as one system" now spans the split — K10's refund clause is a REDESIGN item while K7 stays a recost item, so K7's re-cost cannot close until K10's redesign lands. Flagged, not resolved. |
| K8 | **Link — Chain** | Link | 1 | Connects B ↔ C. Landing from an air jump directly into a slide skips the slide's minimum-speed requirement. |
| K9 | **Kinetic Doctrine** | **Minor ×3** | **3** | **MOVED [O21]** — displaced into Lane A's Minor slot by Reflex's promotion; effect verbatim, cost unchanged (Convergence 3 → Minor 3). Enemy attacks that miss you due to Dodge do not break your slide, your wall ride, or your air jump refresh state. **Survives O1 cleanly and is the node the passive model *improves*:** it was written as "evasion stops costing tempo," and under a passive Dodge there is no tempo cost to begin with — so the node correctly narrows to *the enemy's attack* not breaking your movement state, which is a pure rule change with no input assumption. **GAP [O21]:** a single-rank rule change now occupies a 3-rank Minor slot with no ladder; none invented (O2). *(The "compliant O3 More slot" note is retired — Kinesis's More slot is now filled by K1 Reflex at Convergence.)* |
| **K10** | **SLIPSTREAM** | **Keystone** | **5** | **Rewrite:** your Dodge chance is doubled while airborne or sliding. While grounded and moving below sprint speed, your Dodge chance is **0**. **Re-expressed against the O1 passive model.** The chance clauses survive cleanly and are in fact *more* coherent passive than active: SLIPSTREAM is now purely a state-conditional modifier on a chance layer, with no input and no resource anywhere in it. The `×2` and the `0` are frozen O2 values, not re-authored. **REDESIGN [O20] — the air-jump refund clause** *(was NEEDS-RECOST [O1/O2]; O20 moves the refund clause specifically — the chance clauses above are untouched and stay frozen).* "A successful Dodge in either state refunds your air jump" assumed a player-initiated dodge: the player chose to spend, and got tempo back. Passive, it becomes *the enemy's* attack randomly granting the player a free air jump — a reward the player cannot aim at a moment. It is retained because K7 (Link — Weave) already does exactly this on a narrower trigger and the two must be costed together, but the refund's *trigger* cannot be cleanly re-expressed as a skill payoff under the passive model. Owner call. |

**Constellation arithmetic after the O21 swap:** 11 nodes, 1+3+2+3+2+3+2+1+1+3+5 = **26 points**. Unchanged; the swap was cost-neutral (3↔3), so no node here is COST-MOVED.

**Why the keystone is not universally superior:** it deletes evasion for any player who fights from cover, from a stance, or from a stationary firing position — which includes most of Precision and all of Bulwark. It is a build tax that pays only if you are already committed to constant motion. It also cannot coexist with AEGIS, which zeroes Dodge outright.

**Balance notes — identity theft.** Kinesis contains no raw movement speed, no dash cooldown reduction, no dash charges, no wall-ride duration. All four exist as affixes (§3.3) or belong to Swift/Kinetic. What Kinesis owns that neither can produce: i-frames (K2), the air jump verb (K4), slide steering (K5), and evasion-preserves-state (K9, now a Lane A Minor per O21). None of those is expressible as a percentage on an item, which is the test in Layer-Ownership.

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
| Elements + anything | BLOCKED — **not in the shipping five** | Authored in parallel against Rift/Entropy/Void (O5 + O19); slots in with the resistance step. Every pairing below the line is unvalidatable until then. |

**Shipping-set note (O5 plan of record).** Only the five non-Elements pairings above are live at first release. That is **10 possible pairs** from five constellations rather than 15 from six — and one of those ten (Bulwark + Kinesis) is deliberately impossible at keystone tier, leaving **nine viable two-keystone pairs at ship**. Whether nine clears O4's explicit "breadth of options and creative expression" bar is an owner judgement; it is not made here.

**Mandatory-cornerstone audit (§7.10 risk 3).** Each keystone imposes a real, named loss: FIXATION loses all off-target crit; PERPETUAL loses burst and all on-reload effects; TERMINAL loses all pressure damage; RESONANCE loses element choice; AEGIS loses half its mitigation value and all Dodge; SLIPSTREAM loses grounded evasion. No keystone is a strict upgrade over not taking it.

---

## 10. Vertical-slice subset

Per CONTEXT.md and §7.9: the slice ships approximately **15 skill nodes** with a **Core Point cap of 10** on a compressed curve. This is a pipeline test, not a balance statement.

**Selection principle:** prove one of each *kind* of node — Gateway, ranked Minor, rule-rewriting Notable, verb grant, and prerequisite gating — across an offensive, a defensive, and a mobility constellation, exactly as Progression-Architecture's prototyping order step 3 asks. Elements contributes **zero** nodes; it is blocked and nothing about it can be validated.

**This already matches the O5 plan of record:** the slice draws from the **five** shipping constellations. No change to the fifteen is required by O5. The fifteen do, however, inherit O1 — see the note below.

### 10.1 The fifteen

| # | Node | Constellation | Cost | Ranks | Proves |
|---|---|---|---|---|---|
| 1 | Sightline | Precision | 1 | 1 | Gateway; passive GE grant |
| 2 | Fixate | Precision | 3 | 1 | **More** multiplier bucket; per-target streak state. **GAP [O21]:** Fixate is now a **Convergence** node, and §10.2 omits all Convergence nodes because a two-lane prerequisite cannot be met under the 10-point cap. Either the slice keeps Fixate as a deliberate prerequisite-waived pipeline test of the More bucket, or the slice loses its only More and criteria 7 and 14 lose their subject. **Owner call; no substitute node is authored here** (naming one would re-open the fifteen and the 24-point total). |
| 3 | Tunnel Vision | Precision | 2 | 1 | Notable with a prerequisite; rule change on existing state |
| 4 | Trigger Discipline | Volley | 1 | 1 | Gateway in a second tree; recoil-system hook |
| 5 | Cyclic | Volley | 1/rank | 3 | Conditional ramp with decay; timer-driven GE |
| 6 | Last Round | Volley | 2 | 1 | Multishot hook; **proc coefficient 0 enforcement** |
| 7 | Open Wound | Affliction | 1 | 1 | Gateway; forces status application ignoring chance |
| 8 | Deepen | Affliction | 1/rank | 3 | Stack-cap modification on `UBreakerStatusComponent` |
| 9 | Set Stance | Bulwark | 1 | 1 | Gateway; block arc modification |
| 10 | Read | Bulwark | 1/rank | 3 | **Inert-until-prerequisite node** — validates a node that grants nothing yet |
| 11 | **PARRY** | Bulwark | 2 | 1 | **VERB GRANT.** Ability grant + input binding + stagger event + **its own short cooldown (O1)** — the only defensive input in the slice. **REDESIGN [O20]:** cooldown duration unauthored and its shape is an owner-led redesign, not a re-cost. |
| 12 | Light Footing | Kinesis | 1 | 1 | Gateway; conditional dodge modifier |
| 13 | Phantom Step | Kinesis | 2 | 1 | **I-frame grant with internal cooldown** — the tree-only quantity. **NEEDS-RECOST [O1/O2]:** triggers off a passive Dodge proc, not an input. |
| 14 | Loft | Kinesis | 1/rank | 3 | Second inert-until-prerequisite node; scales a verb the player may not own |
| 15 | **AIR JUMP** | Kinesis | 2 | 1 | **VERB GRANT.** Movement-component verb + refresh conditions |

**Total purchasable:** 24 points across 15 nodes — **unchanged by O21**, since Fixate's Minor 3-rank total (3) equals its Convergence cost (3). What changed is its tier and prerequisite, not its cost. **Slice cap: 10.** A slice player can buy roughly two Gateways, one full Minor, one Notable, and one verb grant — enough to feel a decision and nowhere near enough to buy everything. That gap is the point.

### 10.2 What the slice deliberately omits

- **All keystones** (five live, six authored). They gate on 18 points spent; the cap is 10. Shipping a keystone in the slice would require breaking its own gate.
- **All Convergence nodes.** Two-lane prerequisites cannot be satisfied under a 10-point cap. **GAP [O21]:** this now collides with slice node 2 (Fixate), which O21 promoted to Convergence. Flagged in §10.1; not resolved here.
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
13. **O1 conformance.** No slice node, ability, or Data Asset references `Stamina` or `MaxStamina`; neither attribute exists. Block and Dodge resolve as passive chance layers with no player input bound to either. Parry is the only defensive input, on its own cooldown, and no other ability reads or writes that cooldown except Riposte (B7, not in the slice).
14. **O3 conformance (§2.4).** The damage log must show More multipliers composing as an **unordered product** — reordering the sources produces a bit-identical result. Fixate is the slice's only More; **it is now PROMOTED [O21] to Convergence and is compliant**, so shipping it tests the More *bucket* on a compliant node — subject to the tier/prerequisite GAP in §10.1. No Aberrant signature affix in the slice pool may author a More.
15. **O2 conformance.** Every magnitude in the slice is a placeholder sourced from a Data Asset and is expected to change when wave-mode instrumentation reports. No sign-off may treat a slice value as tuned.

16. **O18 seed targets exist — node tuning validates against divergence from them.** O18 supplies the designer-input TTK/TTD seeds that wave-mode instrumentation measures against: **trash a little under 1s** (scaling exponentially with enemy difficulty), **rare/elite ~3s**, **boss encounters 20–45s** unless a special enemy, and **TTD 4–5s with no resources or sustain invested** (substantially higher once they are). These are seeds, not results. The acceptance test for any future node re-cost is therefore *divergence*: a node is validated by how far a build carrying it moves measured TTK/TTD away from the seed band for that enemy rank, not by the node's magnitude read in isolation. **No value in this document is authored against these targets in this pass** — O2 still freezes every magnitude until instrumentation reports, and the stat chassis solves backwards from O18, not the node table.

---

## 11. Summary of labels applied

**EXTENDS**
- Multishot-generated projectiles carry proc coefficient 0 for all status/on-hit/node triggers, coefficient 1 for damage. Ricochets carry 0.5 and cannot chain. (§4 Balance notes — extends §3.5 and §7.10 risk 1.)
- Affliction transfer ancestry is capped at depth 2 with a normalized payload. (§5, A9.)
- Elements reactions cap at chain depth 2 with proc coefficient 0 on the second reaction. (§6, E9.)
- Per-constellation total of 26 points, producing a hard maximum of two keystones per character. (§2.2 — extends §7.4's "two full plus one partial" into a specific number.)
- Bulwark's Read and Kinesis's Loft are purchasable while inert. (§7, §8 — a node pattern the master sheet does not describe.)
- **One More multiplier per constellation, on Convergence/Keystone only; unordered-product composition; Aberrant signature affixes may not author a More.** (§2.4 — extends O3 and O11.)

**RESOLVED**
- **Block/Dodge model — closed by O1.** No stamina pool; block and dodge are passive chance layers; Parry has its own short cooldown and is the only defensive input. §7.7, Progression-Architecture, and the shipped `UBreakerCombatComponent` action model are superseded. (§7 preamble.)
- **Element identity — closed by O5, renamed by O19.** Rift, **Entropy**, Void ("Time" retired as a name). Three reaction pairs, three resist stats, three affix lines, three UI glyphs. (§6.)

**CONFLICT**
- ~~Elements RESONANCE overlaps Caster/Multispell's stated fantasy.~~ **CLOSED by O19** — separation is "rotates all three" vs "masters one"; the mono-element restriction clause is **RETIRED [O19]**. Residual **FLAG [O19]:** E10's cost argument, already weakened by O5's drop to three elements, rested on that clause and needs re-examination. Flagged, not redesigned. (§6.)
- ~~**[O3-VIOLATION] ×3**~~ — **PROMOTED [O21].** P3 Fixate, A4 Necrosis and K1 Reflex are promoted to their constellation's Convergence tier and are now compliant with §2.4 rule 1. Placements, displacements and the two COST-MOVED [O21] nodes are tabulated in §2.4. Not a conflict any more.

**PROMOTED [O21]**
- **P3 Fixate** → Precision Convergence (3, unchanged); P9 Marksman's Ledger displaced to Lane B Minor ×3 (3).
- **A4 Necrosis** → Affliction Convergence (**COST-MOVED [O21]** 2 → 3); A9 Epidemiology displaced to Lane B Notable (**COST-MOVED [O21]** 3 → 2).
- **K1 Reflex** → Kinesis Convergence (3, unchanged); K9 Kinetic Doctrine displaced to Lane A Minor ×3 (3).
- All three constellations re-verified at **11 nodes / 26 points**. Effects verbatim. Rank-count mismatches flagged **GAP [O21]** on each node, not reconciled (O2).

**NEEDS-RECOST [O1/O2]** — *split by O20; these are the items new numbers can still fix*
- **K2 Phantom Step**, **K7 Link — Weave** (Kinesis) — passive-trigger re-cost; values frozen. (§8.)
- **The Suppressing reference** — stays NEEDS-RECOST per O20. **GAP:** it has no node in this document; it lives outside Core-Constellations (Class-Kits / Ability-Implementation-Spec / Master-Sheet-Import all carry "Suppressing" text). Listed here only to record the O20 split; the owning doc must carry the tag.
- *(B4 PARRY's cooldown duration remains unauthored under O2 and is tracked as Open Question 7a; the Bulwark items listed below moved out of this bucket.)*

**REDESIGN [O20]** — *not recost items; O1 removed their trigger and no new number can fix them*
- **K10 SLIPSTREAM — the air-jump refund clause** (Kinesis). The refund assumed a player-initiated dodge; passive, it is the enemy's attack randomly granting a free air jump. The chance clauses (×2 airborne/sliding, 0 grounded) survive fine and stay frozen under O2 — **only the refund clause is a redesign item.** (§8, K10.)
- **B4 PARRY — the cooldown *shape*** (Bulwark). "Refunds its own cost" was written against a stamina spend; re-expressing it as "a success does not start the cooldown" makes a perfect player parry continuously. The shape needs redesign, not a number. (§7, B4.)
- **B7 Link — Riposte** (Bulwark). Its trigger is now a passive Block proc bypassing Parry's cooldown — chance-driven where it was skill-gated. The one remaining invulnerability-loop input. (§7, B7.)
- **B9 Guard Doctrine** (Bulwark). Its authored effect ("refunds the stamina cost of your last three defensive actions") has no subject at all under O1: no stamina, no other defensive actions. Intent preserved, mechanism void. (§7, B9.)
- Owner-led per O20; **no redesign is attempted in this pass and no value is authored.**

**REMAP — O5**
- E4 Second Order, E5 Penetrance, E6 Insulator's Bane, E8 Link — Ground, E9 Reaction Chain, E10 RESONANCE, and RESONANCE's not-universally-superior argument. (§6.)

**BLOCKED**
- The entire Elements constellation, all 11 nodes, four dependency tags — **BLOCKED on the resistance pipeline step only.** O5 has settled which elements exist; authoring proceeds on paper in parallel, Data Assets do not. (§6.)
- *(Removed: "any Bulwark node touching the stamina pool" — O1 deleted the pool, so the block has no subject. Those nodes are now REDESIGN [O20], not blocked and not recost.)*

---

## 12. OPEN QUESTIONS

1. ~~**Is Block/Dodge a passive chance or a stamina-spending action?**~~ **ANSWERED — O1.** Passive chance, both. No stamina pool. Parry has its own short cooldown and is the only defensive input. AEGIS and SLIPSTREAM both re-express cleanly against the passive model (§7, §8). What *remains* open is the re-cost, not the model: **what is Parry's cooldown, and what does "refund" mean now?** Six nodes were tagged NEEDS-RECOST [O1/O2]; **O20 splits them — four are REDESIGN [O20]** (B4, B7, B9, and K10's refund clause) **and two remain NEEDS-RECOST** (K2, K7). None may be authored as a Data Asset until O2's instrumentation reports; the four redesign items additionally cannot be closed by any number at all.

2. **Is 26 points per constellation the right size?** It is derived from "two full plus one partial" and produces a hard cap of two keystones per character — **re-validated against five live constellations in §2.2a; the conclusion holds.** What changed is the *share*: 65 of 130 is 50% of the live tree, up from 41.7% of 156. If playtesting says two keystones is too many or too few, the correct lever is constellation size, not the point schedule — but §7.4 requires re-validating the ratio either way, and O5's five-constellation ship set is a second reason to re-read it.

2a. **Does 50% of the tree leave enough untaken?** New, raised by §2.2a. At six constellations a character left ~58% of the tree unbought; at five, exactly half. Lower opportunity cost per choice. Owner call — it interacts directly with O4's "err toward more viable builds."

3. **Elemental resistance: one stat or per-element?** (§6.7) **Narrowed by O5, not closed.** With exactly three elements, "per-element" means three resist stats — a cheap enough shape that the argument against it is weaker than when four were assumed. This determines whether Penetrance (E5) and Insulator's Bane (E6) are even coherent nodes. Per-element makes Insulator's Bane meaningful (a 1-in-3 condition) and RESONANCE's mono-element restriction genuinely costly; a single stat makes both nearly inert. Still an owner decision.

4. **Where does the resistance step sit in the §6.1 damage order?** Proposed between armour mitigation (4) and shield routing (5), but not decided. Affects whether elemental DoTs can be made shield-bypassing later.

5. ~~**Does RESONANCE need an explicit Multispell exemption, or is mono-element restriction sufficient separation?**~~ **ANSWERED — O19.** Neither: no exemption node, and the mono-element *restriction clause* is **RETIRED [O19]** as redundant. The separation is **"rotates all three" (Multispell) vs "masters one" (RESONANCE)**. Void Whisperer's coupling to the Void element is likewise intentional, not a collision. **What is now open instead:** E10's cost argument leaned on the retired clause and must be re-examined — flagged in §6, not redesigned (O2).

6. **Are lanes freely mixed, or mutually exclusive at the Convergence tier?** (§7.11) This document assumes freely mixed with Links as the cost of mixing. Mutual exclusivity would be a different and more opinionated tree.

7. **What input does Parry use?** **Half-answered by O1, and sharpened.** Block has no input — it is passive — so the relational half of this question dissolves. Parry needs a dedicated key of its own, and O1 confirms the unusual situation the original question anticipated: **Parry is the only defensive input in the entire game.** That is now a deliberate design position rather than an accident, and it deserves a design pass of its own. The key itself is unchosen.

7a. **What is Parry's cooldown, and what does a successful Parry do to it?** **Reclassified by O20: this is a REDESIGN question, not a recost question** — B4 sits in the REDESIGN [O20] bucket with B7 and B9. B4 currently re-expresses "refunds its own cost" as "a success does not start the cooldown," which means a perfect player parries continuously. Whether that is the intent, and what the miss-cooldown is, are both blocked on O2.

7b. **Does a chance-space More count against O3's hard cap of 3?** Raised by K1 Reflex, which authors a More on Dodge *chance* rather than on damage. **Unchanged by O21** — promoting Reflex to Convergence fixes its *tier* compliance but says nothing about whether a chance-space More consumes a cap slot. O3 caps Mores at 3 without distinguishing the quantity multiplied. Owner call.

8. **Does Fixate's More multiplier snapshot into DoTs?** §6.4 says DoTs snapshot source power at application. A 6-stack Fixate bleed applied at maximum streak would tick at 1.19x for its full life, including after the streak breaks. Consistent with the contract, possibly not intended.

9. **Can TERMINAL's banked damage be capped against bosses without disabling the build?** §7.10 risk 5. A single delivered instance is exactly the shape a boss cap is designed to catch.

10. **What are the ~15 world-content sources for non-level Core Points?** (§7.11) If they are all endgame-gated, the two-full-plus-one-partial shape does not exist until well after level 50 and the third constellation is a post-campaign feature rather than a build decision.

11. **Should keystones be respec-free like everything else?** §7.8 says Core Point respec is free unless playtesting establishes a reason for friction. A keystone that rewrites a rule is the most likely place for that reason to appear.

12. **Does Slipstream's preserved slide speed cross the chained-slide threshold named in §5.4?** It preserves rather than generates, so it should not — but the threshold itself is undefined, which is also an open item in §3.15.
