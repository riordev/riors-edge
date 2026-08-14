# Combat foundation

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Source: `Source/RiorsEdge/Combat/`. The canonical resolution spec is
`Docs/Design/Damage-Pipeline.md`; where this file and that one disagree, that
one wins and this one is the implementation note.

## Resolution order

All weapons, abilities, statuses, hazards, and enemies submit the same damage
request contract (`FBreakerDamageRequest` -> `UBreakerDamageLibrary::
ResolveDamage` -> `FBreakerDamageResult`), and it resolves in this order:

1. Base damage and source scaling.
2. Weak-point multiplier when applicable.
3. Critical roll or previously snapshotted critical result.
4. **Passive dodge roll, then passive block roll** (O1). Neither applies to
   damage over time. A dodge returns immediately with zero damage; the block
   *reduction* is rolled here but applied at step 6.
5. Armour mitigation and penetration — preceded, in `ReceiveDamage`, by the
   facing selection and the flat armour strippers.
6. The incoming-damage multiplier (gear Physical DR plus the keyed modifier
   chain), then a blocked hit's reduction.
7. Shield routing unless explicitly bypassed.
8. Remaining damage to health.
9. Shield-break, damage, dodge/block, and death events, plus the attacker-side
   `OnHitDealt` / `OnKillDealt` pair carrying `FBreakerHitContext`.

Per-element resistance is specified between armour and shields (O5/O19) and is
**NOT BUILT** — `EBreakerDamageFamily` is `Physical / Elemental / TrueDamage`
with no per-element split and no resistance attribute.

True Damage bypasses armour but does not automatically bypass shields. Shield bypass is an independent rule so status and ability behavior stays explicit.

## Armour

Current mitigation uses `EffectiveArmor / (EffectiveArmor + 100)` capped at 80%. Flat armour penetration reduces effective armour before the formula. These constants are prototype values and should later move into a combat policy Data Asset.

Physical shield-bypassing DoTs—currently Bleed and Poison—receive half the normal armour mitigation. This implements the concept's “ignore shields, halved versus armour” rule without inventing a second damage pipeline.

## Critical DoTs and snapshots

When a snapshotting DoT is applied, it captures source power, critical chance/multiplier, DoT multiplier, source tags, and one critical result. Every tick uses that stored result and does not change when the source equips another item or receives a temporary buff.

This means an individual application either critically ticks for its full lifetime or does not. If design later prefers each tick to roll separately, introduce a different explicit snapshot policy rather than silently changing this contract.

DoT ticks carry a proc coefficient. They must not trigger arbitrary on-hit effects unless those effects declare DoT compatibility.

## Critical policy

Critical hits are the only damage multiplier of their kind. `CriticalChance` and `CriticalMultiplier` on the attribute set are the single source of truth.

An earlier itemization draft proposed a separate "chance to deal double damage" affix. That is cut. A second independent multiplier duplicates the critical mechanic, and stacking two of them is the multiplicative-explosion risk identified as the first balance concern in the progression architecture. Do not reintroduce a parallel multiplier through affixes, nodes, or class mechanics without revisiting this section.

## Replicated attributes

`UBreakerAttributeSet` replicates nineteen attributes, every one
`COND_None, REPNOTIFY_Always`:

Health, MaxHealth, Shield, MaxShield, Armor, ClassResource, MaxClassResource,
**ClassResourceFloor**, CriticalChance, CriticalMultiplier, DamageMultiplier,
DamageOverTimeMultiplier, MoveSpeed, SlideSpeedMultiplier,
AirControlMultiplier, DashCooldownReduction, FireRateMultiplier,
ResourceCostMultiplier, ClassResourceRegen.

Fourteen of them are *aggregated* — MaxHealth, MaxClassResource,
CriticalChance, CriticalMultiplier, MoveSpeed, DamageOverTimeMultiplier,
DamageMultiplier, SlideSpeedMultiplier, AirControlMultiplier,
DashCooldownReduction, FireRateMultiplier, Armor, ResourceCostMultiplier,
ClassResourceRegen — meaning gear and skill nodes both submit contributions and
the set folds them as `(Base + sum(Flat)) * (1 + sum(Increased)/100) *
prod(More)`. Shield, MaxShield and ClassResourceFloor are owned outright by one
system each and stay off that path. See the "Unified attribute application"
section of `Docs/Item-Foundation.md` for the rule and why it exists.

Clamps worth knowing, all in `PreAttributeChange`: CriticalChance `[0,1]`,
CriticalMultiplier `>= 1`, DashCooldownReduction and FireRateMultiplier floored
at 0.05 (a zero would divide by zero or hang a weapon), ResourceCostMultiplier
`[0.25, 2.0]`, ClassResourceFloor `<= 0`, and — the one line that makes Overcast
reachable — **ClassResource clamps to `[min(0, ClassResourceFloor),
MaxClassResource]`** rather than to zero.

Mana, Momentum, Scrap, Grit, and Charge all use the generic class-resource attributes. Their generation/decay rules belong to class mechanics, not the shared attribute set.

## The class-resource contract

Two loops are live and they run in **opposite directions**, which is the single
most misdescribed thing in this corpus:

- **Momentum (Swift) accumulates.** It starts at zero, generates from sprint,
  air time, slide, wall ride, dash and weak points against a per-second budget,
  decays in bands, and drives Settled / Running / Redline.
- **Mana (Caster) DRAINS.** Owner ruling 2026-08-14: *the bar starts FULL and
  goes down when using spells.* This SUPERSEDES Class-Kits §2.1's accumulating
  bank. **Passive regeneration is the primary recovery path**
  (`PassiveRegenPerSecond`, O2 PLACEHOLDER 6.0/s), and the conditional sources —
  weapon hits, weak points, kills, status applications, reloads — are
  **accelerators on top of it, not the income**. The generation budget was
  re-weighted at the cap for the inversion (`GlobalGenerationCap` 20.0/s ->
  6.0/s) so the relative rates of the conditional sources survive it.
  Regeneration is applied above the safe-zone gate and outside the budget,
  deliberately.

Two consequences for anything documenting the Caster:

1. **A node or affix that "generates Mana on X" is now an accelerator**, not the
   thing that makes the class function. Any description that reads as "this is
   how you get Mana" is wrong after the inversion.
2. **Efficiency became a real stat.** Once a resource is spent down rather than
   banked up, efficiency and regeneration decide how often a caster acts, and
   only regeneration existed. `Core.ResourceEfficiency` bids a negative
   Increased into `ResourceCostMultiplier`; `UBreakerCasterAbility` floors the
   composed multiplier at 0.10 so no stack of gear makes a cast free.

`ClassResourceFloor` is published by the Mana component and only while the
permanent class is Caster (Overcast, O2 PLACEHOLDER -20), so Swift's bank is
bit-identical to what it was. `UBreakerCasterAbility::CheckCost` compares
against the floor and **refuses** a cast that would breach it, never truncates.

**Known gap:** `ClassResourceRegen` exists as an aggregated, replicated
attribute and **no class loop bids into it** — the Mana component still ticks
its own `PassiveRegenPerSecond` separately, so gear regen and class regen are
not yet in one bucket. It is the same shape as the bugs the aggregation pass
fixed, caught before it shipped a wrong number.

## Stamina — REMOVED [O1 2026-08-12]

There is no stamina. `Stamina` and `MaxStamina` were deleted from the attribute
set and the combat component in the same commit as the ruling, and **they must
never be re-added**. This section previously described a live spend/regen path
(100 maximum, 20/s after 1.2 s) and block/dodge calling into it; none of that
exists. Block and dodge are passive chance layers with no cost — see below.
Parry, when built, is the only defensive input and carries its own short
cooldown.

## Block and dodge — passive [O1]

`UBreakerCombatComponent` exposes `DodgeChance`, `BlockChance` and
`BlockMitigation` (default 0.5) for class kits and gear to raise; the
progression component's bonuses are added on top and the sum is clamped to
[0,1]. Neither applies to a damage-over-time tick. A dodge pays a small class
resource refund (`DodgeResourceRefund`, O2 PLACEHOLDER 5.0) and short-circuits
the whole path, so nothing keyed off being hit fires on a dodged hit.
GAP [O1]: whether the refund survives as base kit or becomes a tree rewrite is
still the owner's call.

## Zones (`Combat/BreakerZoneActor.{h,cpp}`, `Combat/BreakerZoneMath.{h,cpp}`)

A zone is a persistent, replicated damage/effect volume: a lifetime, a
membership set, a tick cadence, a damage/status payload, and an optional
armour strip released when an occupant leaves. It is shared infrastructure —
Rot, Support's Suppress, Gunsmith's Disruptor, boss telegraphs and
environmental hazards are all the same volume with different payloads.

- `FBreakerZoneSpec` carries a whole `FBreakerDamageRequest`, not a damage
  number, so the caster's snapshot (crit chance/multiplier, composed damage
  multiplier, source tags) travels with the zone. The zone fills in the
  Instigator, the source location and a per-occupant, per-tick seed, and
  submits through `ReceiveDamage` like any other hit. Kill credit, outgoing
  modifiers, and the passive defensive layer therefore work unchanged.
- Membership is a cylinder by default (`HalfHeightCm > 0`), so a target on a
  crate inside the footprint is in the puddle and one on the roof is not.
- Cadence is a countdown that SUBTRACTS the interval rather than resetting,
  with a hitch guard whose excess is discarded, never banked. A zone must tick
  at the same rate at 30 and 120 fps.
- Occupancy and damage are SERVER ONLY; the spec replicates so a client can
  draw the footprint.
- `SetExpiryPaused` is the Long Dark keystone: a paused zone keeps working and
  stops ageing.
- Anti-stack (VW4) lives at the SPAWNER, once: `FindRefreshableZone` finds a
  live zone with the same tag, from the same caster, within half a radius, and
  the ability refreshes it instead of placing a second one.

## Facing-dependent armour

`UBreakerDamageLibrary::GetFacingArmorMultiplier` selects the armour value from
where the hit came from: a **2D** dot (Z deliberately excluded, so shooting from
a rooftop is not a rear hit) against `RearArcCosine`; inside the front arc the
multiplier is 1.0, outside it is the target's `RearArmorMultiplier`. It is pure
maths with an early-out when the multiplier is 1.0, and it is applied per hit in
`ReceiveDamage` after the flat strippers and before the mitigation curve.

`UBreakerCombatComponent::RearArcArmorMultiplier` defaults to **1.0 — the
feature is OFF for every ordinary enemy**, which is what keeps it a designed
archetype property rather than a global rule. The only writer today is
`ABreakerWardenEnemy` (and therefore the boss), which sets it to **0.0**: a rear
or flank hit bypasses the Warden's armour entirely. That is the whole answer to
"how do I fight the thing with a shield". `RiorsEdge.Combat.Archetypes
.FacingArmor` covers it.

## Flat armour reduction

`UBreakerCombatComponent::PushArmorReduction(FName Key, float Flat)` / `Pop`.
Flat and never percentage (Class-Kits VW7: this is what protects the boss
armour cap), keyed so two overlapping Rots share one entry rather than
double-stripping, and clamped at zero at the point of use — negative armour
would invert the mitigation formula into a damage bonus.

## Healing (`ApplyHealing`, `UBreakerDamageLibrary::ResolveHealing`)

Healing is the mirror of the damage contract, not a shortcut around it.
`FBreakerHealRequest` -> pure `ResolveHealing` -> `FBreakerHealResult`, written
through the attribute set by the combat component and broadcast as `OnHealed`
on the target and `OnHealingDealt` on the healer (the twin of `OnHitDealt`).

- Health fills first, then overheal. Overheal is REPORTED at full value even
  when some of it became shield: Support's Charge loop must generate nothing
  from overheal, and it cannot honour a number it never receives.
- `bOverhealToShield` is a request field, so Leech routes overheal to shield
  and Siphon does not, without two functions.
- A heal never resurrects. Healing is not the revive system.
- A negative heal is not damage; damage has exactly one entry point.

## Status consumption (`UBreakerStatusComponent`)

`GetDistinctStatusTypeCount`, `ConsumeAllStatuses`, `ConsumeStatus`,
`ScaleRemainingDurations`, `SetStackCapDelta`, and a distinct `OnStatusConsumed`
event so a listener can tell a detonation from a natural expiry. Consuming a
status that is not present is a legal no-op that reports "not found".

Counts are of DISTINCT TYPES, never stacks — the explicit anti-stacking rule
(Class-Kits C6). The detonation curve lives world-free in
`Combat/BreakerStatusConsumption.h` with the §2.7.5 bound (6 statuses is at
most 2.2x 2 statuses) asserted in automation against the shipping defaults.

**Two statuses exist**, both `FGameplayTag`-identified rather than enumerated:
`Status.Bleed` and `Status.Poison`. The detonation parameters are sized for six
("five elements plus Bleed") and the other four arrive with O5's element model,
so the curve is built for content that does not exist yet. Stack cap is 10 per
status plus the Affliction `StackCapDelta`; the boss overrides its own cap to 3
so a DoT build cannot trivialize a 20-45s fight.

**One system consumes**: `UBreakerAbility_Resonance`. Everything else is test
coverage. That is a real gap in reach, not a gap in the mechanic.

## Projectiles (`Combat/BreakerProjectileBase.{h,cpp}`)

One reusable replicated projectile: travel, collision, lifetime, impact
resolution through the ordinary damage contract with a proper Instigator, an
optional list of carried statuses (applied crediting the SHOOTER, not the
projectile), and presentation hooks. `ABreakerEnemyProjectile` is re-expressed
on it with every one of its shipped numbers preserved.
`ABreakerRocketProjectile` is NOT re-expressed: it lives in `Weapons/` and its
explosion and detonation-flash lifetime would make the fold a behaviour change
disguised as a refactor. **So "every projectile shares the base" is false and
deliberately so** — the enemy projectile is the only subclass, plus
`UBreakerAbility_Fracture`, which spawns the base class directly.

## The enemy taxonomy — two families

`EBreakerEnemyFamily` is **`Vestige` and `Altered`** (O9's three fields are
Archetype / Rank / Modifiers; family is the fiction layer over Archetype).
`EBreakerSeveranceStage` is `NotApplicable / Early / Mid / Late` and belongs to
the Altered alone — a Vestige never prints a stage.

The behavioural contract is a **hard family gate**, not a stage lookup:
`StageUsesCover` and `StageFlinches` both return true **only for an Altered in
Early severance**, and false for every Vestige at any stage. A Mid or Late
Altered still carries its equipment and still shoots; it no longer takes cover
and no longer flinches, which is the read the fiction wants.

**NOT REACHED: nothing in `Game/` sets either field.** Every enemy the gym
spawns is a `Vestige` at `NotApplicable`, so cover discipline and flinch are
always off in play, and the two Altered-only modifiers (Anchored, Warding Aura)
**cannot roll at all** today. The mechanism is built and the content does not
use it.

## Enemy modifiers (`Combat/BreakerEnemyModifiers.{h,cpp}`, `BreakerModifierComponent`)

Ten modifiers, each with a weight class and a **pressure** (Durability,
Mobility, SpaceDenial, Attrition) — the pressure is what makes a pack read as a
different fight rather than a bigger one:

| Modifier | Class | Pressure | What it does |
|---|---|---|---|
| WARDED | Common | Durability | A real shield worth 60% of max health, recharging after 4 s. DoTs bypass it for free, because status ticks set `bBypassShield`. |
| VOLATILE | Common | SpaceDenial | A 1.2 s death fuse, then a falloff detonation (500 cm inner / 900 cm outer). Hits pawns only. |
| FLEETFOOT | Common | Mobility | 1.55x speed, stronger weave. |
| ANCHORED | Common | Attrition | Its hits slow you 40% for 2 s. **The immobility half is unimplemented** — there is no stagger system — and that is recorded at the code rather than faked. Altered only. |
| SPLITTING | Uncommon | Attrition | Two copies at 25% health on death. Copies are rank Trash with no modifiers and no loot, so it cannot recurse. Vestige only. |
| WARDING AURA | Uncommon | Durability | Allies within 900 cm take 35% less. One shared key, so auras cannot stack. Altered only. |
| REFLECTIVE | Uncommon | Durability | Reflects 12% of damage dealt as **True Damage that cannot crit**, capped at 5% of its own max health. Skips DoT and skips True Damage, so it cannot loop. Vestige only. |
| PHASING | Rare | Mobility | Blinks toward the player every 6 s behind a 0.55 s telegraph, never closer than a standoff distance, untargetable for 0.35 s. Vestige only. |
| CASCADING | Rare | SpaceDenial | Its landed attacks leave a hazard zone; at most four live, oldest destroyed. |
| WAKEFUL | Rare | Durability | Revives once at 35% after 4 s — **unless the killing blow was a weak point**, which denies the revive and still spends it. |

Composition rules, all in the world-free header: at most **3** per enemy;
forbidden pairs (Warded+Reflective, Splitting+Volatile, Phasing+Cascading,
Wakeful+Splitting); two or more modifiers must span two pressures and three may
not carry two Durability. Health steps `+0.35x` per modifier beyond the first,
multiplying the ModifierBearing rank row rather than replacing it. `SetModifiers`
**rejects** an illegal set rather than trimming it.

Two of Encounter-Design's modifiers are deliberately absent: **Suppressing**
(its trigger was stamina, deleted by O1 — a recost cannot fix it) and
**Tethered** (needs a spawn-time pairing contract that does not exist).

Modifiers DO reach the field: the game mode grants them to the arena elite and
to wave enemies, and it restores the authored rank afterwards so an Elite is not
silently demoted to ModifierBearing's weaker row.

## The boss — THE FIELD MARSHAL (`Combat/BreakerBossEnemy`, `BreakerBossPhases`)

A `ABreakerWardenEnemy` subclass, because the boss is a Warden that **commands**.
Three phases, **health-gated and monotonic** (never timed, never reversible):

1. **Deployment** (100%-66%) — orders adds in every 20 s behind a 2.5 s
   apparatus raise.
2. **Suppression** (66%-33%) — orders permanent gallery Lattices, hard-capped
   at three, every 15 s.
3. **Commitment** (33%-0%) — no more orders: 1.4x speed, faster sweeps, slams on
   a 4 s cooldown that now leave hazard zones, frontal armour halved, and the
   rear apparatus permanently exposed.

Three tells, one per attack, and each is a shape the player already learned from
the Warden: shield draw-back (sweep), a growing ground ring (slam), and the
apparatus raise (order). The order clock **resets** rather than subtracts, so a
frame hitch fires at most one order. DoT stacks are capped at 3 on the boss
specifically.

The arena geometry is authored as offsets **relative to spawn**, because no
arena level exists — the boss is reachable from an input binding and a console
command in the gym, not from content.

## Party scaling

Solo is the balance baseline. Party policy supports up to five players with placeholder scaling for enemy health, damage, and elite frequency. Do not scale only health: five-player encounters need enemy-count and role-pressure adjustments to avoid turning enemies into passive damage sponges.

## FOR THE OWNER — open in this domain (2026-08-14)

1. **`ClassResourceRegen` has no bidder.** The attribute is aggregated and
   replicated; the Mana component ticks its own regen beside it. Either the
   loop reads the attribute (so gear regen and class regen share one bucket, as
   every other stat now does) or the attribute goes. Leaving both is how the
   `DamageMultiplier` bug happened.
2. **The two-family taxonomy reaches no enemy.** Nothing sets `Family` or
   `SeveranceStage`, so cover discipline, flinch and two modifiers are dark.
   Cheap to fix in `Game/`; it is a content decision about which spawns are
   Altered, not a code one.
3. **O22 replication is still unanswered** and it gates Damage-Pipeline
   sign-off — specifically whether the proc-coefficient law and the composed
   More ceiling are server-side enforcement points.
4. **`ProcCoefficient` has exactly one consumer** (Mana generation). The
   multishot-0 / ricochet-0.5 / depth-2 law in Damage-Pipeline §3 is unenforced
   — unbroken only because neither multishot nor ricochet exists yet.
5. ~~The composed More ceiling exists twice: 1.30³ = 2.197 computed in the
   attribute aggregator, and a literal 2.20 in the combat component's outgoing
   chain. One of them should be derived from the other.~~ **ANSWERED [O34]:**
   there is **ONE** ceiling, derived from the aggregator (1.30³ ≈ 2.197); the
   combat component's separate 2.20 literal is deleted, its product counting
   against the same budget. The code fix — `BreakerCombatComponent.h`'s
   hardcoded `2.20f` — is in flight this session. Full canon at
   `Damage-Pipeline.md` §4a.
