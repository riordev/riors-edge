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

**There is ONE More ceiling and it spans all three pools.** A build holds at
most three More multipliers, each at most 1.30x, composing as an unordered
product to 1.30^3. The ceiling is derived from those two numbers rather than
restated as a third constant that can drift, and it is clamped globally across
every contributor so a layer arriving second cannot buy past it. An ability-lane
More counts inside the same budget as a weapon-lane one; a per-lane ceiling
would be the second budget the single ceiling exists to delete.

**Temporary ability windows ARE Mores** and compete for the same headroom. On a
build already holding three, a window buys little. That competition is the
point.

**Crit and weak point are the two site multipliers.** Crit is build-gated,
weak point is skill-gated and bounded per archetype in [1.0, 2.0] and sits
deliberately outside the More budget. Nothing else may multiply at the hit
site.

**A new multiplier lane requires a canon row and a conformance test before it
merges.** The canon below is a standing discipline, not a one-time cleanup.

**Conditional lines are texture, not the route to power.** The backbone is
unconditional. A build must be able to be strong satisfying no conditions at
all; composition makes a line more characterful, not more powerful, and a
two-condition line is worth more than a one-condition line only by a little.

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
    MonsterDamage(AL) = BaseDamage * (1 + d)^(AL - 1) * Rank
    WeaponBase(ilvl)  = ArchetypeBase * (1 + w)^(ilvl - 1)

`w = g`, so a baseline build's TTK is level-invariant and every felt gain comes
from the multiplier band. `d` is materially below `g`: incoming damage scaling
as fast as health means defence must grow as fast as offence, and every build
becomes a defensive build.

| Constant | Value | Note |
|---|---|---|
| `BaseHealth` | 220 | Area level 1 is the chassis that was actually measured |
| `BaseDamage` | 14 melee / 16 ranged | Authored per archetype at area level 1 |
| `g` health growth | 0.09 | x68 over 50 levels |
| `d` damage growth | 0.055 | x14 over 50 levels |
| `w` weapon growth | 0.09 | Equal to `g` by design, not by coincidence |

Item level 1 is the anchor: the scalar is exactly 1.0 there, so the curve is
opt-in by content rather than a silent retune of the only content anyone has
played. An unequipped weapon is item level 1 for the same reason.

### Rank

| Rank | Health | Damage |
|---|---|---|
| Trash | x1 | x1 |
| Veteran / elite | x3.0 | x1.5 |
| Modifier-bearing | x2.5, +0.35 per modifier beyond the first | x1.25 |
| Boss | x75 | x2.0 |

The ratios are derived from the targets rather than guessed. Trash under a
second against elite at three seconds *is* an elite health ratio of three.

### Targets

| Target | Seed value |
|---|---|
| Trash TTK | a little under 1s, scaling exponentially with difficulty |
| Elite TTK | ~3s |
| Boss TTK | 20–45s, unless a special enemy claims the exception explicitly |
| TTD, no resources or sustain | 4–5s |
| TTD, invested | substantially higher |

These describe a **baseline build in on-level content** — one point on two
curves, not a constant to hold at every point of progression. An optimized
character roughly forty hours past 50 deletes trash on contact.

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
| Increased — Weapon | One additive bucket, no cap of its own |
| Increased — Ability | One additive bucket, no cap of its own |
| Increased — Shared | Joins whichever of the two applies |
| More | ONE ceiling, unordered product, 1.30^3, spanning every pool |
| Crit | Site multiplier, build-gated |
| Weak point | Site multiplier, skill-gated, [1.0, 2.0], outside the More budget |
| Distance falloff | Per-pellet geometry, not a stat-layer multiplier |
| Fire rate | Named, watched, uncapped |
| Target-conditional riders | The same additive bucket, never a multiplier |
| DoT | The same additive bucket as direct damage |

### The bands

The ratio between a baseline build and an optimized one at the same area level,
authored at two points because the two are different games.

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
design. Roughly 40 hours solo to 50: short enough that a second character is a
plausible thing to do when class selection is permanent, long enough that the
tree decisions are made by someone who has learned the combat.

Kill value derives from one unit so a single knob retunes the whole game, with
rank as the multiplier. No rested experience, no death penalty, party-neutral
with no headcount bonus, and an account-wide flat catch-up for alts — the one
sanctioned exception to the additive bucket, and there is not a second.

Points: one Class Point per level to 30, one Core Point per level to 50, plus
fifteen from world content.

## Boundaries

This spec owns the curves, the composition law, the pools and the bands. It
does not own:

- the order damage resolves in, armour composition, or the proc-coefficient
  law — **combat**;
- the affix roster, the rarity ladder, drop rates or crafting — **items and
  crafting**;
- which stat targets and conditions exist, and what a node may author —
  **progression and trees**;
- resource loops, ability costs and cooldowns — **classes and abilities**;
- rank composition within an encounter, and modifier selection — **content and
  modes**.

## Asserted invariants

| Invariant | Test |
|---|---|
| Baseline TTK is flat across area level 1–50 | `Combat.PowerCurve.Composition` |
| Baseline TTK is flat across area level 50–100 | `Combat.PowerCurve.EndgameComposition` |
| The chassis is monotonic and geometric; rank ordering holds | `Combat.Chassis.*` |
| Weapon base tracks item level; archetype ordering survives scaling | `Weapons.ItemLevelCurve`, `Weapons.ItemLevelTracksMonsterHealth` |
| The at-cap band lands in 8–10x | `Progression.PowerBand.AtCap` |
| The endgame band lands in 12–20x | `Progression.PowerBand.Endgame` |
| Rewrite impact stays under its per-band ceiling | `Progression.RuleBandImpact` |
| The composed More product never exceeds the ceiling, from any combination of layers | `Items.Rules.NeverAuthorsAMore` |
| An ability-lane More counts inside the same ceiling as a weapon-lane one | `Progression.PowerBand.AbilityLaneMore` |
| Every damage submission passes through the outgoing-modifier chain | `Combat.Ceiling.AbilitySubmissionConformance` |
| The weapon and item-level ceilings are equal | `Items.TierLadder` |
| Items rolled before the ladder widened keep their rolled values | `Items.LegacyItemsSurviveTheWiderLadder` |
| Ability throughput sits within the parity band of weapon throughput at level 50 | `Progression.PowerBand.AbilityLane` |
| Best-to-worst build TTK spread stays inside the band, and is not explained by weapon archetype alone | `Progression.PowerBand.ArchetypeSpread` |
| Hits-to-die does not fall across the level range | `Combat.DefenseCurve.HitsToDie` |
| Boss TTK lands inside its target band | `Combat.PowerCurve.BossBand` |

The last four are targets this spec asserts and the game does not currently
meet. They are held by tests so the gap is a number on every build rather than
a memory.

## Open

- The conditional-line payout ratio is unauthored; the direction is stated
  above and the number waits on measurement.
- The tier-bonus curve past its ordinary cap — how many endgame tiers reach
  item level 120 — is unauthored.
