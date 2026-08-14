# Rior's Edge — agent context

This is the durable handoff document for anyone working on the project. Read it before making changes, then update it whenever architecture, milestone status, paths, or workflow constraints materially change.

## Product vision

Rior's Edge (working codename Project Breaker) is a fast first-person, movement-driven ARPG looter shooter. Movement is part of character building rather than a fixed utility layer. Weapons, affixes, legendary items, abilities, and skill nodes should interact with momentum, dash, slide, wall movement, gravity, and other grounded movement disciplines. Grapple/tether mechanics are explicitly excluded.

The immediate goal is a small vertical slice, not the full world: one graybox biome or arena, expressive movement, three weapon archetypes, three normal enemies, one elite modifier, one boss, a small affix pool, three build-defining legendary items, roughly 15 skill nodes, and save/resume.

The current character concept proposes five classes—Caster, Swift, Gunsmith, Tank, and Support—with three branches each, class-specific resources, and a separate six-constellation universal Core Tree. The implementation analysis is in `Docs/Character-Progression-Architecture.md`. Class identity, universal progression, and equipment affixes are separate layers; do not merge their data models.

Locked progression decisions: class selection is permanent per character; characters equip two class abilities and one ultimate; solo is the primary balance target with parties up to five; DoTs can crit and snapshot offensive stats at application; respecs require a Forge; the level cap is 50 with a hard stop and no post-cap power progression, so all endgame character power comes from gear; dash, slide, wall ride, block, and dodge are all base kit, with trees improving them rather than unlocking them; TWO JUMPS are base kit for everyone and Swift innately unlocks a third later (O25, superseding the earlier air-jump-is-tree-granted line), leaving parry as the only tree-granted verb. Movement is a big part of the game but is NOT the centre of the design and gets no further dedicated passes for now (O26).

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
- `UBreakerAttributeSet` defines replicated health, shield, armour, class-resource, critical, damage, DoT, and movement attributes. (Stamina was removed by ruling O1; never re-add it.)
- The first progression framework is implemented under `Source/RiorsEdge/Progression`: stable class/node IDs, class and tree Data Assets, ranked allocation validation, two class-ability slots plus ultimate, Forge-only respec, versionable runtime state, and snapshot-ready DoT application specs. Content assets and save persistence are not created yet.
- The shared combat foundation is implemented under `Source/RiorsEdge/Combat` and documented in `Docs/Combat-Foundation.md`: replicated GAS attributes, a unified damage request/result contract, armour and penetration, shield routing, snapshot-critical DoT ticks, stamina/resource helpers, and damage/death events. It is framework code; weapons, status lifetime management, and class generation rules are not implemented yet.
- Weapons have a mechanical FEEL layer (`Source/RiorsEdge/Weapons/BreakerWeaponFeel.{h,cpp}` + state on `UBreakerWeaponComponent`): per-archetype recoil (deterministic learnable pattern with a small seeded jitter, held-trigger climb ramp, accumulation ceilings, delayed settle back toward the original aim, and player-compensation credit so counter-pulling is not undone), first-shot accuracy and bloom, ADS tightening on four axes at once, and a substepped spring viewmodel kick that `ABreakerCharacter::Tick` samples onto the placeholder mesh. Hard rule, tested: recoil moves the AIM and the trace follows the aim, so the round always goes to the crosshair; the kick is applied after the trace resolves. Every tunable is EditAnywhere; `RecoilScale` and the per-archetype `RecoilOverrides` map on the component instance retune the whole layer in-editor with no recompile. All values are O2 PLACEHOLDER and NOT playtested — automation proves the maths only. See `Docs/Weapon-Foundation.md`.
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
- Skill trees are LIVE at runtime: `Source/RiorsEdge/Progression/BreakerProgressionLibrary` provides C++ fallback tree content (15-node Core slice + Swift Kinetic/Marksman tiers 1-3, all values O2 placeholders), `UBreakerProgressionComponent` aggregates node effects into the attribute set (equipment-pattern base caching), grants tags for rule rewrites, exposes dodge/block bonuses (consumed by the combat component) and movement multipliers (composed multiplicatively with gear in the movement component). SKILL TREES menu screen enumerates trees, purchases nodes with failure reasons, and respecs. `ApplySliceDefaultsIfFresh` seeds 10 class/12 core points. RESOLVED: equipment and progression no longer cache attribute bases or write absolute values. `UBreakerAttributeSet` owns the one true base per attribute (captured once, idempotently) and both layers submit a complete `FBreakerAttributeContribution`; every submission re-derives all shared attributes as `(Base + flat) * (1 + one additive Increased bucket) * More product`. Gear and nodes stack, order is irrelevant, and unequip/respec restore exactly. See `Source/RiorsEdge/Attributes/BreakerAttributeAggregation.h` and the "Unified attribute application" section of `Docs/Item-Foundation.md`.
- DAMAGE SCALING IS LIVE (owner report: "affixes don't impact the character",
  "no matter what I do damage still feels the same"). Both halves were real
  bugs. `EBreakerNodeStatTarget` had NO damage entry, so a skill node was
  structurally incapable of raising weapon damage; and the `DamageMultiplier`
  attribute was read by every damage path (weapon hitscan/projectile, Cleave,
  the Bleed snapshot's SourcePower) and written by NOBODY, so it was pinned at
  1.0 forever. Now: `DamageMultiplier` joined `EBreakerAggregatedAttribute` and
  is THE one composed damage number; `EBreakerNodeStatTarget::Damage` exists;
  gear's `WeaponDamage` affix bids into the same additive Increased bucket
  instead of being multiplied in separately at the weapon (the private
  `GearWeaponDamageMultiplier` helper is deleted — same bug class as the
  MoveSpeed item below); and `UBreakerProgressionComponent::
  IncreasedDamagePerSpentPoint` (EditAnywhere, O2 PLACEHOLDER 1%/point) pays a
  small Increased Damage for every point COMMITTED to nodes, counted by cost,
  so spending anywhere is felt. Fallback tree content authors damage on Core
  Precision Sightline, Core Volley Cyclic (3 ranks), Swift Marksman Long Lens
  and Pierce Discipline. `FBreakerEquipmentStats::WeaponDamageMultiplier` is
  now DISPLAY ONLY. Full affix/node consumption audit table is in
  `Docs/Item-Foundation.md`; the only inert stat target left is
  `ElementalDamageReduction` (reserved for O5, and absent from the affix pool
  so it lies to nobody). Never playtested — proven by automation only.
- Heavy implementation wave (Tier 1): `FBreakerDamageRequest::Instigator` on every damage path; attacker-side `OnHitDealt`/`OnKillDealt` with `FBreakerHitContext` (DoT ticks credit their applier); push/pop outgoing damage modifiers with the composed More product clamped at the 2.20 ceiling (warning-logged, suite-safe). Swift is COMPLETE end-to-end: `TryRedirect` movement hook (Skim is a true redirect), Lead and Overdrive implemented (E/T/G all live), keystone tag-variant pattern real via `FBreakerAbilityVariant`, `UBreakerAbilityStateComponent` for windows/streaks, push/pop temporary speed multipliers on movement. Caster's Mana loop exists (`UBreakerManaComponent`): accumulating bank, weapon-hit generation at 1/n per pellet, DoT ticks generate zero, Overcast floor with doubled generation, keyed generation suspension, and cap-bypassing `GrantMana`. Overcast's +15% incoming damage is now wired: `UBreakerCombatComponent` gained a keyed incoming-damage modifier chain (`PushIncomingDamageModifier`/`Remove`/`GetComposedIncomingDamageMultiplier`) composed into `FBreakerDefenseState::IncomingDamageMultiplier`, and the Mana component pushes/removes its entry as the debt opens and clears. OVERCAST IS NOW REACHABLE (spec D8 landed): `UBreakerAttributeSet` gained the replicated `ClassResourceFloor` attribute, default **0** for every class (so Swift's Momentum is bit-identical), and `PreAttributeChange` clamps ClassResource to `[Floor, Max]` — which is what lets an ordinary GAS cost GameplayEffect drive the bank negative. `UBreakerManaComponent` owns the floor: it publishes the Overcast floor only while the permanent class is Caster, closes it on class change (bound to `OnProgressionChanged` *and* polled from `AdvanceLoop`, because `DevForceClass` does not broadcast), lifts a bank stranded below a raised floor, and exposes `SetOvercastFloor` for SB4/MS10. `UBreakerCasterAbility::CheckCost` now compares against the floor — and is the first thing to actually put the Caster affordability rule on the activation path; a cast that would breach the floor is REFUSED, never truncated. Momentum gains dodge-proc generation. Enemies ground-snap every tick (no more floating over slabs).
- Ability infrastructure Phase 0 exists under `Source/RiorsEdge/Abilities/`: `UBreakerAbilityDefinition` (fallback registry now carries both the Swift and the Caster kits), `UBreakerGameplayAbility` base with SetByCaller cost/cooldown, `UBreakerAbilityComponent` granting from the progression loadout, E/T/G key fallbacks.
- Caster is playable on E/T/G. `UBreakerCasterAbility` is the shared base holding the two class-wide rules (no cooldowns ever — Mana *is* the cooldown, so no Caster definition authors a cooldown tag; and Unmake rewrites the price of every Caster cast). Three abilities are implemented end-to-end: **Cleave** (E, 20 Mana, 3 m forward arc via the new `UBreakerMeleeSweep` — the project's first melee damage path — tagged `Damage.Melee`, always applies Bleed, GAS-native animation lock that the Edgework keystone removes), **Closequarter** (T, 35 Mana, swept blink to the target under the crosshair arriving 2 m short, velocity zeroed, 15 Mana refund at or below 40% target health), and **Unmake** (G, ultimate, 80 Mana, 6s window in which Caster casts are free and Mana generation is suspended; the Long Dark keystone's 12s-at-50% rewrite is fully parametric). The grant chain is: BREAKER CLASS screen → `ChoosePermanentClassById(Caster)` (leaves the loadout ids None, since no Caster `UBreakerClassDefinition` is authored) → `UBreakerAbilityComponent::ResolveDefinition` → `UBreakerAbilityDefinition::DefaultAbilityIdForSlot`, which now has a Caster row per slot. `UBreakerAbilityStateComponent` windows gained an optional float payload; Unmake's window carries the cost scalar. Not built: Rot, Siphon, Fracture, Resonance (all need Combat/ systems — zones, healing, projectiles, status consumption), and the Edgework-on-Closequarter and Cascade keystone halves (their variant rows exist and resolve, so the gap is visible rather than silent).
- Skim currently routes through TryDash (inherits dash speed floor — wrong for its design intent; needs the TryRedirect movement hook).
- Playtest-feedback QoL wave: HUD redesigned around a Destiny-style bottom-right combat cluster (weapon/ammo/ability slots with cooldowns/resource bar) with bullet tracers, outlined floating damage numbers, and overhead enemy health bars; enemies are primitive humanoids (torso/head-weakpoint/limbs) with a three-gear approach (closing sprint, weave, telegraphed lunge; elites implacable); loot drops as ground pickups with rarity beams, look-at popups, F pickup, 5-min despawn; ammo returns on kills/wave clears/camp supply crate; the field is ~2.5x larger with combat pockets, a marked sniper lane, and wall-ride walls; inventory has EQUIPMENT|SKILL TREES tabs, node cards with hover/Alt details, discards, and dev test-gear grants.
- THERE IS A RANGED ENEMY (owner: "an enemy that stands a certain distance
  away and shoots ranged projectiles (that i can see)"). Every enemy before it
  was a melee chaser whose three gears all ended in contact, so nothing asked
  the player to take cover, strafe, or care about distance.
  `ABreakerRangedEnemy` (Combat/, LATTICE per Encounter-Design §2.2) SUBCLASSES
  `ABreakerEnemy`, so loot, ammo return, the on-death chain, engagement-gapped
  TTK sampling, HUD health bars/state labels, and wave-mode alive counts are
  all inherited untouched. To make that possible `ABreakerEnemy::Tick` now
  delegates its engaged decision to a virtual `TickEngagedBehaviour` (the melee
  three-gear chase is the base implementation, moved verbatim), `PerformAttack`
  and `SetBodyVisible` are virtual, and a new protected `DesiredFacing` lets an
  archetype strafe sideways while facing the player. Behaviour: holds a
  900–1900cm band — advances when too far, backs off FASTER than it advances
  when crowded, strafes with a reversing cadence while firing, no contact
  attack at all. The shot is a real replicated actor `ABreakerEnemyProjectile`
  (Combat/): a ~42cm violet-magenta orb at 1100 cm/s (player sprint is 950)
  with its own point light, straight-line, no gravity, no splash, single
  target, routed through the ordinary `FBreakerDamageRequest`/`ReceiveDamage`
  contract with a proper Instigator so the player's passive dodge/block layer
  applies. Telegraph: 0.85s wind-up ramping the chest emitter's scale, colour
  and light (squared curve) plus a drop to 30% move speed — tuned for PASSIVE
  defence per O1/Encounter-Design §0 and NOT to be shortened. Lead is
  deliberately PARTIAL (`LeadFraction` 0.35): §2.2's zero lead never hits a
  moving player and reads as harmless, a full lead only loses to a direction
  change; 0.35 punishes holding a straight sprint line and loses to any change
  of direction. Set it to 0 to recover the doc's pure Lattice. All pure maths
  (band classification with hysteresis, the intercept solve, the telegraph
  ramp) lives world-free in `UBreakerRangedBehaviorLibrary`
  (Combat/BreakerRangedBehavior.h) and is covered by 5 automation tests.
  Health is deliberately the base chassis' 220, NOT §2.2's 1.6x, because trash
  and elite health are mid-re-anchor — apply the ratio when the ruling lands.
  Spawned in the gym encounter (2, wide on the flanks) and in wave mode (from
  wave 2, `Wave/2` capped at 3 per §5.3, taken OUT OF the melee budget so
  density is unchanged). EVERY value is O2 PLACEHOLDER and NOT PLAYTESTED —
  whether the orb actually reads in flight is exactly what automation cannot
  check. Knob table in `Docs/Playtest-Gym-v1.md`. Known gap: the playtest
  report does not separate ranged from melee TTK samples (Playtest/ untouched).
- ROUNDS IN FLIGHT ARE WORLD PRIMITIVES (owner, twice: "the bullet projectiles
  look a bit strange", then "projectiles are ugly and weird"). The second pass
  changed the APPROACH, not the numbers. `ABreakerTracerRenderer` (UI/) is a
  client-side, non-replicated, lazily-spawned pool actor — 12 tracers x
  (head + trail) plus 24 impact sparks, all CreateDefaultSubobject'ed once, no
  per-bullet spawn. The material is `/Engine/EngineMaterials/EmissiveMeshMaterial`
  (unlit + additive, so a round reads as light AND still depth-tests against
  the opaque scene, which is the whole point: the canvas version composited
  over walls). Its parameters were measured, not guessed: vector `Color`,
  texture `LinearColor` — the latter defaults to a WHITE GRID and must be
  overridden with WhiteSquareTexture, which `UI/BreakerGlowMaterial.h` does for
  every caller. Streak 900 -> 240 cm split into a 55 cm bright head and a
  dimmer trail; thickness is world cm with a screen-width floor; one round in
  three traces above 300 RPM and every round below it; pellet weapons get no
  streak at all (the shot contract carries one impact for a whole spread —
  fixing that properly needs per-pellet impacts in `FBreakerShotResult`); the
  six-spoke impact star is a collapsing point flash. The rocket lost its fins
  and its 540 deg/s roll and is now a dark body with an additive flickering
  flame. Every value is O2 PLACEHOLDER and NOT PLAYTESTED — automation cannot
  see the screen, which is exactly the limit that let two visual passes ship.
  See the second-pass section of `Docs/Weapon-Foundation.md`.
- The gym has an overgrown-Earth dressing pass (O24) in BreakerGameMode: seeded vegetation/ruins/tech props via dynamic material instances, saturated teal on exactly two suppression objects per the object-chroma law. Cosmetic only.
- A parallel design sprint produced nine docs under `Docs/Design/` (class kits, constellations, XP/pacing, encounters, game modes, UI/UX, save architecture, art plan, synthesis overview). `Design-Overview.md` §7 is the ranked owner-decision list; consult it before authoring content in any of those domains.
- `UBreakerStatusComponent` runs snapshot Bleed/Poison DoTs with stack caps; gym enemies grant rolled loot to the player's backpack on death.
- Five weapon archetypes exist (Rifle, SMG, Sniper, Shotgun, Rocket); Rocket is a replicated projectile (`ABreakerRocketProjectile`) with radial falloff damage that ignores its instigator pending the self-damage design pass. Loadout slots carry assignable archetypes (`SetSlotArchetype`), picked from the loadout screen.
- Gear multipliers are consumed by `UBreakerCharacterMovementComponent` (move/slide speed, air control via steer rate, dash cooldown) — affixes now change moment-to-moment movement.
- MOVEMENT WEIGHT PASS (owner: "movement should be less floaty"): the jump arc is now asymmetric and the character has landing consequence. `GravityScale` 1.35 -> 1.60, a continuous gravity curve (`ApexGravityMultiplier` 1.50 at the apex, `FallGravityMultiplier` 1.80 once falling) applied inside `NewFallVelocity`, a 2400 cm/s terminal velocity, variable jump height (release cuts the rise to 55%, driven by `JumpHoldWindow` written onto `ACharacter::JumpMaxHoldTime` at BeginPlay with the engine's hold-to-rise suppressed in `DoJump`), a landing horizontal-speed ramp in `ProcessLanded` with an `OnLandingImpact` presentation delegate, and quicker ground stops (braking 1800 -> 2400, friction 7.5 -> 8.5). No verb changed. Wall ride is exempt from the fall curve; dash clears the cut; a queued slide is exempt from the landing cost. Jump impulse, air control and air steer rate are deliberately unchanged. All values are `EditAnywhere` under the **Weight** category with old values in comments; the before/after table, the revert list and the tuning order live in `Docs/Movement-Design.md`. NOT PLAYTESTED — automation and arithmetic only.
- Save/resume exists: `UBreakerSaveGame` (slot `BreakerSave0`) stores progression state, equipped items, backpack, and weapon slot archetypes; autoloaded in character BeginPlay, autosaved in EndPlay and on class lock.
- Class selection framework: BREAKER CLASS menu screen locks one of the five classes permanently via `ChoosePermanentClassById` (Data-Asset-driven kits still to come).
- MONSTERS ARE CONTENT-SCALED (O27, `Docs/Design/Power-Curve.md` §2). Before
  this, `EnemyLevel` drove nothing but loot item level and health was the
  literal constant `SetMaxHealth(220.0f)` at every level, so the player's 3-5x
  power growth landed on an enemy identical at level 1 and level 50.
  `Source/RiorsEdge/Combat/BreakerMonsterChassis.{h,cpp}` is the curve:
  `Health(AL) = BaseHealth * (1+g)^(AL-1) * Rank * Archetype`, same shape for
  damage, with `g` 0.09 (x68 over 50 levels) and `d` 0.055 (x14 — incoming
  damage MUST scale materially slower than health or every build becomes a
  defensive build). It is pure world-free maths in the precedent of
  `BreakerRangedBehavior.h`, so it is unit-testable and readable by tools with
  no actor, and it is STRUCTURALLY incapable of reading the player — which is
  the failure mode O27 exists to forbid. Rank MULTIPLIES the chassis: elite
  x3.0/x1.5, modifier-bearing x2.5/x1.25, boss x25/x2.0, all derived from
  O18's TTK targets rather than guessed. `ConfigureElite`'s hardcoded 440
  health and `*= 1.5f` damage are folded into that table (one source of truth
  for what an elite is) and `bIsElite` is gone — rank IS the flag. The Lattice
  ranged archetype now takes Encounter-Design 2.2's 1.6x through the same
  composition. Area level is authored on the CONTENT: `AreaLevel` on the enemy
  (1-100, deliberately past the character cap of 50) and `GymAreaLevel` /
  `AreaLevelPerWave` on the game mode, both EditAnywhere, so a playtest walks
  the curve by turning a number up. `EnemyLevel` still drives loot item level
  and now follows area level (clamped 50). Every constant is O2 PLACEHOLDER.
  CAUTION: this is one half of a ratio. For an UNCHANGED player, trash TTK is
  1.81s at area level 1, 3.93s at 10 and 9.3s at 20; the other half is the
  weapon base-damage-by-item-level curve (Power-Curve §3, NOT built here). Set
  `GymAreaLevel = 1` to recover today's exact feel until that lands.
- The gym encounter includes one elite (`ConfigureElite`): 1.25x scale, the
  elite rank chassis, slower implacable advance, drops never below Exceptional.
- Inventory backpack sorts best-rarity-first with per-slot filter chips and auto-height cards.
- Weapons have a mechanical feel layer (`Weapons/BreakerWeaponFeel.h`, pure maths so it is unit-testable): per-archetype recoil with a learnable pattern and a settle that lands on exactly zero, player compensation credited against the recovery budget, ADS tightening four axes, first-shot accuracy with bloom, and a substepped viewmodel spring. The round goes where the crosshair was when fired; the kick moves the aim for the NEXT one, and that invariant is tested. Weak points carry a world-space forgiveness halo (`WeakPointToleranceCm`, 14 cm) after a geometry bug left the bottom of the head unhittable from the front. Falloff is softened per archetype with the ORDERING pinned by test rather than the values. ADS pays aim-in time and a movement spread penalty, so hip fire is the mobile close option rather than strictly worse; it has no ADS movement-speed penalty yet because `Movement/` has no aim awareness.
- `ABreakerRangedEnemy` (LATTICE) holds a 9-19 m band, strafes while firing, has no contact attack, and throws a real replicated projectile at 1100 cm/s against a 950 cm/s sprint with a 0.85 s telegraph and 0.35 lead — enough that holding a lane is punished and any direction change beats it. It SUBCLASSES `ABreakerEnemy`, so loot, waves, health bars and TTK sampling work unchanged; it declares itself through `IsRangedForTelemetry()` so its kills get their own TTK bucket instead of polluting the melee average.
- Rounds are drawn by `ABreakerTracerRenderer` (UI/), a pooled client-side world actor — 12 tracers plus 24 impact sparks allocated once and recycled, nothing spawned per bullet. Additive unlit material so it still depth-tests against the world, which the previous canvas approach could not. The shotgun deliberately draws no streak, because the shot result carries one impact for a whole spread.
- Movement carries weight without floatiness: gravity 1.38 on the rise (eased twice from playtest), a 1.80x fall multiplier, a blended apex band, terminal velocity, variable jump height, a landing speed cost, tighter braking. Wall ride was BROKEN — its entry gate equalled walk speed and is read after wall contact, so it never entered at any angle — now 450 with a regression test. Dash broadcasts `OnDashStarted` for an FOV punch scaled by speed and a direction-signed camera roll.
- Damage scaling is real and unified: `EBreakerNodeStatTarget::Damage` exists, `DamageMultiplier` is an aggregated attribute rather than a permanently-1.0 constant, and gear plus tree plus a per-spent-point baseline (`IncreasedDamagePerSpentPoint`, EditAnywhere, zeroable) all land in ONE additive Increased bucket. Before this, a skill node was structurally incapable of raising weapon damage.
- The skill matrix sizes from the measured viewport once per rebuild, scrolls in both axes, and leads each node card with what a point BUYS (`DAMAGE 1.06x -> 1.10x`) projected through a copy of the live aggregator so it cannot drift from the real numbers. A branch strip browses subclasses; committing to one does not exist in the data model.
- The FIELDPLATE UI system is implemented. `Source/RiorsEdge/UI/BreakerUIStyle.h` is the single token header (sRGB palette, rarity ramp, 4/8/16/24/40/64 spacing scale, rail and border widths, type scale, HUD geometry) and both the canvas HUD and the Slate front end read from it — a colour authored twice is a bug. `ABreakerPlaytestHUD` is rebuilt to `Docs/Design/UI-HUD-Spec.md`: one 440x184 bottom-right cluster on a 3px orange rail, notched momentum track whose block texture changes per state, 56px ability squares with ready/window/cooldown-wedge/unaffordable states, 420-wide vitals plate with fixed 84px value column and armour chips, top-centre wave banner, spec'd damage-number scale with cluster stacking, 180x8 enemy bars, and the violet ultimate frame with edge bands and step-down. All HUD geometry is authored in the spec's 1080p pixels and scaled by `ViewportHeight/1080`. Two known gaps, both content not code: the three OFL faces (Saira Condensed / Barlow / JetBrains Mono) are not imported, so the type *scale* is honoured and the *faces* are not; and no ability glyphs exist, so each square falls back to the ability's short name in its state colour.

## Verification status

The `RiorsEdgeEditor` Development target compiles and links successfully on Apple Silicon and Win64 with Unreal Engine 5.8. The 114-test project automation suite passes on Windows (run headless: `UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi`, then grep the log for `Result={Fail}`). NOTE: builds fail with "Live Coding is active" while the editor is open — close it (or Ctrl+Alt+F11 in-editor) before running Build.bat. A live Windows startup loads `Lvl_FirstPerson`, selects `BreakerGameMode`, uses the `BP_BreakerCharacter` C++ child, opens on the title menu, and exposes the Playtest Gym HUD, targets, enemies, two-slot weapon loadout, reset, report, diagnostics, pause, settings, and loadout controls. Generated build folders are intentionally ignored by Git.

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
  Abilities/      GAS ability defs/base, Skim/Lead/Overdrive, state windows
  Attributes/     GAS attribute set
  Characters/     ABreakerCharacter (input, save, interact, components)
  Classes/        Class resource loops: Momentum (Swift), Mana (Caster)
  Combat/         Damage contract/library, combat + status components, melee
                  and LATTICE ranged enemies + their projectile
  Game/           BreakerGameMode: gym spawning, waves, safe zone, dressing
  Input/          Enhanced Input data contract
  Interaction/    NPCs + dialogue
  Items/          Item types, affixes, loot rolls, equipment, ground pickups
  Movement/       Custom CharacterMovementComponent (dash/slide/wall/redirect)
  Playtest/       Instrumentation + report (TTK vs O18 targets)
  Progression/    Trees/nodes, fallback content, purchase/respec, node effects
  Save/           UBreakerSaveGame (slot BreakerSave0)
  Tests/          107 automation tests (RiorsEdge.* filter)
  UI/             SBreakerMenu (all screens) + ABreakerPlaytestHUD (canvas)
```

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

## Current milestone and next actions (updated 2026-08-13)

Current milestone: **Vertical-slice systems — Swift playable end-to-end**.
Movement gym, combat sandbox, loot loop, and the progression framework are
all live; presentation is deliberately blockout.

The whole loop works in Play In Editor today: spawn at the safe ring →
talk to camp NPCs (F) → fight the encounter/waves (F4) → loot ground drops
(F) → equip (I, tabs for EQUIPMENT | SKILL TREES) → spend points → use
Swift abilities (E Skim / T Lead / G Overdrive) or Caster abilities
(E Cleave / T Closequarter / G Unmake). Playtest keys: F1 reset,
F2 copy report (includes engagement-gapped TTK vs O18 targets), F3
diagnostics, 1/2 weapon slots. Dev tools (class swap, test gear, point
grants) gate on the DEV checkbox on the BREAKER CLASS screen.

Next actions, in priority order:

1. **RULED (O27) and half-built — the power curve.** The TTK re-anchor is no
   longer a single retune: monster health is a curve in AREA level and the
   target is stated for a baseline build in on-level content. The monster
   chassis half is BUILT (see above); Encounter-Design 2.2's 1.6x is applied
   to the Lattice. **The other half is `WeaponBase(ilvl)` (Power-Curve §3) and
   until it lands the two curves do not compose** — an unchanged player at
   `GymAreaLevel = 10` sees 3.93s trash, not 1.81s. Still true for a measuring
   run: set `WeakPointToleranceCm = 0` (the forgiveness halo adds 8-14% damage
   per hit at a 50-60% weak-point rate). The ranged/melee bucket mixing noted
   in session 5 is FIXED — the report splits melee, ranged and elite.
2. **Assets are now the binding constraint on feel, not code.** In order of
   how much they block: AUDIO (nothing exists — recoil, bloom and viewmodel
   kick are all built and land on silence); the three OFL faces (Saira
   Condensed / Barlow / JetBrains Mono — everything renders in Roboto, the
   type scale is already correct so it is a swap); weapon and character
   meshes (recoil currently kicks a grey box); muzzle flash and impact VFX
   (hooks and timings fire into nothing); the nine ability glyph SVGs (code
   stand-ins exist). All of these need the owner: downloading fonts and
   authoring `.uasset`s is editor work.
3. **Owner decisions, none blocking code:**
   - **Movement is the LAST multiplicative gear x tree violation.**
     `GearMoveSpeedMultiplier()` and friends compose percentages
     multiplicatively against the locked one-additive-bucket rule. Damage was
     the same bug class and is fixed. Conforming makes +20/+20 read x1.40 not
     x1.44 — a movement FEEL change, which is why it is a ruling. Related:
     the composed MoveSpeed ATTRIBUTE has no gameplay consumer at all, and
     SlideSpeed/AirControl/DashCooldown never reach the attribute set.
   - **Subclass commitment.** The branch strip browses; committing needs a
     branch field on the progression state or tree, a one-way setter with a
     permanence-or-Forge rule, save versioning, and a ruling on whether
     unselected branches become unpurchasable — which collides with O15.
   - **Swift's third jump** (O25) is unimplemented and needs a kit design:
     when it unlocks and whether it is free.
   - Overdrive's +25% damage window is a 4th More against the O3 budget of 3
     (flagged in `BreakerAbility_Overdrive.h`); the O22 replication position
     page, which gates Damage-Pipeline sign-off and also decides whether
     recoil should be client-predicted; the held items in Decisions.md.
4. **Content gaps that are content, not code:**
   - **Swift's FRENZY branch is unauthored.** Class-Kits 1.3-1.5 names
     Frenzy / Kinetic / Marksman; only two exist in
     `UBreakerProgressionLibrary`, so the branch strip shows two chips.
   - Caster's Rot / Siphon / Fracture / Resonance need Combat/ systems that
     do not exist (zones, partial healing, a projectile base, status
     consumption). Overdrive keystone branch stubs and the inert node tags
     (ledger in `BreakerAbilityStateComponent.h`).
   - Elements constellation has no nodes; the cluster renders sealed.
5. **Real gym map authored in-editor.** The stock First Person template
   geometry still crowds the runtime-spawned field, and the owner has now
   twice reported that the SPACE reads wrong ("walk speed feels weird but i
   think its a map scope issue"). Editor work.
6. **Known smaller gaps, each recorded at the code:** the shotgun draws no
   tracer because `FBreakerShotResult` carries one impact for a whole spread
   (per-pellet impacts are a weapon-contract change); ADS has no movement
   SPEED penalty because `Movement/` has no aim awareness, which is the
   other half of the hip/ADS trade; `DevForceClass` keeps a stale
   `ClassDefinition` after a dev swap; Slate panel-transition and
   purchase-confirm motion are unimplemented.

## Session workflow facts (read before working)

- Build: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RiorsEdgeEditor Win64 Development -Project="<repo>\riors_edge.uproject" -WaitMutex`. Fails while the editor is open (Live Coding lock).
- Tests: headless `UnrealEditor-Cmd.exe` with `-ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi`; 114 pass. NOTE: when the owner has the MAIN tree editor open, an agent building in a separate worktree hits a false-positive Live Coding lock (the guard keys off the shared UnrealEditor.exe, not the project DLL); `-NoHotReloadFromIDE` is the correct override in that case only.
- Authority chain for design questions: `Docs/Design/Decisions.md` (append-only O-ledger) supersedes everything; then Design-Overview.md; then the per-domain docs; then `Docs/Design/Master-Sheet-Import.txt`. O2 freezes value authoring — placeholders must be flagged `O2 PLACEHOLDER`.
- `Docs/Playtest-Feedback-Log.md` records every owner playtest and the responses; append per session.
- The owner works in short playtest loops: expect to build/fix while the editor is closed, relaunch it for them, and push to origin/main after green tests. All work to date is committed and pushed through addfd85.
- Zero-setup convention: all content (weapons, affixes, trees, abilities, class defs) has C++ fallback registries so a clean clone plays with no assets; Data Assets replace them later one-for-one.
- IMPORTANT UI lesson: never use SWrapBox with UseAllottedSize inside a scroll box (layout oscillation), and never poll input/rebuild widgets from per-frame Text_Lambda attributes — both caused owner-visible bugs.

## Working documents

- `README.md` — project entry point
- `Docs/Setup.md` — Mac/Windows onboarding and editor integration
- `Docs/Architecture.md` — ownership boundaries
- `Docs/Roadmap.md` — milestone sequence
- `Docs/Godot-Mechanics-Audit.md` — confirmed prototype mechanics and Unreal mapping
- `Docs/Character-Progression-Architecture.md` — class, Core Tree, status, and GAS architecture
- `Docs/Layer-Ownership.md` — which layer owns verbs, scaling, and identity
- `Docs/Combat-Foundation.md` — damage order, armour, shields, critical DoTs, attributes, and stamina
- `Docs/Weapon-Foundation.md` — hitscan flow, archetypes, weak points and the forgiveness halo, falloff, the hip/ADS trade, recoil and bloom, and round presentation
- `Docs/Movement-Design.md` — the movement verbs, the weight pass with its before/after table and revert list, and the wall-ride entry rule
- `Docs/Playtest-Gym-v1.md` — what the gym spawns, the enemy archetypes including LATTICE, and how to reach each of them
- `Docs/Vertical-Slice.md` — vertical-slice scope and definition of done
- `Docs/Item-Foundation.md` — item instances, item level, affix tiers, loot rolls, equipment, and the stat aggregation rule
- `Docs/Playtest-Feedback-Log.md` — owner playtest findings and actions taken
- `Docs/Design/` — the design corpus: Decisions.md is the append-only rulings ledger (read first); Design-Overview.md maps the space; per-domain docs cover classes (Class-Kits + Gunsmith/Tank/Support), constellations, XP/pacing, encounters, game modes, UI/UX, save architecture, art plan, damage pipeline, and the ability implementation spec. The five `UI-*.md` files are the FIELDPLATE visual authority: `UI-Style-Guide-Fieldplate.md` (palette, type scale, shape, motion), plus the HUD, Inventory, Skill Tree and Ability Icon specs, each carrying its own implementation-status section recording exactly what is built and what is not

## Handoff discipline

Before ending substantial work:

1. Build or otherwise verify changes in proportion to risk.
2. Report anything that was not tested.
3. Update this file if current state, constraints, paths, or recommended next actions changed.
4. Keep unfinished work explicit; never imply that editor assets or gameplay behavior exist merely because a C++ interface compiles.
