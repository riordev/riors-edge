# Rior's Edge — agent context

**Last reconciled against: O32**

This is the durable handoff document for anyone working on the project. Read it before making changes, then update it whenever architecture, milestone status, paths, or workflow constraints materially change.

## Product vision

Rior's Edge (working codename Project Breaker) is a fast first-person, movement-driven ARPG looter shooter. Movement is part of character building rather than a fixed utility layer. Weapons, affixes, legendary items, abilities, and skill nodes should interact with momentum, dash, slide, wall movement, gravity, and other grounded movement disciplines. Grapple/tether mechanics are explicitly excluded.

The immediate goal is a small vertical slice, not the full world: one graybox biome or arena, expressive movement, three weapon archetypes, three normal enemies, one elite modifier, one boss, a small affix pool, three build-defining legendary items, roughly 15 skill nodes, and save/resume.

The current character concept proposes five classes—Caster, Swift, Gunsmith, Tank, and Support—with three branches each, class-specific resources, and a separate six-constellation universal Core Tree. The implementation analysis is in `Docs/Character-Progression-Architecture.md`. Class identity, universal progression, and equipment affixes are separate layers; do not merge their data models.

Locked progression decisions: class selection is permanent per character; characters equip two class abilities and one ultimate; solo is the primary balance target with parties up to five; DoTs can crit and snapshot offensive stats at application; respecs require a Forge; the level cap is 50 with a hard stop and no post-cap power progression, so all endgame character power comes from gear; dash, slide, wall ride, block, and dodge are all base kit, with trees improving them rather than unlocking them; TWO JUMPS are base kit for everyone and Swift innately unlocks a third later (O25, superseding the earlier air-jump-is-tree-granted line), leaving parry as the only tree-granted verb. Movement is a big part of the game but is NOT the centre of the design and gets no further dedicated passes for now (O26).

## Current milestone and next actions (updated 2026-08-14)

Current milestone: **Vertical-slice systems — Swift AND Caster playable
end-to-end, framed by the O33 identity stack** (four avenues: class verbs,
Core axes, gear, legendary rewrites — class is never the sole trunk).
Movement gym, combat sandbox, loot loop, and the progression framework are
all live; presentation is deliberately blockout.

The whole loop works in Play In Editor today: spawn at the safe ring →
talk to camp NPCs (F) → fight the encounter/waves (F4) → loot ground drops
(F) → equip (I, tabs for EQUIPMENT | SKILL TREES | ABILITIES | FORGE) →
spend points, COMMIT a branch (two-step control on the strip; unlocks the
keystone tier per O37), pick abilities per slot on the ABILITIES tab, and
salvage/Temper/Reforge/Attune on the FORGE tab → use Swift abilities
(E Skim / T Lead / G Overdrive) or Caster abilities (defaults E Cleave /
T Rot / G Unmake; all seven pickable). Playtest keys: F1 reset, F2 copy
report (engagement-gapped TTK vs O18 targets, now including the
modifier-bearing bucket, deaths and engaged TTD), F3 diagnostics, 1/2
weapon slots. Dev tools (class swap, test gear, legendary grant, point
grants) gate on the DEV checkbox on the BREAKER CLASS screen; outside dev
mode the class screen offers only implemented kits (O39).

**THE PROJECT CAN LOOK AT ITSELF.** Eight switches, all verified against their
parse sites:

| Switch | Form | What it does |
|---|---|---|
| `-BreakerAutoPlay` | flag | Skips the title menu into the gym. **Required for `-BreakerCaptureMenu`**, which is parsed inside its branch. |
| `-BreakerScreenshots=N` | int, clamped 1-60 | Takes N frames then exits. First at 6.0 s, then every 2.0 s. |
| `-BreakerCaptureMenu=<SCREEN>` | string | `INVENTORY`, `SKILLTREES` (or `SKILLS`), `LOADOUT`, `SETTINGS`, `CLASS` (or `CLASSSELECT`), `PAUSE`. Anything else **silently** falls back to the main screen. |
| `-BreakerCaptureBoard=<BOARD>` | **string, NOT a bare flag** | `CORE`, `COMPARE`, or `BRANCH<n>`. |
| `-BreakerCaptureTour` | flag | Eight authored field vantages instead of the player's eyes. |
| `-BreakerCaptureHUD` | flag | Fabricates the HUD **events** a headless run cannot reach. |
| `-BreakerCycleWeapons=<seconds>` | float, > 0 | Walks the viewmodel through the archetypes. |
| `-BreakerBossOnStart` | flag | Spawns the Field Marshal during the gym build so it can be photographed. |

Frames land in `Saved/Screenshots/breaker_NN.png`; the process exits ~2.5 s
after the last. **Capture runs on a CORE ticker, not a world timer**, because
opening a menu pauses the world — the first version photographed nothing while
logging success. F5 (or `Breaker.Boss`) spawns the boss interactively.

**USE IT. Every agent doing visual work is expected to read its own
screenshots.** Automation proves arithmetic and cannot see a layout. Looking has
found: the first-person arms rendering entirely OFFSCREEN (y=1283 on a 1080
viewport); the template level being a SEALED 40 m courtyard with the whole
runtime field stranded outside it and built 212 cm too high; the HUD reading
`MOMENTUMSETTLED`; a keystone marker drawing as a black hole; the loadout
clipping its own weapon names; the class screen dimming all five class names to
unreadable; two Skirmishers standing beside two Lattices in violation of a cap
the same pass had just added; and the ground's dashed tearing seams. Every one
of those had shipped with a green suite.

**Two structural limits of the harness, both load-bearing.** It cannot move a
mouse, so **every hover state and every zoom/pan gesture in the project is
unverifiable by it** — that is a permanent gap, not a backlog item. And a
vantage set is a hypothesis about what can go wrong: the ground tearing was
invisible from all seven existing vantages and needed an eighth, cut along the
ground at a grazing angle, before it could be seen at all.

**Nothing is in flight.** All three parallel lanes named here previously — the
`Game/`+`Playtest/` enemy integration, the `Save/`+`Interaction/` quest lane,
and the `Combat/`+`Abilities/` Mana lane — have merged, along with O29's item
level 120, the drop pipeline, the skill-board viewport and the HUD feedback
pass. `main` is the truth. If you are picking up work, start from the list
below rather than from a branch.

**THE SUITE IS FULLY GREEN AGAIN — 244 tests, 0 failures — and green is
meaningful.** O36 ruled the band question the former deliberate reds were
holding open: the band is authored at TWO points now — at-cap 8–10x
(measures 8.08x) and endgame seed rails 12–20x at ilvl 120 (measures
15.40x) — and `PowerBand.AtCap` / `PowerBand.Endgame` / `RuleBandImpact`
(PROLIFIC re-anchored to a 1.5x endgame ceiling) pin all three. A red test
is a regression again, everywhere.

**The 2026-08-14 O33–O40 wave** (five parallel worktree lanes, merged
`a877be9`..`2c6b106` plus integration): the owner ruled the identity stack
(O33), the multiplier canon with ONE More ceiling and windows counting
inside it (O34), abilities riding the equipped weapon's item-level scalar
(O35), the two bands and the ilvl 101–120 source (O36), per-axis equip
caps and commitment-as-empowerment (O37), Elements post-slice (O38), slice
class honesty (O39), and the hygiene rulings (O40: dash model final, no
CharacterLevel gates until an XP loop exists, reachability is part of
definition-of-done). Code followed: the second More budget is deleted and
the chain counts against the aggregator's 2.197 (a source-scan test proves
no restated constant); every class ability scales with gear (anchored
bit-identical at ilvl 1); Fracture rides the modifier chain and DoT
snapshots capture window multipliers at application; weak point is clamped
to its ruled [1.0, 2.0]; modifier-bearing enemies exist at kill time (gym
carriers + wave solver row) so the O27 bucket finally receives samples;
the F2 report measures deaths and engaged TTD; the Forge wallet persists
(save v3); the condition mask is uint32 with loud dead-lane warnings; the
five phantom node ability grants are fixed with a registry validation
test; the Caster fallback class definition exists so ability selection
actually works; all three Swift branch keystones are cornerstones behind
the O37 commitment gate; nodes carry a Constellation field and VELOCITY
renders as a real plate (UNMAPPED is a loud fallback only); and
`ChoosePermanentClassById` refuses kitless classes at the component. Docs:
the corpus is reconciled through O40 — Design-Overview §7 renumbered to
Q-numbers (the O18–O25 citation collision is dead), the identity-stack and
multiplier-canon sections exist, the O25/O29/O32 sweeps landed, every
design doc carries a Scope marker, and `Docs/Design/Replication-Position.md`
is a DRAFT awaiting the owner (O22 keeps it owner-authored).

Next actions, in priority order:

1. **THE MEASURING PLAYTEST. Everything is now instrumented for it; nothing
   about it can be automated further.** Set `WeakPointToleranceCm = 0`, walk
   `GymAreaLevel` 1 / 10 / 25 / 50 / 75 / 100, and read the F2 splits —
   melee / ranged / elite / modifier-bearing (now populated) / boss, plus
   deaths and engaged TTD vs the 4–5s target. Drops now track area level to
   the full ladder (O29 end-to-end). The predictions to falsify: baseline
   TTK roughly flat at every stop, and TTD inside its band. While in there,
   FEEL the new reach: pick Caster abilities on the ABILITIES tab, COMMIT a
   branch and buy its keystone, grant a legendary from the dev panel and
   equip it against the per-axis caps, salvage into the wallet and Temper —
   every one of these paths is hours old and none has ever been touched by
   a human. O36's endgame rails (12–20x) are seed arithmetic; this playtest
   and the ones after it are what confirm or move them.
2. **Assets are the binding constraint on feel, and have been for three
   sessions.** In order of how much they block: **AUDIO** (nothing exists at
   all — recoil, bloom, viewmodel kick, the boss's three phases and every
   ability all land on silence, and this is now the largest single gap between
   what is built and what is felt); the three OFL faces (Saira Condensed /
   Barlow / JetBrains Mono — everything renders in the engine default, the type
   scale is already correct so it is a swap, not a layout change); weapon and
   character meshes (recoil currently kicks a grey box); muzzle flash and impact
   VFX (hooks and timings fire into nothing); the nine ability glyph SVGs (code
   stand-ins exist). All of these need the owner: downloading fonts and
   authoring `.uasset`s is editor work.
3. **Owner decisions, none blocking code.** Ranked by how much they cost to
   leave open:
   - **O30's Core-tree redesign is scouted; the mask is ready, the vocabulary
     is not.** The condition bitmask is uint32 now (the silent-overflow trap
     is gone), but `EBreakerBuildCondition` itself is still movement-only —
     the actual widening (ailment/combat/status conditions) waits on the S4
     hook-and-condition vocabulary in Design-Overview, which remains the
     largest unwritten technical document in the project and the single
     highest-leverage unblock in the progression layer. Write S4, widen the
     enum, then pilot ONE axis end-to-end before authoring at scale.
   - **O32's legendary pool is ruled and not built.** The ruling is that the
     pool grows rather than the drop rate; the pool is still the same three
     items (Boots / Primary / Waist), and Helmet, Body Armour, Gloves, Necklace
     and Secondary have none. Until more are authored the measured wait stays
     ~57 hours rather than the ~21 the ruling reasons about.
   - **Subclass commitment is BUILT (O37)** — `CommittedBranch` on the state,
     one-way `CommitToBranch`, Forge respec clears, keystone tier gated,
     ordinary nodes untouched (O15 intact), two-step COMMIT control on the
     branch strip. What remains owner-side is FEELING whether the keystone
     gate reads as empowerment in play.
   - **Two O34 questions the owner still holds:** whether Increased Damage
     and Increased DoT share one additive bucket for DoT ticks (they
     currently multiply — a recorded canon deviation), and sign-off on
     `Docs/Design/Replication-Position.md` (DRAFT; O22 keeps it
     owner-authored; it gates Damage-Pipeline sign-off and decides where the
     More ceiling and recoil prediction live).
   - **Swift's third jump THRESHOLD** (O25). The mechanism is built and the gate
     now defaults to **1**, because it shipped at 20 against a `CharacterLevel`
     that **nothing in the project writes** — no XP loop, no assignment
     anywhere — so the feature was unreachable BY CONSTRUCTION and the only
     instrument that caught it was the owner saying "i never could do a 3rd
     jump". Raise it again the day `CharacterLevel` starts moving:
     `RefreshJumpGrant` warns once if it is ever set above a level the game can
     produce, and `RiorsEdge.Movement.JumpGrantMatrix` fails the same day.
     **`Decisions.md` still records the third jump as unbuilt.** That file is
     append-only and owner-only, so correcting it needs an entry from the owner.
   - **Judge the Mana inversion's fallout.** Two consequences were reported
     rather than silently retuned: Overcast's deterrent is now weaker than
     authored (doubled generation applies to a much larger regen, so a full debt
     repays in under two seconds and the cost is almost entirely the damage
     window), and several Void Whisperer / Spellblade nodes now buy a share of
     the smaller half of income while **VW3 Patience became one of the strongest
     nodes in the class**. Dials exist; nothing has been turned.
   - **Movement's gear x tree composition is CONFORMED — judge the feel.**
     +20/+20 reads x1.40 rather than x1.44 (sprint 1584 -> 1540 cm/s, -2.8%);
     one-layer builds are bit-identical. If it feels wrong the fix is to retune
     the CONTENT, not to put the multiplication back. Table in
     `Docs/Movement-Design.md`.
   - Overdrive's +25% damage window is a 4th More against the O3 budget of 3
     (flagged in `BreakerAbility_Overdrive.h`); the **O22 replication position**
     page, which gates Damage-Pipeline sign-off and also decides whether recoil
     should be client-predicted; the held items at the foot of `Decisions.md`.
4. **The highest-value PLAYTEST available.** The power curve's arithmetic is
   now closed by automation — `RiorsEdge.Combat.PowerCurve.EndgameComposition`
   asserts baseline TTK is flat within 0.9-1.1x across area level 50 to 100,
   replacing the old `EndgameClamp` which asserted the 74x gap was still open.
   What automation cannot answer is whether the curve *feels* like progression.
   The run: set `WeakPointToleranceCm = 0` (the forgiveness halo inflates damage
   per hit by 8-14% at a 50-60% weak-point rate and flatters every number), then
   walk `GymAreaLevel` — 1, 10, 25, 50 — and read the F2 report's split melee /
   ranged / elite / modifier-bearing / boss TTK at each stop. The prediction to
   falsify is that a baseline build holds roughly constant TTK across all four.
   Loot pacing wants the same run: ~134 items/hour and zero Aberrants below area
   level 25 is a projection nobody has felt.
5. **Content that EXISTS but does not reach a player.** This list shrank
   again — the O33–O40 wave closed the Forge, ability selection, the
   legendary grant path and the commitment model.
   - **The legendary POOL is still three items** (Boots / Primary / Waist;
     O32 rules the pool grows, not the drop rate), so the organic wait stays
     ~57 hours; the dev grant button is the sanctioned playtest path.
   - **`EBreakerBuildCondition` is still movement-only** (the mask is
     hardened; the vocabulary widening waits on S4 — see item 3).
   - The Caster branch TREES are still unauthored (the class plays through
     abilities + Core only), and the Gunsmith / Tank / Support kits exist
     only as design docs — but they are no longer traps: the class screen
     gates them outside dev mode and `ChoosePermanentClassById` refuses a
     kitless lock at the component (O39).
   - **Minions and deployables do not exist in any form** (O30). The Gunsmith
     kit designs them; nothing is built.
   - **Keystone REACHABILITY beyond Swift:** five of six keystone tags are
     granted by no node and four behavioral halves are stubs — the variant
     rows resolve and do nothing, silently. Now that commitment gates the
     tier, authoring the missing keystone content is the next Swift/Caster
     depth item.
6. **Editor work only the owner can do.** THE SEAL IS GONE — all eight parapet
   cubes (`SM_Cube2/3/4/5`, `SM_Cube17/18/19/20`) were deleted on 2026-08-14,
   along with four of the twelve corner fillets. The courtyard is open. What
   remains:
   - **`bSpawnBreachRamp` still defaults `true` and nothing in the code detects
     the seal**, so the game builds the full embankment every session climbing
     a wall that no longer exists. Set it false, or keep the embankment
     deliberately as O24 ruin — but decide, rather than leaving the default to
     decide.
   - **Delete the rest of the crowding**: all eight `SM_Cylinder2-9` pillars
     (they now protrude into open air), the remaining fillets, and
     `SM_Ramp`..`SM_Ramp8` — ramps up the deleted wall, and still the yellow
     mass filling the spawn view. Highest-value remaining delete.
   - **Set a Kill Z** in World Settings to spawn minus 4000. Verified absent;
     the only kill plane is the gameplay one in `ABreakerCharacter`.
   - Decide the central plinth: keep it as a 210 cm spawn dais (the frame
     handles it) or delete it and drop the PlayerStart to about z 100.
   - The floor material is a courtyard-scale checker that reads as graph paper
     over 250 m.
   - A stale `WorldExternalActorsReferences` entry may remain from the deletion,
     pointing at a file that is not on disk. Probably harmless; an in-editor
     check, not diagnosable from the binary.
   Full derivation, the "what makes movement feel BAD" table, and the delete
   list are in `Docs/Design/Level-Design.md` §8. **Note the map is World
   Partition with external actors** — grepping the 14 KB `.umap` for actor names
   returns a confident false negative; the actors are individual `.uasset`s
   under `Content/__ExternalActors__/`.
7. **Known smaller gaps, each recorded at the code:** `DevForceClass` keeps a
   stale `ClassDefinition` after a dev swap; Slate panel-transition and
   purchase-confirm motion are unimplemented (the O37 COMMIT control's
   two-step arm/confirm is the pattern to generalize); `SetEnemyDropsLoot`
   reaches a protected `bDropsLoot` **by reflection** because adding a setter
   means editing `Combat/`, and only warns if the property disappears; the
   wave budget curve and the density caps contradict each other from about
   wave 8, so every wave past it reports unspent budget by design; the O34
   clamp warning fires per composed shot when a 3-More build opens Overdrive
   (expected-state noise — consider rate-limiting after owner sign-off); and
   **everything hover-driven across the whole front end is structurally
   unphotographable**, because the harness cannot move a mouse — node detail
   cards, the before/after projection and the discard modal have never been
   seen. (Resolved this wave: the UNMAPPED cluster — nodes carry a
   Constellation field and VELOCITY has a real plate; Bloodrhythm's
   `bCornerstone` — set, along with Overpressure and Culling, all three
   behind the O37 gate.)

## Canonical project

- Unreal project: `riors_edge.uproject` (repository root)
- Engine: Unreal Engine 5.8
- Runtime module: `RiorsEdge`
- Primary development branch: `main`
- Original imported Unreal template backup: `/Users/rior/Desktop/riors edge/riors_edge`

The Desktop copy is a backup and must not be edited. The canonical working copy is this repository.

## Current state

### The last two sessions, first — everything below this block predates them

- **O29: THE ENDGAME POWER SOURCE IS GEAR DEPTH, and it is built.** Item level
  runs to **120**, past the character cap of 50 and past the area-level ceiling.
  The affix ladder widens from T8..T-1 to **T12..T-1** and is **back-loaded**:
  geometric between the authored anchors, bent once more by
  `TierCurveExponent` 1.25, so T12→T11 is +14.8% while T2→T1 is +36.5%, and the
  top step is 2.46x the bottom relatively / 28.8x absolutely. The T0 and T-1
  spikes were re-sited 1.4x/1.8x → **2.2x/3.6x**, because against a +36.5% top
  step the old values would have been barely one more step rather than an event.
  A plain back-loaded lerp was rejected (over eleven steps it makes the bottom
  four tiers indistinguishable) and linear was rejected by O29 itself.
  **`GetDropItemLevel` no longer clamps to 50** — it is the identity across the
  whole area-level range, which is exactly what makes the two curves cancel.
  The 74x endgame gap is CLOSED, not narrowed: baseline TTK is flat to a
  thousandth from area level 50 to 100, asserted by
  `RiorsEdge.Combat.PowerCurve.EndgameComposition` (which replaced the old
  `EndgameClamp`). Every constant is O2 PLACEHOLDER.
- **The tier cap got a second slope** (owner, after playtesting O29: "the item
  level tier capping at 8 might make for awkward feeling progression, let's
  bring that to 6"). One slope of a tier per ten levels put the CHARACTER CAP at
  T8 — a third of a back-loaded ladder — so the levelling game met only the
  shallow half of a curve authored for the endgame. Now ~8.3 item levels per
  tier to ilvl 50 (T12→**T6**), ~14 after it (T6→T1).
- **MANA IS INVERTED** (owner ruling, superseding Class-Kits §2.1). The bar
  starts **full**, spends **down**, and **regenerates**:
  `PassiveRegenPerSecond` 6.0 is the primary recovery path, applied outside the
  generation budget. The re-weighting is deliberately **ONE number** — the
  global generation cap, 20.0/s → **6.0/s** — because the per-source rates carry
  the anti-Multishot 1/n ratio and several branch nodes are authored against
  their magnitudes. Fighting well now recovers at most twice as fast as standing
  still. Overcast still works: floor −20, doubled generation while negative,
  +15% incoming damage, and a cast that would breach the floor is REFUSED rather
  than truncated. Two consequences flagged and NOT retuned — see next-action 3.
- **The drop pipeline exists; kill count is no longer item count.** Before this,
  `GrantLoot` called `RollRarity` unconditionally on every death and the rarity
  table was FLAT, so a 2.5% Aberrant weight applied to a trash mob at area level
  1 exactly as it did to a boss at 50 (owner: "way way too many Aberrants at
  this item level when it shouldn't even be fundamentally possible, and every
  single enemy dropped an item" — literally true, nothing in the loot path read
  level or rank at all). `Items/BreakerDropTable.h` is three steps where there
  was one: **drop chance by rank** (trash 0.10 / elite 0.75 / modifier-bearing
  0.90 / boss 1.0), then **rarity gates**, then the weighted roll. Gate rule in
  one sentence: a rarity rolls only when the drop's item level is at or above
  its unlock AND the monster's rank is at or above its minimum, with gated-out
  weight redistributing across what remains. 692 items/hour → **134**; ~17
  Aberrants/hour at any level → **zero below area level 25** and 0.90/hour
  above. `ProjectLootRate` computes the table analytically and a test simulates
  200 hours through the real `RollDrop` and asserts the two agree, so the
  documented rate cannot drift from the shipped one. The gates were
  **deliberately NOT** rescaled to O29's 1-120 range: scaling proportionally
  would put Aberrant at ilvl 60 and Anomalous at 96, meaning a player finishes
  the entire 1-50 campaign having never seen either.
- **THE ENEMY CONTENT NOW REACHES A PLAYER.** Ten modifiers, three archetypes
  and the boss spawned nowhere; `Game/` now calls all of them. The standing
  encounter is 3 Skitter + 1 modifier-bearing elite + 2 LATTICE + 1 **SEVERED
  WARDEN** (front, because §2.4's axis is that Wardens punish frontal approach)
  + 1 **SEVERED SKIRMISHER** placed against a real cover anchor. A **cover
  registry** (21 anchors) was added because the field built hard cover and
  recorded none of it, and the Skirmisher's placement is a REQUIREMENT — in the
  open it degrades to a plain shooter with a longer telegraph than a Lattice,
  which is strictly worse. It warns rather than degrading silently. **THE FIELD
  MARSHAL** (`ABreakerBossEnemy`) reaches the arena at 17000 cm via F5,
  `Breaker.Boss`, wave 12 or `-BreakerBossOnStart`; three health-gated phases
  (Deployment 100-66%, Suppression 66-33%, Commitment 33-0%), all O2 PLACEHOLDER.
  Two findings pinned by test: `ConfigureWithModifiers` promotes rank
  unconditionally, so calling it on the arena elite would have **demoted** it
  from x3.0/x1.5 to x2.5/x1.25 — the authored rank is now captured and restored;
  and the TTK bug was worse than briefed, because `IsElite()` is `rank == Elite`
  so a Champion's `ModifierBearing` and the boss's `Boss` rank were landing in
  the **melee trash** bucket, the one bucket O18's re-anchor actually reads,
  where a single 25x kill outweighs a hundred trash kills. The report now splits
  melee trash / ranged trash / elite / modifier-bearing / boss by RANK.
- **Wave mode is SOLVED, not ramped.** `Game/BreakerWaveBudget.h` is
  Encounter-Design §4.2/4.3/5.3 as pure world-free maths. What was there was
  `4 + wave*3` capped at 24 — no budget, no archetype costs, no rest waves, no
  boss wave, no variety rule, and **24 live enemies against §5.3's ceiling of
  twelve**. Now `Budget(n) = 6 + 4n` capped at 90; Skitter 1 / Lattice 3 /
  Skirmisher 3 / Warden 6 / +4 per elite modifier; **rest waves every 6** at
  half budget with no elites; **wave 12 is the Field Marshal alone**; loot only
  on rest and boss waves; a 70% single-archetype variety rule; every §5.3 cap
  enforced. Two findings: §4.2's curve and §5.3's caps **contradict each other
  from about wave 8** (a solo wave cannot spend past the mid thirties while the
  curve climbs to 90 — the caps win, and the solver REPORTS the shortfall rather
  than choosing silently), and **12 is a multiple of both the rest and boss
  intervals**, so checking rest first deletes the boss wave. The order is
  load-bearing and tested.
- **Quest flags survive a crash, and a quest is a LENS OVER FLAGS.**
  `AddQuestFlag` wrote into a bare non-`UPROPERTY` array and the only callers of
  `SaveGameState` were EndPlay, class lock and three menu commit points — so a
  flag earned in dialogue reached disk only on a clean shutdown. `SetFlag` now
  requests a save whenever the set actually changes, through a delegate so the
  journal stays world-free. `ComputeQuestState` is a **pure function of the flag
  set**; nothing about a quest is serialized and the save gained exactly one
  field, which is what keeps the format from forking. **`SaveVersion` is finally
  read**: version 2, migrating one step at a time, **refusing a NEWER file**
  rather than repairing it, preserving unknown flags verbatim. Dialogue is
  gated by `RequiredFlags`/`BlockedByFlags` on entries, choices and nodes. The
  flag registry is deliberately NOT GameplayTags — a tag cannot be added by a
  save file and must survive a build that no longer declares it.
- **The HUD gained a minimap and an absorbed-damage read.** Minimap top-right,
  320x176, **landscape and field-aligned rather than square and rotating**,
  because the field is a 25000 cm long axis against ~8000 of width; 56 cm/px
  puts the encounter pocket on the map from the safe ring. It iterates nothing —
  it fills a member blip array inside the loop that already walked every enemy.
  **AUTHORED, not transcribed**: no minimap exists on the owner's design canvas,
  so every decision in it is a proposal awaiting a ruling. Absorbed damage was
  the item worth the most: a Warden's frontal armour made a hit register with no
  health movement, which reads as a broken game rather than a wrong angle. It
  needed nothing new in `Combat/` — `FBreakerDamageResult` already carries
  `RawDamage` and `MitigatedDamage`. Two reads at 20% mitigation: the hit
  marker's ticks close into corner brackets (**geometry, not a third colour**,
  so it cannot be confused with the gold weak-point tick) and the floating
  number recedes to muted with an `ABSORBED -N%` caption.
- **Damage numbers were too WIDE, not too large** (second owner report). They
  had already been cut ~35%; what changed between the two reports was **O29**,
  not the type — ilvl 120 and doubled affix values turned three-digit hits into
  six-digit ones, and `FormatTicker`'s thin space makes six digits eight glyphs.
  Sizes were **held** (they are the only thing separating body from weak point
  from crit) and `BreakerUI::FormatDamage` abbreviates above 10 000, holding
  every number to five glyphs at any magnitude the power curve can produce.
- **The skill board is a VIEWPORT with zoom and pan.** Two nested scroll boxes
  were fighting for one wheel gesture (the likeliest reading of the owner's
  "scrolling is off by a little bit"); both are gone. Wheel zooms about the
  cursor 0.5x-2.0x, drag pans, RESET VIEW returns to the opening zoom, and both
  survive a purchase rebuild. Zoom is a **render transform set imperatively from
  input handlers**, never a rebuild, so it cannot become the per-frame trap this
  file warns about. Boards open at **1:1 deliberately**: fit-to-width was
  implemented, photographed and reverted, because COMPARE ALL is ~2600px in a
  ~1300px column and fitting means 0.5x — 5px type against FIELDPLATE's 11px
  floor. **The clipped rank numbers were a different defect entirely**: `SButton`
  centres its child at the child's DESIRED width, an `STextBlock`'s desired width
  is its MEASURED width, and Slate clips the drawn run to that same box —
  measuring and rasterising round independently. A previous pass read it as "the
  box is too small" and moved 30px→36px, which only reshuffled the rounding.
- **Swift's third jump is REACHABLE** (`SwiftThirdJumpUnlockLevel` 20 → 1) and
  the **descent** was eased (`FallGravityMultiplier` 1.80 → 1.55, `GravityScale`
  deliberately untouched at 1.38 — the four gravity reports are about different
  halves of the arc, and ONE value moves per report so the next one attributes).
  Apex is unchanged by construction at 181 cm, so no ledge, gap or wall-ride
  approach in the field changes reach.
- **O30 and O31 are ruled and scouted, not built.** O30 opens the Core tree to
  redesign around build axes (guns / abilities / minions); several axes need
  `EBreakerBuildCondition` widened past movement-only first, and minions do not
  exist in any form. O31 sets the content shape — Destiny × PoE, raids as
  puzzles rewarded for team play, and **no encounter may have a build that
  cannot participate**.
- **O32 distinguishes legendary from Anomalous**, and the distinction is real in
  code. **Anomalous is a RARITY** — the fifth tier, gating affix count and tier
  ceiling and carrying one ROLLED rule rewrite from a generic pool of four.
  **Legendary is a separate field** (`FBreakerItemInstance::LegendaryId`) naming
  a specific authored item with a fixed slot, guaranteed affixes and a
  HAND-AUTHORED rule; `RollLegendary` forces the rarity to Anomalous. Every
  legendary is Anomalous; **most Anomalous drops are not legendaries**. The
  ruling is that the drop rate holds and the POOL grows — the ~57 hours per
  legendary at area level 50 is mostly an artefact of three legendaries covering
  three of eight slots, and the same arithmetic with a full set gives ~21 hours.
  The pool is **still three** (Boots / Primary / Waist); O32 is recorded and not
  yet built.
- **The level is no longer a sealed box** — the owner deleted all eight parapet
  cubes. See next-action 6 for what that leaves open.

### Everything below predates the two sessions above

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
- THE GROUND WAS COPLANAR WITH ITSELF (owner: "a lot of the textures on the
  ground were tearing"), and it was found and fixed BY LOOKING — a new
  grazing-angle capture vantage, because the defect is invisible from every
  vantage the harness already had. Three populations, all on the apron whose
  top face is exactly the probed ground plane: the 200 tint patches were placed
  by rejection-free random sampling at ONE fixed height, so overlapping pairs
  shared a surface exactly; each patch was a 4 cm-thick cube that CAST A SHADOW,
  and at 150-200 m both that shadow and the lip's own shaded side face are
  sub-pixel and alias into a stippled dashed line tracing every patch outline
  (the visible majority of the report); and the jump-gap trench floor was
  authored with `TopZ = 0.0f`, which is the apron's own top, over the whole
  trench. Now: overlapping placements are REJECTED against the rotated
  footprint (196 placed from 420 attempts, density unchanged, double-tinted
  blotches gone as a bonus), patches are shadowless PLANES with no lip at all,
  and the trench floor sits on the new `GroundOverlayLift` (EditAnywhere, O2
  PLACEHOLDER 6 cm — the rule is that anything laid ON the apron is separated
  from it; 0 reproduces the bug for an A/B). The template `Floor` is not
  involved: the apron is authored as the rectangle around it and the runtime
  log confirms the frame is axis-aligned at the origin. Before/after captures
  read honestly: the dashed seams are gone. Full derivation in
  `Docs/Design/Level-Design.md` §6.5.
- A parallel design sprint produced nine docs under `Docs/Design/` (class kits, constellations, XP/pacing, encounters, game modes, UI/UX, save architecture, art plan, synthesis overview). `Design-Overview.md` §7 is the ranked owner-decision list; consult it before authoring content in any of those domains.
- `UBreakerStatusComponent` runs snapshot Bleed/Poison DoTs with stack caps; gym enemies grant rolled loot to the player's backpack on death.
- Five weapon archetypes exist (Rifle, SMG, Sniper, Shotgun, Rocket); Rocket is a replicated projectile (`ABreakerRocketProjectile`) with radial falloff damage that ignores its instigator pending the self-damage design pass. Loadout slots carry assignable archetypes (`SetSlotArchetype`), picked from the loadout screen.
- MOVEMENT IS CONFORMED TO THE ONE-ADDITIVE-BUCKET RULE, and it is a FELT
  change. `UBreakerCharacterMovementComponent` used to multiply the gear
  multiplier by the tree multiplier for move speed, slide speed, air control and
  dash cooldown, so +20/+20 read x1.44 against a locked x1.40 — the last
  instance of the bug class the damage pass fixed. `EBreakerAggregatedAttribute`
  gained `SlideSpeedMultiplier`, `AirControlMultiplier` and
  `DashCooldownReduction` (base 1.0, replicated, both layers bid raw
  percentages); the movement component now reads the composed ATTRIBUTES
  (`GetComposedMoveSpeedMultiplier()` and friends) and the private
  `GearMoveSpeedMultiplier()` helpers are deleted. The composed `MoveSpeed`
  attribute, previously written by the aggregator and read by NOBODY, is its
  first consumer: the movement component publishes its EditAnywhere `WalkSpeed`
  as that attribute's base (`SetAggregatedAttributeBase`) and reads back
  composed/base, so the attribute genuinely is the character's walk speed
  instead of a stale 650 constant. Sprint at +20/+20 goes 1584 -> 1540 cm/s
  (-2.8%); one-layer-only builds are bit-identical. Dash cooldown is unchanged
  today because no node target authors it — the attribute exists so the first
  one that does is additive from day one. Before/after table in
  `Docs/Movement-Design.md`. Never playtested.
- SWIFT'S THIRD JUMP EXISTS (O25). Two jumps stay base kit for every class
  (`BaseJumpCount`, written onto `ACharacter::JumpMaxCount` at runtime); Swift
  alone gets a third, gated on `SwiftThirdJumpUnlockLevel` (EditAnywhere,
  **O2 PLACEHOLDER 1, free** — the threshold is still an open owner ruling; the
  MECHANISM is what shipped). It was 20, which made the whole feature
  UNREACHABLE BY CONSTRUCTION — nothing writes `CharacterLevel` — and the only
  symptom was the owner saying "i never could do a 3rd jump". The grant reads the permanent class from
  `UBreakerProgressionComponent`, recomputes on `OnProgressionChanged` AND polls
  the live state from the tick as a backstop (the Mana component's pattern), and
  a swap away from Swift returns two jumps and clamps `JumpCurrentCount`
  immediately. The third jump is a course correction, not a repeat: it rotates
  horizontal velocity partway onto input with magnitude preserved exactly
  (`SwiftThirdJumpRedirectAlpha`, O2 PLACEHOLDER 0.55), vertical untouched.
  Covered by `RiorsEdge.Movement.JumpGrant` (the RULE) and
  `RiorsEdge.Movement.JumpGrantMatrix` (the SHIPPED CONFIGURATION, asserted
  against a default-constructed progression state — the state the game actually
  runs in. JumpGrant alone passed for the entire time the feature was
  unreachable, because it fed the rule a level the game cannot produce; that is
  the gap the second test closes), plus
  `RiorsEdge.Movement.AdditiveComposition` and `RiorsEdge.Movement.ComposedAttributes`.
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
- THE OTHER HALF OF THE RATIO IS BUILT. `Weapons/BreakerWeaponMath.{h,cpp}` is
  Power-Curve §3: `WeaponBase(ilvl) = ArchetypeBase * (1 + w)^(ilvl - 1)`, with
  `w` = `UBreakerWeaponComponent::ItemLevelDamageGrowth` (0.09, EditAnywhere,
  O2 PLACEHOLDER) deliberately tracking the monster health growth `g` = 0.09 so
  a BASELINE build holds a roughly constant TTK across the whole game and every
  bit of felt progression comes from the multiplier band instead. Before this,
  `Weapons/` contained no reference to `ItemLevel` at all — base damage was an
  archetype constant, so an ilvl 1 weapon and an ilvl 50 weapon hit identically
  and item level moved only affix tier values. `GetEquippedItemLevel()` reads
  the equipped item for the active slot and falls back to `UnequippedItemLevel`
  for the fallback weapon; the scalar is exactly 1.0 at ilvl 1 for any growth,
  so the curve is opt-in by content rather than a silent retune. The two curves
  now compose: monster health and weapon base damage climb together, and the
  build variance band (Power-Curve §4) is what separates a baseline character
  from an optimized one. Every constant is O2 PLACEHOLDER and NOT PLAYTESTED.
- The gym encounter includes one elite (`ConfigureElite`): 1.25x scale, the
  elite rank chassis, slower implacable advance, drops never below Exceptional.
- Inventory backpack sorts best-rarity-first with per-slot filter chips and auto-height cards.
- Weapons have a mechanical feel layer (`Weapons/BreakerWeaponFeel.h`, pure maths so it is unit-testable): per-archetype recoil with a learnable pattern and a settle that lands on exactly zero, player compensation credited against the recovery budget, ADS tightening four axes, first-shot accuracy with bloom, and a substepped viewmodel spring. The round goes where the crosshair was when fired; the kick moves the aim for the NEXT one, and that invariant is tested. Weak points carry a world-space forgiveness halo (`WeakPointToleranceCm`, 14 cm) after a geometry bug left the bottom of the head unhittable from the front. Falloff is softened per archetype with the ORDERING pinned by test rather than the values. ADS pays aim-in time and a movement spread penalty, so hip fire is the mobile close option rather than strictly worse; it has no ADS movement-speed penalty yet because `Movement/` has no aim awareness.
- `ABreakerRangedEnemy` (LATTICE) holds a 9-19 m band, strafes while firing, has no contact attack, and throws a real replicated projectile at 1100 cm/s against a 950 cm/s sprint with a 0.85 s telegraph and 0.35 lead — enough that holding a lane is punished and any direction change beats it. It SUBCLASSES `ABreakerEnemy`, so loot, waves, health bars and TTK sampling work unchanged; it declares itself through `IsRangedForTelemetry()` so its kills get their own TTK bucket instead of polluting the melee average.
- Rounds are drawn by `ABreakerTracerRenderer` (UI/), a pooled client-side world actor — 12 tracers plus 24 impact sparks allocated once and recycled, nothing spawned per bullet. Additive unlit material so it still depth-tests against the world, which the previous canvas approach could not. The shotgun deliberately draws no streak, because the shot result carries one impact for a whole spread.
- Movement carries weight without floatiness: gravity 1.38 on the rise (eased twice from playtest), a **1.55x** fall multiplier (eased once, from 1.80, for the "slightly more floaty" report — the rise and the fall are tuned separately and ONE value moves per report, so the next one attributes), a blended apex band, terminal velocity, variable jump height, a landing speed cost, tighter braking. Wall ride was BROKEN — its entry gate equalled walk speed and is read after wall contact, so it never entered at any angle — now 450 with a regression test. Dash broadcasts `OnDashStarted` for an FOV punch scaled by speed and a direction-signed camera roll.
- Damage scaling is real and unified: `EBreakerNodeStatTarget::Damage` exists, `DamageMultiplier` is an aggregated attribute rather than a permanently-1.0 constant, and gear plus tree plus a per-spent-point baseline (`IncreasedDamagePerSpentPoint`, EditAnywhere, zeroable) all land in ONE additive Increased bucket. Before this, a skill node was structurally incapable of raising weapon damage.
- The skill matrix sizes from the measured viewport once per rebuild, scrolls in both axes, and leads each node card with what a point BUYS (`DAMAGE 1.06x -> 1.10x`) projected through a copy of the live aggregator so it cannot drift from the real numbers. A branch strip browses subclasses; committing to one does not exist in the data model.
- The FIELDPLATE UI system is implemented. `Source/RiorsEdge/UI/BreakerUIStyle.h` is the single token header (sRGB palette, rarity ramp, 4/8/16/24/40/64 spacing scale, rail and border widths, type scale, HUD geometry) and both the canvas HUD and the Slate front end read from it — a colour authored twice is a bug. `ABreakerPlaytestHUD` is rebuilt to `Docs/Design/UI-HUD-Spec.md`: one 440x184 bottom-right cluster on a 3px orange rail, notched momentum track whose block texture changes per state, 56px ability squares with ready/window/cooldown-wedge/unaffordable states, 420-wide vitals plate with fixed 84px value column and armour chips, top-centre wave banner, spec'd damage-number scale with cluster stacking, 180x8 enemy bars, and the violet ultimate frame with edge bands and step-down. All HUD geometry is authored in the spec's 1080p pixels and scaled by `ViewportHeight/1080`. Two known gaps, both content not code: the three OFL faces (Saira Condensed / Barlow / JetBrains Mono) are not imported, so the type *scale* is honoured and the *faces* are not; and no ability glyphs exist, so each square falls back to the ability's short name in its state colour.

## Verification status

The `RiorsEdgeEditor` Development target compiles and links successfully on Apple Silicon and Win64 with Unreal Engine 5.8.

**The suite is 244 tests, all passing, and GREEN IS MEANINGFUL AGAIN.** The
former deliberate reds were ruled by O36 and split into pinned fixtures
(`PowerBand.AtCap` 8.0–10.0, `PowerBand.Endgame` 12.0–20.0 seed rails,
`RuleBandImpact` at the re-anchored 1.5x PROLIFIC ceiling), so any
`Result={Fail}` is a regression. Run headless with
`UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi`, then grep the LOG FILE
(`Saved/Logs/riors_edge.log` — the results do not reach stdout) for
`Result={Fail}`. One workflow scar from this session: piping build output
through `tail` swallows the exit code — a build "completing" is not a build
succeeding; check for `Result: Succeeded` or the exit code directly.

Two lessons from this suite are worth carrying. `PowerBand` **had never
actually run**: UE's automation tree cannot hold a leaf test at a node that is
also a parent, so `PowerBand.RuleImpact` silently swallowed `PowerBand` and the
8-10x assertion was not enumerated for the whole time it existed. And
`RiorsEdge.Movement.JumpGrant` passed for the entire life of a feature that was
unreachable in the shipped configuration, because it proved the RULE against a
level the game cannot produce — which is why `JumpGrantMatrix` now asserts the
shipped configuration against a default-constructed state.

NOTE: builds fail with "Live Coding is active" while the editor is open — close it (or Ctrl+Alt+F11 in-editor) before running Build.bat. A live Windows startup loads `Lvl_FirstPerson`, selects `BreakerGameMode`, uses the `BP_BreakerCharacter` C++ child, opens on the title menu, and exposes the Playtest Gym HUD, targets, enemies, two-slot weapon loadout, reset, report, diagnostics, pause, settings, and loadout controls. Generated build folders are intentionally ignored by Git.

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
  Tests/          automation tests (RiorsEdge.* filter)
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

## Session workflow facts (read before working)

- Build: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RiorsEdgeEditor Win64 Development -Project="<repo>\riors_edge.uproject" -WaitMutex`. Fails while the editor is open (Live Coding lock).
- Tests: headless `UnrealEditor-Cmd.exe` with `-ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi`; **213 of 215 pass and 2 fail deliberately** (see Verification status). NOTE: when the owner has the MAIN tree editor open, an agent building in a separate worktree hits a false-positive Live Coding lock (the guard keys off the shared UnrealEditor.exe, not the project DLL); `-NoHotReloadFromIDE` is the correct override in that case only.
- Authority chain for design questions: `Docs/Design/Decisions.md` (append-only O-ledger) supersedes everything; then Design-Overview.md; then the per-domain docs; then `Docs/Design/Master-Sheet-Import.txt`. O2 freezes value authoring — placeholders must be flagged `O2 PLACEHOLDER`.
- `Docs/Playtest-Feedback-Log.md` records every owner playtest and the responses; append per session.
- The owner works in short playtest loops: expect to build/fix while the editor is closed, relaunch it for them, and push to origin/main after a clean run. **"Clean" means 213 of 215, with `PowerBand` and `RuleBandImpact` the only failures** — do not treat their names appearing in the log as a regression, and do not silence them. (Do not pin a commit hash here; it goes stale within a session. `git log` is the source of truth.)
- Zero-setup convention: all content (weapons, affixes, trees, abilities, class defs) has C++ fallback registries so a clean clone plays with no assets; Data Assets replace them later one-for-one.
- IMPORTANT UI lesson: never use SWrapBox with UseAllottedSize inside a scroll box (layout oscillation), and never poll input/rebuild widgets from per-frame Text_Lambda attributes — both caused owner-visible bugs.

## Working documents

- `README.md` — routing table and the authority chain (O28)
- `Docs/Setup.md` — Mac/Windows onboarding and editor integration
- `Docs/Architecture.md` — ownership boundaries
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
- `Docs/Design/` — the design corpus: Decisions.md is the append-only rulings ledger (read first); Design-Overview.md maps the space; per-domain docs cover classes (Class-Kits + Gunsmith/Tank/Support), constellations, XP/pacing, encounters, game modes, UI/UX, save architecture, art plan, damage pipeline, and the ability implementation spec. There are **seven** `UI-*.md` files, five of them the FIELDPLATE visual authority: `UI-Style-Guide-Fieldplate.md` (palette, type scale, shape, motion, and the two recurring Slate defect classes), plus the HUD, Inventory, Skill Tree and Ability Icon specs, each carrying its own implementation-status section recording exactly what is built, what has been VERIFIED BY LOOKING, and what the harness structurally cannot check. The other two are `UI-UX-Spec.md` (the broader UX domain doc) and `UI-Generation-Prompts.md` (prompts used to author the mocks)

## Handoff discipline

Before ending substantial work:

1. Build or otherwise verify changes in proportion to risk.
2. Report anything that was not tested.
3. Update this file if current state, constraints, paths, or recommended next actions changed.
4. Keep unfinished work explicit; never imply that editor assets or gameplay behavior exist merely because a C++ interface compiles.
