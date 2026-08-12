# Layer ownership

Class trees, the universal Core Tree, and equipment affixes are three separate systems that can all express "the player is better at something." Without an explicit rule they become three competing ways to author the same modifier, and the class fantasy, the constellation choice, and the loot chase all dilute each other.

This document is the rule. Apply it before adding any node, affix, or class mechanic.

## The three layers

**Class layer — owns resource loops and identity mechanics.** Mana, Momentum, Scrap, Grit, and Charge generation and decay. Class-specific abilities and the ultimate. Behavior no other class can produce.

**Tree layer — grants the few remaining verbs, and rewrites rules.** A rule rewrite changes how a system resolves rather than scaling its output: dodging refunds resource, blocking reflects, a successful evade triggers an effect. Tree nodes are also the correct home for cornerstones.

**Affix layer — scales verbs the player already owns.** Affixes make existing actions better, cheaper, faster, or longer. They do not introduce actions.

## The base kit

The following are available to every character from level one, with no tree investment:

- Walk, sprint, jump, crouch
- Dash
- Slide
- Wall ride and wall jump
- Block
- Dodge

Classes and constellations do not unlock these. They make them better. Swift/Kinetic is the specialist home for dash and velocity interactions; Bulwark and Kinesis deepen block and dodge; but a character who invests in none of them can still perform every one of these actions.

This follows the movement guardrail that a player using ordinary run, sprint, jump, and cover remains viable, and it means no defensive or mobility affix can roll as a dead stat.

## The test

For any proposed affix, ask: **does this do anything if the player has not bought the corresponding tree node?**

Because the base kit is broad, most mobility and defensive affixes now pass automatically. The test still bites on anything gated behind a tree unlock — currently air jump and parry.

If an affix would grant a capability rather than scale one, it belongs in a tree instead.

## Worked examples

| Capability | Source | Scaling affix (gear) |
|---|---|---|
| Dash | Base kit | Cooldown, distance, charges |
| Slide | Base kit | Speed, duration, momentum retention |
| Wall ride | Base kit | Duration, wall jump control |
| Block | Base kit | Block %, stamina cost |
| Dodge | Base kit | Dodge %, stamina cost |
| Air jump | Tree grant | Preserved horizontal speed, height |
| Parry | Tree grant (Bulwark) | Window, reward magnitude |

## Dividing the defensive space

Block and dodge being universal creates a new version of the original problem: baseline, affixes, and two constellations can all improve the same two actions. Differentiate by *kind* of improvement, not magnitude.

- **Affixes** own raw percentages and stamina economy — block chance, dodge chance, stamina cost, stamina regeneration.
- **Trees** own rule changes and quality — i-frame duration, parry, dodge refunds, block reflects, evade-triggered effects.
- **Classes** own the fantasy — Tank converts mitigation into Grit; Swift converts evasion into Momentum.

I-frame duration in particular should stay tree-only. It is the highest-leverage defensive quantity in the game and putting it on a random roll makes survivability unpredictable for encounter design.

## What this forbids

Affixes must not grant air jump, parry, or any future verb. Doing so hands a tree unlock to anyone with a lucky drop and steals the payoff from the constellation that owns it.

Affixes must not scale the shared stamina pool or its regeneration without a cap. The pool exists to create a Bulwark/Kinesis tradeoff; uncapped gear scaling erases the constraint. This risk is larger now that block and dodge are universal, because every character spends from the pool.

Core Tree nodes must not outperform the class branch whose fantasy they overlap. Kinesis must not make non-Swift characters the best movers; Elements must not make non-Casters the best reaction specialists.

Class mechanics must not be reproducible through affix stacking. If enough gear can approximate a class resource loop, the class has no identity.

## Consequence for the tree layer

With dash, slide, wall ride, block, and dodge all in the base kit, the tree layer has almost no verbs left to grant. Air jump and parry are the current total.

This is not a problem, but it does mean constellations must earn their weight through rule rewrites rather than unlocks. A node that reads as a flat percentage is now doing the affix layer's job and should be reconsidered. The existing guidance already points this way — cornerstones should change a rule rather than add a percentage — and that standard should now apply further down each tree, not only at the top.
