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

## Party scaling

Solo is the balance baseline. Party policy supports up to five players with placeholder scaling for enemy health, damage, and elite frequency. Do not scale only health: five-player encounters need enemy-count and role-pressure adjustments to avoid turning enemies into passive damage sponges.
