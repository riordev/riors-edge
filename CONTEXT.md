# Rior's Edge — agent context

This is the durable handoff document for anyone working on the project. Read it before making changes, then update it whenever architecture, milestone status, paths, or workflow constraints materially change.

## Product vision

Rior's Edge (working codename Project Breaker) is a fast first-person, movement-driven ARPG looter shooter. Movement is part of character building rather than a fixed utility layer. Weapons, affixes, legendary items, abilities, and skill nodes should interact with momentum, dash, slide, grapple/tether, gravity, and other movement disciplines.

The immediate goal is a small vertical slice, not the full world: one graybox biome or arena, expressive movement, three weapon archetypes, three normal enemies, one elite modifier, one boss, a small affix pool, three build-defining legendary items, roughly 15 skill nodes, and save/resume.

The current character concept proposes five classes—Caster, Swift, Gunsmith, Tank, and Support—with three branches each, class-specific resources, and a separate six-constellation universal Core Tree. The implementation analysis is in `Docs/Character-Progression-Architecture.md`. Class identity, universal progression, and equipment affixes are separate layers; do not merge their data models.

Locked progression decisions: class selection is permanent per character; characters equip two class abilities and one ultimate; solo is the primary balance target with parties up to five; DoTs can crit and snapshot offensive stats at application; respecs require a Forge.

## Canonical project

- Unreal project: `UnrealProject/riors_edge.uproject`
- Engine: Unreal Engine 5.8
- Runtime module: `RiorsEdge`
- Primary development branch: `main`
- Original imported Unreal template backup: `/Users/rior/Desktop/riors edge/riors_edge`

The Desktop copy is a backup and must not be edited. The canonical working copy is this repository.

## Current state

- The project began as Unreal's Blueprint First Person template.
- Existing template content, input assets, character Blueprint, game mode, and `Lvl_FirstPerson` were preserved.
- The active global game mode still points to `BP_FirstPersonGameMode`; the existing Blueprint player remains the playable baseline.
- A C++ module has been added and compiled successfully on macOS against Unreal 5.8.
- `ABreakerCharacter` is a GAS-enabled first-person character base with Enhanced Input bindings.
- Movement code currently includes walk, sprint, directional dash with cooldown, and a grounded speed-gated slide.
- `UBreakerCharacterMovementComponent` now owns grounded tuning, dash, deterministic sliding/slope response, and a short speed-neutral wall ride with a controlled wall jump. Wall ride presentation remains for Blueprint.
- `UBreakerInputConfig` is a Data Asset contract for Move, Look, Jump, Sprint, Dash, Slide, Fire, Aim, and Reload.
- `UBreakerAttributeSet` currently defines Health, MaxHealth, and MoveSpeed.
- The first progression framework is implemented under `Source/RiorsEdge/Progression`: stable class/node IDs, class and tree Data Assets, ranked allocation validation, two class-ability slots plus ultimate, Forge-only respec, versionable runtime state, and snapshot-ready DoT application specs. Content assets and save persistence are not created yet.
- The shared combat foundation is implemented under `Source/RiorsEdge/Combat` and documented in `Docs/Combat-Foundation.md`: replicated GAS attributes, a unified damage request/result contract, armour and penetration, shield routing, snapshot-critical DoT ticks, stamina/resource helpers, and damage/death events. It is framework code; weapons, status lifetime management, and class generation rules are not implemented yet.
- Gameplay Tags exist for initial movement, weapon, cooldown, damage, rarity, and state concepts.
- The C++ character is not yet wired to an editor-created Blueprint or set as the default pawn.
- No baseline Git commit or remote has been created yet.

## Verification status

The `RiorsEdgeEditor` Development target compiled and linked successfully on Apple Silicon with Unreal Engine 5.8. A focused headless automation run loaded `Lvl_FirstPerson`, entered Play-In-Editor for approximately five seconds with the C++ `BreakerGameMode` configured as the global default, and exited successfully. This verifies project/module/map/pawn startup but does not substitute for a human movement-feel test. Generated build folders are intentionally ignored by Git.

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
- acceleration, friction, gravity, jump, dash, slide, wall, grapple, and momentum constants;
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
5. In Unreal Editor, create the input Data Asset and Blueprint child of `BreakerCharacter`, then migrate first-person presentation.
6. Build and measure a lightweight traversal gym, then begin the combat sandbox.

After movement is stable, follow `Docs/Roadmap.md` through combat sandbox, loot loop, progression, and encounter slice.

## Working documents

- `README.md` — project entry point
- `Docs/Setup.md` — Mac/Windows onboarding and editor integration
- `Docs/Architecture.md` — ownership boundaries
- `Docs/Roadmap.md` — milestone sequence
- `Docs/Godot-Mechanics-Audit.md` — confirmed prototype mechanics and Unreal mapping
- `Docs/Character-Progression-Architecture.md` — class, Core Tree, status, and GAS architecture
- `Docs/Combat-Foundation.md` — damage order, armour, shields, critical DoTs, attributes, and stamina
- `Docs/Vertical-Slice.md` — vertical-slice scope and definition of done

## Handoff discipline

Before ending substantial work:

1. Build or otherwise verify changes in proportion to risk.
2. Report anything that was not tested.
3. Update this file if current state, constraints, paths, or recommended next actions changed.
4. Keep unfinished work explicit; never imply that editor assets or gameplay behavior exist merely because a C++ interface compiles.
