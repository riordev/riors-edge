# Layer ownership

**Scope:** slice — this rule governs systems already live in code (checked against the current build throughout; see the "FOR THE OWNER" section below) and is written to extend unchanged into post-slice endgame gear rather than needing a separate treatment (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Class trees, the universal Core Tree, and equipment affixes are three separate systems that can all express "the player is better at something." Without an explicit rule they become three competing ways to author the same modifier, and the class fantasy, the constellation choice, and the loot chase all dilute each other.

This document is the rule. Apply it before adding any node, affix, or class mechanic.

## The three layers

**Class layer — owns resource loops and identity mechanics.** Mana, Momentum, Scrap, Grit, and Charge generation and decay. Class-specific abilities and the ultimate. Behavior no other class can produce.

**Tree layer — grants the few remaining verbs, and rewrites rules.** A rule rewrite changes how a system resolves rather than scaling its output: dodging refunds resource, blocking reflects, a successful evade triggers an effect. Tree nodes are also the correct home for cornerstones.

**Affix layer — scales verbs the player already owns.** Affixes make existing actions better, cheaper, faster, or longer. They do not introduce actions.

**Item RULE layer — the fourth layer, added 2026-08-14 and not present when this
document was written.** Rarity above Exceptional now MEANS something, and what
it means is a rule rewrite carried on an item:

- **Anomalous is a RARITY** that carries exactly one rewrite, **rolled** from a
  generic pool of four (UNBOUND, OVERFLOW, PROLIFIC, RELENTLESS). Equip cap 1,
  so a character holds at most one rewrite at a time.
- **Legendary is a separate field** on the item naming an authored item with a
  fixed slot, guaranteed affixes and a **hand-authored** rule. Every legendary
  rolls at Anomalous rarity; most Anomalous drops are not legendaries [O32].
  **Do not conflate the two axes** — one is a tier of the rarity ladder, the
  other is an identity.

This layer deliberately does what the tree layer does — it rewrites how a system
resolves — and the boundary between them is the CONSTRAINT, not the kind of
effect: **an item rule may never author a More multiplier.** O3 caps a build at
three composed Mores and the trees already offer six options against it, so a
fourth from an item is either dead weight (the global clamp eats it) or a quiet
nerf to the three the player chose. `RiorsEdge.Items.Rules.NeverAuthorsAMore`
asserts it on every aggregated attribute. Mores stay a tree-layer instrument.

The full catalogue is in `Docs/Item-Foundation.md`.

## The base kit

The following are available to every character from level one, with no tree investment:

- Walk, sprint, **two jumps** [O25], crouch
- Dash
- Slide
- Wall ride and wall jump
- Block (passive chance layer, not an action — see below)
- Dodge (passive chance layer, not an action — see below)

**[RULED O25 2026-08-13]: TWO JUMPS ARE BASE KIT for every class**, and Swift
innately unlocks a **third** later — innate to the class, not a tree purchase.
This SUPERSEDES the earlier line, still repeated further down this document,
that air jump is one of only two tree-granted verbs. **Parry is now the only
tree-granted verb.** Every "Air jump | Tree grant" row below is corrected in
place; see the owner note at the end for the tree node that still exists.

**[RULED O1 2026-08-12]:** Block and dodge are **passive chance layers**, not actions. Dodge is a chance to fully evade an incoming hit; block is a chance to reduce one. They take no input, use no stance, require no shield, and cost nothing. The shared stamina pool is **deleted**. **Parry** is the only defensive *input*, and it runs on its own short cooldown.

Classes and constellations do not unlock these. They make them better. Swift/Kinetic is the specialist home for dash and velocity interactions; Bulwark and Kinesis deepen block and dodge; but a character who invests in none of them still has every one of these capabilities.

This follows the movement guardrail that a player using ordinary run, sprint, jump, and cover remains viable, and it means no defensive or mobility affix can roll as a dead stat.

## The test

For any proposed affix, ask: **does this do anything if the player has not bought the corresponding tree node?**

Because the base kit is broad, most mobility and defensive affixes now pass automatically. The test still bites on anything gated behind a tree unlock — **currently parry, and parry alone** (O25 moved the air jump into the base kit).

If an affix would grant a capability rather than scale one, it belongs in a tree instead.

## Worked examples

| Capability | Source | Scaling affix (gear) |
|---|---|---|
| Dash | Base kit | Cooldown, distance, charges |
| Slide | Base kit | Speed, duration, momentum retention |
| Wall ride | Base kit | Duration, wall jump control |
| Block | Base kit (passive layer) | Block % |
| Dodge | Base kit (passive layer) | Dodge % |
| Second jump | **Base kit [O25]** | Preserved horizontal speed, height |
| Third jump | **Class-innate (Swift) [O25]** — not a tree purchase, not an affix | — |
| Parry | Tree grant (Bulwark) — **the only one left** | Window, reward magnitude |

## Dividing the defensive space

Block and dodge being universal creates a new version of the original problem: baseline, affixes, and two constellations can all improve the same two layers. Differentiate by *kind* of improvement, not magnitude.

- **Affixes** own the raw chances — block chance and dodge chance. There is no stamina economy to own [RULED O1 2026-08-12]. **AS BUILT this is inverted:** the slice affix pool contains no dodge or block line at all, while `EBreakerNodeStatTarget` has both `DodgeChance` and `BlockChance` and six tree nodes bid on them. So today the TREES own the raw chances and the affix layer owns none of it — the reverse of the division below. Either the pool gains the two lines or this rule changes; it should not stay both ways.
- **Trees** own rule changes and quality — i-frame duration, Parry (the only defensive input, on its own short cooldown), refunds on a successful evade proc, block reflects, evade-triggered effects.
- **Classes** own the fantasy — Tank converts mitigation into Grit; Swift converts evasion into Momentum.

I-frame duration in particular should stay tree-only. It is the highest-leverage defensive quantity in the game and putting it on a random roll makes survivability unpredictable for encounter design.

## What this forbids

Affixes must not grant parry or any future verb. Doing so hands a tree unlock to anyone with a lucky drop and steals the payoff from the constellation that owns it.

**[RULED O1 2026-08-12]:** the former prohibition on affixes scaling the shared stamina pool and its regeneration is deleted along with the pool itself. No stamina affixes exist and none may be authored.

GAP [O1]: the shared pool was the entire Bulwark/Kinesis hybrid tradeoff. With it gone there is no tension mechanism between the two defensive constellations, and no cap-shaped constraint on stacking defensive affixes. Owner to decide what replaces it.

Core Tree nodes must not outperform the class branch whose fantasy they overlap. Kinesis must not make non-Swift characters the best movers; Elements must not make non-Casters the best reaction specialists.

Class mechanics must not be reproducible through affix stacking. If enough gear can approximate a class resource loop, the class has no identity.

## Consequence for the tree layer

With dash, slide, wall ride, block, dodge **and two jumps** all in the base kit,
the tree layer has almost no verbs left to grant. **Parry is the current total.**

This is not a problem, but it does mean constellations must earn their weight through rule rewrites rather than unlocks. A node that reads as a flat percentage is now doing the affix layer's job and should be reconsidered. The existing guidance already points this way — cornerstones should change a rule rather than add a percentage — and that standard should now apply further down each tree, not only at the top.

**And it now has competition.** The item rule layer rewrites rules too, and it
is capped at one equipped rewrite where a tree can hold several. The tree
layer's remaining distinctness is the More multiplier — which items may not
author — plus permanence: a tree rewrite is a build you committed points to, an
item rewrite is a build you found.

## FOR THE OWNER — where the code disagrees with this document (2026-08-14)

1. **`Core.Kinesis.AirJump` still exists as a tree node**, granting the
   `Progression.Verb.AirJump` tag and a `GrantedAbilityIds` entry named
   "AirJump" — against O25, which puts two jumps in the base kit and leaves
   parry as the only tree-granted verb. Nothing reads that tag or that ability
   id, so the node is not handing out a third jump; what it actually does is
   +15% Air Control, which is an affix-layer effect wearing a verb's name.
   Either it is renamed and re-specified as the air-control notable it is, or it
   is cut and its points redistributed. It is currently the only node in the
   game whose title lies about what it does.
2. **The defensive division is inverted in code** — see the note above: trees
   author dodge and block chance, affixes author neither.
3. **The stamina GAP [O1] is still open.** The shared pool was the entire
   Bulwark/Kinesis hybrid tradeoff and nothing replaced it, so there is still no
   tension mechanism between the two defensive constellations and no cap-shaped
   constraint on stacking defensive affixes. Unchanged since the ruling; O30's
   Core tree redesign is the natural place to answer it.
4. **The item rule layer has no entry in the "what this forbids" list.** The one
   prohibition it obeys (never a More) is enforced by a test but stated only in
   `Item-Foundation.md`. If a second item-layer prohibition is ever wanted — no
   verb grants from items, say — this is the document that should carry it.
