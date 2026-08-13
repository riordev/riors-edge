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
- Block (passive chance layer, not an action — see below)
- Dodge (passive chance layer, not an action — see below)

**[RULED O1 2026-08-12]:** Block and dodge are **passive chance layers**, not actions. Dodge is a chance to fully evade an incoming hit; block is a chance to reduce one. They take no input, use no stance, require no shield, and cost nothing. The shared stamina pool is **deleted**. **Parry** is the only defensive *input*, and it runs on its own short cooldown.

Classes and constellations do not unlock these. They make them better. Swift/Kinetic is the specialist home for dash and velocity interactions; Bulwark and Kinesis deepen block and dodge; but a character who invests in none of them still has every one of these capabilities.

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
| Block | Base kit (passive layer) | Block % |
| Dodge | Base kit (passive layer) | Dodge % |
| Air jump | Tree grant | Preserved horizontal speed, height |
| Parry | Tree grant (Bulwark) | Window, reward magnitude |

## Dividing the defensive space

Block and dodge being universal creates a new version of the original problem: baseline, affixes, and two constellations can all improve the same two layers. Differentiate by *kind* of improvement, not magnitude.

- **Affixes** own the raw chances — block chance and dodge chance. There is no stamina economy to own [RULED O1 2026-08-12].
- **Trees** own rule changes and quality — i-frame duration, Parry (the only defensive input, on its own short cooldown), refunds on a successful evade proc, block reflects, evade-triggered effects.
- **Classes** own the fantasy — Tank converts mitigation into Grit; Swift converts evasion into Momentum.

I-frame duration in particular should stay tree-only. It is the highest-leverage defensive quantity in the game and putting it on a random roll makes survivability unpredictable for encounter design.

## What this forbids

Affixes must not grant air jump, parry, or any future verb. Doing so hands a tree unlock to anyone with a lucky drop and steals the payoff from the constellation that owns it.

**[RULED O1 2026-08-12]:** the former prohibition on affixes scaling the shared stamina pool and its regeneration is deleted along with the pool itself. No stamina affixes exist and none may be authored.

GAP [O1]: the shared pool was the entire Bulwark/Kinesis hybrid tradeoff. With it gone there is no tension mechanism between the two defensive constellations, and no cap-shaped constraint on stacking defensive affixes. Owner to decide what replaces it.

Core Tree nodes must not outperform the class branch whose fantasy they overlap. Kinesis must not make non-Swift characters the best movers; Elements must not make non-Casters the best reaction specialists.

Class mechanics must not be reproducible through affix stacking. If enough gear can approximate a class resource loop, the class has no identity.

## Consequence for the tree layer

With dash, slide, wall ride, block, and dodge all in the base kit, the tree layer has almost no verbs left to grant. Air jump and parry are the current total.

This is not a problem, but it does mean constellations must earn their weight through rule rewrites rather than unlocks. A node that reads as a flat percentage is now doing the affix layer's job and should be reconsidered. The existing guidance already points this way — cornerstones should change a rule rather than add a percentage — and that standard should now apply further down each tree, not only at the top.
