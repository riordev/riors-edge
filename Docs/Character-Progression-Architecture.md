# Character and progression architecture

> STATUS 2026-08-16: PARTIALLY BUILT — its own "As built" section predates the XP loop, points-per-level, the character roster and the S4 widening; trust HANDOFF.md and Source/RiorsEdge/Progression/ for what exists.

**Scope:** mixed — judge per section; the "As built" status, progression currencies, and locked decisions describe the current slice, while most of the five-class / six-constellation architecture above them is post-slice design intent, explicitly flagged in this document's own opening note as mostly not yet built (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Design source reviewed: `riors-edge-progression.md` supplied by the project owner.

This document is the ARCHITECTURE ANALYSIS. Most of it is intent that has not
been built, and that is correct — it is the plan the implementation is measured
against. The "As built" section near the end states exactly which parts exist,
so nothing here should be read as a claim of implementation.

## Design verdict

The five-class structure is a strong foundation. Each class has a readable resource loop, three distinct branches, a class ability family, and a base ultimate. The universal Core Tree provides cross-class build expression without requiring dozens of subclasses.

The system should be implemented as three separate layers:

1. **Class identity** — resource, branch abilities, class mechanics, and ultimate.
2. **Universal combat specialization** — Precision, Volley, Affliction, Elements, Bulwark, and Kinesis.
3. **Equipment and affixes** — item-specific rolls and build-defining legendary rules, designed separately.

Keeping these layers distinct prevents class trees, core nodes, and affixes from becoming three competing ways to express the same modifier.

## Character model

A saved character owns:

- selected class;
- character level and experience;
- available/spent Class Points;
- available/spent Core Points;
- chosen class nodes and universal core nodes;
- equipped items and loadout;
- unlocked content and versioned migration metadata.

Runtime actors receive the resulting abilities, effects, tags, and attribute modifiers when the character is initialized. UI never owns progression state.

## Five class contracts

### Caster

Resource: Mana. **The loop is INVERTED from what this paragraph originally
described** (owner ruling 2026-08-14, superseding Class-Kits §2.1's accumulating
bank): **the bar starts FULL and drains as spells are cast.** Passive
regeneration is the primary recovery path; spell use, kills and precision are
**accelerators on top of it**, not the income. The concern below — "without
becoming infinite during dense encounters" — is now expressed as a generation
CAP on the accelerators rather than as the shape of the loop.

Two consequences for the branch designs that follow: any node described as
"generates Mana when X" is an accelerator, not the thing that makes the class
function; and **efficiency is now a first-class resource stat** alongside
regeneration, because a spend-down resource is governed by cost and recovery
rather than by income. `Core.ResourceEfficiency` is the gear expression of it.

Branches:

- Spellblade: close-range spell/melee hybrid and target-crossing mobility.
- Void Whisperer: damage over time, sustain, and controlled zones.
- Multispell: sequencing different elements to create reactions.

Architecture note: elemental sequencing needs a combat-event layer and target status container shared with the universal Elements tree. It must not be implemented twice.

### Swift

Resource: Momentum. Build it from purposeful movement and decay it while stationary, but normalize gains so running into a wall or shaking input cannot farm it.

Branches:

- Frenzy: sustained trigger discipline and attack-speed rhythm.
- Kinetic: the specialist home for wall riding, air dash, and velocity interactions.
- Marksman: projectile behavior such as ricochet, pierce, and arcs.

Architecture note: advanced traversal belongs primarily here. The base game keeps grounded movement viable; Swift/Kinetic opts into deeper movement expression without forcing it on every character.

### Gunsmith

Resource: Scrap. Award it through meaningful combat events and cap both storage and deployable density.

Branches:

- Armory: personal weapon modification and ammunition economy.
- Field Tech: turrets, crates, and allied buff devices.
- Tinkerer: traps, mines, disruption, and battlefield preparation.

Architecture note: deployables need a common ownership component, placement validation, lifetime policy, and per-owner limit. Scrap should be a resource attribute; spawned actors should not store the authoritative wallet.

### Tank

Resource: Grit. Damage received can generate it, but generation should use post-mitigation damage and anti-farming rules.

Branches:

- Leech: sustain and overheal-to-shield conversion.
- Bastion: cover, shielding, protection, and enemy attention tools.
- Demolitionist: explosives and self-knockback traversal.

Architecture note: full self-damage immunity risks removing rocket-jump cost and encounter hazards. Prefer strong self-damage reduction plus controlled self-knockback immunity rules, then tune through combat testing.

### Support

Resource: Charge. Healing and assists build it, with anti-overheal and repeated-assist safeguards.

Branches:

- Medic: direct healing, cleanse, and revive specialization.
- Conductor: allied cadence buffs such as reload and fire rate.
- Warden: marks, suppression, debuffs, and crowd control.

Architecture note: support must remain viable in solo play. Every support branch needs a self-use or offensive conversion path, while group effects remain its efficiency advantage.

## Progression currencies

### Class Points

- One per level from level 1 to level 30. Thirty total.
- No class points are granted after level 30; class identity is complete at
  that point and levels 31-50 are pure universal specialization.
- Spent only within the selected class and its branches.
- Free respec at a Forge.
- Define class identity and access to class mechanics.

### Core Points

- One per level from level 1 to level 50, plus approximately 15 from
  world content. Approximately 65 total.
- Shared across all classes.
- Free respec unless later playtests establish a meaningful reason for friction.
- Intended to fully develop two constellations and partially develop a third.

Exact totals belong in progression curves or Data Assets, not C++ constants.

**NOT BUILT, and the gap is structural rather than a missing table:** nothing in
the project writes `CharacterLevel`, so no point is ever granted by levelling.
See "As built" below for what actually happens instead.

## Universal Core Tree contracts

### Precision

Owns critical hits, weak points, execution bonuses, and sustained single-target accuracy. Tunnel Vision needs a per-attacker/per-target hit-streak state with timeout and target-swap reset.

### Volley

Owns projectile count, cadence, magazine/reload behavior, ricochet, and independent projectile outcomes. Extra projectiles require deterministic authority-side generation and must define proc coefficients to prevent exponential status scaling.

### Affliction

Owns physical damage-over-time behavior and spread. Contagion must copy a normalized status payload rather than recursively treating spreads as fresh kill-proccing applications.

### Elements

Owns status buildup and cross-element reactions. Reactions need an explicit matrix Data Asset rather than hard-coded pair checks. The same target application must not trigger multiple reactions accidentally.

### Bulwark

Owns armour, mitigation, and Parry, and deepens the universal Block layer. Block itself is base kit and is not granted by this tree.

**[RULED O1 2026-08-12]:** Block is a *passive chance layer* (a chance to reduce an incoming hit), not an action or stance, and there is no stamina efficiency to own — the shared stamina pool is deleted. Guard should therefore improve block *quality* — reflect behavior, mitigation on a successful block proc, proc-triggered effects — rather than reducing a cost that no longer exists or widening an arc that no longer applies. Parry remains a genuine ability grant, is the constellation's one verb unlock, and is the only defensive input; it runs on its own short cooldown.

### Kinesis

Owns dodge quality, modest movement efficiency, slide handling, and optional aerial investment. Dodge, slide, dash, and wall ride are all base kit; Kinesis improves them rather than unlocking them. **[RULED O1 2026-08-12]:** dodge is a *passive chance layer* (a chance to fully evade an incoming hit), so "dodge quality" means rule rewrites and i-frame behavior hung off the evade proc, not improvements to an input. ~~Air jump remains a verb unlock and is this tree's one genuine grant.~~ **[RULED O25 2026-08-13]: two jumps are base kit for every class, so Kinesis grants NO verb at all.** Swift's third jump is class-innate, not a Kinesis purchase. The fallback tree still carries a `Core.Kinesis.AirJump` node granting a verb tag nothing reads; what it actually does is +15% Air Control, and it is flagged for the owner in `Docs/Layer-Ownership.md`. Kinesis should not turn advanced traversal into a universal requirement.

Recommended revisions:

- ~~Dodge uses a dedicated input action; avoid double-tap detection as the only control.~~ **[RULED O1 2026-08-12]:** superseded — dodge takes no input at all. Parry is the only defensive input and is the only one needing an input slot.
- Fleetfoot should use restrained additive or diminishing-return scaling. Five ranks at 3% each is already significant in a grounded shooter.
- ~~Aerial should not grant one air jump per rank. Consider one verb-unlock node plus later quality upgrades.~~ **Superseded by O25**: there is no air-jump unlock to rank. Aerial investment can only be quality.
- Slipstream may improve slide control/recovery without eliminating every combat tradeoff while sliding.
- Phantom Step should reward a narrowly defined successful evade event with an internal cooldown.

## Shared stamina — SUPERSEDED [RULED O1 2026-08-12]

Superseded in full. There is no stamina pool; `Stamina`/`MaxStamina` are removed from the attribute set and the combat component. Block and dodge are passive chance layers that cost nothing, and Parry is the only defensive input, on its own short cooldown. The prior design (a 100-point pool shared by block and dodge, implemented as a replicated GAS attribute, intended to create a Bulwark/Kinesis hybrid tradeoff and requiring a cap on gear scaling) is retained here only as a record and must not be implemented.

GAP [O1]: the shared pool carried the whole Bulwark/Kinesis hybrid tradeoff. Nothing replaces it. Owner to decide.

## Status architecture

Each target should have one authoritative status component or ability-system interface that records:

- status tag and source;
- snapshot or dynamic magnitude policy;
- remaining duration and tick interval;
- stack count and cap;
- buildup for threshold statuses;
- proc coefficient and spread ancestry;
- boss/elite resistance policy.

Proposed semantics from the concept. **The elemental names below are
PLACEHOLDER**: O5 and O19 rule the elements **Rift, Entropy and Void**, not
fire/ice/lightning, and every Ignite/Chill/Shock name in this corpus is to be
re-flavoured onto them when the resistance model is implemented.

- Bleed and Poison: physical DoTs, bypass shields, reduced by armour.
  **BUILT** — these two are the only statuses that exist.
- Fire: elemental DoT with target-type interactions. Placeholder name; unbuilt.
- Frost: buildup-driven slow/freeze; bosses retain slow but reject hard freeze.
  Placeholder name; unbuilt.
- Void: healing and armour reduction, not a DoT. Void is a real element under
  O19; this status is unbuilt.
- Shock: instantaneous chain event. Placeholder name; unbuilt.

~~needs a formal damage-resolution order~~ **RESOLVED**: the order is authored
once in `Docs/Design/Damage-Pipeline.md` and implemented in
`UBreakerDamageLibrary::ResolveDamage`. Physical shield-bypassing DoTs take half
armour through the one global rule rather than through a second pipeline. The
resistance step is specified and **not built**, because no element model exists.

## Unreal data model

Suggested asset types:

```text
UClassDefinitionDataAsset
  ClassId
  ResourceAttribute
  Branches
  StartingAbilities
  UltimateAbility

UProgressionTreeDataAsset
  TreeId
  Nodes
  PointCurrency
  CornerstoneGate

UProgressionNodeDataAsset
  NodeId
  MaxRank
  Prerequisites
  CostPerRank
  GrantedAbilities
  GrantedEffects
  RequiredTags
  MutuallyExclusiveTags

UElementReactionDataAsset
  FirstElement
  SecondElement
  ReactionAbilityOrEffect
  InternalCooldown
  ProcCoefficient
```

The saved build stores stable IDs and ranks, not direct UObject pointers or calculated attribute totals. On load, the progression subsystem resolves IDs and rebuilds granted state. This supports balance changes and save migrations.

## GAS ownership

Use GAS for:

- class resources, health, shields, and combat attributes (stamina removed, [RULED O1 2026-08-12]);
- active/passive abilities and ultimate activation;
- costs, cooldowns, buffs, debuffs, immunity, and crowd-control tags;
- node-granted passive Gameplay Effects;
- event-driven mechanics such as parry, dodge success, crit streaks, and status reactions.

Use ordinary C++ systems for:

- tree topology and point validation;
- save/load and migration;
- generated item instances;
- deployable ownership/limits;
- projectile simulation and weapon runtime behavior;
- UI-facing progression queries.

## Important balance risks

1. **Multiplicative explosions.** Multishot, independent crits, status applications, reactions, and affixes can multiply each other. Every proc needs a coefficient and recursion rule.
2. **Universal-tree identity theft.** Core nodes must not outperform the class branch whose fantasy they overlap. Kinesis should not make non-Swift characters the best movers; Elements should not make non-Casters the best reaction specialists.
3. **Mandatory cornerstones.** If one cornerstone is universally superior, the apparent six-way choice collapses.
4. **Defensive invulnerability loops.** Parry refunds, dodge refunds, leech, overshields, block, and free respec can combine into permanent safety.
5. **Boss invalidation.** DoT stacking and percent mitigation/armour reduction need boss caps without making status builds feel disabled. **HALF-ADDRESSED:** the boss caps DoT stacks at 3 (against the ordinary 10), and there is **no boss armour-reduction cap at all** — Damage-Pipeline §2 specifies one and nothing implements it. What the boss has instead is a phase-3 armour halving and an exposed rear weak point, which is a different answer to the same problem.
6. **Solo support viability.** Charge generation and abilities cannot depend entirely on allies.
7. **Movement tax.** Do not balance ordinary encounters around Kinetic/Kinesis traversal mastery.
8. **Resource generation now rides on RNG [O1].** With block and dodge as passive chance layers, Tank's Grit (mitigation → Grit) and Swift's Momentum (evasion → Momentum) generate on random procs rather than on player-initiated actions. That makes both resource loops variance-driven: generation rate is no longer under player control, and streaks in either direction change how a fight plays. Recorded as a **tuning risk to be measured by the wave-mode instrumentation (O2)** — not solved here, and no rates or smoothing rules may be authored until that instrumentation reports.

## Recommended prototyping order

Do not build all five classes first.

1. Implement the shared attribute, damage, and status foundations. (Stamina struck — [RULED O1 2026-08-12].)
2. Prototype one class with two contrasting branches.
3. Implement three small Core Tree paths: one offensive, one defensive, one mobility.
4. Validate point allocation, ability/effect grants, respec, and save/load.
5. Build one cornerstone that changes a rule rather than adding a percentage.
6. Add a second class to prove that the universal tree remains universal without stealing identity.

Recommended first class: **Swift**, because its Kinetic branch immediately tests the project's movement boundary while Marksman tests the weapon/projectile framework. Frenzy can follow after the firing pipeline is stable.

Recommended second class: **Caster**, because Multispell and Void Whisperer validate statuses/reactions while Spellblade tests ability-driven close combat.

Gunsmith, Tank, and Support depend on deployables, threat/AI, shielding, healing, and team behavior and should follow after those shared systems exist.

## Locked product decisions

- Class selection is permanent per character.
- A character equips exactly two class abilities plus one ultimate.
- Solo is the primary balance target; cooperative parties support up to five players.
- Damage-over-time effects can critically strike and snapshot offensive stats when applied.
- Respecs require interaction with a Forge.
- Level cap is 50 and is a hard stop. There is no post-cap power
  progression: no paragon track, no infinite stat trickle. All endgame
  character power comes from gear.
- Dash is part of the base movement kit. Swift and Kinesis improve it;
  neither unlocks it.
- Block and Dodge are universal **passive chance layers** available to
  every character — no input, no stance, no shield requirement, no cost
  [RULED O1 2026-08-12]. Certain classes and constellations use them
  better, but no tree grants them. Parry is the only defensive input and
  runs on its own short cooldown.

## As built (2026-08-14)

What exists in `Source/RiorsEdge/Progression/` and around it, against the
architecture above:

- **The three layers are separate**, as specified. Class identity, the Core
  tree and equipment affixes have distinct data models and distinct currencies,
  and gear and nodes reach the attribute set through one shared contribution
  path rather than each writing absolute values.
- **Trees are live at runtime** with C++ fallback content (the zero-setup
  convention): a Core slice tree and three Swift trees — Kinetic, Marksman and
  Frenzy. Purchase validation covers prerequisites, ranks, currency, required
  class, mutual exclusion, tree-investment gates and cornerstone gates.
  Node effects aggregate into the attribute set; granted tags are published.
- **Respec is Forge-gated** (`RespecAtForge` refuses away from one and refunds
  by rank x cost), matching the locked decision.
- **Two class ability slots plus one ultimate** exist as specified, and the
  Ultimate slot accepts only the class's authored ultimate id.
- **More multipliers are real on nodes** and the O3 cap of three at 1.30x each
  is enforced in code, twice — per layer at selection, and globally on the
  composed product.

What does NOT exist, stated so nothing here reads as built:

- **No XP loop.** `FBreakerProgressionState::CharacterLevel` is declared with a
  default of 1 and **nothing in the project ever writes it**. The Class Point
  and Core Point curves above (30 / ~65, one per level) therefore describe a
  progression the game cannot perform; what actually happens is
  `ApplySliceDefaultsIfFresh`, which grants a flat **10 class points and 12 core
  points** to a fresh character. That is a playtest scaffold, not the curve.
  Every level-gated design in this corpus — Swift's third jump included —
  inherits this.
- **Only Swift has a class definition.** `GetFallbackClassDefinition` returns
  content for Swift and null for the other four, so three of the five selectable
  classes grant nothing. Caster is playable through the ability registry rather
  than through an authored class definition, and its branch TREES are
  unauthored.
- **Subclass commitment does not exist in the data model.** The branch strip
  browses; there is no branch field, no one-way setter and no
  permanence-or-Forge rule. Committing collides with O15 (branches freely mixed,
  no mutually exclusive tiers), which is why it is a ruling and not a task.
- **`EBreakerBuildCondition` is movement-only** — `Always / Airborne / Sliding /
  WallRiding / Redline / RecentlyDashed`. No node can key off combat or status
  state, which has now blocked content twice and which O30's taxonomy needs
  widened before ailment, crit or stacking axes can be authored honestly.
- **Deployables, Grit, Scrap and Charge do not exist in any form.** The Gunsmith,
  Tank and Support sections above are design only.

## Decisions still open

- ~~Are branch nodes freely mixed with investment gates, or mutually exclusive at major tiers?~~ **RULED [O15 2026-08-12]: freely mixed with investment gates. No mutually exclusive tiers.** Implemented — gates are `RequiredTreeInvestment` plus a cornerstone gate, and mutual exclusion exists as a mechanism but is not used to build tiers.
- ~~What dedicated input slots do Block and Dodge use?~~ RESOLVED [O1 2026-08-12]: none — both are passive. Only Parry needs an input slot.
- Can snapshot DoTs trigger ordinary on-hit effects on each tick, or only explicitly DoT-compatible effects? **Damage-Pipeline §3 states the rule** (DoT-compatible only) and **nothing enforces it** — `ProcCoefficient` has exactly one consumer, the Mana loop.
- Does multiplayer scale enemy count, enemy health, elite density, or a mixture? Initial policy values are placeholders.
- **When does Swift's third jump unlock, and does it cost anything? [O25]** The
  mechanism is built and the gate defaults to reachable because nothing moves
  `CharacterLevel`. Unanswerable in practice until an XP loop exists.
- **Does the Core tree get O30's redesign?** O30 opens it to a redesign
  organised around GUNS / ABILITIES / MINIONS axes, to be scouted and costed
  rather than implemented blind. The six constellations described above are the
  current shape, not the ruled one.
