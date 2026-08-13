# Item Foundation

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

## Item instances

`FBreakerItemInstance` (`BreakerItemTypes.h`) carries a GUID, a definition id
(FName, never a pointer), slot, rarity, **item level (1-50)**, rolled affixes,
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

GAP [O6]: the ZoneLevel-per-zone table, the mapping from content difficulty
to a specific TierBonus, and the final clamp against the 1-50 item level
range are not authored here. Value authoring is frozen under O2 — owner to
backfill after the wave-mode instrumentation reports.

## Tier scale

Ten tiers, T8 worst to T-1 best, stored as printed (8..1, 0, -1).
`UBreakerAffixLibrary`:

- `ValueForTier` — linear T8→T1, then the deliberate spike: T0 = 1.4x T1,
  T-1 = 1.8x T1.
- `BestTierForItemLevel` — one tier roughly every 7 levels; level 50 opens T1.
  T0/T-1 never come from item level — they are crafting/boss territory.
- `TierCapForRarity` — Standard caps at T3, Uncommon at T1, the rest reach
  T-1 (ceiling only; item level still gates what actually rolls).

## The slice affix pool

`UBreakerAffixLibrary::GetSliceAffixPool()` is the C++ fallback pool (same
zero-setup convention as weapons): the universal core six (Elemental DR
excluded until a resistance model exists), three movement affixes (Slide
Speed, Air Control, Dash CDR — one per weapon archetype pairing), and Crit
Chance/Damage. Do not author the full pool before this pipeline is validated
in play.

## Roll pipeline

`UBreakerLootLibrary` implements steps 1-5 of the master-sheet pipeline,
fully deterministic from a seed:

1. `RollRarity` — weighted table; Drop Chance % drains weight out of Standard.
2. Affix count from the rarity range.
3. Affix selection — slot-legal, weighted, no duplicates, max 4 prefixes and
   4 suffixes.
4. Tier per affix — item level gated, rarity capped, halving odds per step up
   so T1+ feels earned.
5. Value within the tier band.

Step 6 (Aberrant/Anomalous fixed signatures) is not built; signatures need
their own design pass.

## Equipment

`UBreakerEquipmentComponent` (on `ABreakerCharacter`) owns the eight slots
plus a backpack, replicates both, and folds equipped affixes into
`UBreakerAttributeSet`: MaxHealth (preserving the health fraction), Max
Class Resource, Crit Chance/Multiplier, and MoveSpeed. Gear resource regen
ticks on the server. Movement multipliers (slide/air-control/dash) are
exposed on `GetStats()` for the movement component to consume — that wiring
is not connected yet. Physical DR from gear folds into the incoming damage
multiplier inside `UBreakerCombatComponent::ReceiveDamage`.

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
CriticalMultiplier, MoveSpeed and DamageOverTimeMultiplier. Attributes a
single system owns outright (Shield, Armor, DamageMultiplier) stay off it.
`FBreakerEquipmentStats` and `FBreakerNodeStats` are unchanged: the movement,
combat, weapon and loot consumers still read the composed multipliers from
`GetStats()` / `GetNodeStats()`.

Coverage: `Source/RiorsEdge/Tests/BreakerAttributeAggregationTests.cpp` plus
`RiorsEdge.Items.Equipment.AttributeContribution` and
`RiorsEdge.Progression.RespecRestoresAttributes`.

OPEN: `UBreakerCharacterMovementComponent` still multiplies the gear and tree
movement multipliers together (`GearMoveSpeedMultiplier()` and friends) rather
than adding their Increased percentages, which contradicts the rule above.
That is a movement-layer change and needs an owner ruling.

## Gym drops

`ABreakerEnemy` grants a rolled item to the first player's backpack on death
(`bDropsLoot`), seeded by spawn location and kill count. No pickup actor or
inventory UI yet — `OnItemAcquired` is the Blueprint/UI hook.

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

- Crafting (add/reroll/upgrade/exalt), signatures, pickup actors, loot UI.
- Movement affix multipliers are computed but not yet consumed by
  `UBreakerCharacterMovementComponent`.
- Save persistence for items (structs are save-shaped and versioned).
- All values are placeholder until the gym feedback pass re-anchors them.
