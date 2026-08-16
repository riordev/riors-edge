# HANDOFF — Rior's Edge

**Written 2026-08-16 from six domain audits (progression, frontend, world, combat, classes, docs/rulings). Last reconciled against: O40.**
This document exists to make a fresh agent effective in ten minutes. It does not flatter the work.

---

## 1. STATE, IN FIVE LINES

1. Rior's Edge (codename Project Breaker) is a first-person, movement-driven ARPG looter shooter in Unreal 5.8, C++ module `RiorsEdge`, targeting a vertical slice — one arena, expressive movement, a small affix pool, ~15 meaningful nodes, save/resume.
2. Current milestone: **"the game has a front door and a home"** — a title screen (`Lvl_FrontEnd`) → character select (5 GUID-keyed slots) → the Anchor hub (`Lvl_Anchor`) → travel gate → the gym (`Lvl_Gym`).
3. What actually runs today, end to end, is the **gym loop in Play In Editor**: spawn, talk to camp NPCs (F), fight (F4 waves / standing encounter), loot (F), equip (I), spend points, use Swift or Caster abilities, F1/F2/F3 playtest keys.
4. What has **almost certainly never been run end to end** is the three-map flow itself — PIE still boots `Lvl_FirstPerson`, and the three new maps are empty shells with no PlayerStart and no lighting (§6 D1, D2).
5. A very large amount landed in the last 24 hours across parallel lanes. Most of it compiles, much of it is tested, and a substantial fraction of it is **unreachable by a player** — that is what §5 is for, and it is the section to read first.

---

## 2. THE AUTHORITY CHAIN AND THE STANDING RULES

### The chain (O28, `Docs/Design/Decisions.md:121`)

```
Docs/Design/Decisions.md   (the O-ledger, O1..O40)   — supreme
Docs/Design/Design-Overview.md                       — a map, not law
Docs/Design/<per-domain>.md                          — detail
Docs/Design/Master-Sheet-Import.txt                  — SUPERSEDED; never cite as authority
CONTEXT.md                                           — the operative handoff / plan of record
Docs/Roadmap.md, Design-Overview §6 Tier 0-6         — RETIRED TO HISTORICAL by O28
```

If you open `Roadmap.md` first you will follow a plan this project abandoned.

### Non-negotiable rules

**R1 — `Docs/Design/Decisions.md` is OWNER-ONLY and APPEND-ONLY. Do not edit it. Ever. Including to correct a fact you can prove is wrong.**
O25 (`Decisions.md:89`) still records Swift's third jump as unimplemented; it is built. The correct action is to *report* the discrepancy in your final message, not to fix the file. Do not reorder it, do not mark items RESOLVED, do not tidy its whitespace.
Sub-note: O33–O40 (`Decisions.md:224-231`) were *drafted by an agent* from a delegated audit. They are law and supersedable as usual, but they do not carry the same directness as O1–O32.

**R2 — O2 freezes value authoring, and it binds agents specifically.** You may not invent balance numbers. Any new constant must be flagged `O2 PLACEHOLDER` at its declaration. When two implementations are possible, **prefer the one that authors nothing** — this is the project's standard escape hatch (see `Class-Kits.md:771`, where a keystone reachability problem was fixed by tag *adoption* rather than by re-siting, precisely because re-siting would have required authoring a condition and a magnitude).
The corollary that gets forgotten: **a placeholder must be PERCEPTIBLE.** A placeholder the player cannot feel is not a placeholder, it is dead content, and it violates R3.

**R3 — O40(c): reachability is part of definition-of-done.** A feature merges *with* its in-game path and a shipped-configuration test in the `JumpGrantMatrix` mold — a test that asserts the **shipped configuration**, not the rule in the abstract. `RiorsEdge.Movement.JumpGrant` passed for the entire life of a feature that was unreachable, because it proved the rule against a level the game cannot produce. There is exactly one signed exception to O40(c) on record (`Hook-And-Condition-Vocabulary.md:704-711`, the S4 vocabulary widening ahead of consumers). That is a signed exception, not a licence — and §5 shows what it cost.

**R4 — Banned Slate patterns.** Each has a shipped owner-visible bug behind it.
- Never `SWrapBox` with `UseAllottedSize` inside a scroll box — layout oscillation. See `UI/BreakerMenu.cpp:1022`, `:3112`, `:3186`.
- Never a per-frame `Text_Lambda` / attribute that polls input or rebuilds widgets. Readouts in `BreakerMenu.cpp` are `TSharedPtr<STextBlock>` handles written imperatively from `OnValueChanged` (`:1546-1566`). Adding an attribute reintroduces the jitter class.
- Never `AutoWrapText` where the width matters — it measures its own arrangement. Use `MenuWrappedText` (`BreakerMenu.cpp:94`) with a pixel width computed **before** layout. Two legal survivors: `:4286` (class pitch) and `:7613` (travel description), both authored prose inside fixed-height plates.
- Never size a fixed `SBox` to its shortest label. This is the recurring "text is cut off / overdraws its neighbour" class; see §6 D6 for the five live instances.
- Corollary: on any text that could clip, use `HAlign_Fill` + `Justification`, never `HAlign_Left/Right/Center`. A non-Fill child is arranged at its *measured* width and then clipped to that same box, with measure and rasterise rounding independently.

**R5 — the cycle is BUILD → SUITE → COMMIT → PUSH.** In that order, no shortcuts. "Clean" means **zero** `Result={Fail}`. The two former deliberate reds were resolved by O36 into pinned fixtures, so any failure is now a regression without exception.

**R6 — NEVER build while the editor is open.** Live Coding holds the lock and the build fails. Close the editor or Ctrl+Alt+F11 in-editor first.
One documented exception: when the **owner** has the main tree's editor open and you are building in a separate worktree, the lock is a false positive (the guard keys off the shared `UnrealEditor.exe`, not the project DLL). `-NoHotReloadFromIDE` is the correct override **in that case only**.

**R7 — a build "completing" is not a build succeeding.** Piping build output through `tail` swallows the exit code. Check for `Result: Succeeded` or the exit code directly.

**R8 — do not hand-edit `.uasset` or `.umap`.** Create and modify them through Unreal Editor or supported automation.

**R9 — anonymous-namespace helpers in a `.cpp` must carry a `Breaker<Subject>` prefix.** This is a unity-build rule the project has broken twice (`Items/BreakerDropTable.cpp:7-9`, `Items/BreakerLootLibrary.cpp:14-16`, `Combat/BreakerAlteredEnemy.cpp:13`, `Game/BreakerHubBuilder.cpp:39-50`). Do not add a bare `MakeMaterial()`.

---

## 3. HOW TO RUN IT

**Build** (editor closed):
```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
  RiorsEdgeEditor Win64 Development ^
  -Project="C:\Users\rior\Documents\GitHub\riors-edge\riors_edge.uproject" -WaitMutex
```

**Headless suite:**
```
UnrealEditor-Cmd.exe "C:\Users\rior\Documents\GitHub\riors-edge\riors_edge.uproject" ^
  -ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi
```
Results **do not reach stdout**. Grep the log file:
```
Saved/Logs/riors_edge.log      →  grep for  Result={Fail}
```

**Baseline suite count: 314 passing, 0 failing** (measured at commit 1f6a2ba: the 312 measured at 70b8ea4 plus `RiorsEdge.Game.BootFlow.ShippedConfiguration` and `RiorsEdge.Progression.LevelPointEntitlement`). *(An earlier draft of this document carried 252 with an uncertainty note, taken from a stale line in `CONTEXT.md`. Re-run the suite on your first build and treat what you measure as the baseline; a count BELOW 314 means tests went missing, which is itself the regression.)*

**Standalone game:**
```
UnrealEditor-Cmd.exe "<repo>\riors_edge.uproject" -game -windowed -ResX=1920 -ResY=1080
```
This boots `GameDefaultMap` = `/Game/Breaker/Maps/Lvl_FrontEnd`, which has **no PlayerStart** (§6 D2). Expect it to be broken until that is fixed.

**Capture harness** — eight switches, all verified against their parse sites. Frames land in `Saved/Screenshots/breaker_NN.png`; the process exits ~2.5 s after the last. Capture runs on a **CORE ticker, not a world timer**, because opening a menu pauses the world.

| Switch | Form | Effect |
|---|---|---|
| `-BreakerAutoPlay` | flag | Skips the title menu into the gym. **Required for `-BreakerCaptureMenu`**, which is parsed inside its branch. |
| `-BreakerScreenshots=N` | int 1–60 | N frames then exit. First at 6.0 s, then every 2.0 s. |
| `-BreakerCaptureMenu=<SCREEN>` | string | `INVENTORY`, `SKILLTREES`/`SKILLS`, `LOADOUT`, `SETTINGS`, `CLASS`/`CLASSSELECT`, `PAUSE`, `CHARACTERSELECT`, `CHARACTERCREATE`. Anything else **silently** falls back to the main screen. |
| `-BreakerCaptureBoard=<BOARD>` | string, NOT a bare flag | `CORE`, `COMPARE`, `BRANCH<n>`. Also the undocumented back-door to FORGE/ABILITIES: `-BreakerCaptureMenu=INVENTORY -BreakerCaptureBoard=FORGE` (`BreakerMenu.cpp:886-887`). |
| `-BreakerCaptureTour` | flag | Eight authored field vantages instead of the player's eyes. |
| `-BreakerCaptureHUD` | flag | Fabricates the HUD **events** a headless run cannot reach. |
| `-BreakerCycleWeapons=<seconds>` | float > 0 | Walks the viewmodel through the archetypes. |
| `-BreakerBossOnStart` | flag | Spawns the Field Marshal during the gym build. |

Typical reference run:
```
UnrealEditor-Cmd.exe "<repo>\riors_edge.uproject" -game -BreakerAutoPlay ^
  -BreakerScreenshots=16 -BreakerCycleWeapons=2 -windowed -ResX=1920 -ResY=1080
```

**USE IT.** Automation proves arithmetic and cannot see a layout. Every agent doing visual work is expected to read its own screenshots. Looking has previously found: arms rendering entirely offscreen, the template level being a sealed 40 m courtyard with the field stranded outside it, the HUD reading `MOMENTUMSETTLED`, a keystone marker drawing as a black hole, the class screen dimming all five names to unreadable.
`CONTEXT.md:101` lists a shorter screen set than the code supports — the table above is the code.

---

## 4. WHAT LANDED IN THE LAST 24 HOURS — and the point of each

**A front door.** Three maps (`Content/Breaker/Maps/Lvl_FrontEnd|Lvl_Anchor|Lvl_Gym.umap`), map-role dispatch in `Game/BreakerGameMode.cpp:124-236`, a runtime hub builder (`Game/BreakerHubBuilder.cpp`), and a two-way travel registry (`Interaction/BreakerTravelPoint.cpp:60-102`). *Point:* the game stops opening straight into a debug gym, and the Anchor becomes a place rather than a camp inside the arena. *Status:* built, wired, and defective on arrival (§6 D1).

**Characters are real.** GUID-keyed save per character, max 5 slots, two-step delete, and the pre-roster single save is **adopted** rather than orphaned (`Save/BreakerCharacterRoster.cpp`, `Characters/BreakerCharacter.cpp:276-300`). *Point:* the front end has something to select.

**XP exists.** A pure curve, XP on every kill, levels **re-derived from a stored cumulative total** so a retuned curve moves existing characters instead of stranding them (`Progression/BreakerExperience.cpp`, awarded at `Combat/BreakerEnemy.cpp:756`, shown at `UI/BreakerPlaytestHUD.cpp:1442-1490`). Measured: first level in 20 trash kills, ~35 h to cap at area 25. *Point:* O40(b)'s precondition ("no CharacterLevel gate until an XP loop exists") is now **met**. O40(b) is **not self-executing** — gates re-open by ruling.
*Caveat that changes what this means:* **nothing awards points on level-up** (§5 R1).

**Class content.** Caster gained three branch trees (27 nodes); Swift gained tier 4 (9 nodes). Both classes are now structurally complete per Class-Kits. *Point:* the tree is no longer a stub. *Status:* almost all of it is tags with no effect (§5 R2).

**The S4 vocabulary widening.** `EBreakerBuildCondition` 6→24 entries, `EBreakerNodeStatTarget` 10→31, five new aggregation lanes, spec at `Docs/Design/Hook-And-Condition-Vocabulary.md` (716 lines). *Point:* the enums were the named bottleneck for two thirds of authored tree content. *Status:* the enums widened; **not one node was re-authored against a new entry** (§5 R2).

**The settings screen** (`UI/BreakerMenu.cpp:2224` and `Settings/BreakerGameSettings.{h,cpp}`) — input, keybinds (19 actions, conflict arm/confirm), video, audio, with ini persistence that migrates the old `[RiorsEdge.Playtest]` keys verbatim. *Point:* the settings model gets its first consumer. *Status:* video is the **only** group that reaches the engine (§5 R4).

**The inventory rebuild** against the owner's designer reference (`Docs/Design/UI-Reference/Inventory.dc.html`) — arithmetic column solve, readability-driven card count, measured filter chips, hover limit-tell, bulk discard modal (`UI/BreakerMenu.cpp:2795-3800`, `Tests/BreakerInventoryLayoutTests.cpp`). *Point:* kill the layout-jitter class by never sizing anything from its own allotted size.

**Equipping a weapon now arms that gun.** `Weapons/BreakerWeaponComponent.cpp:930 SyncArchetypesToEquipment`, bound at `Characters/BreakerCharacter.cpp:206`. *Point:* loot finally changes what you are holding.

**Combat pass.** Enemy labels decluttered; damage numbers fed by every player damage source (`UI/BreakerPlaytestHUD.cpp:1859`); Warden turn-rate capped (`Combat/BreakerWardenEnemy.cpp:139-160`); the Severed Drudge added (`Combat/BreakerAlteredEnemy.cpp`, spawned at `Game/BreakerGameMode.cpp:1948`); crafting currency drops from kills; cross-class ability equip closed.

**Three unbuilt-class resource loops** — Scrap, Grit, Charge components (`Classes/`), plus 21 catalogued ability rows for Gunsmith/Tank/Support and `Docs/Design/Class-Kits-Unbuilt.md`. *Point:* record the design against code so O39 can lift cleanly later. *Status:* the components are attached to nothing (§5 R3).

**Cover field registry** (`Game/BreakerCoverRegistry.{h,cpp}`) — 9 deterministic passes, world-free and testable, with `IsLayoutLegal` naming the broken rule. *Point:* arena cover stops being ad-hoc and becomes measurable.

---

## 5. THE DEAD CONTENT REGISTER

**Read this before you build anything.** Everything below is authored, compiles, is covered by green tests, and cannot be reached or felt by a player. Ordered by how much it blocks.

### R1 — Levelling grants nothing. *(blocks the entire tree economy)*
`Progression/BreakerProgressionComponent.cpp:436-444` — `AwardExperience` → `RefreshLevelFromXp` → `OnLevelGained.Broadcast`. **No point award exists anywhere on the level-up path.** The only writers of `UnspentClassPoints`/`UnspentCorePoints` are `RespecAtForge:295-296` (refund), `GrantPlaytestPoints:465` (dev), and the one-time `ApplySliceDefaultsIfFresh:491`. `XP-And-Pacing.md` §4 specifies 1 Class Point/level to 30 and 1 Core Point/level to 50; none of it is implemented.
**The XP loop that O40(b) was waiting for produces a level number, a HUD flash, and nothing spendable.**

### R2 — 53 of 97 authored tree nodes have no stat effect; 49 of those are fully silent. *(blocks the point of the tree)*
`Progression/BreakerProgressionLibrary.cpp`. The count is **unchanged by the S4 widening** — `BreakerProgressionTypes.h:61-62` cites "53 of 97" as the *justification* for adding 21 enum entries, and 53 of 97 is still the shipped number. Grep of the library returns only the original ten stat targets. **Zero uses of the 21 new entries anywhere outside tests and UI label switches.**
Of the 53, four have a live tag consumer (`Core.Kinesis.PhantomStep:365`, `Swift.Kinetic.SkimDiscipline:630`, `Caster.Spellblade.Edgework:1182`, `Caster.Multispell.Cascade:1352`). **The other 49 are purchasable, cost points, render on the board, and produce zero player-observable change.**
Per tree: Marksman 9/13, Spellblade 8/9, Multispell 8/9, VoidWhisperer 8/9, Kinetic 7/14, Frenzy 3/13, Core 7/30.
Named cases the docs promised and did not deliver: `Caster.Multispell.Reservoir:1302` (cited in `BreakerProgressionTypes.h:139-143` as "the single clearest case", still a bare tag even though `MaxClassResource` now has a lane); `Core.Volley.TriggerDiscipline:262` (the cheapest node in its constellation, named as the motivating case for `RecoilRecovery`, untouched).

### R3 — Three ability rows, three resource loops, ~1,700 lines: attached to nothing. *(blocks three of five classes)*
`Classes/BreakerScrapComponent`, `BreakerGritComponent`, `BreakerChargeComponent` are **never `CreateDefaultSubobject`'d and never spawned**. `Characters/BreakerCharacter.cpp:76-77` creates only `Momentum` and `Mana`. No Blueprint references them. Their only non-test consumers are three `FindComponentByClass` calls in `Progression/BreakerBuildConditions.cpp:45-56` that always return null. **Even `DevForceClass(Gunsmith)` produces a pawn with no Scrap component at all.**
Worse: every generation entry point on all three is uncalled. `NotifyKill`, `NotifyReloadCompleted`, `NotifyMagazineEmptied`, `NotifyDeployableDestroyed`, `NotifyDeployableDamageDealt` (`BreakerScrapComponent.h:82-95`); `NotifyDamageTaken`, `NotifyBlockProc`, `NotifyMeleeKill`, `NotifyDeath` (`BreakerGritComponent.h:94-112`); `NotifyHealingDone`, `NotifyShieldingDone`, `NotifyMarkedTargetDamage`, `NotifyStatusCleansed`, `NotifyAssist` (`BreakerChargeComponent.h:93-103`). **Zero callers anywhere.** Even if attached, all three would sit at zero forever.
The 21 ability rows (`Abilities/BreakerAbilityDefinition.cpp:437-878`) all have `AbilityClass == nullptr`. That part is deliberate (§8).

### R4 — Six of eight weapon archetypes can never drop. *(blocks the looter half of the looter shooter)*
`Combat/BreakerEnemy.cpp:801` draws the slot with `FRandomStream(Seed).RandRange(0, 7)`. `Items/BreakerLootLibrary.cpp:55` opens `RollItemInternal` with `FRandomStream Random(RandomSeed)` — **the same seed** — and its first draw for a weapon slot is also `RandRange(0, 7)` (`:78-79`). Identical seed, identical first draw ⇒ **archetype index == slot index, always.**
Slot `Primary` (6) → **Machinegun**. Slot `Secondary` (7) → **Sidearm**. Rifle, SMG, Sniper, Shotgun, Rocket and BurstRifle **never drop from any kill, at any level, at any rarity, with any seed.** Their affix leans, recoil profiles, prototype definitions and viewmodel layouts are all authored and all unreachable through loot.
The suite does not catch it: `Tests/BreakerWeaponDropTests.cpp:89` picks the slot with `Seed % 2` rather than from the stream, so the collision is invisible to the test that asserts every archetype drops.
Same root cause, second symptom: for **armour** the first stream draw is the affix count, so the slot fully determines it — Helmet/BodyArmour/Gloves/Boots always roll the low count, Necklace/Waist always the high one, in every rarity band.
Measured rate today: ~134 items/hour, of which ~33.5 are weapons, **split ~16.8 Machinegun + ~16.8 Sidearm and 0.0/hour for each of the other six.** After the fix it would be ~4.2/hour per archetype.

### R5 — All six branch keystones are unreachable at the shipped point budget. *(blocks the payoff of every branch)*
`CornerstoneInvestmentGate = 8` (`Progression/BreakerProgressionTree.h:25`), applied at `BreakerProgressionComponent.cpp:213` as `Max(RequiredTreeInvestment, 8)`. A keystone costs 3. So: 8 invested in one branch **plus** 3 for itself = **11 class points**. `SliceClassPointGrant = 10` (`BreakerProgressionLibrary.h:181`).
Bloodrhythm, Overpressure, Culling, Edgework, LongDark, Cascade are each **one point short of purchasable in a real session**. `Tests/BreakerProgressionAuditTests.cpp:372` grants 30 points, so the suite is green while the content is unreachable — exactly the failure shape `BreakerKeystoneReachabilityTests.cpp` was written to catch, one layer up. The only in-game route is the debug re-grant at `UI/BreakerMenu.cpp:6753`.

### R6 — The three maps are literally empty: no PlayerStart, no light, no sky. *(blocks the shipped boot path)*
Binary inspection of all three `.umap` files yields only `WorldSettings`, `Default__Brush`/`BrushComponent0`, `NavigationSystemModuleConfig`, `WorldThumbnailInfo`. `Content/Python/breaker_make_levels.py:38` uses `LevelEditorSubsystem.new_level()`, which produces an empty level, not a template.
- **No lighting exists anywhere in the project.** Grep for `ADirectionalLight|ASkyLight|ASkyAtmosphere|AExponentialHeightFog|DirectionalLightComponent` across `Source/` returns **zero hits**. The only lights are prop-attached `UPointLightComponent`s. `Lvl_Anchor` and `Lvl_Gym` render with no directional or sky light at all — the plaza, the 250 m gym field and every cover piece lit only by a handful of ~700–1200 intensity point lights at 600–1400 cm attenuation. Everything outside those bubbles is black.
- **No PlayerStart in any of the three maps.** PIE papers over this (the editor injects `PlayerStartPIE` at the viewport camera). A packaged or `-game` launch of `Lvl_FrontEnd` — the shipped boot map — has no start spot, and since the whole field frame is derived from the pawn (`BreakerGameMode.cpp:331-342`), no pawn means no frame means no world.
The gym looks correct today **only because PIE boots `Lvl_FirstPerson`**, which carries the template's lighting and start (§8 T1).

### R7 — All seven TARGET-side build conditions are unreachable by construction.
`Progression/BreakerBuildConditions.cpp:272-315` — `SupplyTargetState` has **no production caller**; only `Tests/BreakerConditionVocabularyTests.cpp:328`. Its documented intended call site, `UBreakerCombatComponent::ReceiveDamage`, does not call it. Any effect naming `TargetAiling`/`TargetBleeding`/`TargetPoisoned`/`TargetMultiStatus`/`TargetLowHealth`/`TargetElite`/`TargetAtCloseRange` returns false forever (warns once).
Alongside: the 4 `Recently*` conditions are inert **by design** (`:128-132` `IsSelfEvaluable` returns false; no recorder exists).
Overall condition usage in content: **5 of 24.** Only `Airborne`, `Sliding`, `WallRiding`, `Redline`, `RecentlyDashed` are authored anywhere. The 18 unused include every one the vocabulary doc named as its justification (Reserve's `Aiming`, NoGround's `Grounded`, Debt/Bloodprice's `ResourceDepleted`, Close's `TargetAtCloseRange`, Cascade's `TargetMultiStatus`).
`AlsoRequires` composition is implemented (`BreakerProgressionComponent.cpp:644`) and has **zero authored users**.

### R8 — 86 of 94 node gameplay tags have no consumer.
`Progression/BreakerProgressionLibrary.cpp:10-137` defines 94 `BreakerNodeTags::`. **86 are referenced nowhere outside the file that defines them.** Six more are referenced only from `Source/RiorsEdge/Tests/`: `Node_Read:19`, `Verb_Parry:22`, `Node_Bloodrhythm:91`, `Node_SecondWind:94`, `Node_RedlineTrigger:95`, `Node_NoSafety:96`. `Verb_AirJump:23` is dead outright — O25 made two jumps base kit.
Of the 27 Caster branch tags (`BreakerProgressionLibrary.h:94-124`, `Node_SB_*`/`Node_VW_*`/`Node_MS_*`), **all 27 have no consumer**. A player can spend a full class-point budget in Spellblade and nothing changes.

### R9 — 16 of 31 stat targets have no aggregation lane; 5 of the 15 that do carry nothing.
`BreakerStatTargetHasAggregationLane` (`Progression/BreakerProgressionTypes.h:212-247`) returns true for 15, and **the register is honest** — verified against `AggregateStats`' contribution block (`BreakerProgressionComponent.cpp:717-775`) plus the two out-of-band reads (`DodgeChance`/`BlockChance` at `Combat/BreakerCombatComponent.cpp:80-81`).
No lane at all (any node authored against them is silently unpaid): `AbilityDamage`, `AbilityCooldown`, `AbilityArea`, `AbilityDuration`, `WeaponDamage`, `MeleeDamage`, `IncomingDamageReduction`, `Lifesteal`, `ClassResourceDecay`, `DashCooldown`, `RecoilRecovery`, `WeaponSpread`, `ProjectileCount`, `Pierce`, `StatusDuration`, `StatusChance`.
Lane exists, nothing writes it: `AbilityCost`, `MaxClassResource`, `ClassResourceRegen`, `FireRate`, `Armor` — five lines of aggregator plumbing carrying zero.

### R10 — Keybind rebinding is entirely cosmetic.
`UI/BreakerMenu.cpp:1942-2011` and `Settings/BreakerGameSettings.cpp` write, clamp, conflict-check, persist and reload `KeybindOverrides` correctly. **Nothing in the running game reads it.** Grep for `KeybindOverrides` outside `Settings/`, `Tests/` and `UI/BreakerMenu.cpp` returns zero hits. `ABreakerCharacter` adds the untouched default mapping context at `Characters/BreakerCharacter.cpp:246` and never consults the model. Wiring it means rewriting the `UInputMappingContext` at runtime. Disclosed on screen at `BreakerMenu.cpp:2011` — deliberate, but under O40(c) arguably already a violation.

### R11 — Audio volumes and scoped sensitivity: saved, clamped, routed nowhere.
All three audio volumes: `Settings/BreakerGameSettings.cpp:430-435` logs Verbose and returns. `FAudioDevice::SetTransientMasterVolume` lives in a module this target does not link, and there are no SoundClass/SoundMix assets. **The project has no audio at all** — no owner, no palette, no document (`Design-Overview.md:662` S2).
`ScopedSensitivityMultiplier`: only reader is the settings screen itself. `ABreakerCharacter::Look` (`BreakerCharacter.cpp:497-498`) multiplies by `LookSensitivity` alone with no `IsAiming()` branch. Both disclosed on screen.

### R12 — `BuildClassSelectScreen` — 122 lines no player can reach.
`UI/BreakerMenu.cpp:4404-4525`. `BuildMainScreen`'s comment at `:1350` says BREAKER CLASS "MOVED to the pause menu". **It did not** — `BuildPauseScreen:1390` offers RESUME / LOADOUT / INVENTORY / SETTINGS / RETURN TO TITLE / QUIT and no class entry. The only `Rebuild(ClassSelect)` calls are `:4474` and `:4517`, both *inside the screen itself*, plus the capture switch. `HandleEscape:858` still lists `ClassSelect` in its back-out set, which is what makes the deadness invisible. **The DEV MODE class-swap checkbox at `:4511` lives only here**, so it is currently reachable only from a capture run. Class choice happens at `BuildCharacterCreateScreen:4229` now.

### R13 — In wave mode, 5 of every 6 waves pay nothing at all.
`Game/BreakerWaveBudget.cpp:53` — `bDropsLoot = Kind != Standard`. With `RestWaveInterval=6`/`BossWaveInterval=12`, only waves 6, 12, 18, 24… drop. `Combat/BreakerEnemy.cpp:633` gates the **whole** of `GrantLoot()` on it, **including the currency credit at `:774`**. The comment at `:768-773` claims currency is "the steady income… so the Forge economy does not inherit loot's sparsity" — it is defeated. The wallet does not move on a standard wave.

### R14 — Aberrant, Anomalous, legendaries and item rules are unreachable in the shipped gym.
`GymAreaLevel = 10` (`Game/BreakerGameMode.h:255`) vs `AberrantMinimumItemLevel = 25` / `AnomalousMinimumItemLevel = 40` (`Items/BreakerDropTable.h:154,157`). The standing encounter rolls ilvl 10, so both weights are zeroed (`BreakerDropTable.cpp:59-68`). Wave mode is `10 + wave*2`, so Aberrant needs wave ≥ 8 and Anomalous wave ≥ 15 — **and the boss wave is 12 (ilvl 34), so the Field Marshal can never drop an Anomalous or a legendary.** Everything downstream of `Rarity == Anomalous` (`BreakerLootLibrary.cpp:198-211` — legendary chance, `RollRule`, the three legendaries, O37's 1-legendary cap) is dead in play. `HoursPerAberrant`/`HoursPerAnomalous` both honestly return `TNumericLimits<float>::Max()`.

### R15 — Smaller inert items, cited for completeness.
- `PendingDestinationId` is written (`BreakerGameMode.cpp:104`) and **read by nothing**. Arrival dispatch uses the map name. Its declaration comment (`BreakerGameInstance.h:40`) describes behaviour that does not exist.
- `Caster.Spellblade.Blink` (SB7, `BreakerProgressionLibrary.cpp:1159-1163`) promises "Closequarter may be cast with no target". `Abilities/BreakerAbility_Closequarter.cpp:83` returns on `!Target` with no tag check. Buying the node changes nothing.
- `Keystone_Caster_Cascade` is **completely inert** — zero references outside registry/library/tests. `BreakerAbility_Unmake.cpp` has no keystone branch at all.
- Half of Edgework: "Closequarter loses its range limit" has no consumer anywhere.
- `Caster.VoidWhisperer.LongDark:1269` authors `DamageOverTime` + `MorePercent`. `AggregateStats:657` composes a More product **only** for `StatTarget == Damage`; everything else falls to warn-and-drop at `:664-681`. The 1.30x is dropped every recalculation — deliberate, blocked on O34 (B1 in §7).
- `Docs/Design/Elements.md` — 287 lines, 11 nodes, 4 dependency tags, all BLOCKED on a resistance model O38 deferred post-slice. Ratified-as-deferred, but it is the largest single block of authored design with zero path to a player.
- `Docs/Design/Class-Kits-Gunsmith.md` / `-Tank.md` / `-Support.md` — ~2,700 lines, 36 branch nodes, 21 abilities, none reachable. Honest under O39, but the project's largest inert authoring investment.
- `FBreakerTravelDestination::bEnabled` never goes false; the refusal branch (`BreakerTravelPoint.cpp:52`) is test-only. Deliberate per `BreakerTravelPoint.h:13-17`.
- `BuildLoadoutScreen`'s archetype tiles are now advisory — `SetSlotArchetype` at `BreakerMenu.cpp:2428` is overwritten by `SyncArchetypesToEquipment` on the next equipment change, with no tell on either screen.

---

## 6. KNOWN DEFECTS

### Reported by the owner, repeatedly

**D1 — Arriving in ANY map re-opens the title screen, paused.** `Characters/BreakerCharacter.cpp:250-252` schedules `ShowInitialMenu` on next tick in `BeginPlay` for any locally-controlled pawn, with **no map guard**. `ShowInitialMenu` (`:1057-1079`) calls `OpenMenu(true)` → `ShowMainMenu()` + `SetPause(true)`. `OpenLevel` destroys the pawn, so the new one re-runs `BeginPlay`:
PLAY on the front end → travel to `Lvl_Anchor` → **title screen again**, paused, standing in the hub. Hub gate → `Lvl_Gym` → **title screen again**.
Recoverable (PLAY → character select → `EnterWorldAsCharacter` a second time takes the non-front-end branch at `:329-340`), not a hard lock — but every level transition dumps you on the title. This is the same class of defect the map split was built to remove, re-introduced by the split. The `-BreakerAutoPlay` early-out at `:1066` is why no automated run has caught it.

**D2 — the three maps are empty shells** (no PlayerStart, no lighting). See §5 R6. The owner has reported darkness and load-in problems; this is the root.

**D3 — menu jitter.** VERIFIED FIXED at every `BuildZonedFrame` call site — all four pass `bFillHeight=true` (`BreakerMenu.cpp:3641` inventory, `:6919` skill matrix, `:7285` forge, `:7474` abilities), and `BuildFrame:1046` uses `HeightOverride(MeasureWideScreen().PanelHeight)`.
**One shrink-wrapped centred plate remains:** `BuildDiscardModal`, `BreakerMenu.cpp:3751-3759` — `SOverlay` centred slot around `SBox.WidthOverride(560)` with **no height override**. Its height varies only across rebuilds, so it cannot oscillate per-frame, but it is structurally the exact shape the fix targeted, and it is **not wrapped in `SBreakerPlateProbe`**, so it is invisible to the instrumentation.

**D4 — text cut off / clipped.** See D6.

**D5 — "hard to hit the Warden's weak spot."** Answered by the armour-facing rule, not by the head sphere. The flank **is** winnable and easier than the header claims: `GetFacingArmorMultiplier` (`Combat/BreakerDamageLibrary.cpp:25`) is **binary** with `RearArcCosine = 0.15`, so the rear multiplier applies past **81.4°** off forward — 0.84–1.16 s of circle-strafe, well inside the 2.2 s sweep cooldown. But the reward is **1.90x**, not the 3x §2.3 promises (`FrontalArmor 90 → 47.4%` mitigation, `RearArmorFraction 0.0f`). And the turn-rate cap does nothing for weak-point reachability: the Warden inherits the base weak point at `(0,0,78)` r20 — the head, on the centreline, above a `BodyHitBox` that tops out at Z 62, exposed from **every** bearing including dead ahead.

### Nobody has reported these

**D6 — five fixed-width boxes whose content exceeds them.** None sets `ClipToBounds`, so overflow **overdraws the neighbour**. Chrome per `MakeButton` = 66 px; Roboto Bold caps ≈ 0.62 em.

| file:line | box | worst content | need | over |
|---|---|---|---|---|
| `BreakerMenu.cpp:2069` `SettingsChipWidth=132` | window-mode chip | `FULLSCREEN`/`BORDERLESS` | ~153 | **+21** |
| `:2104` `WidthOverride(96)` | frame-cap chip | `NONE` | ~101 | **+5** |
| `:1917` `DefaultButtonWidth=110` | keybind DEFAULT | `DEFAULT` | ~127 | **+17** |
| `:6809` `WidthOverride(88)` | skill-matrix BACK | `BACK` | ~101 | **+13** |
| `:6107` `WidthOverride(220)` | constellation back | `< ALL CONSTELLATIONS` | ~240 | **+20** |

The same file already owns the fix — `MeasureChipWidth:349` + `PackChipRows:373`, used by the inventory filter bar and **not** by the settings chip strips.

**D7 — equipment-column wrap width is ~36 px too generous.** `BreakerMenu.cpp:3631` puts the column in `SBox.WidthOverride(EquipmentColumnWidth).Padding(FMargin(Space16,0))` — `SBox::Padding` is **interior**, so real content width is `EquipmentColumnWidth - 32`. The item-name wrap at `:2938-2941` and the EQUIP LIMITS wrap at `:3105` are computed against the **full** width. At 1920: wrap 207 vs actual room 171. `ROCKET LAUNCHER` at TypeH2 ≈195 px draws ~24 px into the `i50` column. Worse at `MinEquipmentColumn = 280` (wrap 87 vs room ~51).

**D8 — `SetActorLabel` called unguarded in five places; non-editor builds will not compile.** `Game/BreakerGameMode.cpp:438, 684, 1086`; `Game/BreakerHubBuilder.cpp:107, 228`. `AActor::SetActorLabel` is inside `#if WITH_EDITOR`, and `Source/RiorsEdge.Target.cs` is `TargetType.Game`. Unhit because only the editor target is ever built.

**D9 — no drop in the shipping game carries an item level above 50.** `ABreakerEnemy::ApplyChassis` re-clamps `EnemyLevel` to `[1,50]` **after** calling `GetDropItemLevel`, so the 74x endgame gap is still live in play — while `EndgameComposition` passes because it tests the library function and never touches the actor. Recorded only at `Docs/Design/Power-Curve.md:485-495`, buried at line 485 of a 587-line doc.

**D10 — Edgework permanently removes Cleave's animation lock.** Class-Kits and the node text (`BreakerProgressionLibrary.cpp:1183`) both say "Rewrites Unmake: **during it**, Cleave has no animation lock." `Abilities/BreakerAbility_Cleave.cpp:156-158` checks only `HasMatchingGameplayTag(Keystone_Caster_Edgework)` — a tag published permanently by node purchase. Buying Edgework removes the 0.45 s lock **forever**, not for Unmake's 6 s. A permanent uncapped Cleave-spam buff shipped as an ultimate rewrite.

**D11 — `ChoosePermanentClassById` does not seed `AbilityLoadout`; `ChoosePermanentClass` does.** `BreakerProgressionComponent.cpp:130-145` vs `:147-170`. The class screen calls the *ById* form (`BreakerMenu.cpp:4472`). A freshly-locked character has three `NAME_None` slots; `ResolveDefinition` rescues gameplay via `DefaultAbilityIdForSlot`, but `GetEquippedAbilityId` returns None, so **the ABILITIES tab shows no ability selected while the HUD is firing Cleave/Rot/Unmake.**

**D12 — the O39 gate in `ChoosePermanentClassById` is short-circuitable.** `BreakerProgressionComponent.cpp:125` reads `if (!ClassDefinition && !GetFallbackClassDefinition(ClassId))`. `ClassDefinition` is `EditDefaultsOnly` (`.h:187`), so any Blueprint assigning a default class-definition asset makes the gate pass **unconditionally, for any ClassId including Gunsmith**. The roster gate (`Save/BreakerCharacterRoster.cpp:111`) has no such hole.

**D13 — two independent answers to "is this class real", and the UI ignores the return value.** The class screen asks `ClassHasImplementedKit`; progression and the roster ask `GetFallbackClassDefinition`. They agree today. The moment one Gunsmith ability lands, the screen offers Gunsmith while `ChoosePermanentClassById` still refuses — and `BreakerMenu.cpp:4470-4473` **does not check the return**, so the button appears enabled, does nothing, and rebuilds the screen.

**D14 — unequipping a weapon leaves the old gun in hand.** `Weapons/BreakerWeaponComponent.cpp:951-957` `continue`s on an empty slot by design. Clicking an equipped Primary in the inventory (`BreakerMenu.cpp:2916 UnequipSlot`) removes the affixes and item level but the player keeps firing the shotgun.

**D15 — damage numbers always draw at the target's pivot.** `Combat/BreakerCombatComponent.cpp:119` sets `Context.WorldLocation = GetOwner()->GetActorLocation()`. A weak-point hit on the Drudge's ridge at 129 cm draws its number at the capsule centre (75 cm), 54 cm below the thing you shot. The HUD's fallback and merge comment both assume per-impact locations that never arrive.

**D16 — the killing blow under-reports.** `BreakerDamageLibrary.cpp:98` clamps `HealthDamage` to remaining health and the HUD shows the sum. A 900-damage rocket into a 30 HP Skitter prints `30`. Overkill is invisible, which makes the numbers useless for exactly the TTK measurement O2 wants.

**D17 — DoT ticks are exempt from facing armour.** `BreakerDamageLibrary.cpp:104 MakeSnapshotDotTick` never sets `bHasSourceLocation`, so `BreakerCombatComponent.cpp:56` skips the facing multiplier. A Bleed on a Warden's back always eats the full 47% frontal mitigation.

**D18 — the Warden's shield primitive stops nothing.** `Combat/BreakerWardenEnemy.cpp:82` sets `NoCollision`. It occupies X 47..57, Y ±47, Z −47..87 in front of the body; the player sees a shield and shoots straight through it into `BodyHitBox`.

**D19 — the Drudge's weak point is a detached floating orb.** `Combat/BreakerAlteredEnemy.cpp:223` places it at relative `(-30,0,54)` r20; nearest cosmetic corner is 24.3 cm away against a 20 cm radius — a **~4.3 cm gap**. The constructor only ever checks "inside the capsule", never "touching the body".

**D20 — `DrudgeFromWave = 3` is a dead gate.** `BreakerGameMode.cpp:2132-2134` computes `Clamp(CurrentWave / 4, 0, 2)`. Wave 3 → 0. First Drudge is wave 4.

**D21 — the gym's return gate spawns buried and unrotated.** `BreakerGameMode.cpp:194-195` spawns at `Up = 0` with identity rotation and no `FActorSpawnParameters`; capsule half-height is 100 and the visual is offset −12, so the marker is ~half submerged and faces arbitrarily. The hub's own point does it correctly (`BreakerHubBuilder.cpp:222-225`).

**D22 — the "hub not built" warning fires on every normal gym arrival.** `TeleportPawnToHub` (`BreakerGameMode.cpp:78-94`) warns and returns when `!bHubBuilt`, which is only set in the anchor branch. Because of D1, `EnterWorldAsCharacter` calls it in the gym too. **Read this as noise, not failure** — the pawn stays put and that is correct.

**D23 — `EvaluateForActor` runs every tick on the server** (`BreakerBuildConditions.cpp:29-42`) doing up to 9 `FindComponentByClass` linear scans per frame per pawn. The constructor comment at `:23-26` — "the tick itself is a byte comparison" — is false; only the *recalculation* is skipped.

**D24 — the loudness guarantee has a hole.** `Grounded` is set only inside the movement-component branch (`BreakerBuildConditions.cpp:218`) but `IsSelfEvaluable(Grounded)` returns true via the default case (`:133-134`), so on an actor with no `UBreakerCharacterMovementComponent` it returns false **with no warning**. Same for `Aiming` (no weapon component) and `Stationary`.

**D25 — 6 of the 21 new stat targets still fall through to `"STAT"`** in both `StatTargetLabel` and `ShortStatLabel` (`BreakerMenu.cpp:4626-4700`): `RecoilRecovery`, `WeaponSpread`, `ProjectileCount`, `Pierce`, `StatusDuration`, `StatusChance`. The comment at `:4640-4643` boasts that all 21 were named "in the SAME pass". Unreported because no node authors those targets yet.

**D26 — three skill-screen readout defects, all latent-to-live.**
- `AggregateStats:637` computes `bConditional = Effect.Condition != Always`, ignoring `AlsoRequires`, while `BreakerProgressionTypes.h:311-314` defines `Always` + non-empty `AlsoRequires` as conditional. Such a line can drop out of `DamageMultiplier` without entering `PotentialConditionalDamagePercent`. Latent only because nothing authors `AlsoRequires`.
- `PotentialConditionalDamagePercent` ignores `MorePercent` entirely (`:645-650` requires `IncreasedPercent`), so Terminal Velocity + Redline Doctrine show zero in the potential readout.
- `DamageMoreSourceCount` (`:689`) counts only **live** Mores, so "3 / 3 MORE" drops to "1 / 3" the moment the player lands — while its own comment says it exists so "a fourth purchase visibly does nothing".

**D27 — two sensitivity clamps disagree.** `ClampMouseSensitivity` = [0.2, 2.0] (`Settings/BreakerGameSettings.cpp:17`) vs `ABreakerCharacter::IncreaseSensitivity/Decrease` = [0.2, **3.0**] (`BreakerCharacter.cpp:1037-1038`), writing the same ini key. Nudge past 2.0 in game, reopen settings, and the value is silently clamped down. Knowingly unresolved at `BreakerGameSettings.h:46-49`.

**D28 — a composite action with a stale override can never be cleared.** `MakeKeybindRow:1694` reads the override for any action, but `:1802` draws composite rows as plain text with no handler and `:1911-1912` returns before the DEFAULT button. Only RESET ALL removes it.

**D29 — gamepad keys are bindable and will overflow the key control.** `OnPreviewKeyDown:1252` calls `CommitKeybind` with no gamepad filter, while `MakeKeybindRow:1673-1679` deliberately excludes gamepad keys from defaults. `Gamepad Left Thumbstick Button` needs ~307 px in a 260 px control.

**D30 — four station collisions in the gym field**, three recorded at `Game/BreakerCoverRegistry.cpp:130-135` and one not recorded anywhere. All confirmed arithmetically:
1. Jump-gap run's third landing (14800→16400) overlaps the elite arena marker ring (near edge 15000) by **1400 cm**; ring pillars physically stand on the landing platform. Mitigated only by truncating `JumpRunFarCm` to 16000 (`BreakerGameMode.cpp:1856-1865`) so the exclusion box does not swallow the arena's pillars — the geometry is untouched.
2. Combat pocket 2 at (12900, +6050); its outer ring (r 2600) sweeps 3450–8650 across the wall-ride corridor at 6420–7220. Cover pieces are dropped by `IsAuthoredPieceBlocked`, but the pocket's own **rim ruins** from `SpawnCombatPocket` (`:1571`, radial up to 2200) are **not** gated by that guard and land on the corridor.
3. Combat pocket 3's **centre** (9000, −6050) is *inside* the sniper lane (−7620…−6020), 30 cm from its inner edge. Two of four inner-ring chest blocks are silently dropped.
4. **Unrecorded:** the sniper lane's range marker post sits at forward 9100 — **100 cm from pocket 3's centre.**

**D31 — the front end still gets the playtest HUD.** `HUDClass` is set unconditionally in `BreakerGameMode.cpp:53`, and the front-end branch returns before any frame is built, so the HUD runs against `bFieldFrameSet == false`. Not visibly broken today; nothing guards it.

**D32 — stale or self-contradictory comments that will mislead you.** These are defects because they cause wrong work:
- `BreakerProgressionComponent.cpp:743-749` says DashCooldown has a lane; `:759-769` says flatly there is none and cannot be. The register is right, the top comment is wrong.
- `BreakerProgressionTypes.h:196-198` says the aggregator consumes ten of the new entries. It consumes fifteen.
- `BreakerBuildConditions.h:331` and the vocab doc §8.5 both say "nineteen conditions" were added. 6 → 24 is **eighteen**.
- `BreakerProgressionLibrary.cpp:578-587` claims a keystone is reachable **earlier** than its branch's rewrites. Backwards: tier-4 gates at 6, keystones at 8 plus `CommitToBranch`. Acting on that comment "fixes" a non-problem while the real inversion (gate 8 > budget 10 − cost 3, §5 R5) stays unrecorded.
- `BreakerAbilityComponent.cpp:240-249` is a stale KNOWN BLOCKER: it says `GetFallbackClassDefinition` returns nullptr for every class but Swift. Caster was added at `BreakerProgressionLibrary.cpp:1397-1443`. Do not re-fix a fixed bug.
- `BreakerMenu.cpp:1350` says the class screen moved to the pause menu. It did not (§5 R12).
- `BreakerGameInstance.h:40` describes `PendingDestinationId` being read on arrival. It is not.
- `Combat/BreakerAlteredEnemy.cpp:270` claims `AreCosmeticExtentsWithinCapsule` checks the same list `SetBodyVisible` toggles. It does not — `SetBodyVisible` also includes `WeakPointVisual` (`BreakerEnemy.cpp:608`).
- `CONTEXT.md:277` says the Caster branch trees are unauthored; `CONTEXT.md:41` and the trees at `BreakerProgressionLibrary.cpp:1109-1362` contradict it. Same for `BreakerKeystoneReachabilityTests.cpp:113-121`.
- `CONTEXT.md:101` lists fewer capture screens than the code supports.

**D33 — the ledger's own pending section is structurally hidden.** `Decisions.md:14` tells the reader "Pending questions live at the bottom." The section is at `:128`, followed by implementation notes and O29–O40. **A fresh agent who follows the instruction and jumps to the bottom concludes nothing is pending.** This is the single most consequential documentation defect in the corpus, and only the owner can fix it (R1).

**D34 — documentation staleness.** Only five docs have been touched since the overnight wave began (`Class-Kits-Unbuilt.md`, `Anchor-Hub-Layout-Brief.md`, `Hook-And-Condition-Vocabulary.md`, `Player-Weapon-Gear-Asset-Brief.md`, `Asset-Prompts-Copypaste.md`). Everything else predates the XP loop, the roster, settings, the three resource loops, travel, the three maps, the gym/Anchor split, the S4 widening, the cover registry and the inventory rebuild. Worst offenders in order:
1. `Docs/Design/Design-Overview.md` — **the second link of the authority chain.** `:666` marks S4 as "the largest unwritten technical document in the project" (it was written: `Hook-And-Condition-Vocabulary.md`, 716 lines). `:653` Q24 says "there is no XP system at all". `:63` says the Core tree is six constellations; O38 ruled five.
2. `Docs/Design/Level-Design.md` — §5 and §8 describe `Lvl_FirstPerson` as the game's field, and its editor-delete list points at a map that is no longer played. **Not one document in `Docs/` mentions `Lvl_FrontEnd`, `Lvl_Anchor` or `Lvl_Gym`.**
3. `Docs/Design/Power-Curve.md` — two closed OPENs presented as live (`:497-506` closed by O36; `:511-513` describes a deliberate red test that no longer exists), and it carries D9, the live code defect nothing else surfaces.
4. `Docs/Design/XP-And-Pacing.md` — 1,135 lines of curve design with the system shipped and no `AS BUILT` section; OQ1 still asks a now-measurable question.
5. `UI-Inventory-Spec.md` (its "Superseded 2026-08-14" note is itself superseded), `Playtest-Gym-v1.md`, `Class-Kits.md`, `Anchor-Hub.md`, `Save-Architecture.md`, `UI-UX-Spec.md`, `Core-Tree-Redesign.md`, `Ability-Implementation-Spec.md`, `Damage-Pipeline.md`.
Only two docs have an `AS BUILT` section (`Encounter-Design.md:683`, `Save-Architecture.md:594`). That is the pattern that keeps a design doc from lying. `Docs/Playtest-Feedback-Log.md` owes an entry for the 08-15 inventory/jitter session.

---

## 7. OPEN OWNER DECISIONS

Ordered by what they block. **An agent cannot rule on any of these.** Report and wait.

### Tier A — blocking work right now

**A1. Points per level.** `XP-And-Pacing.md` §4 specifies 1 Class Point/level to 30, 1 Core Point/level to 50. Nothing is implemented. *Blocks:* the entire tree economy — XP is decorative and the tree is a one-time 10/12 grant (§5 R1). **Highest-leverage unblock in the project.**

**A2. The keystone budget contradiction.** `CornerstoneInvestmentGate` 8 + cost 3 = 11 > `SliceClassPointGrant` 10. Gate to 7, grant to 11+, or superseded by A1. *Blocks:* all six branch keystones, the three Overdrive variants, Edgework's Cleave rewrite, LongDark and Cascade (§5 R5). Both numbers are O2-frozen.

**A3. Does an `AbilityDamage` More count inside O34's single 2.197 ceiling?** `BreakerAttributeAggregation.h:223` — `IsMoreCappedAttribute` returns true for `DamageMultiplier` and nothing else. HCV Ruling 3 (trees scale abilities) invites an ability-damage More that would sit **outside** the budget. *Blocks:* HCV Stage 4, explicitly. *If it stays open:* O34's "ONE More ceiling" quietly stops being true, and the two audit tests do not catch it.

**A4. The DoT bucket question [O34].** Do Increased Damage and Increased DoT share one additive bucket for DoT ticks, or keep multiplying? They currently multiply. *Blocks:* `LongDark`'s More permanently — **Caster's entire O3 keystone budget currently produces zero damage.**

**A5. The tier/gate ladder.** Class-Kits §0.2's five-tier shape was compressed to three in the slice. Re-tier keystones, or accept the shape. Re-tiering moves authored gates (O2) and breaks a pinned assertion. Note the recorded direction is backwards (D32).

**A6. Lighting: asset-authored or runtime-built?** No light exists in the project. This determines whether the three maps can stay empty shells at all (§5 R6).

**A7. Does the authored Anchor map replace or dress the runtime hub builder?** `Anchor-Hub-Layout-Brief.md:129-133`. 24 anchor_hub `.uasset` meshes are committed with no ruling about whether they supersede the blockout. *If it stays open:* both run, and every distance in the layout brief becomes a lie.

**A8. The three-map split has no O-ruling.** `Decisions.md` runs O1..O40 and contains **nothing** about maps, the front end, the hub, or travel. The split, the `GameDefaultMap` change, gym-as-fallback and travel-as-`OpenLevel` are all agent decisions taken under a chat brief. Under O28 these want ratification or overturning.

**A9. The four station collisions (D30).** Move the arena past 16400, shorten the jump lane, or accept the overlap; move pockets 2 and 3 off `FieldHalfExtent × 0.55` (6050 against lanes at ±6820), or move the lanes. All are O2 value changes.

**A10. Vendor duplication.** Kess and the Quartermaster exist in both the gym camp (`BreakerGameMode.cpp:1052-1053`) and the hub (`BreakerHubBuilder.cpp:175-180`), offering the same `Quest.FirstContract` from two maps. Removing the gym copies makes the gym unusable as a standalone playtest space — hence a ruling, not a fix.

**A11. Swift's third jump threshold under O40(b).** `SwiftThirdJumpUnlockLevel` defaults to **1** (free at creation) because it shipped at 20 against a `CharacterLevel` nothing wrote. XP now exists. O40(b) says gates re-open **by ruling**. *If it stays open:* Swift's signature unlock is a day-one freebie. Raising it is safe — `RefreshJumpGrant` warns and `JumpGrantMatrix` fails the same day if it is unproducible. **`Decisions.md:89` still records the third jump as unbuilt and only the owner can correct that.**

**A12. `GymAreaLevel = 10` vs the rarity gates.** As shipped, no gym playtest can ever produce an Aberrant, an Anomalous, a legendary or an item rule (§5 R14). Either the area level moves up or the gates move down; both O2-frozen.

**A13. Do keybinds get wired?** (§5 R10) Wire it, or ratify the on-screen disclosure as the shipped state. Under O40(c) a saved-and-inert rebind screen is arguably already a violation.

**A14. `BuildClassSelectScreen` — delete or re-home?** (§5 R12) If it dies, the DEV MODE class-swap toggle needs a new home.

**A15. Three-across inventory needs a full-bleed frame.** `MeasureWideScreen` spends 80 px on margins and caps at 1760, allowing 745 px where the three-card grid needs 879. **Changing the frame policy alters the skill matrix, Forge and abilities tabs too**, which is why it is the owner's call. The solver picks three up automatically the day the room exists.

### Tier B — carried in the ledger

- **B1. Replication position sign-off [O22]** — `Replication-Position.md` DRAFT with a 7-box checklist at §4. Blocks Damage-Pipeline sign-off, where the More ceiling and proc-coefficient law live, whether recoil is client-predicted. The stated deadline ("this week", `Decisions.md:71`, `:168`, dated 08-12) has silently expired.
- **B2. Movement's multiplicative gear × tree composition** — listed pending at `Decisions.md:133-137`, **already conformed in code** (+20/+20 now reads ×1.40, sprint 1584→1540 cm/s). Append-only means the stale entry correctly stays; nothing closes it. Do not re-open it as work.
- **B3. Endgame band verification [O36]** — currently measures 15.40x against 12–20x seed rails.
- **B4. Movement affix uplift [O29]**, **B5. "Frontier pack" name collision [O8] (HELD — do not re-raise)**, **B6. Sealed/Bare modifier scope** (Support's highest-priority call; the blast radius is at `Class-Kits-Support.md:862-869`), **B7. REDESIGN bucket [O20]** (K10 SLIPSTREAM, Bulwark B4/B7/B9 — new numbers cannot fix them), **B8. Dodge resource refund**, **B9. Rift first-clear budget 8 vs 2** (a straight numeric contradiction between two docs), **B10. XP band-to-Rank collapse**, **B11. the stamina GAP [O1] replacement** (not in the pending list; it should be — with the shared pool gone there is no tension mechanism between the two defensive constellations), **B12. Veteran 3.0x multiplier [O23]**.

### Tier C — Design-Overview's own list, and per-domain questions the ledger has never seen

`Design-Overview.md` Q18–Q25 and S2–S4 remain open; **S2 "who owns audio?" is the highest-value unowned domain in the project** — nine telegraphs, one closing ritual, and the whole dodge/block feedback model depend on a channel with no owner, no palette, no document.
Beyond that, roughly forty owner-level questions exist **only inside their own documents and appear nowhere in `Decisions.md`**. The highest-consequence ones:
- `Core-Tree-Redesign.md:770` — **where do defence and mobility go?** O30's taxonomy is entirely offensive; 7 live nodes plus Parry have no axis. Blocks the whole O30 redesign.
- `Class-Kits.md:851` — is one keystone per character the right ceiling? Shapes all fifteen branches.
- `Class-Kits.md:393-405` — **nine Caster nodes invalidated by the Mana inversion and not re-sited.** VW3 Patience doubles primary income (6.0→12.0/s), more than the entire conditional cap.
- `Class-Kits.md:779-786` — "Terminal Velocity" names two different things (the Swift K12 keystone and `Core.Velocity.TerminalVelocity`), and Culling is the first unconditional More in the class layer, which the section's own closing rule forbids.
- `Anchor-Hub.md:1074` — one primary Anchor or a network? "Blocks the entire layout."
- `Campaign-And-Story.md:812` — does the SEAL ending contradict the endgame? SEAL closes every rift; the player then runs Frontiers, which are rifts.
- `Encounter-Design.md:651` — does facing-dependent armour get built? Warden and boss are both designed around it.
- `Encounter-Design.md:664` [10a] — which wins, the wave budget curve or the density caps? Option B recommended, **not ruled**; the solver reports unspent budget from ~wave 8 rather than choosing.
- `Save-Architecture.md:571` [O12 GAP] — **the crafting currencies themselves are undesigned**: no count, names, tiers or values anywhere. Blocks the Forge economy.
- `Save-Architecture.md:565` — deleted-character retention. **Already shipped an answer** (two-step delete) with no ruling behind it.
- `Art-And-Modelling-Plan.md:1013` — target legendary pool size, the largest uncosted art commitment; `:1002` — is the player a cipher or a person, potentially doubling all character art.
- `Elements.md:277` — do enemies use elements against the player? If not, elemental resistance is a stat with no incoming damage to resist — the inert-stat failure again.
- **Two are resolved-by-citation, not by ruling:** `UI-UX-Spec.md:928` is answered by O15 + O37, and `UI-Skill-Tree-Spec.md:159` is answered by O37. Both docs were never swept.

### Tier D — meta

**D1. Should the S4 vocabulary have shipped ahead of its consumers at all?** `Hook-And-Condition-Vocabulary.md` §8.5 records this as a deliberate O40(c) exception justified by loudness. Twenty-four hours later the exception has produced 21 unused stat targets, 18 unused conditions, 5 empty aggregator lanes and 53 unchanged nodes. Worth explicit re-ratification before another vocabulary pass.
**D2. `PowerBand` relabelling.** `All()` now includes target conditions, so the fixture measures against a hypothetical most-convenient enemy. The number will not move until a target-conditional line is authored — **relabel before that, not after**, or O36's pinned rails silently stop measuring what their names claim.
**D3. Is the archetype-per-drop distribution meant to be uniform?** `BreakerLootLibrary.cpp:71-75` says uniform is deliberate and a weighted table would be "a rarity system for gun CLASSES, which nobody has ruled on". Untested in practice, since only two archetypes have ever dropped (§5 R4).
**D4. Should the Drudge be a real solver archetype or stay a re-skin?** The exact fields a real row would need are written out at `BreakerGameMode.cpp:2121-2123`. Reported, not made.
**D5. Do the three unbuilt-class resource components get attached now (dev-inspectable, HUD-less), or stay detached?** Currently unreachable even in dev mode, which is not what O39's text implies.
**D6. When does `bAutoLockSwiftIfFresh` go false** so the class screen's real path is exercised? O39's own named retirement candidate (`Decisions.md:256`).
**D7. Two screens are both titled LOADOUT**, both on the pause menu. Which keeps the name, and does `BuildLoadoutScreen` survive now that `SyncArchetypesToEquipment` overrides its only verb?

---

## 8. TRAPS — non-obvious invariants and things that look like bugs but are deliberate

### T1 — The editor does not boot the front end.
`Config/DefaultEngine.ini` still has `EditorStartupMap=/Game/FirstPerson/Lvl_FirstPerson`. `GameDefaultMap` is `Lvl_FrontEnd`, which governs standalone/packaged launch only. **Pressing Play in the editor loads `Lvl_FirstPerson`, which is none of the three maps, so `IsGymMap` returns true and the full gym builds.** That is why "the loop works in PIE today" — it works in the *old* map, with the template's PlayerStart and lighting.
**Do not "fix" `EditorStartupMap` without first putting a PlayerStart and lighting in the three maps, or you break PIE and lose the only working loop.**

### T2 — The gym fallback is load-bearing, not laziness.
`Game/BreakerGameInstance.cpp:33-43` defines `IsGymMap` as "not front end AND not anchor". The capture harness, `-BreakerAutoPlay` and any PIE drop-in all run in unnamed maps and all expect a gym. Tightening it to a name comparison silently kills all three.
Related: map names are string-matched against `.umap` short names (`BreakerGameInstance.h:47-49`). Renaming a map asset without editing those three functions is a silent no-op — no compile error, no log; the map just becomes "the gym".

### T3 — Two enums are APPEND-ONLY, forever, and they are serialised by value.
- `EBreakerNodeStatTarget` and `EBreakerBuildCondition` are serialised into Data Assets, node effects and affix rows. Inserting or reordering re-points every authored row after the insertion point. Pinned by `Tests/BreakerConditionVocabularyTests.cpp:442-448` (CriticalChance==0, Damage==9, Always==0, RecentlyDashed==5).
- `EBreakerWeaponArchetype` is stored as `uint8` in the save game and replicated (`BreakerWeaponArchetype.h:24-28`). Inserting a value rearms every saved loadout with a different gun.
- The condition mask is `uint32` with a hard **32** ceiling (`BreakerBuildConditions.h:336`). 24 used, 8 left, ~5 informally reserved for Elements.

### T4 — Adding an enum entry does not make it pay.
`BreakerStatTargetHasAggregationLane` (`BreakerProgressionTypes.h:206-211`) is **hand-maintained on purpose** and must flip to `true` in the same commit as the lane, never before. `BreakerConditionVocabularyTests.cpp:410` pins the count at 15 and will fail — deliberately — on any change.

### T5 — Nodes that grant a tag and no number are usually CORRECT, not incomplete.
The whole tier-4 Swift trio and every Caster non-keystone node are **rule rewrites**. `BreakerProgressionLibrary.cpp:537-588` is the authoritative rationale, and each node carries a `WAITING ON:` comment naming its real consumer. Inventing Increased Damage lines for them is an O2 violation and, for affix-rule rewrites, an explicit Class-Kits §6.4 violation.
Likewise, **reserved More slots are intentionally empty**: `Core.Elements.ReactionChain` (`:519-526`), `Caster.Spellblade.Edgework` (`:1171-1188`), `Caster.Multispell.Cascade` (`:1341-1358`). Authoring them unconditional makes them *stronger* than designed.

### T6 — The 21 null ability rows are not a TODO.
`Abilities/BreakerAbilityDefinition.cpp:385-421` is an explicit instruction block: do not point them at nearest-fit abilities, do not extend `DefaultAbilityIdForSlot`, do not register class definitions. `Tests/BreakerAbilityTests.cpp` asserts a Tank slot resolves to null; `BreakerProgressionAuditTests.cpp:212` asserts Gunsmith grants nothing. "Fixing" any of it turns the suite red **on purpose**.
Within those rows, three more deliberate-looking-wrong things: `WindowDuration = 0` on `Gunsmith.SidearmRig` (`:447-451`) is correct — its window is counted in shots. The Gunsmith cost/cooldown split (`:427-433`) is the class's ergonomic. `AbilityCostMultiplier = 1.0` on Field Assembly and Hold (`:532-539`, `:679-685`) states what the window does to *other* abilities' price; 0.0 would make every subsequent cast free.

### T7 — The exact chain that must happen before an unbuilt class becomes selectable.
1. Attach the resource component in `BreakerCharacter.cpp:76-77` (currently step zero and undocumented).
2. Wire its `Notify*` entry points from combat/weapon/status/healing. Grit needs `Instigator` on the damage request; Charge needs healing with separated overheal reporting. Prerequisites, not polish.
3. Write the `UGameplayAbility` subclasses and assign `AbilityClass`. Four Gunsmith and one Tank ability are deployables, blocked on O30 — **no deployable system exists in any form**. `Tank.Hold`'s per-hit damage cap needs a damage-pipeline hook that does not exist.
4. Add a `GetFallbackClassDefinition` row (`BreakerProgressionLibrary.cpp:1392`) mirroring the registry ids exactly.
5. Add the class to `DefaultAbilityIdForSlot` (`BreakerAbilityDefinition.cpp:958`) or its three keys stay dead.
6. Add the resource to the HUD (`BreakerPlaytestHUD.cpp:688-693` reads only Momentum and Mana).
7. O39 then opens **automatically**, because `ClassHasImplementedKit` is derived, not a list.
**Steps 4 and 3 must not be inverted.** A class definition registered before its abilities execute recreates the exact bug that made every Caster ability read as locked.

### T8 — Caster deliberately authors no cooldowns anywhere.
`Classes/BreakerCasterAbility.cpp:9-16` nulls `CooldownGameplayEffectClass`. **Mana is the cooldown.** An empty `CooldownTag` means cost-gated, which the HUD must distinguish from "cooldown of zero" (spec D3). Do not fill in the missing tags.

### T9 — Deliberate asymmetries you will want to "tidy".
- `ResolveDefinition` returns `nullptr` for a wrong-**slot** id but falls back for a wrong-**class** id (`BreakerAbilityComponent.cpp:104-116`). Pinned by `RiorsEdge.Abilities.SlotResolution`; folding the branches has already broken it once.
- `RespecAtForge(CorePoints, …)` deliberately does **not** clear `CommittedBranch` (`BreakerProgressionComponent.cpp:297-309`) — commitment is a class-branch fact; only the ClassPoints respec clears it.
- `bDropsLoot` gates items **and** currency but does **not** gate XP (`BreakerEnemy.cpp:739-747`). Do not "fix" the inconsistency by gating XP.
- `GrantModifiers` restores the authored rank and re-publishes the modifier set (`BreakerGameMode.cpp:1289-1305`) because `SetMonsterRank` rebuilds the chassis; `GrantModifierCarrier` deliberately does not. Correct.
- Wakeful runs before death by explicit call, not by delegate order (`BreakerEnemy.cpp:616-625`). Do not convert it to an `OnDeath` binding.
- `DevForceClass` re-fetches the class definition only on mismatch (`BreakerProgressionComponent.cpp:94-113`) so an authored Data Asset is never stomped by the C++ fallback. The apparently redundant class filters at `:403`, `:558`, `:591-594` are defence in depth and should stay.
- `Core.Precision.CalledShot` / `Swift.Marksman.CalledShot` and `Core.Volley.TriggerDiscipline` / `Swift.Frenzy.TriggerDiscipline` share display names **deliberately** (`:61-66`, `:77-81`) — separate ids, tags and currencies, names transcribed from Class-Kits. Do not rename either.
- `EBreakerNodeStatTarget` ≠ `EBreakerStatTarget`. Node effects and equipment affixes are separate enums by design (`BreakerProgressionTypes.h:34-36`). Do not merge or cross-reference.
- `AbilityDamage`/`WeaponDamage`/`MeleeDamage` **partition** `Damage`. A node authors one of the three, never `Damage` plus a partition. `BreakerConditionVocabularyTests.cpp:432-437` asserts the partitions have no lane precisely so they cannot silently borrow Damage's.

### T10 — Slate specifics in `BreakerMenu.cpp` you will misread.
- **`CurrentScreen` is assigned synchronously in `Rebuild` (`:932`) while the widget swap is deferred.** This looks like a bug; it is the fix for "the menu snaps to other screens". Do not move the assignment into `ApplyScreen`.
- `Rebuild` unconditionally logs `[MenuRebuild]` and stack-dumps Inventory↔SkillTrees transitions (`:917-920`). **Live instrumentation, not leftovers.**
- `ApplyScreen` drops `GameSettings` on leaving Settings (`:977`) but keeps `DefaultKeybinds`. The model must be re-read because `ABreakerCharacter`'s F-key nudges are a second writer of the same ini keys. Caching it across screens is a regression.
- **Disabled controls are painted, never `IsEnabled(false)`** (`:4457-4463`). Slate fades whole subtrees; FIELDPLATE forbids opacity changes. Refuse the click in the handler instead.
- `SAssignNew` handles must be built first and captured **by value** (`:1540-1545`) — argument evaluation order is unspecified and a by-reference capture dangles on some compilers.
- Delta glyphs are ASCII `+ - =` on purpose (`BreakerUIStyle.h:119-121`) — the engine's Roboto has no Geometric Shapes block and the spec's triangles render as tofu.
- `MeasureWideScreen()` reading the viewport is **not** the banned pattern (`:327-331`) — nothing reads its own arrangement and it is sampled once per `Rebuild`.
- `WearOrder()` deliberately contradicts the mockup's markup (`BreakerMenu.h:226-230`) — the reference's *prose* is the rule; the mockup is one sample of it.
- Class-implemented queries go through `UBreakerAbilityDefinition::ClassHasImplementedKit`, **never through row counts** (`:2557-2572`). A row-count proxy would offer a permanent, irreversible class lock onto nothing.
- Skill boards open at **1:1, not fit-to-width** — fit-to-width was implemented, photographed and reverted (5 px type against FIELDPLATE's 11 px floor).

**Reading the `SBreakerPlateProbe` log** (`:232-317`): `[MenuGeom] <Label> desired=WxH arranged=WxH change=N frame=F dframes=D`.
- `dframes` large + a `[MenuRebuild]` immediately before → **rebuild-time** jitter; the plate is a different size per build and, being centred, lands elsewhere.
- `dframes` small (1–2) advancing with **no** `[MenuRebuild]` between → genuine per-frame oscillation; something reads its own arrangement. The axis whose numbers move names the culprit.
- Watch `[MenuGeom] viewport=... panel=...` from `MeasureWideScreen:440` — logged on change only. If it repeats, the viewport is breathing and every derived plate follows.
- **Instrumentation gap:** `BuildFrame` passes the constant label `"BuildFrame"` (`:1044`) for all eight narrow screens, which have five different `PanelWidth`s. Correlate with `[MenuRebuild]`'s enum pair before trusting it. `BuildDiscardModal`'s plate is not probed at all.

### T11 — Cover-registry invariants that look like off-by-ones and are not.
- `SpawnExpandedField` **must run first** (`BreakerGameMode.cpp:167-169`) — the apron must exist before anything ground-snaps.
- `IsAuthoredPieceBlocked` and `IsClusterCentreExcluded` are two different guards for two different placement kinds (`BreakerCoverRegistry.cpp:136-146` vs `:156-215`). Applying one to the other's callers pushes the whole lattice off the near half of the field; `:184-187` records that exact regression.
- Yaw is drawn from the seed stream **before** the exclusion test (`:326-330`) so rotations stay stable when an exclusion moves. Moving the draw below the `continue` reshuffles the field.
- The pocket-pillar bearing fallback `{180, 90, 270, 0}` (`:266`) is not padding — the third pocket genuinely needs 90 because it sits on the sniper lane.
- `FillThreshold = CoverPitchMax - FillStep * 0.70711` (`:463-464`) is the 1/√2 half-diagonal of the sample grid. Changing `FillStep` without the threshold reintroduces holes exactly halfway between samples.
- The fill grid uses **inclusive** endpoints (`Row <= FillRows`) — `:465-468`, `:523-526`. An exclusive loop leaves the band's far corners unfilled.
- `ResolveGroundZ` probes a **ring**, not straight down (`:303-329`), to avoid `Lvl_FirstPerson`'s 210 cm plinth. In the three empty maps there is no floor at all, so the no-hits path at `:328` is now the normal case.
- **The gym world position is not fixed.** `BuildFieldFrame` derives ground/forward/right from the possessed pawn, so two PIE sessions from different camera positions produce the same field at different world locations. Screenshots comparing absolute coordinates across sessions are meaningless.
- The `Hub*` prefix on `BreakerHubBuilder.cpp`'s anonymous-namespace helpers (`:39-50`) survives unity-build merging. Do not "simplify" it.

### T12 — Combat traps.
- **`RollRarity` is not the loot pipeline.** `BreakerLootLibrary.cpp:24` is a legacy fully-ungated overload kept for dev grants and crafting previews. Content calls `UBreakerDropTableLibrary::RollDrop`. Calling `RollRarity` from content silently restores the exact bug the owner reported (every kill drops; trash rolls Aberrant).
- **The drop salts at `BreakerDropTable.cpp:16-25` must not be changed.** Changing one re-rolls every item in the game from the same kill seeds and breaks "a seed reproduces an item exactly". *(Note: fixing §5 R4 does change every historical drop seed — that is unavoidable and should be stated to the owner.)*
- **The Drudge adding no behaviour is deliberate.** `BreakerAlteredEnemy.h:45-50` — late severance means "nothing left but the shape"; `UsesCoverDiscipline()` and `FlinchesWhenHit()` resolve false and `WeaveStrength`/`LungeRange` are 0 on purpose. Skitter owns the leap.
- **The Drudge's 60/75 capsule override is load-bearing** (`BreakerAlteredEnemy.cpp:104-106`). Reverting to the base 45/90 clips `RightArmVisual` outside the collision. The horizontal check is the **diagonal to the box's far corner**, not per-axis.
- **The wave solver has no Drudge row on purpose** — a Drudge is substituted for a melee body from the **end** of the list so elite/carrier promotions (which take low indices) never become Drudges (`BreakerGameMode.cpp:2112-2130`).
- **The gym is not the boot map.** `HandleStartingNewPlayer_Implementation` returns early for the front end (`:139-144`) and again for the Anchor (`:150-163`). **Nothing in Combat/Weapons/Items is reachable until the player travels front end → Anchor → gym.** A headless smoke test that boots and asserts enemies exist will fail *correctly*.

### T13 — Test-rig traps.
- The progression trees are `static` process-lifetime singletons created with `NewObject(GetTransientPackage())` + `AddToRoot()`. **Mutating a node in one test leaks into every subsequent test in the same editor session.**
- `bAutoLockSwiftIfFresh` defaults **true** (`BreakerProgressionComponent.h:119`, used at `.cpp:488`). Every test assuming a fresh pawn arrives as Swift depends on it. Any rig that does not set the flag gets a Swift character.
- `FBreakerProgressionState::CharacterLevel` is **derived, never authoritative** (`BreakerProgressionTypes.h:401-407`). `RefreshLevelFromXp` is its only legal writer.
- The `KeystoneReachability` test's "honest emptiness" arm (`BreakerKeystoneReachabilityTests.cpp:111-126`) is what currently keeps the 9 Gunsmith/Tank/Support keystone tags legal. Authoring a branch tree for any of those classes without siting its keystone on a `bCornerstone` node turns the suite red — **that is the alarm, not a regression.**
- **Grant more points than the shipped budget and you will hide a reachability bug.** `BreakerProgressionAuditTests.cpp:372` grants 30; the shipped grant is 10. This is exactly how §5 R5 stayed invisible.

### T14 — Deliberate-looking-wrong, elsewhere.
- `EBreakerDamageFamily::Elemental` has no per-element split — O5's resistance model arrives later.
- `ElementalDamageReduction` is reserved and absent from the affix pool — deliberate, "so it lies to nobody."
- Loot rarity gates were **deliberately not** rescaled to O29's 1–120 range — proportional scaling would mean a player finishes the entire 1–50 campaign never seeing an Aberrant.
- The wave solver reporting unspent budget from ~wave 8 is deliberate: the curve and the density caps contradict each other and the solver reports the shortfall rather than silently picking a side (open at §7 Tier C).
- Every treatment doc opens with "NOTHING IN THIS DOCUMENT IS BUILT" and that is no longer quite true — read `Class-Kits-Unbuilt.md` §0 for real status. **Where `Class-Kits-Unbuilt.md` and a full treatment disagree, the treatment wins.**

---

## 9. WHAT I WOULD DO NEXT, IN PRIORITY ORDER

The ordering principle: **first make the shipped configuration reachable at all, then make the content already authored pay, then add.** Every item below is either a bug fix or an owner question — none of it authors new design.

1. **Fix D1 — the title screen on every level arrival.** Map-guard `ShowInitialMenu` in `Characters/BreakerCharacter.cpp:250-252` so it only fires on the front end. *Why first:* it is the only defect that makes the milestone the owner just shipped unusable on its own terms. Everything else in this list is invisible until someone can walk boot → front end → Anchor → gym without landing on the title twice.
2. **Give the three maps a PlayerStart, then take A6 to the owner for lighting** (§5 R6, §7 A6). *Why second:* without a PlayerStart the shipped boot map produces no pawn, no field frame and no world outside PIE — so the three-map flow has never been executed and cannot be tested. Do **not** touch `EditorStartupMap` until both land (T1). Lighting is the owner's call because runtime-vs-authored decides whether the maps stay empty shells.
3. **Write the O40(c) shipped-configuration test that should have caught #1 and #2**: boot → front end → PLAY → Anchor → gym → Anchor, in the `JumpGrantMatrix` mold. *Why third:* the same failure shape (a green test proving a rule against a configuration the game cannot produce) has now shipped three times — `JumpGrant`, `PowerBand`, and the keystone budget. Fixing the bug without the test guarantees a fourth.
4. **Fix the loot seed collision** — `Combat/BreakerEnemy.cpp:801`, salt the slot draw (`FRandomStream(HashCombine(Seed, <new salt>))`). *Why fourth:* one line moves the game from **two** droppable weapon archetypes to **eight**, unlocking authored content that already exists in full (leans, recoil profiles, prototypes, viewmodels). Highest reachability-per-line-changed in the codebase. Fix `Tests/BreakerWeaponDropTests.cpp:89` to draw the slot from the stream in the same commit, or the test still cannot see the bug. Tell the owner it changes every historical drop seed.
5. **Take A1 (points per level) to the owner and build it the moment it is ruled.** *Why fifth:* it is the single highest-leverage unblock (§7 A1) but it is a value question an agent may not answer under O2. Until it lands, the XP loop shipped yesterday is decorative and the entire 97-node tree is a one-time 10/12 grant. Pair it with A2 in the same conversation — A1 may supersede A2 entirely.
6. **Ungate currency from `bDropsLoot`** (§5 R13, `Combat/BreakerEnemy.cpp:633` / `:774`) the way XP is already ungated. *Why sixth:* it restores the Forge economy on 5 of every 6 waves, it is a small mechanical change, and it makes the Forge screens — which are fully built — actually reachable through play.
7. **Fix the misleading comments in D32.** *Why seventh:* they are cheap and each one is a trap that will cost a future agent a wasted session — one of them (`BreakerProgressionLibrary.cpp:578-587`) actively directs an agent to "fix" a non-existent inversion while the real one goes unrecorded. Do not touch `Decisions.md` (R1); report its issues instead.
8. **Fix D6/D7 — the five overflowing fixed boxes and the equipment-column wrap.** *Why eighth:* it is the owner's most-repeated report class, the file already owns the fix (`MeasureChipWidth`/`PackChipRows`), and it is verifiable with the capture harness in one run. Photograph before and after.
9. **Fix D8** (`SetActorLabel` unguarded in five places) *before* anyone attempts a package. Trivial now, a wall later.
10. **Then take the reachability questions to the owner in one batch** — A7 (Anchor map vs hub builder), A9 (station collisions), A10 (vendor duplication), A12 (gym area level vs rarity gates), A13 (keybinds), A14 (dead class screen), A15 (inventory frame). *Why last:* each unblocks a slice of §5, none can be answered by an agent, and batching them costs the owner one conversation instead of seven.

**What I would deliberately NOT do next:** author any new stat targets, conditions, nodes, abilities or classes. The project's demonstrated failure mode is shipping authored content ahead of its consumer — 53 inert nodes, 86 unconsumed tags, 21 unused stat targets, 18 unused conditions, 5 empty aggregator lanes, ~1,700 lines of detached resource components and ~2,700 lines of unreachable class design. **Nothing new should be authored until §5 is materially shorter.**