# The power curve

Authority: ruling **O27** in `Decisions.md`. This document is the architecture
behind it. Every constant here is an `O2 PLACEHOLDER` shape, not a balance
sheet — the point of the document is that the numbers become *derivable* rather
than guessed, which is the thing the project has never had.

## Why this document exists

The owner reported, after session 5: *"fundamentally when I have full level 50
gear + max skill points I should be doing SIGNIFICANTLY more … I don't know if
our approach is wrong or if our numbers are wrong but it feels really off."*

The approach was wrong, in three specific and confirmed ways:

1. **`EnemyLevel` drove nothing.** It exists on every enemy, is clamped 1–50,
   and waves increment it — but it was read in exactly one place: the item
   level of the loot that enemy drops. It never touched health or damage.
2. **Monster health was a constant.** `SetMaxHealth(220.0f)`, identical at
   level 1 and level 50. Elite was a hardcoded 440.
3. **Weapon base damage did not scale with item level.** `Weapons/` contained
   no reference to `ItemLevel` at all. Base damage was an archetype constant
   (rifle 13, sniper 72, shotgun 90/pellet), so an ilvl 1 weapon and an ilvl 50
   weapon hit identically. Item level moved only affix tier values.

So the player's power grew roughly 3–5x across the whole game and spent all of
it on an enemy that never changed, while the single number the design leaned on
— "trash TTK <1s" — was a scalar being asked to describe a curve. Enforcing it
at level 1 *and* at level 50 mathematically forbids progression from being
felt: the only way to satisfy it everywhere is for enemies to scale exactly as
fast as the player, which is the failure mode the ruling explicitly rejects.

**TTK is an output of two curves. It cannot be an input.**

## The four axes

### 1. Area level — the content axis

Area level is the single input that describes how hard a piece of content is.
It drives monster health, monster damage, and the item level of drops. It is
**never** derived from the player's level, gear, or build. Content-scaling
preserves the loop; player-scaling destroys it, because a build that changes
nothing observable is not a build.

Character level caps at 50 with no post-cap power (locked). Area level does not
stop there — endgame tiers keep climbing, and because area level drives drop
item level, climbing tiers is what keeps gear improving after the level cap.
That is the mechanism by which "all endgame character power comes from gear"
actually functions.

### 2. Monster chassis — health and damage as functions of area level

Geometric, not linear. Linear scaling collapses: early levels feel identical
and late levels feel unreachable.

```
MonsterHealth(AL) = BaseHealth * (1 + g)^(AL - 1)
MonsterDamage(AL) = BaseDamage * (1 + d)^(AL - 1)
```

`g` around 9% gives roughly ×67 over 50 levels; `d` must be materially smaller
than `g`, because incoming damage scaling as fast as health means defence has
to grow as fast as offence and every build becomes a defensive build.

**Monster rank multiplies the chassis, it does not replace it.** Rank is where
difficulty lives, per O27:

| Rank | Health | Damage | Notes |
|---|---|---|---|
| Trash | ×1 | ×1 | Exists to be trivialized by an optimized build |
| Elite | ×3–4 | ×1.5 | Current `ConfigureElite` is the seed of this |
| Modifier-bearing | ×2–3 + mods | varies | The mods are the threat, not the health |
| Boss | ×20–40 | ×2 | O18's 20–45s target lives here |

### 3. Player offence — the multiplicand and the multipliers

This is the part that was missing entirely, and it has two halves.

**Base weapon damage scales with item level.** This is the primary power axis
in any ARPG and it does not exist yet:

```
WeaponBase(ilvl) = ArchetypeBase * (1 + w)^(ilvl - 1)
```

`w` should track `g` closely. If base weapon damage grows at the same rate as
monster health, then a *baseline* build holds a roughly constant TTK across the
whole game — and every bit of felt progression comes from the multiplier band
below. That is the clean separation: the base curve keeps the game playable at
every level, and the build decides whether you are crushing it.

**Multipliers ride on top**, under the locked aggregation rule: flat sums
first, one additive Increased bucket per stat, More multipliers reserved for
trees and Anomalous under the O3 ceiling of three.

### 4. The build variance band — where builds actually live

The ratio between a baseline build and an optimized one at the *same* area
level. This is the number O27 is really about, and it needs to be authored
explicitly rather than emerging by accident.

Target: **roughly 8–10×**, reachable within the existing rules:

| Layer | Baseline 50 | Optimized 50 | Ratio |
|---|---|---|---|
| Additive Increased bucket | +100% (×2.0) | +400% (×5.0) | 2.5× |
| More multipliers (O3 cap 3) | ×1.0 | ×1.95 | 1.95× |
| Effective crit | ×1.3 | ×2.2 | 1.7× |
| **Composed** | | | **≈8.3×** |

Two things follow from that table. **O3's cap of three More multipliers does
not need to be broken** — the band is achievable with it intact, provided the
additive bucket has real range and crit is a genuine axis rather than a
rounding error. And the band is *earned across all three layers*, so no single
stat is the whole build.

## What the targets become

O18's numbers survive, restated as statements about a point on the curve rather
than global invariants:

- **Baseline build, on-level content**: trash under ~1s, elite ~3s, boss
  20–45s. This is what "hitting 50 is satisfying with decent power" means.
- **Optimized build (~40h past 50), on-level content**: trash deletes on
  contact — call it under 0.15s, effectively one trigger pull. Elites become
  brief, bosses stay a fight. This is "optimized 50 feels great."
- **Difficulty lives in rank and modifiers**, not in trash health. A player who
  has earned the 8× should feel it on trash and still respect a modifier pack.

The measurement discipline stays: the F2 report already splits melee trash,
ranged trash and elite TTK, and the weak-point forgiveness halo inflates damage
per hit by 8–14%, so a measuring run should set `WeakPointToleranceCm = 0`.

## Choices over accumulation

O27 is explicit that choices must beat accumulation. Two concrete consequences:

- `IncreasedDamagePerSpentPoint` currently contributes about +69% at a full
  point budget, against roughly +19% from every damage node combined — so *how
  many* points you have spent matters about 3.5× more than *where*. That is
  backwards for a build game. The baseline drops to near zero and the power
  moves into nodes.
- Nodes need to offer real, differentiated choices rather than a uniform
  percentage. This game's pillar is movement, so conditional damage that keys
  off the movement state (airborne, sliding, wall-riding, at Redline Momentum)
  is the obvious place for build identity to live, and it is currently absent.

## More options in every avenue

The slice is far too thin for builds to be interesting, and O27 calls this out
directly. Concretely, today:

- **One damage affix exists** (`Offense.WeaponDamage`), and it rolls on 4 of 8
  slots — helmet, body, boots and waist are structurally incapable of
  increasing damage.
- Twelve affix types total, of which one is offensive.
- No conditional or build-defining affixes at all.

The pool needs breadth on every axis — flat and increased damage, crit in both
directions, conditional damage tied to the movement pillar, and per-slot
identity so that gearing is a set of decisions rather than a search for one
line.

## Implementation status (2026-08-13)

The implementation order was: monster chassis by area level, weapon base damage
by item level, then the affix and node breadth that makes the variance band
reachable. **The third item is built.** The first two are not.

**BUILT — §4 the build variance band, §"Choices over accumulation", §"More
options in every avenue".**

- `IncreasedDamagePerSpentPoint` cut 1.0% -> **0.25%**. Still `EditAnywhere`,
  still safe at 0. Because every build spends its whole budget, it now lands
  identically on a baseline and an optimized character and cannot separate
  them — it is a floor so that a defensive purchase is not literally zero
  offence, and nothing more.
- The affix pool went 12 lines / 1 offensive / 4 of 8 slots to **18 lines, 9
  offensive, damage on all 8 slots**, with per-slot identity. Full table in
  `Docs/Item-Foundation.md`.
- **Conditional damage exists**, keyed to the movement pillar: five affixes and
  six node lines that pay out only while airborne, sliding, wall riding, at
  Redline, or shortly after a dash. `FBreakerBuildConditionState`
  (`Progression/BreakerBuildConditions.h`) reads the live state off
  `UBreakerCharacterMovementComponent` and `UBreakerMomentumComponent`; both
  power layers consume it and neither of those components was edited.
- **More multipliers are real on nodes** and the O3 cap is enforced in code:
  six Convergence/keystone options, of which the strongest three count, each
  clamped to 1.30x. Six options against a cap of three is the choice O3
  describes.
- Node content: Core 15 -> **24** (a new Velocity constellation of six, plus
  Called Shot, Salvo and Barrage), Swift Kinetic 8 -> 11, Marksman 8 -> 10.

**The band, measured** (`RiorsEdge.Progression.PowerBand`, composed through the
real aggregator; both characters level 50, both measured in the same instant —
airborne, recently dashed, at Redline):

| Layer | Baseline 50 | Optimized 50 | Ratio | Doc target |
|---|---|---|---|---|
| Flat (Added Damage) | x1.08 | x1.25 | 1.16x | not in the original table |
| Additive Increased | x2.42 | x5.68 | 2.35x | 2.5x |
| More (O3 cap 3) | x1.00 | x1.94 | 1.93x | 1.95x |
| Effective crit | x1.32 (27% @ x2.18) | x2.19 (60% @ x2.98) | 1.66x | 1.7x |
| **Composed** | **x3.44** | **x30.05** | **8.74x** | **8-10x** |

The one deviation from §4's table is the flat layer, which the table does not
have. Added Damage lands in the Flat lane of the `DamageMultiplier` attribute,
so it is multiplied by the additive bucket rather than added after it; it is
kept deliberately small (1.16x) so it colours the band rather than carrying it.
**O3 was not broken to reach the band**, exactly as §4 predicted.

**NOT BUILT.** The two structural gaps this document names first are untouched
by this pass and remain the next work: monster health and damage are still
constants rather than functions of area level, and weapon base damage still
does not scale with item level. Until those land, the band is a statement about
the multiplier stack only — an optimized character deals 8.74x a baseline
character's damage, against an enemy whose health does not yet know what level
the area is.
