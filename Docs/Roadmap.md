# Production roadmap

## Milestone 1 — Movement gym

Deliver a playable C++-backed character in the existing First Person level with walk, look, jump, sprint, directional dash, slide, and a short wall ride. Tune for grounded physical weight and combat readability rather than maximum traversal speed. Grapple is out of scope.

Exit criteria: all actions work on keyboard/mouse and controller; movement values are tunable in a Blueprint child; dash cooldown and slide eligibility are readable; wall riding provides a situational route without generating runaway speed; aiming remains dependable during ordinary combat; camera/VFX events do not own gameplay rules.

## Milestone 2 — Combat sandbox

Add one hitscan weapon, ammunition/reload, a GAS damage effect, health/death, and a reusable target dummy. Establish server-authoritative interfaces even while testing locally.

## Milestone 3 — Loot loop

Create data-driven item definitions, rarity tiers, affix rolls, pickups, inventory, and equipment. Prove the loop with three weapon archetypes and one movement-affecting affix per archetype.

## Milestone 4 — Progression

Add roughly 15 skill nodes across two movement disciplines, one respec path, versioned save data, and three build-defining legendary items.

## Milestone 5 — Encounter slice

Build one graybox biome with three normal enemies, one elite modifier, one boss, reward drops, death/retry, and save/resume. Package and validate from clean clones on macOS and Windows.

## Working rule

Every milestone must remain playable. C++ owns rules; Blueprint owns presentation and assembly; Data Assets own content and balance.
