# Class Kits — Resource Loops, Abilities, and Branch Trees

> STATUS 2026-08-16: PARTIALLY BUILT — Swift and Caster are structurally complete in code, but most authored nodes are tag-only with no consumer (HANDOFF §5 R2), nine Caster nodes invalidated by the Mana inversion were never re-sited (:393-405), and the doc predates the S4 widening and points-per-level.

**Scope:** slice (see `Vertical-Slice.md`). Swift and Caster are authored
end-to-end as the slice's class story (O39); Gunsmith, Tank, and Support
appear here only as one-page treatments and remain designed-only and
post-slice — their full treatments live in their own class-kit files.
**Last reconciled against: O40**

Authority chain per O28: `Decisions.md` -> `Design-Overview.md` (map, not
law) -> this document.

Status: design draft. Numbers are placeholder and must be re-anchored after the Playtest Gym TTK pass, exactly as the affix tables are.

**TTK anchors now exist [O18]:** the re-anchoring pass no longer runs against an unknown target. Seed targets are trash a little under 1s (scaling exponentially with enemy difficulty), rare/elite ~3s, boss 20-45s unless a special enemy, and TTD 4-5s with no resources/sustain (substantially higher once sustain is invested). Every "re-anchor after the TTK pass" note in this document means "measure divergence from those seeds"; no value in this document is re-authored ahead of wave mode's report (O2 freeze).

**Element names [O19]:** the elements are **Rift / Entropy / Void**. "Time" as an element name is retired; all element references in this document read Entropy where they previously read Time.

Scope: the five classes (Caster, Swift, Gunsmith, Tank, Support) — resource loop, three branches, ability list, ultimate, and branch skill trees costed against the 30 Class Points granted across levels 1-30.

Detail level follows the master sheet's prototyping order (7.5): **Swift and Caster are authored in full.** Gunsmith, Tank, and Support are one-page treatments — enough to prove the resource grammar generalizes, not enough to build from.

---

## 0. Rules this document obeys

Restated so a reader does not have to hold four documents open.

**On the "Master N.N" citations throughout this document [O28].**
`Master-Sheet-Import.txt` is **superseded** — historical source material, not
law. Every `Master` reference below and in the rest of this file should be read
as *"this is where the idea came from"*, never as *"this is the authority"*.
Where a Master citation disagrees with `Decisions.md`, the ledger wins. The
citations are left in place rather than stripped because they make the
derivation auditable, which is the same reason the ledger never deletes a
superseded ruling.

**Three later rulings bear on this document and are not yet worked into the
branch content below.**

- **O29** (item level 120, tier ladder widened to T12..T-1, values tuned up
  with a back-loaded high end) is the answer to where endgame power comes from.
  It does not touch a class node — class trees complete at 30 and never grow —
  but it does mean §6.6's "levels 31-50 add nothing to this document's systems"
  is now *the point* rather than an admission.
- **O30** opens the Core tree to redesign around build axes (guns / abilities /
  minions). Two consequences here: the **Gunsmith** kit in §3 is the only place
  in the corpus that designs minions and deployables, and **none of it is
  built**; and `EBreakerBuildCondition` is movement-only, so the ailment, crit
  and stacking axes cannot be authored honestly on any node, class or Core.
- **O31** (raids are puzzles rewarded for team play; **every build must be able
  to make an impact**) is a constraint on class kits, not just on encounters.
  The class that fails it first is **Support** — §5's own acceptance criterion
  5 already tolerates "the worst solo damage dealer". Under O31 that is fine in
  a raid only if every encounter has a lane Support can contribute through.
  Recorded, not solved; see `Encounter-Design.md` and `Game-Modes.md`.

| Constraint | Source | Consequence here |
|---|---|---|
| Level cap 50, hard stop; gear is the entire endgame | Master 7.1 / 9.1 | Class trees complete at 30 and never grow again. No node scales with level. |
| Class selection permanent | Master 7.5 | Branch identity may be strong; class identity must be legible in the first hour. |
| Equip exactly 2 abilities + 1 ultimate | Master 7.5 | Six abilities per class = a real loadout decision, not a rotation. |
| Solo is the primary balance target | Master 11.1 | Every resource loop generates solo. Every Support branch has a self path. |
| Crit is the only multiplier of its kind | Master 6.3 | No node grants "chance to deal double damage", "chance to double-hit for full", or any parallel roll-and-multiply. |
| Affixes scale verbs, trees rewrite rules, classes own the fantasy | Layer-Ownership | **No node in this document is a flat percentage.** Every node is a rule rewrite or a resource-loop modifier. |
| **Parry (Bulwark) is the only tree-granted verb.** Air jump is base kit for everyone (two jumps); Swift's third jump, if built, is class-innate, not tree-granted. | **O25** (was Master 5.2 / 7.6) | No class tree grants a movement or defensive verb. Class trees grant *abilities* (equippable, slot-limited) and rule rewrites. |
| No grapple / tether | Master 5.1 | Kinetic and Demolitionist reposition with impulses and dashes only. |
| Flat sums -> one additive Increased bucket -> More reserved for trees/Anomalous | Item-Foundation | Class nodes may author More multipliers, but each class gets a **hard budget of three** (see 0.1). |
| More multipliers are an unordered product | **O3** | UNBLOCKED: the More-multiplier budget below is ratified — one More per branch keystone, build-wide cap 3, Aberrant signatures may not author a More. Remaining stat-aggregation-bucket work (Master 3.15 / 6.6) no longer gates it. |
| Block/dodge are passive chance layers; no stamina; Parry is the only defensive input | **O1** | Every node keyed to a dodge or block "input" is re-expressed against a passive RNG proc. |

### 0.1 The More-multiplier budget — RESOLVED [O3]

**RESOLVED [O3]: More multipliers form an unordered product; at most one More per branch keystone; build-wide cap 3; Aberrant signatures may not author a More.** The proposal below is ratified as written — the EXTENDS flag is closed and the ordering question that blocked it (old OQ2) is answered by "unordered product," so no aggregation-bucket decision is owed before class content is authored. The "BLOCKED on stat aggregation buckets" line in the table above is superseded for the More budget specifically.


The master sheet reserves More multipliers for trees and Anomalous items but does not say how many a tree may author. Without a cap, five classes x three branches is fifteen independent multipliers and the explosion risk in Master 7.10.1 arrives through the class layer instead of the affix layer.

**Proposed rule: each class may author at most three More multipliers across all three branches — at most one per branch, and only on a branch keystone.** Every other node must be a rule rewrite, a resource-loop change, or a flat/Increased contribution that folds into the existing buckets.

~~This is an EXTENDS on Master 7.6 and Layer-Ownership. Flagged for approval before any class content asset is authored.~~ Approved under O3.

### 0.2 Tree shape and cost grammar — EXTENDS

Master 7.11 leaves open whether branch nodes are freely mixed or mutually exclusive at major tiers. This document assumes **freely mixed with investment gates**, because permanent class selection already carries the "you cannot have everything" weight and a second lock is punitive.

Every branch uses the same five-tier shape:

| Tier | Gate (points already spent in this branch) | Node count | Cost per rank | Max ranks |
|---|---|---|---|---|
| 1 — Entry | 0 | 3 | 1 | 2 |
| 2 — Loop | 3 | 3 | 1 | 2 |
| 3 — Ability | 6 | 2 | 2 | 1 (grants an equippable ability) |
| 4 — Rewrite | 10 | 3 | 2 | 1 |
| 5 — Keystone | 16 | 1 | 4 | 1 |

Nodes per branch: **12**. Full branch cost: **3x2 + 3x2 + 2x2 + 3x2 + 4 = 26 points.**

Against 30 Class Points this produces exactly the intended shape:

- **One branch complete + 4 points elsewhere** (two entry nodes in a second branch, or one Tier-2 loop node). The specialist.
- **Two branches to Tier 4** (16 + 14 = 30, one Rewrite each, no keystone). The hybrid, and genuinely competitive.
- **Three branches to Tier 3** (10 + 10 + 10 = 30). Three abilities, no rewrites — deliberately the weakest of the three shapes, because breadth without a rule rewrite should feel thin.

Keystone gating at 16 means a keystone costs a real 20-point commitment. A character can hold at most **one** keystone. That is the intended ceiling.

**Ability access:** the two Tier-3 nodes in each branch each grant one equippable ability. Six abilities per class, two of them free at level 1 (one from each of the two "starter" branches per class, listed below) so a level-1 character has both ability slots filled.

**Ultimate:** each class has **one** ultimate, available from level 1, per Master 7.5. Branch keystones *rewrite* the ultimate rather than replacing it. This gives three distinct ultimate behaviors per class without three ultimate assets and without breaking the "one ultimate" lock.

### 0.3 Resource grammar — shared rules

All five resources are replicated GAS attributes on `UBreakerAttributeSet` (the class-resource attribute already exists). They share these rules so the HUD, affixes, and tuning generalize:

- **Range:** 0-100 base. `Maximum Resource` (universal core affix, Master 3.2) raises the ceiling. `Resource (regen /s)` raises passive regeneration where the class has any.
- **Generation is capped per second, per source.** Each class defines a per-source rate cap. This is the anti-farm rule and it is not optional; it is what stops wall-humping for Momentum and self-harm for Grit.
- **Generation events carry a proc coefficient**, consistent with Master 6.4. A DoT tick generates at its proc coefficient, not at 1.0.
- **Spending is the only gate on class abilities in three of five classes.** Caster and Gunsmith spend resource with no cooldown; Swift, Tank, and Support use a short cooldown *plus* resource, because their generation is event-driven and spikier.
- **No resource decays in a menu, at a Forge, or in the Anchor.** Decay is combat-state gated.
- **Resource on Kill / Resource on Damage Taken / Resource Cost Reduction affixes (Master 3.9) apply to all five.** Cost reduction is the additive Increased bucket; it does not create a More multiplier.

---

# 1. SWIFT — Momentum

**Prototypes first.** Kinetic tests the movement boundary; Marksman tests the projectile framework (Master 7.5).

**Fantasy:** the character who is punished for standing still. Not the fastest character in a straight line — the character whose damage, defense, and resource all come from *not being where the enemy aimed*.

## 1.1 The Momentum loop

Momentum is a 0-100 bar that fills from purposeful movement and drains when the player stops. It is the only resource in the game that is *lost* by inaction rather than merely un-generated.

**Generation**

| Source | Rate | Cap / anti-farm rule |
|---|---|---|
| Ground speed above 750 cm/s (slide entry threshold) | +6/s at sprint, scaling linearly to +10/s at 1250 cm/s | Requires net displacement: 3.0 m of world-space displacement per second of credit. Running into a wall generates nothing. |
| Airborne | +8/s | Capped at 3.0s of continuous air credit; resets on ground contact. Kills bunny-farm loops. |
| Slide | +12/s | Only while the slide is above the 750 cm/s threshold. Braked slides stop generating. |
| Dash | +10 flat | Once per dash charge consumed; no credit from cooldown-refunded charges beyond one per 1.0s. |
| Wall ride | +10/s | Master 5.4 forbids wall riding generating *speed*; it may generate *resource*. Capped at 0.85s, the wall-ride maximum. |
| Passive dodge proc (evade chance rolls and fires) | +15 flat | 0.5s internal cooldown. **RESOLVED [O1]:** this is an RNG proc off the passive evade chance layer, not a player input — the stamina pool is deleted and Parry is the only defensive input. The player cannot time this source; it is gear- and chance-driven. **Tuning risk (recorded, not solved):** proc variance makes Swift's floor and ceiling generation diverge over a short window. Variance is to be *measured* by wave mode, not solved here (O2). This still satisfies the Layer-Ownership class fantasy line, "Swift converts evasion into Momentum." |
| Weak-point hit while airborne or sliding | +5 flat | 0.25s internal cooldown. Proc coefficient applies. |

**Global generation cap: 25 Momentum per second from all sources combined.** Without it, an airborne sliding dashing headshot stacks four sources into a full bar in under a second.

**Decay**

- Grounded and below 400 cm/s for 1.0s: **-15/s**.
- Grounded and below 750 cm/s but above 400: **-6/s**.
- Above threshold, airborne, sliding, or wall riding: no decay.
- ADS while stationary: decay applies at the normal rate. Swift does not get to camp scoped at full bar. Marksman's Tier-4 rewrite is the *only* thing that changes this, and it is a deliberate branch payoff.

**Spending**

Swift abilities cost Momentum **and** carry a short cooldown (3-8s). The cooldown prevents a full bar from being dumped into one instant; the cost prevents the cooldown from being the only constraint.

**Momentum tiers** — several nodes and the ultimate read thresholds rather than the raw value. Three bands, displayed on the HUD as distinct states:

| Band | Range | Name |
|---|---|---|
| Low | 0-33 | Settled |
| Mid | 34-66 | Running |
| High | 67-100 | **Redline** |

Redline is the state the whole class is built to hold. Nodes that reward it are the spine of the tree.

**Solo viability:** every generation source is self-produced. No ally, no target, no ability required. A Swift with an empty weapon still fills the bar by moving. CONFIRMED against Master 11.1.

## 1.2 Swift abilities (6) + ultimate

Two are granted free at level 1 (marked *starter*): Slipcut and Skim. The other four are Tier-3 node grants.

| # | Ability | Branch | Cost | CD | Behavior |
|---|---|---|---|---|---|
| S1 | **Slipcut** *starter* | Frenzy | 20 Momentum | 4s | 0.4s window in which every weapon hit has its cadence cost halved (fires at 2x rate, consumes ammo normally). Ends early on reload. Rewards holding a full magazine into the window. |
| S2 | **Cadence Break** | Frenzy | 35 Momentum | 8s | Instantly completes the current reload and grants a 3s state: each consecutive hit on the same target adds a stacking flat damage bonus (10 stacks max, resets on miss or target swap). Explicitly the *flat* bucket, not Increased — it must not double-dip with Damage Ramp. |
| S3 | **Skim** *starter* | Kinetic | 15 Momentum | 3s | Directional impulse that preserves current horizontal speed and converts it into a lateral or backward vector. Not a dash (dash is base kit and untouched) — Skim has no speed floor and *cannot* increase speed. It redirects. Usable airborne once per airtime. |
| S4 | **Hard Stop** | Kinetic | 30 Momentum | 6s | Cancels all velocity instantly and grants 0.6s of Damage Reduction While Airborne treatment on the ground. The counter-intuitive Swift ability: dumping Momentum to stop is a real tactical option, and it feeds Kinetic's "spend to survive" identity. |
| S5 | **Sightline** | Marksman | 25 Momentum | 6s | Next shot fired within 2s pierces all targets in a line and cannot be blocked by cover-state enemies. Pierce here is a granted rule, distinct from the Pierce affix, and stacks additively with it (max +3 total). |
| S6 | **Lead** | Marksman | 40 Momentum | 10s | Marks the target under the crosshair for 6s. Shots that hit the mark from more than 25 m away are treated as weak-point hits regardless of impact location. Range gate is what stops it being a free crit engine at close quarters. |

**ULTIMATE — OVERDRIVE.** Cost: 100 Momentum (full bar). No cooldown; the cost *is* the cooldown.

Base behavior: for 8 seconds, Momentum does not decay and all Momentum generation is doubled against the per-second cap (cap raised to 40/s). The player is locked at whatever band they entered at, minimum Redline.

**Naming note [O19]:** Overdrive's time-dilation read is a *presentation* of Swift's speed fantasy and is **unrelated to the Entropy element** — the collision is an artifact of the Time->Entropy rename, not a design coupling, and Overdrive grants no Entropy-element damage, scaling, or interaction.

Branch keystones rewrite it:

- **Frenzy keystone (Bloodrhythm):** Overdrive additionally makes every weapon hit refund 1 Momentum, and the ultimate ends immediately if the player goes 1.5s without landing a hit. Aggression-gated.
- **Kinetic keystone (Terminal Velocity):** Overdrive grants unlimited dash charges for its duration and removes the wall-ride timer. Speed guardrails in Master 5.4 still apply — no self-acceleration past sprint, wall ride still generates no speed. This is an *availability* rewrite, not a speed rewrite.
- **Marksman keystone (Standing Wave):** Overdrive freezes Momentum entirely (no gain, no loss) and converts the frozen value into weapon range and projectile speed treatment: shots behave as if fired at point-blank regardless of distance for the duration. The stationary Swift ultimate.

## 1.3 Swift branch — FRENZY

Identity: trigger discipline and cadence. The branch that wants a full magazine and a target that stays in front of it. Frenzy is the *least* mobile Swift branch on purpose — it is the answer to "what if I want Swift's resource but a shooter's rhythm."

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| F1 — Trigger Discipline | 1 | 2 | 1 | Momentum generation from weak-point hits no longer requires being airborne or sliding. R2: internal cooldown 0.25s -> 0.15s. |
| F2 — Loaded | 1 | 2 | 1 | Reloading while at Redline refunds ammunition to the magazine equal to the shots fired in the previous 2s (R1: half, R2: all). Rule rewrite; does not touch reload speed. |
| F3 — Short Leash | 1 | 2 | 1 | Momentum decay below 400 cm/s is delayed by 0.6s per rank. The node that makes Frenzy playable as a grounded shooter. |
| F4 — Rhythm | 2 | 2 | 1 | Every 5th consecutive hit on any target generates +8 Momentum, ignoring the global per-second cap. R2: every 4th. Missing resets the counter. |
| F5 — Dry Fire | 2 | 2 | 1 | Firing the last round in a magazine generates +12 Momentum. R2: also refunds 1s of ability cooldown. Rewards emptying rather than tapping. |
| F6 — Feed | 2 | 2 | 1 | Kills refund Momentum equal to 10% of the ability cost most recently paid (R2: 20%). Ties the loop to the kill without a flat Resource on Kill duplicate. |
| F7 — Slipcut Mastery | 3 | 1 | 2 | **Grants S2 Cadence Break.** Slipcut's window extends by 0.15s for each ability cooldown currently active. |
| F8 — Ammunition Economy | 3 | 1 | 2 | Ammo Returned on Kill triggers also generate 5 Momentum. Explicit affix-to-class bridge: this is the class layer *reading* the affix layer, not duplicating it. |
| F9 — Second Wind | 4 | 1 | 2 | Cadence Break's stacking flat bonus no longer resets on target swap; it resets only on a full second without a hit. Rewrite. |
| F10 — Redline Trigger | 4 | 1 | 2 | While at Redline, weapon cadence is treated as one tier faster for the purposes of the Damage Ramp Primary affix (stacks accrue at double rate). Reads an affix, changes its rule, adds no percentage. |
| F11 — No Safety | 4 | 1 | 2 | Momentum decay is doubled, and abilities cost 40% less Momentum. Straight rewrite with a real downside; this is the node that makes Frenzy read as a *class* choice rather than a bonus. |
| F12 — BLOODRHYTHM (keystone) | 5 | 1 | 4 | Rewrites Overdrive (above). **More multiplier (1 of 3):** while at Redline, weapon damage is multiplied by 1.20. This is Swift's Frenzy More and the only one in this branch. |

### 1.3.1 Frenzy — implementation status (slice, tiers 1-3)

Frenzy is now authored in `UBreakerProgressionLibrary::GetSwiftFrenzyTree()`, so
the branch strip shows the three chips §1.3-1.5 has always named. Ten nodes,
21 class points, tiers 1-3 only — the same slice cut as Kinetic and Marksman
(§7), not the five-tier full branch above. **Every magnitude is O2 PLACEHOLDER.**

**The problem this section exists to record.** Every node in the table above is
a *Momentum-loop* rewrite, and the Momentum loop is not a `EBreakerNodeStatTarget`.
Transcribed literally, the whole branch would have been ten gameplay tags and
nothing else — a branch the player can buy and cannot feel. So each shipped node
carries **two** halves: the design document's rule, verbatim, as a tag for the
Momentum loop to read when it learns to; and a stat line that states the same
intent in a currency that reaches gameplay today. The second half is **authored
here, not transcribed**, and is listed below so nobody mistakes it for §1.3.

| Shipped node | Rule half (TRANSCRIBED) | Stat half (AUTHORED) | Why that stat |
|---|---|---|---|
| Trigger Discipline | F1 | +3 Critical Chance / rank | The node is about earning weak-point hits on the ground. |
| Loaded | F2 | +6% Increased Damage / rank **at Redline** | A magazine held into Redline is the node's whole payoff. |
| Short Leash | F3 | +5% Increased Move Speed / rank | Decay is keyed to a speed threshold; raising the speed is the same node said as a stat. |
| Rhythm | F4 | +3 Critical Chance / rank | Consecutive hits on one target. |
| Dry Fire | F5 | +5% Increased Damage / rank **at Redline** | Rewards emptying rather than tapping. |
| Feed | F6 | +45 flat Health / rank | Frenzy is the branch that stands its ground; standing costs health. |
| **Overrev** | **none — AUTHORED NODE** | +12% Increased Damage / rank **at Redline** | Frenzy's offensive spine, the counterpart to Kinetic's Downforce/Grind. See below. |
| Slipcut Mastery | F7 (rule only) | +20 flat Critical Damage | — |
| Ammunition Economy | F8 | +5% Increased Damage | The branch's one unconditional line. |
| BLOODRHYTHM | F12 | **More x1.20 at Redline** (transcribed) | Swift's Frenzy More; see §6.1. |

**Three deviations from the table above, each deliberate:**

1. **Overrev is an authored node with no F-number.** O27 rules that choices must
   beat accumulation and that the movement pillar is where build identity
   belongs. Kinetic received Downforce and Grind for exactly this reason;
   Frenzy needed the same, and **Redline** is the state it can hold. The result
   is that the three branches now own three distinct conditions — Kinetic
   airborne/wall/slide, Marksman unconditional, Frenzy Redline — which is what
   makes the branch strip a decision rather than a flavour label.
2. **F7 grants no ability.** §1.3 has it grant S2 Cadence Break; `Swift.CadenceBreak`
   does not exist in the ability fallback registry, and a node that unlocks a
   loadout entry resolving to nothing is worse than a missing grant. The rule
   half ships; the grant lands with the ability.
3. **The keystone costs 3, not 4.** The implemented branch grammar prices
   keystones at 3 (Overpressure, Culling) and internal consistency of the cost
   curve beat matching a number frozen under O2.

**BLOODRHYTHM'S ULTIMATE REWRITE IS REAL.** It is the first branch keystone in
the project whose ultimate rewrite actually resolves: `UBreakerAbility_Overdrive`
picks its variant from the OWNER's tag container, and node tags previously lived
only inside `FBreakerNodeStats` where no ability could see them.
`UBreakerProgressionComponent::PublishNodeTagsToAbilitySystem` now mirrors the
aggregate onto the ability system component as loose tags, diffed against the
last publication so a respec removes exactly what it added. Bloodrhythm carries
`Keystone.Swift.Bloodrhythm`, which is already a row in Overdrive's variant
table. **Kinetic's Terminal Velocity and Marksman's Standing Wave are now
claimed too** (2026-08-14): Overpressure and Culling carry their branch's
keystone tag, so all three Swift rewrites resolve and all three have real
behaviour behind them — see §6.1.1 for what shipped and what the owner still
rules. Not playtested; automation only.

## 1.4 Swift branch — KINETIC

Identity: the specialist home for velocity, aerial work, and evasion. Kinetic does not make the player *faster* — Master 5.4 forbids it. It makes the player *harder to resolve*: more direction changes, more air time, more value extracted per unit of speed.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| K1 — Read the Room | 1 | 2 | 1 | Airborne Momentum generation credit cap 3.0s -> 4.0s (R2: 5.0s). Loop modifier. |
| K2 — Contact | 1 | 2 | 1 | Wall ride Momentum generation continues for 0.4s after loss of contact (R2: 0.8s). Does not extend the wall ride itself — Master 5.3's 0.85s cap is untouched. |
| K3 — Carry | 1 | 2 | 1 | Slide-into-slide chaining generates a flat +10 Momentum on each successful chain (R2: +18), ignoring the per-second cap. Reads the Boots exclusive affix; does not grant chaining. |
| K4 — Redirect | 2 | 2 | 1 | Skim's cooldown is reduced by 1.0s each time the player changes horizontal facing by more than 90 degrees while airborne (R2: 1.5s), max once per airtime. |
| K5 — Evade Conversion | 2 | 2 | 1 | Passive dodge proc Momentum gain 15 -> 25 (R2: 35), and the internal cooldown drops to 0.3s. **RESOLVED [O1]:** re-expressed against the passive proc — this node raises the *yield* of an RNG proc, not the reward for a timed input, so its real value scales with the player's evade chance from gear rather than with skill. The Layer-Ownership class-fantasy node stated literally. **NEEDS-RECOST [O1/O2]:** yield-per-proc and the shortened ICD were costed against a controllable input; recost once wave mode reports proc-rate variance. No value changed here (O2 freeze). |
| K6 — Landing | 2 | 2 | 1 | Landing from more than 4 m of fall converts the fall's kinetic energy into Momentum (+1 per metre above 4, cap +25). R2: also refunds one dash charge. Interacts with Fall Damage Reduction; does not require it. |
| K7 — Skim Discipline | 3 | 1 | 2 | **Grants S4 Hard Stop.** Skim may be used twice per airtime instead of once. |
| K8 — Air Work | 3 | 1 | 2 | While airborne at Redline, the Accuracy While Airborne affix is treated as if one tier higher. If the player has none, grants the T5 value. Scales a verb the affix layer owns — flagged below as a CONFLICT candidate. |
| K9 — Momentum Shield | 4 | 1 | 2 | While at Redline, incoming damage is reduced by an amount equal to the Damage Reduction While Airborne affix value even when grounded. Rewrite: it changes *when* an existing stat applies, not its magnitude. |
| K10 — Spend to Live | 4 | 1 | 2 | Hard Stop's protective window becomes full damage immunity for its 0.6s, but its cost rises to 60 Momentum. Explicit cost-for-power rewrite, and the invulnerability-loop risk in Master 7.10.4 is bounded by the 6s cooldown and the 60 cost. |
| K11 — No Ground | 4 | 1 | 2 | Momentum no longer decays while airborne *or* within 0.5s of leaving the ground, and grounded decay increases by 50%. |
| K12 — TERMINAL VELOCITY (keystone) | 5 | 1 | 4 | Rewrites Overdrive (above). **More multiplier (2 of 3):** while airborne, weapon damage is multiplied by 1.25. Airborne-only is the tax that keeps it from being a general 1.25x. |

## 1.5 Swift branch — MARKSMAN

Identity: projectile behavior — ricochet, pierce, arcs, range. Marksman is the branch that makes Momentum *bankable*: the only branch that can hold a bar while stationary, and it pays for that privilege with a Tier-4 node rather than getting it free.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| M1 — Long Lens | 1 | 2 | 1 | Weak-point hits beyond 30 m generate +8 Momentum (R2: +14), separate internal cooldown from F1. |
| M2 — Steady | 1 | 2 | 1 | ADS while moving above the slide threshold does not increase spread. R2: ADS while airborne likewise. A rule rewrite of a handling behavior; no percentage. |
| M3 — Ledger | 1 | 2 | 1 | Momentum spent on Marksman abilities is refunded at 25% (R2: 50%) if the ability's effect lands a hit within its window. Anti-whiff. |
| M4 — Angle | 2 | 2 | 1 | Ricochets from the Ricochet Chance affix seek the nearest target within 12 m instead of reflecting geometrically (R2: 20 m). Rewrites an affix's behavior; adds no chance. |
| M5 — Mark Economy | 2 | 2 | 1 | Lead's mark persists through the target's death and jumps to the nearest enemy within 15 m (R2: 25 m). Proc coefficient 0 on the jump — it cannot chain-generate. |
| M6 — Pierce Discipline | 2 | 2 | 1 | Each target pierced by a single shot generates +4 Momentum (R2: +7). Caps at 3 targets to bound Multishot/Pierce interaction. |
| M7 — Sightline | 3 | 1 | 2 | **Grants S5 Sightline.** Sightline's pierce also ignores Armour on the second and subsequent targets. |
| M8 — Lead | 3 | 1 | 2 | **Grants S6 Lead.** Lead may be held on two targets simultaneously. |
| M9 — Reserve | 4 | 1 | 2 | **Momentum does not decay while ADS.** The stationary-Swift unlock, deliberately priced at Tier 4 in a single branch. Generation while ADS-stationary remains zero — this holds a bar, it does not build one. |
| M10 — Overpenetration | 4 | 1 | 2 | Shots that kill a target continue with their full remaining damage instead of the Pierce falloff. Bounded by the Pierce cap. |
| M11 — Called Shot | 4 | 1 | 2 | While at Redline, Lead's range gate drops from 25 m to 10 m. Band-gated rewrite of the ability's own rule. |
| M12 — STANDING WAVE (keystone) | 5 | 1 | 4 | Rewrites Overdrive (above). **More multiplier (3 of 3):** shots that hit beyond 40 m are multiplied by 1.25. Swift's three More multipliers are now spent; no further node in this class may author one. |

## 1.6 Swift — worked builds against 30 points

| Build | Spend | Reads as |
|---|---|---|
| Pure Kinetic | K1(2) K2(2) K3(2) K4(2) K5(2) K6(2) K7(2) K8(2) K9(2) K10(2) K11(2) K12(4) = 26, +4 into F1/F3 | Airborne duelist. 1.25x airborne. Never lands. |
| Kinetic/Marksman hybrid | K1-K7 to 16, M1-M3 + M7 to 14 = 30 | Two abilities, one rewrite each side, no keystone. Air-mobile pierce. |
| Frenzy specialist | F1-F12 = 26, +4 into M1/M2 | Grounded shooter with a resource bar. Redline 1.20x. |
| Triple splash | Each branch to Tier 3 (10 each) = 30 | Three abilities available, two equippable. Deliberately flat. |

## 1.7 Swift acceptance criteria

1. A player who ignores every advanced movement verb (walk, sprint, jump only) reaches and holds the Running band during a normal encounter, and reaches Redline at least once per encounter. If not, Momentum is a mobility tax and Master 7.10.7 is violated.
2. Standing still in an open room for 8 seconds drains a full bar to zero. Measured, not assumed.
3. No input pattern generates more than 25 Momentum/s. Verify by driving every source simultaneously in the Gym.
4. Wall-riding into a corner with no net displacement generates zero Momentum from the ground source and stops generating from the wall source at 0.85s.
5. Overdrive cannot be re-cast within 8 seconds of ending under any node combination. Verify with Bloodrhythm + Feed + Rhythm, the fastest known refill.
6. A Kinetic build's effective damage multiplier from class sources never exceeds 1.25x. Verify no second More has crept in via an ability.
7. Equipping two Frenzy abilities and a Kinetic keystone is legal and produces a coherent, non-degenerate character. Cross-branch loadouts must not be punished by the tree topology.

---

# 2. CASTER — Mana

**Prototypes second.** Multispell and Void Whisperer validate statuses and reactions; Spellblade tests ability-driven close combat (Master 7.5).

**Fantasy:** the only class whose weapon is a *resource generator* rather than the primary damage source. The Caster shoots to pay for spells. A Caster with a full Mana bar and no ammunition is still dangerous; a Caster with full ammunition and no Mana is a worse shooter than every other class.

> **AMENDED BY THE FANTASY INVERSION BELOW (owner ruling 2026-08-14).** The weapon is now an *accelerator*, not the income. "The Caster shoots to pay for spells" is superseded; a Caster who never fires still casts, more slowly.

## 2.1 The Mana loop

### OWNER RULING — 2026-08-14 — THE BAR IS INVERTED

> *"Caster's mana bar should be full and go down when using spells, and affixes like resource efficiency and resource regeneration should exist."*

This supersedes the accumulating-bank model described in the rest of this
section wherever the two conflict. **Mana starts FULL, spends DOWN, and
REGENERATES back up.** Passive regeneration is the PRIMARY recovery path; the
conditional sources below are kept, unchanged in their relative rates, and
reframed as **accelerators** on top of it.

What is implemented (`Source/RiorsEdge/Classes/BreakerManaComponent.*`):

| Rule | Value | Note |
|---|---|---|
| Starting bank | `MaxClassResource` | Filled on becoming a Caster and on every vitals restore (spawn, respawn, F1 reset). A fresh Caster casts immediately. |
| Passive regeneration | **6.0/s**, O2 PLACEHOLDER, `EditAnywhere` | Was +2.0/s. That number was a FLOOR under an accumulating loop and is far too slow as the primary path. 6.0/s refills the bar in ~17s and sustains one 30-cost cast every 5s with no target present. |
| Conditional generation cap | **6.0/s**, O2 PLACEHOLDER, was 20.0/s | This is where the re-weighting was done. At par with regeneration it says exactly what the ruling asks: fighting well recovers at most twice as fast as standing still. |
| Per-source rates | **unchanged** | Deliberately. SB1/SB3/VW1/MS1 and §2.7's shotgun-vs-rifle criterion are authored against these magnitudes, and the anti-Multishot 1/n ratio lives between them. |
| Regeneration in the safe zone | runs | The safe-zone gate is an anti-FARM rule aimed at target-dependent income. A Caster who cannot refill in camp has to leave camp to become able to fight. |
| Regeneration during Unmake | suspended | Unmake suspends "Mana generation" (§2.2), and regeneration is now most of it. Otherwise the ultimate refunds most of its own 80-Mana price while it is being spent. |
| Regeneration while Overcast | doubled | Same rule as conditional generation. See the Overcast note below — this is now a materially bigger effect than it was. |

**Resource efficiency.** Cost is composed as
`cost = AuthoredCost * ResourceCostMultiplier * UnmakeWindowScalar`
in `UBreakerCasterAbility::ComposeResourceCost`, read live on every cast so
re-gearing mid-fight is immediate. The two factors multiply rather than fight:
Unmake's 0 scalar makes a cast free regardless of efficiency, Long Dark's 0.5
and a 20% efficiency roll read 0.4x, and there is no division anywhere so no
scalar can be a divide-by-zero. Efficiency is floored
(`MinimumResourceCostMultiplier`) — gear may reduce a cost, never eliminate it,
because Mana *is* the cooldown.

### 2.1.1 NEEDS RE-SITING — the nodes the inversion invalidated

**Verified against `Source/RiorsEdge/Classes/BreakerManaComponent.h` on
2026-08-14.** Every claim in the table above reads true in code:
`PassiveRegenPerSecond = 6.0f`, `GlobalGenerationCap = 6.0f` (comment records
the 20.0 it replaced), `WeaponHitGain = 1.5f` and `WeakPointGain = 4.0f`
unchanged, `OvercastFloor = -20.0f`, `OvercastGenerationMultiplier = 2.0f`,
`OvercastIncomingDamageTaken = 0.15f`. Regeneration is applied in
`AdvanceLoop` *above* the safe-zone gate and *outside* the `GlobalGenerationCap`
budget, so the two paths genuinely are independent and conditional income really
is capped at par with the baseline.

The nine nodes below are the authored content that ruling invalidated. **Their
values are NOT re-authored here.** Rates moved this week and re-costing them is
an owner decision (O2 freeze); every one of them is tagged `NEEDS-RE-SITING
[Mana inversion]` at its own row in §2.3-§2.5 so a reader working from the
branch table cannot miss it.

**Invalidated by the inversion, and NOT yet re-sited** (flagged rather than
silently rewritten — these are owner calls):

- **VW3 — Patience** ("passive Mana regeneration doubles while the caster has
  not fired for 2s") was a minor trickle bonus and is now one of the strongest
  nodes in the class, doubling the primary income for standing still. Reads as
  over-tuned at the new rate.
- **SB1 / SB3 / VW1 / MS1 / MS6 / MS11** are all *generation-rate* nodes. They
  still function, but they now buy a share of the smaller half of the income,
  so the branches that lead with them read weaker than authored.
- **VW2 — Standing Water** (2-4 Mana/s from zones) was competitive with the old
  +2.0/s baseline and is now a fraction of it.
- **MS3 — Reservoir** (+15/+25 maximum Mana) *strengthens* under the inversion:
  maximum Mana is now the size of the magazine you start with, not just a
  ceiling you rarely reach. Its "one intentional stat node" exception is more
  defensible than when it was written.
- **SB9 — Reprisal** and **MS5 — Payment** (cost refunds) are unaffected.
- **§2.7 acceptance criterion 1** ("one 25-cost ability roughly every 13
  seconds from passive regeneration alone") must be restated: at 6.0/s it is
  roughly every 4 seconds, and the criterion's *point* — that a Caster with no
  target is not helpless — is better served by the new number.

**Overcast, re-read against a full bar.** Unaffected in mechanism: the negative
floor, the doubling, the +15% incoming damage, and the refuse-rather-than-
truncate rule at `UBreakerCasterAbility::CheckCost` all behave identically.
Its *meaning* moved, and one part is worth an owner look. Overcast was designed
as "the fourth cast is expensive rather than impossible" when the bar was hard
to fill; with a full starting bar it reads instead as "the bar is a magazine
and the overdraft is the last round", which is arguably a cleaner read. But the
doubled generation now applies mostly to a 6.0/s regeneration rather than to
target-dependent income, so a 20-deep debt is repaid in **under two seconds of
standing still**. Overcast's cost is now almost entirely the 15% damage window,
not the recovery time. §2.7 criterion 4 ("Overcast cannot produce a net-positive
Mana loop") still holds — the debt is repaid, never profited from — but the
*deterrent* is weaker than authored. Deepening the floor, lengthening the
penalty past the debt, or exempting regeneration from the doubling are the
three obvious dials; none is taken here, because that is a tuning ruling.

---

*Original section, retained for reasoning (see the amendment above for what is
superseded):*

Mana is a 0-100 bar with slow passive regeneration and fast conditional generation. Unlike Momentum it never decays — it is a *bank*, not a *state*. This is the deliberate opposite of Swift, and it is why the two classes prototype together: they prove the resource attribute supports both a decaying state machine and an accumulating wallet.

**Generation**

| Source | Rate | Cap / anti-farm rule |
|---|---|---|
| Passive regeneration | +2.0/s, always, in and out of combat — **SUPERSEDED, now 6.0/s and the primary path** | Scaled by the universal `Resource (regen /s)` affix. Alone, a full bar takes 50s — usable but never sufficient. |
| Weapon hit | +1.5 | Proc coefficient applies. Multishot pellets generate at 1/n, so a shotgun and a rifle bank at comparable rates. **This is the anti-Multishot rule and it is mandatory.** |
| Weak-point hit | +4.0 | Replaces the weapon-hit gain, does not stack with it. |
| Kill | +8.0 | Flat. Stacks with `Resource on Kill`. |
| Status application (Bleed, Poison, and later elemental) | +3.0 | 0.4s internal cooldown *per status type*. DoT ticks generate nothing — only applications. Prevents Tick Frequency from becoming a Mana engine. |
| Reload completed | +6.0 | Once per reload; no credit for cancelled reloads. Rewards the down-time the class already has. |

**Global generation cap: 20 Mana per second.** Lower than Swift's because Caster generation is target-dependent and a dense pack would otherwise fill the bar instantly. **SUPERSEDED 2026-08-14: 6.0/s**, at par with the new regeneration rate, so conditional income is an accelerator rather than the income.

**Spending**

Caster abilities cost Mana and have **no cooldown**. Mana *is* the cooldown. This is the class's defining ergonomic: a Caster can cast the same spell three times in a row if they can pay, and that decision is the gameplay.

Exception: the ultimate has a cost and no cooldown, like everything else.

**Overcast — EXTENDS.** Any Caster ability may be cast at up to 20 Mana below zero, driving the bar negative. While Mana is negative:

- All passive and conditional generation is doubled until the bar returns to zero.
- The player takes 15% increased damage from all sources.
- No further ability may be cast until Mana is at or above zero.

Overcast is the mechanic that makes the Caster solo-viable in a burst window without giving them infinite resource, and it is the hook every Caster branch modifies. It is an EXTENDS on the master sheet, which specifies Mana only as "rewards active spell use, kills, and precision without becoming infinite during dense encounters." Overcast satisfies that clause by making the fourth cast expensive rather than impossible.

**Solo viability:** passive regeneration alone sustains a Caster at a slow rate with no target present. CONFIRMED against Master 11.1.

## 2.2 Caster abilities (6) + ultimate

Starters: Cleave and Rot.

| # | Ability | Branch | Cost | CD | Behavior |
|---|---|---|---|---|---|
| C1 | **Cleave** *starter* | Spellblade | 20 Mana | — | Short forward melee arc, 3 m, physical damage scaled by weapon damage. Applies Bleed at a 100% base chance. The Caster's only melee verb and the reason Melee Damage % affixes have a class home. |
| C2 | **Closequarter** | Spellblade | 35 Mana | — | Blink to the target under the crosshair within 12 m, arriving 2 m short of it. Not a dash and not a grapple — instantaneous, no travel, no tether, no velocity carried. Landing refunds 15 Mana if the target is at or below 40% health. |
| C3 | **Rot** *starter* | Void Whisperer | 25 Mana | — | 4 m radius zone at the aim point, 6s duration. Enemies inside take Poison and have their Armour reduced by a flat 40. Zones are the Void Whisperer's whole grammar. |
| C4 | **Siphon** | Void Whisperer | 30 Mana | — | 5s channel on one target: deals Void damage over time and heals the caster for a portion. Channel breaks on the caster taking damage above a threshold. The class's only self-heal and the branch's solo answer. |
| C5 | **Fracture** | Multispell | 30 Mana | — | Projectile that applies one status, cycling deterministically through the caster's available status types on each cast. The sequencing enabler; the cycle order is visible on the HUD. |
| C6 | **Resonance** | Multispell | 40 Mana | — | Detonates every status currently on the target for a burst of damage, consuming them. Damage scales with the *number* of distinct status types, not their stacks — an explicit anti-stacking rule that protects Master 7.10.5. |

**ULTIMATE — UNMAKE.** Cost: 80 Mana. No cooldown.

Base behavior: for 6 seconds, all Caster abilities cost 0 Mana, and Mana generation is suspended. The bar's remaining value at cast time is irrelevant; the window is fixed. Interacts cleanly with Overcast — a Caster can Overcast into Unmake and spend the debt during the free window.

Branch keystones rewrite it:

- **Spellblade keystone (Edgework):** during Unmake, Cleave has no animation lock and Closequarter has no range limit within line of sight. The mobility ultimate.
- **Void Whisperer keystone (Long Dark):** Unmake's duration becomes 12s but abilities cost 50% instead of 0%. Zones placed during it do not expire until the window ends. The attrition ultimate.
- **Multispell keystone (Cascade):** during Unmake, every status application also applies the next status in Fracture's cycle at proc coefficient 0. The reaction ultimate. Proc coefficient 0 is load-bearing: without it this is the recursion bomb named in Master 7.10.1.

### 2.2.1 Caster abilities — implementation status

Read from `Source/RiorsEdge/Abilities/` on 2026-08-14. **All six class abilities
and the ultimate are BUILT.** This section exists because the reverse used to be
true and the doc claimed nothing either way.

| # | Ability | Built? | Class file |
|---|---|---|---|
| C1 | Cleave | **BUILT** | `BreakerAbility_Cleave.{h,cpp}` (melee sweep via `BreakerMeleeSweep`) |
| C2 | Closequarter | **BUILT** | `BreakerAbility_Closequarter.{h,cpp}` |
| C3 | Rot | **BUILT** | `BreakerAbility_Rot.{h,cpp}` (zones via `Combat/BreakerZoneActor`) |
| C4 | Siphon | **BUILT** | `BreakerAbility_Siphon.{h,cpp}` |
| C5 | Fracture | **BUILT** | `BreakerAbility_Fracture.{h,cpp}` (cycle via `Combat/BreakerStatusCycleComponent`) |
| C6 | Resonance | **BUILT** | `BreakerAbility_Resonance.{h,cpp}` (via `Combat/BreakerStatusConsumption`) |
| — | UNMAKE | **BUILT** | `BreakerAbility_Unmake.{h,cpp}` |

**Two keystone rewrites are UNBUILT and are the only Caster gaps left in the
ability layer.** All three keystone tags exist (`Keystone.Caster.Edgework`,
`.LongDark`, `.Cascade`) and all three resolve as rows in Unmake's variant
table, so the gap is visible rather than silent:

- **Edgework's Closequarter half** — "no range limit within line of sight" —
  is not implemented. Its Cleave half (removing the animation lock) is.
- **Cascade** is not implemented at all beyond the variant row. It needs status
  *application* to be interceptable, which the status layer does not expose.

**CASTER CANNOT YET CHOOSE ITS ABILITIES, and the reason is not in the ability
layer.** `UBreakerAbilityComponent` now exposes a real selection API —
`GetSelectableAbilityIds` / `GetEquippedAbilityId` / `PreviewSelection` /
`TryEquipAbility`, with `ValidateSelection` as a world-free pure rule — replacing
the hardcoded fallback table this document was written against. It validates
class, slot and duplicate-equip, then delegates the single write to
`UBreakerProgressionComponent::EquipAbility`. But `EquipAbility` asks
`IsAbilityUnlocked`, which reads `ClassDefinition->StartingClassAbilityIds` /
`BaseUltimateId`, and `UBreakerProgressionLibrary::GetFallbackClassDefinition`
returns `nullptr` for every class but Swift. **A Caster therefore has a null
class definition, nothing reads as unlocked, and every equip attempt is refused
— the fallback default table (Cleave / Rot / Unmake) is still what a Caster
actually plays.** Verified in code, and the blocker is annotated at
`Abilities/BreakerAbilityComponent.cpp`. The fix is a Caster row in that
fallback definition naming Cleave and Rot as starters and `Caster.Unmake` as
the ultimate; the §2.3-2.5 Tier-3 grant nodes then supply the other four. That
is `Progression/` work and belongs to whoever owns that lane.

## 2.3 Caster branch — SPELLBLADE

Identity: close-range spell/melee hybrid and target-crossing mobility. The branch that converts the Mana bank into position.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| SB1 — Contact Charge | 1 | 2 | 1 | Melee hits generate Mana at the weak-point rate (+4.0) instead of the weapon-hit rate. R2: +6.0. **NEEDS-RE-SITING [Mana inversion]** — buys a share of conditional income, now capped at 6.0/s and no longer the bulk of the loop. |
| SB2 — Follow Through | 1 | 2 | 1 | Cleave's Bleed application also generates its status-application Mana even if Bleed is already present (R2: and refunds 5 Mana on kill). Bypasses the 0.4s per-type internal cooldown for melee only. |
| SB3 — Close | 1 | 2 | 1 | Weapon hits within 8 m generate double Mana (R2: within 12 m). The range-gated generation node that defines the branch's play distance. **NEEDS-RE-SITING [Mana inversion]** — doubling a rate that is now capped at par with a regeneration the node cannot touch. The branch's play-distance identity survives; its magnitude does not. |
| SB4 — Debt | 2 | 2 | 1 | Overcast's negative floor extends from -20 to -35 (R2: -50). More rope. |
| SB5 — Momentum Transfer | 2 | 2 | 1 | Closequarter's arrival grants 0.4s in which the next melee hit suppresses the target's passive block and evade *rolls* (R2: 0.8s). **RESOLVED [O1]:** re-expressed against the passive chance layer — this cancels a roll, it does not beat a stance. Rewrites the target's defensive roll, not the player's damage. |
| SB6 — Bloodprice | 2 | 2 | 1 | While Mana is negative, melee hits restore health equal to a portion of damage dealt (R2: doubled). Turns the Overcast penalty into a sustain window. |
| SB7 — Blink | 3 | 1 | 2 | **Grants C2 Closequarter.** Closequarter may be cast with no target to blink 12 m in the aim direction. |
| SB8 — Edge | 3 | 1 | 2 | Cleave's arc widens to 180 degrees and its Bleed applies to every target hit. Rule change; no damage percentage. |
| SB9 — Reprisal | 4 | 1 | 2 | When the passive Block proc fires, the next Cleave within 2s costs 0 Mana. **RESOLVED [O1]:** already authored against the passive chance layer and needs no re-expression; note only that its uptime is RNG-driven, a variance wave mode measures (O2). Reads the passive defensive layer as a resource source. |
| SB10 — No Distance | 4 | 1 | 2 | Closequarter's Mana refund triggers at 100% health instead of 40%, but its cost rises to 50. Reshapes the ability from an execute tool into a traversal tool. |
| SB11 — Overreach | 4 | 1 | 2 | While Mana is negative, all Caster abilities are cast at no cost *and* the damage-taken penalty rises from 15% to 30%. The full Overcast commitment. |
| SB12 — EDGEWORK (keystone) | 5 | 1 | 4 | Rewrites Unmake (above). **More multiplier (1 of 3):** melee damage is multiplied by 1.30. Melee-only is the tax. |

## 2.4 Caster branch — VOID WHISPERER — RULED [O19]

Identity: damage over time, sustain, and controlled zones. The attrition branch and the one that most directly exercises the status architecture.

**RULED [O19]: Void Whisperer IS the Void-element specialist.** The name/element coupling is intentional and descriptive, not an accident to be renamed away — this branch is the class-layer home of Void-element damage, and the collision flagged against Multispell is resolved in Void Whisperer's favour. Elements are Rift / Entropy / Void.

**BLOCKED:** every elemental line in this branch (Rift and Entropy references) waits on the resistance model missing from Master 6.1. Void, Bleed, and Poison are physical or armour-facing and can ship now. Nodes below are authored so that **no node requires an element to function** — elemental interactions are additive upgrades, not prerequisites.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| VW1 — Seep | 1 | 2 | 1 | Status applications generate +5.0 Mana instead of +3.0 (R2: +7.0). **NEEDS-RE-SITING [Mana inversion]** — conditional income, now the smaller half. |
| VW2 — Standing Water | 1 | 2 | 1 | Zones generate 2 Mana/s while at least one enemy is inside (R2: 4/s), independent of enemy count. Count-independence is the anti-farm rule. **NEEDS-RE-SITING [Mana inversion]** — 2-4/s was competitive against a +2.0/s baseline and is a third to two-thirds of a 6.0/s one. The worst-hit node in the class. |
| VW3 — Patience | 1 | 2 | 1 | Passive Mana regeneration doubles while the caster has not fired a weapon for 2s (R2: 1.2s). The stand-back generation node. **NEEDS-RE-SITING [Mana inversion] — the most urgent of the nine.** It doubles the PRIMARY income now: 6.0/s becomes 12.0/s for holding fire, which is more than the entire conditional cap. Authored as a trickle bonus, reads as the strongest node in the class. |
| VW4 — Lingering | 2 | 2 | 1 | Zone duration is refreshed, not stacked, when a second zone overlaps it (R2: refreshed and its radius grows by 1 m, once). Explicit anti-stack rule. |
| VW5 — Attrition | 2 | 2 | 1 | Enemies killed while affected by a Caster DoT refund 15 Mana (R2: 25). |
| VW6 — Drain | 2 | 2 | 1 | Siphon's channel no longer breaks on damage below 15% of max health (R2: 30%). |
| VW7 — Zonework | 3 | 1 | 2 | **Grants C3 Rot upgrade path / grants C4 Siphon.** Rot's Armour reduction becomes 40 flat plus an additional 40 against targets already affected by a DoT. Flat armour, not percentage — protects the boss cap in Master 7.10.5. |
| VW8 — Wellspring | 3 | 1 | 2 | Zones may be placed on the caster's own position and move with them for their duration. One at a time. |
| VW9 — Snapshot Discipline | 4 | 1 | 2 | DoTs applied while the caster is standing inside their own zone snapshot as if the caster's Critical Chance were 25 points higher. Reads Master 6.4's snapshot contract directly. Does not create a second multiplier. |
| VW10 — Terminal | 4 | 1 | 2 | DoTs applied by this Caster do not expire on targets below 25% health; they persist until death or cleanse. |
| VW11 — Long Debt | 4 | 1 | 2 | While Mana is negative, all Caster DoTs tick at double frequency and the caster takes 25% increased damage instead of 15%. **RESOLVED [O10] — tick interval is snapshotted with discrete steps, so this node applies at application time only (a DoT applied while Overcast keeps double frequency for its lifetime). Unblocked.** |
| VW12 — LONG DARK (keystone) | 5 | 1 | 4 | Rewrites Unmake (above). **More multiplier (2 of 3):** damage over time is multiplied by 1.30. **[O34 — RULED 2026-08-16 (A4), LIVE]** The owner ruled the open question: DoT ticks share **one additive Increased bucket** (Increased Damage and Increased DoT no longer multiply for ticks), and the DamageOverTime More lane now exists. This node's DoT-targeted More composes as authored: `AggregateStats` selects it together with Damage Mores (one O34 budget — strongest three, per-source 1.30 ceiling) and `ComposeDotSourcePower` multiplies it into the tick's More side under the single O34 ceiling. DoT ticks only; direct hits never see it — that remains the tax. Not retargeted, not re-bucketed: authored and paying exactly as this row always specified. |

## 2.5 Caster branch — MULTISPELL

Identity: sequencing different statuses to create reactions — **Multispell rotates all three elements (Rift / Entropy / Void); Void Whisperer masters one [O19].** That is the whole separation between the two branches: breadth-and-sequence versus depth-in-Void. The older separator — a mono-element restriction clause — is **retired as redundant under O19**; the retirement itself lives in Core-Constellations (C2) and is referenced here, not restated. The branch reads the shared status container and the Elements constellation without duplicating either.

**BLOCKED:** cross-element reactions require the reaction matrix Data Asset (Character-Progression-Architecture, Elements) and the missing resistance step. Multispell ships in a physical-only form (Bleed/Poison/Void) and expands when elements exist. Every node below is authored against "distinct status types," not against named elements, so the branch does not need rewriting later.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| MS1 — Variance | 1 | 2 | 1 | Applying a status type the target does not already have generates double Mana (R2: triple). The core sequencing incentive stated as a resource rule. **NEEDS-RE-SITING [Mana inversion]** — the incentive is intact; the payout is a multiple of the capped half. |
| MS2 — Cycle | 1 | 2 | 1 | Fracture's cycle advances on hit rather than on cast, so a missed cast does not waste a position (R2: the next position is previewed on the HUD 1 cast ahead). |
| MS3 — Reservoir | 1 | 2 | 1 | Maximum Mana +15 (R2: +25). **The one intentional stat node in this document** — Multispell needs headroom to hold multi-cast sequences and the alternative is a cost reduction that would double-dip with the affix layer. Flagged as a knowing exception in Open Questions. **NEEDS-RE-SITING [Mana inversion] — upward, not downward.** Maximum Mana is now the size of the magazine you spawn with, not a ceiling rarely touched. Also the one node whose exception the inversion makes MORE defensible, so re-siting it may mean nothing but recording that. |
| MS4 — Chain | 2 | 2 | 1 | A target carrying 2 distinct status types spreads the *newest* one to the nearest enemy within 8 m on application (R2: 12 m). Proc coefficient 0 on the spread; the spread cannot itself spread. Mirrors Affliction's Contagion normalization rule. |
| MS5 — Payment | 2 | 2 | 1 | Resonance refunds 5 Mana per distinct status consumed (R2: 10). |
| MS6 — Sequence | 2 | 2 | 1 | Applying 3 distinct status types to one target within 4s generates 20 Mana (R2: 30), once per target per 10s. **NEEDS-RE-SITING [Mana inversion]** — a 20-30 lump metered through a 6.0/s cap now takes 3.3-5.0 seconds to actually arrive, where the old 20/s cap paid it in one to one-and-a-half. The node did not change; the drip did. |
| MS7 — Fracture | 3 | 1 | 2 | **Grants C5 Fracture.** Fracture applies two cycle positions at once instead of one. |
| MS8 — Resonance | 3 | 1 | 2 | **Grants C6 Resonance.** Resonance no longer consumes the statuses it detonates; instead it halves their remaining duration. |
| MS9 — Interference | 4 | 1 | 2 | Resonance's damage scaling changes from linear in distinct-status-count to a fixed value per status *plus* a flat bonus at 3+. Deliberately re-shaped away from a count multiplier — the anti-explosion rewrite. |
| MS10 — Prepared | 4 | 1 | 2 | Overcast's doubled generation also applies to Multispell's status-application bonuses, and the negative floor drops to -35. |
| MS11 — Conductor's Rule | 4 | 1 | 2 | Only one reaction may trigger per target per 0.5s, and reactions that would have triggered instead grant 10 Mana. Turns the "same application must not trigger multiple reactions" architecture requirement into a *player-facing benefit* rather than an invisible clamp. **NEEDS-RE-SITING [Mana inversion]** — the compensation half is conditional income against the 6.0/s cap, so the consolation prize for a suppressed reaction is now a much smaller one. The clamp itself is unaffected and is the half that matters architecturally. |
| MS12 — CASCADE (keystone) | 5 | 1 | 4 | Rewrites Unmake (above). **RE-AUTHORED (owner ruling 2026-08-16):** the designed "1.25x More vs 3+ status targets" ships as a **target-rider Increased line** — +25% Increased Damage against targets carrying 3 or more distinct status types (`TargetMultiStatus`), resolved target-side in `ReceiveDamage`. Same trigger and magnitude, honest bucket: a target-conditional More is unsupported by rule (Hook-And-Condition-Vocabulary §3.3). Caster's third More **slot stays unspent**. |

## 2.6 Caster — worked builds against 30 points

| Build | Spend | Reads as |
|---|---|---|
| Pure Void Whisperer | VW1-VW12 = 26, +4 into MS1/MS3 | Zone attrition. 1.30x DoT. Slow, safe, boss-facing. |
| Spellblade specialist | SB1-SB12 = 26, +4 into VW1/VW3 | Melee burst, Overcast-fueled, 1.30x melee. Highest risk profile in the game. |
| Multispell/Void hybrid | MS to Tier 4 (16) + VW1-VW6 + VW7 (14) = 30 | Status breadth, no keystone, best generalist. |
| Spellblade/Multispell | SB to 16, MS1-MS3 + MS7 (14) = 30 | Melee applicator. Cleave applies, Resonance detonates. |

## 2.7 Caster acceptance criteria

1. A Caster who never fires their weapon can still cast one 25-cost ability roughly every 13 seconds from passive regeneration alone. Verified in an empty Gym room.
2. A Caster firing a Shotgun and a Caster firing a Rifle generate Mana within 15% of each other over a 30-second sustained window. If not, the Multishot 1/n rule is wrong.
3. DoT ticks generate zero Mana under every node combination. Verify with VW1 + MS1 + maximum Tick Frequency.
4. Overcast cannot produce a net-positive Mana loop: entering Overcast and spending the debt must always cost more real time than casting from a positive bar. Verify with SB4 + SB11 + MS10, the deepest debt configuration.
5. Resonance's damage against a target with 6 statuses is no more than 2.2x its damage against a target with 2 statuses, after MS9. Bounds Master 7.10.5.
6. Cascade + MS4 (Chain) does not produce unbounded status propagation. Verify with 20 enemies in a 10 m radius; propagation must terminate within one generation.
7. No Caster node grants a movement verb. Closequarter is an *ability* occupying a loadout slot, not a base-kit addition — a Caster who equips neither Spellblade ability has base-kit mobility only.

---

# 3. GUNSMITH — Scrap (one-page treatment)

**Fantasy:** the battlefield is a workshop. The Gunsmith's power is *placed* rather than held, and their weakness is that placement takes time they may not have.

**Scrap loop.** 0-100, no decay, no passive regeneration. Purely event-driven — the only class with zero idle generation, because deployables are permanent-until-destroyed and idle generation would mean free permanent power.

| Source | Rate | Cap rule |
|---|---|---|
| Kill | +12 | Flat. |
| Reload completed | +4 | Once per reload. |
| Emptying a magazine before reloading | +8 additional | Rewards commitment. |
| Deployable destroyed (yours) | +50% of its cost | Refund, not profit. |
| Damage dealt by your deployables | +1 per 500 damage | 0.5s internal cooldown. The solo self-sufficiency line: a placed turret pays for the next one. |

Global cap 15/s. Spending: deployables cost 25-60 Scrap and are the only Gunsmith abilities with no cooldown; personal abilities have cooldowns and no cost.

**Deployable density cap: 4 active, 2 of any one type.** Enforced by the owning component per Character-Progression-Architecture. Placing a fifth destroys the oldest and refunds it.

**Branches.** *Armory* — personal weapon modification and ammunition economy; the branch that is playable with zero deployables placed and is therefore the solo baseline. *Field Tech* — turrets, ammo crates, buff pylons; the branch that pays for itself. *Tinkerer* — traps, mines, disruption; the branch that requires knowing where the enemy will be.

**Abilities (6).** Sidearm Rig *(starter, Armory: 10s CD, next magazine deals bonus flat damage and pierces)*; Overhaul *(Armory: converts reserve ammo into magazine capacity for 10s)*; Turret *(starter, Field Tech: 40 Scrap, autonomous, 30s lifetime)*; Ammo Crate *(Field Tech: 30 Scrap, refills reserve on interact, solo-usable)*; Mine Cluster *(Tinkerer: 35 Scrap, 3 proximity charges)*; Disruptor *(Tinkerer: 45 Scrap, field that slows and strips Armour)*.

**Ultimate — FIELD ASSEMBLY.** 100 Scrap. Deploys all currently unlocked deployable types at once at no individual cost, and raises the density cap to 8 for 20s. Keystone rewrites: *Armory (Machinist)* — Field Assembly instead applies every deployable's effect to the player's own weapon for 20s, the solo/no-deployable ultimate; *Field Tech (Foundry)* — deployables placed during it never expire; *Tinkerer (Minefield)* — deployables placed during it are invisible until triggered.

**Tree shape.** Standard 12-node/26-point shape from 0.2. Node character: Armory nodes rewrite ammunition rules (magazine-to-reserve conversion, reload-as-a-resource-event); Field Tech nodes rewrite deployable lifetime, targeting, and the density cap itself; Tinkerer nodes rewrite trigger conditions and rearm behavior. **More budget: three, one per branch keystone.**

**Solo note.** Field Tech's Ammo Crate and Turret both function with no allies present; the deployable-damage Scrap source means a solo Gunsmith's economy closes without a party. CONFIRMED against Master 11.1.

**Acceptance criteria.** (1) A Gunsmith who places nothing is still a functional shooter via Armory. (2) Deployable density never exceeds the cap under Field Assembly + any node combination. (3) Deployable damage cannot generate more Scrap than the deployable cost, over the deployable's full lifetime, against a stationary target. (4) No deployable can be placed inside geometry or outside line of sight of the placement point.

---

# 4. TANK — Grit (one-page treatment)

**Fantasy:** the only class that gets stronger by being hit, without ever wanting to be hit more than necessary.

**Grit loop.** 0-100. Generation is post-mitigation damage taken, per the architecture doc's explicit instruction, plus aggression sources so the loop is not purely masochistic.

| Source | Rate | Cap rule |
|---|---|---|
| Post-mitigation damage taken | +1 Grit per 2% of maximum health lost | **Post-mitigation is mandatory.** A high-Armour Tank must not out-generate a low-Armour Tank by taking the same hit. |
| Self-inflicted damage | Generates at 25% rate | The Demolitionist anti-farm rule. Rocket-jumping must not be a Grit engine. |
| Melee kill | +10 | |
| Passive Block proc (block chance rolls and fires) | +6 | 0.4s internal cooldown. **RESOLVED [O1]:** an RNG proc off the passive block chance layer, not a stance the player holds — stamina is deleted, Parry is the only defensive input. **Tuning risk (recorded, not solved):** Grit's inflow now varies with block-chance rolls rather than player commitment; variance is measured by wave mode, not solved here (O2). The Layer-Ownership line holds: "Tank converts mitigation into Grit." |
| Enemy within 5 m | +1.5/s | Count-independent. Rewards holding ground without rewarding pack size. |

Decay: -5/s after 6s without taking damage or being within 5 m of an enemy. Global cap 20/s.

Spending: abilities cost Grit and carry a 5-12s cooldown.

**Branches.** *Leech* — sustain and overheal-to-shield conversion. *Bastion* — cover, shielding, enemy attention; the branch that is a genuine party role and must therefore have the strongest solo conversion (its shields convert to damage). *Demolitionist* — explosives and self-knockback traversal.

**Abilities (6).** Rend *(starter, Leech: melee that heals for a portion of damage; overheal becomes shield)*; Bloodline *(Leech: 8s, all Life on Hit is doubled and applies to DoT ticks at proc coefficient)*; Anchor Point *(starter, Bastion: deployable frontal cover, 12s)*; Provoke *(Bastion: forces enemies within 10 m to target you for 4s; solo conversion — each enemy provoked grants a stacking flat damage bonus)*; Breach Charge *(Demolitionist: thrown explosive, strong self-knockback, heavily reduced self-damage)*; Ground Zero *(Demolitionist: downward slam from airborne, radial damage and stagger)*.

**Ultimate — HOLD.** 100 Grit. For 10s, incoming damage is reduced to a fixed maximum per hit and Grit generation is tripled. Keystone rewrites: *Leech (Vein)* — Hold instead converts all incoming damage into healing over its duration at reduced rate; *Bastion (Wall)* — Hold's mitigation extends to all allies within 8 m and, solo, doubles for the Tank; *Demolitionist (Detonation)* — Hold ends early on command, releasing all damage absorbed during it as a radial explosion.

**Self-damage policy — EXTENDS.** Per Character-Progression-Architecture, full self-damage immunity is rejected. Proposed: Demolitionist nodes grant up to 80% self-damage reduction and full self-*knockback* control, never immunity. Rocket-jumping keeps a cost. This also settles Master 12.5's open self-damage question for the Tank case only; the general weapon rule is still open.

**Tree shape.** Standard 12-node/26-point shape. Node character: Leech rewrites where healing goes (overheal routing, DoT eligibility); Bastion rewrites threat, cover behavior, and shield decay; Demolitionist rewrites self-knockback and explosive falloff. **More budget: three.**

**Acceptance criteria.** (1) A Tank with maximum Armour generates the same Grit per incoming hit as a Tank with none, at equal health lost. (2) Self-damage cannot sustain a Grit loop: 60 seconds of uninterrupted rocket-jumping with no enemies present must not fill the bar. (3) Provoke has a solo effect that is meaningful with zero allies. (4) No combination of Leech nodes, Hold, and the passive Block layer produces indefinite survivability — verify against Master 7.10.4.

---

# 5. SUPPORT — Charge (one-page treatment)

**Fantasy:** force multiplication that works on a party of five and on a party of one. The hardest solo problem in the game and the one the master sheet calls out by name (7.10.6).

**Charge loop — the solo rule is the design.** Charge generation must never require an ally. Every group source has a self-facing twin.

| Source | Rate | Cap rule |
|---|---|---|
| Healing done to allies | +1 per 3% of the target's maximum health | Overheal generates nothing. |
| **Healing or shielding done to self** | +1 per 3% of own maximum health | Identical rate. This is the anti-7.10.6 clause and it is non-negotiable. |
| Damage dealt to a marked target | +1 per 2% of the target's maximum health | The offensive conversion path, available in all three branches. |
| Buff uptime on any target including self | +2/s while at least one Support buff is active | Count-independent — buffing five allies generates the same as buffing yourself. **Critical:** without this, Support is a party class with a solo penalty. |
| Assist (damage to an enemy killed by an ally within 5s) | +8 | Party-only bonus. Efficiency advantage, never a requirement. |

Decay: none. Global cap 18/s. Spending: abilities cost Charge, 4-10s cooldowns.

**Branches.** *Medic* — direct healing, cleanse, revive; self path is self-healing at full rate. *Conductor* — allied cadence buffs (reload, fire rate, swap); self path is that every Conductor buff applies to the Support first and to allies second. *Warden* — marks, suppression, debuffs, CC; the natively solo branch and therefore the recommended starting branch.

**Abilities (6).** Patch *(starter, Medic: instant heal, applies to self at full value)*; Purge *(Medic: cleanse plus 3s status immunity, self-castable)*; Cadence *(starter, Conductor: 8s aura, reload speed and swap tempo; applies to self)*; Metronome *(Conductor: allies including self gain a stacking cadence bonus per consecutive hit)*; Mark *(Warden: 10s target mark; marked targets take increased damage and generate Charge when damaged)*; Suppress *(Warden: field that slows and reduces enemy accuracy)*.

**Ultimate — CONDUIT.** 100 Charge. For 12s, all Support abilities affect every valid target in a 15 m radius simultaneously and cost no Charge. Solo, this means every self-buff runs at once. Keystone rewrites: *Medic (Triage)* — Conduit continuously heals instead of enabling free casts, and prevents one lethal hit per target; *Conductor (Downbeat)* — Conduit's cadence effects double and extend to weapon damage as a flat contribution; *Warden (Blackout)* — Conduit marks and suppresses every enemy in radius, and the Support's own damage against marked targets is the More multiplier.

**Tree shape.** Standard 12-node/26-point shape. Node character: Medic rewrites where healing routes (overheal-to-shield, healing-as-damage against marked targets); Conductor rewrites buff propagation and duration rules; Warden rewrites mark behavior, mark propagation, and CC resistance handling. **More budget: three — and Warden's is the only one that is unconditional offense, which is intentional: it is the solo branch.**

**Acceptance criteria.** (1) A solo Support fills the Charge bar in a normal encounter within 20% of the time a partied Support takes. This is the single most important number in the class. (2) Every branch has at least one ability that is fully effective with zero allies present. (3) Buff-uptime generation is provably count-independent. (4) Overheal generates zero Charge under all node combinations. (5) A Support's solo damage output is within 25% of the five-class median. Support may be the worst solo damage dealer; it may not be unplayable.

---

# 6. Cross-class checks

## 6.1 More-multiplier ledger

| Class | Branch | Multiplier | Condition |
|---|---|---|---|
| Swift | Frenzy | 1.20x weapon damage | At Redline |
| Swift | Kinetic | 1.25x weapon damage | Airborne |
| Swift | Marksman | 1.25x | Beyond 40 m |
| Caster | Spellblade | 1.30x melee | Melee only |
| Caster | Void Whisperer | 1.30x DoT | DoT only — **[O34 — RULED 2026-08-16 (A4), LIVE]** authored and composing, see §2.4 VW12 |
| Caster | Multispell | *(slot unspent)* | Re-authored 2026-08-16 as +25% Increased vs 3+ distinct statuses (target rider) — see §2.5 MS12 |
| Gunsmith / Tank / Support | one per branch | TBD | To be authored with the full treatments |

**RESOLVED [O3]:** these multiply as an **unordered product**; each is authored on a branch keystone (the only legal class-layer site); the **build-wide cap is 3**; and **Aberrant signatures may not author a More**, so the Aberrant layer does not spend against this budget.

A character can hold **one** keystone (0.2). Therefore the class layer contributes at most **one** More multiplier, maximum 1.30x, always conditional. Combined with Anomalous items — the only other More source — the theoretical ceiling from non-crit multipliers is two conditional multipliers, inside O3's build-wide cap of 3. This is the intended bound and should be re-verified whenever a keystone or Anomalous is added.

### 6.1.1 What actually shipped — the ledger above is the DESIGN, not the build

Read from `Source/RiorsEdge/Progression/BreakerProgressionLibrary.cpp` on
2026-08-14. Three Swift branch keystones exist in code and **none of the three
matches the row above.** The design intent is kept; the divergence is recorded
so nobody cites the ledger as a description of the game.

| Branch | Ledger says | Code ships | Node id |
|---|---|---|---|
| Frenzy | 1.20x at Redline | 1.20x at Redline — **matches** | `Swift.Frenzy.Bloodrhythm` |
| Kinetic | K12 Terminal Velocity, 1.25x airborne | **Overpressure, 1.20x while SLIDING** | `Swift.Kinetic.Overpressure` |
| Marksman | M12 Standing Wave, 1.25x beyond 40 m | **Culling, 1.18x UNCONDITIONAL** | `Swift.Marksman.Culling` |

Two consequences the owner should see, neither of them fixable from this
document:

1. ~~**Overdrive's Kinetic and Marksman rewrites are unreachable.**~~ **CLOSED
   2026-08-14, by adoption.** Overdrive's variant table carried
   `Keystone.Swift.TerminalVelocity` and `Keystone.Swift.StandingWave` rows and
   neither Overpressure nor Culling granted either tag, so only Bloodrhythm's
   rewrite resolved. Of the two options recorded here — the shipped keystones
   adopt the tags, or the rewrites are re-sited — **adoption shipped**, because
   it is the half that changes NO authored value: Overpressure keeps its
   1.20x-while-sliding More and Culling its unconditional 1.18x, exactly as the
   table below records, and each additionally resolves Overdrive to its branch
   row. Re-siting would have meant authoring a new condition and magnitude,
   which O2 forbids an agent from doing.

   All three rewrite BEHAVIOURS are now built as well; before this they were
   empty C++ branches, so even Bloodrhythm — the one reachable keystone —
   rewrote nothing. Terminal Velocity suspends the dash cooldown and the
   wall-ride timer (an availability rewrite under O40(a)'s final single-dash
   model, not a charge pool); Standing Wave freezes Momentum and resolves
   weapon falloff to point-blank; Bloodrhythm refunds Momentum per landed hit
   outside the generation budget and ends the ultimate after 1.5s without one.

   **The gap is now instrumented rather than described.**
   `RiorsEdge.Abilities.KeystoneReachability` walks the shipped registries and
   fails if any variant row's tag is granted by no node, and its twin
   `KeystoneGrantsAreRead` fails if any node grants a keystone tag nothing
   consumes. Caster's three rows are exempt only while no Caster branch tree
   exists, and the exemption closes itself the day one is authored.

   **Still owner's to rule:** this section's own table still describes a
   different design (K12 1.25x airborne, M12 1.25x beyond 40 m) than the code
   ships. Adoption resolved the reachability defect; it did not resolve the
   design divergence, and only an owner ruling can.
2. **"Terminal Velocity" now names two different things.** K12 here, and a Core
   **Velocity** Convergence node (`Core.Velocity.TerminalVelocity`, a More of
   x1.30 while airborne) added under O27 — see `Core-Constellations.md`. That
   Core node is the closer match to what K12 describes, and it lives in the
   universal tree rather than a Swift branch. **Unresolved: which layer owns
   "airborne More".** Both existing is inside O3's cap only because a character
   cannot easily hold both, and that is luck, not design.

**Culling is also the first unconditional More in the class layer**, which the
ledger's own closing sentence ("always conditional") forbids. It was authored as
the deliberate pick for a build that refuses to organise around a movement
state. That is a defensible design, but it is a change to this section's rule
and needs an owner ruling rather than a doc edit.

## 6.2 Crit policy compliance

No node, ability, or resource loop in this document rolls a chance to multiply damage. VW9 (Snapshot Discipline) raises Critical Chance, which is the sanctioned stat, not a parallel roll. Master 6.3 CONFIRMED.

## 6.3 Verb compliance

No class tree grants walk, sprint, jump, crouch, dash, slide, wall ride, wall jump, air jump, or **Parry** (the only defensive input, per O1). **[O1]:** block and dodge are removed from this list because they are no longer verbs — they are passive chance layers, so there is nothing for a tree to grant; nodes may only read or re-yield their procs. Skim (S3) and Closequarter (C2) are *abilities occupying loadout slots*, not base-kit additions, and both are gated behind Tier-3 nodes or the starter grant. Master 5.2 CONFIRMED.

## 6.4 Affix-layer compliance

Nodes that reference affixes (F8, F10, K3, K8, K9, M4) read or re-rule an existing affix; none of them grants an affix's capability, and none is expressed as a flat percentage of a stat the affix layer owns. **K8 (Air Work) is the closest to the line** — it grants a floor value of Accuracy While Airborne to players who have none. Flagged in Open Questions.

## 6.5 Solo compliance

| Class | Idle generation | Target-free generation | Ally-free generation |
|---|---|---|---|
| Swift | No (decays) | **Yes** — movement | Yes |
| Caster | **Yes** — passive regen | Yes | Yes |
| Gunsmith | No | No — kills/reloads only | Yes |
| Tank | No (decays) | No — requires enemies | Yes |
| Support | No | Partial — self-buff uptime | **Yes, by explicit rule** |

Gunsmith and Tank both require enemies to generate, which is correct — neither is a resource the player should bank before a fight. Master 11.1 CONFIRMED.

## 6.6 Build-time compliance

Class identity completes at level 30 with 30 points and a 26-point branch. A player who reaches 30 having spent every point has a complete character, and levels 31-50 add nothing to this document's systems. Master 7.1 / 7.3 CONFIRMED.

---

# 7. Implementation notes for the slice

Per Master 7.9's vertical-slice override (slice cap 10, ~15 nodes), the slice should ship **Swift only, Kinetic and Marksman only, Tiers 1-3 only**: 6 entry nodes, 6 loop nodes, 4 ability nodes = 16 nodes, 10 points spendable. That exercises point validation, ability granting, respec, and save/load without authoring 60 nodes.

Data assets required, mapping to the existing `UClassDefinitionDataAsset` / `UProgressionNodeDataAsset` shapes:

- `DA_Class_Swift` — ClassId, Momentum attribute binding, 3 branch references, 2 starter abilities, 1 ultimate.
- `DA_Branch_Kinetic`, `DA_Branch_Marksman` — node lists, tier gates, cost curve.
- `DA_MomentumPolicy` — generation rates, per-source caps, global cap, decay thresholds and rates, band boundaries. **All Momentum tuning must live here, not in C++.** The same shape generalizes to `DA_ManaPolicy` and the other three. **[O18]:** these policy assets are the surface the TTK/TTD seed targets are tuned against once wave mode reports; **[O2]:** no value in them is re-authored before that report.
- Node effects that are pure Gameplay Effects (SB1, VW1, MS3) are content. Node effects that rewrite a rule (F2, K9, M9, MS11) need a code-side hook and should be enumerated before any of them is authored.

---

# 8. OPEN QUESTIONS

**REDESIGN bucket [O20] — empty for this document.** O20's redesign items (K10 SLIPSTREAM's refund clause and the stamina-built Bulwark nodes) live in the constellation layer, not here. Every class node touching block or dodge in this document survives re-expression against the passive proc; none was built on the deleted stamina spend, so no class node is promoted to the redesign bucket. One recost flag was raised: **K5 (Evade Conversion) — NEEDS-RECOST [O1/O2]**.

1. **Block and dodge as passive rolls versus player inputs — RESOLVED [O1].** The CONFLICT is closed: **block and dodge are passive chance layers, the stamina pool is deleted entirely, and Parry is the only defensive input** (Parry uses its own short cooldown). The document's original passive-roll reading was correct; Master 5.2/7.7, Layer-Ownership, and `UBreakerCombatComponent` are the stale side and have been corrected in code. Consequence retained as a standing note, not an open question: Swift's dodge->Momentum and Tank's block->Grit sources are uncontrollable and gear-driven by design, so their inflow carries **RNG variance that is measured by wave mode, not solved on paper** (O2). Affected nodes are annotated in place.
2. **More-multiplier ordering — RESOLVED [O3]:** unordered product; max one More per branch keystone; build-wide cap 3; Aberrant signatures may not author a More. The stat-aggregation-bucket question no longer gates the 0.1 budget; class-layer and Anomalous/Aberrant Mores multiply as an unordered set, so "before or after" has no answer to give and "take the highest" is rejected.
3. **Is the one-keystone-per-character ceiling correct, or should 30 points allow two?** Two keystones would require dropping branch cost to ~14, which would make full branches trivial. The current shape forces a genuine specialist/hybrid decision but may make hybrids feel like they got nothing memorable. Playtest question.
4. Does the Caster's Overcast mechanic survive contact with the damage pipeline, or does a negative resource attribute break GAS cost prediction? Needs a technical spike before Caster prototyping.
5. K8 (Air Work) grants a floor value of an affix the player may not have. Is that a legal class-layer action, or does it violate "affixes scale verbs the player already owns" from the other direction?
6. MS3 (Reservoir) is the only flat-stat node in the document, granted as a knowing exception. Should it be cut and Multispell's headroom solved another way, or does one such node per class become the pattern?
7. Momentum's global 25/s cap and Mana's 20/s are placeholders authored against no TTK. Both must be re-anchored after the Gym feedback pass alongside the affix tables. **[O18]:** seed targets now exist to anchor against (trash <1s, elite ~3s, boss 20-45s, TTD 4-5s with no sustain), so this is a measurement task rather than an unanswered design question — still frozen for authoring under O2.
8. Do branch keystones rewriting a shared ultimate hold up in implementation, or does each rewrite become a separate ability asset in practice — and if so, does that violate the "one ultimate" lock?
9. RESOLVED [O10]: tick interval snapshots with discrete steps; VW11 applies at application time and is unblocked.
10. Support's Charge count-independence rule means a five-player Support generates no faster than a solo one from buffs. Is that the right call, or should party play retain a modest efficiency edge on that source as it does on assists?

## Top three, if only three get answered

1. ~~Block/dodge — passive rolls or active inputs.~~ **RESOLVED [O1].**
2. ~~Stat aggregation buckets and More ordering.~~ **RESOLVED [O3].**
3. Whether one keystone per character is the right ceiling. Shapes all fifteen branches. **Now the top open question** — followed by OQ4 (Overcast vs GAS cost prediction) and OQ5 (K8's affix floor).
