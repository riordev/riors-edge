# The power curve

> STATUS 2026-08-16: PARTIALLY BUILT — the curve is implemented as O2-placeholder code defaults; both OPENs near the end are closed by O36 and the EnemyLevel re-clamp recorded at :485-495 was fixed on 2026-08-14, so read the AS BUILT section at the end before acting on anything past §7.

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

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
stop there: it climbs to 100.

**What carries endgame power is GEAR DEPTH, ruled by O29 on 2026-08-14.** This
paragraph used to claim the mechanism and the code did not implement it, which
is what produced the 74x gap recorded at the end of this document. The claim is
now true, and it is true in three specific places:

1. **Item level runs to 120**, past the character cap of 50 and past the
   area-level ceiling of 100. `UBreakerAffixLibrary::MaxItemLevel` is the single
   authority and `FBreakerWeaponMath::MaxSupportedItemLevel` equals it by test.
2. **Drop item level tracks area level**, so an on-level character's
   `WeaponBase(ilvl)` climbs at exactly the rate `MonsterHealth(AL)` does and
   the two cancel term for term across the whole game rather than only across
   the first fifty levels.
3. **The affix tier ladder widened from T8..T-1 to T12..T-1** and stopped being
   linear. The curve is back-loaded so the step grows from +14.8% at the bottom
   to +36.5% at the top, which is what makes a top-tier roll an event rather
   than one more step. The item-level-to-tier mapping is **two-slope** after an
   owner ruling ("the item level tier capping at 8 might make for awkward
   feeling progression, let's bring that to 6"): **T12 -> T6 across item levels
   1-50** at ~8.2 levels per tier, then **T6 -> T1 across 50-120** at ~14. So a
   player finishing the levelling game has crossed HALF the ladder rather than a
   third of it, and **T1 opens at item level 120 exactly**. Full derivation and
   the value table are in `Docs/Item-Foundation.md`.

Three properties of that choice are worth stating, because each was a live
alternative O29 rejected:

- **No new multiplier lane.** Affix magnitudes stay strictly inside tier ranges,
  so O3's cap of three More multipliers is untouched. Endgame power is the same
  arithmetic, deeper.
- **No post-cap character power.** A paragon-style tree was rejected outright:
  it collides with the locked no-post-cap rule and is pure accumulation against
  O27's "choices beat accumulation".
- **Existing items are not migrated.** A rolled affix stores its `Tier` and
  `Value`, so widening the ladder cannot invalidate one. Items rolled before O29
  keep the values they rolled and read as weak - a pre-O29 T1 is worth about
  what a post-O29 T4 is. That is correct, it is what a deeper ladder MEANS, and
  `RiorsEdge.Items.LegacyItemsSurviveTheWiderLadder` verifies it rather than
  assuming it.

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

This is the part that was missing entirely when this document was written. Both
halves are now **BUILT** — see the "§3 multiplicand" section below for what
shipped; the architecture is kept here because it is what the code implements.

**Base weapon damage scales with item level.** This is the primary power axis
in any ARPG:

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

O27 is explicit that choices must beat accumulation. Two concrete consequences,
**both now acted on**:

- `IncreasedDamagePerSpentPoint` used to contribute about +69% at a full point
  budget, against roughly +19% from every damage node combined — so *how many*
  points you had spent mattered about 3.5× more than *where*. That is backwards
  for a build game. **It is now 0.25%/point** (EditAnywhere, zeroable): a floor
  so a purely defensive purchase is not literally zero offence, and because
  every build spends its whole budget it cannot differentiate two builds.
  `RiorsEdge.Progression.PowerBand` asserts that removing it widens the band
  rather than narrowing it, which is the assertion that notices if anyone
  raises it back toward 1.0.
- Nodes needed real, differentiated choices rather than a uniform percentage.
  This game's pillar is movement, so conditional damage keyed off the movement
  state is where build identity lives. **Built**: `EBreakerBuildCondition` is
  `Always / Airborne / Sliding / WallRiding / Redline / RecentlyDashed`, with
  five conditional affix lines and six conditional node lines against it.
  **It is still MOVEMENT-ONLY**, which is the constraint O30 names: no node or
  affix can key off combat or status state, and that has now blocked content
  twice.

## More options in every avenue

The slice was far too thin for builds to be interesting, and O27 calls this out
directly. **What this section described has been fixed; the diagnosis is kept
because it is the standard the pool is held to.** As it stood:

- **One damage affix existed** (`Offense.WeaponDamage`), rolling on 4 of 8
  slots — helmet, body, boots and waist were structurally incapable of
  increasing damage.
- Twelve affix types total, of which one was offensive.
- No conditional or build-defining affixes at all.

**Now: 24 lines, 11 offensive, damage on all eight slots**, with per-slot
identity, five conditional lines, four rollable Anomalous rule rewrites and
three authored legendaries. `RiorsEdge.Items.Affixes.Breadth` asserts that no
slot is structurally incapable of raising damage and that every slot has a
conditional line of its own, so this failure cannot come back silently. The full
pool table is in `Docs/Item-Foundation.md`.

The standard the pool is still held to: breadth on every axis — flat and increased damage, crit in both
directions, conditional damage tied to the movement pillar, and per-slot
identity so that gearing is a set of decisions rather than a search for one
line.

## Implementation status

**ALL THREE ITEMS ARE BUILT** (chassis 2026-08-13, weapon curve 2026-08-13,
breadth 2026-08-13/14). The section below is the running log, oldest entry
first; where an entry says something is not built, a later entry supersedes it
and says so. One thing in the chain is still open and it is neither of the
curves — see "the last clamp" at the end of this document.

**BUILT — §4 the build variance band, §"Choices over accumulation", §"More
options in every avenue".**

- `IncreasedDamagePerSpentPoint` cut 1.0% -> **0.25%**. Still `EditAnywhere`,
  still safe at 0. Because every build spends its whole budget, it now lands
  identically on a baseline and an optimized character and cannot separate
  them — it is a floor so that a defensive purchase is not literally zero
  offence, and nothing more.
- The affix pool went 12 lines / 1 offensive / 4 of 8 slots to **18 lines, 9
  offensive, damage on all 8 slots**, with per-slot identity. (It has since
  reached **24 lines, 11 offensive** — Fire Rate, then the four non-damage
  lines, then Resource Efficiency.) Full table in `Docs/Item-Foundation.md`.
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

**The band as it measured on 2026-08-13, BEFORE O29** (`RiorsEdge.Progression
.PowerBand`, composed through the real aggregator; both characters level 50 in
item level 50 gear at T5 and T1, both measured in the same instant — airborne,
recently dashed, at Redline). **These figures are historical**: O29 widened the
ladder and the fixture has since moved to item level 120 at T3 and T1. See "the
band moved" below.

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

**NOT BUILT, as of that entry** — superseded the same week, and both entries
follow. The two structural gaps this document names first were untouched by that
pass: monster health and damage were still constants rather than functions of
area level, and weapon base damage still did not scale with item level. Until
they landed the band was a statement about the multiplier stack only.
**Both landed.** Read on.
### §2 Monster chassis — BUILT

`Source/RiorsEdge/Combat/BreakerMonsterChassis.{h,cpp}` is the curve, as pure
world-free maths in the precedent of `BreakerRangedBehavior.h` and
`BreakerWeaponMath.h`. It takes an area level, a rank, and an authored
parameter block, and it is *structurally* incapable of reading a player —
there is no world, no actor and no player pointer within reach of any function
in it. That is the guarantee, not a convention.

```
Health(AL) = BaseHealth * (1 + g)^(AL - 1) * Rank * Archetype
Damage(AL) = BaseDamage * (1 + d)^(AL - 1) * Rank * Archetype
```

| Constant | Value | Note |
|---|---|---|
| `BaseHealth` | 220 | The chassis session 5 actually measured, so area level 1 is bit-identical to the shipping enemy |
| `BaseDamage` | 14 (melee) / 16 (Lattice) | The archetype's authored damage at area level 1 |
| `g` `HealthGrowthPerLevel` | 0.09 | ×68.2 over 50 levels |
| `d` `DamageGrowthPerLevel` | 0.055 | ×13.8 over 50 levels — materially below `g` |
| Elite rank | ×3.0 health, ×1.5 damage | |
| Modifier-bearing rank | ×2.5 health, ×1.25 damage | |
| Boss rank | ×25 health, ×2.0 damage | |
| Lattice archetype | ×1.6 health | Encounter-Design §2.2, applied now that O27 has landed |

All are `O2 PLACEHOLDER` and all are `EditAnywhere` on
`FBreakerMonsterChassisParams`, which every enemy carries.

The rank ratios are **derived from O18's targets rather than guessed**, which
is the point of the document. Trash under ~1s and elite ~3s *is* an elite
health ratio of 3. A boss at 20–45s against a 1s trash *is* a ratio inside
20–45; 25 was picked at the low end. The shipping `ConfigureElite` used ×2.0
health and still measured 3.01s only because trash was simultaneously 1.81×
too slow — the two errors were cancelling.

`ConfigureElite`'s hardcoded 440 health and `AttackDamage *= 1.5f` are gone,
folded into that rank table so there is one source of truth for what an elite
is. `bIsElite` is gone too: rank is the flag.

**Area level is authored on the content.** `ABreakerEnemy::AreaLevel`
(EditAnywhere, 1–100 — deliberately past the character cap of 50, because
endgame tiers keep climbing and that is what keeps drop item level improving
after the cap) and `ABreakerGameMode::GymAreaLevel` / `AreaLevelPerWave`, so a
playtest walks the curve by turning a number up rather than by levelling a
character. Wave escalation is the same arithmetic it always was
(`10 + wave × 2`), now expressed through those two properties.

`EnemyLevel` still drives loot item level and follows area level. **It is still
clamped to 50, on the actor** (`ABreakerEnemy::ApplyChassis`:
`EnemyLevel = FMath::Clamp(EnemyLevel, 1, 50)`), and the field's comment still
gives the retired reason — affix tiers being authored only to 50. O29 authored
them to 120. **That clamp is now the one thing standing between the item system
and the endgame curve**; see "the last clamp" below.

Three automation tests (`RiorsEdge.Combat.Chassis.*`) cover monotonicity and
the geometric identity, the rank table and its ordering, and the ruling
itself: the same area level produces the same chassis every time, and the drop
item level follows area level.

### The shape of the curve, for an unchanged player

Session 5 measured melee trash at 1.81s against 220 health, i.e. ~121.5 hp/s
effective (weak-point halo included), and elite at 3.01s against 440, i.e.
~146 hp/s. Holding those numbers fixed — **no weapon change at all** — the
chassis produces:

| Area level | Trash health | Trash TTK | Elite health | Elite TTK | Trash hit |
|---|---|---|---|---|---|
| 1 | 220 | 1.81s | 660 | 4.5s | 14 |
| 5 | 311 | 2.56s | 932 | 6.4s | 17 |
| 10 | 478 | 3.93s | 1 434 | 9.8s | 23 |
| 20 | 1 131 | 9.3s | 3 394 | 23.2s | 39 |
| 30 | 2 678 | 22.0s | 8 034 | 55.0s | 66 |
| 50 | 15 003 | 123.4s | 45 010 | 308s | 193 |

That table is deliberately alarming and is **not** a balance failure: it is
one half of a ratio. The other half is §3's `WeaponBase(ilvl)` curve, which is
a separate task. With `w = g`, a baseline build's TTK is *flat* at 1.81s at
every area level, and the whole table collapses to its first row — at which
point the remaining 1.81× to O18's <1s target is a weapon-side anchor
question, exactly as O27 says it should be.

**That was true only until the weapon curve landed, which it has.** With
`w = g = 0.09` the table above collapses to its first row for a baseline build
in on-level gear, and `RiorsEdge.Combat.PowerCurve.Composition` asserts exactly
that across area levels 1-50. `GymAreaLevel` defaults to 10; set it to 1 to
recover the level-1 combat feel exactly.

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
  slot 1 <-> `Primary` and slot 2 <-> `Secondary`. ~~OPEN: an item instance
  carries no weapon archetype~~ **CLOSED**:
  `FBreakerItemInstance::WeaponArchetype` is drawn on a weapon drop before its
  affixes and armed on equip by
  `UBreakerWeaponComponent::SyncArchetypesToEquipment`, so a dropped Primary is
  a specific gun and not a stat sheet. There are **eight** archetypes, not
  five — Rifle, SMG, Sniper, Shotgun, Rocket Launcher, Burst Rifle, Machinegun,
  Sidearm.

Details and the full rationale are in `Docs/Weapon-Foundation.md`; coverage is
`RiorsEdge.Weapons.ItemLevelCurve`, `.ItemLevelTracksMonsterHealth`,
`.ArchetypeOrderingAcrossLevels` and `.EquippedItemLevel`.

### §4 and the affix/node breadth — BUILT

See the §4 entry above: the accumulation baseline is cut to 0.25%/point, the
affix pool has damage on all eight slots, and `RiorsEdge.Progression.PowerBand`
pins the ratio so a future tuning pass cannot flatten builds silently. The band
measured 8.74x when that entry was written; O29 moved it and the test is now
red — see "the build variance band moved" below for the current reading.

## CLOSED: the endgame power source is gear depth [O29 2026-08-14]

This section recorded the biggest open problem in the project. It is answered;
the history is kept because the arithmetic is what made the answer derivable.

### What the gap was

Found while writing `RiorsEdge.Combat.PowerCurve.Composition`, the first test to
compose the two curves rather than checking each half alone. Across the levelling
game the composition was **exact**: an unmodified character in on-level gear had
an identical baseline TTK at area level 1, 10, 25 and 50, because `w` and `g`
are both 0.09 and cancel term for term.

Past the character cap it broke, and the contradiction was between this document
and the code. Section 1 claimed drop item level was what kept gear improving
after the cap; `GetDropItemLevel` clamped to 50, for a sound local reason stated
at the function - affix tier tables were authored to 50 and rolling past the end
of the tier curve produces illegal items. Meanwhile the chassis kept climbing to
area level 100. Measured: baseline TTK **1.00x at area level 50 and 74x at area
level 100**, with nothing in the game to answer it. The build variance band was
8.7x, which does not come close, and it is the same band a level-50 player
already has - it was not endgame progression, it was the price of entry.

`1.09^50 = 74.4`. The gap was exactly the monster curve running for fifty levels
while the player's stood still.

### What closed it

O29 chose "extend the affix tier table past 50" from the five tabulated options,
and extended item level with it. The reason the clamp existed is no longer true,
so the clamp is the only thing left in the way:

    GetDropItemLevel(AL) = Clamp(ClampAreaLevel(AL), 1, UBreakerAffixLibrary::MaxItemLevel)

Since `ClampAreaLevel` already bounds area level to 1-100 and `MaxItemLevel` is
120, that is the **identity across the whole area-level range**, which is
precisely the property the composition needs: item level equals area level, so
`(1+w)^(ilvl-1)` and `(1+g)^(AL-1)` cancel at every point rather than only up to
50. It is one line in `Combat/BreakerMonsterChassis.cpp`.

### The composition, measured

`RiorsEdge.Combat.PowerCurve.EndgameComposition` walks area level 50 to 100 with
drop item level tracking area level. Baseline TTK is **1.00x at every step** -
flat to within a thousandth, because the cancellation is algebraic rather than
approximate. The 74x is gone, not reduced.

| Area level | Trash health | Item level | Weapon base | Relative baseline TTK |
|---|---|---|---|---|
| 1 | 220 | 1 | 13.0 | 1.000x |
| 25 | 1 740 | 25 | 102.8 | 1.000x |
| 50 | 15 008 | 50 | 886.8 | 1.000x |
| 75 | 129 415 | 75 | 7 647 | 1.000x |
| 100 | 1 115 953 | 100 | 65 943 | 1.000x |

**The growth rates do NOT need to differ above 50.** `w = g = 0.09` holds the
whole range on its own; the only thing that was ever wrong was the clamp. Any
divergence introduced above 50 would be a deliberate design choice about whether
the endgame should get gradually easier or harder, not a correction.

### What this does and does not fix

**Fixed, in `Items/` and `Weapons/`:** the item system supports the endgame
curve end to end. Items roll to level 120, the tier ladder covers it, the weapon
damage curve evaluates it, the Forge prices it, and every clamp to 50 is gone.

**NOT fixed — THE LAST CLAMP, and it moved rather than closed.**
`UBreakerMonsterChassisLibrary::GetDropItemLevel` **has** been fixed: it is now
`Clamp(ClampAreaLevel(AL), 1, MaxItemLevel)`, i.e. the identity across 1-100,
and `EndgameComposition` takes its green branch on it.

But `ABreakerEnemy::ApplyChassis` calls that function and then immediately
re-clamps its own field:

    EnemyLevel = GetDropItemLevel(AreaLevel) + (IsElite() ? EliteDropItemLevelBonus : 0);
    EnemyLevel = FMath::Clamp(EnemyLevel, 1, 50);

and `EnemyLevel` is what `GrantLoot` hands to `RollDrop` and `RollItem`. So
**no drop in the shipping game carries an item level above 50**, the 74x gap is
still live in play, and nothing in the suite fails on it — `EndgameComposition`
checks the library function, which is now correct, and never touches the actor.
The item system, the weapon curve and the Forge all support the endgame; the
enemy does not hand it out. Whether that clamp is a leftover or a deliberate
hold until item levels 101-120 have a source is an owner call, recorded at the
end of `Docs/Item-Foundation.md`.

**OPEN: item level 101-120 has no source.** Drop item level derives from area
level and area level stops at 100, so the top twenty item levels are reachable
only through O6's TierBonus (authored 0..+5, which does not cover it) or the
Forge. Those twenty levels are worth `1.09^20 = 5.6x` base damage over on-level
content - either the reward at the end of the chase, or twenty levels of dead
ladder. Owner ruling.

**OPEN: the build variance band moved, and O27 is its subject.**
`RiorsEdge.Progression.PowerBand` fails deliberately rather than being retuned.
Two things happened, and only the first is arithmetic:

- The back-loaded ladder widened the distance between a mediocre roll and a
  perfect one, and that compounds through the flat and crit lanes.
- More importantly, **the original fixture described a character that cannot
  exist.** It built item level 50 gear at T5 and T1, and at item level 50 the
  two-slope mapping reaches only T6 — neither tier is obtainable there at any
  rarity. The 8-10x band was measured on a pairing the loot pipeline could
  produce before O29 and cannot produce after it.

**The fixture has since moved to where builds actually compete**: item level
**120**, baseline **T3** against optimized **T1** — a gear spread of about
1.85x, close to the 1.97x the old T5-vs-T1 fixture had. It still fails, at
around **15x** against the authored 8-10x, and the extra did NOT come from the
gear spread. It came from absolute affix values roughly doubling, which moves
flat crit chance and the additive bucket into a different part of their own
curves for the optimized build specifically. That is a real consequence of O29,
not a fixture artefact. (The **23.70x** this section previously recorded was the
pre-move measurement at the impossible ilvl 50 fixture; it is superseded.)

So the honest reading is not that the band broke. **O29 moved where the band
lives**: the spread between a decent item and a perfect one is no longer
available at the character cap, it is available in the endgame, which is exactly
what "all endgame character power comes from gear" is supposed to mean. Two
rulings would each resolve it — the band target moves because the endgame is
longer, or the content retunes (crit and the additive bucket come down) until
the composed band lands back at 8-10x on the new ladder. **Widening the asserted
range is not a third option**; that is choosing the first without saying so.

Two smaller consequences, both reported rather than retuned:

- **PROLIFIC got stronger without being edited.** Its whole value is the size of
  a tier step, and T1 -> T0 went from 1.4x to 2.2x. Measured at x1.462 on an
  optimized build against an authored ceiling of x1.35;
  `RiorsEdge.Progression.RuleBandImpact` fails on it.
- **The movement affixes inherited the pool-wide 2.2x uplift.** Move Speed,
  Slide Speed, Air Control and Dash Cooldown all scaled with everything else, so
  the composed movement band in `Docs/Movement-Design.md` is now reachable from
  gear alone at high item level. That is a balance question for `Movement/`,
  called out rather than pre-emptively retuned.

### The five options, kept for the record

| Option | Shape | Cost | Outcome |
|---|---|---|---|
| Extend the affix tier table past 50 | Fewest moving parts; the mechanism works as written | Tier authoring to whatever the ceiling becomes; the T0/T-1 spike needs re-siting | **CHOSEN (O29).** The spike was re-sited to 2.2x/3.6x |
| An item level track above 50 with a separate value curve | Keeps the 1-50 tier table untouched | A second curve to author and keep aligned with `g` | Rejected |
| Ascended rarities above Anomalous | Rarity is already a strong felt axis | Rarity gates affix COUNT, not magnitude - a new rule | Rejected |
| A separate endgame multiplier (paragon-like) | Familiar, easy to tune | Collides with the locked "no post-cap character power" rule | **Rejected explicitly by O29** - also pure accumulation against O27 |
| Cap area level at 50 | Free; the contradiction disappears | Deletes the endgame tier ladder, and the reason to play past 50 | Rejected |

`RiorsEdge.Combat.PowerCurve.EndgameClamp` asserted the gap was still open and
carried an instruction to delete it when someone closed it. It is deleted, and
`RiorsEdge.Combat.PowerCurve.EndgameComposition` stands in its place.

## FOR THE OWNER — open on this document (2026-08-14)

Ranked by how much each blocks the others. None is decided here.

1. **The enemy's item level clamp.** `ABreakerEnemy::ApplyChassis` clamps
   `EnemyLevel` to 50 after `GetDropItemLevel` correctly returns up to 100. It
   is one line, it has no test on it, and until it moves the O29 endgame does
   not exist in play. Lifting it to `UBreakerAffixLibrary::MaxItemLevel` makes
   the measured composition describe the shipping game; leaving it holds the
   endgame closed until item 3 below is answered.
2. **The build variance band.** Authored 8-10x, measuring ~15x, deliberately
   red. Either the target moves or the content retunes. Every measured figure
   in this document and in `Item-Foundation.md` moves with the answer.
3. **Item levels 101-120 have no source.** Drop item level derives from area
   level and area level ceilings at 100, so the top twenty levels are reachable
   only through O6's TierBonus (0..+5, which does not cover it) or the Forge.
   Worth `1.09^20 = 5.6x` base damage: either the reward at the end of the
   chase, or twenty levels of dead ladder.
4. **PROLIFIC exceeds the rewrite ceiling** (x1.462 against x1.35) because its
   value IS a tier step and the steps grew. Raise the ceiling or re-specify the
   rewrite; `RiorsEdge.Progression.RuleBandImpact` fails until one happens.
5. **`EBreakerBuildCondition` is movement-only.** O30's taxonomy (ailment,
   crit, stacking) needs it widened before those axes can be authored honestly.
   Cheap to widen; it has blocked content twice.

---

## AS BUILT (2026-08-16)

- **Both OPENs above are CLOSED.** The "item level 101-120 has no source" OPEN
  (~:497-506) and the deliberately-red `PowerBand` OPEN (~:511-513) were
  resolved by O36 into pinned fixtures. There are no deliberate reds in the
  suite any more; any `Result={Fail}` is a regression, full stop.
- **The EnemyLevel re-clamp recorded above (~:485-495) is FIXED** — commit
  09aa66c, 2026-08-14. `ABreakerEnemy::ApplyChassis` now clamps to
  `[1, UBreakerAffixLibrary::MaxItemLevel]` (= 120) at
  `Combat/BreakerEnemy.cpp:179-181`, with the old defect recorded in the
  comment directly above the clamp. Drops above ilvl 50 DO ship; the 74x
  endgame gap is no longer blocked at the actor. (`Docs/HANDOFF.md` §6 D9
  still describes the pre-fix state — this section is the correction, and the
  discrepancy is reported rather than edited there.)
- The curve constants remain O2 PLACEHOLDER code defaults; nothing here has
  moved to a Data Asset.
- Related but separate: kill XP is paid from `EnemyLevel` (the drop item
  level), not `AreaLevel` — `Combat/BreakerEnemy.cpp:760-762`, contra the
  comment beside it. Recorded in `XP-And-Pacing.md`'s AS BUILT; above area
  ~50 the two ladders diverge.
