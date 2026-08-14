# Class Kit — TANK / Grit (full treatment)

**Last reconciled against: O32** (2026-08-14).

> ## NOTHING IN THIS DOCUMENT IS BUILT.
>
> Verified against the code on 2026-08-14, because a full-depth treatment reads
> like a specification and this one is not yet one:
>
> - **The Tank is selectable and grants nothing.**
>   `UBreakerProgressionLibrary::GetFallbackClassDefinition` returns `nullptr`
>   for every class but Swift, so a locked Tank has no class definition, no
>   starting abilities, no ultimate and no branch trees.
> - **No Tank tree and no Tank ability exist.** `GetAllFallbackTrees` returns
>   the Core slice plus Swift's three; the ability registry carries Swift and
>   Caster only.
> - **The Grit loop does not exist.** `Source/RiorsEdge/Classes/` holds two
>   components, Momentum and Mana. `ClassResource` is inert for a Tank.
> - **The pipeline hooks this class is built on DO exist**, which is worth
>   saying because it makes the Tank the cheapest of the three unbuilt classes:
>   post-mitigation damage is already resolvable through the damage contract,
>   the passive Block proc is real (`UBreakerCombatComponent` exposes
>   `BlockChance` / `BlockMitigation`), `ApplyHealing` exists, and attacker-side
>   `OnHitDealt` / `OnKillDealt` are live. The Grit component would be reading
>   things that already report.
>
> **One structural gap this document cannot design around:**
> `EBreakerBuildCondition` is **movement-only**, so no node in this document can
> key off "while shielded", "while an enemy is within 5 m", or any other combat
> state. **O30** names the same widening as a prerequisite for its ailment, crit
> and stacking axes. Nothing here should be trusted as validated.

Status: design draft, **UNBUILT** (see above), authored to the depth of Class-Kits §1 (Swift) and §2 (Caster).
This document **extends** `Docs/Design/Class-Kits.md` §4 (Tank's one-page treatment). Where
§4 states a rule, this document honours it verbatim and elaborates it; nothing here contradicts
it. Where §4 is silent, this document authors.

> ## ⚑ EVERY MAGNITUDE IN THIS DOCUMENT IS AN O2 PLACEHOLDER
>
> **One flag, stated once, applying to every number below.** O2 freezes value authoring until
> wave-mode instrumentation reports. Every rate, cost, cooldown, duration, radius, threshold,
> percentage, and multiplier in this document is a **placeholder chosen for plausibility against
> the O18 seed targets**, not an authored value. Individual nodes are therefore **not**
> re-flagged; assume the flag. Where a number is not merely a placeholder but a *shape* that
> must survive re-costing (post-mitigation generation, the 25% self-damage rate, the 20/s global
> cap's existence, the guard ceilings in §7), the text says so explicitly.
>
> **O18 anchor used throughout.** TTD 4–5s with no resources/sustain; "substantially higher"
> with sustain invested. **Tank is the class that stretches "substantially": the placeholder
> target is 3.0–3.5× the no-sustain TTD — i.e. ~14–17s of sustained focus from a same-level
> elite pack for a fully invested Tank, and never an asymptote.** That multiplier is itself a
> placeholder and is the single most important number wave mode must return for this class.

**Rulings this document is authored against:** O1 (stamina deleted; block and dodge are passive
chance layers with no inputs; Parry is the only defensive input and is Bulwark-local), O2 (value
freeze), O3 (More = unordered product, one per branch keystone, build-wide cap 3), O13 (rocket:
strong self-damage reduction, full self-knockback control, **never** immunity; rocket-jumping
tolerated, never required), O15 (branch nodes freely mixed with investment gates), O18 (TTK/TTD
seeds), O19 (elements are Rift / Entropy / Void), O20 (the REDESIGN bucket is a list of what
**not** to build on).

**Rulings this document was authored BEFORE, added 2026-08-14.**

- **O28.** `Master-Sheet-Import.txt` is **superseded** — historical source
  material, not law. Master citations here record provenance only. Chain:
  `Decisions.md` -> `Design-Overview.md` (map) -> this document.
- **O30.** No defensive axis exists in O30's taxonomy at all — it names GUNS,
  ABILITIES and MINIONS. `Core-Tree-Redesign.md` flags the same hole for
  Bulwark and Kinesis (seven Core nodes with nowhere to go). **The Tank is the
  class that hole is largest for**, and where its defensive identity lives if
  the Core tree re-themes onto three offensive axes is genuinely unanswered.
  Recorded for the owner; not decided here.
- **O31.** Every build must be able to make an impact and **feel player power**;
  no encounter may have a build that cannot participate. The Tank passes the
  participation half of that easily — a Tank is never excluded by a damage
  check — and is the class most at risk on the **felt power** half. §5's
  Bastion branch is an explicit party role, and a build whose contribution is
  "the group survived" is exactly the shape O31's *"feel player power"* clause
  is aimed at. §4's solo-conversion rule (Bastion's shields convert to damage)
  is already the right instinct and now has a ruling behind it. **Not audited
  against the encounter roster; that is `Encounter-Design.md`'s pass.**

---

## 0. The O1 reality this kit is authored against

This is stated first because it is the single constraint that shapes the whole class.

**Tank has no defensive input.** Block does not happen because the player pressed something; it
is an RNG proc off a passive chance layer. Grit's block source (+6 Grit, 0.4s internal cooldown,
per the recorded Class-Kits §4 baseline) is therefore **gear-driven inflow that the player cannot
time, cannot bait, and cannot spend skill on.** Parry exists, but it belongs to the Bulwark
constellation and is not part of the class kit.

Three consequences the kit is built around rather than around-written:

1. **No node in this document may be phrased as "when you block" in a way that implies agency.**
   Block-keyed nodes are *yield modifiers on a proc*, exactly as Swift's K5 Evade Conversion is.
   They scale with Block Chance from the item layer, not with play.
2. **The block source can never be the spine of the loop.** A player with zero Block Chance must
   still have a complete, playable Grit economy. The spine is post-mitigation damage taken and
   proximity — two sources that fire deterministically whenever the Tank is doing its job.
   Block is a *bonus lane*, and §1.2's per-source caps enforce that.
3. **Variance is measured, not solved.** Block-proc variance makes Grit's floor and ceiling
   diverge over short windows, exactly as recorded for Swift. That is an O2 measurement task.

**The fantasy restated (Class-Kits §4):** *the only class that gets stronger by being hit,
without ever wanting to be hit more than necessary.* The kit's job is to make "being hit" a
consequence of holding a position, never a thing the player farms. Every anti-farm rule in §1.3
exists to protect that sentence.

---

## 1. The Grit loop

Grit is a 0–100 bar. Like Momentum it decays; unlike Momentum it decays on a *timer* rather than
on a state, because the Tank's "am I doing my job" signal is discrete (took a hit / enemy near)
rather than continuous (moving fast). It is neither Swift's state machine nor Caster's wallet —
it is a **third shape: a banked state with a lapse timer**, and it is the reason Tank is worth
prototyping after those two rather than alongside them.

### 1.1 Generation table

All damage-taken generation is computed on **post-mitigation** damage. This is mandatory and is
not a placeholder: it is the rule that stops Armour from being a Grit multiplier.

| # | Source | Rate | Cap / anti-farm rule |
|---|---|---|---|
| G1 | **Post-mitigation damage taken** | +1 Grit per 2% of maximum health lost | **Post-mitigation is mandatory** (Class-Kits §4). A high-Armour Tank must not out-generate a low-Armour Tank on the same incoming hit. Reads `FBreakerDamageResult::HealthDamage`, never `MitigatedDamage`. Per-source cap **10/s**. |
| G2 | **Damage taken to shield** | +1 Grit per 4% of maximum health, i.e. **half rate** | Shield absorption is still post-mitigation damage but it costs no health. Half rate is the guard that stops the Leech overshield loop from being a *generation* engine as well as a survival one (§7). Shares G1's 10/s cap — the two together never exceed 10/s. |
| G3 | **Self-inflicted damage** | Generates at **25% of the G1 rate** | **O13 / Class-Kits §4, recorded baseline.** The Demolitionist anti-farm rule. Rocket-jumping must not be a Grit engine. Requires the `Instigator` field on `FBreakerDamageRequest` (§6.0). Per-source cap **3/s**, so even at 25% it cannot be driven. |
| G4 | **Passive Block proc** (block chance rolls and fires) | **+6** flat | **0.4s internal cooldown** — the recorded Class-Kits §4 baseline, carried verbatim. An RNG proc, not a stance (O1). Per-source cap **8/s**, which the 0.4s ICD already implies; the cap is stated so a node that shortens the ICD cannot silently uncap the source. |
| G5 | **Melee kill** | **+10** | Flat. Aggression source, so the loop is not purely masochistic. No ICD needed — kills are self-limiting. |
| G6 | **Enemy within 5 m** | **+1.5/s** | **Count-independent.** Rewards holding ground, not standing in the biggest pack. One enemy at 4.9 m generates exactly what nine do. This is the source that guarantees solo viability without requiring damage intake. |
| G7 | **Ally shielded or damage redirected to you** *(Bastion only, via nodes)* | +1 per 3% of your maximum health absorbed on an ally's behalf | Party-only *efficiency*, never a requirement. Bastion's solo path (§4) does not route through G7. |

**Global generation cap: 20 Grit/s from all sources combined** (Class-Kits §4). Lower than
Swift's 25 because Tank's generation is adversary-driven and a boss slam plus a block proc plus
proximity would otherwise fill a third of the bar in one frame.

**Proc coefficient applies** to every event-driven source (Class-Kits §0.3). A DoT tick landing
on the Tank generates G1 at its proc coefficient, not at 1.0 — otherwise a stacked Bleed becomes
the cheapest Grit engine in the game.

### 1.2 Why the caps are shaped the way they are

The per-source caps are not decoration; they encode the class thesis.

- **G1+G2 capped at 10/s (half the global cap)** means a Tank *cannot* reach the global cap by
  taking damage alone. Filling the bar fast requires damage **plus** presence **plus** kills.
  Being hit is necessary and never sufficient. This is the mechanical statement of "never wants
  to be hit more than necessary."
- **G4 capped at 8/s** means block is worth roughly 40% of the bar per 5s at maximum realistic
  Block Chance — meaningful, never load-bearing. A zero-Block-Chance Tank loses a lane, not the
  loop.
- **G3 capped at 3/s** means 60s of uninterrupted rocket-jumping with no enemies present cannot
  fill the bar, because decay (§1.4) exceeds 3/s outside the lapse window. This is Class-Kits §4
  acceptance criterion 2 made structurally true rather than a thing to test for.
- **G6 count-independent** is the anti-pack-farm rule and mirrors Support's buff-uptime clause.

### 1.3 Anti-farm rules, stated as law

1. **Post-mitigation only.** Armour, resistances, and block mitigation all reduce Grit gain in
   exact proportion to how much they reduce damage. There is no configuration in which investing
   in mitigation increases Grit per hit.
2. **Self-damage at 25%, capped at 3/s, and never exempted.** No node in Demolitionist (§5) or
   anywhere else raises the 25% rate or the 3/s cap. Nodes may reduce self-*damage* (O13) — and
   reducing self-damage reduces the Grit it generates, by rule 1. **Rocket-jumping therefore gets
   strictly worse as a Grit engine the more the player invests in Demolitionist.** That is the
   intended direction and it is what makes O13's "tolerated, never required" true at the
   resource layer rather than only at the damage layer.
3. **Proximity is count-independent.** Verified against enemy count, not asserted.
4. **No generation from damage dealt to a target the Tank cannot legally damage** (friendly fire,
   invulnerable-phase bosses, own deployables). Anchor Point taking hits generates nothing.
5. **No generation outside combat state.** Decay is combat-gated (Class-Kits §0.3) and so is
   generation: standing in an Anchor with a Breach Charge in hand generates nothing.
6. **Overheal never generates.** Leech routes overheal to shield (§3), and shield *absorption*
   generates at G2's half rate — the healing itself generates nothing at any point.

### 1.4 Decay and banking

- **Lapse timer: 6 seconds.** Grit does not decay while the Tank has taken damage or had an enemy
  within 5 m in the last 6s. This is the "am I doing my job" window.
- **Outside the window: −5 Grit/s.**
- **Banking:** within the window, Grit is a bank — it does not decay at all, so a Tank can build
  through an approach and spend at the point of contact. This is deliberately more forgiving
  than Momentum and less forgiving than Mana. The Tank is *allowed* to bank, but only while
  engaged.
- **No decay in a menu, at a Forge, or in the Anchor** (Class-Kits §0.3). On leaving combat state
  entirely, Grit drains to zero at −5/s rather than snapping — so a re-engage inside a few
  seconds is not punished.
- **Death sets Grit to 0.** No banking through a death; there is no hardcore mode (O16) but there
  is no free ultimate on respawn either.

### 1.5 Grit bands — named states

Several nodes and the ultimate read thresholds rather than the raw value. Three bands, displayed
on the HUD as distinct states, mirroring Swift's Settled/Running/Redline grammar:

| Band | Range | Name |
|---|---|---|
| Low | 0–33 | **Winded** |
| Mid | 34–66 | **Braced** |
| High | 67–100 | **IRONCLAD** |

**IRONCLAD is the state the class is built to hold**, and it is the Tank's analogue of Redline.
The critical asymmetry: Redline is *achieved by playing well*, IRONCLAD is *achieved by being
under pressure*. A Tank at IRONCLAD in an empty room has failed at something; a Tank at IRONCLAD
in the middle of a pack is exactly right. Nodes that reward IRONCLAD are the spine of all three
trees, and every one of them is implicitly gated on danger.

**Winded is a real state, not just "low".** Two Tier-1 nodes across the branches read it, and the
class's worst moment — opening an encounter at zero — is a designed problem rather than an
accident. Base kit softens it: **combat entry grants +15 Grit once per combat state**, so the
first three seconds are not dead. (Placeholder; the *existence* of an entry grant is the design
commitment.)

### 1.6 Spending

Tank abilities cost Grit **and** carry a **5–12s cooldown** (Class-Kits §4). Same rationale as
Swift: the cooldown stops a full bar becoming one instant, the cost stops the cooldown being the
only constraint. Tank's cooldown band is the longest of the five classes because Tank abilities
are the most survivability-dense and the invulnerability audit (§7) leans on cooldown length as
its primary guard.

**Resource Cost Reduction affixes (Master 3.9) apply**, into the additive Increased bucket, never
as a More. **Maximum Resource raises the ceiling** and therefore raises the IRONCLAD threshold
proportionally — bands are percentages of maximum, not absolute values. Stated so a Maximum
Resource roll does not accidentally make IRONCLAD easier to hold.

### 1.7 Solo viability

| Requirement | Status |
|---|---|
| Idle generation | **No** — correct. Grit is not a resource to bank before a fight. |
| Target-free generation | **No** — requires enemies, matching Class-Kits §6.5. |
| Ally-free generation | **Yes** — G1, G2, G3, G4, G5, G6 all fire solo. G7 is the only ally source and no branch requires it. |

**CONFIRMED against Master 11.1.** Every branch has a solo conversion, stated explicitly per
branch in §3–§5. Bastion is the branch that carries the party role and therefore, per Class-Kits
§4, **must have the strongest solo conversion** — its shields convert to damage (§4).

---

## 2. Abilities (6) + ultimate

Starters (free at level 1, one from each of the two starter branches): **Rend** (Leech) and
**Anchor Point** (Bastion). The other four are Tier-3 node grants.

| # | Ability | Branch | Cost | CD | Behavior |
|---|---|---|---|---|---|
| T1 | **Rend** *starter* | Leech | 25 Grit | 6s | Melee sweep, 3 m arc. Heals for **35% of post-mitigation damage dealt**. **Overheal converts to shield** at 1:1, up to a cap of 25% of maximum health; shield decays at 4%/s after 3s. The class's core sustain verb and the reason Life on Hit affixes have a Tank home. |
| T2 | **Bloodline** | Leech | 40 Grit | 12s | 8s window: all Life on Hit is **doubled** and additionally applies to **DoT ticks at their proc coefficient**. Does not create healing where there is none — it multiplies an existing stat, so a Tank with no Life on Hit gets nothing. That is intentional: Bloodline is a *gear payoff*, and it is the ability that makes the item layer legible to the class. |
| T3 | **Anchor Point** *starter* | Bastion | 30 Grit | 10s | Deploys a frontal cover panel, 2.5 m wide × 2 m tall, **12s** lifetime, with its own health pool (placeholder: 20% of the Tank's maximum health). Blocks enemy projectiles; the Tank and allies may shoot through it from behind (one-way). Destroying it early refunds nothing — this is not a Scrap economy. |
| T4 | **Provoke** | Bastion | 35 Grit | 12s | Forces every enemy within 10 m to target the Tank for **4s**. **Solo conversion:** each enemy provoked grants a stacking **flat** damage bonus for 6s (placeholder: +4% of weapon base damage per enemy, 6 stacks max), delivered as a flat contribution so it cannot double-dip with the Increased bucket. Solo, against one enemy, it is still a real damage window. |
| T5 | **Breach Charge** | Demolitionist | 30 Grit | 8s | Thrown explosive, 1.2s fuse, 5 m radius. **Strong self-knockback with full directional control** (the impulse is computed from the blast normal and is not damped); **self-damage heavily reduced but never zero** (O13; base reduction placeholder 50%, up to 80% with nodes, floor never below 20% of the damage dealt to an enemy at the same distance). |
| T6 | **Ground Zero** | Demolitionist | 45 Grit | 10s | Usable only while airborne. Slams downward, dealing radial damage in a 6 m radius scaled by fall distance (cap at 12 m of fall) and applying **Stagger** for 1.5s. The Tank's only hard-CC verb and the ability that makes rocket-jumping *expressive* without making it mandatory — Ground Zero is fully usable from a normal jump off geometry. |

**Loadout note.** Two ability slots. Rend + Anchor Point is the level-1 shape and is coherent
(sustain + position). Every pair of the six is legal; none is a required combination.

### 2.1 ULTIMATE — HOLD

**Cost: 100 Grit (full bar). No cooldown; the cost is the cooldown.**

**Base behavior (10s):** incoming damage is **capped at a fixed maximum per hit** (placeholder:
8% of maximum health per instance, before shields), and **Grit generation is tripled** against a
raised global cap (60/s) — so Hold refills toward the next Hold, but never within its own
duration (100 Grit at 60/s is 1.67s of *theoretical* fill against a 10s window, and real inflow
is far below the cap; the guard in §7.3 makes this a hard rule rather than an emergent one).

**A per-hit cap is not damage reduction and the distinction is load-bearing.** Hold does nothing
against a stream of small hits and everything against a boss slam. That is the intended texture:
Hold is a *spike answer*, not a durability button, and it composes with the passive block layer
without multiplying it (§7.2).

Branch keystones rewrite it, via the **tag-driven variant** pattern locked in
Ability-Implementation-Spec §1.1 D1 (`Keystone.Tank.Vein` / `Keystone.Tank.Wall` /
`Keystone.Tank.Detonation` — the tags already exist in that spec's required-tags block):

- **Leech keystone (VEIN):** Hold's per-hit cap is removed. Instead, **all incoming damage is
  converted to healing at a reduced rate** (placeholder: you take the damage, and heal for 60%
  of the post-mitigation amount over 1.5s). Net: a heavily damped attrition window rather than a
  spike shield. **Explicitly not immunity** — the Tank still dies to enough incoming damage
  inside Vein, which is the guard (§7.4).
- **Bastion keystone (WALL):** Hold's per-hit cap **extends to all allies within 8 m**, and —
  **solo, with zero allies in radius — the cap for the Tank is halved** (i.e. doubled
  effectiveness). This is the strongest expression of Class-Kits §4's "the party branch must have
  the strongest solo conversion." Party and solo are two different, both-good outcomes rather
  than a party bonus with a solo penalty.
- **Demolitionist keystone (DETONATION):** Hold **ends early on command** (a second input
  binding — see §6.0; this is the only ability in the game that needs one), releasing **all
  damage absorbed during it** as a radial explosion (placeholder: 70% of absorbed damage, 8 m
  radius, no falloff, self-damage exempt because the Tank *is* the origin — this is the one
  self-damage exemption in the class and it is on the ultimate, not on a repeatable ability, so
  O13's "never immunity" for the *rocket* case is untouched).

---

## 3. Branch — LEECH

**Identity.** Leech is the branch that refuses to let a heal be wasted. Its whole grammar is
**routing**: where healing goes when the health bar is full, what counts as healing, and what
healing turns into when there is nothing left to heal. A Leech Tank at full health is not
overhealing — they are *charging*, and the shield they are charging is both their second health
bar and, at the keystone, the thing that keeps them alive through Hold.

The branch's honest weakness: it needs an enemy in melee range and a Life on Hit / leech
investment on gear. A Leech Tank fighting at 20 m with no leech affixes has a tree full of
inert nodes. That is the tax for having the game's most direct sustain, and Tier-1 node L3
exists specifically so the tax is felt as a build requirement rather than a dead branch.

**Solo conversion:** total. Every Leech node functions with zero allies. Leech is the
recommended starting branch for a solo Tank.

**Does not own:** health, shield capacity, or regeneration as *quantities* — those are affix-layer
(Core-Constellations §3.8, and Bulwark's §7 note applies here by symmetry). Leech owns where
healing *goes*, never how much of it there is.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **L1 — Clot** | 1 | 2 | 1 | Rend's overheal-to-shield conversion rate rises from 1:1 to 1.25:1 (R2: 1.5:1). Rewrites the routing ratio; grants no shield capacity. |
| **L2 — Slow Bleed** | 1 | 2 | 1 | Leech shield decay is delayed from 3s to 5s (R2: 8s) and the decay rate is unchanged. A duration rewrite, not a capacity one. |
| **L3 — Open Wound** | 1 | 2 | 1 | Life on Hit from all sources also triggers on the **first** hit of a multi-hit ability (Rend's sweep, Ground Zero's radial) rather than only on the primary target. R2: on every target hit, at proc coefficient. Explicit affix-to-class bridge: the class layer reading the affix layer, not duplicating it. |
| **L4 — Feed the Wound** | 2 | 2 | 1 | Grit generated from G2 (damage taken to shield) rises from half rate to **two-thirds** rate (R2: full G1 rate). Still bound by the shared 10/s G1+G2 cap — the cap is what keeps this from being a shield-farming engine. |
| **L5 — Bloodlet** | 2 | 2 | 1 | Melee kills additionally heal for 8% of maximum health (R2: 14%). Ties the aggression source to the sustain loop; overheal from this routes to shield through the normal path, so it is subject to the same cap. |
| **L6 — Transfusion** | 2 | 2 | 1 | While Leech shield is active, the passive Block proc's Grit yield rises from +6 to +9 (R2: +12) and its internal cooldown drops 0.4s → 0.3s. **Authored against the O1 passive layer:** this raises the *yield of an RNG proc*, exactly as Swift's K5 does; its real value scales with Block Chance from gear, not with play. Still bound by G4's 8/s per-source cap, which is why the shortened ICD is not a back door. |
| **L7 — Rend Mastery** | 3 | 1 | 2 | **Grants T2 Bloodline.** Rend's arc widens to 180° and its heal applies per target hit rather than once per cast, at proc coefficient. |
| **L8 — Second Heart** | 3 | 1 | 2 | Leech shield no longer decays while at **IRONCLAD**. Band-gated rewrite; it changes *when* shield persists, never how much there is. |
| **L9 — Nothing Wasted** | 4 | 1 | 2 | **All** healing you receive — from any source, including allies, potions, and affixes — routes its overheal into Leech shield, not just Rend's. The branch's thesis node. Cap unchanged (§7.1). |
| **L10 — Reciprocity** | 4 | 1 | 2 | Damage absorbed by Leech shield heals you for 20% of the absorbed amount **after** the shield breaks, over 2s. Deliberately post-break: it cannot participate in an absorb-heal-reabsorb cycle within one shield's lifetime (§7.4). |
| **L11 — Exsanguinate** | 4 | 1 | 2 | Bloodline's window no longer expires on time; it expires when you go **2 seconds without landing a melee hit**. Straight rewrite with a real downside — a Leech Tank forced off a target loses the window immediately. This is the node that makes Leech read as a *class* choice rather than a bonus. |
| **L12 — VEIN (keystone)** | 5 | 1 | 4 | Rewrites Hold (§2.1). **More multiplier (1 of 3): while Leech shield is active, melee damage is multiplied by 1.25.** Melee-only *and* shield-gated — a double tax, chosen because Leech's shield uptime is high enough that a single condition would make this an unconditional 1.25×. |

---

## 4. Branch — BASTION

**Identity.** Bastion is the branch that changes where the enemy is looking and what the ground
is shaped like. Its grammar is **attention and geometry**: threat, cover, and the conversion of
defensive investment into offensive output so that a party role is never a solo penalty. A
Bastion Tank does not survive by mitigating more; they survive by controlling who is shot at,
from what angle, and through what.

**Solo conversion — the branch's defining requirement (Class-Kits §4).** Every Bastion defensive
quantity has an offensive twin: Provoke's stacking flat damage per enemy provoked, B9's
shield-to-damage conversion, and Wall's solo doubling. A solo Bastion is a *close-range brawler
with cover*, not a party fixture with nothing to do.

### 4.1 Division of territory with the Bulwark constellation — STATED EXPLICITLY

Core-Constellations §7 Bulwark and this branch are adjacent and must not overlap. The division:

| Territory | Owner | Never authored by the other |
|---|---|---|
| Block **chance**, Block **quality**, block arc, block certainty (rolls → guaranteed) | **Bulwark** (B0 Set Stance, B1 Bracing, B2 Deflect, B10 AEGIS) | Bastion never modifies Block chance, arc, mitigation %, or converts Block from a roll to a certainty. Bastion may only read a block *proc* as a **Grit event**, and only through G4's yield. |
| **Parry** — the verb, its window, its cooldown economy | **Bulwark** (B3, B4, B7, B9) | **No Tank node in this document references Parry at all.** Not its window, not its cooldown, not its stagger. Parry is a Core Tree verb and the class layer does not touch it (Class-Kits §6.3). |
| Armour as a **quantity**, armour floors against shred | **Bulwark** (B5 Weight) | Bastion authors no armour value and no armour floor. |
| Stagger/knockback **immunity while shielded** | **Bulwark** (B6 Unyielding) | Bastion does not grant stagger immunity. Bastion *applies* stagger (T6 Ground Zero, Demolitionist) — a different direction. |
| **Threat / enemy attention** | **Bastion** | Bulwark has no threat node and must not gain one. |
| **Cover as a placed object** — deployable geometry, its lifetime, its one-way rule | **Bastion** (Anchor Point and its rewrites) | Bulwark's "refusal to be moved" is about the character's own body; Bastion's is about the ground. |
| **Shield generation, shield decay routing, shield-to-damage conversion** | **Bastion** and **Leech**, split: Leech owns *where healing routes into shield*, Bastion owns *what shield does when it exists* | Bulwark explicitly grants no shield capacity (§7's closing note). |
| **The Grit resource itself** | **Tank, exclusively** | Bulwark generates no resource of any kind — its own §7 note says so. |

**Keystone-space check.** AEGIS (B10) converts a passive chance layer into a flat certainty and
zeroes Dodge. **WALL (B12) does none of those things** — it extends a per-hit *cap* to allies and
doubles it solo. AEGIS answers "my mitigation is unreliable"; WALL answers "the spike is going to
land on someone." A character holding both gets two non-overlapping effects, which is the
intended outcome, not a collision. **No Bastion node re-treads B10's conversion-to-certainty
space, and no Bastion node touches Dodge in any direction.**

**REDESIGN [O20] awareness.** B4's cooldown shape, B7 Riposte, and B9 Guard Doctrine are in the
redesign bucket precisely because they were built on the deleted stamina spend and on
block-as-an-action. **No node in this branch is built on either.** Every block reference here is
a passive proc read for Grit yield, and no node refunds a cost, because Tank has no defensive
cost to refund. This branch introduces **zero** new items to the O20 bucket.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **B1 — Line of Sight** | 1 | 2 | 1 | Anchor Point's lifetime 12s → 16s (R2: 20s). Duration rewrite. |
| **B2 — Footing** | 1 | 2 | 1 | The proximity Grit source (G6) extends from 5 m to 7 m (R2: 9 m) **while you are within 3 m of your own Anchor Point**. Still count-independent. Ties the branch's geometry to the branch's resource. |
| **B3 — Loud** | 1 | 2 | 1 | Provoke's radius 10 m → 13 m (R2: 16 m). Rewrite of the ability's own rule. |
| **B4 — Held Ground** | 2 | 2 | 1 | Grit does not decay while you are within 3 m of your own Anchor Point, regardless of the 6s lapse timer (R2: and the entry grant in §1.5 re-triggers once per Anchor Point placed). A banking rewrite bounded by the deployable's own lifetime and cooldown. |
| **B5 — Answering Fire** | 2 | 2 | 1 | Enemies currently targeting you due to Provoke generate proximity Grit (G6) at 1.5× rate (R2: 2×). Still count-independent and still bound by the 20/s global cap. |
| **B6 — Bulk** | 2 | 2 | 1 | Anchor Point's health pool rises by 50% (R2: 100%) and it no longer takes damage from AoE that was not aimed at it. A durability rewrite on an *object*, not on the player — explicitly outside Bulwark's armour territory. |
| **B7 — Emplacement** | 3 | 1 | 2 | **Grants T4 Provoke.** Anchor Point may be placed on any surface including walls and ceilings, and while you are behind your own Anchor Point, weapon spread is treated as if stationary. |
| **B8 — Interposition** | 3 | 1 | 2 | Anchor Point additionally projects a 4 m shield-sharing field behind it: allies inside gain a portion of your Leech shield (party) — **and solo, the field instead grants you the shield-sharing amount as additional shield capacity headroom for the duration.** The self-facing twin is authored, not assumed. |
| **B9 — Conversion** | 4 | 1 | 2 | **The solo-conversion thesis node.** While you hold any shield, your weapon and melee damage receive a **flat** contribution scaled to your *current* shield value (placeholder: +1 flat damage per 2% of maximum health held as shield). Flat bucket, before the Increased bucket, so it cannot double-dip. Shield spent on absorbing damage reduces the bonus in real time — this is a *hold it or use it* tension, not a stacking bonus. |
| **B10 — Standing Order** | 4 | 1 | 2 | Provoke's forced-target duration no longer decays on its own; it ends when the enemy takes damage from a source that is not you, or after 10s, whichever is first. Rewrite of the threat *rule*. |
| **B11 — Immovable Object** | 4 | 1 | 2 | Anchor Point becomes indestructible for the first 4s of its lifetime, but its total lifetime is halved. Straight cost-for-power rewrite with a real downside. |
| **B12 — WALL (keystone)** | 5 | 1 | 4 | Rewrites Hold (§2.1). **More multiplier (2 of 3): while you are within 4 m of your own Anchor Point, all damage you deal is multiplied by 1.20.** The lowest of the three Tank Mores because it is the least conditional in kind (all damage, not melee-only) — the tax is positional and the ability's 10s cooldown and 12s lifetime bound its uptime. |

---

## 5. Branch — DEMOLITIONIST

**Identity.** Demolitionist is the branch that treats an explosion as a *tool with two ends*: one
end kills things, the other end moves the Tank. Its grammar is **blast geometry and impulse
control** — falloff shapes, radius, self-knockback vectors, and what a slam does to the ground it
lands on. It is the only Tank branch whose defensive answer is *not being there*, and it is the
branch that makes the heaviest class in the game capable of surprising vertical play.

**O13 compliance is structural, not decorative.** Three rules govern the entire branch:

1. **Self-damage reduction has a hard floor and never reaches zero.** Nodes stack to at most
   **80% reduction** (Class-Kits §4's proposed ceiling, carried), and the floor clause is
   absolute: **a Demolitionist always takes at least 20% of the self-damage a same-distance enemy
   would take from the same blast.** No node, no keystone, no ultimate, no gear combination
   removes this. Detonation's self-exemption (§2.1) applies to Hold's release, which is not a
   rocket and not a repeatable ability.
2. **Self-knockback control is total and is granted early.** Full directional control over the
   impulse — magnitude and vector — because O13 says knockback control, not damage immunity, is
   the thing that gets to be complete. D2 grants it at Tier 1.
3. **Rocket-jumping is never required.** Ground Zero works from a normal jump. Every mobility
   node in this branch has a non-rocket trigger. **No node in this branch, and no Demolitionist
   build, requires the player to damage themselves to function.** A Demolitionist who never
   rocket-jumps loses expression, not viability. And per §1.3 rule 2, investing in the branch
   makes self-damage a *worse* Grit source, not a better one.

**Solo conversion:** total. Explosives need no allies. Demolitionist is the highest-damage,
lowest-sustain Tank branch and is the closest the class comes to a conventional damage build.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **D1 — Shaped Charge** | 1 | 2 | 1 | Explosive damage falloff changes from linear to a flat plateau over the inner 40% of the radius (R2: 60%), then falls off normally. A falloff *shape* rewrite; peak damage is unchanged. |
| **D2 — Bootstraps** | 1 | 2 | 1 | **Full directional control of self-knockback from your own explosives** — the impulse is applied along your aim vector rather than the blast normal (R2: and its magnitude is unaffected by the self-damage reduction, so reducing the damage never reduces the launch). **The O13 "full self-knockback control" clause, granted at Tier 1 and free of any self-damage requirement.** |
| **D3 — Braced for Impact** | 1 | 2 | 1 | Self-damage reduction 50% → 65% (R2: 80%, the branch ceiling). **This node lowers your Grit generation from G3 by rule 1 of §1.3** — the text says so on the node, because the interaction is the design. |
| **D4 — Fragmentation** | 2 | 2 | 1 | Enemies killed by your explosives detonate for a portion of their maximum health in a 3 m radius (R2: 4 m). **Proc coefficient 0 on the secondary detonation and it cannot itself chain** — mirrors Caster MS4's normalization rule and is the anti-recursion guard (Master 7.10.1). |
| **D5 — Concussion** | 2 | 2 | 1 | Ground Zero's Stagger duration 1.5s → 2.0s (R2: 2.5s), and it additionally applies to enemies that were airborne when hit. |
| **D6 — Overpressure** | 2 | 2 | 1 | Breach Charge's fuse may be detonated early by re-pressing the input (R2: and it sticks to the first enemy it touches). A trigger-condition rewrite; no damage change. |
| **D7 — Demolition** | 3 | 1 | 2 | **Grants T5 Breach Charge.** Breach Charge may be held for two charges, sharing one cooldown. |
| **D8 — Terminal Descent** | 3 | 1 | 2 | **Grants T6 Ground Zero.** Ground Zero's fall-distance scaling cap rises from 12 m to 25 m, and **it may be cast from any airborne state including a normal jump** — restated on the node so the "never required" clause is visible where a player reads it. |
| **D9 — Blast Radius** | 4 | 1 | 2 | All your explosive radii increase by 50%, and self-damage is computed at the *un-increased* radius — so widening the blast does not widen the self-hit. Rewrite of which geometry each side of the explosion reads. |
| **D10 — Kinetic Recovery** | 4 | 1 | 2 | Landing after being launched by your own explosive within the last 3s cancels all fall damage and grants 1.5s of Stagger immunity. Makes the rocket-jump *land* cleanly without making the rocket-jump free — the damage on takeoff is untouched, and O13's floor still applies. |
| **D11 — Chain Reaction** | 4 | 1 | 2 | Your explosives detonating within 1.5s of each other on the same target apply a stacking **flat** damage bonus to the later blast (3 stacks max, flat bucket). Explicitly flat, not Increased, and explicitly capped — the anti-explosion rewrite in the branch that most needs one. |
| **D12 — DETONATION (keystone)** | 5 | 1 | 4 | Rewrites Hold (§2.1). **More multiplier (3 of 3): explosive damage against targets within the inner plateau of the blast (D1's inner radius) is multiplied by 1.30.** Tank's three More multipliers are now spent; **no further node in this class may author one** (O3). The condition is genuinely tight — the inner plateau is 40–60% of a 5–6 m radius, so this is a point-blank multiplier on a class that has to be point-blank anyway, which is the identity, not a loophole. |

---

## 6. Engineering — GAS mapping and missing hooks

Follows the patterns locked in `Docs/Design/Ability-Implementation-Spec.md`: D1 (tag-driven
ultimate variants), D2 (ability class hierarchy), D3 (cost/cooldown always GEs), D4 (standard tag
block), D5 (replication posture), D6 (statuses via `UBreakerStatusComponent`), D7 (Mores never on
abilities). **No number in this section is authored here; all values come from §1–§5 and are
placeholders per the header flag.**

### 6.0 Tank-wide prerequisites

**`UBreakerGritComponent`** (new, `Source/RiorsEdge/Classes/`), mirroring the shipped
`UBreakerMomentumComponent` shape. Binds `OnDamageReceived` (✅ exists — `FBreakerDamageResult`
already carries `HealthDamage`/`MitigatedDamage`, so post-mitigation is directly available) and
`OnBlockRolled` (missing hook, Ability-Implementation-Spec §4.0). Needs the same push/pop loop
override API Momentum has, because Hold, B4 Held Ground, and L8 Second Heart can all be live at
once and must revert independently:

```cpp
// Classes/BreakerGritComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerGritBandChanged, EBreakerGritBand, Band);

void  PushLoopOverride(FGameplayTag SourceTag, bool bSuspendDecay, float GenerationScalar, float GlobalCapOverride);
void  PopLoopOverride(FGameplayTag SourceTag);
void  GrantGrit(float Amount, bool bIgnoreGlobalCap);
EBreakerGritBand GetGritBand() const;                       // Winded / Braced / IRONCLAD
float GetPerSourceRateRemaining(FGameplayTag SourceTag) const; // §1.2 caps, queryable for tests
```

Per-source caps must be **enforced in the component and queryable**, not implemented as ad-hoc
timers per source, or §1.2's guards become untestable.

**MISSING HOOK — threat / aggro (T4 Provoke).** Named in Ability-Implementation-Spec §7 and it is
the single largest new system this class needs. **The enemy AI has no threat concept at all** —
there is no threat table, no target-override, and no notion of a forced target with a duration.
This is not a one-function addition:

```cpp
// AI/BreakerEnemy.h
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI|Threat")
void ForceTarget(AActor* Target, float Duration, FGameplayTag SourceTag);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI|Threat")
void ClearForcedTarget(FGameplayTag SourceTag);
UFUNCTION(BlueprintPure, Category="AI|Threat") AActor* GetForcedTarget() const;
UFUNCTION(BlueprintPure, Category="AI|Threat") bool IsForcedTargeting(const AActor* Target) const;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerForcedTargetChanged, AActor*, Target, bool, bForced);
```

Design requirements the hook must satisfy, or Provoke and half of Bastion do not work:
- **It must be an override, not a threat-number bump.** A threat table is a bigger system than
  this class needs and would silently interact with every other damage source.
- **B10 Standing Order needs the break condition** — "ends when the enemy takes damage from a
  source that is not you" — so the forced target must be cancellable from `ReceiveDamage` with an
  instigator check. That requires the `Instigator` field below.
- **B5 Answering Fire needs `IsForcedTargeting`** to scale G6 proximity generation.
- **Behaviour-tree integration is the real work:** the enemy's existing target-selection must
  consult `GetForcedTarget()` first and must resume normal selection cleanly on expiry, including
  mid-attack. **This is a blocking dependency for the whole Bastion branch.**

**MISSING HOOK — `ApplyStagger` (T6 Ground Zero).** Also required by Parry (Bulwark B4) and read
by Bulwark B6 Unyielding. Build once:

```cpp
// Combat/BreakerCombatComponent.h
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void ApplyStagger(float Duration, FGameplayTag SourceTag);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PushStaggerImmunity(FGameplayTag SourceTag, float Duration);   // D10 Kinetic Recovery, B6 Unyielding
UFUNCTION(BlueprintPure, Category="Combat") bool IsStaggered() const;
```

Stagger must interrupt an enemy's in-progress attack, not merely play an animation, or D5
Concussion is cosmetic.

**MISSING HOOK — the overheal-to-shield path (Leech's whole grammar).** Healing does not exist as
a first-class routed event today. This is the second-largest dependency after threat:

```cpp
// Combat/BreakerCombatComponent.h
USTRUCT(BlueprintType)
struct FBreakerHealResult
{
    UPROPERTY(BlueprintReadOnly) float Requested = 0.f;
    UPROPERTY(BlueprintReadOnly) float HealthRestored = 0.f;
    UPROPERTY(BlueprintReadOnly) float Overheal = 0.f;        // Support's Charge needs this to generate ZERO
    UPROPERTY(BlueprintReadOnly) float ShieldGranted = 0.f;   // the routed portion
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Instigator;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer SourceTags;
};
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
FBreakerHealResult ApplyHealing(float Amount, AActor* Instigator, const FGameplayTagContainer& SourceTags);

// Overheal routing policy — pushed by L1/L2/L9, popped on respec. Ratio and cap are data.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PushOverhealRouting(FGameplayTag SourceTag, float ConversionRatio, float ShieldCapFractionOfMaxHealth,
                         float DecayDelaySeconds, float DecayRatePerSecond);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PopOverhealRouting(FGameplayTag SourceTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerHealed, const FBreakerHealResult&, Result);
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHealed OnHealed;
```

Three requirements on this hook that the audit in §7 depends on:
- **The shield cap is enforced in `ApplyHealing`, once, for all routing sources.** L9 routes *more
  sources* into the shield; it must not be able to raise the cap. One cap, one place.
- **`Overheal` must be reported even when fully routed to shield** — Support's Charge generates
  zero on overheal and needs the number regardless of where it went.
- **Shield decay is a property of the shield pool, not of the granting node**, so L2's delay
  extension is a parameter push, not a second decay system.

**MISSING HOOK — `Instigator` on `FBreakerDamageRequest`.** Already named in
Ability-Implementation-Spec §7; restated because **three separate Tank systems block on it**: G3's
25% self-damage rate, O13's self-damage reduction on Breach Charge (the rocket currently "ignores
its instigator" as a stopgap), and B10's "damage from a source that is not you" break condition.

```cpp
// Combat/BreakerCombatTypes.h — FBreakerDamageRequest
UPROPERTY(EditAnywhere, BlueprintReadWrite) TWeakObjectPtr<AActor> Instigator;
```

**MISSING HOOK — incoming damage *cap* (Hold).** `PushIncomingDamageModifier` is a multiplier;
Hold needs a per-hit ceiling, which is a different operation:

```cpp
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PushIncomingDamageCap(FGameplayTag SourceTag, float MaxPerHitFractionOfMaxHealth);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PopIncomingDamageCap(FGameplayTag SourceTag);
```
Multiple caps compose as **min**, never as a product. Stated so a future second cap source cannot
multiply into immunity.

**MISSING HOOK — second input binding for "end ultimate early"** (Detonation). The only ability in
the game that needs one. `UBreakerInputConfig` gains `UltimateSecondary`; the ultimate base class
gains `bSupportsEarlyEnd` and `EndUltimateEarly()`. Flag for the input design pass — the binding
must be inert for every non-Detonation build, not a dead key.

**Also required, already specified elsewhere and reused unchanged:** `ABreakerDeployable` +
density cap (Ability-Implementation-Spec §2.7 — Anchor Point reuses the Gunsmith framework;
**build it once for Gunsmith, reuse it here, do not fork it**), `PushFlatDamageBonus` (§4.2 — used
by Provoke's solo conversion, B9 Conversion, D11 Chain Reaction; all three must land in the flat
sum stage before the Increased bucket), `GetAffixValue` (§4.4 — Bloodline's Life on Hit read),
`OnHitDealt` / `OnKillDealt` (SI-8 — melee kills, leech-on-hit, DoT-tick leech at proc
coefficient), `FBreakerMoreMultiplier` on the damage request (SI-7 — blocks all three Tank
keystones), and `UBreakerAbilityStateComponent` windows/streaks (SI-9).

### 6.1 Per-ability mapping

| # | Ability | GAS archetype | Net policy | Cost / CD GEs | Missing hooks |
|---|---|---|---|---|---|
| T1 | **Rend** | `_Instant` | `ServerOnly` (melee sweep resolves collision + damage) | `GE_Cost_Tank_Rend` (−25), `GE_CD_Tank_Rend` (6s) | Melee sweep (Ability-Impl §5.1, shared with Cleave); `ApplyHealing` + `PushOverhealRouting`; `OnHitDealt` for per-target leech. |
| T2 | **Bloodline** | `_Window` (8s) | `LocalPredicted` (window is a stat state; the healing inside is server-resolved) | −40 / 12s | `GetAffixValue(Affix.LifeOnHit)`; DoT ticks must route through `OnHitDealt` **with proc coefficient intact** or the "applies to DoT ticks at proc coefficient" clause silently becomes 1.0. |
| T3 | **Anchor Point** | `_Deployable` | `ServerOnly` (spawns an actor) | −30 / 10s | `ABreakerDeployable` + density cap (1 active for Anchor Point; placing a second destroys the first, **no refund** — §2). One-way projectile blocking needs a collision channel, not a per-projectile owner check. |
| T4 | **Provoke** | `_Instant` | `ServerOnly` (mutates other pawns) | −35 / 12s | **`ABreakerEnemy::ForceTarget` — does not exist (§6.0), blocking.** Solo conversion reuses `PushFlatDamageBonus` with a stack count from the number of enemies actually forced (query the return, do not count the overlap sphere — an immune or already-forced enemy must not grant a stack). |
| T5 | **Breach Charge** | `_Instant` (projectile) | `ServerOnly` (damage), impulse `LocalPredicted` | −30 / 8s | `Instigator` field; self-damage reduction with the **20% floor enforced in the damage library, not in the ability** (so no ability can bypass it); self-knockback impulse along aim vector (D2) via the movement component's existing impulse path. |
| T6 | **Ground Zero** | `_Instant` | `ServerOnly` | −45 / 10s | `ApplyStagger`; fall-distance query on the movement component; airborne gate (`IsFalling`), **not** a rocket-source gate — D8 restates why. |
| — | **HOLD** | `_Ultimate` (variant-resolving, D1) | `ServerOnly` | −100, **no cooldown GE at all** (author none — the HUD must distinguish "no cooldown" from "cooldown of zero", Ability-Impl §1.3) | `PushIncomingDamageCap`; `PushLoopOverride` for the tripled generation; Vein needs `ApplyHealing` on a timer keyed to absorbed damage; Wall needs an ally query + solo check (`UBreakerPartyPolicy` exists — use it, do not write a second one); Detonation needs an **absorbed-damage accumulator** on the ultimate instance and the early-end input binding. |

**Keystone Mores are passive conditional GEs granted by the keystone node** (D7), contributing
into `FBreakerMoreMultiplier` with `SourceTag = Keystone.Tank.Vein / Wall / Detonation`. **No Tank
ability authors a More.**

---

## 7. Defensive-invulnerability-loop audit (Master §7.10 risk 4)

Master 7.10.4 names parry refunds, dodge refunds, leech, overshields, and block combining into
permanent safety. O1 removed most of that surface by construction (no stamina to refund, no
defensive actions to chain — Core-Constellations §7 says so for Bulwark). **Tank is the class
where the remaining three — leech, overshield, and passive block — genuinely compose**, so the
audit is done here in full rather than deferred.

### 7.1 The loop, stated honestly

A fully invested Leech Tank, at IRONCLAD, holding Hold, with high Block Chance, running L9 +
L10 + L8 + Vein, has: healing routed to shield on every overheal, shield that does not decay,
shield absorption that heals after breaking, damage capped per hit, block procs feeding the
resource that pays for the next Hold, and 1.25× melee to keep the leech input flowing. **Written
as a list it looks like an infinite loop.** The guards below are what stop it, and they are
deliberately structural rather than numeric, because O2 means no numeric guard can be trusted yet.

### 7.2 The guards

| # | Guard | Where it lives | What it stops |
|---|---|---|---|
| **V1** | **Leech shield is capped at 25% of maximum health, and the cap is enforced once in `ApplyHealing`.** L1 raises the *conversion ratio*, L2 raises the *duration*, L9 raises the *number of sources* — **no node raises the cap.** | `ApplyHealing` / `PushOverhealRouting` (§6.0) | Unbounded shield accumulation. This is the single most important guard; it converts "infinite overheal" into "a second health bar with a ceiling." |
| **V2** | **Overheal generates no Grit; shield absorption generates at half rate (G2), sharing G1's 10/s cap.** | Grit component per-source caps (§1.2) | The leech→shield→Grit→Hold→leech cycle closing on itself. The resource cost of Hold cannot be paid faster by surviving harder. |
| **V3** | **Hold is a per-hit cap, not a reduction, and caps compose as `min`, never as a product.** | `PushIncomingDamageCap` (§6.0) | Hold + a future second mitigation source multiplying to zero. A cap can never reach immunity by composition. |
| **V4** | **Vein does not prevent damage.** It takes the damage and heals a fraction over time. A Tank in Vein can be killed by sufficient incoming DPS, and by burst before the heal-over-time lands. | Ultimate variant definition (§2.1) | The "leech ultimate = immortality" failure. Vein is a throughput trade, not a shield. |
| **V5** | **L10 Reciprocity's heal fires only *after* the shield breaks.** It cannot participate in an absorb→heal→re-absorb cycle within one shield's lifetime. | L10's node text, enforced in the shield-break event | The tightest inner loop available in the branch. |
| **V6** | **Hold costs a full bar (100), has no cooldown, and its tripled generation cannot refill it within its own duration.** Acceptance criterion §8.5 makes this a test, not a hope. | Ultimate cost + §1.2 caps | Chain-casting Hold. |
| **V7** | **Block is a passive chance layer (O1) and its Grit yield is capped at 8/s.** Block procs can never be induced, timed, or chained, and L6 raises yield inside a cap it cannot raise. | O1 + G4's per-source cap | Block becoming the loop's engine. Under O1, block *cannot* be an input at all — this guard is mostly free, and that is a benefit of the ruling. |
| **V8** | **No Tank node touches Parry, Dodge, or Block chance.** The Tank layer cannot stack with Bulwark's certainty conversion because it authors nothing in that space (§4.1). | The §4.1 territory division | The class layer and the constellation layer multiplying the same defensive quantity. |
| **V9** | **Tank abilities carry the longest cooldown band in the game (5–12s) *and* a Grit cost.** No Tank defensive effect has uptime approaching 100%. | §1.6 | Continuous defensive windows. |
| **V10** | **No Tank effect anywhere is damage immunity**, with the single exception of Detonation exempting the Tank from *its own* ultimate's release explosion — which deals no damage to the Tank in the first place, so it removes nothing. | Whole document | The category error that O13 forbids for rockets, applied to defense generally. |

### 7.3 The composition test (must be automated)

Ability-Implementation-Spec §7 asks for this as an automation test alongside the immunity-uptime
test. Its precise shape:

> **Configure the maximum-defense Tank** — Leech complete (L1–L12), Vein active, maximum Block
> Chance, maximum Life on Hit, Bloodline up, Rend on cooldown-perfect cadence, against a
> continuous same-level elite damage stream sized to kill a no-sustain character in 4–5s (O18).
> **Assert: the Tank dies.** Then assert time-to-death lands inside the "substantially higher"
> band — placeholder **3.0–3.5× the no-sustain TTD**. Both bounds matter: below the band the
> class is not a Tank, above it the loop is broken.

**Second test — the *combined* worst case:** the same Tank *plus* Bulwark's AEGIS (guaranteed
block conversion) *plus* an Anomalous defensive More. Assert the Tank still dies against the same
stream. This is the cross-layer case no single document owns and it is the one most likely to be
missed.

**Third test — cap composition:** assert that with Hold, a second incoming-damage cap, and any
multiplier source active, effective incoming damage is never zero for any positive input.

### 7.4 Residual risk, recorded not solved

- **Block-proc variance (O1/O2).** Grit inflow's floor and ceiling diverge with Block Chance. A
  high-Block-Chance Tank has meaningfully higher Hold uptime than a low one, and the *shape* of
  that curve is unknown until wave mode reports. Recorded, not solved (O2).
- **The 3.0–3.5× TTD multiplier is the least defensible number in this document.** It is the one
  place where "substantially higher" had to be turned into a figure, and it is the number most
  likely to move after measurement.
- **Wall's solo doubling is untested against party balance.** Halving the cap solo is a large
  swing and the party/solo parity has no measurement behind it.

---

## 8. Acceptance criteria

Extends Class-Kits §4's four criteria (1–4 below are those four, restated precisely); 5–12 are
new and specific to this treatment.

1. **A Tank with maximum Armour generates the same Grit per incoming hit as a Tank with none, at
   equal health lost.** Verify by driving identical post-mitigation damage through both.
2. **Self-damage cannot sustain a Grit loop:** 60 seconds of uninterrupted rocket-jumping with no
   enemies present must not fill the bar. Verify with D3 at both ranks — and note the *correct*
   result is that D3 makes it worse, not better.
3. **Provoke has a solo effect that is meaningful with zero allies.** Against a single enemy,
   Provoke must produce a measurable damage window.
4. **No combination of Leech nodes, Hold, and the passive Block layer produces indefinite
   survivability.** The §7.3 test, all three parts, automated.
5. **Hold cannot be re-cast within its own duration under any node combination.** Verify with the
   fastest known refill: Vein + L4 + L6 + B5 at maximum Block Chance inside a dense pack.
6. **No input pattern generates more than 20 Grit/s**, and no single source exceeds its §1.2
   per-source cap. Verify by driving every source simultaneously in the Gym.
7. **Proximity generation (G6) is provably count-independent.** One enemy at 4.9 m generates
   exactly what nine do. Assert on the component, not on observed values.
8. **A Tank with zero Block Chance has a complete, playable Grit economy** and reaches IRONCLAD at
   least once per normal encounter. If not, §0's consequence 2 is violated and block has become
   load-bearing.
9. **A Demolitionist who never damages themselves is viable.** Complete an elite encounter without
   a single self-damage instance, using D8's normal-jump Ground Zero. O13's "never required" made
   testable.
10. **Self-damage reduction never reaches 100%** under any combination of D3, D9, gear, and the
    ultimate. Assert the 20% floor in the damage library with a fuzz test over node/gear
    combinations.
11. **A Tank's effective damage multiplier from class sources never exceeds 1.30×.** Verify no
    second More has crept in via an ability, a shield conversion, or the ultimate. B9 Conversion
    and Provoke's stacks must appear in the **flat** bucket in the damage log.
12. **Equipping two Demolitionist abilities with a Leech keystone is legal and produces a
    coherent, non-degenerate character.** Cross-branch loadouts must not be punished by tree
    topology (O15).

### 8.1 Worked builds against 30 points

| Build | Spend | Reads as |
|---|---|---|
| Pure Leech | L1–L12 = 26, +4 into B1/B3 | Melee attrition brawler. 1.25× melee while shielded. Highest sustain, lowest range. |
| Pure Bastion | B1–B12 = 26, +4 into L1/L2 | Cover-and-threat controller. 1.20× near Anchor. The party role that is also a solo brawler. |
| Pure Demolitionist | D1–D12 = 26, +4 into L3/L5 | Point-blank explosive damage. 1.30× inner-plateau. Lowest sustain of any Tank build. |
| Leech / Demolitionist | L to 16, D1–D3 + D7 = 14 | Sustained brawler with a gap-closer and a stagger. No keystone. Probably the best generalist. |
| Bastion / Leech | B to 16, L1–L3 + L7 = 14 | Cover, threat, and enough leech to hold the ground you took. |
| Triple splash | Each branch to Tier 3 (10 each) = 30 | Three abilities available, two equippable, no rewrites. Deliberately flat, per §0.2. |

---

## 9. Compliance audit

| Rule | Source | Status |
|---|---|---|
| Block/dodge are passive chance layers; no stamina; no defensive input in the class layer | **O1** | **CONFIRMED.** Every block reference (G4, L6) is a proc-yield modifier. No node implies agency. No node references Parry. No node refunds a defensive cost, because none exists. |
| Value freeze | **O2** | **CONFIRMED.** One header flag; every magnitude is a placeholder; nothing re-authored. |
| More = unordered product, one per branch keystone, cap 3, ≤1.30× | **O3** | **CONFIRMED.** Exactly three: L12 Vein 1.25× (melee + shield-gated), B12 Wall 1.20× (Anchor proximity), D12 Detonation 1.30× (inner plateau). All on keystones. None exceeds 1.30×. A character holds at most one keystone (§0.2), so the class layer contributes at most one More. |
| Builds viable by mid-campaign; breadth over ceiling | **O4** | **CONFIRMED.** Six worked builds in §8.1; every branch is playable at Tier 3 with two abilities and no keystone. |
| Elements are Rift / Entropy / Void | **O19** | **N/A** — no Tank node authors elemental damage. Explosive damage is physical family; the resistance model does not gate this class. |
| Rocket: strong self-damage reduction, full knockback control, never immunity; jumping tolerated never required | **O13** | **CONFIRMED.** 80% reduction ceiling with a hard 20% floor enforced in the damage library (§5, §6.1 T5, criterion 10); full knockback control granted at Tier 1 free of any self-damage requirement (D2); every mobility node has a non-rocket trigger; criterion 9 tests "never required"; and §1.3 rule 2 makes self-damage a *worse* Grit source the more the branch is invested in. |
| Branch nodes freely mixed, no exclusive tiers | **O15** | **CONFIRMED.** Standard 5-tier / 26-point grammar, investment gates only. Criterion 12. |
| TTK/TTD seeds | **O18** | **CONFIRMED as the anchor**; the "substantially higher" figure is stated as a flagged placeholder (3.0–3.5×) rather than left implicit, per the brief. |
| REDESIGN bucket | **O20** | **CONFIRMED — this document adds nothing to the bucket.** No Tank node is built on a stamina spend or on block-as-an-action. §4.1 states the Bulwark division so no Tank node inherits a redesign item's problem. |
| Affixes scale verbs, trees rewrite rules, classes own the fantasy | Layer-Ownership | **CONFIRMED.** No node is a flat percentage of a stat the affix layer owns. Nodes that reference affixes (L3 Open Wound, T2 Bloodline via `GetAffixValue`, B9 Conversion) *read or re-rule* an existing affix. **B9 is the closest to the line** — it converts a defensive quantity into a flat damage contribution. It grants no shield and no damage stat; it is a conversion rule. Flagged in Open Questions. |
| No tree-granted movement or defensive verbs | Master 5.2 / 7.6 | **CONFIRMED.** Tank grants abilities and rule rewrites only. D2's knockback control modifies an *existing* explosive impulse; it grants no movement verb. Anchor Point and Ground Zero are abilities occupying loadout slots. |
| Crit is the only multiplier of its kind | Master 6.3 | **CONFIRMED.** No node rolls a chance to multiply damage. |
| No grapple / tether | Master 5.1 | **CONFIRMED.** Demolitionist repositions with impulses only. |
| Solo is the primary balance target | Master 11.1 | **CONFIRMED.** §1.7 and a per-branch solo conversion. G7 is the only ally source and no branch requires it. |
| Level cap 50, trees complete at 30 | Master 7.1 / 9.1 | **CONFIRMED.** No node scales with level. |
| Anti-stacking / explosion risks | Master 7.10.1, 7.10.5 | **CONFIRMED.** D4 Fragmentation is proc-coefficient-0 and cannot chain; D11 Chain Reaction is flat and capped at 3; B9 and Provoke are flat-bucket contributions; the shield cap is enforced once. |
| Invulnerability loop | Master 7.10.4 | **AUDITED in full — §7.** Ten structural guards, three automated tests. |

---

## 10. OPEN QUESTIONS

1. **Is the "substantially higher" TTD multiplier 3.0–3.5×, and is it a multiplier at all?** The
   O18 seed says "substantially higher with resources and sustain invested" and Tank is the class
   that defines the top of that range. A flat additive seconds target ("+10s over the no-sustain
   baseline") behaves very differently from a multiplier as encounter damage scales. **This is the
   single most important number wave mode must return for this class**, and the shape of the
   target — multiplier vs additive — is a design question that measurement will not answer on its
   own.
2. **Does the threat system stay an override, or does it become a real threat table?** Provoke as
   a forced-target override is the cheapest thing that works and the whole Bastion branch is
   authored against it. A threat *table* would let Bastion author much richer nodes (threat
   generation from damage taken, threat decay, threat transfer) — but it is a large system that
   would interact with every damage source in the game. **This is the largest scope decision in
   the class and it should be made before Bastion is built, not during.**
3. **Is B9 Conversion (shield → flat damage) a legal class-layer action?** It converts a
   defensive quantity into an offensive one. The flat bucket keeps it out of the Increased
   stack, and it grants neither shield nor a damage stat — but it is the closest node in this
   document to the affix layer's territory, and it is structurally similar to Swift's K8 Air
   Work, which Class-Kits OQ5 already flags from the other direction. Same question, opposite
   direction: is *conversion* a class-layer verb?
4. **Should the Leech shield cap (25% of maximum health) scale with anything?** It is currently a
   fixed fraction, which means it becomes proportionally weaker as encounter damage grows and
   proportionally stronger as max health grows. Fixed-fraction is the safest choice for V1 and
   the least interesting for build expression. Related: should `Maximum Shield` exist as an affix
   at all, or does that immediately break V1?
5. **Is a per-hit *cap* the right base ultimate, given that trash mobs die in under a second
   (O18)?** Hold does nothing against a swarm of small hits by design. Against O18's trash target
   that may mean Hold is a boss-only ultimate with two of three keystones (Vein, Detonation)
   fixing it. Correct texture, or a base ultimate that is dead half the time?
6. **Detonation's early-end input is the only second binding in the game.** Does one keystone
   justify a global input slot, or should early-end reuse the ultimate key with a hold/re-press
   grammar? An input-design question with a UI-UX-Spec dependency.
7. **Does Wall's solo doubling break party balance in the other direction** — is a solo Bastion
   now strictly better off *without* allies in radius? The rule as written creates a perverse
   incentive to stand away from the party. Should the solo bonus scale down smoothly with ally
   count instead of switching at zero?
8. **Grit's combat-entry grant (+15, §1.5) is a base-kit crutch for the Winded problem.** Is a
   free entry grant the right fix, or should the opening seconds genuinely be the Tank's weakest
   moment? Swift has no equivalent and starts every encounter at whatever the approach built.
9. **Does the 0.4s block-proc ICD survive contact with high attack-speed enemy packs?** At high
   incoming-hit rates the ICD, not Block Chance, becomes the binding constraint, which would make
   L6's ICD reduction the strongest node in the branch for reasons nobody designed. Measurement
   question (O2).
10. **Do three ultimate rewrites on a per-hit-cap base hold up, or is Hold three different
    ultimates wearing a coat?** Vein removes the cap entirely; Detonation adds a whole second
    phase. Class-Kits OQ8 asks this generally and Ability-Implementation-Spec D1 answers it
    structurally (tag-driven variants) — but Tank is the class where the three variants diverge
    most from the base, so if the pattern breaks anywhere it breaks here.

### Top three, if only three get answered

1. **The TTD multiplier (OQ1)** — it is the class's entire balance target and nothing else can be
   costed against it until it exists.
2. **Threat override vs threat table (OQ2)** — it is a scope decision that determines whether
   Bastion as authored is buildable at all.
3. **Is conversion a legal class-layer verb (OQ3)** — it recurs across B9, Leech's routing, and
   Support's healing-as-damage, so answering it once settles three classes.
