# Architecture

C++ owns framework contracts, network-sensitive rules, GAS integration, movement/combat primitives, inventory algorithms, and save formats. Blueprint owns asset assembly, animation, VFX/audio hooks, encounter scripting, and tuning. Data Assets and Data Tables own weapons, affixes, loot tables, abilities, skill nodes, enemies, and balance values.

The initial module contains a GAS-enabled character, starter attributes, an Enhanced Input data contract, and a game-mode base. The existing Blueprint First Person template remains active until its presentation is migrated onto the C++ character.

Add systems vertically—one playable loop at a time—rather than filling speculative folders.
