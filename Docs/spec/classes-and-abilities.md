# Classes and abilities

## What this system is for

A class is how you act: the verbs you have and the resource you spend. Core and gear must also support independent build identities.

Resource generation must reward deliberate play, and spending must change what the player can do next. Permanent class selection makes this consequential.

## The rules

**Five classes, five resources.** Caster spends Mana, Swift builds Momentum, Gunsmith banks Scrap, Tank earns Grit, Support accrues Charge. Selection is permanent per character.

**A class is offered only once its kit executes.** The gate is derived from whether the abilities run, never from a list someone maintains — a row-count
proxy would offer a permanent, irreversible lock onto nothing.

**Generation is capped per second, per source.** This is the anti-farm rule and it is not optional: it is what stops sprint-looping for Momentum and self-harm for
Grit. Generation events carry a proc coefficient, so a tick generates at its coefficient rather than at full value.

**No resource decays in a menu, at a Forge, or in the Anchor.** Decay is barred in SAFE states — that enumeration is the rule. It is not "decay only in
combat": a loop may move in the field between fights, and one that converges on a midpoint out of combat is moving toward a resting value rather than bleeding
toward nothing.

**Cost reduction joins the additive bucket. It never becomes a More.**

**Every class generates solo.** Solo is the primary balance target, and every Support branch has a self path. A generation source that only fires with allies
present is a class that does not work alone.

**Every build must be able to make an impact in every encounter.** Builds may excel in some situations and be weak in others; none may be unable to
participate.

**Two abilities plus one ultimate are equipped**, from the class's registered kit. Starters are per-class authored — one or two free at level one. Swift is
the one-starter class: Skim plus an enhanced-dash tree node granted at level one, and its second slot stands visibly empty until the first unlock — the
empty slot is the first thing the quartermaster fills. Many against two slots is a loadout decision rather than a rotation.

**The remaining class abilities unlock one at a time, per character.** The ultimate and the starters are free and never unlock; every remaining class
ability is bought with a one-time token at the quartermaster, an Anchor interaction. Tokens rather than
the crafting currency, because that currency is account-wide and would let an established account buy out a new character's kit at level one. One token per
unlockable means acquisition is an ordering choice — which ability first — rather than a scarcity one.

**Branch keystones rewrite the ultimate; they never replace it.** One ultimate per class, available from level one, with three distinct behaviours from three
keystones and no additional ultimate assets.

**Ability damage draws its own additive pool plus the shared one**, and rides gear depth through the equipped weapon's item-level scalar, anchored to exactly
1.0 at the bottom so nothing moves at the anchor. Depth keeps an ability's base from decaying against the content curve; breadth is what lets an ability build
compete. **Which pool applies is decided by what delivers the damage, not by what triggers it** — an ability that swings the equipped weapon deals
weapon-delivered damage.

**Temporary ability windows are More multipliers and compete inside the same budget as tree keystones.** A window bought on a build already holding three
Mores buys little, and that competition is the design.

**Behavioural gaps are recorded, never faked.** Where an ability needs a primitive the game does not have — threat, stagger, status immunity, a lethal
save — the absence is written at the ability's own site and the ability ships honestly short. Substituting a nearest-fit primitive silently is how a kit
reads finished and plays wrong.

**A class ability may not author a magnitude on a stat target the Core tree already carries on the same axis.** The class layer changes what an axis does;
it does not restate it.

## The model

Each Caster doctrine offers twelve nodes, twenty-four points and four final-tier choices against eight spendable points. Overreach makes Caster abilities, including Unmake, free while Mana is negative and replaces the incoming penalty with30%. Prepared lowers the existing Overcast floor to at most-35; ordinary capped income and recovery remain. Both magnitudes are O2 placeholders in `Data/caster-resource.json`. Death ends every Unmake variant's window and generation suspension before respawn.

Cascade echoes the next physical status in Fracture's shared cycle, skipping elemental buildup entries. Echoes have zero proc and cannot echo again. The
ultimate includes targets spawned during its window; closing that window, death or loss of Cascade cancels queued echoes.

Long Dark pauses the lifetime of owned zones placed during its Unmake window. Damage and membership continue. That window ending, owner death or loss of
the keystone releases the pause; later ultimates cannot rearm an old zone. Refreshing a zone placed before the window does not acquire this pause.

Lingering rank two expands a refreshed overlapping Rot by one metre once per zone. New zones start at their ordinary radius; further refreshes do not grow
them again. The footprint and affected enemies use the expanded radius.

Resonance deals an untyped Elemental detonation based on status count. It does not apply elemental buildup or reactions. Its preservation node halves status
durations instead of consuming them; this also reduces unpaid Rot damage.

Fracture's selected elemental positions share one conversion budget and one direct hit. Duplicate elements merge; physical carried statuses are unchanged.
The first eligible reaction in selection order consumes an existing status and prevents all new buildup from that impact. A status earned by that impact
cannot react with another of its selections. Sympathetic pays Entropy once. The starter cycle is Bleed, Poison, Entropy. Unlocking Siphon appends Void
once, including an existing unlocked character whose cycle is created later.

Hold caps each damage application after mitigation and before shield/health spending. Its editable starting cap is 25% of current maximum health; solo
Wall uses 12.5% (O2 tuning). Small hits below the cap are unchanged. Active caps compose by minimum, including True damage and damage-over-time ticks.
Vein removes the cap. Expiry, cancellation and death clear its protection.

Detonation releases its stored damage on a second ultimate press or normal window expiry, without another resource payment. It pays once at 70% of
damage taken in an 8 m radius, without falloff or self-damage. Cancellation, death or loss of the keystone discards the stored damage.

Interposition grants its solo owner shield capacity behind a live owned Anchor
Point within 4 m. The starting headroom is 10% of maximum health (O2 tuning).
Entering grants no shield. Leaving, losing the node, death or Anchor removal
restores the underlying capacity and clamps excess shield; repeated entry
does not accumulate capacity.

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

## Swift movement nodes (2026-09-07)

Swift's Read the Room now extends airborne resource credit from three seconds
to 4.5/6 seconds at ranks 1/2. Only returning to real ground refills it;
buying a rank while airborne does not. Landing measures the continuous fall
from its peak and grants 2/3 Momentum per metre beyond six metres, capped at
20/30 per landing. Teleports and traversal reset the measured fall. These are
editable Momentum-component O2 defaults, tested through real movement and
node purchases; they are not yet playtest-balanced values.

Contact replaces its retired wall-ride behavior (O144): after a completed vault
or mantle, ranks 1/2 add the existing 8 Momentum/s movement rate for 0.35/0.70s
(O2). This source shares the 25/s income cap and the traversal's one-second
anti-farm gate; aborted traversal, teleport, death and class changes cancel it.

## Implemented node behavior (2026-09-07)

Tank Overpressure uses the existing Breach Charge input again to detonate a
live charge without a second cost or cooldown. Rank two follows the first
living enemy hit. The ordinary fuse remains active; Demolition permits its
two sequential placements under one shared cooldown. Death and unequipping
remove pending charges.

Gunsmith Dead Ground now modifies a real deployment cast. Pending placements
can be cancelled and must pass the ground/range/sight/cost checks again at
completion. Failed payment does not evict an existing deployable; refunds use
the exact cost paid before spending changes the Scrap band.

Support Blackout Protocol prevents healing and active beneficial effects on
the owner's marked enemy inside Suppress while that owner is Resonant. It
reads those conditions live, including overlapping fields and release/expiry.
The existing Warding Aura benefit and Warded shield recharge are suppressed;
intrinsic boss mechanics and shields already granted are preserved.

## Support buffs and marks

Cadence grants reload and swap tempo to living players within its footprint.
The ordinary footprint follows at walking speed; Section enlarges it and
keeps it with the caster at sprint speed. The caster receives the ordinary
buff continuously. Detached Baton stays where cast and requires the caster
to enter its footprint like any other recipient.

Each cast owns its recipient effects. Leaving removes that cast's effect;
another Support's active cast remains. Downbeat Discipline extends the
caster's copy, and Conducting supplies a brief self tail after leaving a
detached footprint. Conducting shortens the actual recipients' cooldowns.
Buff-upkeep Charge belongs to the caster maintaining at least one recipient,
including self, and never scales with the number of recipients or casts.

Tempo uses the strongest active reload and swap bonuses separately. Reloads
and swaps capture their duration when they begin; aura changes affect the
next action. Reloading transfers ammunition without creating rounds.

Metronome selects living players within 500 cm when cast, always including
its caster (O2 radius). Each selected holder keeps a separate ramp for the
buff duration; later movement does not change this cast's membership.
Successful weapon shots advance the ramp by proc coefficient. Ability and
melee hits require Counterpoint, which also accepts damaging status ticks.
Zero damage and zero proc do not advance or sustain a streak. Tempo improves
the caster's cap and reset gap at rank one and all recipients at rank two.
Rehearsal refreshes surviving holders without discarding their ramps.

Metronome and Downbeat flat bonuses add before weapon Increased and More,
including distinct maintaining sources. They exclude ability, melee and DoT
requests, even when those requests use the weapon damage pool. Conduit's
Downbeat counts unique living recipients of the caster's active Conductor
buffs, and doubles Cadence's tempo bonus and Metronome's flat bonus while live.

Mark generates Charge from the caster's successful weapon shots. Painted adds
own ability and DoT damage at rank one, then allied player damage at half yield
at rank two (O2). Actual proc coefficient scales all yields; zero proc pays
nothing. Blood Debt cashes out only on the caster's weapon hit. Overlapping
Marks use the strongest vulnerability and Tell reduction without multiplying
identical effects; ending one cast preserves the others.
Tell shows an attack warning beside the marked target's health bar while its
actual melee, Lattice or Warden attack is winding up. The marking caster must
still own Tell; idle targets do not show a warning.

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
