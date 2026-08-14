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

The architecture was agreed under O27 and written before the code so that the
numbers become derivable. Implementation order: monster chassis by area level,
weapon base damage by item level, then the affix and node breadth that makes
the variance band reachable.

| §  | Piece | Status |
|---|---|---|
| 1 | Area level as the content axis | not built |
| 2 | Monster chassis by area level (`g`, `d`, rank multipliers) | in progress, `Combat/` |
| 3 | **Base weapon damage by item level (the multiplicand)** | **BUILT — `Weapons/`** |
| 3 | Multiplier band (Increased / More / crit) | unchanged, already correct |
| 4 | Build variance band 8-10x | not authored |
|  | Choices over accumulation (`IncreasedDamagePerSpentPoint`) | not built |
|  | Affix and node breadth | in progress, `Items/` |

### §3 multiplicand — built

`WeaponBase(ilvl) = ArchetypeBase * (1 + w)^(ilvl - 1)` lives in
`FBreakerWeaponMath::ItemLevelDamageScalar` / `WeaponBaseDamage` as a pure,
world-free function, and every damage path in `UBreakerWeaponComponent` reads
it: the hitscan pellet loop, the rocket projectile's payload, and the Bleed
DoT's base per tick.

- **`w` = 0.09/level**, chosen equal to the `g` this document proposes, so a
  baseline build's TTK is level-invariant and all felt progression comes from
  the multiplier band. It is `EditAnywhere` and O2 PLACEHOLDER, and 0 restores
  the flat pre-curve behaviour for A/B.
- **`g` is assumed, not read.** The monster chassis is another layer's work
  this same session. The relationship, not the value, is what the tests pin: if
  `w < g` baseline TTK climbs with level, if `w > g` it falls. **If the Combat
  layer lands a different `g`, `w` should be moved to match it** — that is one
  editable property, not a code change.
- Item level 1 is the anchor (scalar exactly 1.0), so no previously measured
  TTK moves and the zero-setup gym is unaffected. An unequipped weapon is item
  level 1 for the same reason.
- One shared exponent means the archetype table keeps its shape: a sniper
  out-hits an SMG per shot at every level by the same ratio.
- Item level reaches the weapon through the owner's equipment component, weapon
  slot 1 <-> `Primary` and slot 2 <-> `Secondary`. OPEN: an item instance
  carries no weapon archetype, so which of the five guns a Primary item *is*
  is still unanswered; only item level crosses the boundary today.

Details and the full rationale are in `Docs/Weapon-Foundation.md`; coverage is
`RiorsEdge.Weapons.ItemLevelCurve`, `.ItemLevelTracksMonsterHealth`,
`.ArchetypeOrderingAcrossLevels` and `.EquippedItemLevel`.
