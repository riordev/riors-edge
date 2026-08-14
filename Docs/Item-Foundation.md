# Item Foundation

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

The first pass at the itemization prerequisites the master sheet calls out as
"cheap now, miserable to retrofit": item instances, item level, the roll
pipeline, and the equipment/stat-recalculation path. Source lives under
`Source/RiorsEdge/Items/`.

## Stat aggregation — the locked-in rule

Every affix declares a bucket:

- **Flat** values sum first (Health, Max Resource, Crit Chance/Damage points).
- **Increased %** values sum into a single additive bucket per stat and apply
  once (Movement Speed, Slide Speed, Air Control, Dash CDR, Drop Chance,
  Physical DR).
- **More %** multipliers multiply individually and are RESERVED for tree
  nodes and Anomalous rule rewrites. Affixes must not use them.

Crit remains the only damage multiplier of its kind, per the critical policy.
This is where the multiplicative-explosion risk is managed; do not add a new
bucket without a design pass.

**The rule is unchanged by the O27 breadth pass, and that is the point.** Two
things were added that look like new buckets and are not:

- **Conditional affixes** (Damage while Airborne, while Sliding, while Wall
  Riding, at Redline, after Dashing) are ordinary Increased percentages. While
  their condition holds they join the SAME single additive bucket as Weapon
  Damage; while it does not they contribute nothing at all. There is no
  conditional multiplier and no second bucket — the bucket's *contents* change
  with the movement state. `FBreakerBuildConditionState`
  (`Progression/BreakerBuildConditions.h`) is the one place the state is read;
  both the equipment and progression components consume it and neither edits
  Movement/, Classes/ or Abilities/ to do so.
- **Added Damage** is a Flat value on the `DamageMultiplier` attribute, whose
  base is 1.0. Flat sums first, so it is multiplied by the Increased bucket
  rather than added after it — the ordinary "added damage" shape, and the
  reason a flat line and an increased line are two different decisions rather
  than the same line twice.

**More is now expressible on nodes** (`EBreakerNodeStatBucket::MorePercent`)
and the O3 cap is enforced in code, not by convention:
`UBreakerProgressionComponent::AggregateStats` collects every owned More
source, sorts descending, keeps the strongest `MaxDamageMoreSources` (3) and
clamps each to `SingleMoreCeiling` (1.30x, Damage-Pipeline §4). A fourth
purchase is dead weight rather than a quiet nerf to the other three.

**RESOLVED — the cap is now GLOBAL as well as per-layer.** This section used
to carry a limit: "the clamp is per-LAYER … when Anomalous items author Mores,
the cap has to move to one shared clamp across the whole composition or a build
could hold three tree Mores plus an item's."
`FBreakerAttributeAggregator::Compose` now clamps the composed More product for
`DamageMultiplier` at `ComposedMoreCeiling()` — `SingleMoreCeiling^
MaxComposedMoreSources`, i.e. 1.30³ = **2.197**, which is Damage-Pipeline §4's
2.20 and `UBreakerCombatComponent::ComposedMoreCeiling` reached from the two
numbers that define it rather than restated as a third constant that can drift.
The per-layer selection stays where it is, because only that layer knows its
individual sources; what moved is the ceiling on what any combination of layers
can compose to. **A contributor cannot buy its way past O3 by arriving second.**
Damage only: no other aggregated attribute has an authored More budget, and
silently clamping (say) move speed would be a balance decision hiding in a
safety net. Pinned by `RiorsEdge.Items.Rules.NeverAuthorsAMore`, which also
asserts the ceiling does *not* touch a build already inside the budget.

Note that no rule rewrite actually authors a More — see the rarity section
below for why that was a design constraint rather than an oversight. The global
clamp is the guard rail for the next thing that tries.

## Item instances

`FBreakerItemInstance` (`BreakerItemTypes.h`) carries a GUID, a definition id
(FName, never a pointer), slot, rarity, **item level (1-120)**, rolled affixes,
and a save version. Definitions stay immutable content; instances are save
data.

### Item level — RATIFIED [O6 2026-08-12]

Item level is **hybrid**:

    ItemLevel = ZoneLevel + TierBonus + Variance

- **ZoneLevel** is authored on the rift/zone Data Asset — the zone, not the
  individual enemy, is the authority for how good its drops are.
- **TierBonus** is a content-difficulty step in the range 0..+5.
- **Variance** is ±1, so two drops from the same zone are not identical.

Zone-based sourcing is no longer an open question. The gym keeps the
**enemy-level fallback** (`ABreakerEnemy::EnemyLevel`) for when no zone Data
Asset is in play, so the zero-setup gym continues to work unchanged.

The clamp is now **1-120** (O29), not 1-50. That is the endgame: item level
runs past the character cap and past the area-level ceiling of 100, and it is
what makes "all endgame character power comes from gear" function rather than
merely be stated. `UBreakerAffixLibrary::MaxItemLevel` is the one number, and
`FBreakerWeaponMath::MaxSupportedItemLevel` must equal it -
`RiorsEdge.Items.TierLadder` pins that they do, because a weapon clamping lower
than the item system rolls would cap base damage while the affixes on the same
item kept climbing.

OPEN [O29]: **item level 101-120 has no source yet.** Drop item level derives
from area level and area level ceilings at 100, so the top twenty item levels
are reachable only through the TierBonus above (0..+5 does not cover it) or the
Forge. That headroom is worth 1.09^20 = **5.6x base damage** over on-level
content, so it is not a rounding error - it is either the reward at the end of
the chase or it is 20 levels of dead ladder, and which one is an owner ruling.

GAP [O6]: the ZoneLevel-per-zone table and the mapping from content difficulty
to a specific TierBonus are not authored here. Value authoring is frozen under O2 — owner to
backfill after the wave-mode instrumentation reports.

## Tier scale - WIDENED AND BACK-LOADED [O29 2026-08-14]

**Fourteen tiers, T12 worst to T-1 best**, stored as printed (12..1, 0, -1).
It was ten (T8..T-1) against an item level ceiling of 50; O29 rules that the
endgame power source is gear depth, so the ladder widened with the item level
range rather than a new multiplier lane being invented. Affix magnitudes still
live strictly inside tier ranges, which is what keeps O3 intact.

The curve is **no longer linear**. It is geometric between the two authored
anchors, bent once more by an exponent, so the step grows toward the top:

    p(T)     = (12 - T) / 11
    Value(T) = ValueAtT12 * (ValueAtT1 / ValueAtT12) ^ (p ^ 1.25)

On the pool's flagship line, Core.Health (25 -> 400):

| Tier | Value | Step | Tier | Value | Step |
|---|---|---|---|---|---|
| T12 | 25.00 | - | T6 | 91.70 | x1.303 |
| T11 | 28.71 | x1.148 | T5 | 120.87 | x1.318 |
| T10 | 34.75 | x1.210 | T4 | 160.93 | x1.331 |
| T9 | 43.18 | x1.243 | T3 | 216.23 | x1.344 |
| T8 | 54.70 | x1.267 | T2 | 292.97 | x1.355 |
| T7 | 70.36 | x1.286 | T1 | 400.00 | x1.365 |

- T1 is **16.0x** T12 (a property of the authored anchors, not of the curve).
- The bottom step is **+14.8%** / +3.71 absolute; the top step is **+36.5%** /
  +107.03 absolute. The top step is **2.46x** the bottom in relative terms and
  **28.8x** in absolute terms. That is what "a materially bigger jump between
  the high tiers" has to mean on a ladder whose values are themselves growing.
- Two failure modes are avoided at once. A LINEAR ladder over 11 steps makes a
  top-tier roll arithmetically just one more step, which O29 forbids. A pure
  `p^k` lerp back-loads so hard over 11 steps that the bottom four tiers become
  indistinguishable - a cliff with a flat approach.
- Tier for tier against the old ladder: T8 2.19x, T7 1.49x, T6 1.32x, T5 1.32x,
  T4 1.42x, T3 1.59x, T2 1.86x, T1 2.22x. Every tier number is materially up
  and the top is up most.
- **T12 is deliberately the old T8 value, unchanged.** An item level 1 drop is
  bit-identical to what it was before O29 - the same anchoring
  `FBreakerWeaponMath::ItemLevelDamageScalar` uses at ilvl 1, so the curve is
  opt-in by content rather than a silent retune of the only content anybody has
  played. The ceiling anchors went up ~2.2x pool-wide and uniformly, so no line
  became stronger relative to another and the per-slot identity table, the
  archetype leans and the offence/defence balance are all preserved exactly.

**T0 and T-1 are RE-SITED, not carried over: 2.2x and 3.6x T1**, up from 1.4x
and 1.8x. Against a linear ladder whose top step was +14%, 1.4x was about 2.9
ordinary steps and read as a spike. Against a +36.5% top step it would be barely
one step - the spike would have been demoted into "one more step", which is
precisely what O29 rules out. At 2.2x/3.6x they sit ~2.5 and ~4.1 top-steps
above T1, preserving what the spike MEANT rather than what it measured.

`UBreakerAffixLibrary`:

- `ValueForTier` - the curve above.
- `BestTierForItemLevel` - **1-120 onto T12..T1 on TWO SLOPES, not one.** Owner
  ruling after playtesting O29 ("the item level tier capping at 8 might make for
  awkward feeling progression, let's bring that to 6"): a single slope of one
  tier per 10 levels put the character cap at T8, so a player who finished the
  levelling game had crossed only a third of the ladder and every tier they had
  met was in its shallow lower half. The levelling band is therefore steeper
  than the endgame band:

  | Band | Item levels | Tiers | Rate |
  |---|---|---|---|
  | The campaign | 1 -> 50 | T12 -> T6 | ~8.2 levels per tier |
  | The chase | 50 -> 120 | T6 -> T1 | ~14 levels per tier |

  Level 1 rolls only T12; **the character cap of 50 reaches T6** — half the
  ladder, standing on the shoulder of the curve where the steps start to be
  worth something; the area-level ceiling of 100 reaches T3; **T1 opens at
  ilvl 120 exactly**. The endgame is longer per tier BECAUSE each of those tiers
  is worth more, not as a tax. Both the anchor (T6 at the cap) and the two
  slopes are `O2 PLACEHOLDER`. T0/T-1 still never come from item level -
  crafting or a rule rewrite only.
- `TierCapForRarity` - **RE-DERIVED against 11 steps rather than 7.** Standard
  caps at **T4** (verified in code) (it used to be denied the top 2 of 7 steps, 29% of the ladder;
  29% of 11 steps is 3.1), Uncommon at **T2**, the rest reach T-1. The
  Standard/Uncommon gap stays exactly two tiers. Uncommon moves off T1
  deliberately: on the old ladder T1 was only +14% over T2 and cost little to
  hand out, but on a back-loaded ladder T1 is +36.5% over T2 and O29 wants it to
  be an event - a rarity that drops from every third kill cannot be the thing
  that hands it out.

**T0 and T-1 are now reachable.** They were not, by any means, until the Forge
landed: `BestTierForItemLevel` stops at T1 at level 50, the comment there says
they are "crafting/boss territory", there was no crafting and there is no boss.
Two tenths of the tier scale — the two the whole curve *spikes* toward — existed
only as a comment. There are now exactly two routes, and both are gated:
**tempering at the Forge** (Sigil-only, see the Forge section) and the
**PROLIFIC** Anomalous rewrite (see rarity, below).

## The affix pool — WIDENED [O27 2026-08-13]

`UBreakerAffixLibrary::GetSliceAffixPool()` is the C++ fallback pool (same
zero-setup convention as weapons). It was twelve lines of which exactly ONE
was offensive, and that one rolled on four of eight slots — so helmet, body
armour, boots and waist were structurally incapable of raising damage. That is
the concrete reason "full level 50 gear" did not feel like anything, and O27
rules it out.

**TWENTY-FOUR lines, eleven of them offensive** (counted from
`BuildSliceAffixPool` on 2026-08-14; the pool grew twice after the first
breadth pass — four non-damage lines, then Resource Efficiency with the Mana
inversion):

| Line | Bucket | Condition | Slots |
|---|---|---|---|
| Health, Resource Regen, Max Resource, Move Speed, Drop Chance, Physical DR | as before | — | all 8 |
| **Resource Efficiency** | Increased | — | all 8 |
| Slide Speed / Air Control / Dash CDR | Increased | — | boots+waist+**both weapons** / boots+neck / boots+gloves+**both weapons** |
| Critical Chance | Flat | — | helmet, gloves, neck, weapons |
| Critical Damage | Flat | — | helmet, gloves, neck, weapons |
| **Weapon Damage** | Increased | — | **all 8** |
| **Added Damage** | Flat | — | helmet, gloves, waist, neck, weapons |
| **Damage while Airborne** | Increased | Airborne | boots, helmet, neck, primary |
| **Damage while Sliding** | Increased | Sliding | boots, waist, body, secondary |
| **Damage while Wall Riding** | Increased | Wall riding | boots, waist, gloves, body |
| **Damage at Redline** | Increased | Redline momentum | neck, body, helmet, primary |
| **Damage after Dashing** | Increased | Recently dashed | gloves, waist, neck, secondary |
| Fire Rate | Increased | — | primary, secondary |
| **Armour** | Flat | — | helmet, body, gloves, boots, waist |
| **Health on Kill** | Flat | — | body, gloves, waist, neck, both weapons |
| **Resource on Kill** | Flat | — | helmet, gloves, neck, waist, both weapons |
| **Damage over Time** | Increased | — | helmet, body, gloves, neck, primary |

How the pool got here, because the line count in this document has been wrong
twice: **12** lines (one offensive) -> **18** in the first O27 breadth pass ->
**19** when Fire Rate arrived with the weapon-drop pass -> **23** in the
2026-08-14 non-damage breadth pass (Armour, Health on Kill, Resource on Kill,
Damage over Time — see that section for why each has a live consumer) ->
**24** with Resource Efficiency.

**Resource Efficiency** arrived with the owner's Mana inversion ruling: once a
class resource is spent DOWN rather than banked UP, efficiency and regeneration
are the two stats that decide how often a caster gets to act, and only
regeneration existed — which left Maximum Resource holding up a whole class's
gearing on its own. It is authored as an Increased percentage OF THE REDUCTION
(12.0 == casts cost 12% less), and it is deliberately a peer of Resource
Regeneration rather than a better version: efficiency pays a build casting
expensive spells rarely, regeneration one casting cheap spells constantly.
O2 PLACEHOLDER 2% (T12) -> 26% (T1).

Elemental DR is still excluded until a resistance model exists.

**Per-slot identity is the design, not the slot list.** Every slot can raise
damage (pinned by `RiorsEdge.Items.Affixes.Breadth`), but no two slots offer
the same set: boots are the airborne/slide/wall piece, the waist is
slide/dash/wall, the necklace is Redline and dash, body armour is the grounded
traversal piece, and helmet and weapons are where precision lives. Two players
hunting damage on boots and on a necklace are hunting different lines.

Conditional lines roll roughly twice what the unconditional line does, because
they are off whenever you are standing still. That trade is what makes
"build around a movement state" a decision rather than a strictly better
version of the same line.

## THE DROP PIPELINE (2026-08-14)

Owner playtest, verbatim: *"not every single enemy needs to drop an item — we
need to make the calculations for drop rates and how they should work. I was
getting way way too many Aberrants at this item level when it shouldn't even be
fundamentally possible, and every single enemy dropped an item."*

### What was actually wrong, confirmed in code before anything was written

Both halves of that report were **one structural gap**.
`ABreakerEnemy::GrantLoot` called `UBreakerLootLibrary::RollRarity`
unconditionally on every death and spawned whatever came back. So:

1. **There was no "does this enemy drop at all" step.** Kill count *was* item
   count. At the documented kill rate below that is ~690 items an hour.
2. **The rarity table was doing the entire job on its own, and it is flat.** A
   2.5% Aberrant weight applied at area level 1 to a trash mob exactly as it did
   to a boss at area level 50 — roughly **17 Aberrants an hour, at any level**,
   against O11's cap of *three equipped*. The owner's "it shouldn't even be
   fundamentally possible" was literally true: nothing made it impossible,
   because nothing in the loot path read the level or the rank at all.

### The pipeline is now three steps where it was one

`Items/BreakerDropTable.{h,cpp}` — world-free pure functions in the precedent of
`Combat/BreakerMonsterChassis.h` and `Weapons/BreakerWeaponMath.h`, so it is
unit-testable and structurally incapable of reading a player.

1. **DROP CHANCE**, per monster rank. Most trash kills drop nothing.
2. **RARITY GATE**. A rarity whose gate is unmet has its weight zeroed *before*
   the roll — not rerolled after.
3. **RARITY ROLL**, the weighted table over whatever survived step 2.

`UBreakerDropTableLibrary::RollDrop` is the one entry point content calls. It
derives two independent sub-seeds from the kill seed so the drop decision and
the rarity do not correlate; sharing a stream would make "dropped at all" and
"dropped well" the same coin, which shows up as the rare tiers clustering in the
same seeds. Determinism is preserved end to end and pinned.

`RollRarity` keeps its signature and delegates with every gate open, so there is
**one** weight table in the project rather than two that drift. Content must not
call it — it is for dev grants, fixtures and crafting previews, which have no
rank and no item level to supply.

### THE RARITY GATE RULE, in one sentence

> **A rarity can only roll when BOTH the drop's item level is at or above that
> rarity's unlock level AND the killed monster's rank is at or above that
> rarity's minimum rank; a gated-out rarity's weight is zero and its share
> redistributes across the rarities that are open.**

Two levers because the complaint had two halves. Item level answers "at this
item level". Rank answers "every single enemy dropped an item" — the top tiers
now come from the encounter content O27 says the difficulty lives in.

Redistribution rather than a reroll is deliberate: a reroll preserves the SHAPE
of the table and just relabels illegal results, whereas zeroing the weight means
a low-level trash kill genuinely has a *different* rarity distribution.

### The authored block — `FBreakerDropTableParams`

One struct, `EditAnywhere`, every value `O2 PLACEHOLDER`, sitting on
`ABreakerEnemy` beside the existing `Chassis` params so drop rates retune from
the details panel with no recompile.

| Rank | Drop chance | Why |
|---|---|---|
| Trash | **0.10** | O27: trash exists to be trivialized. Occasional, not routine. |
| Elite | **0.75** | Reliable. |
| ModifierBearing | **0.90** | O27 puts the difficulty here, so the loot follows. |
| Boss | **1.00** | Guaranteed. |

| Rarity | Weight | Min item level | Min rank |
|---|---|---|---|
| Standard | 62 | — | — |
| Uncommon | 25 | — | — |
| Exceptional | 10 | 8 | Trash |
| **Aberrant** | **1.2** (was 2.5) | **25** | **Elite** |
| **Anomalous** | **0.25** (was 0.5) | **40** | **Elite** |

Exceptional is deliberately reachable from trash: something has to be worth
picking up off an ordinary kill, or a 10% drop rate is 10% of nothing and the
player stops looking at the floor.

**The Aberrant weight had to fall as well as be gated.** O11 caps Aberrant at
three equipped, *globally*. A rarity capped at 3 must be rare enough that 3 is a
goal; at the old flat rate the cap filled in the first twenty minutes and was
thereafter only ever displaced. The gates do most of the work, but elites alone
would have refilled the cap hourly at 2.5.

**Drop Chance now bids on quantity as well as quality.** It multiplies the
per-rank chance (`DropChanceQuantityScale`, 1.0 — so +100% Drop Chance doubles
it, clamped so a guaranteed drop stays one drop) *and* keeps its existing
drain-weight-out-of-Standard effect on the rarity table. A stat printed as "Drop
Chance" that only changed which rarity came out of a drop you were getting
anyway is the same quiet lie as the `DamageMultiplier` nothing wrote. Set the
scale to 0 to make it a pure quality stat again.

**Off switches, both one field:** set every rank chance to 1.0 to recover the old
"every enemy drops" behaviour exactly; set every gate to item level 1 / rank
Trash to recover the old flat table.

### THE LOOT-PER-HOUR ARITHMETIC

This is the number the owner actually wants to tune against, and nothing in the
project could answer it before. `UBreakerDropTableLibrary::ProjectLootRate`
computes it analytically; `RiorsEdge.Items.Drops.LootPerHour` simulates 200
hours through the real `RollDrop` and asserts the two agree, so this table
cannot drift away from what the pipeline actually does.

**The reference hour** (`FBreakerKillRateSample`, all `O2 PLACEHOLDER` — an
*input*, not a measurement; nothing in the codebase measures kills per hour yet
and the wave-mode report is the instrument that eventually should):
600 trash + 60 elite + 30 modifier-bearing + 2 boss = **692 kills**.

At no Drop Chance, by the area level of the content:

| Area / item level | Items/h | Exceptional+/h | Aberrant/h | Hours per Aberrant | Anomalous/h | Hours per Anomalous |
|---|---|---|---|---|---|---|
| 5 | 134 | **0** | 0 | never | 0 | never |
| 10 | 134 | 13.8 | 0 | never | 0 | never |
| 25 | 134 | 14.6 | 0.90 | 1.1 | 0 | never |
| 50 | 134 | 14.8 | 0.90 | **1.1** | 0.19 | 5.3 |

Simulated figures at item level 50, from the test log: 133.9 items/h, 14.4
Exceptional+, 0.87 Aberrant, 0.17 Anomalous.

**Read against what it replaced:** 692 items/h became **134** (a 5.2x cut), and
~17 Aberrants/h at *any* level became **zero below area level 25** and 0.90/h
above it. Three Aberrants — O11's whole allowance — is about **3.3 hours** of
on-level elite killing, and because a drop lands in one of eight random slots,
three in *useful* slots is materially longer than that. That is the shape O11's
cap wants.

**The legendary cadence — RULED [O32 2026-08-14].** A legendary is 25% of an
Anomalous drop in one of the three slots that has one, so **~57 hours per
legendary** at area level 50. This document previously proposed raising
`LegendaryChanceWithinAnomalous` (0.25). **O32 rules that it is NOT raised.**
The 57 hours is mostly an artefact of there being three legendaries covering
three of eight slots; the same arithmetic against a full set of eight gives
**~21 hours**, which is a reasonable cadence for a named item against O4's
300-400 hour horizon. **Authoring more legendaries is the fix**, and the
effective wait falls on its own as the pool fills. To playtest a legendary
before the pool is full, use `DevGrantLegendaries` rather than bending the drop
rate.

### Coverage

`RiorsEdge.Items.Drops.ChanceByRank` (the ordering is the design, and the roll
measurably honours it), `.TrashCannotRollAberrant` (the gate checked
**exhaustively** over the whole item level range plus 40k real rolls at ilvl 50
with max Drop Chance — "fundamentally impossible" is the bar the owner set, and
a gate that leaks one in ten thousand is not a gate), `.Determinism` (a seed
reproduces the drop decision, the rarity and the item; and the chance step does
not bias the rarity step), `.LootPerHour` (the sweep above).

### Reconciled with O29: the gates did NOT scale, deliberately

This section used to say the drop pipeline was built against `MaxItemLevel` 50
and that the rarity gates should scale upward with the new range. **O29 has
landed and the gates did not move, on purpose.** The reasoning is at
`FBreakerDropTableParams`:

Scaling 8 / 25 / 40 proportionally onto a 120-level range gives roughly
19 / 60 / 96, which would put Aberrant past the end of the campaign and
Anomalous at the area-level ceiling — a player would finish the entire levelling
game having never seen either rarity, so the ladder would be introduced only
after the content that teaches it. **These gates pace the player's INTRODUCTION
to rarity, and that introduction still happens across item levels 1-50
regardless of how far the tier ladder now runs.**

What O29 does argue for is the top two WEIGHTS falling again, because a longer
tier ladder makes each rarity step worth more than it was. That is a tuning
question with a measurable answer (the hours-per-Aberrant figure in the
projection) and it wants a playtest, not a guess. Recorded, not acted on.

## Roll pipeline

`UBreakerLootLibrary` implements steps 1-5 of the master-sheet pipeline,
fully deterministic from a seed. **Step 0 is now the drop pipeline above** —
these five steps only run on a kill that has already passed the chance step and
been given a gated rarity:

1. `RollRarity` — weighted table; Drop Chance % drains weight out of Standard.
   Superseded as the *content* entry point by
   `UBreakerDropTableLibrary::RollDrop`; see the drop pipeline section.
2. Affix count from the rarity range.
3. Affix selection — slot-legal, weighted, no duplicates, max 4 prefixes and
   4 suffixes.
4. Tier per affix - item level gated, rarity capped, **0.64 odds per step up**
   so the top tiers feel earned. RE-DERIVED for 11 steps [O29]: the walk used
   to continue at one half over 7 steps, so a full climb was 0.5^7 = 1/128. At
   one half over eleven steps it would be 1/2048, which is not rarity, it is a
   tier nobody sees. Solving q^11 = 1/128 gives 0.64, and 0.64^11 = 1/136 -
   the rarity of a top roll survives the ladder tripling in length, while each
   individual step gets likelier, which is correct now that the low tiers are
   worth proportionally less.
5. Value within the tier band.

Step 6 (fixed signatures) is **partly built**: a LEGENDARY carries a guaranteed
signature (`FBreakerLegendaryDefinition::GuaranteedAffixIds`, filled in by
`RollLegendary`). What is still unbuilt is a signature on an ordinary
**Aberrant or Anomalous** drop — O11 reserves Aberrant's "1-2 unique modifier
affixes" for the owner to name and design, and the Focused slot is the seat
those will occupy. That is a design pass, not a code gap.

Step 7, ANOMALOUS ONLY: the rule rewrite is drawn last, after every affix, so
that every rarity below Anomalous consumes exactly the draws it always did and
no previously-recorded roll moves. A legendary is picked here too, after slot
and item level are resolved.

## Equipment

`UBreakerEquipmentComponent` (on `ABreakerCharacter`) owns the eight slots
plus a backpack, replicates both, and folds equipped affixes into
`UBreakerAttributeSet`: MaxHealth (preserving the health fraction), Max
Class Resource, Crit Chance/Multiplier, and MoveSpeed. Gear resource regen
ticks on the server. Movement multipliers (slide / air control / dash cooldown)
**are consumed**: each has its own aggregated attribute and
`UBreakerCharacterMovementComponent` reads the composed value — see the
movement conformance note in the consumption audit below; the "not connected
yet" this section used to carry is resolved. Physical DR from gear folds into
the incoming damage multiplier inside `UBreakerCombatComponent::ReceiveDamage`.

## Unified attribute application

Gear affixes and skill nodes both move the same attributes, so they share one
application path. It lives in `Source/RiorsEdge/Attributes/` —
`BreakerAttributeAggregation.h` for the types, `UBreakerAttributeSet` for the
state and the writes.

The bug it replaced: `UBreakerEquipmentComponent` and
`UBreakerProgressionComponent` each cached its own "base" value at BeginPlay
and then wrote an absolute result (`Base + bonus`, `Base * multiplier`).
Whichever recalculated last erased the other, so gear and nodes never stacked
— a real, player-visible correctness bug.

The rules the new path enforces:

- **One owner of the base.** The attribute set captures the authored values
  once, via `CaptureAttributeBases()`, which is idempotent — the first caller
  wins. No contributor can snapshot a base that already contains another
  contributor's work.
- **Contributions, never writes.** Each layer rebuilds a complete
  `FBreakerAttributeContribution` (flat / increased-percent / More, per
  attribute) from scratch whenever anything it owns changes, and submits it
  with `ApplyAttributeContribution`. Contributors are a closed enum
  (`EBreakerAttributeContributor`), so the fold runs in a fixed order and the
  result never depends on who recalculated last.
- **The locked rule, once.** Every submission re-derives every shared
  attribute as
  `(Base + sum(Flat)) * (1 + sum(IncreasedPercent) / 100) * prod(More)`.
  All Increased percentages from every layer share ONE additive bucket per
  stat. Nodes cannot author More multipliers yet; when keystones gain them
  under O3 they compose through `ComposeMore` with no other change.
- **Exact removal.** Unequipping or respeccing submits a smaller (or empty)
  contribution and everything is recomputed from the bases, so there is no
  incremental subtraction to drift. Health and class resource ride their
  maximum by fraction/clamp.
- **Convergence.** Equipping, buying a node, respeccing and loading a save all
  end in the same numbers regardless of sequence.

The attributes on this path are MaxHealth, MaxClassResource, CriticalChance,
CriticalMultiplier, MoveSpeed, DamageOverTimeMultiplier, **DamageMultiplier**
and — since the movement conformance pass — **SlideSpeedMultiplier**,
**AirControlMultiplier** and **DashCooldownReduction**.
Attributes a single system owns outright (Shield, Armor) stay off it.
`FBreakerEquipmentStats` and `FBreakerNodeStats` are otherwise unchanged: the
movement, combat and loot consumers still read the composed multipliers from
`GetStats()` / `GetNodeStats()`.

## Damage scaling lands in exactly one place

`DamageMultiplier` is THE composed outgoing-damage number. Every damage path
already read it (`UBreakerWeaponComponent` hitscan and projectile,
`UBreakerAbility_Cleave`, and the Bleed snapshot's `SourcePower`); until this
pass nothing wrote it, so it was permanently 1.0 — the literal cause of the
owner's report that damage never changes no matter what is equipped or spent.

Four sources bid into its single additive Increased bucket, and one composes
into its More product:

- **Gear.** The `WeaponDamage` affix target. Its raw percentage is submitted by
  `UBreakerEquipmentComponent::AggregateStats`. It used to reach the weapon on a
  private path (`GearWeaponDamageMultiplier`, MULTIPLIED against the attribute),
  which would have made gear and tree damage compose multiplicatively; that
  helper is deleted. `FBreakerEquipmentStats::WeaponDamageMultiplier` survives
  as the gear-only figure the inventory totals panel prints — reading it at a
  damage site would double-count gear.
- **Skill nodes.** The new `EBreakerNodeStatTarget::Damage`. Before it existed a
  node was structurally incapable of raising weapon damage.
- **Conditional lines, gear and node alike.** Damage while Airborne / Sliding /
  Wall Riding / at Redline / after Dashing. Same bucket, present only while the
  state holds.
- **The point-spend baseline.** `UBreakerProgressionComponent::
  IncreasedDamagePerSpentPoint` (EditAnywhere, O2 PLACEHOLDER **0.25%** per
  point, cut from 1.0% under O27) pays a small Increased Damage per point
  COMMITTED to nodes, counted by cost so a 3-point Convergence is worth three
  times a 1-point minor. At 1.0% it contributed roughly +69% at a full budget
  against roughly +19% from every damage node combined, so how MANY points had
  been spent mattered ~3.5x more than where — backwards for a build game. At
  0.25% it is a floor: it stops a purely defensive purchase being literally zero
  offence, and because every build spends its whole budget it cannot
  differentiate two builds. It is a property of spending, not of level, so it
  does not touch the cap-50 / no-post-cap-power ruling. Set it to 0 to leave
  only node content.
- **More multipliers (a different bucket).** Six Convergence/keystone nodes
  author one each; at most three count, each capped at 1.30x. See the locked
  rule above.

Slice fallback content authoring damage (all O2 PLACEHOLDER). Unconditional:
Core Precision Sightline +4%, Called Shot +3%/rank x2, Core Volley Cyclic
+3%/rank x3, Salvo +6%/rank x3, Swift Marksman Long Lens and Pierce Discipline
+3%/rank x2 each. Conditional: Core Velocity Freefall +9%/rank x3 (airborne),
Slipstream +9%/rank x3 (sliding), Traction +14%/rank x2 (wall riding),
Afterburn +8%/rank x3 (recently dashed), Swift Kinetic Downforce +11%/rank x2
(airborne) and Grind +13%/rank x2 (wall riding). More: Fixate x1.22, Barrage
x1.22, Culling x1.18 (all unconditional), Terminal Velocity x1.30 (airborne),
Overpressure x1.20 (sliding), Redline Doctrine x1.20 (at Redline).

**The band test is the guard rail, and it is RED [O29].**
`RiorsEdge.Progression.PowerBand` (`Tests/BreakerPowerBandTests.cpp`) composes a
baseline and an optimized character out of the real affix pool and the real
trees, through the real `FBreakerAttributeAggregator`, and asserts the composed
ratio lands in Power-Curve §4's 8-10x band. It logs the ratio layer by layer, so
a tuning pass can see WHICH layer moved rather than only that the band broke.

O29 moved where the band lives, and the fixture moved with it: both characters
are now built at **item level 120**, baseline **T3** against optimized **T1**
(constants `PowerBandItemLevel`, `PowerBandBaselineTier`,
`PowerBandOptimizedTier`). At the old ilvl 50 / T5 / T1 fixture the pairing was
one the loot pipeline can no longer produce — `BestTierForItemLevel(50)` is T6,
so neither T5 nor T1 is rollable there at any rarity.

**The band is left FAILING rather than retuned**, on the same precedent as the
old `PowerCurve.EndgameClamp`: a red test that states an open decision is more
honest than a green one that hides it by moving the goalposts. The test's own
note records the post-O29 figure at around **15x** against the authored 8-10x.
The extra did not come from the gear spread (T3-vs-T1 at ilvl 120 is ~1.85x,
close to the old fixture's 1.97x) — it came from absolute affix values roughly
doubling, which moves flat crit chance and the additive bucket into a different
part of their own curves for the optimized build specifically.

**Do not "fix" this by widening the asserted range.** Two ways out, both owner
rulings: the band target moves (8-10x was authored before the endgame existed,
and a longer ladder arguably should separate builds further), or the content
retunes so crit and the additive bucket land the composed band back at 8-10x.

The pre-O29 measurement this document used to quote — flat 1.16x, increased
2.35x, more 1.93x, crit 1.66x, composed 8.74x — was taken against the old
ladder and the old fixture and is kept here only as the historical anchor. Every
number in this section wants a suite run to restate; none has been re-measured
since the fixture moved.

Coverage: `Source/RiorsEdge/Tests/BreakerAttributeAggregationTests.cpp` —
including `RiorsEdge.Attributes.Damage.NodePurchaseRaisesWeaponDamage`, which
buys a node and asserts the damage a weapon would deal actually rises — plus
`RiorsEdge.Items.Equipment.AttributeContribution` and
`RiorsEdge.Progression.RespecRestoresAttributes`.

## Affix and node consumption audit (2026-08-13)

Every stat target, end to end. "Live" means a purchased/equipped line changes
something a player can observe.

| Layer | Target | Consumer | Live |
|---|---|---|---|
| Affix | Health | MaxHealth attribute | yes |
| Affix | ResourceRegen | `UBreakerEquipmentComponent::TickComponent` writes ClassResource (server) | yes |
| Affix | MaxResource | MaxClassResource attribute; Momentum/Mana clamp and the HUD bar read it | yes |
| Affix | MoveSpeed | MoveSpeed attribute → `GetComposedMoveSpeedMultiplier()` | yes |
| Affix | DropChance | `ABreakerEnemy` loot roll (`RollRarity`) | yes |
| Affix | PhysicalDamageReduction | `UBreakerCombatComponent::ReceiveDamage` | yes |
| Affix | ElementalDamageReduction | none — no stats field, no consumer, deliberately absent from the pool | **no, reserved for O5** |
| Affix | CriticalChance | CriticalChance attribute → weapon/Cleave | yes |
| Affix | CriticalDamage | CriticalMultiplier attribute | yes |
| Affix | SlideSpeed | SlideSpeedMultiplier attribute → movement | yes |
| Affix | AirControl | AirControlMultiplier attribute → steer rate | yes |
| Affix | DashCooldownReduction | DashCooldownReduction attribute → dash | yes |
| Affix | WeaponDamage | DamageMultiplier attribute | yes |
| Affix | Armour | Armor attribute → `GetEffectiveArmor()` → mitigation | yes (2026-08-14) |
| Affix | LifeOnKill | `OnKillDealt` → `UBreakerCombatComponent::ApplyHealing` | yes (2026-08-14) |
| Affix | ResourceOnKill | `OnKillDealt` → ClassResource | yes (2026-08-14) |
| Affix | DamageOverTime | DamageOverTimeMultiplier attribute → DoT snapshots | yes (2026-08-14) |
| Affix | FireRate | FireRateMultiplier attribute → `GetEffectiveRoundsPerMinute()`, through which every fire-timing site runs | yes |
| Affix | ResourceEfficiency | Bid as a NEGATIVE Increased into `ResourceCostMultiplier` (base 1.0, clamped 0.25-2.0) → `UBreakerCasterAbility::GetResourceCostMultiplier`, floored at 0.10 so no stack makes a cast free | yes (2026-08-14), **Caster only** |
| Node | CriticalChance | CriticalChance attribute | yes |
| Node | CriticalDamage | CriticalMultiplier attribute | yes |
| Node | MoveSpeed | MoveSpeed attribute → `GetComposedMoveSpeedMultiplier()` | yes |
| Node | SlideSpeed | SlideSpeedMultiplier attribute → movement | yes |
| Node | AirControl | AirControlMultiplier attribute → steer rate | yes |
| Node | DodgeChance | `UBreakerCombatComponent` defense state | yes |
| Node | BlockChance | `UBreakerCombatComponent` defense state | yes |
| Node | Health | MaxHealth attribute | yes |
| Node | DamageOverTime | DamageOverTimeMultiplier attribute → DoT snapshots | yes |
| Node | Damage | DamageMultiplier attribute (this pass) | yes |

`BonusMaxResource` and `ResourceRegenPerSecond` were suspected dead and are
not; `RiorsEdge.Attributes.Affixes.ResourceAffixesAreLive` now pins both so a
refactor that drops either fails loudly. They stay in the pool.
`ElementalDamageReduction` is the only genuinely inert target, and because no
affix in the pool rolls it, it lies to nobody — it is left in place, commented,
for the O5 resistance model.

RESOLVED (movement conformance pass): `UBreakerCharacterMovementComponent` no
longer multiplies the gear and tree movement multipliers together. Slide speed,
air control and dash cooldown reduction gained aggregated attributes of their
own, both layers bid raw percentages into them, and the movement component reads
the composed attributes (`GetComposedMoveSpeedMultiplier()` and friends); the
private `GearMoveSpeedMultiplier()` helpers are deleted, exactly as
`GearWeaponDamageMultiplier` was. **This is a felt change** — +20% gear with
+20% tree now reads x1.40 rather than x1.44 — and the before/after table at
representative investment levels is in `Docs/Movement-Design.md`.

The composed `MoveSpeed` attribute also gained its first gameplay consumer. It
holds a SPEED, not a multiplier, and `WalkSpeed` is authored `EditAnywhere` on
the movement component, so the movement component publishes its `WalkSpeed` as
the attribute's base through
`UBreakerAttributeSet::SetAggregatedAttributeBase` and reads back
`composed / base`. An attribute-set constant would have gone stale the moment
the owner retuned `WalkSpeed`, and a composed speed that disagrees with the
speed the character actually walks at is the same class of lie the unaggregated
`DamageMultiplier` was.

`DashCooldownReduction` is stored as a DIVISOR (x1.20 == a 20% shorter
cooldown), because that is the only shape two layers can share additively. No
node stat target authors dash cooldown yet, so gear is currently its only
bidder — the attribute exists so that the first node to add one is additive from
the day it lands rather than repeating this bug.

## Gym drops

`ABreakerEnemy::GrantLoot` runs the drop pipeline on death (`bDropsLoot`),
seeded by spawn location and kill count: `RollDrop` first (most trash kills now
return early and drop nothing), then the ordinary `RollItem`, then a ground
pickup. The elite Exceptional floor survives the rewrite but is now *composed*
with the gates rather than competing with them — it can only lift a drop to a
rarity that drop's item level actually unlocks, so an elite in a level-3 area no
longer produces an Exceptional out of nowhere.

## Also in this pass

- **Weapon swap tempo layer** — `SwapInDuration` on the weapon definition,
  `bSwapping` state blocking fire/reload, `OnSwapChanged`, and
  `GetSecondsSinceSwapIn()`. This unblocks the Secondary exclusive affixes.
- **Block/dodge as passive chance layers** [RULED O1 2026-08-12] — block is
  a passive chance to *reduce* an incoming hit; dodge is a passive chance to
  *fully evade* one. No input, no stance, no shield requirement, no facing
  requirement, and no cost — the shared stamina pool is deleted, along with
  `Stamina`/`MaxStamina`. Both live in `UBreakerCombatComponent`; the
  resolution order is in the damage library and covered by tests. Neither
  applies to DoTs. Nothing to bind: the previous `SetBlocking`/`TryDodge`
  input-facing entry points are superseded by the passive roll. Parry, when
  built, is the only defensive input and uses its own short cooldown.
  GAP [O1]: the old dodge refunded a little class resource on a dodged hit.
  Whether that refund survives as a passive-proc effect — and if so whether
  it is base kit or a tree rule rewrite — is unresolved. Owner to decide.
- **Status runtime** — `UBreakerStatusComponent` runs snapshot DoTs (Bleed,
  Poison): stack-capped reapplication keeps the original snapshot, physical
  DoTs bypass shields and take half armour via the existing global rule.

## Not built / open

- ~~Crafting~~ and ~~signatures~~ are BUILT (2026-08-14) — see the Forge and
  legendary sections. What is still missing from crafting is the "add an affix"
  verb (deliberately: an item's affix COUNT is rarity's job and adding one would
  make rarity craftable) and any UI at all. Pickup actors and loot UI exist.
- ~~Movement affix multipliers are computed but not yet consumed~~ **RESOLVED**
  — the movement component reads the composed attributes.
- ~~Save persistence for items~~ **BUILT** — `UBreakerSaveGame` stores
  `EquippedItems` and `BackpackItems` (slot `BreakerSave0`, autoloaded in the
  character's BeginPlay, autosaved in EndPlay and on class lock), and
  `MigrateToCurrent` gives `SaveVersion` a meaning. **The Forge wallet is NOT
  in the save**, so currency does not survive a session — that is the one item
  gap left in persistence.
- All values are placeholder until the gym feedback pass re-anchors them.
- **Added Damage prints without a unit.** `SBreakerMenu::DescribeAffix` decides
  the "%" suffix from the bucket, with a hard-coded exception for the two crit
  targets. Added Damage is a Flat line authored in percentage points of base
  weapon damage, so it renders as "Added Damage  +5.0  T1". Fixing it properly
  means a display-format flag on `FBreakerAffixDefinition` that the UI reads,
  which is a UI-layer change; reported rather than hacked around.
- ~~The O3 More cap is enforced per LAYER, not globally.~~ **RESOLVED
  2026-08-14**: `FBreakerAttributeAggregator::Compose` now clamps the composed
  More product for `DamageMultiplier` at 1.30³ across every contributor, so a
  layer arriving second cannot buy its way past O3. See the aggregation section
  at the top of this document.
- **Conditional lines are evaluated on a component tick.** The equipment and
  progression components each re-read the movement state each frame and rebuild
  their contribution only on a transition. That is correct and cheap, but it
  means a conditional bonus lands on the frame AFTER the state changes. If a
  one-frame lag on entering a slide ever reads as mushy, the fix is an event
  from the movement component rather than a faster poll.

## Weapon drops carry their archetype (2026-08-13)

Owner: *"weapons should also be randomized on drop … when primaries and
secondaries drop they should be different weapon classes with their respective
affixes … make sure certain guns have certain leans towards affixes — like smg
fire rate, lmg damage, sidearm slide speed — **not required stats** but they can
drop more likely with those affixes."*

`Docs/Design/Power-Curve.md` §3 had already flagged this as the open boundary:
an item instance carried an item level and crossed nothing else, so which of
the eight guns a Primary drop actually *was* had no answer. The loadout screen
picked the weapon and the item supplied only numbers, which made a shotgun drop
and a sniper drop the same object wearing different affixes.

### What changed

- `FBreakerItemInstance::WeaponArchetype`, meaningful on Primary and Secondary
  only. It defaults to Rifle rather than to `Count`, so an item saved before
  the field existed loads as a rifle instead of as an invalid archetype.
- `EBreakerWeaponArchetype` moved into its own tiny header
  (`Weapons/BreakerWeaponArchetype.h`) so `Items/` can name a gun without
  pulling in the weapon component, the combat types and the feel layer.
- `RollItem` draws the archetype **before** the affixes, from the same
  deterministic stream, so a seed still reproduces an item exactly. The draw is
  uniform on purpose: a weighted table here would be a rarity system for gun
  *classes*, which is a separate design nobody has ruled on.
- Equipping a weapon item arms its archetype
  (`UBreakerWeaponComponent::SyncArchetypesToEquipment`, bound to
  `OnEquipmentChanged`). The loadout screen still works; an equipped item simply
  overrides it, which is the direction the owner asked for.
- Item cards print the gun, not the slot: `PRIMARY · SIDEARM`. "Primary" tells
  the player nothing the card's position did not.
- The three archetypes added in the O27 breadth pass are now called what they
  are — **Burst Rifle**, **Machinegun**, **Sidearm** — and one name table in
  `BreakerWeaponArchetype.h` serves the HUD, the loadout screen and item cards.
  A gun named in three places gets renamed in two. (`Rocket` also became
  `Rocket Launcher` everywhere as a consequence; two tests pinned the old
  short name and were updated.)

### Leans are weights, never filters

`UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier` multiplies an affix's
existing roll weight. It is clamped so a lean may only ever make a line **more**
likely — making one less likely is a different feature whose failure mode is a
stat that quietly cannot be found, and nobody asked for it. Every affix legal
on a weapon slot stays reachable on every archetype.

That distinction is the whole design. A hard restriction would make an SMG with
a huge damage roll impossible, and the item you were *not* supposed to get is
the one that makes a looter interesting. Setting every multiplier to 1.0
switches the feature off without making any item unrollable.

Each archetype leans toward what it already *is* mechanically, so the lean
reinforces the niche the weapon table authored rather than inventing a second,
contradictory identity:

| Archetype | Leans toward |
|---|---|
| SMG | Fire Rate ×3.0, Crit Chance, Added Damage |
| Machinegun | Weapon Damage ×3.0, Added Damage, Health |
| Sidearm | Slide Speed ×3.0, Move Speed, Dash Cooldown, Sliding Damage |
| Sniper | Crit Damage ×3.0, Crit Chance, Weapon Damage |
| Shotgun | Added Damage, Health, Sliding Damage |
| Rocket Launcher | Weapon Damage, Airborne Damage |
| Burst Rifle | Crit Chance, Crit Damage, Fire Rate |
| Rifle | The flattest row in the table, deliberately — the rifle's identity is that it has no sharp edge |

Multipliers top out at ×3.0. Much above ×4 starts to read as a filter.
All are `O2 PLACEHOLDER`.

### Two supporting changes the leans forced

**Fire Rate did not exist.** The owner named the SMG's lean specifically, so
`Weapon.FireRate` is a new affix with a real consumer rather than a card
number: `EBreakerStatTarget::FireRate` →
`EBreakerAggregatedAttribute::FireRateMultiplier` (base 1.0, replicated,
floored at 0.05 because a zero multiplier turns the fire interval into an
infinity and hangs the weapon) → `GetEffectiveRoundsPerMinute`, through which
**every** fire-timing call site now runs. That last part matters: a cadence stat
that applied to some timing sites and not others is how a weapon ends up firing
faster while its burst gap stays at the old rate. It rolls on the two weapon
slots only, because fire rate is a property of the gun.

**Slide Speed and Dash Cooldown now roll on weapon slots** as well as their
armour homes. Not breadth for its own sake: a lean toward a line that cannot
roll on the slot at all is a comment rather than a feature, and the first
version of this shipped with the sidearm's headline lean measurably doing
nothing. It also gives the Secondary slot a movement identity, which pairs with
the sidearm's fast swap.

### Coverage

`RiorsEdge.Items.WeaponDrops.Archetype` (every archetype drops; a seed
reproduces the item; armour is untouched) and
`RiorsEdge.Items.WeaponDrops.Leans`, which pins both halves of the design
against each other: the lean must be **visible** in real rolls, and it must
**not** be a filter. Measured across 120 drops per archetype — SMG 70 fire-rate
lines against Sniper 36, Sidearm 74 slide-speed lines against Rifle 42 — with
the off-lean roll asserted to still happen.

## Rarity MEANS something now (2026-08-14)

Ruling **O27**: *"choices should beat accumulation but there should be
significantly more options in all avenues."*

The problem, stated plainly. **Rarity gated affix COUNT and a tier ceiling and
nothing else.** An Anomalous item was a Standard item with more lines. Finding
one was an arithmetic event, not a build event — and this document had, from the
first pass, reserved Anomalous as the home of rule rewrites (the locked
aggregation rule says More multipliers are "reserved for tree nodes and
Anomalous rule rewrites") without anything implementing it.

### The rarity ladder, end to end

| Rarity | Affixes | Tier cap | Qualitative rule |
|---|---|---|---|
| Standard | 1-2 | T4 | — |
| Uncommon | 2-3 | T2 | — |
| Exceptional | 3-5 | T-1 | — |
| **Aberrant** | 4-6 | T-1 | **FOCUSED** — one affix rolls a tier better |
| **Anomalous** | 5-6 | T-1 | **A ROLLED RULE REWRITE**, exactly one. Equip cap 1 non-legendary — legendaries carry their own separate 1-equip cap [O37] |

The tier caps are the O29 re-derivation (T4 / T2, not the pre-O29 T3 / T1); see
the tier scale section for why Uncommon moved off T1.

**RARITY AND LEGENDARY ARE DIFFERENT AXES [O32].** Anomalous is a RARITY: the
fifth tier, gating affix count and the tier ceiling, and carrying one rule
rewrite ROLLED from the generic pool of four. **Legendary is a separate field**
(`FBreakerItemInstance::LegendaryId`) naming a specific authored item with a
fixed slot, guaranteed affixes and a HAND-AUTHORED rule that is never rolled.
Every legendary rolls AT Anomalous rarity; **most Anomalous drops are not
legendaries.** The two are stored separately (`Rule` and `LegendaryId`) because
a rule is a mechanic and a legendary is an identity — two legendaries could one
day share a rewrite, and the display name, the signature and the drop table all
key off the identity. **Equip caps split the same way [O37]:** exactly one
equipped legendary and, separately, one non-legendary Anomalous piece (plus
three Aberrant, O11) — a legendary does not draw against the Anomalous cap.

**ABERRANT IS FOCUSED.** Its first affix rolls against a ceiling one tier above
what item level alone allows, and never *worse* than the ordinary ceiling — a
floor as well as a raised roof, because a headline property that is invisible on
most drops is a comment rather than a feature (the first version of the archetype
leans shipped exactly that way). It is deliberately ONE slot: **O11 reserves
Aberrant's "1-2 unique modifier affixes" for the owner to name and design**, and
the focused slot is the seat those will occupy when they land. This pass does not
guess at what they are.

**ANOMALOUS CARRIES A RULE.** One rewrite, drawn deterministically from a pool of
four, on top of its affixes. Its equip cap of 1 is unchanged **and applies to
non-legendary Anomalous pieces only** — legendaries carry their own separate
1-equip cap and never draw against this one [O37] — so a character holds at
most one ROLLED rewrite at a time — which is what makes finding a *different*
Anomalous a decision rather than an accumulation.

### The constraint that shaped every rewrite: none of them is a More

O3 caps a build at **three** composed More multipliers, and the trees already
author six options against that cap. So an Anomalous rewrite that is simply a
fourth More is either dead weight (the new global clamp eats it) or a quiet nerf
to the three the player chose. **Every rewrite therefore changes a RULE the
aggregation obeys**, in the precedent of a tree keystone — which removes an
animation lock or makes casts free rather than adding a number.

`RiorsEdge.Items.Rules.NeverAuthorsAMore` walks every rule in the table, applies
it to a maximal loadout, and asserts the equipment contribution's More multiplier
is exactly 1.0 on **every** aggregated attribute.

### The four rollable rewrites

All four live in `Items/BreakerItemRules.{h,cpp}` and are applied inside
`UBreakerEquipmentComponent::AggregateStats` — the one function whose output is
both folded by `UBreakerAttributeSet` and read by `UBreakerCombatComponent`. A
rewrite expressed anywhere else could only reach a card.

| Rule | What it rewrites | Live consumer |
|---|---|---|
| **UNBOUND** | The condition PREDICATE. Every conditional affix pays regardless of its condition. | `FBreakerBuildConditionState` → the additive Increased bucket |
| **OVERFLOW** | The BUCKET boundary. Each point of Added Damage also grants 1% Increased Damage. | `DamageMultiplier`, Flat lane *and* Increased lane |
| **PROLIFIC** | TIER RESOLUTION. Every affix on this item resolves one tier better. | The rolled value, scaled by the authored T1→T0 ratio |
| **RELENTLESS** | A CAP. Physical Damage Reduction caps at 80% instead of 60%. | `PhysicalDamageReductionPercent` → `ReceiveDamage`'s incoming multiplier |

Design notes worth keeping:

- **UNBOUND is worth exactly the conditions, never more.** Pinned: its value
  while standing still equals the plain item's value with every condition live. A
  rewrite that also inflated the numbers would be a multiplier in a rule's
  clothing.
- **OVERFLOW does not MOVE the point, it doubles where it counts.** Added Damage
  stays in the Flat lane; the Increased lane gains the same figure. That is
  pinned too, because a rewrite that silently relocated the line would be a nerf
  to a build that had already invested in it.
- **PROLIFIC scales by the tier RATIO** rather than re-deriving at the better
  tier, so a lucky in-band roll is carried upward instead of flattened to the new
  tier's floor. T1 → T0 is the authored spike, **2.2x since O29**. NOTE: PROLIFIC
  got materially stronger without anybody editing it, because its whole value IS
  the size of a tier step and the steps grew. Measured at x1.462 on an optimized
  build against an authored ceiling of x1.35 - `RiorsEdge.Progression`
  `.RuleBandImpact` fails on it deliberately rather than the ceiling being widened. It is resolved **per item**,
  not per wearer — folding it into the wearer-wide rule set would leak the uplift
  onto every other equipped piece, and there is a test for that.
- **PROLIFIC is the non-Forge route to T0/T-1**, and its printed temper ceiling
  tightens by its own uplift so the two routes compose to one T-1 rather than to
  a T-2 the value curve has no entry for.

### `FBreakerItemInstance::Rule` is a FIELD, never derived from rarity

Load-bearing. Deriving the rewrite from `Rarity` would hand one to every
Anomalous item that already exists in a save, in a test fixture, and in the two
power-band loadouts — which build every piece at Anomalous purely to lift the
tier cap. An item earns a rewrite when it is *rolled* one.
`RiorsEdge.Progression.RuleBandImpact` asserts exactly this, so a future
refactor that "simplifies" the field away fails loudly.

## Three build-defining legendaries (2026-08-14)

`Docs/Vertical-Slice.md` has scoped "three build-defining legendary items" since
the first slice document. Zero existed.

The bar they were authored against is this project's own history: it has shipped
a skill node structurally incapable of raising damage, an affix pool where four
of eight slots could not raise damage, and a `DamageMultiplier` attribute read by
every damage path and written by nobody. **A legendary that lies is that failure
at higher volume.** So every effect below was checked against a live consumer
*before* it was designed, and every one has a test that drives the number rather
than the card.

**LEGENDARY AND ANOMALOUS ARE DIFFERENT AXES, NOT ONE SHARED CAP [O32, O37].**
Anomalous is a RARITY — the fifth tier, gating affix count and the tier
ceiling, and carrying one rule rewrite ROLLED from the generic pool of four.
Legendary is a separate field (`FBreakerItemInstance::LegendaryId`) naming one
of these three specific authored items, with a fixed slot, guaranteed affixes
and a HAND-AUTHORED rule that is never rolled. Every legendary still rolls AT
Anomalous rarity (O32), but equip caps are per axis (O37): exactly **one
equipped legendary**, separately from **one non-legendary Anomalous** piece
(and three Aberrant, O11) — a legendary does **not** consume the Anomalous
cap, so a player could in principle hold one legendary and one different
non-legendary Anomalous piece equipped at the same time. What actually keeps
these three legendaries from stacking is the dedicated one-legendary cap, not
a shared Anomalous cap: "build-defining" means the build is defined by the one
you chose, and three that stack would be a set bonus.

### DEADFALL — boots — bends the CONDITION system

> *Damage while Airborne also applies while Sliding and while Wall Riding. 40%
> less Air Control.*

Airborne is the richest conditional family in the game (the Airborne affix, plus
Freefall, Downforce and Terminal Velocity in the trees) and the hardest to hold:
you are airborne in bursts and it pays nothing the rest of the time. Deadfall
makes the two grounded traversal states count as airborne **for those lines
only**, turning a burst build into a rotation.

It is deliberately **not** UNBOUND. Freeing every conditional line would make the
legendary strictly better than the Anomalous rewrite that does exactly that, and
a legendary that outclasses the generic rewrite makes the generic rewrite dead
content. Tested: Deadfall pays **nothing** while standing still, and does not
free an unrelated conditional line.

The bill is an ordinary **negative Increased percentage** into the same additive
bucket every other layer bids on — never a sub-1.0 More. A downside is not an
exemption from the locked rule, and `RiorsEdge.Items.Legendary.Deadfall` asserts
both halves: exactly −40 points additive, and a More of exactly 1.0. Consumer:
`AirControlMultiplier` → the movement component's air steer rate.

Signature: Damage while Airborne, Slide Speed, Movement Speed.

### CADENCE — primary — bends the BUCKET rule and the SLOT rule

> *Fire Rate also grants Increased Damage at half its value. Cannot be worn with
> a Secondary.*

Fire Rate is a peer of Weapon Damage that lands on a *different* attribute
(`FireRateMultiplier` → `GetEffectiveRoundsPerMinute`), so the two never
compound. Cadence makes half of it compound — the first reason in the game to
stack cadence past the point the gun already feels fast. Fire Rate still reaches
the fire-timing attribute in full; the rewrite adds, it does not divert.

The bill is the Secondary slot, which is worth roughly a whole item's affixes
plus a swap-tempo option. It **ejects rather than refuses, in both directions**:
equipping Cadence sends the Secondary to the backpack, and equipping a Secondary
sends Cadence back. Nothing in `UBreakerEquipmentComponent` refuses an equip —
the rarity cap does not — and a rule inventing a second, harsher failure mode
would be worse than a disclosed swap. `FBreakerEquipPreview` gained
`bRuleDisplaces` / `RuleDisplaced` so the consequence is stated before the click,
exactly like the rarity cap's `LimitDisplaced`.

Signature: Fire Rate, Weapon Damage, Added Damage.

### OVERRUN — waist — bends the RESOURCE REGEN rule

> *Triple Resource Regeneration while airborne, sliding or wall riding. None
> otherwise.*

Gear resource regeneration is a flat per-second trickle ticked on the server,
which pays a player standing in the safe ring exactly as well as one in a fight.
Overrun deletes the trickle and pays triple for fast traversal — the only item in
the game that makes the class-resource loop a movement question. Both live class
loops (Swift's Momentum, Caster's Mana) read `ClassResource` as their bank.

Deliberately **not** a damage item. Two of the three would otherwise be offence,
and survivability and resource are the thin axes.

Signature: Resource Regeneration, Maximum Resource, Damage while Sliding.

### Finding them

`RollItem` redirects an Anomalous drop in a legendary's slot into that legendary
with probability `LegendaryChanceWithinAnomalous` (**0.25**, O2 PLACEHOLDER).
Anomalous is ~0.5% of drops before Drop Chance and only three of eight slots have
a legendary, so the effective rate is deliberately rare — and
`RiorsEdge.Items.Legendary.Signature` asserts one *can* come out of the ordinary
pipeline, because a legendary reachable only through a dev grant is the "exists
but cannot be found" failure one step removed.
`UBreakerEquipmentComponent::DevGrantLegendaries` puts all three in the backpack
(not equipped — the cap of one would silently eject two and the grant would look
broken).

A legendary is a real rolled item: its affix values come off the same tier curve
and two of them differ. The signature is what is *guaranteed*; a guaranteed line
the ordinary roll already produced is left exactly as it rolled, because
overwriting it would quietly re-roll a good value down to the floor of its tier.

## The Forge — minimal item agency (2026-08-14)

`Items/BreakerForgeLibrary.{h,cpp}`. This document has said since the first pass
that T0 and T-1 come "from crafting"; there was no crafting, so the two best
tiers in the game were unreachable by any means. The Forge exists in the world
already — it is the respec location and Kess, the Forge Keeper, stands at it.

**What this deliberately is NOT**: an economy. No vendor, no material drop table,
no orb inventory, no bench progression, and no item-derived materials — O12 rules
the currencies *scalar and tiered*, 3-4 of them, and that is exactly what this
implements. The slice does not want a crafting game; it wants the top of its own
tier curve to be reachable and a reason not to vendor every drop.

**The loop**: salvage a backpack item → scalar currency → temper, reforge or
attune. Three verbs, all deterministic, all pure functions over an item and a
wallet, all gated on `bIsAtForge` — the same flag and the same rule
`UBreakerProgressionComponent::RespecAtForge` already uses, so "the Forge is a
place you go" is one rule rather than two.

| Currency | Source | Buys |
|---|---|---|
| **Slag** | Every rarity, scaled by item level | Reforges; tempers into T12..T5 |
| **Flux** | Uncommon and above, rarity-pure | Attunes; tempers into **T4..T1** |
| **Sigil** | **Aberrant and above only**, rarity-pure | **T0 and T-1, and nothing else** |

Item level scales the **Slag** yield only. Flux and Sigil stay rarity-pure so
farming a low level cannot substitute for finding the rarity, which is the
shortest route from "minimal crafting" to "crafting replaces looting".

| Verb | Moves | Keeps | Cost |
|---|---|---|---|
| **Temper** | One affix, one tier better | Everything else | Steps with the target tier |
| **Reforge** | Every affix VALUE within its band | Ids and tiers | Slag, scaled by affix count |
| **Attune** | WHICH affixes | Count and tiers | Flux, the expensive verb |

Rules worth keeping in view:

- **Rarity still caps crafting.** A Standard item stops at **T4** (its
  `TierCapForRarity` ceiling since O29) no matter how much Sigil the player
  holds. Otherwise crafting would erase rarity's meaning in the
  same session this pass gave it one.
- **A refused craft costs nothing.** `FBreakerForgeWallet::Spend` is
  all-or-nothing; the Forge gate and the ceiling check both run before any spend.
  A partial spend is how one refused craft becomes a lost-currency bug report.
- **Temper re-derives the value at the new tier** rather than scaling the old
  one. Scaling would carry a bad in-band roll upward forever; re-deriving means a
  temper is always exactly what that tier is worth, which is also the only
  version a player can reason about.
- **Reforge draws from the identical distribution the drop pipeline's step 5
  uses**, so a reforged value and a dropped value are the same kind of number.
- **Attune keeps a legendary's signature and its rule.** Its identity is those
  lines plus that rewrite; a craft that could roll them away would turn the
  build-defining item into a lottery ticket.
- **Salvage and discard are different verbs.** `DiscardFromBackpack` still exists
  and still pays nothing; salvage destroys the same items and pays. That is what
  finally gives the discard pile a purpose.

Coverage: `RiorsEdge.Items.Forge.Wallet`, `.Salvage`, `.TemperReachesTheSpike`
(walks an affix all the way to T-1 and asserts it is worth the authored spike,
read from `TierSpikeTopMultiplier` — **3.6x since O29**, not the 1.8x this
document used to quote),
`.ReforgeAndAttune`, `.Loop` (salvage → temper an **equipped** item → the
composed MaxHealth attribute moves, which is the assertion that the Forge is a
gameplay system and not a data editor).

**KNOWN GAP, stated rather than hidden: there is no Forge UI.** `UI/` is another
lane's directory this pass, so every verb is reachable from Blueprint, from a
console exec, and from automation, and not yet from the inventory screen. The
mechanic is real; the button is not. The wallet is a replicated field on
`UBreakerEquipmentComponent`; **it is not yet in `UBreakerSaveGame`**, so
currency does not survive a session.

## Affix breadth on the non-damage axes (2026-08-14)

The first breadth pass took offence from one line to nine and left the other axes
where it found them: survivability was Physical DR alone, the resource family was
two lines that both did the same thing slowly, and damage-over-time had six skill
nodes bidding on it and **no gear support at all** — the same one-sided gap the
damage pass found on the other side. Four lines, taking the pool from 19 to
**23** (Resource Efficiency later made it 24):

| Line | Bucket | Slots | Live consumer |
|---|---|---|---|
| **Armour** | Flat | Helmet, body, gloves, boots, waist | `Armor` aggregated attribute → `GetEffectiveArmor()` → the mitigation formula |
| **Health on Kill** | Flat | Body, gloves, waist, neck, both weapons | `OnKillDealt` → `ApplyHealing` |
| **Resource on Kill** | Flat | Helmet, gloves, neck, waist, both weapons | `OnKillDealt` → the ClassResource bank |
| **Damage over Time** | Increased | Helmet, body, gloves, neck, primary | `DamageOverTimeMultiplier` → every DoT's application snapshot |

- **`Armor` joins `EBreakerAggregatedAttribute`.** The player's Armor attribute
  was authored 0 and written by *nobody*, so the most ordinary defensive stat in
  the genre had a whole mitigation pipeline behind it and no way in. It goes
  through the aggregator rather than a private path so that the first Bulwark
  node to author armour is additive from the day it lands — the same reasoning
  that produced `SlideSpeedMultiplier` and friends. Enemies and target dummies
  are untouched: they carry neither an equipment nor a progression component, so
  nothing captures their bases and the recompute never runs on them.
- **The on-kill pair is paid at an EVENT**, so it has no attribute to live in and
  needs a listener. `UBreakerEquipmentComponent::BindCombatEvents` subscribes to
  `UBreakerCombatComponent::OnKillDealt` once (guarded by `IsAlreadyBound`, so a
  rebind cannot pay the affix twice per kill) and is public for the same reason
  `BindAttributes` is: an affix that only pays when a real actor in a real world
  lands a real kill is exactly the line that ships broken.
  `RiorsEdge.Items.Affixes.OnKillReachesGameplay` binds, broadcasts the real
  delegate, and asserts health and resource actually move — then unequips and
  asserts the payment stops exactly.
- **On-kill rather than on-hit, deliberately.** On-hit sustain scales with fire
  rate and would make the SMG the only defensive weapon in the game; on-kill
  scales with how well the build is already doing and pays *nothing* against a
  boss, which is the right shape for a game whose difficulty lives in elites and
  bosses (O27).
- **DoT damage does not leak into the weapon-damage bucket.** DoTs snapshot
  separately and always have; a line that quietly buffed both would be strictly
  better than Weapon Damage. Pinned.

New archetype leans, all O2 PLACEHOLDER: SMG → Damage over Time ×2.0 (it is the
gun that applies Bleed on hit), Shotgun → Health on Kill ×2.0 (sustain belongs on
the gun that has to be in the fight), Sidearm → Resource on Kill ×1.8.

### FOUND AND REPORTED, not worked around

`UBreakerCombatComponent::AddClassResource` and `SpendClassResource` write
through the **generated** attribute setter, which `ensure()`s when there is no
owning ability system — so the entire class-resource path is unexercisable in a
world-less rig. `UBreakerAttributeSet::ApplyClassResource` exists for exactly
this and routes through the same `PreAttributeChange` clamp. Resource on Kill
therefore uses `ApplyClassResource` directly, with identical observable
behaviour, and the note is at the code. **The one-line fix belongs in `Combat/`**
(have those two functions use `ApplyClassResource`), which is another lane's
directory this pass.

## Band impact of the rule rewrites

The band is unchanged **by the rarity pass**, and that is by construction rather
than by luck: `FBreakerItemInstance::Rule` defaults to `None` and the two
power-band loadouts are authored affix by affix, so neither character carries a
rewrite even though every piece is built at Anomalous to lift the tier cap. The
newer affixes are not in either loadout either.

Which means the band test alone would say **nothing** about whether the rewrites
are balanced. `RiorsEdge.Progression.RuleBandImpact` (**renamed** off
`RiorsEdge.Progression.PowerBand.RuleImpact` — UE's automation tree cannot hold
a leaf test at a node that is also a parent, so the old path was silently
swallowing `RiorsEdge.Progression.PowerBand` itself and the 8-10x assertion had
not run since this test was added) measures the thing that actually matters: the
STEP one Anomalous rewrite is worth on top of a build that has already done
everything else right, measured both in the optimized build's rotation
(airborne / recently dashed / at Redline) and standing still.

The properties it asserts, all `O2 PLACEHOLDER` bounds: a rewrite never *lowers*
an optimized build (in the rotation or grounded), it is worth at most **x1.35**
on top of one, and it is **smaller than the band it lives in** — a rewrite that
outweighed the band would make every other decision a rounding error, which is
O27 inverted.

The shape of the answers, which is what should survive a retune even though the
figures will not:

| Rewrite | Where it pays |
|---|---|
| UNBOUND | Nothing in the rotation; the largest step in the table standing still |
| OVERFLOW | A small step in both, larger grounded |
| PROLIFIC | A small step in both, and it grew with the ladder |
| RELENTLESS | Exactly 1.000 in both — it is purely defensive and the band measures damage; its effect is asserted separately through the mitigation formula |

Read that carefully: **UNBOUND is worth nothing in the rotation and the most
standing still**, which is exactly right — its whole job is to free conditional
lines, so it is worthless to a player already holding every condition and
transformative to one who is not. Measuring it only in the rotation would have
reported the largest rewrite in the table as inert.

**PROLIFIC got materially stronger without anybody editing it**, because its
whole value IS the size of a tier step and O29 grew the steps (T1 -> T0 went
from 1.4x to 2.2x). It was last measured at x1.462 against the authored x1.35
ceiling, and the test **fails on it deliberately** rather than the ceiling being
widened — that is a balance ruling, not test maintenance.

**Every numeric step in this section predates the fixture move to item level
120** (baseline T3 vs optimized T1) and none has been re-measured since. Treat
the figures as needing a suite run, and the ordering above as the design.

## FOR THE OWNER — contradictions this audit could not resolve (2026-08-14)

Recorded rather than decided, per the audit rules. Each states both readings and
what each would cost.

1. **NO DROP CAN CARRY THE ENDGAME CURVE, and the reason is not the one this
   document and Power-Curve.md both name.** `GetDropItemLevel` no longer clamps
   to 50 — that was fixed — but `ABreakerEnemy::ApplyChassis` immediately
   re-clamps its own field: `EnemyLevel = FMath::Clamp(EnemyLevel, 1, 50)`
   (`Combat/BreakerEnemy.cpp`), and `EnemyLevel` is what `GrantLoot` passes to
   `RollDrop` and `RollItem`. The field's own comment still gives the retired
   reason ("affix tiers are authored to 50"). So the item system rolls to 120,
   the weapon curve evaluates to 120, the Forge prices to 120 — and every item
   the shipping game actually drops stops at 50.
   *Reading A*: it is a leftover and the clamp should become
   `UBreakerAffixLibrary::MaxItemLevel`. Cost: one line in `Combat/`, plus a
   test that asserts it, and the endgame gap really closes.
   *Reading B*: it is a deliberate hold until the owner rules on item levels
   101-120 having no source. Cost: nothing, but the O29 chase does not exist in
   the playable game and `EndgameComposition` describes an intended game rather
   than the shipping one.
   **Nothing in the suite currently fails on this** — `EndgameComposition`
   checks the library function, which is now correct, and never touches the
   actor.

2. **The build variance band, `RiorsEdge.Progression.PowerBand`, is RED.** The
   authored target is 8-10x; the test's own note records ~15x after O29. The
   test refuses to widen its own range because that would choose an answer
   silently. *Reading A*: the band moves — a longer ladder should separate
   builds further, and 8-10x was authored before the endgame existed. Cost: two
   constants and an amended Power-Curve §4. *Reading B*: the content retunes —
   crit and the additive bucket come down until the composed band lands back at
   8-10x on the new ladder. Cost: a pass over the pool's ceiling anchors and the
   crit lines, and every measured figure in this document moves with it.

3. **PROLIFIC breaches the rewrite ceiling.** Last measured x1.462 against the
   authored x1.35, because its value IS a tier step and O29 grew the steps.
   *Reading A*: the ceiling was authored against a linear ladder and should
   rise. *Reading B*: PROLIFIC is re-specified (a fractional uplift, or the T0
   spike re-sited). Cost is small either way; the point is that it is a balance
   ruling, not a test fix.

4. **Item levels 101-120 still have no source.** Unchanged from the note in the
   item-level section, restated here because it composes with item 1: if the
   enemy clamp is lifted to 120, drops still stop at area level 100, so the top
   twenty levels remain Forge-only. Worth `1.09^20 = 5.6x` base damage.

5. **Two code comments in `Items/` are stale and would mislead the next reader**
   (not fixable from this lane — `Source/` is off limits to this pass):
   `BreakerForgeLibrary.h`'s header still says the spikes are "T0 at 1.4x T1 and
   T-1 at 1.8x" and that "a Standard item stops at T3"; `BreakerAffixLibrary.h`'s
   `BestTierForItemLevel` declaration still says "one tier per 10 levels", which
   the two-slope implementation below it replaced.
