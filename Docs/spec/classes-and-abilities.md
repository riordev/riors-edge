# Classes and abilities

## What this system is for

A class is how you act: the verbs you have and the resource you spend to use
them. It is one of four avenues a character's identity can come from, and it
must never be the only trunk — a Core-tree build or a gear build has to be a
real answer to "what is your character", not a supporting layer under a class
choice.

It fails when the resource is decoration. If a loop generates without being
played toward, or spends without changing what the player can do next, the
class is a colour on a bar and the abilities are cooldowns with a theme.
Selection is permanent, so a class that fails this way is a mistake the player
cannot take back.

## The rules

**Five classes, five resources.** Caster spends Mana, Swift builds Momentum,
Gunsmith banks Scrap, Tank earns Grit, Support accrues Charge. All five ship
playable, and class selection is permanent per character.

**A class is offered only once its kit executes.** The gate is derived from
whether the abilities run, never from a list someone maintains — a row-count
proxy would offer a permanent, irreversible lock onto nothing.

**Generation is capped per second, per source.** This is the anti-farm rule and
it is not optional: it is what stops wall-riding for Momentum and self-harm for
Grit. Generation events carry a proc coefficient, so a tick generates at its
coefficient rather than at full value.

**No resource decays in a menu, at a Forge, or in the Anchor.** Decay is barred
in SAFE states — that enumeration is the rule. It is not "decay only in
combat": a loop may move in the field between fights, and one that converges on
a midpoint out of combat is moving toward a resting value rather than bleeding
toward nothing.

**Cost reduction joins the additive bucket. It never becomes a More.**

**Every class generates solo.** Solo is the primary balance target, and every
Support branch has a self path. A generation source that only fires with allies
present is a class that does not work alone.

**Every build must be able to make an impact in every encounter.** Builds may
excel in some situations and be weak in others; none may be unable to
participate.

**Two abilities plus one ultimate are equipped**, from the class's registered
kit. Starters are per-class authored — one or two free at level one. Swift is
the one-starter class: Skim plus an enhanced-dash tree node granted at level
one, and its second slot stands visibly empty until the first unlock — the
empty slot is the first thing the quartermaster fills. Many against two slots
is a loadout decision rather than a rotation.

**The remaining class abilities unlock one at a time, per character.** The
ultimate and the starters are free and never unlock; every remaining class
ability is bought with a one-time token at the quartermaster, an Anchor
interaction. Tokens rather than
the crafting currency, because that currency is account-wide and would let an
established account buy out a new character's kit at level one. One token per
unlockable means acquisition is an ordering choice — which ability first —
rather than a scarcity one.

**Branch keystones rewrite the ultimate; they never replace it.** One ultimate
per class, available from level one, with three distinct behaviours from three
keystones and no additional ultimate assets.

**Ability damage draws its own additive pool plus the shared one**, and rides
gear depth through the equipped weapon's item-level scalar, anchored to exactly
1.0 at the bottom so nothing moves at the anchor. Depth keeps an ability's base
from decaying against the content curve; breadth is what lets an ability build
compete. **Which pool applies is decided by what delivers the damage, not by
what triggers it** — an ability that swings the equipped weapon deals
weapon-delivered damage.

**Temporary ability windows are More multipliers and compete inside the same
budget as tree keystones.** A window bought on a build already holding three
Mores buys little, and that competition is the design.

**Behavioural gaps are recorded, never faked.** Where an ability needs a
primitive the game does not have — threat, stagger, status immunity, a lethal
save — the absence is written at the ability's own site and the ability ships
honestly short. Substituting a nearest-fit primitive silently is how a kit
reads finished and plays wrong.

**A class ability may not author a magnitude on a stat target the Core tree
already carries on the same axis.** The class layer changes what an axis does;
it does not restate it.

## The model

### The five loops

| Class | Resource | Shape | Spends on |
|---|---|---|---|
| Caster | Mana | Starts full, spends down, regenerates | Cost only, no cooldowns |
| Swift | Momentum | Built by movement, decays out of combat | Cost plus a short cooldown |
| Gunsmith | Scrap | Banked from combat and salvage | Deployables cost, armory abilities do not |
| Tank | Grit | Earned by taking and mitigating | Cost plus a short cooldown |
| Support | Charge | Accrued by contribution | Cost plus a short cooldown |

Three of five gate on spending alone; the two whose generation is spikiest and
event-driven carry a short cooldown as well.

### Mana is inverted, and Caster authors no cooldowns

The bar starts full, spends down, and regenerates. Passive regeneration is the
primary recovery path and sits outside the generation budget, so fighting well
recovers at most about twice as fast as standing still rather than replacing
the trickle entirely.

**A rule rewrite may change where the bar rests, never how fast fighting pays.**
The doubling bound above is the ceiling on combat recovery and nothing rewrites
it. What a rewrite may do is move the resting point — a loop that converges on a
midpoint out of combat drains from above it and regenerates from below it, so
the bar is never at rest and a character never starts a fight verbless. That
second clause is not decoration: Mana IS the cooldown, and O92 already names
starting a fight without your resource as the mistake a permanent class cannot
take back.

**Mana is the cooldown.** No Caster ability authors a cooldown tag. An empty
cooldown means cost-gated, and the HUD must distinguish that from a cooldown of
zero.

Overcast drives the bar negative to an authored floor: generation doubles,
incoming damage rises, and **a cast that would breach the floor is refused, not
truncated.** Truncating a cast is a silent failure at the exact moment the
player is spending everything.

### Class identities

**Swift is projectile manipulation** — multishot, pierce, chain, ricochet, and
momentum state modulating the shot. Pierce feeds Momentum back, so the identity
and the loop are the same mechanic seen twice.

**Caster is priced casting.** Everything is Mana; nothing is time.

**Gunsmith is deployables**, and the design's cost-saving ruling is load
bearing: **deployables do not move.** Nothing in the tree makes them move — a
moving turret is a pet, and pets are a different fantasy with a different AI
budget. Each type carries its own authored health rather than inheriting the
player's, takes direct damage only with no damage over time and no status, and
is targeted opportunistically rather than through a threat mechanic; bosses and
champions ignore them. **A deployable's damage is the Gunsmith's damage** — it
is a delivery mechanism, not a pet with its own stat block, so the player's
affixes, crit and statuses apply.

The Gunsmith's cost split is the class's ergonomic and not an inconsistency:
armory abilities cost nothing and carry a cooldown, deployables cost Scrap and
carry none. Tidying a cooldown onto a deployable deletes the class.

**Tank converts mitigation into Grit.** **Support converts contribution into
Charge**, and every Support branch carries a path that works with nobody else
present.

## Boundaries

This spec owns the loops, the abilities and the class identities. It does not
own:

- the aggregation law, the More ceiling, or the item-level scalar's curve —
  **power and scaling**;
- the damage resolution order or the status contract — **combat**;
- what a branch node may author, and the branch shape — **progression and
  trees**;
- what an affix or a rewrite may do to a class — **items and crafting**;
- what a resource bar looks like — **art and UI**.

## Asserted invariants

| Invariant | Test |
|---|---|
| Every generation entry point has a real caller, and every resource attaches | `Classes.BuiltClassKit.Generation` |
| Every ability row names a real implementation, and the definition and registry ids mirror exactly | `Classes.BuiltClassKit.Registry` |
| A wrong-slot ability id is refused; a wrong-class id falls back to the class default | `Abilities.SlotResolution` |
| Every damage submission passes through the outgoing-modifier chain | `Combat.Ceiling.AbilitySubmissionConformance` |
| No Caster ability authors a cooldown | `Abilities.CasterHasNoCooldowns` |
| A cast that would breach the resource floor is refused, not truncated | `Abilities.OvercastRefuses` |
| Per-source generation caps hold under a maximal farming rotation | `Classes.GenerationCaps` |
| Every class reaches its ultimate solo, against solo content | `Classes.SoloGeneration` |
| A resource-depleted condition cannot be satisfied by a loop that has never been spent | `Progression.ConditionVocabulary.ResourceDepleted` |
| Every class's resource generates in every state that class is expected to fight in | `Classes.GenerationReachability` |
| Starters, unlockables and the ultimate partition the class's registered abilities exactly | `Abilities.Catalogue.Partition` |
| No registered ability is offered and permanently refusable | `Abilities.Catalogue.NoPermanentlyRefusable` |
| Each class's authored starter count seeds its slots — two for most, one for Swift with slot two empty | `Abilities.StarterPair` |
| Every ability of every class is reachable by level 50 at the shipped token entitlement | `Progression.AbilityUnlocks.ReachableByFifty` |
| No class is ever paid a token it cannot spend | `Progression.AbilityUnlocks.NoUnspendableTokens` |
| A refused unlock costs nothing | `Progression.AbilityUnlocks.SpendRefusals` |
| A Forge respec clears neither the unlocked set nor the token counters | `Progression.AbilityUnlocks.SurvivesRespec` |
| Unlocks and token counters survive save and load | `Progression.AbilityUnlocks.SurviveSaveLoad` |
| A save written before unlocks existed loads with its abilities still unlocked | `Save.Migration.V4ToV5` |

The last two are targets the game does not currently meet.

**A bank-style resource sitting at zero is not "depleted".** Grit, Scrap and
Charge all start at zero and stay there if the player simply never spends, so a
condition testing for depletion is permanently true for three of five classes —
which makes one conditional line passively best-in-slot on those classes and
trivially loopable on the class that can drive its bar negative deliberately.
Depletion has to mean *drained past empty*: only a loop that rests full can
satisfy it.

**A resource the player cannot generate while playing normally is a trap, and
the threshold moves rather than the playstyle.** Momentum's generation
threshold sat above walking speed and above every heavy aim-down-sights state,
so a Swift who aimed was locked out of their own resource — and Marksman is the
branch that most wants to aim. The threshold comes down, or aim-down-sights
states are exempt from it, or both. A permanent class whose most natural
playstyle disables its own resource is a mistake the player cannot take back.

## Open

- Whether Momentum's generation threshold is a deliberate tension or a trap.
- Whether deployable stats are snapshotted at placement or read live. The
  design says the player's stats apply and does not say when they are read;
  everything downstream depends on the answer.
- Whether the party layer's ally-facing generation sources are built or stay
  honestly caller-less.
- Whether one keystone per character is the right ceiling across all fifteen
  branches.
- What the class layer offers a build whose Core axis is weak in an encounter,
  beyond its rewrites.
