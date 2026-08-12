# Character and progression architecture

Design source reviewed: `riors-edge-progression.md` supplied by the project owner.

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

Resource: Mana. Its loop should reward active spell use, kills, and precision without becoming infinite during dense encounters.

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

- One per level with approximately 30 available by level 50.
- Spent only within the selected class and its branches.
- Free respec at a Forge.
- Define class identity and access to class mechanics.

### Core Points

- One per level plus world-content rewards, targeting approximately 65 total.
- Shared across all classes.
- Free respec unless later playtests establish a meaningful reason for friction.
- Intended to fully develop two constellations and partially develop a third.

Exact totals belong in progression curves or Data Assets, not C++ constants.

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

Owns block, armour, mitigation, stamina efficiency, and parry. Guard unlocking Block is a meaningful verb unlock and should be represented as an ability grant, not a Boolean stat.

### Kinesis

Owns dodge, modest movement efficiency, slide handling, and optional aerial investment. It should not turn advanced traversal into a universal requirement.

Recommended revisions:

- Dodge uses a dedicated input action; avoid double-tap detection as the only control because it conflicts with precise strafing and accessibility.
- Fleetfoot should use restrained additive or diminishing-return scaling. Five ranks at 3% each is already significant in a grounded shooter.
- Aerial should not grant one air jump per rank. Consider one verb-unlock node plus later quality upgrades.
- Slipstream may improve slide control/recovery without eliminating every combat tradeoff while sliding.
- Phantom Step should reward a narrowly defined successful evade event with an internal cooldown.

## Shared stamina

Block and dodge can share a 100-point stamina pool, regenerating at 20 per second after 1.2 seconds without spending. Implement stamina as a replicated GAS attribute with Gameplay Effects for costs and regeneration delay.

This shared pool is valuable because it creates a real Bulwark/Kinesis hybrid tradeoff. Avoid letting gear scale the pool and regeneration without caps or diminishing returns; otherwise equipment can erase the intended constraint.

## Status architecture

Each target should have one authoritative status component or ability-system interface that records:

- status tag and source;
- snapshot or dynamic magnitude policy;
- remaining duration and tick interval;
- stack count and cap;
- buildup for threshold statuses;
- proc coefficient and spread ancestry;
- boss/elite resistance policy.

Proposed semantics from the concept:

- Bleed and Poison: physical DoTs, bypass shields, reduced by armour.
- Fire: elemental DoT with target-type interactions.
- Frost: buildup-driven slow/freeze; bosses retain slow but reject hard freeze.
- Void: healing and armour reduction, not a DoT.
- Shock: instantaneous chain event.

The current statement that Bleed and Poison both ignore shields yet are halved by armour needs a formal damage-resolution order. Define shield routing, armour mitigation, resistance, critical eligibility, and damage amplification once in the damage pipeline.

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

- class resources, stamina, health, shields, and combat attributes;
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
5. **Boss invalidation.** DoT stacking and percent mitigation/armour reduction need boss caps without making status builds feel disabled.
6. **Solo support viability.** Charge generation and abilities cannot depend entirely on allies.
7. **Movement tax.** Do not balance ordinary encounters around Kinetic/Kinesis traversal mastery.

## Recommended prototyping order

Do not build all five classes first.

1. Implement the shared attribute, damage, stamina, and status foundations.
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

## Decisions still open

- Are branch nodes freely mixed with investment gates, or mutually exclusive at major tiers?
- Does the base movement kit include dash, with Swift improving it, or does Swift uniquely unlock dash?
- Are Block and Dodge universal once purchased, and what dedicated input slots do they use?
- Can snapshot DoTs trigger ordinary on-hit effects on each tick, or only explicitly DoT-compatible effects?
- Does multiplayer scale enemy count, enemy health, elite density, or a mixture? Initial policy values are placeholders.
