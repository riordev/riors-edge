# Design Overview — the map, the locks, the collisions, and what to build next

Status: synthesis pass 1. This document does not author new systems. It reads the master sheet and the seven design documents written against it, and produces four things the individual documents cannot produce for themselves:

1. a map of the design space, including the domains nobody owns;
2. the decisions those documents have effectively locked;
3. every `CONFLICT` and `EXTENDS` label, with a recommended resolution;
4. the cross-document contradictions none of the seven authors could see, because each was writing alone.

It closes with a build-order recommendation and a single deduplicated, ranked list of the decisions that need the project owner.

**Authority.** `Docs/Design/Master-Sheet-Import.txt` is law. Where this document disagrees with it, the disagreement is labelled and the master sheet still wins until the owner rules. Where this document disagrees with one of the seven design docs, this document wins, because it can see the other six.

**Corpus read for this pass:** Master-Sheet-Import.txt, Core-Constellations.md, Class-Kits.md, XP-And-Pacing.md, Encounter-Design.md, Game-Modes.md, Save-Architecture.md, UI-UX-Spec.md, Art-And-Modelling-Plan.md, plus `CONTEXT.md`, `Docs/Layer-Ownership.md`, `Docs/Character-Progression-Architecture.md`, `Docs/Item-Foundation.md`, `Docs/Roadmap.md`.

---

## 1. Map of the design space

### 1.1 Coverage

| Domain | Primary owner | Depth | State |
|---|---|---|---|
| Premise, setting, terminology | Master §1 | Full | Stable. Naming constraints are law. |
| Antagonist, erasure fiction, final choice | Master §1.4–1.7, §8 | Full | Stable, story-only |
| Gear slots and the three binding rules | Master §2 | Full | Stable |
| Affix tables, tiers, prefix/suffix | Master §3 | Full list, **placeholder values** | Blocked on TTK |
| Loot rarity, budget, roll pipeline, crafting verbs | Master §4 | Structure only | Implemented through step 5; step 6 unbuilt |
| Movement base kit and guardrails | Master §5, `Movement-Design.md` | Full | Implemented |
| Damage order, armour, crit, DoT snapshot | Master §6, `Combat-Foundation.md` | Full | Implemented; **two proposed new steps unmerged** (§5.7) |
| Progression currencies and schedules | Master §7 | Full | Framework implemented, no content |
| **Core Tree — six constellations, 66 nodes** | `Core-Constellations.md` | Full | Design complete; Elements BLOCKED |
| **Class kits — resources, abilities, branch trees** | `Class-Kits.md` | Swift + Caster full; other three one-page | Design complete for the prototyping order |
| **XP curve, pacing, act breaks, item level** | `XP-And-Pacing.md` | Full | Design complete; hours are a hypothesis |
| Story acts, locations, delivery | Master §8 | Full | Stable |
| Endgame thesis, chase structure | Master §9 | Thesis only | "Time to build complete" unanswered |
| **Content modes — rifts, Anomalies, dungeon/raid/conquest** | `Game-Modes.md` | Rifts + Anomalies full; three one-page | Design complete; naming unresolved |
| **Encounters — elites, enemies, boss, waves, scaling** | `Encounter-Design.md` | Full for the slice | Design complete |
| Party play policy | Master §11 | Position only | Scaling implemented in `Encounter-Design.md` §6 |
| Weapon archetypes and data boundary | Master §12, `Weapon-Foundation.md` | Full | Five archetypes implemented |
| NPC roles and characters | Master §13 | Full | Stable, story-only |
| **UI / UX / HUD / tooltip / trees** | `UI-UX-Spec.md` | Full | Design complete |
| **Save, state, versioning, server path** | `Save-Architecture.md` | Full | Step 0 of 8 implemented |
| **Art direction and production sequence** | `Art-And-Modelling-Plan.md` | Full | Nothing authored |

### 1.2 Domains nobody owns

These are not gaps in a document. They are gaps in the *set* of documents, and several are load-bearing for work already scheduled.

| Unowned domain | Who is already depending on it | Cost of continuing to ignore it |
|---|---|---|
| **Audio design** | Encounter telegraphs ("audible rising tone", "whip-crack"), Game-Modes closing ritual ("audio drops to a single tone"), Art dodge/block feedback, UI ultimate-ready cue | Every telegraph in `Encounter-Design.md` §2 assumes an audio channel that has no owner, no palette, and no mix budget. Telegraph tuning cannot be validated without it. |
| **Enemy taxonomy** | `XP-And-Pacing.md` §5.1 (seven tiers), `Encounter-Design.md` §2 (three archetypes + modifier count), `Game-Modes.md` §4.3 (four pack rarities) | Three incompatible vocabularies already exist. See §5.4. |
| **Economy — currency, vendor pricing, salvage yield, material sources** | `UI-UX-Spec.md` §7 (shows "CREDITS 12,480", "240 mat"), `Game-Modes.md` §4.6 (token economy), `Save-Architecture.md` §2.1 (account currencies) | Three documents draw currency UI and reward tables against a currency system that has never been designed. |
| **Crafting system proper** | Master §4.7 names five verbs; UI §7.1 draws the screen; Game-Modes gates T-1 through it; Save §4.3 makes it a transaction | Nobody has designed costs, success/failure rates, escalation curves, or material sources. The Forge is the highest-interaction NPC in the game and its system is five bullet points. |
| **Aberrant signature list and Anomalous item list** | `Game-Modes.md` §5 (dungeons own signature pools), `Art-And-Modelling-Plan.md` §3.3 (one bespoke mesh per Anomalous), UI §5.3 (signatures render first) | Master §4.5/§4.6 give five examples each, explicitly as illustrations. Dungeons and Anomalous art are both blocked on a real list. |
| **Ability implementation architecture** | `Class-Kits.md` §7 says rule-rewrite nodes "need a code-side hook and should be enumerated before any of them is authored" | ~90 class nodes and ~66 core nodes contain perhaps 40 distinct rule-rewrite hook types. That enumeration is the actual engineering spec and it does not exist. |
| **Networking / replication plan** | Everything. Master calls the game an MMO; the Anchor is non-instanced | `Save-Architecture.md` §7 covers *storage* authority only. Nothing covers replication topology, listen-host vs dedicated, or the Anchor's shared-world layer. |
| **Telemetry pipeline** | `Encounter-Design.md` §4.3 lists ten per-wave metrics; XP §3 says "replace with telemetry"; XP §10 has measured acceptance criteria | Every acceptance criterion phrased as "measured, not estimated" needs a pipeline that does not exist beyond the gym's clipboard report. |
| **Onboarding / tutorial rift** | `XP-And-Pacing.md` §2 grants it a 3,000 XP bolus; §7 hangs Core Point #1 on it | The first ten minutes of the game are a line item in an XP table. |
| **Performance budget and target platform** | Art §7.5 triangle budgets, §6.3 paper-doll GPU budget, Game-Modes §7 Conquest "thousands of mobs" | Art OQ10 flags this. Conquest at 9 players with thousands of enemies is a platform decision wearing a game-mode costume. |
| **Localization** | UI OQ8 (generated vs authored item names) | Cheap now. Item name generation is the specific thing that becomes expensive after content authoring. |

---

## 2. The ten highest-leverage decisions now locked

These are decisions the seven documents made that resolve real ambiguity, are internally validated, and should be treated as settled unless the owner objects. Each carries the cost of reversing it.

| # | Decision | Source | Why it is high leverage | Cost to reverse |
|---|---|---|---|---|
| 1 | **26 points per constellation → a hard maximum of two Core keystones per character, ever.** | Core-Constellations §2.2 | This is the single number that makes the six-way constellation choice real. 20-point trees give three keystones and the choice collapses; 32-point trees make the third constellation meaningless. It is derived from, and validates, Master §7.4. | Low now (Data Asset), catastrophic after node content is authored and balanced |
| 2 | **Multishot-generated projectiles carry proc coefficient 0 for status, on-hit, and node triggers, and 1 for damage. Ricochets carry 0.5 and cannot chain.** | Core-Constellations §4 | This is the actual mitigation for Master §7.10 risk 1. Without it, Volley × Affliction × Elements is unbounded and the master sheet's stated first balance risk arrives through the tree layer. | Low now, unbounded later |
| 3 | **XP curve: accelerating, act-scoped multipliers, one deliberate discount at levels 28–31. Total 4,770,050 XP; ~40 hours solo to 50.** | XP §1–3 | Makes the mechanical and narrative arcs land together three times, and picks a campaign length short enough that a second character (permanent class!) is plausible. 40 hours is a *product* decision disguised as a curve. | Low (baked table in a Data Asset); the *shape* is the commitment, the multipliers are the knob |
| 4 | **Item level is zone-anchored hybrid: `ZoneLevel + TierBonus(0..+5) + Variance(−1/0/+1)`.** | XP §8 | Resolves a Master §4.9/§6.5 open item. Keeps rarity and item level doing genuinely different jobs (count vs tier), makes zones authorable, and lets "do enemies scale to player level" be answered independently. | Cheap now, "miserable to retrofit" per Master §4.8 — and the enemy-level fallback will silently become shipping behaviour if deferred |
| 5 | **No rested XP. Account-wide flat Veteran's Path for alts. No XP loss on death. Party XP neutral with no headcount bonus.** | XP §6 | Four retention-mechanic decisions made *against* genre default, each justified by a locked constraint (hard stop, permanent class, solo-primary). The party-XP ruling in particular is what stops solo from becoming the punished path. | Low |
| 6 | **Class branches are 12 nodes / 26 points across five tiers, keystone gated at 16 → exactly one class keystone per character. Each class may author at most three More multipliers, one per branch keystone.** | Class-Kits §0.1–0.2 | Gives the class layer the same "one big commitment" shape the Core Tree has, and is the *only* written cap on More multipliers anywhere in the project. | Low now; see §5.9 — the Core Tree has no equivalent cap and needs one |
| 7 | **Elites are modifier-driven, not stat-driven: ten modifiers, three pressure kinds, forbidden pairs, a required-diversity rule, and a reduced stat chassis (1.25× scale / 2.0× health / 1.5× damage).** | Encounter §1 | Directly implements Master §11.2's "do not scale only health" at the elite level, and converts `ConfigureElite`'s fixed multipliers into content. Every modifier has a graybox tell and a movement counterplay, which is what keeps the movement pillar load-bearing in combat. | Medium — replaces a shipped function signature |
| 8 | **Party scaling is count-first: 3.8× enemy count against 1.2× enemy health at five players, plus a role-pressure ladder (extra Lattice slot → second Warden → overlapping packs → flanking spawns).** | Encounter §6.2–6.3 | The clearest available implementation of Master §11.2. The role-pressure ladder is the part that matters: it makes each additional player add a *kind* of problem, not a quantity. | Low (table-driven) |
| 9 | **The Anomaly modifier system has three classes and four inviolable rules: no modifier grants the player anything; no modifier creates a player-side damage multiplier; no modifier adds or subtracts a percentage from a player stat; no modifier requires a verb the player may not own. Reward multipliers are additive.** | Game-Modes §4.4 | Keeps the entire endgame modifier system *outside* the flat/Increased/More pipeline. This is the discipline that lets an infinite-replay mode exist without reopening the multiplicative-explosion risk. | Low now, very high after modifier content exists |
| 10 | **Save is three-tier (account / character / session) with atomic write + rotating backups + a header hash, an append-only run journal, two-phase item transit, and load-time validation that enforces the Aberrant/Anomalous equip limits.** | Save §2–6 | Item duplication is the one bug that destroys a loot economy, and equip limits *are* the endgame decision (Master §9.2) — if the save layer does not enforce them they are advisory. All of this is cheap before there is an economy and impossible after. | Low now; unbounded after players have items |

**Honourable mentions**, locked but narrower: no item score number anywhere in the UI, ever (UI §5.4); damage numbers on by default with a mandatory 120 ms per-target aggregation window (UI §4.4); Class Tree and Core Tree are never merged into one view (UI §6.2); dodge and block produce reactive feedback with **zero world-position delta** and no triggered animation (Art §3.5); the blockout → playtest → author gate on all bespoke art (Art §7.1).

---

## 3. Label ledger — every CONFLICT and EXTENDS, with a recommended resolution

### 3.1 CONFLICT

| ID | Document | The conflict | Recommended resolution |
|---|---|---|---|
| **C1** | All seven (Core-Constellations §7/OQ1, Class-Kits OQ1, Encounter §0/OQ1, UI §3.3/OQ1, Art §3.5/OQ1, XP §12, Game-Modes §0) | Block/dodge as passive chance layers vs stamina-spending player inputs | **Already resolved; the documents are stale.** `CONTEXT.md` line 47 records the owner's ruling — passive, superseding the stance/window model — and `UBreakerCombatComponent` already exposes `DodgeChance` / `BlockChance` / `BlockMitigation`. What is stale is Master §5.2, §3.8 ("BLOCK requires a shield or stance"), §3.9, §7.7 (shared stamina), §7.11, `Layer-Ownership.md`, `Character-Progression-Architecture.md`, and `Docs/Item-Foundation.md` §"Also in this pass". **Action: propagate the ruling into those seven files and strike the shared stamina pool** (see O1 in §7 for the residual real decision). |
| **C2** | Core-Constellations §6 | Elements RESONANCE overlaps Caster/Multispell's stated fantasy | Take the document's own preferred option: mono-element restriction *is* the separation, because it is precisely what Multispell does not have. No exemption node. Do not build either until the resistance model exists. |
| **C3** | XP §8 | Hybrid item level vs `Docs/Item-Foundation.md` recording enemy level | Accept hybrid. Add a `ZoneLevel` input to `UBreakerLootLibrary` defaulting to the enemy's level so the gym keeps working, and update `Item-Foundation.md` in the same change. This is one afternoon now. |
| **C4** | Class-Kits §2.4 (VW11) | Void Whisperer's Long Debt depends on the unresolved Tick Frequency cap and snapshot question | Ship Void Whisperer without VW11. Replace the node slot with a non-tick rewrite (recommend: while Mana is negative, DoTs applied cannot be cleansed). Revisit when O10 resolves. |
| **C5** | Class-Kits §6.4 (K8 Air Work) | A class node grants a *floor value* of an affix the player may not own | **Reject the floor.** Layer-Ownership says affixes scale verbs the player owns; a class node granting an affix's baseline inverts the rule from the other side and makes the marquee `Accuracy While Airborne` affix partly redundant. Keep the "treated as one tier higher" clause, delete "if the player has none, grants the T5 value." |
| **C6** | Game-Modes §1 | "Anomalies" as a content-type name collides with the Anomalous rarity tier, against Master §1.2's stated intent | Take the recommendation: **Frontier**. Master §8.3 and §10.1 already use "Rior's frontier" for exactly this content. Zero new vocabulary, zero collision. Decide before any content string is authored. |
| **C7** | Game-Modes §4.7 | Anomaly death budget: solo 1, five-player party 3 — harsher per capita on groups, harsher in aggregate on solo | Ship solo at **2**, party at `1 + floor(size/2)`. Solo is the primary balance target and the document's own fallback position is to raise the solo budget rather than lower the group's. Cheaper to start generous and tighten. |
| **C8** | Save §7 | Loot distribution is open upstream; the save transaction model assumes instanced-per-player | Ratify **instanced-per-player**. Shared or need/greed loot introduces cross-account item transit, which is exactly the case §4.5's two-phase protocol is designed to avoid. Instanced also removes a whole class of party social friction that a solo-primary game gains nothing from. |
| **C9** | UI OQ10 | Nameplate policy (health-on-damage, 2.5 s fade) does not survive Conquest's density | Accept the deferral, but write the constraint now: **the nameplate system must take a per-mode policy asset from day one.** A hardcoded policy is the retrofit this flags. |
| **C10** | Art §0/§5 | Slice scoped to three weapon archetypes (Master §12.3) while five exist in code | Ship **three authored + two kitbashed**, as the plan proposes. The code shipping five is not a reason to author five; it is a reason to have two placeholder silhouettes that are not embarrassing. |
| **C11** | UI §4.2 (undeclared) | UI spec inverts the shipped HUD's health/shield stacking order without a CONFLICT label | Accept the inversion — shield renders above health so depletion reads downward, matching the §6.1 mitigation order — but **label it**, because it is a change to shipped, played code and the spec presents it as a note in parentheses. |

### 3.2 EXTENDS

| ID | Document | The extension | Recommendation |
|---|---|---|---|
| E1 | Core-Constellations §4 | Multishot proc coefficient 0 / ricochet 0.5, no chaining | **Adopt as law.** Promote out of the constellation doc into the combat pipeline spec — it governs weapons, classes, and items, not just Volley. |
| E2 | Core-Constellations A9 | Affliction transfer ancestry capped at depth 2 with a normalized payload | Adopt. All three mitigations (depth cap, normalized payload, proc 0) are required together, not as alternatives. |
| E3 | Core-Constellations E9 | Elements reaction chain depth 2, proc 0 on the second reaction | Adopt when Elements unblocks. |
| E4 | Core-Constellations §2.2 | 26 points per constellation → two keystones maximum | Adopt. Locked decision #1. |
| E5 | Core-Constellations §7/§8 | Nodes purchasable while inert (Read before Parry, Loft before Air Jump) | Adopt. It is a genuinely good pattern and slice acceptance criterion #5 already tests it. |
| E6 | Core-Constellations §4 (V9) | Barrage Doctrine changes multishot *geometry*, against Master §3.5's "should multiply with nothing" | Compatible — geometry is not multiplication. Adopt, but only after the authority-side deterministic generation path exists. |
| E7 | XP §2 | Tutorial rift grants a scripted 3,000 XP bolus | Adopt as a *content* grant. Do not flatten the first three curve rows; they are reused by Veteran's Path. |
| E8 | XP §6 | Veteran's Path sits outside the affix Increased bucket and is exempt from its +100 % cap | Adopt, and adopt the document's own guard: **this is the one sanctioned exception; do not add a third global multiplier.** |
| E9 | XP §8 | `EnemyLevel = ZoneLevel + PackModifier[−1,+3]`; enemies never scale to player in campaign zones | Adopt. It resolves Master §6.7's open item favourably and cheaply. |
| E10 | Class-Kits §0.1 | Each class may author at most three More multipliers, one per branch keystone | Adopt — **and extend the same discipline to the Core Tree**, which currently has none. See §5.9. |
| E11 | Class-Kits §0.2 | Branch nodes freely mixed with investment gates; 12 nodes / 26 points; one class keystone per character | Adopt. Permanent class selection already carries the "you cannot have everything" weight; a second lock is punitive. This also answers Master §7.11's first open item. |
| E12 | Class-Kits §2.1 | Caster Overcast — abilities castable to −20 Mana with doubled generation and +15 % damage taken | Adopt in design, **gate on a technical spike** (Class-Kits OQ4): a negative GAS resource attribute may break cost prediction. Spike before Caster prototyping, not after. |
| E13 | Class-Kits §4 | Tank self-damage: up to 80 % reduction plus full self-knockback control, never immunity | Adopt for Tank. It does **not** settle the general Rocket self-damage question (Master §12.5) — that is still O15. |
| E14 | Encounter §1.1 | Reduced elite stat chassis; difficulty carried by modifiers | Adopt. See §5.3 for the three-document collision this creates. |
| E15 | Encounter §6.5 | Downed state, revive scaling via Interaction & Revive Speed %, **spawn pressure pauses during a revive** | Adopt. The pause rule is the load-bearing half: without it, reviving is strictly incorrect play in a game where standing still loses. |
| E16 | Encounter §7 | Facing-dependent armour as a new step in the damage order | Adopt — it is the mechanism that makes positioning a damage stat without reintroducing momentum-to-damage conversion. **But it must be merged with the resistance step decision, not landed independently.** See §5.7. |
| E17 | Game-Modes §2 | A specific list of the ~15 world-content Core Points | **Reject in favour of XP §7's list.** See §5.1 — these are two different lists and only one can exist. |
| E18 | Game-Modes §3.4 | The Carry objective disables the Secondary weapon slot | Adopt. The swap system already exists, and it is a clean use of a shipped mechanic as an objective constraint. |
| E19 | Game-Modes §3.5 | A ~2 % unexplained silhouette in the Act III+ closing ritual | Adopt, and record it as *intentional* in the ticket, in the asset, and in the QA notes — the document is right that this is the kind of thing that gets closed as a bug. |
| E20 | Save §2.1 | Fragments are account-wide | Adopt. Fragments unlock capability, not power; re-earning them on every alt is repetition with zero build expression, and Veteran's Path already signals that alts are expected. |
| E21 | Save §5.4 | Enum discipline: append-only, never reorder, never reuse a retired value | Adopt immediately and enforce in review. `EBreakerClassId` reordering silently converting every Tank into a Support is a real, cheap-to-prevent disaster. |
| E22 | UI §1 | Tier A code-driven Slate until a data model has been stable for two milestones *and* the screen has been played | Adopt. It is the correct posture for the two-machine constraint and it is a discipline, not a preference. |
| E23 | UI §4.4 | Damage numbers: mandatory 120 ms per-target aggregation, semantic typing, 40-number hard cap, zero shown not suppressed | Adopt whole. The aggregation window is not a polish item — Multishot +3, Pierce, and shotgun pellets make it a correctness requirement. |
| E24 | UI §4.2 | Armour displays raw value *and* derived mitigation percentage | Adopt. Highest stat-literacy return per line of code in the project. |
| E25 | UI §5.3 | Tooltip ordering law: signatures → prefixes → suffixes, never interleaved; tier badges in a fixed right-aligned column | Adopt. |
| E26 | UI §6.5 | Rule-rewrite nodes render as wide prose cards, percentage nodes as compact one-liners — UI as a design-smell detector | Adopt, and enjoy it. If the tree screen fills with one-liners, the tree has drifted into the affix layer and the UI says so without anyone filing a bug. |
| E27 | UI §3.2 | Auto-lock on rarity ≥ Aberrant, any T0/T-1 affix, or equipped; two-key destructive rule | Adopt. T0 items are the only raw material for T-1; accidental salvage is materially worse here than in most ARPGs. |
| E28 | Art Pillar 3 | Teal rift chroma reserved globally to rift phenomena, Vestige interiors, Anomalous items, and suppression hardware | Adopt. It makes Anomalous items read as rule-rewriting *and* rift-derived with zero UI. |
| E29 | Art §3.3 | Gear rarity visual ladder, ~1 bespoke mesh per Anomalous | Adopt, and note the dependency: it needs the Anomalous item list that nobody owns (§1.2). |
| E30 | Art §3.3 | `Accuracy While Airborne` at T2+ adds a physical stabiliser element on HELM/GLOV | Adopt. Master §3.4 explicitly asks for this and this is the cheapest credible answer. |

---

## 4. Deferred and blocked, consolidated

| Blocked thing | Blocked by | Documents affected |
|---|---|---|
| Entire Elements constellation (11 nodes, 4 dependency tags) | No elemental resistance model in the damage pipeline | Core-Constellations §6, Class-Kits (Multispell, Void Whisperer), UI §4.5 glyphs, Encounter §7 (elemental elite modifiers deliberately excluded) |
| All elemental affix lines + Elemental Damage Reduction | Same | Master §3.2, §3.7 |
| Secondary slot exclusive affixes | **Unblocked** — the weapon swap tempo layer shipped (`CONTEXT.md`) | Master §3.12 should be updated; it still reads BLOCKED |
| Stamina-touching affixes and Bulwark `[STAMINA-DEP]` nodes | The stamina pool's existence is now itself in question (C1) | Master §3.9, §7.7, Core-Constellations §7 |
| All drop rates, rarity weights, token economy | "Intended time from 50 to a finished build" is unanswered | Game-Modes §3.7, §4.6; Master §9.4 |
| Every numeric value in every document | No real TTK | All seven, explicitly |
| Anomaly Severed/Frontier pack design | Elite modifier list — **now unblocked** by Encounter §1.2 | Game-Modes OQ11 can be closed |
| Rocket art and self-knockback pose | Self-damage rules | Art OQ2, Encounter OQ10, Master §12.5 |

---

## 5. Cross-document contradictions the authors missed

Each author wrote in isolation against the master sheet. These are the collisions that only appear when all seven are read together. They are ordered by how much work they threaten.

### 5.1 There are two different, incompatible lists of the ~15 world Core Points

`XP-And-Pacing.md` §7 authors sixteen sources pinned to story beats: tutorial rift, first Forge interaction, cumulative rift counters at 10/30/75, both act bosses, three Rior fragments, The Breach, the Altered commander, three Erased Earth completions, and the Survivor. It states the list is "now pinned to story beats" and that if the budget must move, **move node costs, not this list.**

`Game-Modes.md` §2 independently authors a completely different fifteen: eight Local Rift *archetype* first-clears, The Breach, three Erased Earth discoveries, and three Anomaly tier clears (tiers 5/10/15). It states this "matches the 7.2 schedule exactly."

They overlap on two entries. Both claim to satisfy Master §7.2. Neither author knew the other existed.

The Game-Modes list also violates two of XP's own stated design rules: three of its fifteen are endgame-gated Anomaly clears, and XP §7 rule 2 requires the points be "spread across all three acts" — XP's own OQ10 warns that if they are endgame-gated, "the two-full-plus-one-partial shape does not exist until well after level 50 and the third constellation is a post-campaign feature rather than a build decision."

**Recommendation: adopt XP §7's list.** It is act-distributed, none are missable, none require party content, and each is load-bearing on a story beat. Fold Game-Modes' good idea — one Core Point per Local Rift *archetype* first-clear — into it by replacing XP entries #3/#7/#16 (the cumulative rift counters) with archetype first-clears, which reward *variety* rather than *repetition*. That preserves both authors' intent and keeps the count at 15.

### 5.2 The UI's Core Tree wireframe is built on 18-point constellations; the design is 26

`UI-UX-Spec.md` §6.4 draws six constellation cards reading `18 / 18`, `7 / 18`, `16 / 18`, and its node card reads `Requires: 8 invested in Kinesis`. Its budget line — "41 spent · 24 remaining ▸ enough to complete Kinesis (2) and partially open one more (22)" — is arithmetic on 18-point trees.

`Core-Constellations.md` §2.1 sets **26 points** per constellation, with 18 as the *keystone gate*, not the total. The UI author appears to have taken the gate number as the size.

Under the real numbers, 65 points buys 26 + 26 + 13, and the budget-consequence line — which §6.4 calls "the whole design of this screen" — reads completely differently. The wireframe also implies six trees × 18 = 108 points of content, when the real content is 156.

**Recommendation:** the UI's *principle* (state what the remaining budget can actually complete) is correct and should survive. Redraw against 26, and make the budget line explicitly name the two-keystone ceiling, because that ceiling is the most important thing about the Core Tree and no screen currently communicates it.

### 5.3 Four documents specify three different elite stat chassis, and the art plan's cost model breaks

| Source | Scale | Health | Damage |
|---|---|---|---|
| Shipped `ConfigureElite` | 1.5× | 3.0× | 2.0× |
| `Encounter-Design.md` §1.1 (proposed replacement) | 1.25× | 2.0× + 0.35×/extra modifier | 1.5× |
| `Game-Modes.md` §4.3 | "existing gym elite behaviour … is the correct baseline" (1.5× / 3× / 2×) | | |
| `Art-And-Modelling-Plan.md` §2.1 | "+50% scale", material + scalar only, **no new mesh** | | |

`XP-And-Pacing.md` §5.1 separately maps the shipped elite to its **Veteran** tier at a 3.0× XP multiplier, which is anchored to the 3×-health chassis Encounter-Design is deleting.

Worse: Art §2.1 budgets elites at **zero mesh authoring** — "a scale multiplier plus a material parameter and one extra emissive mask" — and its acceptance criteria assume that. Encounter §1.2 then specifies ten modifiers whose graybox tells include a translucent shield capsule, an inflating strobing body, a trail ribbon, a ground decal ring, a projected cone plus ground polygon, visible body seams, drawn tether lines to allies, a faceted shell, a desaturating blink streak, and persistent hazard polygons. Several of those are real VFX or geometry work. Art OQ8 half-anticipates this ("if any elite modifier needs bespoke geometry, that assumption and the Phase C budget both break") but was written without the modifier list.

**Recommendation:** adopt Encounter's chassis (it is the one derived from Master §11.2), update Game-Modes' Marked-pack baseline and XP's Veteran anchor to match, and revise Art's Phase C budget: elites cost **one shared modifier-VFX library** — roughly six reusable effects covering capsule / decal-ring / tether / trail / polygon / streak — rather than either zero cost or per-modifier meshes. Budget it explicitly rather than discovering it.

### 5.4 Three incompatible enemy vocabularies

- `XP-And-Pacing.md` §5.1: seven **tiers** — Trash, Standard, Veteran, Elite, Champion, Rift boss, Act boss — each with an XP multiplier from 0.35× to 400×.
- `Encounter-Design.md` §2: three **archetypes** — Skitter, Lattice, Severed Warden — plus an elite flag carrying 1–3 modifiers. No Veteran or Champion exists.
- `Game-Modes.md` §4.3: four **pack rarities** — Common, Marked, Severed, Frontier — with a "named elite" concept.

These are three different axes (reward tier, behavioural role, pack composition) using overlapping words. "Elite" means something different in all three. XP's own OQ6 flags it: "someone owns enemy taxonomy and it is not this document." Nobody owns it.

**Recommendation:** they are genuinely orthogonal and should be formalised as three separate fields on the enemy, not reconciled into one word:
- `Archetype` (behaviour + stat block) — Encounter-Design owns
- `Rank` (reward tier + XP multiplier + loot floor) — XP owns, renamed away from "Elite" to avoid the collision (Standard / Veteran / Champion / Boss)
- `Modifiers` (0–3 elite modifiers) — Encounter-Design owns, and *drives* Rank rather than being a Rank

Then pack rarity in Game-Modes is a *composition template* over those three, which is what it actually is. This costs one design pass and removes a class of permanent confusion.

### 5.5 Two incompatible item-level formulas

`XP-And-Pacing.md` §8 locks `ItemLevel = clamp(ZoneLevel + TierBonus + Variance, 1, 50)`, TierBonus 0…+5 by enemy tier, and states plainly: "no code path reads `EnemyLevel` into `ItemLevel` directly."

`Game-Modes.md` §3.7 independently defines rift drops as `enemy level + tier_bonus`, where `tier_bonus = 0 below T20, +1 per 5 tiers above`, and enemy level itself derives from rift tier via `min(50, 3 + tier × 1.6)`.

Different anchor (zone vs rift-tier-derived enemy level), different bonus scale, different intent. Game-Modes §8 also lists item level as a *satisfied* prerequisite ("exists now per CONTEXT.md") while XP §8 labels the same thing a CONFLICT requiring a change.

**Recommendation:** XP's formula wins — it is the one that answers the master sheet's open item and it keeps rarity and item level doing different jobs. Game-Modes' rift-tier curve becomes the source of `ZoneLevel` for an instanced rift (`ZoneLevel = min(50, round(3 + tier × 1.6))`), and the tier bonus comes from XP's enemy-tier table. The two then compose instead of competing.

### 5.6 Game-Modes' rift tier cap and enemy level formula let a levelling player outclass themselves by twelve levels

Game-Modes §3.7 states "the rift tier available to a player is capped at their level, so tiering is invisible during levelling," while enemy level is `min(50, 3 + tier × 1.6)`. A level-15 player may therefore enter a T15 rift containing level-27 enemies — a +12 delta. XP §5.2's falloff caps over-level XP at +1.25×, so it is not an XP exploit, but it is a difficulty cliff hidden inside two innocuous rules.

**Recommendation:** cap available tier at `player level / 1.6` during the campaign, or clamp campaign rift enemy level to `player level + 3`. Either is one line; discovering it in playtest is a week.

### 5.7 Two documents each add a step to the damage resolution order, and nobody composed the result

Master §6.1 has seven steps. Two proposals add an eighth and a ninth:

- `Encounter-Design.md` §7: **facing-dependent armour**, "applied before the armour step."
- `Core-Constellations.md` OQ4: **elemental resistance**, proposed between armour mitigation (4) and shield routing (5).

Independently, three systems now write into the armour step with no defined interaction: Core-Constellations P9 (Marksman's Ledger — bypasses 50 % of armour, explicitly "bypass, not reduction, so it does not stack into negative armour"), Class-Kits VW7 (Rot — flat 40 + 40 armour reduction, flat "to protect the boss cap"), and Encounter §2.3 / §3.2 (facing armour — full 90 frontal, treated as 0 from behind). Game-Modes §4.5 then adds "boss armour reduction caps at 60 % of applied value."

Nobody has written the composed order, and armour-shred stacking into negative armour is the classic way this goes wrong.

**Recommendation:** before either new step lands, author a single **damage pipeline spec** that states the full ordered list, where each new step inserts, and the armour composition rule (recommended: facing selects the base armour value → flat reductions apply → bypass percentages apply → the boss cap clamps the *total* reduction → mitigation formula runs, with an explicit floor at 0). This is a half-day of writing that prevents a month of bugs, and it is the natural home for the proc-coefficient law in E1.

### 5.8 "Exposed" deletes a Tank's resource loop, not just its mitigation

`Game-Modes.md` §4.4 defines the Class C modifier **Exposed** — "Dodge and Block do not function" — at the highest reward multiplier of any player modifier (+22 %). Its own FORBIDDEN list was carefully checked against verb ownership ("Grounded disables air jump for everyone including players who never bought it — that is acceptable and intentional").

But `Class-Kits.md` §4 makes "passive Block roll firing" a **Grit generation source** (+6, 0.4 s ICD), and `Class-Kits.md` §1.1 makes "successful dodge" a **Momentum generation source** (+15 flat, raised to +35 by Kinetic K5). Under Exposed, a Tank and a Swift do not merely lose mitigation — they lose a resource loop, and with it their ability to cast anything. No other class is affected at all.

The rule that was applied to verbs was never applied to class resources.

**Recommendation:** extend Game-Modes' FORBIDDEN list with a fifth line — **no modifier may disable a class resource generation source** — and either re-scope Exposed to "Block and Dodge do not reduce or evade damage, but still generate class resource," or cut it. Then audit the remaining Class C modifiers against every class's generation table (Dry vs Gunsmith reload/ammo economy is the next one to check).

### 5.9 The Core Tree authors More multipliers with no budget, and nobody has computed the composed ceiling

`Class-Kits.md` §0.1 introduces a More-multiplier budget — three per class, one per branch keystone, one holdable per character, maximum 1.30× — and §6.1 concludes: *"the theoretical ceiling from non-crit multipliers is two conditional multipliers"* (one class, one Anomalous).

That count omits the Core Tree entirely. `Core-Constellations.md` authors Mores freely and with no budget: Precision **Fixate** (up to 1.19× at 6 stacks), Affliction **Necrosis** (1.15×), Kinesis **Reflex** (a More on dodge chance). Master §2.6 and `Item-Foundation.md` reserved More multipliers "for trees/Anomalous" — which handed the tree layer a blank cheque that nobody noticed cashing.

A character holding one class keystone, two Core keystones, and an Anomalous can plausibly carry: `1.30 (class) × 1.19 (Fixate) × 1.15 (Necrosis) × Anomalous(?) ≈ 1.78× before the Anomalous term`, all on top of the additive Increased bucket and a crit multiplier that Master §6.3 already designates the only multiplier of its kind. Master §7.10 risk 1 is the first-listed balance risk in the project and this is it, arriving through the layer that was granted the exemption.

**Recommendation:** apply Class-Kits' discipline to the Core Tree — **one More multiplier per constellation, authored only on the Convergence or the Keystone** — and then write the composed ceiling down as a number in the damage pipeline spec, with an automated test that asserts it. Fixate and Necrosis are both defensible; a third and fourth in the same tree are not.

### 5.10 Alt friction is being removed from three directions at once

Not a contradiction, but a compound effect no single document could see. `XP-And-Pacing.md` §6 gives alts Veteran's Path (40 h → 26 h). `Save-Architecture.md` §2.1 gives them an account-wide stash and account-wide materials. §2.1 also makes fragment capability unlocks account-wide. Save OQ1 raises the concern in isolation — "it makes alts instantly geared, which softens the gear chase" — without knowing about Veteran's Path.

Together, a second Breaker arrives at level 50 in 26 hours wearing the first one's endgame gear, having skipped the fragment hunt. Given that gear is the *entire* endgame and class selection is permanent, that is either exactly right (characters are builds; gear is an account asset) or it hollows out the second playthrough completely.

**Recommendation:** it is exactly right, and should be stated as an explicit product position rather than emerging as an accident of three documents: **characters are builds, gear is an account asset, and the campaign is a tutorial you pay for once.** But say so on purpose, and reconsider only if the loot chase proves too short (Master §9.3 already holds horizontal unlocks in reserve for that case).

---

## 6. Build-order recommendation

The governing observation: **almost every number in all seven documents is downstream of a time-to-kill that does not exist, and the only instrument that can produce it is wave-mode instrumentation in the gym.** The second observation: three things in this corpus are cheap this week and expensive forever after — item-level sourcing, save file safety, and enum discipline.

The order below follows both, and preserves the project rule that every milestone stays playable.

### Tier 0 — decisions, not code (days, not weeks)

**0a. Propagate the block/dodge ruling and settle the stamina pool.** Zero engineering. It unblocks 22 Bulwark/Kinesis nodes, Swift's and Tank's resource loops, every enemy telegraph window, the HUD's stamina element, all defensive animation, and three master-sheet open items. Six of seven documents name it their #1 blocker. Nothing below Tier 1 should start first.

**0b. Rename the Anomaly content type.** One word, before any content string exists.

**0c. Write the damage pipeline spec** (§5.7) — full ordered steps, insertion points for resistance and facing armour, armour composition and floor rule, the proc-coefficient law from E1, and the composed More ceiling from §5.9. Half a day. It is the single document that four other documents are currently guessing at.

### Tier 1 — the measurement instrument

**1. Wave mode + instrumentation** (`Encounter-Design.md` §4). Twelve-wave cycle, spawn budget solver, rest waves, per-wave metrics including the time-airborne-or-sliding fraction. *Why first:* it is the only thing that converts placeholders into values, and every subsequent tuning task is guesswork without it. It also reuses the arena that the boss fight will need, so no work is thrown away.

**2. Enemy archetypes as three Data Assets over one `ABreakerEnemy`** (`Encounter-Design.md` §2), plus **facing-dependent armour** (§7). *Why here:* wave mode needs something worth measuring, and Skitter/Lattice/Warden are the three enemies that make movement a damage stat. Facing armour is one dot product and it is what the Warden and the boss are both built on.

**3. Elite modifier system replacing `ConfigureElite`** (`Encounter-Design.md` §1). Data-driven modifiers, forbidden-pair validation with the 10,000-roll automated test, weight table. *Why here:* it is the difficulty axis wave mode is measuring, and Game-Modes' Severed/Frontier packs are blocked on it.

### Tier 2 — the cheap-now, expensive-later retrofits

**4. Zone level and hybrid item level** (`XP-And-Pacing.md` §8, C3). Add `ZoneLevel` to the loot library with an enemy-level fallback; update `Item-Foundation.md`. *Why here:* the XP document is correct that the fallback will silently become shipping behaviour by neglect, and Master §4.8 already names this as the retrofit to avoid.

**5. Save subsystem extraction, header, hashing, atomic write, rotating backups** (`Save-Architecture.md` Steps 1–2), plus enum-discipline enforcement (E21). *Why here:* there is exactly one save slot, no backup, no validation, and a permanent class decision. Every hour of content built before this is an hour of content that a mid-write crash deletes. Steps 3–8 can wait; Steps 1–2 cannot.

### Tier 3 — the progression vertical slice

**6. XP, levelling, and the compressed slice curve** (`XP-And-Pacing.md` §9). There is currently *no XP system at all* — the progression framework exists but nothing grants a point. Cap, curve, and per-level grants from a Data Asset; XP bar removed at cap.

**7. The fifteen slice Core nodes, including both verb grants** (`Core-Constellations.md` §10). Proves Gateway, ranked Minor, rule-rewriting Notable, inert-until-prerequisite, and verb grant/revoke across a save round trip. The More-bucket assertion (criterion 7) and the proc-coefficient-0 assertion (criterion 8) are the two tests that protect the whole balance thesis.

**8. Swift, Kinetic and Marksman, Tiers 1–3** (`Class-Kits.md` §7). Sixteen nodes, ten spendable points, one `DA_MomentumPolicy` asset carrying every generation rate and cap. *Why Swift:* Master §7.5's prototyping order, and Kinetic is the branch that stress-tests the movement boundary the whole game is built on.

### Tier 4 — making gear legible

**9. UI phases 1–3** (`UI-UX-Spec.md` §11): armour percentage readout, item tooltip with prefix/suffix split and tier badges, automatic comparison with the NET EFFECT block. *Why here:* gear is the entire endgame and the player currently has no way to evaluate a drop. This is the highest-value UI work in the project and it is unblocked the moment 0a lands.

### Tier 5 — content

**10. The Field Marshal and its arena** (`Encounter-Design.md` §3), reusing the wave-mode arena.
**11. The Local Rift skeleton — threshold / body / anchor / closing ritual — with objectives 1 and 2 only** (`Game-Modes.md` §8). The closing ritual specifically: it is cheap, it is the highest narrative leverage in the corpus, and it must exist before there are four hundred rifts to retrofit.

### Tier 6 — art

**12. Art Phase A** (`Art-And-Modelling-Plan.md` §7.4) — FP arms, gloves, the Rifle, the four movement poses, and the dodge/block *feedback* with no animation. Not before Tier 5. The plan's own blockout → playtest → author gate is the reason, and it is correct.

**Deliberately deferred:** Elements (blocked), Anomalies/Frontier mode, dungeons, raids, Conquest, multi-character save slots, the stash, the Forge screen, the constellation map UI, all Altered assets, and the Anchor. None of them are on the critical path to a measured TTK, and every one of them is cheaper after it exists.

---

## 7. OPEN QUESTIONS — the owner-decision list, deduplicated and ranked

Ranked by how much downstream work each blocks. Every item carries a recommended default, so a decision can be a single word.

---

**O1. Block and dodge: passive chance layers, or stamina-spending player inputs? And if passive, does the shared stamina pool survive?**
*Raised by:* all seven documents as their #1 or #2 open question.
*Blocks:* Bulwark and Kinesis in full (22 nodes, both keystones, the only tree-granted defensive verb); Swift's dodge→Momentum loop and Tank's Block→Grit loop; every enemy telegraph window in Encounter-Design §2–3; the HUD stamina element and UI phase 1; all defensive animation and Art Phase A; Master §3.9's stamina-affix block, §3.15's "does Block need a shield", §7.7's pool, §7.11's input-slot question.
*Finding:* **this is already ruled.** `CONTEXT.md` line 47 records the owner's decision — passive, explicitly superseding the stance/window model — and the shipped `UBreakerCombatComponent` implements it. All seven authors flagged a conflict against stale text in Master §5.2/§3.8/§7.7, `Layer-Ownership.md`, `Character-Progression-Architecture.md`, and `Item-Foundation.md`.
*Decision actually needed:* ratify, propagate to those six files, and rule on the residual — **does the 100-point shared stamina pool still exist?** Recommend **no**: delete it, and give Parry a short cooldown of its own rather than a stamina cost. That closes four master-sheet open items in one stroke and removes a resource the player can no longer spend.
*Recommended:* Passive, ratified. Stamina pool deleted. Parry is the only defensive input in the game.

---

**O2. Approve measurement-before-authoring: build wave-mode instrumentation next, and freeze value authoring until it reports.**
*Raised by:* Master §3.0 standing warning; XP OQ1; Encounter OQ3; Game-Modes §7 note; every "placeholder" disclaimer in the corpus.
*Blocks:* every number in all seven documents — affix values, enemy health, XP hours, drop rates, encounter timings, reward multipliers, TTK-derived acceptance criteria.
*Why it is a decision and not a task:* the temptation is to author affix content and class nodes now because they are designed. The recommendation is to build the instrument first and accept a visible gap where content would be.
*Recommended:* Yes. Wave mode is Tier 1, item 1.

---

**O3. Stat aggregation: how do multiple More multipliers compose, and what is the Core Tree's More budget?**
*Raised by:* Class-Kits OQ2 ("blocks every keystone"); Master §3.15, §6.6; §5.9 of this document.
*Blocks:* all fifteen class branch keystones; every Core Notable and Keystone that authors a More; the Anomalous item design space; the composed damage ceiling that Master §7.10 risk 1 exists to prevent.
*Partly answered already:* `Item-Foundation.md` locks flat → one additive Increased bucket → More reserved for trees/Anomalous, and `CONTEXT.md` confirms it. What is *not* answered is (a) ordering, and (b) how many Mores the tree layer may author.
*Recommended:* (a) All More multipliers multiply as an unordered product, so ordering is a non-question by construction. (b) **One More per constellation, authored only on Convergence or Keystone**, mirroring Class-Kits' one-per-branch rule. (c) Write the composed ceiling into the damage pipeline spec with an automated assertion.

---

**O4. What is the intended time from level 50 to a finished build?**
*Raised by:* Master §9.4; Game-Modes OQ2 ("the single largest blocker on this document"); Master §4.9.
*Blocks:* every rarity weight table (Game-Modes §3.7), the entire Anomaly reward structure and token economy (§4.6), T-1 acquisition gating, crafting material costs and the whole crafting economy, Dungeon/Raid lockout shapes, and the decision on whether the horizontal post-cap unlocks held in reserve (Master §9.3) ever get built.
*Why it is hard:* the game has no parallel progression track absorbing pressure if the answer is wrong. Master §9.1 calls this out as a deliberate bet.
*Recommended:* a number, even a wrong one — **300–400 hours to a build the player considers finished, with the last 20 % of that time spent on T-1 and the Anomalous slot.** Any figure lets the drop tables be derived; no figure leaves eight tables unanchorable.

---

**O5. Does an elemental resistance model exist, is it one stat or per-element, and where does it sit in the damage order?**
*Raised by:* Master §6.1, §6.7, §3.2, §3.7; Core-Constellations OQ3 and OQ4 plus the whole of §6; Class-Kits (Multispell, Void Whisperer); UI §4.5.
*Blocks:* the entire Elements constellation (11 nodes); the Ignite/Chill/Shock affix lines and Elemental Damage Reduction; Multispell's full form and Void Whisperer's elemental upgrades; three UI ailment glyphs; elemental elite modifiers (deliberately excluded from Encounter's list for this reason).
*Note:* one sixth of the Core Tree currently cannot be authored, and the UI is instructed to hide the constellation entirely in shipping builds. That is a shippable state — but it means the game ships with five constellations, and that should be a decision rather than an omission.
*Recommended:* **per-element resistance**, inserted after armour mitigation and before shield routing. Per-element is what makes Penetrance, Insulator's Bane, and RESONANCE's mono-element restriction coherent; a single stat makes all three nearly inert. And decide explicitly whether the slice and first release ship five constellations or six.

---

**O6. Ratify hybrid item level, and name the owner of `ZoneLevel`.**
*Raised by:* Master §4.9, §6.5; XP §8 (CONFLICT) and OQ3; §5.5 of this document.
*Blocks:* loot pipeline correctness, rift tiering, Anomaly tiering, and the ilvl→affix-tier mapping that the whole tier-gating scheme assumes.
*Also resolves:* the collision between XP §8's formula and Game-Modes §3.7's (§5.5).
*Recommended:* Yes to hybrid. `ZoneLevel` lives on the **rift/zone definition Data Asset**, with an enemy-level fallback so the gym keeps working. Game-Modes' tier curve becomes the source of `ZoneLevel` for instanced rifts. Ship the change with the `Item-Foundation.md` update in the same commit so the fallback cannot become permanent by neglect.

---

**O7. Which of the two ~15 world Core Point lists is canon?**
*Raised by:* Master §7.11; XP §7 and OQ5; Game-Modes §2; §5.1 of this document.
*Blocks:* campaign content planning, the 65-point budget validation that Master §7.4 requires, act pacing, and whether the third constellation is a build decision or a post-campaign feature.
*Recommended:* **XP §7's list**, with Game-Modes' rift-archetype first-clears substituted for XP's cumulative rift counters. Both authors' intent survives; the count stays at 15; nothing is endgame-gated.

---

**O8. Rename the "Anomaly" content type.**
*Raised by:* Game-Modes OQ1 ("needs a decision, not a discussion"); Master §1.2 naming constraints.
*Blocks:* all endgame content authoring, every UI string, every future localization key, and community vocabulary from the first public build onward.
*Cost of delay:* trivial today, a rename across content and loc tomorrow.
*Recommended:* **Frontier.** Master §8.3 and §10.1 already use "Rior's frontier" for this exact content.

---

**O9. Who owns enemy taxonomy, and are Rank / Archetype / Modifiers three fields or one word?**
*Raised by:* XP OQ6 (explicitly: "someone owns enemy taxonomy and it is not this document"); Master §10.3; §5.4 of this document.
*Blocks:* XP's kill-value tables, elite loot floors, Encounter's wave budget solver, Game-Modes' pack rarity system, and the elite chassis reconciliation in §5.3.
*Recommended:* three orthogonal fields — `Archetype` (behaviour, Encounter-Design owns), `Rank` (reward tier and XP multiplier, XP owns, renamed to Standard/Veteran/Champion/Boss), `Modifiers` (0–3, Encounter-Design owns and which *drives* Rank). Pack rarity becomes a composition template over the three.

---

**O10. Tick Frequency: hard cap or discrete intervals, and is tick interval part of the DoT snapshot?**
*Raised by:* Master §3.7 ("the most dangerous affix in the game"), §3.15, §6.4; Class-Kits OQ9 and C4; Core-Constellations A3.
*Blocks:* the Tick Frequency affix line; Affliction's Slow Bleed (designed to *fight* Tick Frequency rather than stack with it); Caster VW11 in full; boss DoT caps; and Affliction's TERMINAL keystone, which explicitly does not benefit from Tick Frequency.
*Recommended:* **snapshot the interval** (consistent with the existing DoT contract) and use **discrete tick intervals** rather than a percentage cap, so stacking has visibly diminishing steps rather than an invisible ceiling.

---

**O11. Is the Aberrant 3-equipped limit global, or chosen per slot?**
*Raised by:* Master §4.9; UI OQ3 ("blocking for the equip-limit UI"); Save §6 validation table.
*Blocks:* the equip-limit counter and swap picker (UI §5.2), save-load validation, and the paper-doll's at-limit tell (Art §6.4).
*Recommended:* **global.** It is simpler to communicate, and the constraint that carries the endgame should be one number the player can hold in their head.

---

**O12. Are crafting materials a separate currency, or item-derived?**
*Raised by:* Master §4.9; UI OQ4; Save OQ7.
*Blocks:* the salvage modal's yield line, the Forge screen's header, the stash transaction cost (item-derived materials inherit the two-phase transfer; a scalar currency does not), and the whole unowned economy domain in §1.2.
*Recommended:* **scalar currencies**, three or four of them, tiered. Item-derived materials multiply the stash transaction surface for no gameplay gain, and Save §4.5's two-phase protocol is the most fragile code in the save design.

---

**O13. Is the Rocket's self-damage and self-knockback rule settled, and does rocket-jumping become a movement verb?**
*Raised by:* Master §12.5; Art OQ2 ("blocking for the Rocket"); Encounter OQ10; Class-Kits §4 (settles the Tank case only).
*Blocks:* Rocket art in full (launch pose, camera treatment, recovery animation); whether Volatile and Cascading elite modifiers are fair to a Rocket player; and — importantly — whether Master §5.4's movement guardrails need a clause they currently do not have.
*Recommended:* adopt Class-Kits' Tank policy as the general rule — **strong self-damage reduction, full self-knockback control, never immunity** — and explicitly rule that rocket-jumping is a *tolerated* traversal consequence, not a designed verb, so no encounter or level may assume it.

---

**O14. Is the player character a cipher or a person, and are Effigies a player option?**
*Raised by:* Master §1.8; Art OQ3 ("the largest single swing in total character-art cost in the project — potentially doubling it").
*Blocks:* whether the third-person body needs a face, whether character customisation exists at all, whether the Effigy arms set is ever built, the class-select and death screens' framing (UI OQ7), and the Survivor NPC's and final choice's emotional weight (Master §1.8 notes a responsive character serves both substantially better).
*Recommended:* **a person, lightly** — a character with a body and a face but minimal voice. It costs far less than full VO, it serves the Survivor and the final choice, and it lets the Effigy option be deferred as a later cosmetic rather than a doubling of the character pipeline.

---

**O15. Are branch nodes freely mixed with investment gates, or mutually exclusive at major tiers?**
*Raised by:* Master §7.11; Class-Kits §0.2 (assumed freely mixed); Core-Constellations OQ6; UI OQ2 ("blocking for progression UI").
*Blocks:* class tree topology, the Core Tree's Link-node design, and the shape of both tree screens (mutual exclusivity requires a lockout confirmation modal and a visibly different layout).
*Recommended:* **freely mixed with investment gates**, as both tree documents already assume. Permanent class selection is already the game's "you cannot have everything" mechanism; a second irreversible lock inside the tree is punitive and works against free respec.

---

**O16. Is there hardcore / permadeath?**
*Raised by:* Save OQ3 ("decide before Step 2, not after").
*Blocks:* the entire backup story — rotating `.bak` files are a cheat vector if a character can die permanently — and it must be answered before the atomic-write work in Tier 2, not after.
*Recommended:* **no.** Nothing in the master sheet asks for it, and it is directly at odds with a permanent-class game that already asks the player to commit.

---

**O17. Is the account-wide stash accepted, and is the "characters are builds, gear is an account asset" position deliberate?**
*Raised by:* Save OQ1; §5.10 of this document.
*Blocks:* Save Step 5, and — compounded with Veteran's Path and account-wide fragments — the whole texture of a second playthrough.
*Recommended:* **yes, and state it as a product position** rather than letting it emerge from three documents independently. Revisit only if the loot chase proves too short, which is exactly the trigger Master §9.3 already defines for the reserved horizontal unlocks.

---

**O18. Does the slice ship three finished weapons or five?**
*Raised by:* Art OQ4; Master §12.3 vs `CONTEXT.md` (five archetypes shipped).
*Blocks:* Art Phase D scope and roughly two weeks of authoring.
*Recommended:* **three authored, two kitbashed.**

---

**O19. Are the three slice build-defining legendaries Anomalous or Aberrant?**
*Raised by:* Art OQ9; Master §12.5 ("are unique weapons and Anomalous items the same system?").
*Blocks:* roughly a week of art (bespoke mesh vs attachment plus emissive mark), and the loot pipeline's unbuilt step 6.
*Recommended:* **one Anomalous and two Aberrant.** It exercises both signature paths, respects the max-1-equipped rule so the player actually feels the constraint, and costs one bespoke mesh instead of three.

---

**O20. Loot distribution in parties: instanced, shared, or need/greed?**
*Raised by:* Master §11.3; Save §7 (assumes instanced); Encounter OQ7; Game-Modes OQ8.
*Blocks:* every party mode's reward section, the save transaction model's complexity, and whether elites should drop more or better at higher party sizes.
*Recommended:* **instanced per player.** It is the only option that keeps the two-phase item transit simple, and it removes social friction that a solo-primary game gains nothing from.

---

**O21. Are Anomaly/Frontier tiers infinite or capped at 30?**
*Raised by:* Game-Modes OQ5; Master §9.4.
*Blocks:* endgame content shape and, arguably, the spirit of the level-50 hard stop — infinite tiers are a soft paragon track even when they grant no power.
*Recommended:* **cap at 30**, then let modifier count and pack density be the ceiling.

---

**O22. Does the elite loot floor scale with modifier count?**
*Raised by:* Encounter OQ8.
*Blocks:* elite reward tuning; a 3-modifier elite is meaningfully harder than a 1-modifier one and currently pays identically.
*Recommended:* **yes** — floor rises one rarity step at 3 modifiers only, so the common case stays simple.

---

**O23. Does a stagger/flinch system exist?**
*Raised by:* Encounter OQ9 (elites are assigned 2× stagger resistance, presupposing a model that does not exist); Bulwark B6 ("cannot be staggered while your shield is intact"); Master §3.8's Stagger/Knockback Resistance affix; Encounter's Relentless modifier.
*Blocks:* one affix line, one Bulwark node, one elite modifier, and the boss's stagger immunity.
*Recommended:* **yes, minimal** — a binary interrupt state with a resistance stat and a per-enemy immunity flag. Four systems already assume it.

---

**O24. Does XP exist at all in endgame content?**
*Raised by:* XP OQ7; Master §7.1.
*Blocks:* whether the kill-value tables mean anything at cap, and whether a clean crafting-currency sink exists.
*Recommended:* **no XP at cap, no conversion.** A conversion is a post-cap progression track wearing a currency costume, and Master §7.1 is unambiguous. Use a separate, explicit currency drop instead.

---

**O25. Should Core Point respec carry friction at the keystone tier?**
*Raised by:* Master §7.8; Core-Constellations OQ11.
*Blocks:* nothing structural; it is a playtest question.
*Recommended:* **free, as locked**, and revisit only if keystone-swapping per encounter becomes a real behaviour.

---

### Questions this synthesis raises that no document asked

**S1. Does the game ship with five constellations or six?** Elements is blocked, the UI is instructed to hide it in shipping builds, and no document treats "ship without it" as a scenario. If the resistance model is not on the near roadmap, five constellations against a ~65-point budget changes the two-full-plus-one-partial validation.

**S2. Who owns audio?** Nine telegraphs, one closing ritual, and the entire dodge/block feedback model depend on an audio channel with no owner, no palette, and no document.

**S3. What is the replication topology?** The Anchor is non-instanced and shared, parties go to five, Conquest matchmakes nine. `Save-Architecture.md` §7 covers storage authority only. Nothing covers the network.

**S4. Who enumerates the rule-rewrite hooks?** `Class-Kits.md` §7 correctly identifies that rule-rewriting nodes need code-side hooks and asks for them to be enumerated before authoring. Across the class and core trees there are roughly forty distinct hook types. That enumeration is the real engineering specification for the progression system, and it is the largest unwritten technical document in the project.
