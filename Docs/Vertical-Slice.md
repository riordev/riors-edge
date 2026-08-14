# Vertical slice

**Last reconciled against: O32**

## Scope

The first slice proves movement and item feedback in one graybox arena: sprint,
dash, slide, mantle, and short wall movement; three weapons; three enemies, one
elite, one boss; basic GAS health/damage/cooldowns; randomized loot with
**three build-defining legendary items**; about 15 skill nodes; and save/load.
Grapple is excluded.

**Legendary is not the same thing as Anomalous, and the slice scope means
legendary.** O32 fixes the distinction the docs had been blurring:

- **Anomalous is a RARITY** — the fifth tier. It gates affix count and the tier
  ceiling, and it carries one **rolled** rule rewrite drawn from a generic pool.
- **Legendary is a separate field** (`FBreakerItemInstance::LegendaryId`) naming
  a specific authored item with a fixed slot, guaranteed affixes and a
  **hand-authored** rule.

Every legendary rolls at Anomalous rarity; most Anomalous drops are not
legendaries. "Three legendaries" in the line above therefore means three
*authored named items*, not three Anomalous drops, and the four generic
Anomalous rule rewrites do not count toward it.

## Done means

A fresh player can enter, learn movement, defeat enemies, equip randomized
loot, choose a skill, defeat the boss, quit, and resume — with a clean clone
building on both macOS and Windows.

## Where the slice actually stands

Verified against code, not against the previous version of this page, which was
written before most of the slice existed and understated it badly.

| Slice item | State |
|---|---|
| Movement verbs | **Built.** Sprint, dash, slide, wall ride + wall jump, two jumps base kit with Swift's third (O25). Mantle exists as runtime facilities in the gym. Weight pass applied and eased across four owner reports. |
| Three weapons | **Exceeded.** Five archetypes exist — Rifle, SMG, Sniper, Shotgun, Rocket — with a full recoil/bloom/ADS feel layer and a pooled tracer renderer. |
| Three enemies | **Exceeded.** Skitter (melee), LATTICE (ranged), Severed Warden, Severed Skirmisher, plus the elite and the boss. All of them now spawn in the gym. |
| One elite | **Built**, and now carrying rolled modifiers rather than being a health multiplier. |
| One boss | **Built.** THE FIELD MARSHAL — F5, `Breaker.Boss`, wave 12, or `-BreakerBossOnStart`. |
| GAS health/damage/cooldowns | **Built**, including armour, shields, snapshot DoTs, passive dodge/block, and one unified attribute-aggregation rule. |
| Randomized loot | **Built**, and now paced: a drop-chance step by rank and rarity gates by item level precede the weighted roll. |
| **Three legendaries** | **Built but UNREACHABLE THROUGH UI.** They occupy Boots, Primary and Waist (O32). Blueprint/console/automation only; the crafting wallet is not in `UBreakerSaveGame`. |
| ~15 skill nodes | **Built and exceeded** — a 15-node Core slice plus Swift's Kinetic/Marksman/Frenzy branches and the Elements constellation. |
| Save/load | **Built**, at save version 2 with one-step migration, and quest flags now write through on change rather than only at a clean shutdown. |

**What "done" is still missing**, in the order it blocks the definition above:

1. **Nothing teaches anything.** "A fresh player can enter, learn movement" has
   no tutorial, no onboarding and no first-run path. The gym is an instrument.
2. **The legendaries cannot be equipped by a player**, so "equip randomized
   loot" is only true of ordinary drops.
3. **Three of the five selectable classes grant nothing** — Gunsmith, Tank and
   Support exist as design documents only, and the Caster branch trees are
   unauthored. Class selection is permanent, so a player can permanently
   choose an empty class.
4. **Assets.** No audio at all, no weapon or character meshes, no muzzle flash
   or impact VFX, no ability glyphs, and the three OFL faces are not imported.
   Every one of those hooks is built and fires into nothing.
5. **The arena is the Unreal template courtyard plus a procedural field.** See
   `Docs/Design/Level-Design.md` §8 for the editor work that remains.

**Not playtested** is the standing caveat on every "Built" above except where
the feedback log records an owner session. Automation proves arithmetic; the
capture harness proves composition; neither is play.
