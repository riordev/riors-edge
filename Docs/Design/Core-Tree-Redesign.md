# Core Tree Redesign — scouting the build axes (O30)

> STATUS 2026-08-16: UNBUILT TREATMENT — an O30 scouting document; the redesign has not begun and is blocked on the defence/mobility axis question (:770), which only the owner can answer.

**Scope:** post-slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

> **CORRECTION, 2026-08-14: the headline count in §1.1 was wrong by six.** The
> live Core roster is **seven clusters and 30 nodes**, not 24. The §1.1 table
> itself has always been right — its rows sum to 4+5+2+3+4+6+6 = 30 — and 24 is
> the total *without* the six Elements nodes, which is what the roster looked
> like before the Elements cluster was authored. Every "24" in this document is
> corrected below and marked. The count was re-derived by listing every
> `Tree->Nodes.Add` call in `GetCoreSliceTree()`. **This matters beyond
> bookkeeping: the "24" figure was quoted onward into
> `Core-Constellations.md`'s O30 banner and into at least one work brief, so
> the error had already propagated twice before it was caught.**

**Status: SCOUTING. Nothing here is implemented and nothing here is ruled.**
This document costs O30's proposal; it does not execute it. Every recommendation
is overrulable and the open questions at the end are genuinely open.

**Label key**, used on every claim so the owner knows what they are free to
overrule:

- **TRANSCRIBED** — copied from a ruling, a design document, or read directly
  out of the code. Not an opinion.
- **AUTHORED** — this document's own invention. New. Overrule freely.
- **RECONCILED** — an existing thing restated after checking it against the
  code or another document, where the two disagreed and this document picked.

**What O30 asks for** (TRANSCRIBED, Decisions.md O30): the Core tree reorganised
around the axes a build is actually built on — GUNS (ailment/element, poison,
bleed, flat damage, crit, fire rate, movement), ABILITIES (stacking, multispell,
cooldown reduction, AoE, poison, bleed, flat damage, crit), MINIONS (drones,
turrets, deployables). Weapon archetypes should fit some axes more naturally
than others. Three knobs become several; subclasses solidify identities the axes
create rather than introducing separate ones.

**The constraints that shape every answer below** (TRANSCRIBED): O3 (at most
three composed More multipliers per build, clamped in the aggregator), O27
(choices beat accumulation), O15 (branch nodes mix freely, no exclusive tiers),
O2 (every number is a placeholder), O31 (every build must be able to make an
impact in every encounter).

---

## 1. What the Core tree actually is today

### 1.1 The roster is not what the design document says it is

**RECONCILED.** `Docs/Design/Core-Constellations.md` describes **six**
constellations of **11 nodes / 26 points** each, of which five ship. The live
roster in `Source/RiorsEdge/Progression/BreakerProgressionLibrary.cpp`
(`GetCoreSliceTree`, lines 158-461) is **seven clusters and 30 nodes**
(**CORRECTED** from 24 — see the banner at the top; the table below always
summed to 30), none of them 11 nodes deep:

| Cluster | Live nodes | Where |
|---|---|---|
| Precision | 4 — Sightline, Tunnel Vision, Called Shot, Fixate | L166-200 |
| Volley | 5 — Trigger Discipline, Cyclic, Last Round, Salvo, Barrage | L203-246 |
| Affliction | **2** — Open Wound, Deepen | L249-258 |
| Bulwark | 3 — Set Stance, Read, Parry | L261-282 |
| Kinesis | 4 — Light Footing, Loft, Phantom Step, Air Jump | L285-310 |
| **Velocity** | 6 — Freefall, Slipstream, Traction, Afterburn, Terminal Velocity, Redline Doctrine | L321-371 |
| Elements | 6 — Conductive, Charge Up, Threshold, Catalyst, Penetrance, Reaction Chain | L404-452 |

**Velocity is not in `Core-Constellations.md` at all.** It was added under O27 to
give the movement pillar an offensive expression, and it is the six nodes the
skill-tree board currently draws in its `UNMAPPED` catch-all cluster
(`Source/RiorsEdge/UI/BreakerMenu.cpp` L3267-3278; the cluster table at L3246-3251
knows only six prefixes and `Core.Velocity.` is not one of them). CONTEXT.md
records this as a known gap. It matters for O30 because Velocity is the *only*
existing cluster organised the way O30 wants the whole tree organised — around a
build axis (movement state) rather than a fantasy.

### 1.2 Which of O30's axes each cluster already serves

**RECONCILED** — derived from the authored `AddEffect` calls, not from the
constellation themes.

| Cluster | Axis it serves today, in code | Axis its DESIGN doc claims |
|---|---|---|
| Precision | crit (flat CriticalChance + CriticalDamage), flat Increased damage | crit, weak points, execution |
| Volley | flat Increased damage (Salvo, Cyclic), one unconditional More | fire rate, magazine, ricochet |
| Affliction | damage-over-time (one node, `DamageOverTime` Increased) | bleed/poison application, spread |
| Bulwark | defence (BlockChance, Health) + the Parry verb | armour, mitigation, parry |
| Kinesis | defence/mobility (DodgeChance, MoveSpeed, AirControl) + the Air Jump verb | evasion quality, aerial |
| Velocity | **movement-conditional damage** — four conditional Increased ladders and two conditional Mores | (not in the doc) |
| Elements | damage-over-time and crit, wearing an elemental costume | buildup, reactions, conduction |

The gap between the two columns is the honest state of the tree: **the design
document's fantasies are mostly carried as node NAMES and tags, and the thing
that reaches gameplay is one of five stat targets** — Damage, DamageOverTime,
CriticalChance, CriticalDamage, and a handful of defensive/movement stats. Volley
is named after fire rate and authors none. Affliction is named after bleed and
poison and authors neither — it authors generic DoT scaling. Elements is
explicitly authored in a "physical-only pre-resistance form" (the block comment
at L373-403 says so plainly) and pays in DoT and crit.

**This is not a criticism of the authoring.** Each of those decisions was made
deliberately and documented at the code, precisely to avoid shipping a node that
does nothing. But it means O30's premise is already half-true by accident: the
tree is *already* an axis tree wearing constellation names. The redesign is
mostly a matter of admitting it and then filling the axes that have no home.

### 1.3 Which axes have no home at all

**TRANSCRIBED from the roster.** Of O30's taxonomy:

- **Fire rate** — named by Volley, authored by nobody. (The attribute exists; see §2.)
- **Poison** — no node anywhere targets poison specifically.
- **Bleed** — no node anywhere targets bleed specifically. Open Wound *forces*
  a Bleed application as a tag; nothing scales bleed.
- **Ailment / element** — no node; the whole Elements cluster is a placeholder
  for it.
- **Every ABILITIES axis** — stacking, multispell, cooldown reduction, AoE,
  ability crit, ability flat damage. Zero nodes. The Core tree does not know
  abilities exist.
- **Every MINIONS axis** — zero nodes, and zero gameplay (§6).

Axes that DO have a home: crit (Precision), flat/Increased damage (Volley,
Precision, Elements), movement (Velocity, Kinesis).

### 1.4 What survives a redesign unchanged

**AUTHORED assessment.** A redesign that throws away working content is more
expensive than it looks, so this is the salvage list. "Survives unchanged" means
the node's *effect authoring* is portable to any new structure with only its
`NodeId` prefix changed.

| Survives as-is | Count | Why |
|---|---|---|
| All six Velocity nodes | 6 | Already axis-shaped: a condition and a payout. Movement axis, done. |
| Precision's four | 4 | Crit axis, already pure. Sightline/Called Shot are chance, Tunnel Vision is multiplier, Fixate is the More. |
| Volley's Salvo, Cyclic, Barrage | 3 | Flat/Increased damage axis. Cyclic's *name* is fire rate; its *effect* is generic damage, so it re-homes cleanly either way. |
| Affliction's two, Elements' six | 8 | All eight are DoT-or-crit ladders. They re-home onto an ailment axis intact — but see §2, they scale a DoT nobody but the SMG applies. |
| Bulwark's three, Kinesis' four | 7 | Defence and mobility. **O30's taxonomy has no defensive axis at all.** See Open Question 1. |
| Volley's Trigger Discipline, Last Round | 2 | Tag-only. They survive because they do nothing today either. |

**CORRECTED: 30 live nodes, not 24** — and the table above accounts for all
thirty, every row of it marked "survives as-is". **The "21 survive" figure that
stood here does not follow from the table and is left unrecomputed.** It is
reachable as 30 − 7 (Bulwark + Kinesis, homeless in O30's taxonomy) − 2
(Volley's two tag-only nodes), but that reading contradicts the very next
clause, which counts those same 7 as being *among* the survivors. Rather than
pick a derivation and present it as the finding, both readings are recorded:

- **30 nodes have portable effect authoring** (the table's own claim), of which
- **7 (Bulwark + Kinesis) have nowhere to go in O30's taxonomy as stated** —
  the taxonomy has no defensive axis at all (Open Question 1), and
- **4 are inert or mis-named today** (§1.6), so re-homing them re-homes a debt.

The conclusion the section was drawing survives either way: the expensive part
of the redesign is not the existing content. It is the axes with no consumer
(§2) and the minion system (§6).

### 1.5 The cost basis nobody should forget

**TRANSCRIBED, `Docs/Design/Power-Curve.md` implementation status.** The
measured build variance band is **8.74x** composed from four layers (flat 1.16x,
additive Increased 2.35x, More 1.93x, effective crit 1.66x), and O3 was not
broken to reach it. Any redesign has to land inside that band. More axes must not
mean more multiplier layers — see §4.4.

### 1.6 Three things in the live tree that already do nothing

**TRANSCRIBED**, and they matter because a redesign inherits them:

1. **Open Wound's tag is read by nobody.** The Affliction gateway
   (`BreakerProgressionLibrary.cpp:249-252`) grants `Node_OpenWound` and its
   description promises "weak-point hits apply Bleed regardless of chance". No
   code reads that tag; it is listed as unimplemented in
   `Abilities/BreakerAbilityStateComponent.h:49`. The gateway node of the
   bleed constellation does not touch bleed.
2. **Deepen's stack half does nothing.** Its description says "stacks deeper";
   it authors only `DamageOverTime` Increased. `SetStackCapDelta` exists on
   `UBreakerStatusComponent` and has no caller outside `Tests/`.
3. **Volley's Trigger Discipline and Last Round are tag-only.** They author no
   effect at all. Cyclic's fire-rate ramp is likewise a tag nothing consumes;
   the code comment at L211-214 says so.

None of these is a scandal — each is documented at the code — but four of the
tree's 30 nodes are currently either inert or paying in a currency their name
does not describe. **A redesign that re-themes the tree without wiring these
carries the same debt under new names.**

---

## 2. The axis map — does a live consumer exist?

**Why this section exists** (TRANSCRIBED, CONTEXT.md): this project has shipped
`EBreakerNodeStatTarget` with no damage entry, so a skill node was structurally
incapable of raising damage, and an affix pool where four of eight slots could
not raise damage at all. The standard now is that an axis without a consumer
gets **named as needing one**, not quietly authored. Every row below names the
attribute, component, or system a node on that axis would have to read.

**Two plumbing facts that govern the whole table** (TRANSCRIBED):

- `EBreakerNodeStatTarget` has exactly **eleven** entries — CriticalChance,
  CriticalDamage, MoveSpeed, SlideSpeed, AirControl, DodgeChance, BlockChance,
  Health, DamageOverTime, Damage, Count
  (`Progression/BreakerProgressionTypes.h:38-59`). **A skill node cannot author
  anything outside that list.** Not fire rate, not cooldown, not AoE, not
  armour, not a stack cap.
- `EBreakerAggregatedAttribute` has more entries than the node enum does —
  including `FireRateMultiplier`, `Armor`, `DashCooldownReduction` and
  `ResourceCostMultiplier` (`Attributes/BreakerAttributeAggregation.h:24-88`).
  Three of those are **gear-only** because no node target maps to them, and one
  (`ResourceCostMultiplier`) is written by gear and **read by nobody** — the
  Resource Efficiency affix is currently a line of text.

### 2.1 GUNS

| Axis | Live consumer? | What a node would have to read | Verdict |
|---|---|---|---|
| **Flat damage** | **YES** | `EBreakerNodeStatTarget::Damage` → `DamageMultiplier` → `BreakerWeaponComponent.cpp:1273,1383` → `BreakerDamageLibrary.cpp:31` | Authorable today, in both the Flat and Increased lanes. |
| **Crit** | **YES** | `CriticalChance` / `CriticalDamage` → `ResolveDamage:41,44` | Authorable today. The healthiest axis in the project. |
| **Fire rate** | **ATTRIBUTE YES, NODE NO** | `FireRateMultiplier` (`AttributeSet.h:81`) → `GetEffectiveRoundsPerMinute` (`BreakerWeaponComponent.cpp:996-1000`). Gear bids through `EBreakerStatTarget::FireRate`; **no node target exists** | **One enum row from live.** Append `EBreakerNodeStatTarget::FireRate`, map it in `AggregateStats`. Cheapest axis on the board. |
| **Movement** | **YES** | `MoveSpeed`, `SlideSpeedMultiplier`, `AirControlMultiplier` node targets, plus the six `EBreakerBuildCondition` states | Authorable today, and already authored — this is what Velocity is. |
| **Bleed** | **PARTIAL — magnitude only** | A node can move only `DamageOverTimeMultiplier`, which scales *every* DoT identically. `BleedChance`, `BleedDamagePerTick`, `BleedDuration`, `BleedTickInterval` are authored constants on `UBreakerWeaponDefinition` (`BreakerWeaponDefinition.h:67-70`) with **no modifier plumbing at all** | A "bleed" node is a lie unless it means "all DoT". Needs a per-status split. |
| **Poison** | **PARTIAL — same bucket as bleed** | Poison exists only as ability constants (`BreakerAbility_Rot.h:51-53`), applied with **no chance roll**. `UBreakerStatusComponent` does not special-case either status — Bleed and Poison differ only by `StatusTag` | **A poison node and a bleed node authored today would be the same node.** That is the single most important fact in this table. |
| **Ailment / element** | **NO** | `EBreakerDamageFamily::Elemental` exists (`Combat/BreakerCombatTypes.h:12`) but **nothing scales by family**; `ElementalDamageReduction` is marked RESERVED, NOT LIVE (`Items/BreakerItemTypes.h:61-67`) | **Fully inert.** Needs the whole Elements chain: a resistance step in `ResolveDamage` (Damage-Pipeline §1 step 6 — its own §7 confirms it is unimplemented), per-element resist attributes, a buildup track, a reaction matrix. |

### 2.2 ABILITIES

| Axis | Live consumer? | What a node would have to read | Verdict |
|---|---|---|---|
| **Ability flat damage** | **SHARED, NOT SEPARATE** | Every damaging ability reads the **same** `DamageMultiplier` weapons read (`_Cleave.cpp:122,178`, `_Rot.cpp:87,100`, `_Fracture.cpp:54,86`, `_Siphon.cpp:159`, `_Resonance.cpp:93`) | An "ability damage" node today is indistinguishable from a gun damage node. Needs a second attribute plus a family/tag test at the damage request. |
| **Ability crit** | **SHARED, NOT SEPARATE** | Same — abilities read the shooter's `CriticalChance`/`CriticalMultiplier` | Same fix, same cost. |
| **Cooldown reduction** | **NO** | `ApplyCooldown` sets the GE magnitude straight from `Definition->CooldownSeconds` with no attribute read (`BreakerGameplayAbility.cpp:137-154`). Only `DashCooldownReduction` exists, and only for the dash | **Inert.** Note also that only two abilities in the game carry a non-zero cooldown, and Caster's kit rules that Caster abilities never have one — so this axis is narrow *by design*, not merely unbuilt. |
| **AoE / area** | **NO** | Radii are `EditDefaultsOnly` constants (`BreakerAbility_Rot.h:37`, `RadiusCm = 400.0f`). No attribute, node target, affix target or item rule touches any radius | **Inert.** Needs an area attribute and every ability rewritten to read it. |
| **Stacking** | **NO** | `FBreakerActiveStatus::Stacks` and `StackCapDelta` exist (`BreakerStatusComponent.h:98,112`), but `SetStackCapDelta` has **no caller outside `Tests/`** | **Inert, and already being promised** (see §1.6). Wiring is one node target plus one call site. |
| **Multispell / projectile count** | **NO** | `PelletsPerShot` is an authored constant read at `BreakerWeaponComponent.cpp:1213`; nothing modifies it. No multi-cast concept exists | **Inert.** The proc-coefficient law (Damage-Pipeline §3) exists specifically to bound this axis and has **no enforcement written yet**. |
| **Ability poison / bleed** | **PARTIAL** | Same shared `DamageOverTimeMultiplier` | Same fix as GUNS. |

### 2.3 MINIONS

| Axis | Live consumer? | Verdict |
|---|---|---|
| Drones / turrets / deployables | **NO — nothing exists in any form** | The only spawner in the project is the boss's adds (`BreakerBossEnemy.cpp:381-403`), enemy-side and unreachable by any player stat. Full cost in §6. |

### 2.4 The condition enum is the second bottleneck

**TRANSCRIBED.** `EBreakerBuildCondition`
(`Progression/BreakerBuildConditions.h:24-43`) has six entries — `Always`,
`Airborne`, `Sliding`, `WallRiding`, `Redline`, `RecentlyDashed` — and is
evaluated in exactly one function, `FBreakerBuildConditionState::EvaluateForActor`
(`BreakerBuildConditions.cpp:8-38`), which reads only
`UBreakerCharacterMovementComponent` and `UBreakerMomentumComponent`.

It is movement-only, and that has now blocked content twice: it cost Elements its
More multiplier (the block comment at `BreakerProgressionLibrary.cpp:398-403`
says so in as many words) and it is why no node can key off combat or status
state.

Several of O30's axes **cannot be authored honestly until it is widened**:

| O30 axis | Condition it needs | Where the state already lives |
|---|---|---|
| ailment | `TargetAfflicted`, `TargetAtMaxStacks` | `UBreakerStatusComponent` already tracks per-target stacks |
| stacking | `SelfAtNStacks` | same component |
| crit-on-condition | `RecentlyCritical` | `FBreakerDamageResult` already reports the roll |
| abilities / cooldown | `AbilityRecentlyUsed`, `WindowOpen` | `UBreakerAbilityStateComponent` already owns windows and streaks |
| guns | `RecentlyReloaded`, `MagazineBelow`, `Aiming` | `UBreakerWeaponComponent` already has all three |

**Cost of widening** (AUTHORED estimate): the enum is append-only (it is
serialized by value in Data Assets — the same rule that governs
`EBreakerWeaponArchetype`), and `EvaluateForActor` gains one component lookup
and one test per entry. The polling already exists in two places
(`BreakerProgressionComponent.cpp:27-34`, `BreakerEquipmentComponent.cpp:777-784`)
and covers gear and nodes at once, so **conditional affixes come free with
conditional nodes.**

This is a small, contained change and it unblocks more of O30's taxonomy than
any other single edit. **It should land before any axis content is authored, not
after.** Otherwise the first pass of axis nodes will be unconditional
percentages, which is precisely what O27 forbids.

### 2.5 Summary — what each axis costs

| Cost tier | Axes |
|---|---|
| **Free — authorable today** | flat damage, crit, movement |
| **One enum row** | fire rate |
| **Small — one attribute or one call site** | stacking, ability-vs-gun damage split, AoE, ability cooldown reduction |
| **Medium — a system split** | bleed and poison as *separate* axes; per-status chance/duration/tick plumbing where today there are only constants |
| **Large — a whole system** | ailment/element (the Elements chain); multispell/projectile count (plus proc-coefficient enforcement, currently unwritten) |
| **A new game system** | minions (§6) |

**The honest headline: of the sixteen axis names in O30, three are free, one is
nearly free, four are small, and the rest need systems that do not exist.** A
tree redesign delivers the cheap half. It cannot deliver the other half, and
authoring the other half anyway is the failure mode this project has already
had twice.

---

## 3. The archetype fit table

O30 says *"certain weapon archetypes should intuitively fit categories better
than others."* This section derives that fit from the **authored mechanics** in
`Source/RiorsEdge/Weapons/`, not from genre intuition. Every number below is
TRANSCRIBED from `ArchetypeRecoilProfile` (`BreakerWeaponComponent.cpp:31-304`)
and `GetPrototypeDefinition` (`:306-510`); the Rifle column is the struct
defaults in `BreakerWeaponDefinition.h:24-77`, because every other archetype
authors only its differences. **All of it is O2 PLACEHOLDER.**

### 3.1 The four mechanical facts the fit actually rests on

**AUTHORED derivation** from TRANSCRIBED numbers.

**(a) Firing duty cycle** — the fraction of a magazine cycle spent shooting.
Fire rate only pays across the firing part; `ReloadDuration` is scaled by
nothing (confirmed: no attribute touches it).

| | Rifle | SMG | Sniper | Shotgun | Rocket | Burst | MG | Sidearm |
|---|---|---|---|---|---|---|---|---|
| RPM | 600 | 900 | 150 | 85 | 55 | 720 in-burst | 700 | 420 |
| Mag | 30 | 35 | 8 | 8 | 4 | 27 | 120 | 14 |
| Reload | 1.8 | 1.5 | 2.3 | 2.2 | 2.8 | 2.0 | 4.2 | 1.1 |
| **Duty** | 62% | 61% | 58% | **72%** | 61% | 70% | **71%** | 65% |

**(b) Damage instances per second**, which is what flat added damage and every
per-hit proc scale with. Pellets count: the Shotgun fires **8** and each one is
its own damage instance and its own crit roll.

| | Rifle | SMG | Sniper | Shotgun | Rocket | Burst | MG | Sidearm |
|---|---|---|---|---|---|---|---|---|
| Base dmg / instance | 24 | 13 | 72 | **10** | 90 | 29 | 11 | 21 |
| Instances / s (firing) | 10.0 | **15.0** | 2.5 | **11.3** | 0.9 | 5.9 | 11.7 | 7.0 |

**(c) Weak-point multiplier and the accuracy to land it.**

| | Rifle | SMG | Sniper | Shotgun | Rocket | Burst | MG | Sidearm |
|---|---|---|---|---|---|---|---|---|
| WeakPointMultiplier | 1.75 | 1.5 | **2.0** | 1.35 | **1.0** | 1.9 | 1.4 | 1.8 |
| AimSpreadDegrees | 0.25 | 0.9 | **0.05** | 3.0 | 0.2 | **0.12** | 0.8 | 0.30 |
| MaxBloomDegrees | 1.4 | 2.4 | 3.0 | 2.0 | 1.2 | **0.6** | **4.2** | 1.8 |

**(d) Ability to shoot while moving**, which is what every movement-conditional
node is really asking for.

| | Rifle | SMG | Sniper | Shotgun | Rocket | Burst | MG | Sidearm |
|---|---|---|---|---|---|---|---|---|
| MoveSpreadDegrees | 0.35 | 0.30 | **1.10** | **0.25** | 0.30 | 0.55 | 0.80 | 0.35 |
| AimMoveSpeedMultiplier | 0.72 | 0.88 | **0.50** | 0.85 | 0.65 | 0.70 | **0.45** | **0.92** |
| AimInSeconds | 0.20 | 0.14 | 0.38 | 0.16 | 0.30 | 0.22 | **0.42** | **0.10** |

### 3.2 The table

**AUTHORED**, derived from §3.1. `++` natural home, `+` fits, `o` neutral,
`-` fights the archetype, `--` structurally bad.

| Axis | Rifle | SMG | Sniper | Shotgun | Rocket | Burst | MG | Sidearm |
|---|---|---|---|---|---|---|---|---|
| **Fire rate** | + | + | - | **++** | + | **--** | **++** | **--** |
| **Flat damage** | o | + | -- | **++** | -- | o | **++** | + |
| **Crit** | + | o | **++** | o | **--** | **++** | - | + |
| **Bleed / poison (application)** | + | **++** | -- | **++** | -- | o | **++** | + |
| **Movement** | o | **++** | **--** | **++** | o | - | **--** | **++** |
| **Ailment / element** | — | — | — | — | — | — | — | — |

The ailment row is empty on purpose: **no archetype has any element or ailment
hook.** Every weapon path hardcodes `EBreakerDamageFamily::Physical`
(hitscan `:1263`, rocket `:1377`, bleed `:1361`), there is no elemental field on
`UBreakerWeaponDefinition`, and the only archetype that applies any status at all
is the SMG.

### 3.3 The four findings that are not intuition

**1. Burst Rifle is structurally the worst fire-rate weapon in the game.**
`BurstCycleSeconds = 0.34` is **not scaled by `FireRateMultiplier`** — the code
takes `FMath::Max(BurstCycleSeconds, Interval)`, so fire rate compresses only
the 0.083 s gaps *inside* a burst. A +50% fire rate roll moves the Burst Rifle
from 5.92 to 6.65 shots/s: **+12% for +50%.** Any fire-rate node or affix is
worth about a quarter on this gun what it is worth on the others. That is either
a deliberate identity ("the gun whose cadence you cannot buy") or a bug, and the
owner should rule which. It is invisible on a tooltip either way.

**2. Sidearm cannot be bought fire rate either, for a different reason.**
`bAutomatic = false` at 420 RPM means the ceiling is already 7 clicks per second.
A fire-rate roll raises a cap the player's hand cannot reach. The semi-autos that
*do* convert fire rate fully are the **slow** ones — Shotgun at 1.4 shots/s and
Rocket at 0.9.

**3. The Shotgun is the flat-damage and ailment archetype, by a wide margin, and
nobody designed it that way.** Eight pellets at a base of 10 each means the
lowest damage-per-instance in the game and 11.3 instances per second. Flat Added
Damage lands on *each* pellet and is then multiplied by the whole Increased
bucket (Item-Foundation's locked aggregation rule), so a flat roll that is +7%
on a Sniper is **+50% on a Shotgun**. The same arithmetic makes it the best
bleed-application weapon that does not apply bleed. **Exposure worth naming:**
the proc-coefficient law (Damage-Pipeline §3) zeroes status application on
*Multishot-generated* projectiles, but Shotgun pellets are native, not generated
— they carry full coefficient. If bleed ever rolls on a Shotgun at SMG rates,
that interaction is the explosion, and no enforcement code exists yet.

**4. Rocket is the anti-crit archetype and it is not obvious.** Its
`WeakPointMultiplier` is 1.0 — and hardcoded 1.0 again at `:1378` — so it has no
weak-point payoff at all, and at 0.9 damage instances per second the variance on
a crit *chance* roll is the highest in the game. Crit chance is a bad buy on a
Rocket; crit damage is a fine one. A tree that offers "crit" as one undifferentiated
axis cannot express that. A tree that splits chance from damage can.

**Sniper is the clean case**: highest weak-point multiplier (2.0), tightest ADS
cone (0.05°), largest damage per instance (72), worst movement penalties in the
game (1.10° move spread, 0.50 aim-move speed). It is the crit archetype and the
anti-movement archetype simultaneously, which is exactly the kind of pull O30
wants — a build reason to pick one gun over another.

**Machinegun is the mirror**: 120 rounds, 71% duty, 11.7 instances/s, and the
worst accuracy stack in the game (`MaxBloomDegrees` 4.2, `ClimbRampShots` 22 to
a 2.6x multiplier). Fire rate, flat damage, and ailment application all land on
it; crit and movement do not.

---

## 4. Structural options, with costs

### 4.0 What "cost" means here

Four columns, because they are genuinely independent:

- **Authored content** — nodes that must be written or rewritten.
- **Code** — enum rows, attributes, consumers.
- **Save compatibility** — see §4.5. This is the one people underestimate.
- **UI** — see §4.6. The Core board is a hardcoded constellation map.

### 4.1 Option A — keep six constellations, re-theme onto axes

Keep the cluster count and the point grammar; change what each cluster is *about*
so the six names are the six axes.

A plausible mapping (AUTHORED, illustrative only):

| Cluster today | Becomes | Absorbs |
|---|---|---|
| Precision | **CRIT** | Precision's four, Elements' Catalyst |
| Volley | **CADENCE** (fire rate) | Volley's five + the new fire-rate node target |
| Affliction | **AILMENT** | Affliction's two + Elements' four DoT nodes |
| Velocity | **MOVEMENT** | Velocity's six, unchanged |
| Kinesis + Bulwark | **DEFENCE** | seven nodes merged into one cluster |
| (new) | **ABILITIES** | nothing exists — all-new |

| Cost | Assessment |
|---|---|
| Authored content | **LOWEST.** Almost every one of the 30 live nodes survives with a prefix change (§1.4; the old “21 of 24” is CORRECTED — the total is 30 and the 21 is unrecomputed). One new cluster to author. |
| Code | One enum row (fire rate) buys Cadence. Everything else is optional. |
| Save | Prefix renames orphan every allocation — see §4.5. |
| UI | Rename six strings, reposition six clusters, delete the `UNMAPPED` catch-all. Under an hour. |
| **Risk** | Merging Kinesis and Bulwark into one defence cluster halves the defensive content's shelf space, and O30's taxonomy has **no defensive axis at all**. This is the option's real problem, not its cost. |

### 4.2 Option B — one constellation per axis

Sixteen axes, sixteen clusters. Or a trimmed set — say ten.

| Cost | Assessment |
|---|---|
| Authored content | **HIGHEST.** Sixteen clusters at Core-Constellations' 11-node/26-point grammar is 176 nodes against 30 today. Even at four nodes each it is 64. |
| Code | Every axis in §2.5's "large" and "new system" tiers must be built first, or two-thirds of the board is inert clusters. |
| Save | Same orphaning as A, at greater volume. |
| UI | The board is a 1060x800 canvas holding six 300px cluster plates. Sixteen does not fit; it becomes a scrolling or paged map. Real UI work, not a rename. |
| **Risk** | It breaks the structural invariant the whole tree rests on. §2.2 of Core-Constellations derives "a character has at most two keystones, ever" from 26-point constellations against a 65-point budget. **With sixteen smaller constellations the budget buys more keystones and the choice collapses** — the document names this failure explicitly and rejects 20-point constellations for exactly this reason. Re-deriving the budget is mandatory, not optional. |

### 4.3 Option C — two levels: Core carries axes, class trees carry identities

**This is the option that matches what O30 actually says.** The stated intent is
*"the 3 knobs of two different trees and gear on another layer becomes several
knobs"* with *"subclasses supporting the identities of each"*. That is a
statement about the **relationship between layers**, not about cluster count.

Shape (AUTHORED):

- **Core = axes, and only axes.** Each cluster is one axis, and every node in it
  is a magnitude or a condition on that axis. Core answers *what kind of damage
  do I do*. It is class-agnostic by construction, which it already must be —
  Core is `EBreakerClassId::None` in every node (`GetCoreSliceTree`).
- **Class branches = identities, and only identities.** Every branch node is a
  rule rewrite, a resource-loop change, or an ability grant. Class-Kits §0
  already commits to exactly this: *"No node in this document is a flat
  percentage. Every node is a rule rewrite or a resource-loop modifier."* The
  live Swift trees violate it (Long Lens, Deadeye and Pierce Discipline all
  author flat crit) but the rule is already written.

| Cost | Assessment |
|---|---|
| Authored content | **MEDIUM.** Core re-themes cheaply as in A. The expensive half is *removing* percentages from the class trees and replacing them with rewrites — roughly a dozen Swift nodes, and the four unbuilt classes get it for free by being authored correctly the first time. |
| Code | Same as A, plus the condition widening in §2.4 (which the identity layer needs to have anything to key off). |
| Save | Same as A. |
| UI | Same as A for the Core board. The branch strip already exists. |
| **Risk** | It requires the discipline to hold the line. The moment a class node authors a flat crit percentage, the two layers are competing again (§5). |

### 4.4 What none of the options may do: add More multipliers

**TRANSCRIBED, O3.** Three composed Mores per build, clamped in the aggregator
at 1.30x each with a global composed ceiling of 2.197
(`RiorsEdge.Items.Rules.NeverAuthorsAMore` and the clamp described in
Item-Foundation).

The trees **already author six More options against a ceiling of three**: Fixate,
Barrage, Terminal Velocity, Redline Doctrine (Core), Overpressure, Culling
(Swift). That is healthy — six options for three slots is a choice. **Sixteen
axes with a More each would be sixteen options for three slots, which is not
sixteen times the choice; it is the same three slots with a longer menu and a
much larger chance that one combination dominates.**

**AUTHORED recommendation:** hold the More count where it is or lower it. More
axes should mean more *conditions* and more *rule rewrites*, which is what O27
means by choices over accumulation. The measured band is 8.74x and O3 was not
broken to reach it (Power-Curve implementation status); nothing in O30 requires
moving that.

### 4.5 Save compatibility — the underestimated cost

**TRANSCRIBED from code.** `UBreakerSaveGame` stores `FBreakerProgressionState`,
which stores `TArray<FBreakerNodeRank> CoreNodeRanks` — **node IDs and ranks**
(`Progression/BreakerProgressionTypes.h:161-173`). Constellation membership on
the board is derived from the `NodeId` **prefix**
(`BreakerMenu.cpp:3253-3255`), so **any re-theme that renames clusters renames
node IDs.**

What happens to a save then, exactly:

- `CollectKnownNodes` finds no definition for the old ID, so the allocation
  contributes **no effect and no tag** — the player silently loses the node.
- Worse, `GetRefundValue` (`BreakerProgressionComponent.cpp:333-342`) falls back
  to `CostPerRank = 1` when the definition is missing. **A 3-point Convergence
  node refunds 1 point.** A respec after a rename silently destroys points.

Three ways to handle it, with costs (AUTHORED):

| Approach | Cost | Verdict |
|---|---|---|
| **Keep the old NodeIds, change only display names and cluster assignment** | Needs a real `Constellation` field on `UBreakerProgressionNode` instead of prefix-sniffing — which the UI code comment at `:3253-3255` already says is the right fix | **Cheapest and safest.** Recommended regardless of which option is chosen. |
| Migration map old ID → new ID | A table plus a version bump; must handle nodes that no longer exist | Correct but needs maintenance forever. |
| Version bump + forced full Core respec on load | Points returned, allocation lost; needs `SaveVersion` to actually be checked (today it is stored and, as far as this scout can see, not branched on) | Acceptable pre-release. Not acceptable after. |

**Independent of any of this, `GetRefundValue`'s fallback-to-1 is a latent bug.**
An unknown node ID should refund nothing and warn, not refund a wrong number
quietly. Worth fixing whether or not O30 proceeds.

### 4.6 UI cost — smaller than it looks, in one specific way

**TRANSCRIBED.** The Core board is built by six literal `AddCluster` calls
(`BreakerMenu.cpp:3246-3251`) with hardcoded names, ID prefixes, and canvas
coordinates, plus a Kinesis-is-the-hub layout and a sealed-cluster flag for
Elements. Membership is prefix matching; anything unmatched falls into an
`UNMAPPED` cluster (`:3267-3278`) — which is where all six Velocity nodes are
sitting right now.

So: **re-theming six clusters is about twenty lines.** Changing the *number* of
clusters is the expensive version, because the 1060x800 canvas holds 300px plates
and the spec (`UI-Skill-Tree-Spec.md` §"Core is spatial") describes five clusters
around a hub with convergence lines. Ten or sixteen clusters is a new layout, and
the hub metaphor stops meaning anything when everything is a spoke.

**AUTHORED note:** if Core becomes axes, the hub should stop being Kinesis. A hub
that is one of the axes privileges that axis for no reason. Either make the hub
a neutral origin node, or drop the hub metaphor.

### 4.7 Recommendation

**AUTHORED. Option C, delivered in the order below, with Option A's cluster
count as the interim step.**

Why C: O30's stated intent is about the *relationship* between the layers, and C
is the only option that addresses it. A is a rename; B is a rebuild that breaks
the two-keystone invariant. C keeps the invariant, keeps the content, and fixes
the thing that would actually make the trees compete (§5).

Sequence, cheapest and highest-confidence first:

1. **Widen `EBreakerBuildCondition`** (§2.4). Nothing else should be authored
   before it, and it costs almost nothing.
2. **Add a real `Constellation`/`Axis` field to `UBreakerProgressionNode`** and
   read it in the board instead of the ID prefix. This is what makes a re-theme
   free of save damage (§4.5), and the UI already asks for it in a comment.
3. **Append `EBreakerNodeStatTarget::FireRate`.** One row. It gives Volley the
   axis it is named after.
4. **Wire the three inert promises** (§1.6) — Open Wound's tag, Deepen's stack
   cap, Cyclic's ramp. Small, and it stops the redesign inheriting lies.
5. **Re-theme Core onto axes at the current cluster count**, moving Velocity out
   of `UNMAPPED` and deciding what happens to Bulwark + Kinesis (Open Question 1).
6. **Split bleed from poison** only if the owner wants them as separate axes.
   That is a real system change and should be a separate ruling.
7. **Ailment/element and minions are not part of this work.** They are systems
   (§2.5, §6) and pretending otherwise is how a tree ships with a dead cluster.

---

## 5. How subclasses solidify and empower without duplicating

**This is the part of O30 most likely to go wrong.** Two trees that both offer
"crit" is not more knobs. It is one knob with two handles, and the player's
decision is reduced to "which one is numerically bigger".

### 5.1 The failure is already happening in the live content

**TRANSCRIBED.** Class-Kits §0 states the rule: *"No node in this document is a
flat percentage. Every node is a rule rewrite or a resource-loop modifier."*

The shipped Swift trees break it:

| Node | Authors | Duplicates |
|---|---|---|
| `Swift.Marksman.LongLens` | +18 flat Critical Damage, +3% Increased Damage | Precision's Tunnel Vision |
| `Swift.Marksman.Deadeye` | +4 flat Critical Chance | Precision's Called Shot |
| `Swift.Marksman.PierceDiscipline` | +6 flat Critical Chance, +3% Damage | Precision's Sightline |
| `Swift.Frenzy.TriggerDiscipline`, `Rhythm` | +3 flat Critical Chance each | Precision, again |
| `Swift.Kinetic.Downforce`, `Grind` | Increased Damage while Airborne / WallRiding | Velocity's Freefall and Traction, same conditions |
| `Swift.Marksman.Culling` | unconditional More | Fixate and Barrage, same shape |

Every one of those was authored deliberately and for a good reason — the code
comments say so, and the reason was that a branch of pure tags reached no
gameplay. But the result is that **Swift's class trees and the Core tree
currently offer the same four things in the same buckets.** A player choosing
Marksman over Frenzy is not choosing a different kind of damage; they are
choosing a slightly different mix of the same three numbers.

Under O30 that is the exact thing to fix, because O30 wants the class layer to
*solidify* an axis identity rather than restate it.

### 5.2 The composition rule

**AUTHORED.** One sentence:

> **Core sets the axis. The class layer changes what the axis DOES.**

Concretely, three permitted class-layer forms, and one forbidden one:

| Form | Example (AUTHORED, illustrative) | Why it composes |
|---|---|---|
| **Conversion** | "Your Bleed applications also count as Poison for stack purposes" | Multiplies the value of Core's ailment investment without adding to its bucket. A player with no ailment nodes gains nothing. |
| **Condition change** | "Your movement-conditional damage also applies for 1.5 s after leaving the state" | Widens the window Core's conditions pay in. Worthless without Core conditions. |
| **Rule rewrite** | "Weak-point hits cannot fail to crit while your magazine is above half" | Changes the shape of crit rather than its magnitude. |
| **FORBIDDEN: magnitude on an axis Core already carries** | "+4 flat Critical Chance" | Same bucket, same knob, two sources. This is what the live trees do. |

The test is mechanical and can be enforced by an automation test: **a class node
may not author an `EBreakerNodeStatTarget` that a Core node on the same axis also
authors.** That is a real invariant, not a style guide, and it is the kind of
thing this project already pins with tests (`RiorsEdge.Items.Rules.NeverAuthorsAMore`
is the precedent).

### 5.3 Why this makes the branch strip a real choice

**RECONCILED.** The three Swift branches already carry three distinct
`EBreakerBuildCondition` values — Kinetic owns Airborne/WallRiding/Sliding,
Marksman owns unconditional, Frenzy owns Redline (the code comment at
`BreakerProgressionLibrary.cpp:629-635` states this as the design intent). That
is the right instinct and it is the half that works.

Under the §5.2 rule the branches would differ by **what they do to an axis**
rather than by which condition they pay under:

- The same CRIT investment in Core reads as *reliability* under one branch and
  as *burst* under another.
- The same AILMENT investment reads as *spread* under one and *magnitude* under
  another.

That is what turns three knobs into several: not more sliders, but the same
slider meaning different things depending on the identity bolted to it. It also
satisfies O31 directly — a build whose axis is weak in an encounter still has its
class-layer rewrites, so it can still make an impact.

### 5.4 O15 is not in the way

**TRANSCRIBED.** O15 says branch nodes mix freely with no mutually exclusive
tiers. Nothing in §5.2 is exclusivity — a player may buy conversions from two
branches. What bounds them is the existing budget (26 points per branch against
30 Class Points, so at most one branch keystone, per Class-Kits §0.2), not a
lock.

**Open item, not decided here:** subclass *commitment* is still an unruled owner
question (Decisions.md pending list), and §5's whole argument assumes a player
can meaningfully "be" a subclass. If branches stay freely mixed with no
commitment, "subclasses solidify identities" has no subject. That is Open
Question 4.

---

## 6. The MINIONS problem, stated plainly

**A minion axis is not a tree redesign. It is a new gameplay system plus a tree
axis, and the tree axis is the small half.**

### 6.1 What exists

**TRANSCRIBED.** Nothing. No player-side spawner, no deployable actor, no
ownership model, no minion stat routing. The only spawner in the project is
`ABreakerBossEnemy`'s adds (`BreakerBossEnemy.cpp:381-403`), which is enemy-side
and unreachable by any player stat. There is no minion, pet, deployable, summon
or turret affix anywhere in the item pool, in the four Anomalous rewrites, or in
the three legendaries.

### 6.2 What is designed

**TRANSCRIBED, `Docs/Design/Class-Kits-Gunsmith.md`.** The design is thorough
and it is worth reading before costing anything, because it has already made most
of the expensive decisions:

- **Four deployable types**: Turret (30 s, autonomous acquisition ~18 m with LOS,
  0.4 s reacquire), Ammo Crate (45 s, 4 interact charges), Mine Cluster (60 s,
  3 charges, trigger-condition driven), Disruptor (20 s, 6 m slow + flat armour
  reduction zone).
- **§2.3, a hard ruling**: *"Deployables do not move. Nothing in the tree makes
  them move. A moving turret is a pet, and pets are a different class fantasy
  with a different AI budget."* This is the single most cost-saving line in the
  document — it removes pathfinding, follow behaviour and stuck-recovery from
  scope entirely.
- **§2.4**: own health pool per type, authored, **not** inherited from the
  player. Direct damage only, no DoT, no status. Opportunistic enemy targeting,
  never a threat/aggro mechanic; bosses and Champions ignore them.
- **§1.3, the ruling that matters for O30**: *"Damage dealt by a Gunsmith's
  deployable is the Gunsmith's damage."* The deployable is a delivery mechanism,
  not a pet with its own stat block — the player's affixes, crit and statuses
  apply. Proc coefficient 0.5 continuous / 1.0 one-shot.
- **Caps**: 4 active, 2 of any one type; 5 with a node; 8 during the ultimate;
  9 maximum composed. *"No node in this document raises the per-type cap above 2."*
- **§7.3**: zero flat-percentage nodes. The only More that scales deployable
  damage is the FOUNDRY keystone at 1.30x.

### 6.3 What would have to exist before a minion axis could be authored

**AUTHORED cost breakdown.** In dependency order:

| # | What | Why it is required | Rough size |
|---|---|---|---|
| 1 | A deployable actor base — replicated, owned, its own health, lifetime, destruction path | Nothing like it exists | New class, comparable to `ABreakerRangedEnemy` in scope |
| 2 | Placement — server sweep, LOS trace, floor-only, ~8 m, 0.4 s cast | §2.3 specifies it exactly | Small, but it is new input and new prediction surface |
| 3 | Ownership and instigator routing so `FBreakerDamageRequest::Instigator` credits the player | The attacker-side `OnHitDealt`/`OnKillDealt` chain, Scrap generation, and TTK telemetry all key off it | Small — the contract already exists and already handles DoT ticks crediting their applier |
| 4 | **A snapshot-vs-live ruling** | Gunsmith §1.3 says the player's stats apply and **never says whether they are snapshotted at placement or read live**. This is unauthored and it changes everything downstream | A ruling, then a small implementation |
| 5 | Turret targeting AI | The only type that needs it; the other three are triggers and zones | Medium — but `ABreakerRangedEnemy`'s band/intercept maths in `BreakerRangedBehavior.h` is directly reusable |
| 6 | A cap/registry per player | 4 active, 2 per type, oldest-replaced-and-refunded | Small |
| 7 | The Scrap resource loop | Gunsmith is otherwise unbuilt as a class | Medium — the Momentum and Mana components are the pattern |
| 8 | HUD: deployable count, lifetimes, health | Nothing shows them | Small-medium |
| 9 | **Only then**: node targets for count, lifetime, deployable damage | The tree axis itself | Small |

Items 1-8 are the system. Item 9 is the axis. **The axis is perhaps 5% of the
work.**

### 6.4 The specific reason a minion axis in the *Core* tree is harder than in the class tree

**AUTHORED, and this is the finding worth arguing about.**

Gunsmith's design deliberately gives deployables **no independent stat block** —
they scale on the player's own affixes. That is what makes them cheap. But a
**Core** tree is universal: every class buys from it. So a Core minion axis is
either

- **dead for four of five classes** — a whole constellation that does nothing
  unless you picked Gunsmith, which is worse than Redline Doctrine's situation
  (that node is one node, and O15 lets a Tank see and decline it honestly; a
  whole cluster is a different scale of dead), or
- **a commitment that every class gets deployables**, which is a much larger
  design change than O30 describes and collides with Gunsmith's class identity.

**Recommendation (AUTHORED): minions are a class axis, not a Core axis.** They
belong in the Gunsmith tree, where the design already puts them, and where
Field Tech and Tinkerer already *are* the two minion sub-identities O30 is asking
for. If O30's taxonomy wants a third top-level category, the honest answer is
that MINIONS is a category of the **class** layer, and Core's third category
should be something universal — defence being the obvious candidate, since O30's
taxonomy currently has no home for the seven live Bulwark and Kinesis nodes.

---

## 7. OPEN QUESTIONS

Each has options and costs. None is decided here.

**1. Where do defence and mobility go?** O30's taxonomy is entirely offensive.
Seven live nodes (Bulwark 3, Kinesis 4) and Parry — **[O25 correction: the
game's only tree-granted verb, not "two of three."** Air jump is base kit for
everyone under O25, not a tree grant; Swift's third jump, if built, is
class-innate. The pre-O25 count of three tree-granted verbs no longer applies —
Parry is the sole one.] — have no axis. Options: (a) a DEFENCE axis in Core,
breaking the GUNS/ABILITIES/MINIONS symmetry but keeping the content; (b) push
defence entirely to gear and class trees, which deletes 7 nodes and Parry
and collides with "trees improve movement verbs, gear does not grant them";
(c) treat defence as a fourth category outside the taxonomy. **Cost: (a) near
zero, (b) high and lossy, (c) a naming decision.** This is the biggest unanswered
question in O30 as written.

**2. Is MINIONS a Core axis or a class axis?** §6.4 argues class. If the owner
wants it in Core, the follow-on question is whether every class gets deployables.
**Cost of the Core version: the whole system in §6.3 plus a design change to four
class kits. Cost of the class version: the system in §6.3 only, and it can wait
until Gunsmith is built.**

**3. Are bleed and poison separate axes or one ailment axis?** Today they are one
thing wearing two tags (§2.1). Options: (a) one AILMENT axis — free, honest,
and smaller than O30's taxonomy; (b) split them with per-status multipliers,
chance and duration plumbing — medium cost, and it makes the SMG (the only bleed
source) carry an entire axis alone until more weapons apply statuses.

**4. Does "subclass" mean anything yet?** §5.3 assumes a player can commit to a
branch. Subclass commitment is on Decisions.md's pending list and collides with
O15. Until it is ruled, "subclasses solidify identities" describes a browsing
strip. **Cost: a branch field, a permanence-or-Forge rule, save versioning, and
the O15 collision.**

**5. Is Burst Rifle's fire-rate wall intentional?** (§3.3 finding 1.) Options:
(a) intentional identity, document it and show it in the UI; (b) bug, scale
`BurstCycleSeconds` by `FireRateMultiplier` — one line, but it materially changes
the archetype's ceiling; (c) partial scaling. **Cost: (a) a doc line, (b) one
line plus a retune, (c) one line plus a new constant.**

**6. Does Core keep its hub?** (§4.6.) A hub that is one of the axes privileges
that axis. Options: neutral origin, or drop the metaphor. **Cost: layout work
either way, and the spec (`UI-Skill-Tree-Spec.md`) needs updating.**

**7. How is a re-theme delivered without destroying saves?** (§4.5.) The
recommended answer is an explicit `Axis`/`Constellation` field on the node so IDs
never change, but that is a real (small) code change and it should be ruled
before any content moves.

**8. Does the More count stay at three options-per-slot ratios it has now?**
(§4.4.) The trees offer six Mores for three slots today. If axes multiply, does
the offered count grow with them? **Recommendation: no.** But it is the owner's
ceiling to move.

---

## 8. What this document did NOT do

- **No gameplay code was changed.** Nothing under `Source/` was edited.
- **No values were authored.** O2 stands; every number quoted is TRANSCRIBED
  from existing code or documents and is itself a placeholder.
- **No ruling was made.** §4.7 is a recommendation with a sequence, not a
  decision.
- **`Core-Constellations.md` was not rewritten.** It still describes the
  six-constellation design; §1.1 above records where it and the live roster
  disagree. Whether to reconcile that document or supersede it depends on which
  option the owner picks.

