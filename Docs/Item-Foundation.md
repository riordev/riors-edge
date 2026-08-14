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
purchase is dead weight rather than a quiet nerf to the other three. LIMIT:
the clamp is per-LAYER. Anomalous items do not author Mores yet; when they do,
the cap has to move to one shared clamp across the whole composition or a
build could hold three tree Mores plus an item's.

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

## The affix pool — WIDENED [O27 2026-08-13]

`UBreakerAffixLibrary::GetSliceAffixPool()` is the C++ fallback pool (same
zero-setup convention as weapons). It was twelve lines of which exactly ONE
was offensive, and that one rolled on four of eight slots — so helmet, body
armour, boots and waist were structurally incapable of raising damage. That is
the concrete reason "full level 50 gear" did not feel like anything, and O27
rules it out.

**Eighteen lines, nine of them offensive:**

| Line | Bucket | Condition | Slots |
|---|---|---|---|
| Health, Resource Regen, Max Resource, Move Speed, Drop Chance, Physical DR | as before | — | all 8 |
| Slide Speed / Air Control / Dash CDR | Increased | — | boots+waist / boots+neck / boots+gloves |
| Critical Chance | Flat | — | helmet, gloves, neck, weapons |
| Critical Damage | Flat | — | helmet, gloves, neck, weapons |
| **Weapon Damage** | Increased | — | **all 8** |
| **Added Damage** | Flat | — | helmet, gloves, waist, neck, weapons |
| **Damage while Airborne** | Increased | Airborne | boots, helmet, neck, primary |
| **Damage while Sliding** | Increased | Sliding | boots, waist, body, secondary |
| **Damage while Wall Riding** | Increased | Wall riding | boots, waist, gloves, body |
| **Damage at Redline** | Increased | Redline momentum | neck, body, helmet, primary |
| **Damage after Dashing** | Increased | Recently dashed | gloves, waist, neck, secondary |

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

**The band test is the guard rail.** `RiorsEdge.Progression.PowerBand`
(`Tests/BreakerPowerBandTests.cpp`) composes a baseline and an optimized
level-50 character out of the real affix pool and the real trees, through the
real `FBreakerAttributeAggregator`, and asserts the composed ratio lands in
Power-Curve §4's 8-10x band. It logs the ratio layer by layer, so a future
tuning pass can see WHICH layer moved rather than only that the band broke.
Current: flat 1.16x, increased 2.35x, more 1.93x, crit 1.66x, composed 8.74x.

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
| Affix | WeaponDamage | DamageMultiplier attribute (this pass) | yes |
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
- **Added Damage prints without a unit.** `SBreakerMenu::DescribeAffix` decides
  the "%" suffix from the bucket, with a hard-coded exception for the two crit
  targets. Added Damage is a Flat line authored in percentage points of base
  weapon damage, so it renders as "Added Damage  +5.0  T1". Fixing it properly
  means a display-format flag on `FBreakerAffixDefinition` that the UI reads,
  which is a UI-layer change; reported rather than hacked around.
- **The O3 More cap is enforced per LAYER, not globally.** Three tree Mores is
  the whole budget today because nothing else authors one. When Anomalous items
  gain Mores, the clamp has to move to a single shared pass over the composed
  contribution or a build can hold three tree Mores plus an item's.
- **Conditional lines are evaluated on a component tick.** The equipment and
  progression components each re-read the movement state each frame and rebuild
  their contribution only on a transition. That is correct and cheap, but it
  means a conditional bonus lands on the frame AFTER the state changes. If a
  one-frame lag on entering a slide ever reads as mushy, the fix is an event
  from the movement component rather than a faster poll.
