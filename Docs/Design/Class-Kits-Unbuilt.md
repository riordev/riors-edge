# Class Kits — THE THREE UNBUILT CLASSES, CONSOLIDATED

> STATUS 2026-08-16: PARTIALLY BUILT — the resource components and 21 ability rows it catalogues exist in code but are attached to nothing (HANDOFF §5 R3); where this consolidation and a full treatment disagree, the treatment wins.

**Scope:** post-slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Authority chain per O28: `Decisions.md` -> `Design-Overview.md` (map, not law) ->
the per-class treatments -> this document. `Master-Sheet-Import.txt` is
superseded and is never cited as authority.

> ## READ THIS BEFORE READING ANYTHING ELSE HERE
>
> **This document does not replace the three full treatments and must not be
> read as if it did.** `Class-Kits-Gunsmith.md`, `Class-Kits-Tank.md` and
> `Class-Kits-Support.md` are ~900 lines each, they are canonical for their own
> class, and they hold the thirty-six branch nodes, the compliance audits, the
> acceptance criteria and the open questions that this page deliberately does
> not restate. Where this page and one of them disagree, **that document wins**
> and this one is stale.
>
> This page exists because those three do not answer the question they are most
> often opened to answer: *what, of all this, actually exists in the build, and
> what happens if I select one of these classes today?* It is the reconciliation
> layer — one table per class of design against code — plus the cross-class
> comparison of the five resource loops that only becomes visible when they are
> read side by side.

**Every magnitude in this document is an O2 PLACEHOLDER, flagged once here and
not repeated per line.** Every number below is *quoted* from a class-kit
document that is itself under an O2 freeze banner; nothing on this page is
authored balance, and nothing on it may be treated as measured until wave-mode
instrumentation reports. Structure is the deliverable.

---

## 0. Status, in one table

The honest answer to "can I play these", stated before anything else, because
every one of the three treatments opens with **NOTHING IN THIS DOCUMENT IS
BUILT** and that is no longer quite true — a thin layer now is.

| Layer | Gunsmith | Tank | Support |
|---|---|---|---|
| Resource loop component | **BUILT** — `UBreakerScrapComponent` | **BUILT** — `UBreakerGritComponent` | **BUILT** — `UBreakerChargeComponent` |
| Resource loop reachable in play | **NO** — the component is inert unless the permanent class matches, and the class cannot be locked | **NO** | **NO** |
| Ability rows in the fallback registry | **BUILT (data)** — 7 rows | **BUILT (data)** — 7 rows | **BUILT (data)** — 7 rows |
| Any ability that EXECUTES | **NO** — every `AbilityClass` is null | **NO** | **NO** |
| Keystone ultimate variant rows | **BUILT (data)** — 3 each | **BUILT (data)** — 3 each | **BUILT (data)** — 3 each |
| Branch trees / the 36 nodes | **NOT BUILT** | **NOT BUILT** | **NOT BUILT** |
| Class definition (`GetFallbackClassDefinition`) | **nullptr**, deliberately | **nullptr** | **nullptr** |
| Selectable on the class screen | **NO, and must stay NO** (O39) | **NO** | **NO** |

**Why the loops were built before the abilities.** A resource loop is the one
part of a class that can be proven correct with no world, no ability, no enemy
and no ally — it is arithmetic with anti-farm rules attached. Building the three
loops first means the three hardest-to-retrofit decisions in each class (does the
bar accumulate or deplete; what refuses to generate; what the failure state is)
are settled and pinned by tests before any ability is written against them. The
reverse order is how a class ends up with abilities that need a loop shaped
differently from the one it has.

---

## 1. The five resource loops, side by side

This table is the reason this document exists. Each loop is a deliberate answer
to a different question, and three of the five answers are only legible against
the other two.

| | **Momentum** (Swift) | **Mana** (Caster) | **Scrap** (Gunsmith) | **Grit** (Tank) | **Charge** (Support) |
|---|---|---|---|---|---|
| Shape | A **state** | A **wallet** | A **ledger** | A **banked state with a lapse timer** | A **bank** |
| Starts | Empty | **Full** | Empty | Empty (+ a one-off combat-entry grant) | Empty |
| Idle income | No | **Yes — the primary path** | **None at all** | No | **None at all** |
| Decays | Yes, on a state (are you moving) | No | **Never, in or out of combat** | Yes, on a timer (6s lapse) | Only **out of combat**, and only down to a ceiling |
| Can go negative | No | **Yes — Overcast** | No | No | No |
| Spend gate | Cost **and** cooldown | Cost only — Mana *is* the cooldown | **Split: deployables cost only, personal abilities cooldown only** | Cost **and** the longest cooldowns in the game | Cost **and** cooldown |
| Global generation cap | 25/s | 6/s | 15/s | 20/s | 18/s |
| Per-source sub-caps | No | No | No | **Yes — damage 10/s, self-damage 3/s, block 8/s** | **Yes — self-heal 6/s** |
| Bands | Settled / Running / **Redline** | — | Dry / Stocked / **Surplus** | Winded / Braced / **IRONCLAD** | Cold / Attuned / **RESONANT** |
| Band the class is built around | Hold Redline permanently | — | Spend out of Surplus constantly | Hold IRONCLAD under pressure | **Reach** Resonant, spend, climb back |

**The three new shapes, and why each is what it is:**

- **Scrap accumulates and never decays, because its spend persists in the
  world.** It is the only loop with zero idle income, and that is the price of
  the only loop that never loses value: a class whose resource converts into
  objects that stay on the battlefield cannot be allowed to accrue that resource
  out of combat, or it arrives at every fight pre-armed. No decay is the
  compensation, and it makes Scrap a record of work already done rather than a
  thing you can save toward.
- **Grit accumulates and decays on a timer, because "am I doing my job" is a
  discrete signal for a Tank.** Swift's question is continuous (how fast are you
  going) and can be read every frame; a Tank's is an event (took a hit / enemy
  near), so the loop reads a 6s window that either of those refreshes. Inside the
  window Grit banks; outside it bleeds. More forgiving than Momentum, less than
  Mana, and the only loop of the five with per-source caps *under* the global
  one.
- **Charge accumulates, never decays in combat, and is clamped out of it.** It
  looks like Mana and behaves like its opposite: no passive regeneration at all,
  because the ultimate costs a full bar and a Support who banks one before the
  fight starts opens with the strongest thing they have. Decay would be the
  obvious answer and is refused, because Support's generation is the least
  self-directed of the five and a decaying bar punishes the player for a lull the
  enemy chose. The out-of-combat clamp does decay's job without decay's unfairness.

### 1.1 The band asymmetry, stated once

Momentum, Grit and Scrap all read even-ish thirds. **Charge does not** — Resonant
starts at three quarters, not two thirds. That is deliberate and is the clearest
single expression of how the two bars differ: Swift is built to *hold* its top
band and Support is built to *reach* its own, spend it, and climb back. Putting
Resonant immediately below the ultimate's full-bar cost creates a real "spend the
ultimate or hold the band for the node bonuses" tension that Swift's loop does not
have. It is pinned by a test so it cannot be tidied into thirds by someone making
the five loops look consistent.

### 1.2 Two guarantees that are properties of the C++ types, not of a clamp

Both count-independence rules — Grit's "one enemy at 4.9 m generates exactly what
nine do" and Charge's "buffing five allies generates exactly what buffing
yourself generates" — are implemented as functions taking a **bool**, with no
overload taking a count. A future caller cannot pass nine and be paid nine times,
because there is nothing to pass. That is a stronger guarantee than a clamp
somebody can delete, and it is why those two rules are the only anti-farm rules
in the corpus that need no test to defend them (they are tested anyway).

The same shape carries Charge's non-negotiable clause: **the pure healing rule
has no `bSelfTargeted` parameter at all.** The rate cannot differ between healing
yourself and healing an ally, because there is nothing for it to differ on. The
self/ally distinction exists exactly once, one level up, and only to route the
6/s self-heal sub-cap — which meters the *rate* and never the *rate per event*.

---

## 2. GUNSMITH — Scrap

**Identity, in one sentence:** the battlefield is a workshop — the Gunsmith's
power is *placed* rather than held, and their weakness is that placement takes
time they may not have.

**The loop.** Generation is entirely event-driven: kills (at the killing
instance's proc coefficient, so a DoT tick that lands the kill cannot make Tick
Frequency an engine), completed reloads (only if a round actually left the
magazine), emptying a full magazine, deployables being destroyed (a **50% refund,
never profit**), and damage dealt by deployables (per unit of damage actually
applied, so overkill pays nothing). Global cap 15/s. **Failure state:** an empty
bar, refused outright — there is no Overcast analogue to borrow against. The
class's floor is that this is survivable: both Armory abilities cost no Scrap, so
a Gunsmith at zero is still a functional shooter.

**The acceptance invariant the economy hangs on:** over a deployable's full
lifetime against a stationary target, its damage must not generate more Scrap
than the deployable cost. Placement has a positive *combat* return and a negative
*economic* one. If that inverts, the class farms itself.

**Branches.**

| Branch | Identity | Tension | Keystone | More |
|---|---|---|---|---|
| **Armory** | Never places anything; the gun in your hands is the project. The participation floor and the solo baseline. | Magazine vs reserve | MACHINIST | ×1.25 weapon damage while **no deployables are active** |
| **Field Tech** | Builds machines that work while you work — deployables that *produce* rather than punish. | Density vs durability | FOUNDRY | ×1.30 damage dealt by your deployables |
| **Tinkerer** | Prediction made mechanical; rewrites *trigger conditions* and rearm behaviour. | Arm time vs reward | MINEFIELD | ×1.20 inside a Disruptor field, or from a charge armed ≥10s |

**Abilities.** Note the two clocks — this is the class's defining ergonomic and
it is the one thing a casual edit is most likely to destroy.

| Ability | Branch | Cost | CD | Behaviour |
|---|---|---|---|---|
| **Sidearm Rig** *starter* | Armory | — | 10s | The next magazine deals bonus **flat** damage and gains +1 Pierce. Counted in **shots, not seconds** — it ends when the magazine empties or on reload. |
| **Overhaul** | Armory | — | 18s | For 10s, reserve ammunition converts into magazine capacity; the unspent remainder settles back. A bet that trades sustain for burst. |
| **Turret** *starter* | Field Tech | 40 | — | Autonomous emplacement, 30s. Consistent, never optimal — it does not lead perfectly and does not prioritise weak points, which is why you still hold a gun. |
| **Ammo Crate** | Field Tech | 30 | — | Reserve ammunition on a charge pool. Fully self-usable at full value; a party drains it faster, which is an efficiency difference and never a solo penalty. |
| **Mine Cluster** | Tinkerer | 35 | — | Three proximity charges on an arm delay. Counts as **one** placement, not three. |
| **Disruptor** | Tinkerer | 45 | — | A field that slows and strips a **flat** amount of armour. The class's only crowd control. |
| **FIELD ASSEMBLY** *(ultimate)* | — | 100 | — | Deploys every unlocked type at once and raises the density cap for 20s. "Unlocked" means node-purchased, not equipped — the one place the class rewards tree breadth over loadout choice. |

**Keystone rewrites of Field Assembly.** *Machinist* places nothing and instead
applies every unlocked type's effect to the player's own weapon and person — the
solo ultimate, and the thing that makes a zero-placement Armory build complete
rather than partial. *Foundry* pauses the lifetime clock on everything placed
during the window: permanent but bounded, because every later placement culls one
of the permanent ones. *Minefield* makes them invisible and excluded from enemy
perception until they act.

---

## 3. TANK — Grit

**Identity, in one sentence:** the only class that gets stronger by being hit,
without ever wanting to be hit more than necessary.

**The loop.** Post-mitigation damage taken is the spine, with shield absorption
paying half rate; a count-independent proximity trickle guarantees the loop runs
solo without requiring damage intake; melee kills are the aggression source that
stops it being purely masochistic; the passive block proc is a bonus lane.
Global cap 20/s, with per-source caps beneath it. **Failure state:** Winded —
opening an encounter at zero — which is a designed problem, softened by a
one-off combat-entry grant so the first seconds are not dead. Death sets the bar
to zero: no banking through a death, and no free ultimate on respawn.

**Three rules that are shapes rather than placeholders, and must survive
re-costing:**

1. **Generation is computed on POST-MITIGATION damage.** There is no
   configuration in which investing in mitigation increases Grit per hit. Without
   this, Armour is silently a Grit multiplier.
2. **Damage sources are capped at half the global cap.** A Tank therefore
   *cannot* reach the global rate by taking damage alone — filling the bar needs
   damage **plus** presence **plus** kills. Being hit is necessary and never
   sufficient, which is the fantasy sentence stated as arithmetic.
3. **Self-damage generates at a fraction, capped separately, and is never
   exempted anywhere.** Because nodes may reduce self-*damage* (O13) and reducing
   damage reduces the Grit it generates by rule 1, **rocket-jumping gets strictly
   worse as a Grit engine the more the player invests in the branch that rocket-
   jumps.** That direction is the point: it makes O13's "tolerated, never
   required" true at the resource layer and not only at the damage layer.

**O1, and why it bites hardest here.** The Tank has **no defensive input**. Block
is an RNG proc off a passive chance layer — never a stance, never a button — and
Parry belongs to the Bulwark constellation and is not part of this kit. The block
source is therefore gear-driven inflow the player cannot time, cannot bait and
cannot spend skill on, and its per-source cap exists so a node shortening the
proc's internal cooldown cannot quietly promote it into the loop's spine. **A
Tank with zero Block Chance must have a complete, playable economy**; block is a
lane, not the engine.

**Branches.**

| Branch | Identity | Keystone | More |
|---|---|---|---|
| **Leech** | Refuses to let a heal be wasted. Its grammar is *routing* — where healing goes when the bar is full, and what it becomes when there is nothing left to heal. | VEIN | ×1.25 melee damage while the Leech shield is active |
| **Bastion** | Changes where the enemy is looking and what the ground is shaped like. Attention and geometry, and the branch that must convert defensive investment into offence so a party role is never a solo penalty. | WALL | ×1.20 all damage while near your own Anchor Point |
| **Demolitionist** | Treats an explosion as a tool with two ends: one kills things, the other moves the Tank. The only Tank branch whose defensive answer is *not being there*. | DETONATION | ×1.30 explosive damage inside the blast's inner plateau |

**Abilities.** All cost Grit **and** carry a cooldown; the 5–12s band is the
longest in the game, because the invulnerability audit leans on cooldown *length*
as its primary guard. Shortening one of these is a safety change, not a feel one.

| Ability | Branch | Cost | CD | Behaviour |
|---|---|---|---|---|
| **Rend** *starter* | Leech | 25 | 6s | Melee sweep that heals for a portion of damage dealt; **overheal converts to shield**, capped, decaying. |
| **Bloodline** | Leech | 40 | 12s | Doubles Life on Hit for the window and extends it to DoT ticks at proc coefficient. Multiplies what you have and grants nothing if you have none — deliberately a *gear payoff*, and the ability that makes the item layer legible to the class. |
| **Anchor Point** *starter* | Bastion | 30 | 10s | A frontal cover panel with its own health pool. Blocks enemy fire; you and allies shoot through it from behind. |
| **Provoke** | Bastion | 35 | 12s | Forces nearby enemies to target you. **Solo conversion:** each one provoked grants stacking **flat** damage, so it is a real damage window against one enemy. |
| **Breach Charge** | Demolitionist | 30 | 8s | Thrown explosive. Full directional control of the self-knockback; self-damage heavily reduced and **never zero**. |
| **Ground Zero** | Demolitionist | 45 | 10s | Airborne slam scaling with fall distance, applying Stagger. Fully usable from a normal jump — which is what makes rocket-jumping *expressive* rather than mandatory. |
| **HOLD** *(ultimate)* | — | 100 | — | Caps the damage any single hit can do, and triples Grit generation for the duration. |

**A per-hit cap is not damage reduction, and the distinction is load-bearing.**
Hold does nothing against a stream of small hits and everything against a boss
slam: it is a *spike answer*, not a durability button, and multiple caps compose
as `min`, never as a product.

**Keystone rewrites of Hold.** *Vein* removes the cap and converts incoming
damage to healing at a reduced rate — a damped attrition window, **explicitly not
immunity**, and a Tank inside it still dies to enough incoming damage. *Wall*
extends the cap to nearby allies and, **solo with none in radius, doubles its
effectiveness on the Tank** — party and solo are two different good outcomes
rather than a party bonus with a solo penalty. *Detonation* ends early on command,
releasing the absorbed damage as a radial blast; it is the one self-damage
exemption in the class, and it is on the ultimate rather than on a repeatable
ability, so the rocket case O13 governs is untouched.

---

## 4. SUPPORT — Charge

**Identity, in one sentence:** force multiplication that works on a party of five
and on a party of one — a Support with no allies is one whose buffs, heals and
marks all land on a single target, themselves, at full value, and who converts
that value into damage through a mark.

**The loop.** Healing, shielding, damage to a marked target, buff uptime,
cleanses, and — the one party-exclusive source, deliberately the smallest of the
eight — assists. Global cap 18/s, lower than Swift's because part of the income
is time-based and a time-based source under a high cap is a passive drip.
**Failure state:** an empty bar with no allies, which the design treats as a
first-class case rather than an edge one — Mark is a starter, it is the cheapest
ability in the class, and damage to a mark generates, so the loop can always be
restarted from nothing. **Charge is never spent to zero involuntarily:** nothing
drains it, and the only ways it decreases are ability costs, the ultimate, and
the out-of-combat clamp.

**Solo, in one row.** Every source except assists yields identically solo and in
a party — buff uptime because it is count-independent by construction, healing and
shielding because self and ally pay the same rate by rule, marked damage and
cleanses because neither involves an ally at all. The party premium is one source
out of eight. That is what makes the class's balance target and its fantasy stop
fighting each other.

**O31, and the half of it that bites.** The ruling that raids are puzzles
rewarded for team play gives a Support-shaped contribution first-class standing —
but it also demands **felt player power**, and a build that participates by making
other people better does not automatically feel powerful. "Not unplayable" is a
floor on participation and is no longer sufficient. Warden — natively solo, the
recommended starting branch, and the one whose keystone More is unconditional
offence — is where the design already answers that, and under O31 that answer
stops being a hedge and becomes load-bearing. **Not resolved:** whether the
class's solo-damage acceptance criterion should gain an explicit felt-power
clause, and what would test it. Owner call.

**Branches.**

| Branch | Identity | Keystone | More |
|---|---|---|---|
| **Medic** | Asks *where does a heal go when the bar is already full* and answers it three ways — into a shield, into a damage bonus, into Charge. Highest raw generation, lowest raw damage. Every node functions with self as the target, at the same rate, without exception. | TRIAGE | ×1.20 weapon damage for a window after you heal anything, yourself included |
| **Conductor** | Cadence, tempo and propagation — rewrites *how buffs behave* rather than what they contain. Non-negotiable rule: **every Conductor buff applies to the Support first and to allies second.** Not "also"; *first*. | DOWNBEAT | ×1.25 weapon damage while one of your own buffs is live on yourself |
| **Warden** | Marks, suppression and control — plays the enemy rather than the ally. The natively solo branch, and the class's only unconditional-offence multiplier. | BLACKOUT | ×1.30 your damage against targets you marked |

**Abilities.** Costs run 20–40 and cooldowns 5–10s. Mark is deliberately the
cheapest and shortest, because it is the loop's ignition and a loop whose
ignition is expensive stalls at low Charge. Suppress is the most expensive
because it is the only ability with no generation attached.

| Ability | Branch | Cost | CD | Behaviour |
|---|---|---|---|---|
| **Patch** *starter* | Medic | 25 | 6s | Heals the ally under the crosshair, **or yourself with no target, at identical value**. Scales off the *target's* maximum health, so it is equally meaningful on a Tank and on a Caster. |
| **Purge** | Medic | 30 | 10s | Strips every status and grants brief status immunity. Self-castable. The immunity window is the whole value — the cleanse alone is not worth a slot. |
| **Cadence** | Conductor | 30 | 8s | A following aura improving reload and swap tempo for everyone inside, starting with the Support. Being a buff, it drives the count-independent uptime source. |
| **Metronome** | Conductor | 35 | 9s | Consecutive hits by any buffed target build a cadence ramp, **per holder** — the Support's own ramp is not shared or averaged. |
| **Mark** *starter* | Warden | 20 | 5s | Paints a target: it takes more damage from **all sources including allies**, and damage you deal to it generates Charge. The offensive conversion path, available in every build. |
| **Suppress** | Warden | 40 | 10s | A zone that slows and spoils accuracy. Deals no damage. The answer to being the squishiest class in a room it chose to hold. |
| **CONDUIT** *(ultimate)* | — | 100 | — | For 12s every Support ability affects every valid target in radius and costs nothing. Cooldowns still apply: breadth, not spam. |

**The valid-target set always includes the Support — unconditionally, in every
variant, at every party size.** If the target query ever excludes self, the solo
case silently does nothing and the class's ultimate stops existing solo. That is
a correctness requirement, not a balance choice.

**Keystone rewrites of Conduit.** *Triage* stops enabling free casts and becomes
a continuous healing field with one lethal-hit save per target — solo, a 12s
survival window with a guaranteed death save. *Downbeat* keeps the free casts,
doubles the cadence effects and adds **flat** weapon damage for every buffed
target (flat is load-bearing: it enters the flat-sum stage before the additive
Increased bucket and cannot double-dip with gear). *Blackout* marks and
suppresses every enemy in radius **instead of** casting abilities, and the marks
are yours for generation purposes — the one rewrite that turns a full bar
directly into damage.

---

## 5. What is DESIGNED and NOT BUILT

Listed as systems rather than as features, because in every case the missing
piece is a system and the class content is downstream of it.

### 5.1 Deployables and minions — O30, and the largest hole in the corpus

> O30, verbatim in its implementation notes: *"minions/deployables do not exist
> in any form. The Gunsmith kit designs them; nothing is built."*

No component, no actor, no density cap, no lifetime, no targeting, no placement
validation. This blocks **four of six Gunsmith abilities, the Gunsmith ultimate,
all three Gunsmith keystones, roughly twenty of its thirty-six nodes, and the
Tank's Anchor Point.**

The exposure is worse than a per-class blocker. O30 puts **MINIONS** on the Core
tree's build-axis taxonomy alongside GUNS and ABILITIES, and
`Class-Kits-Gunsmith.md` is the only place in the entire corpus that designs
them — so the axis O30 asks the Core tree to be organised around has its design
in one document and its implementation nowhere.

**Nothing in this pass fakes any of it.** `UBreakerScrapComponent` carries the
two deployable-facing generation rules as pure arithmetic with **no caller** —
the destruction refund and the per-damage credit — so the economy's shape is
pinned by a test before the system that will use it exists, and so a future
deployable cannot invent its own economy. No actor is spawned by anything in this
pass.

### 5.2 The party layer — Support's ally-facing half

No ally, no party membership, no ally query, no buff propagation, no mark
component. Healing exists but **does not report overheal as a separate
quantity**, and "overheal generates zero" is the Charge loop's single most
load-bearing anti-farm rule. Consequence: **the self-facing half of every Support
source could be built today and the ally-facing half could not** — the reverse of
the order the design implies. `UBreakerChargeComponent` therefore takes the
effective/overheal split as explicit parameters rather than binding a healing
delegate that cannot distinguish them, because binding it would credit the
exploit by construction.

### 5.3 Threat, stagger, and the damage instigator — Tank

Enemy AI has **no threat concept at all**, which blocks Provoke and with it the
whole Bastion branch. `ApplyStagger` does not exist, which makes Ground Zero's
hard CC unimplementable. **`Instigator` on the damage request does not exist, and
three separate Tank systems block on it** — the self-damage generation rate,
O13's self-damage reduction, and Bastion's forced-target break condition. That
last one is why `UBreakerGritComponent` does *not* bind the existing damage
delegate: doing so today would credit a rocket-jump at the full rate and make
rocket-jumping the cheapest Grit engine in the game, which is the exact outcome
the class's own anti-farm law forbids.

### 5.4 The branch trees — all thirty-six nodes per class

Not authored, and the reason is not time. `EBreakerBuildCondition` is
**movement-only**, so no node in any of the three classes can key off "while
shielded", "while an enemy is within 5 m", "while a buff is active", or "against
a marked target" — which is nearly every node all three classes want. O30 names
the same widening as a prerequisite for its own axes. Authoring the trees before
that lands would repeat the Caster branch outcome, where a whole class's nodes
ship as gameplay tags with no stat effect because the enum cannot express what
they do.

### 5.5 Elements — O38

Support's Conductor branch authors two element-adjacent nodes. **O38 puts
Elements post-slice**, so no elemental content is authored here, and the branch
is accepted as partially blocked rather than re-sited.

---

## 6. Against the north star

The owner's framing for this pass was *Destiny feel, PoE elements, cool
interactions, bigger number better person, intuitive progression.* Where the
three designs answer it, and where they do not yet:

**Interactions that combine rather than stack.** The strongest examples are all
cross-layer rather than within-class: Tank's Bloodline **multiplies a gear stat
and grants nothing without it**, which makes the item layer legible from inside
the class; Gunsmith's Sidearm Rig grants Pierce as a *rule* that stacks additively
into the one game-wide pierce ceiling rather than a parallel one; Support's mark
increases damage from **all sources including allies**, so it is a multiplier on
other people's builds. These are O33's four-avenue identity stack working:
class supplies the verb, gear supplies the magnitude, and neither is complete
alone.

**Bigger number, better person — bounded.** Each class spends exactly three More
multipliers, one per branch keystone, none above ×1.30, and a character holds at
most one keystone. So the class layer contributes **at most one More** to any
build, against the single ceiling of ≈2.197 (O34). The classes are not where the
number gets big; **gear depth is** (O29), and that is the intended division.

**Intuitive progression.** Each class's first hour teaches its whole loop through
two starters that sit on opposite sides of the class's tension: Gunsmith holds a
gun buff and a turret, Tank holds sustain and position, Support holds a heal and
a mark. No spreadsheet is required to read any of those pairs.

**Where it is weakest, recorded rather than solved.** Support under O31's felt-
power clause (§4). Tank under O30's taxonomy, which names three offensive axes
and **no defensive axis at all** — where a Tank's defensive identity lives if the
Core tree re-themes is genuinely unanswered, and the Tank is the class that hole
is largest for. Gunsmith's ramp: trash dies to the gun, an elite maybe justifies
one placement, and only a boss makes the field the fight — that ramp *is* the
class, and if it makes the Gunsmith the worst trash-clearing class, the fix is
Armory's floor and not the deployables.

---

## 7. What has to happen next, in order

1. **Widen `EBreakerBuildCondition`** beyond movement. Nothing in the three
   branch layers can be authored honestly until it lands, and O30's own axes
   need the same widening.
2. **`Instigator` on the damage request.** Cheapest item on the list, unblocks
   three Tank systems, and is a prerequisite for the Grit loop binding real
   events instead of explicit notifies.
3. **Healing with separated overheal reporting**, plus its shielding twin.
   Unblocks the Charge loop's self-facing half — which is most of it — and the
   whole Leech branch.
4. **The deployable system.** The long pole, and the one whose absence is a
   corpus-level problem rather than a class-level one (§5.1).
5. **Ability classes**, then class definitions, then O39. In that order and not
   another: a class definition registered before its abilities execute
   re-creates the exact bug that made every Caster ability read as locked.
