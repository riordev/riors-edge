# Power and scaling

## What this system is for

To make a game where the content scales and the player does not, so that
building well is the thing that changes the fight. A baseline character stays
playable at every area level; a built one feels the difference; and every
number in the game is derivable from two curves rather than guessed.

It fails in two directions. If the curves diverge, content becomes unplayable
or trivial and the loot loses its job. If they cancel too completely — if a
built character kills no faster than a baseline one — then a build changes
nothing observable, and a build that changes nothing observable is not a build.

## The rules

**Area level is the only content axis.** It runs 1 to 100, is authored on the
content, and is never derived from the player's level, gear or build. It drives
monster health, monster damage, and the item level of what the content drops.

**Character level caps at 50, hard stop.** There is no post-cap character
power. Item level runs to 120, past both the character cap and the area-level
ceiling, and that is what makes "all endgame power comes from gear" literally
true rather than merely stated.

**TTK is an output of two curves and cannot be an input.** A single
time-to-kill target enforced at level 1 and level 50 mathematically forbids
progression from being felt: the only way to satisfy it everywhere is for
enemies to scale exactly as fast as the player. Targets are designer inputs
that the chassis solves backwards from, and the instrument reports divergence
from them, never truth.

**Drop item level tracks area level**, so the weapon curve and the monster
curve cancel term for term across the whole range rather than only to 50.

**Item levels 101–120 are sourced from endgame tier bonus.** Area level stops
at 100; pushing endgame tiers extends the tier bonus past its ordinary cap, and
that is the ladder's top twenty levels. Pushing tiers is pushing the ladder.

**Monster rank multiplies the chassis; it does not replace it.** Difficulty
lives in rank and modifiers, never in trash health. Trash exists to be
trivialized by an optimized build.

**The aggregation law**, one line, unchanged by everything below:

    value = (Base + sum(Flat)) * (1 + sum(Increased)/100) * product(More)

**Damage has three additive pools, not one.**

- **Increased Weapon Damage** feeds weapon-delivered damage.
- **Increased Ability Damage** feeds ability-delivered damage.
- **Increased Damage** is the shared pool: smaller, rarer, and it feeds both.

A weapon hit composes `(1 + (Weapon + Shared)/100)`. An ability composes
`(1 + (Ability + Shared)/100)`. One additive bucket per stat still holds — the
stat is now weapon damage or ability damage rather than "damage", so the law is
satisfied by construction and the shared pool is a legal contributor to both
rather than a second bucket.

**The pool is decided by what DELIVERS the damage, not by what triggers it.**
A melee ability that swings the equipped weapon deals weapon-delivered damage;
it draws the weapon pool and the shared pool, and never the ability pool. This
rule pre-answers every future case and there are no exceptions to argue about.

**A source authors at most one specific pool, plus optionally the shared one.**
Never two specific pools from one source — that is one bucket double-dipped.

**Added Ability Power bids Flat into the ability multiplier**, dividing its
printed percentage by 100 before Increased and More compose; Added Damage stays in the weapon lane.
Its airborne, sliding, Redline, recent-dash and grounded variants use the same
condition gates as weapon flat damage and never contribute to the weapon pool.

**There is ONE More ceiling across damage pools.** Gear and tree sources enter
one selection: the strongest three, each at most 1.30x, compose as an unordered
product bounded by 1.30^3. Weapon, ability, shared, DoT, elemental, Void,
reaction and Effective Health sources spend this budget. Shared spends one slot. Authored
sources retain their identity across contributors; ties resolve consistently.
The ceiling derives from the source count and per-source limit rather than a
separate constant. Effective Health is the supplied Core's explicit defensive exception.

**Temporary ability windows ARE Mores** and compete for the same headroom. On a
build already holding three, a window buys little. That competition is the
point.

**Crit and weak point are the two site multipliers.** Crit is build-gated,
weak point is skill-gated and bounded per archetype in [1.0, 2.0] and sits
deliberately outside the More budget. Nothing else may multiply at the hit
site.

**An invariant asserting a property is INVARIANT must be paired with one
asserting its LEVEL.** A flat curve can sit at any value forever and report
green; trash and elite TTK sat 1.88x off at every level while flatness passed.

**A new multiplier lane requires a canon row and a conformance test before it
merges.** The canon below is a standing discipline, not a one-time cleanup.

**Removing a multiplier's gate is a canon event, exactly as adding a lane is.**
Weak point sits outside the More budget BECAUSE skill gates it, so a source that
guarantees one has bought a build multiplier, and it moves into the accounting
the gate stood in for; crit does not also multiply on that hit. Gate removal
adds no lane, so the rule above would never have caught it.

**Conditional lines are texture, not the route to power.** A build must be able
to be strong satisfying no conditions at all, and a two-condition line is worth
more than a one-condition line only by a little.

**Choices beat accumulation.** The per-point spend baseline is a floor so that
a purely defensive purchase is not literally zero offence. Because every build
spends its whole budget it cannot separate two builds, and it must never grow
back into something that can.

**Kill experience pays from area level**, not from the drop item level. Rank
already pays the elite premium; paying from the drop ladder charges it twice.

## The model

Every constant here is a placeholder until measured. The relationships are the
design; the values are the knob.

### The two curves

    MonsterHealth(AL) = BaseHealth * (1 + g)^(AL - 1) * Rank * Archetype
    MonsterDamage(AL) = BaseDamage * (1 + d)^(AL - 1) * Rank * Archetype
    WeaponBase(ilvl)  = ArchetypeBase * (1 + w)^(ilvl - 1)

`w = g`, so a baseline build's TTK is level-invariant and every felt gain comes
from the multiplier band. `d` is materially below `g`: incoming damage scaling as
fast as health makes every build a defensive build.

**`d` is set so that hits-to-die does not fall across the level range.** That is
the property it exists to produce, and it is the test, not the value. Defence
does not scale up to meet incoming damage — the damage curve comes down until
the two hold.

| Constant | Value | Note |
|---|---|---|
| `BaseHealth` | 220 | Area level 1 is the chassis that was actually measured |
| `BaseDamage` | 30.7 melee / 35.0 ranged | Per archetype at area level 1; the ratio is the authored thing |
| `g` health growth | 0.09 | x68 over 50 levels |
| `d` damage growth | 0.0173 | x2.32, against what a baseline's gear buys, x2.33 |
| `w` weapon growth | 0.09 | Equal to `g` by design, not by coincidence |

Item level 1 is the anchor: the scalar is exactly 1.0 there, so the curve is
opt-in by content, not a silent retune. An unequipped weapon is item level 1.

### Rank

| Rank | Health | Damage |
|---|---|---|
| Trash | x1 | x1 |
| Veteran / elite | x3.0 | x1.5 |
| Modifier-bearing | x2.5, +0.35 per modifier beyond the first | x1.25 |
| Boss | x75 | x1.0 |

Ratios are derived from the targets, and a rank composes with the actor's
archetype: the boss's x75 on the Field Marshal's x0.30 is the **x22.5** a 21s
kill needs. A boss hits like its archetype and rank adds no damage: its
interest is its adds and phases, and no boss attack kills the baseline from full.

### Targets

| Target | Seed value |
|---|---|
| Trash TTK | a little under 1s, scaling exponentially with difficulty |
| Elite TTK | ~3s |
| Boss TTK, baseline build | 20–45s, unless a special enemy claims the exception explicitly |
| TTD, no resources or sustain | 7–8s; one trash melee attacker at 1.15s kills the baseline in 7.5s |
| Optimized, and invested | substantially past the baseline figure, asserted separately |

These describe a **baseline build in on-level content at archetype x1.0**; other
archetypes multiply the target, so a Warden mob at x3.2 kills in 2.9s on target.
An optimized character forty hours past 50 deletes trash on contact.

**Bosses are meant to die fast to a comfortable build**, so baseline and optimized
are asserted separately — one target for both measures the wrong character.

### The affix tier ladder

Fourteen tiers, T12 worst to T-1 best, geometric between two authored anchors
and bent once more so the step grows toward the top:

    p(T)     = (12 - T) / 11
    Value(T) = ValueAtT12 * (ValueAtT1 / ValueAtT12) ^ (p ^ 1.25)

The bottom step is about +15%, the top about +37%, which is what makes a
top-tier roll an event rather than one more step. A linear ladder makes the top
step arithmetically ordinary; a pure back-loaded lerp over eleven steps makes
the bottom four tiers indistinguishable. This shape avoids both.

Item level maps to tier on **two slopes**, so a player who finishes the
levelling game has crossed half the ladder rather than a third of it:

| Band | Item levels | Tiers | Rate |
|---|---|---|---|
| The campaign | 1 to 50 | T12 to T6 | ~8 levels per tier |
| The chase | 50 to 120 | T6 to T1 | ~14 levels per tier |

T1 opens at item level 120 exactly. T0 and T-1 sit 2.2x and 3.6x above T1 and
never come from item level — crafting or a rule rewrite only. The tier walk
climbs one step with probability 0.64, so a full climb stays about as rare as
it was on the shorter ladder while each individual step gets likelier.

### The multiplier canon

Every lane permitted to touch outgoing player damage.

| Lane | Bucket / cap |
|---|---|
| Flat | Summed first |
| Added weapon damage | Literal hit damage after item-level base scaling, before falloff, secondary-copy fractions and source Increased/More; weapon-delivered melee shares this base |
| Added ability power | Literal ability base after item-level scaling, before delivery power/falloff and Increased/More; snapshots inherit it once, utility zeroes remain zero |
| Maintained weapon flat | Metronome and Conduit source contributions add to shot base before Increased/More; ability-tagged, melee and DoT requests excluded |
| Increased — Weapon | One additive bucket, no cap of its own |
| Increased — Ability | One additive bucket, no cap of its own |
| Increased — Shared | Joins whichever of the two applies |
| More | Strongest three gear/tree sources; ONE 1.30^3 ceiling across delivery, critical/beyond-first weapon hits, DoT, Elemental, Void, Reaction and Effective Health scopes |
| Crit | Site multiplier, build-gated |
| Weak point | Site multiplier, skill-gated, [1.0, 2.0], outside the More budget |
| Weak point, gate removed | A build multiplier. Inside the accounting the skill gate stood in for, and crit does not also multiply on that hit |
| Distance falloff | Per-pellet geometry, not a stat-layer multiplier |
| Effective range | Primary Increased percentages add; scale falloff distances and maximum travel together, never close-range damage |
| Magazine capacity | Primary Increased percentages add; round base capacity down before temporary round deltas, conserve ammunition |
| Sustained accuracy | Primary Increased divisor on accumulated bloom only; base spread, movement and recoil unchanged |
| Gear pierce | Primary hitscan count, exact tier steps; additional impacts use existing penetration falloff and total travel budget |
| Fire rate | Named, watched, uncapped |
| Reload and swap tempo | Strongest active source on each axis; divide authored duration by speed, snapshot when the action starts, conserve ammunition |
| Target-conditional riders | The same additive bucket, never a multiplier |
| DoT | The same additive bucket as direct damage |
| Ability duration | Generic and matching zone/window or buff/window Increased contributions add once before the zero floor; snapshot at activation, excluding animation locks and fixed Afterimage tails |
### The bands
The ratio between a baseline and optimized build at the same area level is authored at two points because the two are different games.

| Band | Where | Target |
|---|---|---|
| At cap | Level 50, tiers a level-50 drop can produce | 8–10x |
| Endgame | Item level 120, producible tiers | 12–20x |

Rewrite-impact ceilings re-anchor per band. The band is earned across the
additive bucket, crit and the More product together, so no single stat is the
whole build.

### Experience

An accelerating curve with per-act multipliers and one deliberate discontinuity
— a discount across the act II/III seam, the only non-monotonic thing in the
design. Roughly 40 hours solo to 50: short enough that a second character is
plausible when class selection is permanent, long enough that the tree
decisions are made by someone who has learned the combat.

Kill value derives from one unit, with rank as the multiplier, so a single knob
retunes the whole game. No rested experience, no death penalty, party-neutral,
and an account-wide flat catch-up for alts — the one sanctioned exception to the
additive bucket, and there is not a second.

Points: one Core Point per level to 50 plus fifteen from world content.
Eight Doctrine points come from four authored campaign benchmarks.

**There is no experience at cap.** A currency drops instead — a conversion
would be a post-cap progression track wearing a currency's clothes.

## Boundaries

This spec owns curves, composition, pools and bands. Damage resolution belongs
to **combat**; loot and treatment to **items and crafting**; node vocabulary
to **progression and trees**; resource loops to **classes and abilities**;
encounter ranks and modifiers to **content and modes**.

## Asserted invariants

| Invariant | Test |
|---|---|
| Baseline TTK is flat across area level 1–50 | `Combat.PowerCurve.Composition` |
| Baseline trash TTK lands under its seed, and elite TTK inside its band | `Combat.PowerCurve.TrashTtk`, `Combat.PowerCurve.EliteTtk` |
| Baseline TTK is flat across 50–100, and TTD holds its level at both ends | `Combat.PowerCurve.EndgameComposition`, `Combat.DefenseCurve.TimeToDieBare` |
| The chassis is monotonic and geometric; rank ordering holds | `Combat.Chassis.*` |
| Weapon base tracks item level; archetype ordering survives scaling | `Weapons.ItemLevelCurve`, `Weapons.ItemLevelTracksMonsterHealth` |
| The at-cap band lands in 8–10x | `Progression.PowerBand.AtCap` |
| The endgame band lands in 12–20x | `Progression.PowerBand.Endgame` |
| Rewrite impact stays under its per-band ceiling | `Progression.RuleBandImpact.Step` |
| Gear and tree sources share three slots and the composed More ceiling | `Attributes.JointMoreSelection`, `Attributes.ScopedMoreCompetition`, `Items.ReserveSurgeRuntime` |
| An ability-lane More counts inside the same ceiling as a weapon-lane one | `Progression.PowerBand.AbilityLaneMore` |
| A hit whose weak-point gate was removed does not also take crit | `Combat.Ceiling.GateRemoval` |
| Every damage submission passes through the outgoing-modifier chain | `Combat.Ceiling.AbilitySubmissionConformance` |
| The weapon and item-level ceilings are equal | `Items.TierLadder` |
| Items rolled before the ladder widened keep their rolled values | `Items.LegacyItemsSurviveTheWiderLadder` |
| Ability throughput sits within the parity band of weapon throughput at level 50 | `Progression.PowerBand.AbilityLane` |
| A weapon hit draws weapon plus shared; an ability draws ability plus shared, and neither reads the other's specific pool | `Combat.Pools.Composition` |
| A source authors at most one specific pool, plus optionally the shared one | `Combat.Pools.OneSpecificPoolPerSource` |
| Best-to-worst build TTK spread stays inside the band, and is not explained by weapon archetype alone | `Progression.PowerBand.ArchetypeSpread` |
| Hits-to-die does not fall, and a defensive commitment buys substantially more | `Combat.DefenseCurve.HitsToDie`, `Combat.DefenseCurve.TimeToDieInvested` |
| Monster damage growth stays materially below health growth | `Combat.Chassis.DamageBelowHealth` |
| Boss TTK for a baseline build lands inside its target band | `Combat.PowerCurve.BossBand` |
| No single boss attack kills the baseline from full; the boss damage row is x1.0 | `Combat.DefenseCurve.BossHitsToDie` |
| An optimized build kills a boss substantially faster than the baseline band | `Combat.PowerCurve.BossOptimized` |

## Open

- The conditional-line payout ratio is unauthored and waits on measurement.
- The tier-bonus curve past its ordinary cap — how many endgame tiers reach
  item level 120 — is unauthored.
