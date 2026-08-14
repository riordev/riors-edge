# Game Modes — Content Type Design

**Last reconciled against: O32** (2026-08-14).

Domain: Local Rifts, Frontiers, Dungeons (4), Raids (7), Conquest (9).
Status: Local Rifts and Frontiers are full designs. Dungeon/Raid/Conquest are one-page treatments.
~~Authority: subordinate to `Docs/Design/Master-Sheet-Import.txt`. Every LOCKED decision there is law here.~~
**SUPERSEDED [O28].** The master sheet is a raw import and has **lost its
standing** — it is historical source material, not law, and is not to be cited
as authority again. The chain is `Decisions.md` -> `Design-Overview.md` (map,
not law) -> this document. Bare parenthesised section numbers throughout
(`(7.1)`, `(11.1)`, `(9.2)`) are master-sheet references and read as
provenance, never as rules. Where one disagrees with the ledger, the ledger
wins.

> **VOCABULARY CORRECTION [O32]. This document conflates a rarity with an item
> type, in the exact way O32's implementation note forbids.** O32 rules:
> **Anomalous is a RARITY** — the fifth tier, gating affix count and tier
> ceiling, carrying one *rolled* rule rewrite from a generic pool of four.
> **Legendary is a separate field** (`FBreakerItemInstance::LegendaryId`) naming
> a specific authored item with a fixed slot, guaranteed affixes and a
> *hand-authored* rule. Every legendary rolls at Anomalous rarity; **most
> Anomalous drops are not legendaries.**
>
> §1's vocabulary table says every remaining use of "Anomalous" in this document
> is a rarity reference and is correct as written. **That is no longer true.**
> Three lines describe *legendaries* and call them Anomalous:
>
> - **§2's mode map, Raid row** — "unique Anomalous". A rarity is not unique.
> - **§6 Function** — "Raid-exclusive Anomalous items that each imply a whole
>   build". An item that implies a whole build is a hand-authored rule, which
>   is the definition of a legendary.
> - **§6 Rewards** and **Open Question 6** — "Raid-exclusive Anomalous items at
>   a low rate". Same conflation; the open question inherits it.
>
> **Read all four as LEGENDARY.** They are not corrected in place because the
> substitution changes what the mode promises — a Raid-exclusive legendary is a
> named item on the O32 pool-growth list, and O32 explicitly rules that the fix
> for legendary cadence is **authoring more legendaries**, not raising the drop
> rate. So "Raid-exclusive legendaries" is a *content commitment* against that
> list, and committing to it is an owner call. **The other uses in this document
> — §2's Frontier row, §4.3's tier band, §4.6's rarity weights, §4.8's
> acceptance criterion — are genuine rarity references and are correct.**
>
> **Bearing on O32's arithmetic:** O32 records ~57 hours per legendary at area
> level 50, falling to ~21 with a full set, because three authored legendaries
> cover only Boots, Primary and Waist. Helmet, Body Armour, Gloves, Necklace and
> Secondary have none. **Any mode in this document that promises legendaries is
> promising content off that unwritten list.**

All numbers in this document are PLACEHOLDER until the Playtest Gym establishes a real time-to-kill, per master sheet 3.0.

---

## 0. Constraints this document operates under

Inherited and non-negotiable:

- Level cap 50, hard stop. No mode grants power outside gear. (7.1)
- Gear is the entire endgame. Mode rewards are loot, materials, and access — never stats. (9.1)
- Solo is the primary balance target; co-op supports up to five. (11.1)
- Crit is the only multiplier of its kind. No mode modifier may create a second one. (6.3)
- Flat sums -> one additive Increased bucket -> More reserved for trees/Anomalous. Mode modifiers that touch the player use the same pipeline; enemy-side modifiers are authored on the enemy, not the player.
- Affixes scale verbs, never grant them. No mode grants a verb either. Air jump and parry remain the only tree-granted verbs.
- No grapple, no tether. Level design must not require one.
- Dodge and block are passive/stance defensive layers, not player-input dodges. Encounter design must never require a timed dodge input. This is the single largest encounter-design constraint in this document and it is easy to violate by accident.
- Closing a rift erases the timeline behind it. The player does not know this initially. (1.6)

---

## 1. NAMING — RESOLVED (O8)

**RESOLVED by owner ruling O8** (`Docs/Design/Decisions.md`): *the endgame farm content type is named "Frontier."* The prior working name "Anomaly" collided with the **Anomalous** rarity tier and violated the intent of master sheet 1.2 NAMING CONSTRAINTS. That collision is now closed.

Vocabulary after O8:

| Term | Refers to | Status |
|---|---|---|
| **Frontier** | The endgame farm content type ("running a Frontier") | Ruling O8. Already the master sheet's own term for the endgame region (8.3, 10.1) — zero new vocabulary. |
| **Anomalous** | The top item rarity tier | Unchanged. Not renamed, not affected by O8. |
| **Aberrant** | The chase item rarity tier below Anomalous | Unchanged. |

**"Anomaly"/"Anomalies" no longer appear in this document as a content type.** Every remaining use of "Anomalous" in this document is a *rarity* reference and is correct as written.

**GAP [O8]:** §4.3 already uses **"Frontier pack"** as the name of the top pack-rarity tier inside a Frontier. Post-rename that reads as "the Frontier pack in a Frontier." O8 renames the content type only and does not rule on the pack tier name, so it is left as written. This needs an owner naming decision for the pack tier; do not resolve it in propagation.

---

**Campaign consumer.** `Docs/Design/Campaign-And-Story.md` (reconciled against
O28) consumes the rift skeleton (§3.2), the eight objective archetypes (§3.4),
the closing ritual (§3.5) and the Frontier handoff (§4) to author the story
mission list. It authors no mode value. It also records one collision this
document does not: the SEAL ending closes every rift permanently while the
Frontier is a rift-shaped mode the player runs afterwards — see that document's
§5.2 and OPEN QUESTION 1.

## 2. Content type map

| Mode | Players | Instanced | Repeatable | Primary reward | Nearest-term |
|---|---|---|---|---|---|
| Anchor | shared | No | n/a | Services (Forge, vendors) | Exists as concept |
| Local Rift | 1-5 | Yes | Yes | Progression loot, Core Points (one-time) | **Build first** |
| The Breach | 1-5 | Yes, gated | One-time | Act II gate, capability unlock | Campaign |
| Erased Earths | shared semi-open | No | Partly | World events, materials | Campaign |
| **Frontier** | 1-5 | Yes | Yes, infinite | T0 bases, Aberrant, Anomalous, T-1 materials | **Build second** |
| Dungeon | 4 | Yes | Yes, lockout-lite | Targeted Aberrant signatures | Post-slice |
| Raid | 7 | Yes | Yes, weekly-ish | Guaranteed T-1, unique Anomalous | Post-slice |
| Conquest | 9 matchmade | Yes | Yes | Volume + Frontier access currency | Post-slice |

### Repeatable vs one-time ratio — RECOMMENDATION

Target **roughly 20% one-time, 80% repeatable, measured in first-playthrough hours.**

- The campaign (levels 1-50, Acts I-III) is the one-time content. It must be good enough to sell the game and short enough that the endgame is where players live.
- Every one-time piece must hand over a *permanent capability or currency*, never a stat: a fragment capability (1.7), a Core Point (7.2 — the ~15 world-content points), or an Frontier modifier unlock.
- Every repeatable piece must be runnable in under 12 minutes solo at a competent skill level, or it will not survive a hundred repetitions.
- **Do not build a second one-time campaign as an expansion model.** The Erased Earths content engine (8.4) is the correct expansion vector: new tilesets and modifier pools feed the repeatable layer.

**Where the ~15 world-content Core Points come from (EXTENDS 7.11 open item):**

| Source | Points | Notes |
|---|---|---|
| First-clear of each Local Rift archetype (8 archetypes) | 8 | One-time per archetype, not per instance |
| The Breach completion | 1 | Act II gate |
| Erased Earth zone discovery (3 zones) | 3 | One per zone |
| First Frontier tier 5 / 10 / 15 clear | 3 | Bridges campaign into endgame |
| **Total** | **15** | Matches the 7.2 schedule exactly |

This is deliberately front-loadable but not endgame-gated: a solo player who never touches a Dungeon or Raid can reach the full ~65 Core Points. **Group content must never gate Core Points.** (11.1 solo balance target.)

### Procedural vs handcrafted — RECOMMENDATION

**Handcrafted rooms, procedural assembly, handcrafted set pieces.** Specifically:

| Mode | Approach | Reason |
|---|---|---|
| Local Rift | **Handcrafted layouts, ~3 layouts per archetype, procedural objective/spawn/modifier placement** | The campaign is seen once. Layout quality matters more than variety. Cheap to author because rift interiors are small. |
| Frontier | **Procedural assembly from handcrafted room modules ("tiles"), handcrafted boss arena** | Infinitely repeatable. Needs novelty. But a movement shooter dies on procedural geometry that does not read for wall-ride and slide lines. |
| Dungeon | **Fully handcrafted** | 4-player coordination requires known geometry. |
| Raid | **Fully handcrafted** | Same, more so. |
| Conquest | **Handcrafted warzone maps, procedural front-line state** | The map is a persistent stage; the fight over it is the variable. |

**The load-bearing rule for procedural assembly:** every tile module must publish a *movement contract* — at minimum one continuous wall-ride surface of ≥ 6 m, one sliding descent, and one route that requires none of the above. Guardrail 5.4 says level design must offer movement opportunities without punishing conventional routes; in a procedural system that is a per-tile authoring requirement, not a level-designer instinct. Tiles that fail the contract are rejected at cook time by an automated check.

Room modules also publish a socket type (entry/exit count and elevation delta) so the assembler cannot produce geometry with a dead route or an unreachable exit.

---

## 3. LOCAL RIFTS — full design

### 3.1 Fantasy and function

A rift is open. Something is holding it open from the other side. You go in, you kill the thing, the rift closes. This is the game's atomic loop and the player performs it from level 1 to level 50 and beyond.

It is also, unknown to the player, the game's central moral crime (1.6). Design consequence for this mode: **the closing ritual must be satisfying and slightly too long.** The player should have several seconds with nothing to do but watch a world end. Nobody comments on it. In Act III, when the player learns what closure does, the ritual they have performed four hundred times retroactively acquires meaning. This costs almost nothing to build and is the highest-leverage narrative decision in this document.

### 3.2 Structure — the three-beat rift

Every Local Rift is exactly three beats. Do not add a fourth.

```
  ENTRY          ->   BODY (2-3 rooms)      ->   ANCHOR POINT      ->   CLOSING RITUAL
  Threshold           Objective rooms            Boss / holder          Erasure sequence
  ~20 s               ~4-7 min                   ~90-150 s              ~25 s
```

**ENTRY / THRESHOLD (~20 s).** A short corridor with no enemies. Purpose: (a) load-hiding, (b) a readable tonal shift from the Anchor, (c) the tier and modifier readout is displayed diegetically on the threshold so the player sees what they signed up for *inside* the mode rather than on a menu. The threshold is also where a party regroups; it is a safe zone that does not de-spawn.

**BODY (2-3 rooms, ~4-7 min).** Each room is one objective (below). Rooms are connected by a traversal segment — the movement pillar's actual home. A traversal segment is 15-30 seconds of geometry with no enemies where slide, wall ride, and dash are the content. This is the pacing valve: it lets combat be dense without the whole rift being dense.

**ANCHOR POINT (~90-150 s).** The thing holding the rift open. Not always a boss — see 3.4.

**CLOSING RITUAL (~25 s).** See 3.5.

### 3.3 Target length

| Rift class | Rooms | Target solo clear | Use |
|---|---|---|---|
| Short | 2 | 5-7 min | Campaign filler, daily loop, the default |
| Standard | 3 | 8-11 min | The archetype baseline |
| Deep | 3 + a gated optional room | 12-15 min | Higher tiers only |

**8-11 minutes is the number to defend.** Long enough for a build to express itself and for one bad decision to matter; short enough that a failed run is not a punishment. If the mode drifts past 15 minutes solo, cut a room, do not cut enemy health.

### 3.4 Objective types

Eight archetypes. Each is a room-scale objective; the rift picks 2-3 without repetition. Each also has a first-clear Core Point attached (see ratio table above).

| # | Objective | Player verb | Movement pressure | Notes |
|---|---|---|---|---|
| 1 | **Clear** | Kill everything in the room | Low | The control. Every content system needs one plain room. |
| 2 | **Hold** | Stand in a zone while waves arrive | Low — anti-movement | Deliberately punishes pure mobility. Balances the movement pillar. Zone is generous enough to fight inside. |
| 3 | **Sever** | Destroy 3-5 tethers on walls/ceilings at height | **High** | The wall-ride objective. Tethers are reachable by conventional route but 40% slower. |
| 4 | **Escort** | Move a suppression drone through the room at its pace | Medium | Forces the player to fight at someone else's tempo. Drone is invulnerable — it is a pacer, not an escort-mission health bar. |
| 5 | **Carry** | Move charges from spawn to a socket, N times | **High** | Carrying disables the Secondary slot (EXTENDS: requires the swap system, already implemented). Movement speed is the whole objective. |
| 6 | **Collapse** | Survive while the room degrades — floor sections erase | **High** | Best expression of the erasure fiction as a mechanic. |
| 7 | **Hunt** | Kill 3 marked elites that flee between rooms | Medium | Uses the elite modifier system as content rather than as difficulty. |
| 8 | **Silence** | Kill a broadcasting Vestige that continuously buffs the room | Low-medium | Priority-target training. Teaches the skill the Frontier boss will demand. |

Distribution rule: a rift may contain at most one high-movement-pressure objective (3, 5, 6). This is a direct implementation of 7.10 risk 7 (movement tax) — no rift may be gated end-to-end on traversal mastery.

### 3.5 The closing ritual

Locked sequence:

1. Anchor Point dies. All remaining trash dies with it (no cleanup phase — respects the player's time).
2. **Collection window, 15 s.** Loot is spawned and pulled toward the player. Pickup Radius % (3.10) matters here.
3. **The seal, 8 s.** Player interacts with the rift core. Camera does not lock. Player retains full movement.
4. **The erasure, ~10 s.** The rift interior desaturates and unloads *around the player*, geometry-first, from the far walls inward. Audio drops to a single tone. No VO. No UI. No score screen yet.
5. Return to Anchor.

**Design rules for step 4:**
- Nobody speaks. Not Command, not a squadmate, not the player character.
- The sequence is identical at level 3 and level 50. It must not get flashier with tier — that would frame it as a reward.
- It is skippable via input *after* the first three clears, and the skip prompt is deliberately small. Players who skip it will re-watch it once, deliberately, after the Act III reveal.
- **EXTENDS 1.6:** in Act III+, add one thing to the sequence — a single silhouette in the unloading geometry, visible only if the player is looking the right direction, present at a very low rate (~2%). Never acknowledged. Never explained. Do not add a codex entry.

### 3.6 Death and retry

- Solo: death returns the player to the Threshold with the rift's progress intact, on a 20 s respawn timer. **No run loss on Local Rifts.** This is the campaign loop; it must not be punishing.
- Party: bleedout with ally revive (Interaction & Revive Speed %, 3.10, becomes relevant); full-party wipe returns everyone to Threshold.
- **Frontiers use a different, harsher rule.** See 4.7.

### 3.7 Rift tiering

Tier is a single integer on the rift instance. It drives three things and nothing else:

| Knob | Formula | Notes |
|---|---|---|
| Enemy level | `min(50, 3 + tier * 1.6)` rounded | Caps at 50; past that, tier drives density and modifiers only |
| Item level of drops | `enemy level + tier_bonus` where tier_bonus = 0 below T20, +1 per 5 tiers above | Item level gates affix TIER (4.2) |
| Modifier count | `floor(tier / 4)`, cap 4 | See 4.3 modifier system |

**Tier 1-15 is campaign.** The rift tier available to a player is capped at their level, so tiering is invisible during levelling.

**Tier 16-30 is the endgame band.** Available at 50. This is where Frontiers take over — Local Rifts above T15 exist as a lower-intensity alternative for players who do not want the Frontier modifier stack, with a corresponding reward penalty (see 4.6).

**Reward scaling per tier — the shape that matters:**

- Rarity weights improve **smoothly** with tier. No cliffs.
- Item level (and therefore affix tier ceiling) improves **stepwise**, every 5 tiers. Cliffs here are good: they give a player a specific tier to grind toward.
- **Quantity is nearly flat.** Higher tiers give better items, not more items. This is the anti-vendor-simulator rule and it protects the whole loot chase from becoming an inventory management chore.

Placeholder rarity weight table (drop-level, per item rolled):

| Tier band | Standard | Uncommon | Exceptional | Aberrant | Anomalous |
|---|---|---|---|---|---|
| T1-5 | 62% | 30% | 8% | 0% | 0% |
| T6-10 | 45% | 38% | 16% | 1.0% | 0% |
| T11-15 | 28% | 44% | 26% | 2.0% | 0.02% |
| T16-20 | 12% | 42% | 42% | 3.8% | 0.15% |
| T21-25 | 4% | 30% | 58% | 7.2% | 0.40% |
| T26-30 | 0% | 18% | 72% | 9.4% | 0.75% |

Note that **Exceptional is the tier that grows most**, per 9.2 — it is the real farm target because it can be crafted to 7 affixes. Aberrant and Anomalous are chase items and their rates are deliberately low relative to the loot volume; the equip limits (3 and 1) mean the tenth Aberrant is worth far less than the first.

**BLOCKED:** these weights cannot be finalised before a real TTK exists (4.9) and before "intended time from 50 to a finished build" is answered (9.4). They are shape, not values.

**UPDATE [O18] — TTK/TTD seed targets now exist.** O18 supplies designer-input seed targets (trash a little under 1s scaling exponentially with difficulty; rare/elite ~3s; boss 20–45s unless a special enemy; TTD 4–5s with no resources/sustain, substantially higher with them invested). They are *inputs*, not the measured TTK this section is blocked on — the block above stands. What changes is the validation rule: **tier tuning validates against divergence from the O18 targets.** A tier band whose measured trash TTK drifts far above ~1s, or whose rift boss falls outside 20–45s, is a tuning failure of that band, reported as divergence rather than silently re-baselined. Enemy level, modifier count, and pack density are the knobs that answer a divergence; no value here changes in this pass (O2).

### 3.8 Acceptance criteria — Local Rifts

- [ ] A solo player at the intended level clears a Standard rift in 8-11 minutes, measured across 20 runs, with a standard deviation under 2 minutes.
- [ ] No rift can be completed without firing a weapon.
- [ ] No rift can be completed without moving through a traversal segment — but every traversal segment has a conventional route no more than 40% slower.
- [ ] No objective requires a timed defensive input. Verified by design review, not by playtest — dodge and block are passive layers (5.2, this doc §0).
- [ ] A player using only walk/sprint/jump/crouch completes every objective type at the intended tier. (5.1)
- [ ] The closing ritual runs identically at T1 and T30.
- [ ] Two rifts of the same archetype and tier, generated back to back, differ in objective set and spawn composition.
- [ ] Tier is a single integer and every difficulty consequence derives from it. No hand-authored per-tier data.
- [ ] Death costs 20 seconds and no progress.
- [ ] First-clear of each of the 8 archetypes grants exactly 1 Core Point, once per character, tracked in save.

---

## 4. FRONTIERS — full design

### 4.1 Fantasy and function

*(Name ratified by O8. See §1.)*

An Frontier is a timeline Rior is currently working on. It is a recognisable Earth tileset, modded — the same rooms the player knows, running wrong. It is relatively open, densely populated with packs of varying rarity, and it ends with a map boss.

This is the endgame. It is where a level-50 player spends the next several hundred hours. Everything about it is built for repetition.

**Structural difference from a Local Rift:** a Local Rift is a corridor with objectives. An Frontier is **an open field with a boss at the end of it**. The player chooses their own route, their own pack order, and their own risk. There is no objective list. The objective is loot.

### 4.2 Structure

```
  INGRESS  ->  OPEN FIELD (procedural, ~6-10 tiles)  ->  BOSS ARENA  ->  EXTRACTION
  ~15 s        8-14 min, player-paced                    2-4 min        ~20 s
```

- **Ingress.** Threshold room. Displays tier and the full active modifier list. Party regroup point. Safe.
- **Open field.** 6-10 handcrafted tile modules assembled procedurally, with a guaranteed spanning route and 2-4 optional loops. Contains 12-20 mob packs. Packs have their own rarity (below). The player is not required to clear everything — and should not be able to, comfortably, within the time budget. **Choosing which packs to fight is the mode's core decision.**
- **Boss arena.** Always handcrafted, always the same 3-4 arenas per tileset. Entering commits — no leaving without killing or dying.
- **Extraction.** Loot collection window, then a closing ritual identical in structure to the Local Rift's but scaled to a whole timeline: the erasure sequence is a *horizon* unloading, not a room.

### 4.3 Pack rarity — the density system

Master sheet: *"kill lots of packs of mobs with different rarities that drop gear."* Formalised:

**Taxonomy (O9).** Pack composition is expressed in the three orthogonal enemy
fields, not in the old "normals + elites" vocabulary:

- **Archetype** — behavior (Skitter / Lattice / Warden / …). Owned by `Encounter-Design.md`.
- **Rank** — Standard / Veteran / Champion / Boss. Owned by `XP-And-Pacing.md`.
- **Modifiers** — 0-3 per enemy. Owned by `Encounter-Design.md`.

**Modifier count drives Rank** (adopted from XP §5.1, not re-authored here):
0 modifiers = Standard, 1 = Veteran, 2-3 = Champion. Boss is authored, not
modifier-derived. There is no separate "elite" field — what this document
previously called an *elite* is any enemy at Rank Veteran or above.

| Pack rarity | Visual | Composition (Archetype / Rank / Modifiers) | Drop behaviour | Frequency at T20 |
|---|---|---|---|---|
| **Common pack** | none | 4-7 Standards (0 modifiers), archetype mix | Standard loot roll, low quantity | ~60% of packs |
| **Marked pack** | pack-wide aura | 6-9 Standards + 1 Veteran (1 modifier) | +1 guaranteed item, rarity floor Uncommon | ~28% |
| **Severed pack** | heavy aura, audible | 8-12 Standards + 2 above-Standard, at least one Champion (2-3 modifiers) carrying the pack modifier | +2 items, rarity floor Exceptional, guaranteed material | ~10% |
| **Frontier pack** | unmissable, visible across the map | 1 named Champion + retinue | Rarity floor Exceptional, elevated Aberrant chance, always drops a boss-table token | ~2% |

**Elite (Veteran+) stat chassis — reconciled to Encounter-Design §1.1.** The old
gym `ConfigureElite` values (1.5x scale, 3x health, 2x damage) are superseded.
The canonical chassis is Encounter-Design §1.1, adopted here verbatim:

| Property | Value |
|---|---|
| Visual scale | 1.25x |
| Health | 2.0x base, +0.35x per modifier beyond the first |
| Damage | 1.5x |
| Stagger resistance | 2.0x |
| Loot floor | Exceptional |

Health therefore reads 2.0x at Veteran (1 modifier) and 2.35x / 2.70x at
Champion (2 / 3 modifiers). No values are authored here — these are
Encounter-Design §1.1's numbers, referenced. Named Champions in Severed and
Frontier packs still need distinct authored *behaviour* on top of the chassis,
not further stat multiples.

**Rule: pack rarity is visible from a distance.** The player must be able to route-plan on sight. A player who chooses to run past every Common pack and only fight Severed and Frontier packs is playing the mode correctly and should be rewarded for it — that is a build decision (movement speed, burst, survivability) expressing itself as a farming strategy.

### 4.4 The modifier system

This is the mode's replayability engine and its largest balance risk. Structure:

**Modifiers are rolled onto the Frontier instance before entry, visible before entry, and the player may re-roll at a material cost.** Count = `floor(tier / 4)`, cap 4.

Three modifier classes:

**Class A — ENEMY modifiers.** Authored on the enemy side. Cannot interact with player stat aggregation, so they carry no multiplicative-explosion risk.

| Modifier | Effect | Reward mult |
|---|---|---|
| Entrenched | Enemies have +60% armour | +12% |
| Swarming | Pack sizes +50%, individual health -25% | +14% |
| Volatile | Enemies detonate on death for area physical damage | +16% |
| Hardened | Weak-point multiplier reduced to 1.25x on all enemies | +18% |
| Relentless | Enemies do not stagger; Stagger Resistance is irrelevant | +15% |
| Legion | Frontier pack count doubled, all others -20% | +20% |
| Bulwarked | All enemies gain a shield equal to 40% of health | +14% |

**Class B — FIELD modifiers.** Authored on the level. Also aggregation-safe.

| Modifier | Effect | Reward mult |
|---|---|---|
| Unstable Ground | Floor sections periodically erase and reform | +15% |
| Low Gravity | Global gravity -35%. Jump height and air time up; air control unchanged | +10% |
| Pressure | A slow, map-wide advancing erasure line. The map has a time limit expressed as geography | +25% |
| Occluded | Fog; effective sight range halved | +12% |
| Reactive | Killing an enemy briefly buffs the nearest surviving pack | +16% |

**Class C — PLAYER modifiers.** Constrain the player. **These are the dangerous ones and the list is deliberately short.** Every Class C modifier must be a *subtraction or a rule change*, never a stat penalty, because a stat penalty enters the additive Increased bucket and interacts unpredictably with gear.

| Modifier | Effect | Reward mult |
|---|---|---|
| Sealed | Shields do not recharge inside the Frontier | +20% |
| Dry | Ammo Returned on Kill does not function | +14% |
| Exposed | Block and Dodge do not reduce or evade damage, but still generate class resource | +22% |
| Grounded | Air jump does not function | +18% |
| Bare | No healing from Life on Hit or Life on Kill; regeneration only | +20% |

**Class C audit against class resource generation (O1).** Every Class C modifier
checked against the generation tables in `Docs/Design/Class-Kits.md`:

- **Exposed — RESOLVED, re-scoped above.** Under O1's passive block/dodge, the
  original "Dodge and Block do not function" deleted Swift's *Successful dodge
  (+15 flat)* source and Tank's *Passive Block roll firing (+6)* source outright,
  while costing the other three classes nothing. It also disabled Caster's SB9
  (Reprisal) trigger. The re-scope keeps the defensive subtraction — the roll
  still fires, it just no longer mitigates — so the modifier remains a real
  difficulty increase without being a class-selective resource deletion.
- **Dry — AUDIT [O1]: does not collide with Gunsmith.** Checked first per
  instruction. Gunsmith's Scrap sources are Kill, Reload completed, Emptying a
  magazine, Deployable destroyed, and Deployable damage. **None of them read
  Ammo Returned on Kill**, so Dry does not touch the Gunsmith economy; if
  anything it raises reload frequency and therefore Scrap. The actual collision
  is **Swift**: node F8 (Ammunition Economy) generates 5 Momentum per Ammo
  Returned on Kill trigger, and Dry zeroes that node's entire output. F8 is one
  node of a twelve-node branch and Swift's other seven generation sources are
  untouched, so this is a node-level dead-pick, not a loop kill. Flagged, not
  redesigned.
- **Sealed — AUDIT [O1]: collides with Support, scope ambiguous.** Support's
  Charge source *"Healing or shielding done to self, +1 per 3% of own maximum
  health"* is the explicit anti-7.10.6 solo clause and is non-negotiable per
  Class-Kits §5. If "shields do not recharge" is read as *shield regeneration
  only*, Support is unaffected. If it is read as *no shield may be applied*, it
  deletes a named-mandatory Support generation source and also disables Tank's
  Leech overheal-to-shield conversion. **The scope of "recharge" is undefined and
  must be pinned to regeneration-only.** Owner call; not resolved here.
- **Bare — AUDIT [O1]: collides with Support if read broadly.** As written it
  subtracts only the *Life on Hit / Life on Kill affixes*, which are gear, not a
  class loop — safe. But the trailing clause "regeneration only" invites the
  broader reading "no healing except regeneration," which would zero Support's
  two largest Charge sources (healing done to allies, healing or shielding done
  to self) and gut Medic. **The narrow affix-only reading must be the normative
  one.** Owner call; not resolved here.
- **Grounded — no generation collision.** Swift's *Airborne (+8/s)* source is
  credited from any airborne state, and the base kit's jump, dash, and slide
  remain. Air jump is a tree verb; per the fourth FORBIDDEN line, disabling a
  verb some players never bought is acceptable and intentional. Reduces Swift's
  air-time ceiling; does not remove the source.

**FORBIDDEN in the modifier system — these lines must never be crossed:**

- **No modifier may grant the player anything.** Not a verb, not damage, not a resource. A modifier that helps the player is a reward, and rewards are loot. This closes the whole "modifier as build enabler" design space and it should stay closed.
- **No modifier may create a damage multiplier of any kind on the player side.** Crit is the only multiplier of its kind (6.3). "Enemies take double damage from behind" is a second multiplier wearing a costume. Rejected.
- **No modifier may add a percentage to a player stat.** Class C is subtractive/rule-based only. This is what keeps the modifier system entirely outside the flat/Increased/More pipeline.
- **No modifier may require a verb the player might not own.** "Grounded" disables air jump for everyone including players who never bought it — that is a flat reward bonus for Bulwark players and that is acceptable and intentional. But no modifier may *require* air jump or parry to complete.
- **No modifier may disable a class resource generation source.** (O1.) Class
  resource loops are the class layer, not the affix layer, and a modifier that
  switches one off is not a difficulty knob — it is a class ban wearing a
  costume. This line is why Exposed is scoped to the *mitigation* half of
  block/dodge and leaves the *generation* half intact, and it is the test every
  future Class C modifier must pass.
- **Exposed is the only modifier touching the defensive layer, and it removes the mitigation entirely rather than reducing it.** Percentage reductions to Dodge % and Block % would interact with the additive bucket. Binary off does not. Under O1 the block/dodge rolls still *fire* — they simply do not reduce or evade damage — so Swift's dodge-to-Momentum and Tank's block-to-Grit conversions continue to pay out.

**Stacking rule:** reward multipliers are **additive with each other**, applied to quantity and rarity weight separately (see 4.6). Four modifiers at +20% each yields +80%, not +107%. This is a deliberate refusal of multiplicative reward scaling; it is the same discipline as the stat pipeline and for the same reason.

**Conflict pairs:** the roller must never produce Swarming + Legion, or Occluded + Pressure. Author a conflict matrix in the modifier Data Asset.

### 4.5 The map boss

Every Frontier ends with a map boss. Requirements:

- **3-4 bosses per tileset**, rolled per instance. The player should not know which one they are running until Ingress.
- **2-4 minutes at appropriate gear.** Longer than this and it becomes the run's bottleneck; players will optimise by re-rolling for the fast boss, which is a failure state.
- **Fought with base kit only.** No arena mechanic may require air jump, parry, a timed dodge, or a grapple. (§0.) A boss may *reward* mobility — a phase where a wall-ride line reaches a weak point 25% faster — but never require it.
- **Boss caps on DoT stacking and armour reduction** per 7.10 risk 5, without making status builds feel disabled. Recommended shape: DoT stacks from a single source cap at their normal maximum but boss armour reduction caps at 60% of applied value rather than being immune.
- **The boss owns a dedicated T-1 table.** This is one of the three legal T-1 sources (3.1). A map boss should have a small, per-boss list of 3-5 T-1 affixes it can drop directly, so the community learns "you farm Boss X for the T-1 Slide Momentum Retention." This is the single most effective way to make an infinite mode feel targeted.
- No boss may be immune to a damage type. Immunity invalidates builds; caps constrain them.

**NOTE [O18] — seed targets now exist and this bullet diverges from them.** O18 sets boss encounters at **20–45 seconds unless a special enemy**. The "2-4 minutes at appropriate gear" figure above is 3-6× that band. Either the Frontier map boss is one of O18's sanctioned *special* enemies (a multi-phase set-piece rather than a rank-and-file boss), or the 2-4 minute figure is a divergence to be resolved when wave mode reports. **Not resolved here** — the figure is unchanged under O2 and this is recorded as an owner item.

### 4.6 Reward structure

The Frontier reward package, per completed run:

| Source | Yield |
|---|---|
| Packs | Per-pack rolls per 4.3. The bulk of item volume. |
| Map boss | 3-5 guaranteed items at the rift's rarity weights with a +2 item level bonus, plus a roll on the boss's T-1 table |
| Completion cache | 1 item at rarity floor Exceptional, 1 re-roll material, 1 Frontier access token |
| Frontier pack tokens | Consumable, spends at the boss arena to add one extra boss-table roll |

**Modifier reward multipliers apply to:** item *quantity* from packs, and the Aberrant/Anomalous *rarity weights*. They do **not** apply to the boss's guaranteed drops or the completion cache — those are the floor that makes a low-modifier run still worth doing.

**Frontiers out-reward equivalent-tier Local Rifts by ~35% in items per minute.** That gap is the mode's reason to exist. Local Rifts above T15 remain viable for players who prefer them but are explicitly the lower-yield option.

**Tuning validation [O18].** Every reward figure in this section is expressed per *run* or per *minute*, so it is only as stable as the TTK the run assumes. With O18's seed targets in place, Frontier tier tuning validates against **divergence from those targets** (trash a little under 1s scaling exponentially; rare/elite ~3s; boss 20–45s unless a special enemy; TTD 4–5s bare, substantially higher with resources and sustain invested), not against a re-derived internal baseline. If a tier's measured TTK diverges, the items-per-minute gap in this section moves with it and must be re-checked rather than assumed. No reward value changes in this pass (O2).

**Access:** Frontiers are entered with an access token, dropped from Frontier completion caches and from Local Rifts at T10+. A completed Frontier yields slightly more than one token on average (~1.15), so the mode is self-sustaining with a small surplus and a bad run streak cannot lock a player out. **Do not make tokens tradeable or the surplus large enough to hoard.**

**Tier progression:** completing an Frontier at tier N drops tokens for tier N+1 at ~40%, N at ~50%, N-1 at ~10%. Players climb by playing, not by grinding a currency.

### 4.7 Death and retry — HARSHER THAN LOCAL RIFTS

- **One death per Frontier instance.** The second death ends the run; the player keeps everything looted so far and is returned to the Anchor. The access token is consumed.
- Rationale: the mode's core decision is *which packs to fight*, and that decision is meaningless without a real cost to being wrong. A free-respawn Frontier is a mode where the correct play is always to fight everything.
- **Party:** the death budget is per-party, not per-player: 1 + floor(party_size / 2). A 5-player party gets 3. Full wipe ends the run regardless of budget.
- **CONFLICT flag with 11.1 (solo primary):** a solo player gets 1 death; a 5-player party gets 3 for 5 players, i.e. fewer per capita. This is intentional and worth stating: group play is more forgiving in aggregate and less forgiving individually. If playtest shows solo runs failing at a materially higher rate than group runs at equivalent gear, **raise the solo budget to 2 rather than lowering the group budget** — solo is the balance target.

### 4.8 Acceptance criteria — Frontiers

- [ ] Median solo run time at appropriate gear is 12-18 minutes including boss.
- [ ] A player can identify every pack's rarity from across the open field without aiming at it.
- [ ] Two instances at the same tier with the same modifiers produce visibly different layouts.
- [ ] Every generated layout passes the tile movement contract check at cook time (≥6 m wall-ride surface, one sliding descent, one conventional route per tile).
- [ ] Every generated layout has a spanning route from Ingress to Boss Arena with no unreachable exits, verified by the assembler, not by playtest.
- [ ] No modifier grants the player anything.
- [ ] No modifier adds or subtracts a percentage from a player stat.
- [ ] No modifier disables a class resource generation source. (O1.) Verified by running each Class C modifier against all five generation tables in `Docs/Design/Class-Kits.md`, not by playtest.
- [ ] No modifier combination produces an unwinnable run at intended gear. Verified against the conflict matrix.
- [ ] The map boss is beatable using only walk/sprint/jump/crouch/dash/slide and a weapon.
- [ ] Token economy sustains: 200 simulated runs at a 60% success rate never produce a token count of zero.
- [ ] Tier, modifier count, enemy level, and item level all derive from one integer plus one Data Asset curve. No hand-authored tier tables in C++. (7.9)
- [ ] A player who runs 50 Frontiers and never sees an Anomalous item has still made measurable build progress via Exceptional bases and materials.

---

## 5. DUNGEONS — one-page treatment

**4 players. Handcrafted. Instanced. Repeatable.**

**Function:** the targeted-farm mode. Where an Frontier gives volume and variety, a Dungeon gives *specificity*. Each Dungeon owns a small pool of Aberrant signature affixes (4.5) that drop nowhere else. A player who wants "sliding applies Bleed to enemies you pass through" knows exactly which Dungeon to run.

**Structure:** 3 encounters, ~20-25 minutes.
1. **Opener** — a fight that teaches the Dungeon's mechanic in a low-stakes form.
2. **Middle** — the same mechanic under pressure, plus a traversal/puzzle segment that four players solve by splitting into two pairs.
3. **Boss** — the mechanic as the boss's core loop.

**The four-player design question:** four is too few for role lock and too many for pure solo scaling. Recommendation: **no role requirements.** A Dungeon must be completable by four of any class, including four Supports. The party-size benefit is *simultaneity* — mechanics that need two things done at once, or four sockets covered, not "a healer." This preserves 11.1 and keeps matchmaking possible.

**Scaling:** enemy count and elite density scale with party size; health scales only mildly. (11.2 — do not scale only health.) A Dungeon should be soloable at significantly over-tuned gear, as an aspirational flex, but never as the efficient path.

**Lockout:** none, but the Aberrant signature drop is once-per-week-per-signature per character, with the remaining loot uncapped. Farming for volume happens in Frontiers; Dungeons are visited with a purpose.

**Reward:** Dungeon-exclusive Aberrant signatures, Exceptional bases at good item level, and crafting materials weighted toward tier upgrades.

**Recommend 6 Dungeons at launch**, one per Frontier tileset plus two.

---

## 6. RAIDS — one-page treatment

> **RULED [O31] — this section's brief has changed and the treatment below
> predates it.** O31: *content shape is a cross between **Destiny and Path of
> Exile**. Raids are **puzzles rewarded for team play** — many distinct
> encounters that force players into different situations. Builds may excel in
> some situations and be weak in others, but **every build must be able to make
> an impact and feel player power**. No encounter may have a build that cannot
> participate.*
>
> **Four things O31 changes about the treatment below.**
>
> 1. **"Puzzle" is now the primary noun, not a segment type.** The structure
>    diagram has one "traversal/puzzle" leg between two encounters. Under O31
>    the *encounters themselves* are the puzzles; a raid is many distinct
>    problems, and combat is how a solved problem is executed rather than the
>    thing the puzzle interrupts. The four-checkpoint shape survives; what sits
>    inside each checkpoint does not.
> 2. **"Many distinct encounters" raises the count.** The diagram authors two
>    encounters, a gauntlet and a final boss. That is a Destiny raid's skeleton
>    at its thinnest. O31 asks for a roster of situations, which is a content
>    volume question this treatment does not cost. **Not costed here.**
> 3. **The seven-player split becomes the delivery mechanism, not a flavour
>    note.** The "seven is deliberately awkward" paragraph below already
>    proposes asymmetric 3/4 and 2/2/3 splits. Under O31 that stops being a
>    signature and becomes the *structural answer* to "rewarded for team play" —
>    the reward is for the split being solved, not for damage dealt.
> 4. **A fifth encounter constraint is added, and it is the strictest one in
>    this document.** See below.
>
> **THE PARTICIPATION FLOOR.** No raid encounter may reduce any legal build's
> contribution to zero. **Weak is allowed and is explicitly endorsed by O31;
> excluded is not.** The distinction is testable: if there is no action a build
> can take that advances the encounter, the encounter is illegal, however
> flavourful the reason.
>
> `Core-Constellations.md` §9 and `Encounter-Design.md` §0 name the same three
> hardest cases, and a raid encounter must be checked against all three:
>
> | Build | Excluded by |
> |---|---|
> | **FIXATION** (Precision keystone) | Any phase where damage must land on more than one target at once. Off-target Critical Chance is **zero**, not reduced. |
> | **TERMINAL** (Affliction keystone) | Any phase where the target does not die and has no reachable weak point. Banked damage is delivered on death, expiry, or a weak-point hit — otherwise nothing is delivered at all. |
> | **PERPETUAL** (Volley keystone) | Any short burst window. It cannot reload; a four-second damage window opening once per phase excludes it structurally. |
>
> **The cheap general fix, recorded as a design pattern rather than a rule:**
> give every encounter at least two lanes that advance it — typically a damage
> lane and a mechanic lane (carry, interrupt, position, solve) — and let a build
> weak in one be strong in the other. That is also what makes "puzzles rewarded
> for team play" and "every build participates" the *same* design rather than
> two constraints in tension. **It is a pattern, not a ruling; the owner may
> want a different answer.**
>
> **Not audited:** no encounter in this section or in `Encounter-Design.md` has
> been checked against the floor, and the slice boss's phases predate O31
> entirely. Recorded in OPEN QUESTIONS.

**7 players. Handcrafted. Instanced. Checkpointed.**

**Function:** the game's ceiling. The most reliable T-1 source and the home of a small set of Raid-exclusive Anomalous items that each imply a whole build (4.6).

**Structure:** a large continuous map, ~60-90 minutes for a competent group, four checkpoints.

```
  ENTRY -> [CP1] Encounter A -> traversal/puzzle -> [CP2] Encounter B
        -> [CP3] gauntlet + mini-boss -> [CP4] FINAL BOSS
```

**Checkpoint rules:**
- Checkpoints persist for the group's lockout period. A group that reaches CP3 on Tuesday resumes at CP3 on Thursday.
- Checkpoints restore ammunition and reset cooldowns. They do not heal — that is what the encounter's own recovery windows are for.
- **The group's roster may change between sessions but not mid-encounter.** Backfill is allowed only at a checkpoint.

**The seven-player question:** seven is deliberately awkward. It does not divide into two, three, or four evenly. **Use that.** The mode's signature should be encounters that split 3/4 or 2/2/3 and require the groups to be asymmetric — one group fights, one group solves, and they swap under time pressure. Seven is the number that makes "who goes where" a real conversation.

**Encounter design constraints (inherited, restated because raids are where they get violated):**
- No encounter requires a timed dodge input. Dodge is passive. (§0.)
- No encounter requires air jump or parry — those are tree verbs and the group cannot assume anyone has them.
- No encounter requires a specific class. A raid that needs a Tank is a raid that cannot be matchmade or scheduled.
- Boss DoT and armour-reduction caps apply, per 7.10 risk 5.
- **[O31] No encounter reduces any legal build's contribution to zero.** The
  four constraints above forbid *requiring* something; this one forbids
  *excluding* something, which is the failure mode they do not cover. A build
  may be weak; it may not be a passenger. Test against FIXATION, TERMINAL and
  PERPETUAL — see the O31 block at the top of this section.

**Rewards:** guaranteed T-1 affix from the final boss (one per character per lockout), Raid-exclusive Anomalous items at a low rate, and the largest crafting material payout in the game. **Raids must not be a required stop for build completion** — everything a Raid drops must have a slower path elsewhere, or 11.1's solo balance target is a lie.

**Lockout:** weekly, per character, on the reward — not on entry. Groups may re-run for practice.

---

## 7. CONQUEST — one-page treatment

**9 players, matchmade. Handcrafted maps, procedural front-line state. Repeatable.**

**Function:** the low-commitment, high-volume, drop-in mode. Reclaiming land. Thousands of enemies, mini-bosses, a warzone. It is the mode a player runs when they do not want to think.

**Structure:** a persistent-feeling battle over a large handcrafted map divided into 5-7 sectors. Sectors are contested, captured, and lost. The session is 20-30 minutes and ends when the map is fully reclaimed or the timer expires — **both outcomes pay out**, scaled by sectors held.

**Matchmaking:** nine players, no roles, join-in-progress enabled. Leaving does not fail the session. This is the mode that must tolerate a chaotic player population, so nothing in it may require coordination. **Design rule: every objective must be completable by one player, faster by several.** Coordination is an efficiency multiplier, never a gate.

**Scaling:** enemy *count* is the primary knob, per 11.2. Conquest is the mode where enemy count scaling has room to be dramatic — hundreds of low-health enemies is a distinct combat texture and it is the mode's identity. Performance budget is the real constraint here, not balance.

**Mini-bosses:** spawn per sector on capture-contest. Roughly Frontier-pack strength. They are events, not walls.

**Rewards:** high item *quantity* at moderate rarity, and — importantly — **Frontier access tokens at the highest rate in the game.** Conquest's economic role is to feed the Frontier loop. A player who prefers chaos to precision can play Conquest exclusively and still fund an Frontier-tier gear chase, they just do it more slowly.

**Risk to watch:** with 9 matchmade players and no failure state, Conquest can become the AFK-optimal mode. Mitigation: reward scales with *personal contribution weighted by sector participation*, and the completion cache requires having been present for at least one sector capture.

---

## 8. Build order recommendation

1. **Local Rift, Short class, objectives 1 and 2 only, tier 1-5.** Wraps the existing gym in a real loop. Proves the threshold/body/anchor/ritual skeleton.
2. **The closing ritual.** Cheap, high narrative leverage, and it should exist before there are four hundred rifts to retrofit.
3. **Remaining six objective types + Standard class + tiering to T15.**
4. **Frontier: tile assembler and the movement contract check.** The riskiest technical piece; validate it before content depends on it.
5. **Frontier: pack rarity, modifiers Class A only.** Class A is aggregation-safe and proves the reward multiplier maths.
6. **Frontier: map boss + T-1 tables + token economy.**
7. **Modifiers Class B and C.**
8. Dungeon → Raid → Conquest, in that order. Each depends on the prior mode's encounter tooling.

Blocking prerequisites from the master sheet that this document depends on: item level (4.8, 6.5 — exists now per CONTEXT.md), stat aggregation buckets (6.6 — locked per CONTEXT.md), a real TTK (blocks every number here), and elemental resistances (blocks any elemental modifier — none are proposed above, deliberately). **Element set is ruled and final [O19]: Rift / Entropy / Void** (Time renamed to Entropy); only the resistance model is still outstanding, not the names.

---

## 9. OPEN QUESTIONS

1. ~~**Naming.**~~ **CLOSED by O8** — the content type is **Frontier**. See §1. One residual gap remains: the §4.3 "Frontier pack" rarity tier now shares the content type's name and needs its own owner naming decision (flagged in §1).
2. **Intended time from level 50 to a finished build** (9.4, unresolved upstream). Every drop rate, token rate, and reward multiplier in §3.7 and §4.6 is unanchorable until this number exists. This is the single largest blocker on this document.
3. **Solo death budget in Frontiers.** §4.7 gives solo 1 death and a 5-player party 3. Per capita this is harsher on groups but in aggregate harsher on solo. Does this survive the solo-primary balance target, or does solo need 2?
4. **Does Conquest's contribution-weighted reward create a griefing or kill-stealing dynamic** at 9 matchmade players? Needs a scoring model before implementation.
5. **Are Frontier tiers infinite or capped at 30?** (9.4 open item, restated for this mode.) Infinite tiers are a soft paragon track and arguably contradict the 7.1 hard stop in spirit even though they grant no power. Recommendation: **cap at 30**, then let modifier count and pack density be the ceiling.
6. **Do Raid-exclusive *legendaries* have a slower non-Raid path?** §6 asserts they must, for 11.1. If they do not, group content gates build completion and the solo balance target is violated. Needs an explicit yes. **[O32] Re-read: this question was written as "Anomalous items" and means LEGENDARIES** — see the vocabulary correction at the top. That makes it a bigger question than it looks: O32 rules that the fix for legendary cadence is authoring more legendaries, and only three exist (Boots, Primary, Waist). **Committing to Raid-exclusive legendaries commits to authoring named items off a list that has five empty slots and no owner.**
7. **How many tile modules does the Frontier assembler need before layouts stop feeling repetitive?** Estimate 25-35 per tileset. This is a content-cost question with a large budget implication and it should be answered with a prototype, not an estimate.
8. **Loot distribution in party content** (11.3, unresolved upstream). This document assumes instanced-per-player throughout. If that assumption is wrong, every party mode's reward section changes.
9. **Does Anchor defense exist as a content type?** (10.3 open.) Not designed here. It would most naturally be a Conquest variant with an inverted objective, which is cheap — but it competes with Conquest for the same player time.
10. **Do modifiers persist per-account as unlocks, or roll from a global pool?** §4.4 assumes global pool. Unlockable modifiers would give the endgame a progression texture without granting power, which is attractive — but it is close to a horizontal post-cap unlock, which 9.3 explicitly holds in reserve and says not to build.
11. **Elite modifier list** (10.3 open). §4.3 assumes elites exist with authored behaviour beyond stat multiples. That list is a separate design pass and Severed/Frontier packs are blocked on it.
12. **Does the ~2% silhouette in the Act III+ closing ritual (§3.5) survive a "no unexplained content" review?** It is deliberately never acknowledged. Confirm this is wanted before it is built, because it is the kind of thing that gets cut in QA as a bug.
13. **[O31] Has any encounter, in this document or in `Encounter-Design.md`, been audited against the participation floor?** No. §6's four inherited constraints all forbid *requiring* something and none of them forbids *excluding* something, which is the failure O31 names. The slice boss's phases predate the ruling entirely. Needs a design pass with FIXATION, TERMINAL and PERPETUAL as the test cases — see §6's O31 block.
14. **[O31] Is "many distinct encounters" a content-volume commitment, and how many?** §6's structure diagram authors two encounters plus a gauntlet plus a boss. O31 asks for many distinct situations, Destiny-style. That is a volume question with a real budget behind it and this treatment does not cost it.
15. **[O29] Does item level 120 change the mode reward tables?** §3.7's rift tiering, §4.6's Frontier rewards and §6's guaranteed T-1 were all authored against a T8..T-1 ladder and a 50-ish item level ceiling. O29 runs item level to **120** and widens the ladder to **T12..T-1** with values tuned up and back-loaded at the high end. **A guaranteed T-1 means something different now**, and which mode supplies which *item level* band is unauthored anywhere. This interacts with open question 2, which is already named as the largest blocker here.
