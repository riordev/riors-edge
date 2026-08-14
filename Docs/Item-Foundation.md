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
| Fire Rate | Increased | — | primary, secondary |
| **Armour** | Flat | — | helmet, body, gloves, boots, waist |
| **Health on Kill** | Flat | — | body, gloves, waist, neck, both weapons |
| **Resource on Kill** | Flat | — | helmet, gloves, neck, waist, both weapons |
| **Damage over Time** | Increased | — | helmet, body, gloves, neck, primary |

The last four landed in the 2026-08-14 non-damage breadth pass, taking the pool
to **22 lines**; see that section for why each one has a live consumer.

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
| Affix | WeaponDamage | DamageMultiplier attribute | yes |
| Affix | Armour | Armor attribute → `GetEffectiveArmor()` → mitigation | yes (2026-08-14) |
| Affix | LifeOnKill | `OnKillDealt` → `UBreakerCombatComponent::ApplyHealing` | yes (2026-08-14) |
| Affix | ResourceOnKill | `OnKillDealt` → ClassResource | yes (2026-08-14) |
| Affix | DamageOverTime | DamageOverTimeMultiplier attribute → DoT snapshots | yes (2026-08-14) |
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

- ~~Crafting~~ and ~~signatures~~ are BUILT (2026-08-14) — see the Forge and
  legendary sections. What is still missing from crafting is the "add an affix"
  verb (deliberately: an item's affix COUNT is rarity's job and adding one would
  make rarity craftable) and any UI at all. Pickup actors and loot UI exist.
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
| Standard | 1-2 | T3 | — |
| Uncommon | 2-3 | T1 | — |
| Exceptional | 3-5 | T-1 | — |
| **Aberrant** | 4-6 | T-1 | **FOCUSED** — one affix rolls a tier better |
| **Anomalous** | 5-6 | T-1 | **A RULE REWRITE**, exactly one. Equip cap 1 |

**ABERRANT IS FOCUSED.** Its first affix rolls against a ceiling one tier above
what item level alone allows, and never *worse* than the ordinary ceiling — a
floor as well as a raised roof, because a headline property that is invisible on
most drops is a comment rather than a feature (the first version of the archetype
leans shipped exactly that way). It is deliberately ONE slot: **O11 reserves
Aberrant's "1-2 unique modifier affixes" for the owner to name and design**, and
the focused slot is the seat those will occupy when they land. This pass does not
guess at what they are.

**ANOMALOUS CARRIES A RULE.** One rewrite, drawn deterministically from a pool of
four, on top of its affixes. Its equip cap of 1 is unchanged, so a character
holds at most one rewrite at a time — which is what makes finding a *different*
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
  tier's floor. T1 → T0 is the authored 1.4x spike. It is resolved **per item**,
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
`RiorsEdge.Progression.PowerBand.RuleImpact` asserts exactly this, so a future
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

A legendary is always **Anomalous**, which means the existing equip cap of one
Anomalous piece is also the cap on legendaries. That is the design, not a side
effect: "build-defining" means the build is defined by the one you chose, and
three that stack would be a set bonus.

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
| **Slag** | Every rarity, scaled by item level | Reforges; tempers into T8..T4 |
| **Flux** | Uncommon and above, rarity-pure | Attunes; tempers into T3..T1 |
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

- **Rarity still caps crafting.** A Standard item stops at T3 no matter how much
  Sigil the player holds. Otherwise crafting would erase rarity's meaning in the
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
(walks an affix all the way to T-1 and asserts it is worth the authored 1.8x),
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
damage pass found on the other side. Four lines, taking the pool from 18 to
**22**:

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

## Band impact of this pass

**The 8-10x build variance band is UNCHANGED at 8.74x**, and that is by
construction rather than by luck: `FBreakerItemInstance::Rule` defaults to `None`
and the two power-band loadouts are authored affix by affix, so neither character
carries a rewrite even though every piece is built at Anomalous to lift the tier
cap. The four new affixes are not in either loadout either.

Which means the band test alone would say **nothing** about whether the rewrites
are balanced, so `RiorsEdge.Progression.PowerBand.RuleImpact` measures the thing
that actually matters — the STEP one Anomalous rewrite is worth on top of a build
that has already done everything else right, measured both in the optimized
build's rotation (airborne / recently dashed / at Redline) and standing still:

| Rewrite | Step in rotation | Step standing still | Band with it |
|---|---|---|---|
| UNBOUND | x1.000 | **x1.645** | 8.74x |
| OVERFLOW | x1.044 | x1.083 | 9.12x |
| PROLIFIC | x1.079 | x1.074 | 9.43x |
| RELENTLESS | x1.000 | x1.000 | 8.74x |

Read that table carefully. **UNBOUND is worth nothing in the rotation and 1.65x
standing still**, which is exactly right: its whole job is to free conditional
lines, so it is worthless to a player already holding every condition and
transformative to one who is not. Measuring it only in the rotation would have
reported the largest rewrite in the table as inert. **RELENTLESS is 1.000 in
both** because it is purely defensive and the band measures damage — its effect
is asserted separately, all the way through the mitigation formula.

The test asserts three properties per rewrite, all `O2 PLACEHOLDER` bounds: it
never *lowers* an optimized build, it is worth at most **x1.35** on top of one,
and it is **smaller than the band it lives in** — a rewrite that outweighed the
8.7x band would make every other decision a rounding error, which is O27
inverted.

The band is therefore 8.74x for a character with no rewrite and at most 9.43x for
one carrying the best rewrite for its build. Both are inside the 8-10x band.
