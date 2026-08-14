# Architecture

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

C++ owns framework contracts, network-sensitive rules, GAS integration, movement/combat primitives, inventory algorithms, and save formats. Blueprint owns asset assembly, animation, VFX/audio hooks, encounter scripting, and tuning. Data Assets and Data Tables own weapons, affixes, loot tables, abilities, skill nodes, enemies, and balance values.

The initial module contains a GAS-enabled character, starter attributes, an Enhanced Input data contract, and a game-mode base. The existing Blueprint First Person template remains active until its presentation is migrated onto the C++ character.

Add systems vertically—one playable loop at a time—rather than filling speculative folders.

## The pure-maths layer, and why it keeps being used

A pattern has hardened into a rule through repeated use, and it is worth stating
once. Where a system's rules are arithmetic rather than world interaction, the
arithmetic is extracted into a **world-free header** with no `AActor`, no
`UWorld` and no engine subsystem dependency, and the actor becomes a thin caller
of it. The precedent chain is
`Combat/BreakerRangedBehavior.h` → `Combat/BreakerMonsterChassis.h` →
`Weapons/BreakerWeaponMath.h` → `Game/BreakerWaveBudget.h` →
`Items/BreakerDropTable.h` → `Abilities/`'s selection validation.

The payoff is not tidiness. It is that automation can drive those rules directly
with no world, no ASC and no pawn, which is the only reason the suite covers
things like the wave budget's density caps or the drop pipeline's rarity gates
at all. The corresponding limit is equally load-bearing: **a pure-maths test
proves the rule, never the wiring.** `RiorsEdge.Movement.JumpGrant` passed for
the entire life of a feature that was unreachable in the shipped configuration.
Where a rule has a shipped configuration, assert that configuration too, against
the default-constructed state the game actually runs in.

## What automation cannot do, and what answers it

Automation proves arithmetic and cannot see a layout, so the project carries a
screenshot harness (`-BreakerAutoPlay`, `-BreakerScreenshots=N` and the capture
switches listed in `CONTEXT.md`). Reading a capture is a real verification step
and is expected of any agent doing visual work. It is still not a playtest:
a screenshot shows composition and cannot say whether an arc, a cadence or a
window feels right.
