# Production roadmap

## Milestone 1 — Movement gym

Status: playable baseline complete on Windows. Walk, toggle sprint, four-second directional dash, slide/landing-slide, slide jump, double jump, mantle, air control, wall ride/jump, reset/report/diagnostics, and dedicated gym facilities are implemented. Movement polish remains iterative.

Deliver a playable C++-backed character in the existing First Person level with walk, look, jump, sprint, directional dash, slide, and a short wall ride. Tune for grounded physical weight and combat readability rather than maximum traversal speed. Grapple is out of scope.

Exit criteria: all actions work on keyboard/mouse and controller; movement values are tunable in a Blueprint child; dash cooldown and slide eligibility are readable; wall riding provides a situational route without generating runaway speed; aiming remains dependable during ordinary combat; camera/VFX events do not own gameplay rules.

## Milestone 2 — Combat sandbox

Status: playable baseline complete on Windows. Rifle, scattergun, and marksman archetypes exist; the player carries two persistent-ammo weapon slots; hitscan damage, reload, weak points, falloff, health/shield/armor, death/recovery, reusable targets, three basic enemies, combat HUD, and title/pause/settings/loadout menus are implemented. Authored weapon/enemy presentation and deeper AI remain.

Add one hitscan weapon, ammunition/reload, a GAS damage effect, health/death, and a reusable target dummy. Establish server-authoritative interfaces even while testing locally.

## Milestone 3 — Loot loop

Status: not started beyond the three fallback weapon definitions and two-slot loadout presentation. Item instances, rarity, affixes, pickups, inventory, and equipping from the armory remain.

Create data-driven item definitions, rarity tiers, affix rolls, pickups, inventory, and equipment. Prove the loop with three weapon archetypes and one movement-affecting affix per archetype.

## Milestone 4 — Progression

Status: framework complete; playable content not started. Class/tree definitions, allocation validation, Forge-gated respec, versionable runtime state, two class-ability slots, and one ultimate slot exist in code. Skill content, ability implementations, UI interaction, and persistence remain.

Add roughly 15 skill nodes across two movement disciplines, one respec path, versioned save data, and three build-defining legendary items.

The shipping level cap is 50, but the slice uses a compressed curve capped at level 10 because roughly 15 nodes cannot absorb a full point budget. The cap and curve live in a Data Asset, so the override is a content change rather than a code change. Do not read slice point totals as a balance signal.

## Milestone 5 — Encounter slice

Status: early prototype. Three patrol/chase/attack enemies and death/retry recovery are present in the movement gym. Elite modifiers, boss behavior, reward drops, authored encounter layout, save/resume, and packaged clean-clone validation remain.

Build one graybox biome with three normal enemies, one elite modifier, one boss, reward drops, death/retry, and save/resume. Package and validate from clean clones on macOS and Windows.

## Working rule

Every milestone must remain playable. C++ owns rules; Blueprint owns presentation and assembly; Data Assets own content and balance.
