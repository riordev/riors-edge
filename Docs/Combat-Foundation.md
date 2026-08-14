# Combat foundation

## Resolution order

All weapons, abilities, statuses, hazards, and enemies should submit the same damage request contract:

1. Base damage and source scaling.
2. Weak-point multiplier when applicable.
3. Critical roll or previously snapshotted critical result.
4. Armour mitigation and penetration.
5. Shield routing unless explicitly bypassed.
6. Remaining damage to health.
7. Shield-break, damage, and death events.

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

The GAS attribute set currently contains Health, MaxHealth, Shield, MaxShield, Armor, Stamina, MaxStamina, ClassResource, MaxClassResource, CriticalChance, CriticalMultiplier, DamageMultiplier, DamageOverTimeMultiplier, and MoveSpeed.

Mana, Momentum, Scrap, Grit, and Charge all use the generic class-resource attributes. Their generation/decay rules belong to class mechanics, not the shared attribute set.

## Stamina

The combat component provides an authoritative prototype stamina spend and regeneration path: 100 maximum, 20 per second after 1.2 seconds without spending. Block and dodge will call the same spend contract. This should graduate to Gameplay Effects when those abilities are implemented so prediction and costs are handled natively by GAS.

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

## Projectiles (`Combat/BreakerProjectileBase.{h,cpp}`)

One reusable replicated projectile: travel, collision, lifetime, impact
resolution through the ordinary damage contract with a proper Instigator, an
optional list of carried statuses (applied crediting the SHOOTER, not the
projectile), and presentation hooks. `ABreakerEnemyProjectile` is re-expressed
on it with every one of its shipped numbers preserved.
`ABreakerRocketProjectile` is NOT re-expressed: it lives in `Weapons/` and its
explosion and detonation-flash lifetime would make the fold a behaviour change
disguised as a refactor.

## Party scaling

Solo is the balance baseline. Party policy supports up to five players with placeholder scaling for enemy health, damage, and elite frequency. Do not scale only health: five-player encounters need enemy-count and role-pressure adjustments to avoid turning enemies into passive damage sponges.
