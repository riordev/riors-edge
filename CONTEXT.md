# Rior's Edge — agent context

This is the durable handoff document for anyone working on the project. Read it before making changes, then update it whenever architecture, milestone status, paths, or workflow constraints materially change.

## Product vision

Rior's Edge (working codename Project Breaker) is a fast first-person, movement-driven ARPG looter shooter. Movement is part of character building rather than a fixed utility layer. Weapons, affixes, legendary items, abilities, and skill nodes should interact with momentum, dash, slide, wall movement, gravity, and other grounded movement disciplines. Grapple/tether mechanics are explicitly excluded.

The immediate goal is a small vertical slice, not the full world: one graybox biome or arena, expressive movement, three weapon archetypes, three normal enemies, one elite modifier, one boss, a small affix pool, three build-defining legendary items, roughly 15 skill nodes, and save/resume.

The current character concept proposes five classes—Caster, Swift, Gunsmith, Tank, and Support—with three branches each, class-specific resources, and a separate six-constellation universal Core Tree. The implementation analysis is in `Docs/Character-Progression-Architecture.md`. Class identity, universal progression, and equipment affixes are separate layers; do not merge their data models.

Locked progression decisions: class selection is permanent per character; characters equip two class abilities and one ultimate; solo is the primary balance target with parties up to five; DoTs can crit and snapshot offensive stats at application; respecs require a Forge; the level cap is 50 with a hard stop and no post-cap power progression, so all endgame character power comes from gear; dash, slide, wall ride, block, and dodge are all base kit, with trees improving them rather than unlocking them, leaving air jump and parry as the only tree-granted verbs.

## Canonical project

- Unreal project: `riors_edge.uproject` (repository root)
- Engine: Unreal Engine 5.8
- Runtime module: `RiorsEdge`
- Primary development branch: `main`
- Original imported Unreal template backup: `/Users/rior/Desktop/riors edge/riors_edge`

The Desktop copy is a backup and must not be edited. The canonical working copy is this repository.

## Current state

- An asset-free Slate front end opens on launch with title, pause, loadout, and settings screens. Settings persist sensitivity, FOV, and vertical-look inversion; Escape backs out one menu layer at a time.
- The playtest HUD uses a compact bottom-left band for health, shield, armor, movement state, speed, weapon slot, and ammunition, followed by presentation-only placeholders for two class abilities and one ultimate.
- The project began as Unreal's Blueprint First Person template.
- Existing template content, input assets, character Blueprint, game mode, and `Lvl_FirstPerson` were preserved.
- The active global game mode uses `BreakerGameMode` and the C++ `BreakerCharacter` fallback pawn; existing template assets remain preserved for reference and presentation migration.
- A C++ module has been added and compiled successfully on macOS against Unreal 5.8.
- `ABreakerCharacter` is a GAS-enabled first-person character base with Enhanced Input bindings.
- Movement code currently includes walk, sprint, directional dash with cooldown, and a grounded speed-gated slide.
- `UBreakerCharacterMovementComponent` now owns grounded tuning, dash, deterministic sliding/slope response, and a short speed-neutral wall ride with a controlled wall jump. Wall ride presentation remains for Blueprint.
- `UBreakerInputConfig` is a Data Asset contract for Move, Look, Jump, Sprint, Dash, Slide, Fire, Aim, and Reload.
- `UBreakerAttributeSet` defines replicated health, shield, armour, stamina, class-resource, critical, damage, DoT, and movement attributes.
- The first progression framework is implemented under `Source/RiorsEdge/Progression`: stable class/node IDs, class and tree Data Assets, ranked allocation validation, two class-ability slots plus ultimate, Forge-only respec, versionable runtime state, and snapshot-ready DoT application specs. Content assets and save persistence are not created yet.
- The shared combat foundation is implemented under `Source/RiorsEdge/Combat` and documented in `Docs/Combat-Foundation.md`: replicated GAS attributes, a unified damage request/result contract, armour and penetration, shield routing, snapshot-critical DoT ticks, stamina/resource helpers, and damage/death events. It is framework code; weapons, status lifetime management, and class generation rules are not implemented yet.
- The weapon foundation under `Source/RiorsEdge/Weapons` now exposes rifle, scattergun, and marksman fallback archetypes with distinct cadence, ammunition, spread, falloff, damage, composite placeholder presentation, deterministic multi-pellet handling, server damage submission, and cosmetic events. The combat sandbox also includes reusable targets, three patrol/chase/attack enemies, incoming player damage/recovery, and runtime-spawned movement facilities. See `Docs/Weapon-Foundation.md` and `Docs/Playtest-Gym-v1.md`.
- Playtest Gym v1 adds a code-driven combat HUD, placeholder weapon visual, hit/weak-point feedback, four diagnostic recycling targets, persistent sensitivity/FOV/invert controls, reset/recovery, session statistics, performance readout, a clipboard-ready report, and title/pause/settings/loadout menus. See `Docs/Playtest-Gym-v1.md`.
- Gameplay Tags exist for initial movement, weapon, cooldown, damage, rarity, and state concepts.
- `BP_BreakerCharacter` is the active pawn assembly and remains a child of the C++ `BreakerCharacter`; the C++ class is retained as the clean-clone fallback. `DA_PlayerInputConfig` and `IMC_Player` provide keyboard/mouse and controller mappings. Blockout first-person arms and weapon geometry remain intentionally replaceable presentation.
- The canonical private GitHub remote is configured and `main` is the cross-machine integration branch.
- The item foundation is implemented under `Source/RiorsEdge/Items` and documented in `Docs/Item-Foundation.md`: item instances with item level, the ten-tier affix value curve with the T0/T-1 spike, the deterministic five-step loot roll pipeline, the vertical-slice fallback affix pool, and a replicated eight-slot equipment component that folds affixes into the attribute set. Locked aggregation rule: flat sums first, all Increased percentages form one additive bucket per stat, More multipliers are reserved for trees/Anomalous.
- Weapon slots now have a swap tempo layer (swap-in duration blocks fire/reload, swap events, seconds-since-swap-in query), which unblocks Secondary exclusive affix design.
- Block and dodge are PASSIVE defensive layers, not inputs (design decision by the owner, superseding the earlier stance/window model): dodge is a chance to evade a hit entirely (small class-resource refund), block is a chance to reduce it. `UBreakerCombatComponent` exposes DodgeChance/BlockChance/BlockMitigation for class kits and gear to raise; neither applies to DoTs.
- The SMG applies a snapshotting Bleed DoT on hit (25% per pellet); the HUD shows DODGED/BLOCKED popups, an active-status readout, and an ELITE DOWN confirm.
- Falling 40m below spawn triggers playtest reset (the template level has no kill volume).
- The class select screen has a DEV MODE checkbox (persisted in GameUserSettings) allowing class swap during playtests via `DevForceClass`; the shipping rule remains permanent selection.
- NPC/dialogue groundwork exists under `Source/RiorsEdge/Interaction`: `ABreakerNPC` carries id-linked dialogue nodes with text choices; F talks to the nearest NPC in range (HUD shows the prompt); choices can set quest flags persisted in the save. The gym spawns an Anchor camp behind the safe pad with placeholder Kess (Forge Keeper) and a Quartermaster, plus watchtowers and an elite arena ring. Vendors and quest logic hang off the same flags later.
- Owner rulings on all 17 design questions live in `Docs/Design/Decisions.md` (stamina deleted, elements are Rift/Time/Void, Frontier rename, 3-More cap, mid-campaign viability, and more). The stamina attribute no longer exists.
- Wave mode (F4) is the TTK instrument: dense escalating packs (up to 24, elites every third wave), non-respawning, per-kill time-to-kill sampled into the playtest report. Enemies chain-detonate on death (35% max health, enemies only) so packs cascade.
- The Swift Momentum resource loop is implemented in `Source/RiorsEdge/Classes/BreakerMomentumComponent`: generation from sprint/air/slide/wall-ride/dash/weak-points with a 25/s budget, safe-zone and anti-farm gates, Settled/Running/Redline states, decay bands, writing ClassResource. Active only when the permanent class is Swift. Values are C++ EditAnywhere pending the DA_MomentumPolicy data asset.
- Skill trees are LIVE at runtime: `Source/RiorsEdge/Progression/BreakerProgressionLibrary` provides C++ fallback tree content (15-node Core slice + Swift Kinetic/Marksman tiers 1-3, all values O2 placeholders), `UBreakerProgressionComponent` aggregates node effects into the attribute set (equipment-pattern base caching), grants tags for rule rewrites, exposes dodge/block bonuses (consumed by the combat component) and movement multipliers (composed multiplicatively with gear in the movement component). SKILL TREES menu screen enumerates trees, purchases nodes with failure reasons, and respecs. `ApplySliceDefaultsIfFresh` seeds 10 class/12 core points. KNOWN ISSUE: equipment and progression both cache attribute bases and write absolute values — last recalc wins; fold into one application pass.
- Ability infrastructure Phase 0 exists under `Source/RiorsEdge/Abilities/`: `UBreakerAbilityDefinition` (+ fallback Swift kit: Skim implemented, Lead/Overdrive registered unimplemented), `UBreakerGameplayAbility` base with SetByCaller cost/cooldown, `UBreakerAbilityComponent` granting from the progression loadout, E/T/G key fallbacks. Skim currently routes through TryDash (inherits dash speed floor — wrong for its design intent; needs the TryRedirect movement hook).
- Playtest-feedback QoL wave: HUD redesigned around a Destiny-style bottom-right combat cluster (weapon/ammo/ability slots with cooldowns/resource bar) with bullet tracers, outlined floating damage numbers, and overhead enemy health bars; enemies are primitive humanoids (torso/head-weakpoint/limbs) with a three-gear approach (closing sprint, weave, telegraphed lunge; elites implacable); loot drops as ground pickups with rarity beams, look-at popups, F pickup, 5-min despawn; ammo returns on kills/wave clears/camp supply crate; the field is ~2.5x larger with combat pockets, a marked sniper lane, and wall-ride walls; inventory has EQUIPMENT|SKILL TREES tabs, node cards with hover/Alt details, discards, and dev test-gear grants.
- The gym has an overgrown-Earth dressing pass (O24) in BreakerGameMode: seeded vegetation/ruins/tech props via dynamic material instances, saturated teal on exactly two suppression objects per the object-chroma law. Cosmetic only.
- A parallel design sprint produced nine docs under `Docs/Design/` (class kits, constellations, XP/pacing, encounters, game modes, UI/UX, save architecture, art plan, synthesis overview). `Design-Overview.md` §7 is the ranked owner-decision list; consult it before authoring content in any of those domains.
- `UBreakerStatusComponent` runs snapshot Bleed/Poison DoTs with stack caps; gym enemies grant rolled loot to the player's backpack on death.
- Five weapon archetypes exist (Rifle, SMG, Sniper, Shotgun, Rocket); Rocket is a replicated projectile (`ABreakerRocketProjectile`) with radial falloff damage that ignores its instigator pending the self-damage design pass. Loadout slots carry assignable archetypes (`SetSlotArchetype`), picked from the loadout screen.
- Gear multipliers are consumed by `UBreakerCharacterMovementComponent` (move/slide speed, air control via steer rate, dash cooldown) — affixes now change moment-to-moment movement.
- Save/resume exists: `UBreakerSaveGame` (slot `BreakerSave0`) stores progression state, equipped items, backpack, and weapon slot archetypes; autoloaded in character BeginPlay, autosaved in EndPlay and on class lock.
- Class selection framework: BREAKER CLASS menu screen locks one of the five classes permanently via `ChoosePermanentClassById` (Data-Asset-driven kits still to come).
- The gym encounter includes one elite (`ConfigureElite`): 1.5x scale, tripled health, doubled damage, drops never below Exceptional.
- Inventory backpack sorts best-rarity-first with per-slot filter chips and auto-height cards.

## Verification status

The `RiorsEdgeEditor` Development target compiles and links successfully on Apple Silicon and Win64 with Unreal Engine 5.8. The 19-test project automation suite passes on Windows. A live Windows startup loads `Lvl_FirstPerson`, selects `BreakerGameMode`, uses the `BP_BreakerCharacter` C++ child, opens on the title menu, and exposes the Playtest Gym HUD, targets, enemies, two-slot weapon loadout, reset, report, diagnostics, pause, settings, and loadout controls. Generated build folders are intentionally ignored by Git.

When C++ changes are made, verify with Unreal Build Tool on the relevant platform. Do not claim editor behavior has been playtested unless it actually has been tested in Play In Editor or a packaged build.

## Architecture rules

- C++ owns durable rules, network-sensitive behavior, movement/combat primitives, GAS integration, inventory algorithms, item generation, save formats, and reusable components.
- Blueprint owns asset assembly, animation, VFX, audio, camera presentation, encounters, and rapid tuning through exposed properties/events.
- Data Assets and Data Tables own weapons, affixes, loot tables, abilities, skill nodes, enemies, and balance values.
- Gameplay Ability System owns abilities, attributes, effects, costs, cooldowns, tags, and eventual replication behavior.
- Build systems vertically and keep the project playable after each milestone.
- Do not replace working template assets before the C++ replacement reaches feature parity.
- Do not hand-edit `.uasset` or `.umap` files. Create or modify them through Unreal Editor or supported Unreal automation.

## Source layout

```text
Source/RiorsEdge/
  Attributes/     GAS attribute sets
  Characters/     Player/enemy character bases
  Game/           Game modes and orchestration
  Input/          Enhanced Input data contracts
```

Add future systems in focused directories such as `Abilities`, `Combat`, `Inventory`, `Items`, `Movement`, `Save`, and `UI` only when implementing a real vertical feature.

## Content conventions

New game-owned content should live under `Content/ProjectBreaker`. Existing Unreal template assets may remain in their original folders during migration.

Use standard prefixes: `BP_` Blueprint, `WBP_` widget, `IA_` input action, `IMC_` input mapping context, `DA_` data asset, `DT_` data table, `GA_` gameplay ability, `GE_` gameplay effect, `L_` level, `M_` material, `MI_` material instance, `T_` texture, `SK_` skeletal mesh, and `SM_` static mesh.

Move and rename assets only inside Unreal Editor. Fix redirectors before merging. Coordinate ownership of maps and Blueprints because Unreal binary assets cannot be merged reliably.

## Development-machine constraints

The MacBook has limited memory. Treat it as the lightweight authoring and integration machine:

- Prefer C++ compilation, documentation, Data Asset setup, small graybox edits, and short smoke tests.
- Keep the editor scalability low and disable expensive real-time viewport features when unnecessary.
- Avoid running the editor, IDE indexing, large shader compilation, and other memory-heavy applications simultaneously.
- Do not repeatedly delete Derived Data Cache; that forces expensive shader recompilation.
- Use the Windows desktop for extended playtests, large shader builds, performance profiling, packaging, and content-heavy work.
- Both machines must use Unreal 5.8 and pull before opening the project.

## Version control

- The project has its own Git repository; do not use the accidental repository rooted at the user's home directory.
- Git LFS is installed locally and `.gitattributes` routes Unreal binary assets through LFS.
- Never commit `Binaries`, `DerivedDataCache`, `Intermediate`, or `Saved`.
- Close Unreal before switching machines. Commit and push on one machine, then pull before opening on the other.
- Never place the working repository inside iCloud, OneDrive, Dropbox, or another live-sync folder.

## Godot reference project

The user may provide a ZIP of a pre-existing Godot version. Treat it as read-only reference material. Extract it into a clearly named ignored reference directory, inventory it, and analyze:

- project version and renderer;
- input actions and physical bindings;
- player scene/component hierarchy;
- movement state machine and transition rules;
- acceleration, friction, gravity, jump, dash, slide, wall, and momentum constants;
- weapon firing, damage, recoil, ammunition, and reload logic;
- enemies, loot, UI, save data, and progression;
- animation, camera, audio, and VFX cues that communicate mechanics.

For each mechanic, record player-facing behavior, source files, important values, edge cases, dependencies, and an Unreal implementation recommendation. Distinguish behavior verified from code from behavior inferred from names or incomplete assets. Do not mechanically port Godot architecture when Unreal/GAS provides a better boundary.

The first audit is recorded in `Docs/Godot-Mechanics-Audit.md`. It confirms the old prototype's Source/Quake momentum model, but the Unreal direction is deliberately more grounded. Movement supports positioning and combat rather than dominating the gameplay loop. Grapple is excluded. Wall riding should be short, situational, and speed-neutral; air control and speed ceilings should remain restrained.

The user's future affix system is intentionally different from the Godot attachment/perk model. Do not design or implement Unreal affixes by copying those systems. Affix architecture is deferred to a dedicated design pass.

## Current milestone and next actions

Current milestone: **Movement gym**.

Recommended sequence:

1. Add `UBreakerCharacterMovementComponent` and move locomotion velocity rules out of `ABreakerCharacter`.
2. Tune grounded acceleration, braking, jump behavior, and moderate air control through combat-oriented tests rather than copying the Godot speed scale.
3. Integrate and smoke-test the implemented deterministic slide, short speed-neutral wall ride, and restrained wall jump in the character Blueprint.
4. Move dash cooldown and activation into GAS after the base movement behavior is validated.
5. Replace the current blockout first-person presentation in `BP_BreakerCharacter` with authored meshes, animation, VFX, and audio after baseline feel feedback.
6. Use the Playtest Gym report to tune the existing movement and combat baseline, then begin the combat sandbox presentation pass.
7. Wire the equipment movement multipliers (slide/air-control/dash) into `UBreakerCharacterMovementComponent`, add Block/Dodge input actions to `DA_PlayerInputConfig`, and surface backpack/equip in the loadout menu.

After movement is stable, follow `Docs/Roadmap.md` through combat sandbox, loot loop, progression, and encounter slice.

## Working documents

- `README.md` — project entry point
- `Docs/Setup.md` — Mac/Windows onboarding and editor integration
- `Docs/Architecture.md` — ownership boundaries
- `Docs/Roadmap.md` — milestone sequence
- `Docs/Godot-Mechanics-Audit.md` — confirmed prototype mechanics and Unreal mapping
- `Docs/Character-Progression-Architecture.md` — class, Core Tree, status, and GAS architecture
- `Docs/Layer-Ownership.md` — which layer owns verbs, scaling, and identity
- `Docs/Combat-Foundation.md` — damage order, armour, shields, critical DoTs, attributes, and stamina
- `Docs/Weapon-Foundation.md` — hitscan flow, prototype rifle, weak points, and target dummy
- `Docs/Vertical-Slice.md` — vertical-slice scope and definition of done
- `Docs/Item-Foundation.md` — item instances, item level, affix tiers, loot rolls, equipment, and the stat aggregation rule
- `Docs/Playtest-Feedback-Log.md` — owner playtest findings and actions taken
- `Docs/Design/` — the design corpus: Decisions.md is the append-only rulings ledger (read first); Design-Overview.md maps the space; per-domain docs cover classes (Class-Kits + Gunsmith/Tank/Support), constellations, XP/pacing, encounters, game modes, UI/UX, save architecture, art plan, damage pipeline, and the ability implementation spec

## Handoff discipline

Before ending substantial work:

1. Build or otherwise verify changes in proportion to risk.
2. Report anything that was not tested.
3. Update this file if current state, constraints, paths, or recommended next actions changed.
4. Keep unfinished work explicit; never imply that editor assets or gameplay behavior exist merely because a C++ interface compiles.
