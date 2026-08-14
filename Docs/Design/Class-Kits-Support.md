# Class Kit — SUPPORT / Charge (full treatment)

**Last reconciled against: O32** (2026-08-14).

> ## NOTHING IN THIS DOCUMENT IS BUILT.
>
> Verified against the code on 2026-08-14, because a full-depth treatment reads
> like a specification and this one is not yet one:
>
> - **The Support is selectable and grants nothing.**
>   `UBreakerProgressionLibrary::GetFallbackClassDefinition` returns `nullptr`
>   for every class but Swift, so a locked Support has no class definition, no
>   starting abilities, no ultimate and no branch trees.
> - **No Support tree and no Support ability exist.** `GetAllFallbackTrees`
>   returns the Core slice plus Swift's three; the ability registry carries
>   Swift and Caster only.
> - **The Charge loop does not exist.** `Source/RiorsEdge/Classes/` holds two
>   components, Momentum and Mana. `ClassResource` is inert for a Support.
> - **The party layer this class is designed against does not exist either**,
>   and this is the gap that matters most here. There is no ally, no party
>   membership, no ally query, no buff propagation, and no `UBreakerMarkComponent`.
>   `ApplyHealing` exists, so the *self* path of every source could be built
>   today; **none of the ally-facing halves could be.** For this class
>   specifically, that means the solo baseline is buildable and the party role
>   is not — the reverse of the order the design implies.
> - **`EBreakerBuildCondition` is movement-only**, so no node here can key off
>   "while a buff is active", "against a marked target", or any other combat or
>   status state. **O30** names the same widening as a prerequisite for its own
>   axes. Effectively every Warden and Conductor node depends on it.
>
> Nothing here should be trusted as validated.

Status: design draft, **UNBUILT** (see above), authored to the depth of `Docs/Design/Class-Kits.md` §1 (Swift)
and §2 (Caster). This document **replaces** Class-Kits §5's one-page treatment as the
canonical Support design; §5 remains the source of the non-negotiable clauses, all of
which are carried forward verbatim and extended, never weakened.

> ## ⚠ ALL MAGNITUDES IN THIS DOCUMENT ARE PLACEHOLDER — O2 FREEZE
>
> **Every number below is an O2 placeholder.** Per `Docs/Design/Decisions.md` O2
> ("measure before authoring"), no value here may be treated as authored balance until
> wave-mode instrumentation reports. Numbers are chosen to be *plausible against the O18
> seed targets* — trash a little under 1s, rare/elite ~3s, boss 20–45s, TTD 4–5s with no
> resources/sustain and substantially higher with sustain invested — so that the loop can
> be reasoned about and prototyped, **not** because they have been measured. This single
> header flag stands in for a per-line annotation; do not re-flag individual values.
> Structure, rules, and rewrites are the deliverable. Magnitudes are not.

**Rulings this document obeys as law** (`Decisions.md`): **O1** (no stamina; block and
dodge are passive chance layers; Parry is the only defensive input), **O2** (value
freeze), **O3** (Mores are an unordered product; one per branch keystone; build-wide cap
3; Aberrant signatures may not author a More), **O4**, **O15**, **O18**, **O19** (the
elements are **Rift / Entropy / Void**; Void Whisperer is the Void specialist; saturated
teal is a property of objects, not of damage), **O21**, **O24**.

**Rulings this document was authored BEFORE, added 2026-08-14.**

- **O28.** `Master-Sheet-Import.txt` is **superseded** — historical source
  material, not law. Master citations record provenance only. Chain:
  `Decisions.md` -> `Design-Overview.md` (map) -> this document.
- **O30.** The taxonomy names GUNS, ABILITIES and MINIONS and has **no support
  or defensive axis at all**. Support is the class least served by it: healing,
  buff propagation, marks and suppression map onto none of the three. Where
  Support's identity lives if the Core tree re-themes onto those axes is
  unanswered, and it is a bigger hole here than for the Tank, whose damage-
  facing nodes at least land on GUNS. Recorded for the owner.
- **O31 is the most consequential ruling this class has received, and it cuts
  both ways.** O31: raids are **puzzles rewarded for team play**, and **every
  build must be able to make an impact and feel player power**; no encounter
  may have a build that cannot participate.
  - **The half that helps.** "Rewarded for team play" is the first ruling that
    gives a Support-shaped contribution first-class standing. Master 7.10.6's
    solo problem — the one this class is built around — is not withdrawn, but
    it is no longer the only lens.
  - **The half that bites.** Class-Kits §5's acceptance criterion 5 reads
    *"a Support's solo damage output is within 25% of the five-class median.
    Support may be the worst solo damage dealer; it may not be unplayable."*
    Under O31 that criterion is **necessary but no longer sufficient.** "Not
    unplayable" is a floor on *participation*; O31 also demands *felt player
    power*, and a build that participates by making other people better does
    not automatically feel powerful. The Warden branch — natively solo, the
    recommended starting branch, and the one whose keystone More is
    unconditional offence — is where this document already answers that, and
    under O31 that answer stops being a hedge and becomes load-bearing.
  - **Not resolved:** whether §5's criterion 5 should gain an explicit felt-power
    clause, and what would test it. That is an owner call and a playtest
    question, not a doc edit.

**Shared grammar inherited unchanged** from Class-Kits §0 / §0.1 / §0.2 / §0.3: 5-tier
branch shape, 12 nodes, 26 points per branch, 30 Class Points, one keystone per
character, two equipped abilities plus one ultimate, no node is a flat percentage, no
class tree grants a verb, resource range 0–100, per-source generation caps, generation
events carry a proc coefficient.

---

## 0. Corrections to Class-Kits §5, made here

Two things in the one-page treatment do not survive contact with the shared grammar.
Both are corrected here rather than silently carried.

1. **Three abilities were marked *starter* (Patch, Cadence, Mark); the grammar allows
   two** (§0.2: "two of them free at level 1, one from each of the two 'starter'
   branches"). **Ruling made here: the starters are Patch (Medic) and Mark (Warden).**
   Cadence moves to a Tier-3 grant in Conductor. Rationale: §5 already names Warden "the
   natively solo branch and therefore the recommended starting branch," and Medic is the
   branch whose fantasy is unreadable without a heal in hand. Conductor is the branch
   that reads *least* legibly in the first hour and is therefore the correct one to gate.
   Logged as OQ1 in case the owner prefers Cadence over Patch.
2. **§5 gives Support "4–10s cooldowns" and no per-ability values.** This document
   authors per-ability costs and cooldowns as O2 placeholders so that
   `Ability-Implementation-Spec` §11 GAP item 10 has a structural target to fill against.
   They are placeholders, not a lift of the O2 freeze.

Everything else in §5 — the Charge sources, the count-independence rule, the
self-at-full-rate rule, the branch names, the six ability names, CONDUIT, and the three
keystone names (Triage / Downbeat / Blackout, already tag-locked in
`Ability-Implementation-Spec` §1.1) — is honored exactly.

---

# 1. SUPPORT — Charge

**Fantasy.** Force multiplication that works on a party of five and on a party of one.
The Support does not win by out-damaging anything; it wins by making a fight resolve on
terms it chose — who is healthy, who is fast, who is marked. **The class's whole design
problem is that its fantasy is relational and its balance target is solo** (Master 11.1),
and Master 7.10.6 names it. Every rule in §1.1 exists to answer that.

**The one-sentence class read:** *a Support with no allies is a Support whose buffs, heals,
and marks all land on one target — themselves — at full value, and who converts that
value into damage through a mark.*

**Legibility in the first hour** (§0's class-selection-is-permanent constraint): the first
two abilities a Support ever holds are Patch (heal yourself or an ally at identical value)
and Mark (paint a target; it takes more damage and pays you Charge). That pair teaches the
entire class: *sustain generates, marks convert.*

## 1.1 The Charge loop

Charge is a 0–100 bar. Like Caster's Mana and unlike Swift's Momentum it is a **bank, not
a state** — it does not decay in combat. Unlike Mana it has **no passive regeneration at
all**: Charge is 100% event-driven, because a Support who banks a full bar before a fight
starts arrives with a free CONDUIT, and CONDUIT is the strongest opening the class has.

### Generation

| Source | Rate | Cap / anti-farm rule |
|---|---|---|
| **Healing done to an ally** | +1 per 3% of the *target's* maximum health restored | **Overheal generates nothing** — the heal must have moved the health bar. Percentage-of-target, not absolute, so healing a Tank is not a Charge engine. |
| **Healing done to SELF** | +1 per 3% of *own* maximum health restored | **Identical rate. This is the anti-7.10.6 clause and it is non-negotiable** (Class-Kits §5, carried verbatim). Overheal generates nothing here either. |
| **Shielding done to an ally** | +1 per 3% of the target's maximum health granted as shield | Shield granted *above* the target's shield cap generates nothing — the shield-side twin of the overheal rule. |
| **Shielding done to SELF** | +1 per 3% of own maximum health granted as shield | Identical rate. Same non-negotiable clause. **Threatened by the `Sealed` modifier — see §6.4.** |
| **Damage dealt to a MARKED target** | +1 per 2% of the target's maximum health dealt | **The offensive conversion path. Available in all three branches** because Mark is a starter ability, not a Warden-gated one. Percentage-of-target-max means a boss pays the same total for the same fraction, so a 20–45s boss (O18) yields a bounded, predictable bar. |
| **Buff uptime, any target including self** | +2/s while **at least one** Support buff authored by this Support is live on **at least one** target | **Count-independent by construction.** Buffing five allies generates exactly what buffing yourself generates. Implemented as a boolean (`HasAnyBuffActive()`), never a count — see §5.2. **Critical: without this, Support is a party class with a solo penalty.** |
| **Assist** — damage dealt to an enemy killed by an ally within 5s | +8 flat | **The only party-exclusive source in the loop, and deliberately the smallest.** It is an *efficiency* advantage, never a requirement. 1.0s internal cooldown. |
| **Cleanse / status removal** | +4 per status actually removed | 0.5s internal cooldown, and it must have removed a real status — cleansing a clean target pays nothing. The Medic-flavored twin of the overheal rule. |

**Global generation cap: 18 Charge per second from all sources combined.** Lower than
Swift's 25 because Support generation is partly time-based (buff uptime) rather than
wholly event-based, and a time-based source under a high cap is a passive drip.

**Proc coefficients apply** (§0.3). Healing over time ticks generate at the healing
source's proc coefficient, not at 1.0. A HoT that ticks ten times does not out-generate an
instant heal of the same total by an order of magnitude.

### Anti-farm rules — the full list

These are not optional and each one closes a named exploit:

1. **Overheal generates zero.** Closes "spam Patch on a full-health target forever."
   Requires `FBreakerHealResult::Overheal` to be reported separately (§5.1). This is
   Class-Kits §5 acceptance criterion 4 and it is structural, not a tuning value.
2. **Over-shield generates zero.** Closes the same exploit through the shield door.
3. **Buff uptime is a boolean, never a count.** Closes "stack five buffs on yourself for
   5× generation" *and* "party of five for 5× generation" in one rule. Count-independence
   cuts both ways and that symmetry is the point.
4. **Buff uptime does not credit outside combat.** Charge is combat-state gated per §0.3.
   Standing in the Anchor with Cadence running generates nothing.
5. **Self-healing credit is capped per second at 6 Charge/s** from the self-heal source
   alone, independent of the global cap. Closes the Life-on-Hit interaction: a Support
   running heavy Life on Hit affixes plus a high-cadence weapon is healing dozens of
   times a second, and without this cap the item layer becomes the class's primary
   generator — a Layer-Ownership violation (affixes scale verbs; they do not own the
   class loop).
6. **Marked-target damage credit requires the mark to be *yours*.** An allied Support's
   mark does not pay you. Closes double-Support mark-sharing.
7. **A target may be re-marked, but re-marking the same target within 3s does not refresh
   its Charge yield** — the mark's *damage* effect refreshes, the generation eligibility
   does not. Closes mark-flicker farming.
8. **Assists carry a 1.0s ICD and require real damage** (not a zero-damage proc), closing
   the "tag everything in the room for 8 apiece" pattern in a dense wave.
9. **Healing a target you are simultaneously damaging is legal and generates from both
   sources** — this is intentional, it is Warden's whole loop, and it is bounded by the
   18/s global cap.

### Decay and banking

**No decay.** Charge is a bank. The reasons are specific and each one is load-bearing:

- Support's generation is the *least* self-directed of the five classes — a decaying bar
  would punish the player for a lull the enemy chose.
- Decay plus the no-passive-regen rule would produce a class that can sit at zero through
  no fault of its own, which is the failure mode Master 7.10.7 names.
- The class's ultimate is a 100-cost commitment; a decaying bar makes the commitment a
  race rather than a decision.

**Banking is bounded instead**, by two rules that do the work decay would have done:

- **Out-of-combat clamp.** On leaving combat, Charge decays to a ceiling of **60** over
  4s and then holds. A Support may open a fight with a strong bar but never with a free
  CONDUIT. This is the only decay in the loop and it is *out-of-combat only* — it does
  not violate §0.3's "no resource decays in a menu, at a Forge, or in the Anchor" because
  in those states the clamp is already satisfied and never ticks below 60.
- **No pre-combat generation.** With no passive regeneration and combat-gated buff
  uptime, there is nothing to farm before the encounter starts.

### Named states

Three bands, displayed on the HUD as distinct states, matching Swift's precedent:

| Band | Range | Name | What reads it |
|---|---|---|---|
| Low | 0–33 | **Cold** | Nothing rewards Cold; it is the state the class is trying to leave. |
| Mid | 34–74 | **Attuned** | Conductor's buff-duration rewrites and MD5 read Attuned-or-better. |
| High | 75–100 | **RESONANT** | The spine band. Warden's mark rewrites, Medic's overheal routing, and Downbeat's condition all read Resonant. |

Resonant is *not* Redline. Swift is built to hold Redline permanently; Support is built to
**reach** Resonant, spend it, and climb back. The bar is a wind-up, and the ultimate is at
100 — so the band that matters most is the one immediately below the ultimate, which
creates a real "spend the ultimate or hold Resonant for the node bonuses" tension that
Swift's loop does not have.

### Spending

Support abilities cost Charge **and** carry a cooldown of **4–10s**, per §0.3's rule for
event-driven, spiky generation. Cost prevents the cooldown from being the only constraint;
the cooldown prevents a full bar from becoming five instant heals.

**Charge is never spent to zero involuntarily.** No node, ability, or enemy modifier drains
Charge — the only ways it decreases are ability costs, the ultimate, and the out-of-combat
clamp. This is a deliberate contrast with Swift and it is what makes the bank read as a
bank.

---

# 2. SOLO VIABILITY — the load-bearing section

This is the section the class exists to satisfy. **Master 7.10 risk 6 states the failure
condition directly: Charge generation cannot depend on allies.** Master 11.1 makes solo
the primary balance target. Class-Kits §5 acceptance criterion 1 makes it the single most
important number in the class.

## 2.1 The solo generation loop, in full

A Support alone in a room with enemies runs this loop with **zero allies and no party
sources whatsoever**:

```
             ┌──────────────────────────────────────────┐
             │  CAST A BUFF ON SELF (Cadence / Metronome)│
             │  → buff uptime source: +2/s, count-       │
             │    independent, identical to buffing five │
             └────────────────────┬─────────────────────┘
                                  │
                                  ▼
             ┌──────────────────────────────────────────┐
             │  MARK A TARGET (starter ability)          │
             │  → marked target takes increased damage   │
             └────────────────────┬─────────────────────┘
                                  │
                                  ▼
    ┌─────────────────────────────┴──────────────────────────────┐
    │  SHOOT THE MARK                                             │
    │  → damage to marked target: +1 per 2% of its max health     │
    │  → weapon Life on Hit / Life on Kill heals SELF             │
    │      → self-heal source: +1 per 3% of own max health        │
    │        (capped 6/s, anti-farm rule 5)                       │
    └─────────────────────────────┬──────────────────────────────┘
                                  │
                                  ▼
             ┌──────────────────────────────────────────┐
             │  PATCH / shield SELF when damaged          │
             │  → self-heal + self-shield at FULL ally    │
             │    rate. Non-negotiable clause.            │
             └────────────────────┬─────────────────────┘
                                  │
                                  ▼
                        RESONANT → CONDUIT
                 (solo: every self-buff runs at once)
```

**Three of the loop's four inputs are self-produced and require no target at all**
(buff uptime, self-heal, self-shield). The fourth (marked-target damage) requires an
enemy, which is correct — Support should not bank a bar in an empty room.

**Nothing in the loop requires an ally.** The single ally-exclusive source (assist, +8) is
the smallest number in the table by design.

## 2.2 The solo/party generation ratio — the number that matters

Class-Kits §5 acceptance criterion 1: *a solo Support fills the Charge bar within 20% of
the time a partied Support takes.* The structure that makes that achievable rather than
aspirational:

| Source | Solo yield | Party-of-five yield | Ratio |
|---|---|---|---|
| Buff uptime | +2/s | +2/s | **1.00** — count-independent by construction |
| Healing done | full rate on self | full rate on allies | **1.00** — identical rate by rule |
| Shielding done | full rate on self | full rate on allies | **1.00** |
| Marked-target damage | full | full | **1.00** |
| Cleanse | full (self-castable) | full | **1.00** |
| Assist | 0 | up to +8 per ally kill | **0** — the entire party premium lives here |

The party premium is therefore **one source out of eight**, and it is the smallest. The
20% criterion is a measurement of whether +8 assists, ICD-gated at 1.0s, exceed 20% of a
partied Support's total inflow in a real encounter. That is a wave-mode question (O2) and
it is the *only* one; the structural work is done.

**Note on the party premium's direction (Class-Kits OQ10):** the count-independence rule
means a five-player Support generates no faster than a solo one from buffs. This document
keeps that call and argues for it: the alternative — a modest per-ally efficiency edge —
reintroduces the exact failure mode the rule exists to prevent, one small step at a time.
The assist source already gives party play its edge and it is bounded and visible.
Carried to OPEN QUESTIONS as a live owner question, not reopened here.

## 2.3 Damage conversion — every branch has a path

Class-Kits §5 acceptance criterion 5: *a Support's solo damage output is within 25% of the
five-class median. Support may be the worst solo damage dealer; it may not be unplayable.*

The conversion architecture is: **Charge buys a state; the state converts to damage
through the mark, the buff, or the heal.** One path per branch, each with a different
shape, and **none of them requires an ally**:

| Branch | Conversion path | Requires an ally? |
|---|---|---|
| **Medic** | *Healing-as-damage.* MD10 (Blood Debt) routes a portion of healing done — including self-healing at full rate — into a bonus on the next weapon hit against a marked target. The Medic converts sustain into burst. | **No.** Self-healing is the same rate. |
| **Conductor** | *Cadence-as-damage.* Every Conductor buff applies to the Support first and to allies second (§4 identity clause). Solo, the whole branch is a personal weapon-handling suite: reload speed, swap tempo, and a per-hit cadence ramp. Downbeat converts cadence to flat weapon damage. | **No.** Self-first is the branch's defining rule. |
| **Warden** | *Marks-as-damage.* The natively offensive branch. Marks increase damage taken, propagate on death, and Blackout marks everything in radius and carries the class's unconditional-offense More. | **No.** Marks are cast on enemies, not allies. |

**Every branch also has an ally-free *survival* path**, because damage conversion is
worthless if the Support cannot hold the ground: Medic self-heals, Conductor's Metronome
buffs its own handling, Warden's Suppress reduces incoming accuracy. Class-Kits §5
acceptance criterion 2 ("every branch has at least one ability fully effective with zero
allies present") is satisfied twice over per branch.

## 2.4 The solo failure modes this design closes

| Failure mode | Closed by |
|---|---|
| "Charge only fills when someone else is hurt" | Self-heal and self-shield at identical rate; combat-gated buff uptime |
| "Buffs are dead weight solo" | Count-independent uptime; Conductor's self-first rule |
| "Support has no way to kill anything" | Mark is a **starter**, so the offensive conversion path is available at level 1 in every build |
| "Support banks a full bar before every fight" | No passive regen; out-of-combat clamp at 60 |
| "Support is only viable with Life on Hit gear" | Self-heal generation capped at 6/s so gear supplements the loop rather than becoming it |
| "The ultimate does nothing solo" | CONDUIT's valid-target query **includes self unconditionally** (§5.3) — solo, every self-buff runs at once |

---

# 3. Support abilities (6) + ultimate

Starters (free at level 1): **Patch** (Medic) and **Mark** (Warden). See §0 for the
correction that produced this pair.

| # | Ability | Branch | Cost | CD | Behavior |
|---|---|---|---|---|---|
| U1 | **Patch** *starter* | Medic | 25 Charge | 6s | Instant heal on the ally under the crosshair within 20 m, **or on self with no target**. Self-cast value is identical to ally-cast value — not reduced, not increased. Heals for a percentage of the *target's* maximum health, so it is equally meaningful on a Tank and on a Caster. Overheal is discarded (and generates nothing) unless MD9 routes it. |
| U2 | **Purge** | Medic | 30 Charge | 10s | Removes all statuses from the target and grants 3s of status immunity. Self-castable with no target. The immunity window is the *whole* value — the cleanse alone is not worth a slot. Generates from the cleanse source per status actually removed. |
| U3 | **Cadence** | Conductor | 30 Charge | 8s | 8s aura centred on the Support, 10 m radius: reload speed and weapon-swap tempo improved for everyone inside, **including and starting with the Support**. Follows the Support, does not persist at a location. This is a *buff*, so it drives the count-independent uptime source. |
| U4 | **Metronome** | Conductor | 35 Charge | 9s | 8s state in which every consecutive weapon hit by a buffed target — the Support first — adds a stacking cadence bonus, to a cap. Resets on a full second without a hit. Stacks are **per-target-of-the-buff**, so the Support's own ramp is not shared or averaged. |
| U5 | **Mark** *starter* | Warden | 20 Charge | 5s | Marks the target under the crosshair for 10s. Marked targets take increased damage from **all sources including allies**, and damage you deal to them generates Charge. The offensive conversion path and the class's most-used button. |
| U6 | **Suppress** | Warden | 40 Charge | 10s | 6s zone at the aim point, 6 m radius: enemies inside are slowed and have their accuracy reduced. Does not deal damage. The Support's answer to being the squishiest class in a room it chose to hold. |

**Cost and cooldown shape.** Costs run 20–40 and cooldowns 5–10s, keeping §5's band. Mark
is deliberately the cheapest and shortest — it is the loop's ignition, and a loop whose
ignition is expensive stalls at low Charge. Suppress is the most expensive because it is
the only ability with no generation attached.

## 3.1 ULTIMATE — CONDUIT

**Cost: 100 Charge (full bar). No cooldown; the cost is the cooldown.**

**Base behavior:** for 12 seconds, all Support abilities affect **every valid target within
15 m simultaneously** and cost **no Charge**. Cooldowns still apply — CONDUIT removes the
cost gate, not the cadence gate, so the window is a burst of *breadth*, not of spam.

**The valid-target set always includes the Support.** Unconditionally, in every variant,
in every party size. Solo, CONDUIT means every self-buff, self-heal and self-shield runs at
once; in a party it means the same effects fan out. **If the target query ever excludes
self, the solo case silently does nothing and the class's ultimate stops existing solo** —
this is a correctness requirement, not a balance choice (§5.3).

**Charge generation continues during CONDUIT.** Free casts still heal, still shield, still
buff, and still generate — which means a well-played CONDUIT partially refunds itself. It
cannot fully refund itself: the 18/s cap over 12s is 216 theoretical Charge against a 100
cost, but ability cooldowns bound how many generation events can actually be triggered
inside the window. **Acceptance criterion 6 (§7) tests exactly this.**

### Branch keystone rewrites — tag-driven variants

Per `Ability-Implementation-Spec` §1.1 (D1), each keystone grants a passive
infinite-duration GameplayEffect carrying one tag; CONDUIT reads its owner's tag container
at `ActivateAbility` and selects a variant row from `UBreakerUltimateDefinition`. **No
keystone grants, replaces, or blocks an ability.** Tags are already reserved:
`Keystone.Support.Triage` / `Keystone.Support.Downbeat` / `Keystone.Support.Blackout`.

| Keystone | Tag | CONDUIT becomes |
|---|---|---|
| **TRIAGE** (Medic) | `Keystone.Support.Triage` | CONDUIT stops enabling free casts and instead becomes a **continuous field**: every valid target in radius, self included, is healed each second for the duration, and **one lethal hit per target is prevented** during the window. The defensive ultimate. Solo, it is a 12s survival window with a guaranteed death save. |
| **DOWNBEAT** (Conductor) | `Keystone.Support.Downbeat` | CONDUIT's cadence effects are **doubled** and additionally contribute to weapon damage as a **flat** amount for every buffed target. Flat is load-bearing: it enters the flat-sum stage, before the additive Increased bucket, and therefore does not double-dip with gear (Item-Foundation aggregation rule). The offensive-tempo ultimate. |
| **BLACKOUT** (Warden) | `Keystone.Support.Blackout` | CONDUIT **marks and suppresses every enemy within radius** for its duration, instead of casting abilities. Marks applied this way are yours for generation purposes. The control ultimate, and the one that turns a full bar directly into damage. |

**Variant behavior that is not expressible as a GameplayEffect** — Triage's lethal-hit
prevention, Blackout's radius enumeration — lives as a named branch in the ultimate's C++
guarded by `Variant.KeystoneTag`, per D1's explicit allowance.

---

# 4. The three branches

Each branch is 12 nodes / 26 points on the §0.2 grammar:

| Tier | Gate | Nodes | Cost/rank | Max ranks |
|---|---|---|---|---|
| 1 — Entry | 0 | 3 | 1 | 2 |
| 2 — Loop | 3 | 3 | 1 | 2 |
| 3 — Ability | 6 | 2 | 2 | 1 |
| 4 — Rewrite | 10 | 3 | 2 | 1 |
| 5 — Keystone | 16 | 1 | 4 | 1 |

**Every node below is a rule rewrite or a resource-loop modifier. No node is a flat
percentage** (Layer-Ownership, restated in Class-Kits §0). Exactly one More multiplier per
branch, on the keystone only, none exceeding 1.30× (O3).

---

## 4.1 SUPPORT branch — MEDIC

**Identity.** Direct healing, cleanse, and the routing of healing that has nowhere to go.
Medic is the branch that asks *where does a heal go when the bar is already full* and
answers it three different ways — into a shield, into a damage bonus, into Charge. It is
the branch with the highest raw generation and the lowest raw damage, and its Tier-4 tier
is entirely about converting the former into the latter. **Solo, every Medic node functions
with self as the target**, at the same rate, without exception — that is the branch's
compliance statement and it is checked node by node in §7.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **MD1 — Field Dressing** | 1 | 2 | 1 | Healing done to **yourself** generates Charge at the ally rate even when the heal came from a *non-Support source* — Life on Hit, Life on Kill, regeneration, a health pickup. R2: also credits healing received *from* an ally. Loop modifier; the anti-farm cap of 6 Charge/s on the self-heal source is untouched and is what keeps this from becoming a gear engine. |
| **MD2 — Triage Priority** | 1 | 2 | 1 | Patch's heal value scales with how far below maximum the target is: it heals for more on a badly hurt target and less on a healthy one, at equal Charge yield. R2: the scaling extends to Purge's status-immunity duration. A rule rewrite of *where* the heal's value sits, not an increase to it — total throughput is unchanged, distribution is not. |
| **MD3 — Clean Hands** | 1 | 2 | 1 | Purge's cleanse generates Charge per status removed **and** refunds cooldown per status removed (R2: doubled refund). Explicitly bounded by the cleanse source's 0.5s ICD, so a target carrying six statuses does not refund six times instantly. |
| **MD4 — Steady Hands** | 2 | 2 | 1 | While at **Attuned** or better, Support ability cooldowns are reduced by a fixed amount each time you generate Charge from the *self*-heal source (R2: also from the ally-heal source). Caps at one reduction per second. The loop-closing node: sustain buys tempo. |
| **MD5 — Second Opinion** | 2 | 2 | 1 | Patch may be cast on a target that is at full health; it grants a **shield** instead of a heal, which generates from the shielding source (R2: the shield also applies to yourself at half value when cast on an ally). Turns the overheal-generates-nothing rule from a dead end into a decision. |
| **MD6 — Attending** | 2 | 2 | 1 | Healing a target that is **marked by you** additionally generates from the marked-target source at the damage rate (R2: and refreshes the mark's duration). Solo this fires when you heal yourself while a mark is live — the Medic/Warden bridge, and the reason a Medic build still wants Mark equipped. |
| **MD7 — Field Kit** | 3 | 1 | 2 | **Grants U2 Purge.** Purge additionally removes one enemy *buff* if cast on an enemy, and Purge's 3s immunity window also suppresses incoming status *application*, not merely existing statuses. |
| **MD8 — Sustained Care** | 3 | 1 | 2 | Patch becomes a heal-over-time on top of its instant portion, splitting its value. Ticks generate at the heal source's proc coefficient (§1.1), **not at 1.0** — this node must not out-generate the instant it replaces. No new ability granted; this is Medic's second Tier-3 node and it is a rewrite, mirroring Swift's F8/K8 shape. |
| **MD9 — Overflow** | 4 | 1 | 2 | **Overheal is no longer discarded.** It converts to a shield on the same target at a fraction of its value, and that shield generates from the shielding source. **This does not violate the overheal-generates-zero rule** — the overheal itself still generates nothing; the *shield it becomes* generates through the shield door, which has its own over-shield cap. The distinction is exact and it must be exact in code (§5.1). |
| **MD10 — Blood Debt** | 4 | 1 | 2 | Healing done — **including self-healing at full rate** — accumulates into a debt pool. Your next weapon hit against a **marked** target consumes the pool and adds it as **flat** damage. The Medic's damage-conversion path, and the node that makes a solo Medic a real character rather than a self-sustaining bystander. Flat bucket, before the Increased bucket, per Item-Foundation. |
| **MD11 — No Triage** | 4 | 1 | 2 | Patch and Purge can no longer target allies at all — they are self-only — and in exchange both cost substantially less Charge and their cooldowns are shortened. The deliberate solo-specialist rewrite, and a real *choice* rather than a bonus: it is a downgrade in a party and an upgrade alone. Mirrors the shape of Swift's F11 (No Safety). |
| **MD12 — TRIAGE (keystone)** | 5 | 1 | 4 | Rewrites CONDUIT (§3.1). **More multiplier (1 of 3): for 4s after you heal any target including yourself, weapon damage is multiplied by 1.20×.** Conditional on the branch's own loop, satisfiable solo by self-healing, and the lowest of the three because Medic has the highest generation. |

---

## 4.2 SUPPORT branch — CONDUCTOR

**Identity.** Cadence, tempo, and propagation — the branch that rewrites *how buffs
behave* rather than what they contain. Conductor's non-negotiable rule, carried from
Class-Kits §5: **every Conductor buff applies to the Support first and to allies second.**
Not "also to the Support" — *first*. Ordering is the branch's solo guarantee: if a buff
lands anywhere, it landed on the Support, so the count-independent uptime source is always
fed and every Conductor node is meaningful alone.

**Conductor is the elemental-adjacent branch.** Its Tier-4 tier authors *attunement* — a
buff that colors the buffed target's damage as **Rift, Entropy, or Void** (O19). This is
where Support touches the element system, and it is deliberately shallow: Conductor
**conducts** an element onto a weapon, it does not master one. Void Whisperer is the Void
specialist (O19) and nothing here competes with that; Multispell rotates all three and
nothing here competes with that either. Conductor's claim is narrower and different: *the
Support decides which element the party's guns are speaking this fight.*

**BLOCKED — the elemental nodes.** Following Core-Constellations' Elements convention
exactly, every elemental line here is tagged with its dependency and **may be authored on
paper but not as a Data Asset until its dependency tag clears.** Dependency tags used,
matching Core-Constellations §Elements:

- `[ELEM-RES]` — requires the elemental resistance model missing from Master 6.1 (whether
  it is one stat or three per-element stats is itself open).
- `[ELEM-PIPE]` — requires the resistance step inserted into the damage resolution order
  **after armour and before shields** (O5).
- `[ELEM-MATRIX]` — requires `UElementReactionDataAsset` and a reaction resolution order.

**Every non-tagged node in this branch functions today with no element system**, and
**no tagged node is a prerequisite for any untagged node.** Conductor ships in a
cadence-only form and gains its elemental layer later without a rewrite — the same posture
Void Whisperer and Multispell take in Class-Kits §2.4 / §2.5.

| Node | Tier | Ranks | Cost/rank | Effect | Blocked |
|---|---|---|---|---|---|
| **CO1 — Downbeat Discipline** | 1 | 2 | 1 | Conductor buffs applied to yourself last longer than the same buff applied to an ally (R2: longer still). The self-first rule expressed as duration, and the node that makes buff-uptime generation cheap to hold solo. | — |
| **CO2 — Section** | 1 | 2 | 1 | Cadence's aura radius grows, and its follow-behavior tightens so it does not lag the Support at sprint speed (R2: radius grows again). A handling rewrite; no magnitude on any stat the affix layer owns. | — |
| **CO3 — Sustain** | 1 | 2 | 1 | Buff-uptime Charge generation continues for a short grace period after the last buff expires (R2: longer grace). Loop modifier — it smooths the gap between casts so the +2/s source is not a sawtooth. Still combat-gated; still count-independent. | — |
| **CO4 — Rehearsal** | 2 | 2 | 1 | Re-applying a Conductor buff that is already live **refreshes** it rather than restarting its stack count, and refunds part of its cost (R2: larger refund). Explicit anti-stack rule, mirroring Caster's VW4 (Lingering). | — |
| **CO5 — Tempo** | 2 | 2 | 1 | Metronome's stack cap rises and its reset window lengthens for **you specifically** (R2: for all buffed targets). The self-first rule stated as a loop bonus. | — |
| **CO6 — Attunement** | 2 | 2 | 1 | Conductor buffs additionally attune the buffed target's weapon damage to one element chosen at cast — **Rift, Entropy, or Void** (O19). R2: the attunement persists for a short window after the buff ends. **Attunement converts the damage *type*; it adds no damage and no multiplier.** | **`[ELEM-RES]` `[ELEM-PIPE]`** — a type conversion is meaningless until resistances exist and until the resistance step sits in the damage order. Authored, not built. |
| **CO7 — Conducting** | 3 | 1 | 2 | **Grants U3 Cadence.** Cadence additionally improves ability *cooldown* recovery for buffed targets, and its aura persists on the Support for a short time after the Support leaves its own radius (which cannot happen — it is centred on them — but matters after CO11 detaches it). | — |
| **CO8 — Counterpoint** | 3 | 1 | 2 | **Grants U4 Metronome.** Metronome's per-hit stacks accrue from **any** damage the buffed target deals, not weapon hits alone — abilities, DoT ticks (at proc coefficient), and deployables all count. | — |
| **CO9 — Standing Ovation** | 4 | 1 | 2 | While at **Resonant**, all Conductor buffs are applied with their duration extended and cannot be removed by enemy effects. Band-gated rewrite; reads the state, adds no percentage to a stat. | — |
| **CO10 — Sympathetic Resonance** | 4 | 1 | 2 | A buffed target that is attuned to an element applies **elemental buildup** on hit at a rate independent of the damage dealt, and buildup applied by a target you buffed decays more slowly. **No reaction is triggered by this node** — it feeds the buildup track and stops there, deliberately, so it does not tread on Multispell or on Elements' Reaction lane. | **`[ELEM-BUILDUP]` `[ELEM-MATRIX]`** — requires a buildup track that accepts elemental writes; the status component records buildup but no elemental status writes to it (Core-Constellations, Elements). Authored, not built. |
| **CO11 — Detached Baton** | 4 | 1 | 2 | Cadence may be placed as a stationary zone at a location instead of following you, and while detached its radius is much larger. In exchange, **it no longer applies to you first** — the only node in the branch that suspends the self-first rule, and it is a deliberate party-play trade the solo player will decline. Explicitly flagged as such so the solo audit in §7 does not read it as a violation: **taking CO11 is optional, and no other Conductor node depends on it.** | — |
| **CO12 — DOWNBEAT (keystone)** | 5 | 1 | 4 | Rewrites CONDUIT (§3.1). **More multiplier (2 of 3): while at least one Conductor buff authored by you is live on yourself, weapon damage is multiplied by 1.25×.** *On yourself* is the condition and it is deliberate — the buff the Support always has is the one they gave themselves first, so this More is fully satisfiable solo and is not larger in a party. | — |

**Elemental scope note.** Conductor authors **no elemental damage of its own**, **no
reaction triggers**, and **no resistance manipulation**. It converts type (CO6) and feeds
buildup (CO10). That is the entire elemental surface, and it is drawn that narrowly on
purpose: Core-Constellations §7.10 risk 2 forbids a core or class node out-performing the
branch it overlaps, and both branches it could overlap (Void Whisperer, Multispell) are
Caster-side and deeper.

---

## 4.3 SUPPORT branch — WARDEN

**Identity.** Marks, suppression, debuffs, and control — the branch that plays the enemy
rather than the ally. **Warden is the natively solo branch** and per Class-Kits §5 it is
the recommended starting branch; Mark being a starter ability is the structural expression
of that. Warden owns the offensive conversion path in its purest form: a mark is a debuff
the Support applies to an enemy, it generates Charge when damaged, it increases damage
taken by everyone, and **none of that changes with party size.**

Warden's More is the class's only unconditional-offense multiplier, which Class-Kits §5
calls out as intentional: *it is the solo branch.*

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **WA1 — Painted** | 1 | 2 | 1 | Marked-target Charge generation applies to damage dealt by **your abilities and DoTs**, not weapon hits alone (R2: and to damage dealt by *allied* sources, at a reduced rate — a party bonus that adds nothing solo and therefore cannot become a solo dependency). Proc coefficients apply. |
| **WA2 — Long Watch** | 1 | 2 | 1 | Mark's duration extends, and re-marking a target before its mark expires does not consume the ability's cooldown (R2: extends further). Loop modifier — Warden's whole tempo is mark uptime. |
| **WA3 — Field of View** | 1 | 2 | 1 | Suppress's radius grows and its slow applies immediately on entry rather than after a delay (R2: the accuracy reduction also applies immediately). Handling rewrite. |
| **WA4 — Handoff** | 2 | 2 | 1 | When a marked target dies, the mark **jumps** to the nearest unmarked enemy within a radius (R2: larger radius). **Proc coefficient 0 on the jump** — the jump itself cannot generate Charge, which is what stops a dense wave from being a chain-generation engine. Mirrors Swift's M5 (Mark Economy) rule exactly, deliberately, so the shared `UBreakerMarkComponent` implements one rule not two. |
| **WA5 — Pressure** | 2 | 2 | 1 | Enemies inside Suppress generate Charge for you at a slow rate, **count-independent** — one enemy in the zone pays the same as six (R2: faster rate). The count-independence rule applied to an enemy-facing source for the same reason it applies to buffs: pack density must not be a resource multiplier. |
| **WA6 — Tell** | 2 | 2 | 1 | Marked targets have their next attack telegraphed to you, and damage they deal to you is reduced while the mark is live (R2: reduction extends to allies). A defensive node in an offensive branch — Warden's answer to being alone in the room it just aggravated. |
| **WA7 — Suppression** | 3 | 1 | 2 | **Grants U6 Suppress.** Suppress additionally reduces the Armour of enemies inside it by a **flat** amount — flat, never a percentage, protecting the boss cap called out in Master 7.10.5 and matching Caster's VW7 (Zonework) precedent. |
| **WA8 — Deep Mark** | 3 | 1 | 2 | Mark may be applied to a target already marked by you, deepening it: a deepened mark increases damage taken further and its Charge yield rate rises. **Anti-farm rule 7 still applies** — deepening does not refresh generation eligibility within its 3s window. No ability granted; Warden's second Tier-3 node is a rewrite. |
| **WA9 — Executioner's Ledger** | 4 | 1 | 2 | Killing a marked target refunds a portion of Mark's cost and part of its cooldown, scaled by how much of the mark's duration remained. Rewards marking a target you can actually kill, rather than marking everything. |
| **WA10 — Blackout Protocol** | 4 | 1 | 2 | While at **Resonant**, Suppress additionally prevents marked enemies inside it from receiving enemy buffs or being healed. Band-gated rule rewrite; no percentage. |
| **WA11 — Hunter's Economy** | 4 | 1 | 2 | Mark costs no Charge, but its duration is much shorter and it may only be held on **one** target at a time. The tempo rewrite: Warden trades reach for a free, constantly-cycling ignition source. The node that makes a zero-Charge Warden able to restart the loop from nothing — **the class's floor-recovery answer** and the reason a Support at 0 Charge is never soft-locked. |
| **WA12 — BLACKOUT (keystone)** | 5 | 1 | 4 | Rewrites CONDUIT (§3.1). **More multiplier (3 of 3): your damage against targets marked by you is multiplied by 1.30×.** Support's three Mores are now spent; no further node in this class may author one. The condition is self-supplied at level 1 by a starter ability, which is exactly why this is the branch the solo player is pointed at. |

---

## 4.4 Support — worked builds against 30 points

| Build | Spend | Reads as |
|---|---|---|
| **Pure Warden** | WA1–WA12 = 26, +4 into MD1/MD2 | The solo answer. 1.30× on marks, permanent mark uptime, self-heals to generate. The recommended first character. |
| **Pure Medic** | MD1–MD12 = 26, +4 into WA1/WA2 | Sustain-to-burst. 1.20× after any heal, Blood Debt converts self-healing into flat damage on marks. Slowest, safest, best boss-facing. |
| **Warden/Conductor hybrid** | WA1–WA7 to 16, CO1–CO3 + CO7 to 14 = 30 | Two abilities, one rewrite each side, no keystone. Cadence on self + permanent marks. The best generalist. |
| **Medic/Warden hybrid** | MD to 16, WA1–WA3 + WA7 = 14 | Heal-and-suppress. Holds ground; kills slowly. |
| **Triple splash** | Each branch to Tier 3 (10 each) = 30 | Four abilities available, two equippable, no rewrite. Deliberately the flattest shape, per §0.2. |
| **Pure Conductor** | CO1–CO12 = 26, +4 into WA1/WA2 | 1.25× while self-buffed. **Partially blocked** — CO6 and CO10 are the elemental nodes and do not function until `[ELEM-RES]`/`[ELEM-PIPE]`/`[ELEM-BUILDUP]` clear. Playable today at 10 of 12 nodes. |

---

# 5. GAS mapping and missing hooks

Archetypes are the `UBreakerGameplayAbility` subclasses locked in
`Ability-Implementation-Spec` §1.2 (D2). Cost and cooldown are always GameplayEffects
(D3); tag block follows D4; replication posture follows D5.

| # | Ability | Archetype | Net policy | Cost / CD | Tags |
|---|---|---|---|---|---|
| U1 | Patch | `_Instant` | `ServerOnly` (heals another pawn) | 25 / 6s | `Ability.Class.Support.Patch`, blocked by `Cooldown.Support.Patch` |
| U2 | Purge | `_Window` | `ServerOnly` | 30 / 10s | `State.Ability.Purge`; grants `State.StatusImmune` for the window |
| U3 | Cadence | `_Window` + buff | `ServerOnly` (mutates other pawns) | 30 / 8s | `Window.Support.Cadence`; `Buff.Support.Cadence` on targets |
| U4 | Metronome | `_Window` + buff | `ServerOnly` | 35 / 9s | `Window.Support.Metronome`; `Streak.Support.Metronome` on SI-9 |
| U5 | Mark | `_Instant` | `ServerOnly` (mutates a target's mark state) | 20 / 5s | `Mark.Support.Warden` on `UBreakerMarkComponent` |
| U6 | Suppress | `_Zone` | `ServerOnly` | 40 / 10s | `Zone.Support.Suppress` on `ABreakerZoneActor` |
| — | **CONDUIT** | `_Ultimate` | `ServerOnly` | 100 / — | variant resolved from `Keystone.Support.*` |

**No Support ability is `LocalPredicted`.** Every one of them either heals, buffs, marks,
or debuffs another actor, and D5 is explicit that anything mutating another pawn is
`ServerOnly`. The one thing that *is* predicted is the **cost**: `ClassResource` spend
predicts client-side so the HUD bar does not rubber-band, with the server as truth.

## 5.1 Missing hook — healing with overheal reporting *(named in the spec, blocking Medic entirely)*

`Ability-Implementation-Spec` §5.4 already names this as a missing hook. Restated here
because **the entire Medic branch and two Charge sources depend on it**:

```cpp
// Combat/BreakerCombatComponent.h
USTRUCT(BlueprintType)
struct FBreakerHealResult
{
    UPROPERTY(BlueprintReadOnly) float HealthHealed  = 0.f;
    UPROPERTY(BlueprintReadOnly) float Overheal      = 0.f;  // Support Charge MUST generate 0 from this
    UPROPERTY(BlueprintReadOnly) float ShieldGranted = 0.f;  // MD9 Overflow / Leech routing
};
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
FBreakerHealResult ApplyHealing(float Amount, AActor* Healer, FGameplayTag SourceTag);
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHealEvent OnHealed;
```

**Three requirements this document adds to that signature:**

1. **`Healer` must be populated on every path, including self-heals from gear.** MD1
   (Field Dressing) credits Life on Hit healing to the Charge loop; if a weapon's leech
   calls `ApplyHealing` with a null healer, MD1 is unimplementable.
2. **`HealthHealed` and `Overheal` must sum to `Amount`, always.** The Charge component
   reads `HealthHealed` only. That is how "overheal generates zero" becomes structurally
   true rather than a thing to remember.
3. **MD9 (Overflow) routes `Overheal` into `ShieldGranted` and the shield generates
   through the *shielding* source, which has its own over-shield clamp.** The overheal
   itself still contributes zero. Any implementation that credits Charge directly from
   `Overheal` fails acceptance criterion 4 (§7) even though the player-facing behavior
   looks identical.

**Also required: a shielding twin.** Nothing in the codebase reports shield application
with an over-cap remainder.

```cpp
USTRUCT(BlueprintType)
struct FBreakerShieldResult
{
    UPROPERTY(BlueprintReadOnly) float ShieldApplied = 0.f;
    UPROPERTY(BlueprintReadOnly) float OverShield    = 0.f;  // generates 0, same rule as Overheal
};
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
FBreakerShieldResult ApplyShield(float Amount, AActor* Source, FGameplayTag SourceTag);
```

## 5.2 Missing system — ally targeting

**Nothing in the codebase can answer "who are my allies."** Every Support ability except
Mark and Suppress needs it, CONDUIT's radius query needs it, and the assist source needs
it. This is a Support-shaped hole that Tank's Wall keystone also falls into.

```cpp
// Source/RiorsEdge/Combat/BreakerTargetingLibrary.h  (new)
UFUNCTION(BlueprintPure, Category="Targeting")
static bool IsAlly(const AActor* A, const AActor* B);          // solo: self is always an ally of self

UFUNCTION(BlueprintCallable, Category="Targeting")
static void QueryAlliesInRadius(const AActor* Origin, float Radius,
                                bool bIncludeSelf,               // CONDUIT passes TRUE, unconditionally
                                TArray<AActor*>& OutAllies);

// Crosshair ally resolution for Patch/Purge. Returns Origin itself when nothing
// friendly is under the crosshair — this is what makes every Medic ability self-castable
// with no target, which is a design requirement, not a fallback convenience.
UFUNCTION(BlueprintCallable, Category="Targeting")
static AActor* ResolveFriendlyTarget(AActor* Origin, float MaxRange);
```

**`bIncludeSelf` must default to true and CONDUIT must never pass false.** §2.4's failure
mode "the ultimate does nothing solo" is exactly one wrong boolean away.

## 5.3 Missing system — auras and zones

Cadence is an aura (follows the caster, re-evaluates membership continuously); Suppress is
a zone (static, placed). `Ability-Implementation-Spec` §2.7 lists `ABreakerZoneActor` /
`UBreakerZoneComponent` as needed by Rot, Suppress, and Disruptor — Support adds the
*following aura* variant, which the zone actor does not currently cover.

```cpp
// Source/RiorsEdge/Abilities/BreakerAuraComponent.h  (new)
// A radius that follows its owner and maintains a membership set with enter/exit events.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void OpenAura(FGameplayTag AuraTag, float Radius, float Duration);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void CloseAura(FGameplayTag AuraTag);
UPROPERTY(BlueprintAssignable) FBreakerAuraMembershipChanged OnMemberEntered;
UPROPERTY(BlueprintAssignable) FBreakerAuraMembershipChanged OnMemberExited;
```

Membership churn must be **event-driven, not polled per tick per member** — a 10 m aura in
a dense wave is the worst-case tick cost in the class. CO11 (Detached Baton) is the one
node that converts the aura into a static zone, so the two systems must be able to swap
under one ability.

## 5.4 Missing system — buffs with count-independent uptime

Restated from `Ability-Implementation-Spec` §8, because it is the mechanical guarantee
behind acceptance criterion 3:

```cpp
// Source/RiorsEdge/Classes/BreakerBuffComponent.h  (on the Support)
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
void ApplyBuff(FGameplayTag BuffTag, const TArray<AActor*>& Targets, float Duration);

// TRUE if at least one buff from this Support is live on ANY target including self.
UFUNCTION(BlueprintPure) bool HasAnyBuffActive() const;
```

**The Charge component calls `HasAnyBuffActive()` and never a target count.** Written as a
boolean, count-independence is not a rule to be tested — it is a fact about the type.

## 5.5 Per-ability hook list

| Ability | Missing hooks |
|---|---|
| **U1 Patch** | `ApplyHealing` + `FBreakerHealResult` (§5.1); `ResolveFriendlyTarget` (§5.2); `ApplyShield` for MD5/MD9; a HoT driver for MD8 that ticks at proc coefficient |
| **U2 Purge** | `UBreakerStatusComponent::CleanseAll()` returning a *count of statuses actually removed* (the cleanse Charge source and MD3 both read the count, not a bool); `PushStatusImmunity(FGameplayTag, float)`; MD7 additionally needs enemy-buff removal, which needs an enemy buff registry that does not exist |
| **U3 Cadence** | `UBreakerAuraComponent` (§5.3); `UBreakerBuffComponent` (§5.4); `UBreakerWeaponComponent::PushReloadSpeedMultiplier` / `PushSwapSpeedMultiplier` (tag-keyed push/pop, same shape as the existing `PushCadenceMultiplier` request); CO7 needs `UBreakerAbilityComponent::ReduceCooldown` |
| **U4 Metronome** | `UBreakerBuffComponent`; SI-9 `UBreakerAbilityStateComponent` streaks **keyed per buffed target, not per Support** — the shipped streak API is per-target-of-damage and needs a per-holder variant; `PushCadenceMultiplier`; CO8 needs SI-8 `OnHitDealt` fan-out to non-owner actors |
| **U5 Mark** | `UBreakerMarkComponent` (shared with Swift's S6 Lead, different `MarkTag`) — must record **mark ownership** so anti-farm rule 6 is enforceable, and must implement the proc-coefficient-0 jump once for both WA4 and M5 |
| **U6 Suppress** | `ABreakerZoneActor`; `PushSpeedMultiplier`; `ABreakerEnemy::PushAccuracyMultiplier(FGameplayTag, float)` / `Pop`; WA7 needs the flat-armour-reduction path already required by Caster's Rot |
| **CONDUIT** | `QueryAlliesInRadius(bIncludeSelf=true)`; `UBreakerCombatComponent::PushLethalDamagePrevention(FGameplayTag, int32 Charges)` resolved in `ReceiveDamage` **before death is broadcast** (Triage); `PushFlatDamageBonus` (Downbeat); mass-mark enumeration (Blackout) |
| **Charge loop itself** | `UBreakerChargeComponent` binding `OnHealed` / `OnShielded` / `OnHitDealt` (SI-8) / `HasAnyBuffActive` / mark-damage events / `GetRecentDamagers(float WithinSeconds)` for assists, which needs `FBreakerDamageRequest::Instigator` (spec §11 dependency 9) |

## 5.6 Data assets

Mirroring Class-Kits §7's shape:

- `DA_Class_Support` — ClassId, Charge attribute binding, three branch references, two
  starter abilities (Patch, Mark), one ultimate.
- `DA_Branch_Medic`, `DA_Branch_Conductor`, `DA_Branch_Warden` — node lists, tier gates,
  cost curve.
- **`DA_ChargePolicy`** — per-source generation rates, per-source caps (including the 6/s
  self-heal cap), the global 18/s cap, the out-of-combat clamp ceiling and rate, band
  boundaries. **All Charge tuning lives here, not in C++**, and it is the surface the O18
  seed targets are tuned against once wave mode reports. Per O2 no value in it is
  re-authored before that report.
- `DA_Ultimate_Conduit` — `UBreakerUltimateDefinition` with four variant rows (base +
  three keystones), per D1.

---

# 6. Compliance audit

## 6.1 Crit policy — Master 6.3 CONFIRMED

**No node, ability, or resource rule in this document rolls a chance to multiply damage.**
There is no "chance to double-heal", no "chance to apply the mark twice", no parallel
roll-and-multiply of any kind. Mark increases damage *taken* — that is a target-side
Increased contribution, not a multiplier of its own kind. The three keystone Mores are
deterministic conditional multipliers, which is the sanctioned form.

Support authors **no node that raises Critical Chance** — even the sanctioned route Caster
uses in VW9. Deliberate: a Support that buffs allied crit chance is a party-scaling node
whose solo value is a fifth of its party value, which is the exact shape §2 exists to
forbid.

## 6.2 Verb ownership — Master 5.2 CONFIRMED

**No Support node grants walk, sprint, jump, crouch, dash, slide, wall ride, wall jump,
air jump, or Parry.** Air jump (Kinesis) and Parry (Bulwark) remain the only tree-granted
verbs and both are Core Tree grants, not class grants. Per **O1**, block and dodge are not
verbs at all — they are passive chance layers — so there is nothing for this tree to grant
and no node here reads them as inputs.

Cadence and Suppress occupy *ability slots*; neither is a base-kit addition. A Support who
equips neither has base-kit mobility and base-kit defense exactly.

**Affix-layer compliance** (Layer-Ownership): the nodes that touch affixes — MD1 (reads
Life on Hit / Life on Kill healing as a Charge source), WA1 (reads ability and DoT damage
against a mark) — **read** the affix layer, they do not duplicate it or grant its
capabilities. MD1 is the closest to the line and is called out in OPEN QUESTIONS by analogy
to Swift's K8.

## 6.3 O-ruling compliance

| Ruling | Status |
|---|---|
| **O1** — no stamina; block/dodge passive; Parry only defensive input | **CONFIRMED.** No Support node reads a block or dodge *input*. No node reads a block or dodge proc either — Support is the only class whose loop does not touch the passive defense layer at all, so it carries none of the RNG-variance risk Swift and Tank carry. |
| **O2** — value freeze | **CONFIRMED.** Header flag; every magnitude is a placeholder; `DA_ChargePolicy` is the re-anchoring surface. |
| **O3** — Mores unordered product, one per keystone, cap 3, no Aberrant Mores | **CONFIRMED.** Exactly three Mores, all on keystones (MD12 1.20×, CO12 1.25×, WA12 1.30×), none above 1.30×. A character holds one keystone (§0.2), so the class layer contributes **at most one** More. Class-Kits §6.1's "Support — TBD" row is now fillable. |
| **O4** — viable by ~level 25, breadth over ceiling | **CONFIRMED.** Warden is playable from its first node; the starter pair teaches the class in the first hour; six worked builds in §4.4. |
| **O15** — branch nodes freely mixed with investment gates | **CONFIRMED.** No mutually exclusive tiers; a Medic keystone with two Conductor abilities equipped is legal. |
| **O18** — TTK/TTD seeds | Placeholders authored *against* the seeds, not measured. Support's contribution to the TTD side (4–5s bare, substantially higher with sustain) is the branch that most directly moves that number, and §7 criterion 8 tests it. |
| **O19** — elements are Rift / Entropy / Void; Void Whisperer is the Void specialist; saturated teal is a property of objects, not damage | **CONFIRMED.** Conductor names all three elements and masters none. No node claims Void specialization. No Support VFX may use saturated teal for *damage*; mark and aura VFX are objects and may. |
| **O21** — the three promoted nodes | Not applicable — constellation layer. |
| **O24** — overgrown Earth | Presentation note only: Support VFX read as *signal* against vegetation and weathered tech — marks as a scan overlay, Cadence as a ground-level pulse. No mechanical consequence. |

## 6.4 The Sealed / Bare modifier threat — which Charge sources are at risk

`Game-Modes.md` Class C audit records two unresolved owner calls that both point at this
class. Restating precisely **which sources they threaten**, so the owner call has a
concrete target:

| Modifier | Frontier effect | Charge sources threatened | Severity |
|---|---|---|---|
| **Sealed** — "Shields do not recharge inside the Frontier" | If read as *shield regeneration only*: **no source affected.** If read as *no shield may be applied*: it deletes **"Shielding done to self"** — a named-mandatory, non-negotiable anti-7.10.6 source — and **"Shielding done to an ally"**, and it disables **MD5 (Second Opinion)** and **MD9 (Overflow)** entirely, turning two Medic nodes into dead picks and removing the Medic branch's overheal-routing answer. | **CRITICAL under the broad reading.** It removes a source Class-Kits §5 calls non-negotiable. **This document's position: "recharge" must be pinned to regeneration-only.** Owner call; recorded, not made here. |
| **Bare** — "No healing from Life on Hit or Life on Kill; regeneration only" | Under the narrow, affix-only reading: **MD1 (Field Dressing) rank 1 loses its Life-on-Hit / Life-on-Kill input** — a node-level degradation, comparable to Swift's F8 under `Dry`, not a loop kill, since MD1 still credits Patch, MD5, MD9 and regeneration. Under the broad reading ("no healing except regeneration"): it zeroes **"Healing done to allies"** *and* **"Healing done to self"** — the two largest sources in the table — and guts the entire Medic branch, MD10's Blood Debt pool included. | **CRITICAL under the broad reading, ACCEPTABLE under the narrow one.** **This document's position: the narrow affix-only reading must be normative.** Owner call; recorded, not made here. |
| **Exposed** — re-scoped: block/dodge do not mitigate but still generate | **No Support source affected** — Support's loop does not read the passive defense layer at all (§6.3). Noted for completeness. | None. |

**Design consequence if either broad reading is ever adopted:** Support would need a
modifier-specific exemption, and *"a Frontier modifier that class-selectively deletes a
named-mandatory generation source"* is exactly what the Class C FORBIDDEN list is trying
to avoid on the other side of the ledger. The clean resolution is the narrow reading of
both, and this document is written assuming it.

---

# 7. Acceptance criteria

Criteria 1–5 are Class-Kits §5's, carried verbatim and made testable. 6–12 are new and
specific to the full treatment.

1. **A solo Support fills the Charge bar in a normal encounter within 20% of the time a
   partied Support takes.** *The single most important number in the class.* Measure in
   wave mode with an identical encounter, one Support alone versus one Support with four
   allies, identical gear, five runs each.
2. **Every branch has at least one ability that is fully effective with zero allies
   present.** Medic: Patch self-cast. Conductor: Cadence self-first. Warden: Mark and
   Suppress are enemy-facing and party-agnostic.
3. **Buff-uptime generation is provably count-independent.** Verify by measuring generation
   with buffs live on 1 target and on 5; the values must be *identical*, not close. Because
   the source reads `HasAnyBuffActive()`, this should be true by construction — if it is
   not, the Charge component is reading a count and must be fixed, not tuned.
4. **Overheal generates zero Charge under all node combinations.** Verify with MD1 + MD5 +
   MD8 + MD9 simultaneously, healing a full-health target for 10× its maximum health.
   Charge gained from the overheal itself must be exactly zero; the shield MD9 creates may
   generate, and that shield's own over-cap remainder must likewise generate zero.
5. **A Support's solo damage output is within 25% of the five-class median.** Measured per
   branch, not per class: Medic (MD10 Blood Debt), Conductor (Downbeat), and Warden
   (Blackout) each measured on a fresh solo character against the O18 elite target.
6. **CONDUIT cannot fully refund itself.** Entering CONDUIT at 100 and playing optimally
   for its 12s must leave the Support below 100 at the window's end under every node
   combination. Verify with the fastest known configuration (MD4 + MD6 + WA1 + CO3). If it
   can, the ultimate is free and the 100-cost commitment is decorative.
7. **The self-heal Charge source never exceeds 6/s and the global never exceeds 18/s.**
   Drive every source simultaneously in the Gym: self-heal from a maximum Life-on-Hit build
   at maximum cadence, plus a live buff, plus damage on a mark, plus a cleanse, plus an
   assist.
8. **A Support with sustain invested reaches "substantially higher than 4–5s" TTD (O18),
   and a Support with none sits inside the 4–5s bare band.** Support is the class most able
   to move the TTD seed target and therefore the one that most needs it verified in both
   directions.
9. **Charge cannot be banked above 60 out of combat**, and cannot be generated at all with
   no enemies present. Verify: full bar, leave combat, wait 30s — the bar reads 60. Stand
   in an empty room with Cadence self-cast for 60s — the bar reads 0.
10. **No Support node grants a movement or defensive verb**, and no node reads a block or
    dodge proc. Static audit against §6.2.
11. **A Support at 0 Charge can restart the loop from nothing**, with no allies, no gear
    sustain, and no ultimate. Verify with WA11 (Hunter's Economy) equipped and also without
    it — the base case must work too, via Mark's low cost and the marked-damage source.
12. **Equipping two Warden abilities with a Medic keystone is legal and produces a coherent
    character.** Cross-branch loadouts must not be punished by tree topology (mirrors Swift
    criterion 7).
13. **The elemental Conductor nodes (CO6, CO10) fail closed, not open.** With the element
    system absent, both must be unpurchasable or explicitly inert — never silently
    contributing zero while charging points. Same posture Core-Constellations takes for
    Elements: authored on paper, not authored as a Data Asset until the dependency clears.

---

# 8. OPEN QUESTIONS

1. **Starter pair — Patch + Mark, or Cadence + Mark?** §0 rules Patch and Mark and gives
   the argument (Medic is unreadable without a heal; Conductor is the least legible branch
   in the first hour and therefore correctly gated). The counter-argument is real: Cadence
   is the ability that most immediately *feels* like a Support, and a class whose first
   hour is "heal yourself and shoot the glowing enemy" may read as a worse Swift. Owner
   call; the correction is reversible with no structural consequence.
2. **Does the party premium live only in assists?** Class-Kits OQ10, carried. Count
   independence means a five-player Support generates no faster than a solo one from
   buffs, and assists are the only party-exclusive source. This document argues for keeping
   it that way; the owner may want a modest per-ally efficiency edge on a second source.
   **Any answer that adds a per-ally scalar to healing, shielding, or buffs must be
   rejected** — that is the 7.10.6 failure mode returning by increments.
3. **Sealed's scope — regeneration-only, or no-shield-application?** Game-Modes Class C
   audit, unresolved. §6.4 gives the concrete blast radius: the broad reading deletes a
   named-mandatory Charge source and kills MD5 and MD9. Owner call, and it is now the
   highest-priority one for this class.
4. **Bare's scope — affix-only, or all non-regeneration healing?** Same audit. The broad
   reading zeroes the two largest sources in the generation table and guts Medic. §6.4
   argues the narrow reading must be normative. Owner call.
5. **Is MD1 (Field Dressing) a legal class-layer action?** It credits healing from the
   *affix* layer (Life on Hit / Life on Kill) into the *class* loop. This is the same shape
   as Swift's F8 (Ammunition Economy), which Class-Kits accepts as "the class layer reading
   the affix layer, not duplicating it" — but MD1 makes a gear stat into a resource
   generator, which is a stronger claim than F8's. The 6/s cap is the guardrail. Does the
   guardrail make it legal, or is the whole shape wrong?
6. **Should CO11 (Detached Baton) exist at all?** It is the only node in the class that
   suspends a branch's solo-guarantee rule (self-first). It is optional and nothing depends
   on it, so it is not a compliance violation — but a solo-primary game containing exactly
   one node the solo player must be told to skip is a wart. Cut it, or accept it as the
   branch's one party-play affordance?
7. **Does Conductor's elemental layer belong in Support at all?** Conductor is
   elemental-adjacent by assignment, and CO6/CO10 are both blocked. If the resistance model
   slips, Conductor ships at 10 of 12 nodes indefinitely. The alternatives are (a) ship the
   blocked nodes as cadence-only rewrites and add the elemental layer as a later
   *replacement*, which O2/O19 both discourage, or (b) accept a partially-blocked branch
   the way Core-Constellations accepts a fully-blocked Elements. Currently (b).
8. **Is "no decay + out-of-combat clamp at 60" the right banking shape**, or should Charge
   decay in combat like Momentum? Decay would make the class more active and would make
   CONDUIT a genuine race; it would also punish the player for lulls the enemy chose, which
   is the one thing a reactive class must not be punished for. Playtest question.
9. **Does the 6/s self-heal cap survive contact with a real Life-on-Hit build?** It is the
   only per-source cap in the class that exists to bound the *item* layer rather than the
   player's inputs. If the cap binds constantly, Support's gear choices collapse to
   "anything but leech," which is a worse outcome than the exploit it prevents. Measurement
   question (O2) with a design consequence.
10. **Do three Support keystone Mores at 1.20 / 1.25 / 1.30 read as a meaningful choice?**
    Warden's is the largest *and* the easiest to satisfy (mark it, shoot it) because it is
    the solo branch. That is intentional per Class-Kits §5, but it may mean Warden's
    keystone is simply the best one and the other two are never taken by a solo player.
    Balance-shaped, but the *shape* is a design question and it is not answerable under O2.
11. **Does the shared `UBreakerMarkComponent` serve Swift's Lead and Support's Mark
    cleanly**, or do the two marks need separate ownership semantics? Both need
    proc-coefficient-0 jump-on-death (M5 / WA4) and both need owner attribution, so the
    shape looks shared — but Swift's mark is a *weak-point forcing* rule and Support's is a
    *damage-taken and generation* rule, and one component serving both may end up as two
    components in a trenchcoat.

## Top three, if only three get answered

1. **Sealed and Bare scope** (OQ3, OQ4). They are the only open items that can delete a
   clause this class is defined by. Everything else is tuning or taste.
2. **Whether the party premium stays confined to assists** (OQ2). It shapes the class's
   single most important acceptance number.
3. **Whether Conductor's elemental layer belongs here** (OQ7). It decides whether one of
   three branches ships whole.
