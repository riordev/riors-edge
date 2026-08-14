# Encounter Design — Vertical Slice

Owner: encounter systems. Status: design, not implemented.
Scope: elite modifiers, three normal enemy archetypes, the slice boss, the gym wave/arena mode, spawn pacing, and 1-5 player scaling.

This document sits under the master sheet (`Docs/Design/Master-Sheet-Import.txt`). Where it goes beyond the sheet it is labelled **EXTENDS**; where it disagrees with a written line it is labelled **CONFLICT** and states the resolution I recommend.

---

## 0. Constraints this document is built inside

Locked, not negotiable here:

- Level cap 50, hard stop. Encounter difficulty must never be solvable by "come back two levels later" past 50 — only gear and play.
- Gear is the entire endgame. Encounters exist to make gear differences legible.
- Solo is the primary balance target; parties up to five are supported, not assumed.
- Crit is the only multiplier of its kind. No enemy, elite modifier, or boss mechanic may introduce a second independent damage multiplier for the *player*. Enemy-side multipliers are fine — they are not in the player's stacking economy.
- Flat sums -> one additive Increased bucket -> More reserved for trees/Anomalous. Elite modifiers are enemy-side and use a separate enemy budget; they never write into player buckets.
- No grapple. Encounter geometry must never assume a tether.
- Affixes scale verbs, never grant them. Encounters must therefore be beatable with the base kit alone (walk/sprint/jump/crouch/dash/slide/wall ride/wall jump), and air jump/parry must never be the only answer to a mechanic.
- Naming: no "Bastion", no "Aberrant", no "Anomalous" as enemy or modifier names. Elite modifier names below deliberately avoid all three.
- Master sheet 8.2: **no Altered enemy may appear anywhere before the Act II turn beat.** The slice contains that beat, so the slice's Altered content is gated behind it.

### RESOLVED (O1) — dodge and block are passive layers

**Ruling O1** (`Docs/Design/Decisions.md`): *the Stamina pool is removed entirely; block and dodge are passive chance layers (ratified). Parry, when built, uses its own short cooldown.*

The former CONFLICT is closed in favour of **passive**. Consequences this document is now bound by:

- **Movement is the player's only active defensive input** — dash, slide, wall ride, jump, and cover. Block and dodge resolve as rolls inside the damage pipeline and are never a reaction the player performs.
- **Telegraphs must be spatial and temporal, never reaction-window frames.** A passive-defence game telegraphs where and when, because the player answers with position rather than a button. Every telegraph and counterplay line below is written against that.
- **No mechanic may reference stamina.** The pool no longer exists.
- Parry remains a tree verb on its own cooldown, and per §0 no encounter may require it.

Telegraph windows below are tuned for passive defence and are not to be shortened.

---

## 1. Elite modifiers

### 1.0 Enemy taxonomy — three orthogonal fields (O9)

**Ruling O9** fixes the enemy taxonomy at three fields. There is no fourth field, and "elite" is not one of them:

| Field | Values | Owner |
|---|---|---|
| **Archetype** | Behavior identity — Skitter, Lattice, Severed Warden, … (§2) | **This document** |
| **Rank** | Standard / Veteran / Champion / Boss | `Docs/Design/XP-And-Pacing.md` §5.1 |
| **Modifiers** | 0-3 per enemy, drawn from the list in §1.2 | **This document** |

**Modifier count drives Rank** (adopted from XP §5.1, not authored here): 0 modifiers = Standard, 1 = Veteran, 2-3 = Champion. Boss is authored, not modifier-derived.

**Ownership statement.** Archetype and Modifiers are owned here — the archetype roster in §2, the modifier list in §1.2, the combination rules in §1.3, and the selection weights in §1.4 are this document's to author. **Rank is owned by XP-And-Pacing** because it is the XP and reward axis; this document consumes it and must not author Rank bands or payouts.

**Vocabulary.** "Elite" survives in this document only as shorthand for *any enemy at Rank Veteran or above*. Where a number or rule below says "elite," read "Veteran or Champion." The three fields are the normative names in data and UI.

### 1.1 The elite (Veteran+) stat chassis — CANONICAL

An enemy at Rank Veteran or above is a normal-archetype enemy carrying 1-3 modifiers, at elevated stats, that drops at a raised rarity floor. The modifier is the interesting part; the stat bump is the smallest part.

The existing `ConfigureElite()` (1.5x scale, 3x health, 2x damage, drops never below Exceptional) is the legacy **stat chassis**. Keep the loot floor. **EXTENDS:** reduce the scale and health multipliers and let modifiers carry the difficulty.

**This table is the canonical elite stat chassis for the project.** `Docs/Design/Game-Modes.md` §4.3 and `Docs/Design/XP-And-Pacing.md` §5.1 both reference it rather than restating their own; the legacy 1.5x/3x/2x values are superseded everywhere. Changes to these numbers are made here and propagate outward.

| Property | Current code | Proposed |
|---|---|---|
| Visual scale | 1.5x | 1.25x (1.5x reads as a miniboss and confuses the boss silhouette) |
| Health | 3.0x | 2.0x base, +0.35x per modifier beyond the first |
| Damage | 2.0x | 1.5x |
| Stagger resistance | — | 2.0x (elites should not be perma-flinched) |
| Loot floor | Exceptional | Exceptional (unchanged) |
| Modifier count | — | 1 (common), 2 (uncommon), 3 (rare) — see §1.4 |
| Resulting Rank | — | 1 modifier = Veteran; 2-3 = Champion (per O9 / XP §5.1) |

Rationale for cutting health: a 3x-health 2x-damage elite with no modifier is a sponge, which is the exact failure mode the master sheet names in 11.2. Difficulty should come from *what it does*.

#### What the chassis solves backwards from — TTK/TTD SEED TARGETS [O18]

**Ruling O18** supplies the designer inputs this chassis is solved backwards from. Recorded verbatim as an owner ruling; no value below is authored here and none is changed (O2):

| Target | Seed value [O18] |
|---|---|
| Trash mobs | a little under 1 second, scaling exponentially with enemy difficulty |
| Rare / elite enemies | ~3 seconds |
| Boss encounters | 20–45 seconds, unless a special enemy |
| TTD, no resources/sustain | 4–5 seconds |
| TTD, resources and sustain invested | substantially higher |

**The stat chassis solves backwards from these.** The health, damage, and stagger multipliers in the table above are the free variables; the targets are the constraint. Wave mode (§4) reports **divergence from these targets**, not truth — a divergence means either the chassis or the target is wrong, and which one is an owner call.

**The 2.0x health figure is the canonical anchor.** Note the internal consistency check this enables: a Veteran at 2.0x the health of a Standard, against a Standard trash target a little under 1s, lands near the ~3s rare/elite target only once the player's own damage ramp and the modifier's difficulty contribution are counted — the modifiers are expected to carry that gap, exactly as the rationale above asserts. The retired 3.0x chassis overshot it.

**Boss length — the 20–45s band replaces any prior boss-length assumption.** Any figure anywhere in Docs/ that assumes a longer default boss fight is superseded as the *default*; the O18 escape hatch is the "unless a special enemy" clause, which must be claimed explicitly for a given boss rather than assumed.

### 1.2 The modifier list — 10 modifiers

Each modifier must satisfy three tests:

1. **Readable in graybox.** It has a silhouette, colour, or VFX tell that survives with zero art.
2. **Answerable by movement.** A player with only the base kit has a positional answer.
3. **Not a stat.** "+30% damage" is not a modifier; it is the stat chassis.

| # | Name | Effect | Graybox tell | Movement counterplay | Weight class |
|---|---|---|---|---|---|
| 1 | **Warded** | Carries a regenerating shield equal to 60% of max health. Shield recharges after 4s without damage. Shield ignores DoT (physical shield-bypass DoTs still route to health per 6.2). | Bright translucent capsule around the body, visibly shrinking | Burst it down inside the 4s window, or bring bleed/poison which bypasses it entirely | Common |
| 2 | **Volatile** | On death, detonates after a 1.2s fuse for 45% of player max health inside 500cm, linear falloff to 0 at 900cm. | Body inflates and strobes; audible rising tone | Kill it at range, or dash/slide out — 1.2s at sprint clears the radius comfortably | Common |
| 3 | **Fleetfoot** | +55% move speed, gains the ability to strafe-circle at attack range instead of standing. | Trail ribbon, faster limb cadence | Fight it in a corridor; it cannot circle where there is no room. Rewards route choice, not aim | Common |
| 4 | **Anchored** | Immune to knockback and stagger. Attacks apply a 40% slow for 2s on hit. | Squat, wider stance; ground decal ring under it | Do not get hit. The slow is the punish, not the damage — it converts a movement player into a stationary one | Common |
| 5 | **Suppressing** | Fires a persistent cone that applies a 3s zone denial field where it lands; standing in the field applies -25% air control. **NEEDS-RECOST [O1/O2]:** the original effect also drained stamina; O1 removed the stamina pool entirely, so half this modifier's pressure has no referent. Air-control reduction alone may not carry an Uncommon weight class. A replacement second effect is a new authoring decision and is frozen by O2 until instrumentation reports. | Visible cone projection and a ground polygon where it sweeps | Break line of sight, go vertical, or flank — the cone is slow to traverse | Uncommon |
| 6 | **Splitting** | On death, spawns two copies at 25% health with no modifiers and no loot. | Segmented body with visible seams | Positioning: kill it away from the pack so the splits do not merge into a wall | Uncommon |
| 7 | **Warding Aura** | Nearby allies within 900cm take 35% reduced damage. Does not stack with another Warding Aura. | Tether lines drawn to every buffed ally | It is a priority-target puzzle. Rewards target selection over raw DPS | Uncommon |
| 8 | **Reflective** | Returns 12% of damage dealt to it back to the attacker as unmitigated-by-armour damage, capped at 6% of player max health per instance. | Faceted shell, hit flashes read as outward rather than inward | Reduces the "hold the trigger" answer. Cap is essential — an uncapped reflect punishes high-DPS builds for existing | Uncommon |
| 9 | **Phasing** | Every 6s, blinks 800cm toward the player and is briefly untargetable during the blink (0.35s). | Body desaturates and streaks before the blink | Never fight it with your back to geometry. It closes distance you thought was safe | Rare |
| 10 | **Cascading** | Its attacks leave a lingering hazard at the impact point for 5s. Hazards stack and never expire early. | Ground polygons that persist and darken | The arena degrades over time — it converts a fight into an eviction. Pure movement counterplay | Rare |

**Rocket interaction with Volatile and Cascading — O13.** Both modifiers are space-denial: Volatile's death detonation and Cascading's accumulating hazards both ask the player to vacate ground. A Rocket player carries their own splash, so the fair-costing question is whether they have enough answers. **Ruling O13** settles the inputs: *Rocket gets strong self-damage reduction and full self-knockback control, never immunity; rocket-jumping is tolerated, never required.* Design consequences here:

- **Never required.** No Volatile radius and no Cascading hazard spread may be escapable only by rocket-jumping. Both must remain answerable by the base kit — dash, slide, sprint — for every archetype and weapon. Volatile's counterplay line ("kill it at range, or dash/slide out") is the normative answer and stays that way.
- **Never immune.** A Rocket player clearing a Volatile radius by rocket-jumping still pays reduced self-damage for it. That cost is deliberate and neither modifier may waive it.
- **Tolerated.** Full self-knockback control means the rocket-jump escape is *reliable* when a player chooses it, so these modifiers must not be tuned on the assumption that Rocket players are pinned.

**Optional 11-12 if the list needs breadth later (designed, not slice-scoped):**

| # | Name | Effect | Weight class |
|---|---|---|---|
| 11 | **Tethered** | Two elites spawn linked; damage to one is shared 50/50 until one dies. Link is a visible beam and breaks if they move >2000cm apart. | Rare |
| 12 | **Wakeful** | Revives once at 35% health after 4s unless a killing blow was a weak-point hit. | Rare |

### 1.3 Combination rules

Modifiers combine, but not freely. Combination is where elite design turns into unplayable noise.

**Forbidden pairs:**

- Warded + Reflective — the player cannot make progress and is punished for trying.
- Splitting + Volatile — a death detonation that spawns two more detonations is a chain the player cannot read.
- Phasing + Cascading — an enemy that relocates constantly and permanently pollutes the arena has no stable answer.
- Any modifier with itself (obviously).

**Required-diversity rule:** a 2- or 3-modifier elite must draw from at least two different *pressure kinds*:

| Pressure kind | Modifiers |
|---|---|
| Durability | Warded, Warding Aura, Reflective |
| Space denial | Suppressing, Cascading, Volatile |
| Mobility | Fleetfoot, Phasing |
| Attrition / count | Splitting, Anchored |

A 3-modifier elite must not draw two from Durability. That is how sponges are born.

### 1.4 Rarity-like weights

Modifier count roll, per elite:

| Count | Weight | Effective rate |
|---|---|---|
| 1 modifier | 65 | 65% |
| 2 modifiers | 28 | 28% |
| 3 modifiers | 7 | 7% |

Modifier selection weights (drawn without replacement, forbidden pairs re-rolled up to 3 times then the count drops by one):

| Weight class | Per-modifier weight | Members |
|---|---|---|
| Common | 100 | Warded, Volatile, Fleetfoot, Anchored |
| Uncommon | 45 | Suppressing, Splitting, Warding Aura, Reflective |
| Rare | 15 | Phasing, Cascading |

Total weight pool = (4x100) + (4x45) + (2x15) = 610. A Rare modifier appears on roughly 2.5% of single-modifier elites and roughly 7% of elites overall.

**Naming note:** these weight classes are internal and must never be surfaced in UI using the words Aberrant or Anomalous — those are item rarities and the master sheet 1.2 forbids the collision.

### 1.5 Elite acceptance criteria

- [ ] Every modifier is identifiable from 20m in a fully untextured graybox, within 1.5s of the elite entering view.
- [ ] A player with base-kit movement only, no air jump, no parry, and median slice gear can defeat any legal 1-modifier elite solo without dying, in 8-20 seconds.
- [ ] A legal 3-modifier elite is defeatable solo but should read as "I should reposition first," not "I should leave."
- [ ] No elite fight exceeds 45s solo at median gear. Past 45s the fight is a sponge regardless of what the modifiers do.
- [ ] Forbidden pairs never generate. Automated test asserts this over 10,000 rolls.

---

## 2. Three normal enemy archetypes

Design principle for a movement-driven FPS: **normals should shape the player's route, not test their reflexes.** Three enemies, three different reasons to move.

Family assignment follows master sheet 1.5 and 8.2. Two are Vestiges (alien, unreadable, no military logic). One is an Altered infantry unit which **may only appear after the slice's Act II turn beat** — before it, only Vestiges exist.

### 2.1 SKITTER — Vestige, pressure/melee

| Property | Value |
|---|---|
| Role | Close the distance, deny standing still |
| Health | 100 (1.0x slice baseline) |
| Armour | 0 |
| Move speed | 420 cm/s (below player sprint 950, above player walk 650) |
| Detection range | 2200 cm |
| Attack | Lunge, 18 damage, 400cm reach |
| Attack cadence | 1.4s |
| Weak point | Dorsal mass, 1.75x |

**Behaviour.** Patrols in a small orbit. On detection, runs a direct intercept. At 700cm it enters a **lunge wind-up**: it crouches, the dorsal mass rises, and it commits to a straight-line 900cm leap 0.55s later. The leap direction is locked at wind-up — it cannot track.

**Telegraph.** 0.55s wind-up, body compresses, dorsal weak point becomes exposed and enlarges during the wind-up. In graybox this is a scale change on two primitives; no animation required.

**Counterplay vs a movement player.** The committed leap is the whole design. Any lateral movement — a strafe, a slide, a dash — defeats it, and the wind-up *exposes the weak point*, so the correct answer is "step sideways and shoot the thing it just showed you." It teaches the game's core rhythm in a single enemy and never punishes a player who simply keeps moving.

**Why it exists.** Skitters are the reason you do not stand still. They are cheap, they come in numbers, and they are the primary count lever for party scaling (§5).

### 2.2 LATTICE — Vestige, ranged/zone

| Property | Value |
|---|---|
| Role | Deny space, force route choice |
| Health | 160 (1.6x) |
| Armour | 40 (~29% mitigation at slice values) |
| Move speed | 180 cm/s (repositions, does not chase) |
| Detection range | 3500 cm |
| Attack | Slow projectile volley, 3 projectiles, 11 damage each, 900 cm/s |
| Attack cadence | 2.6s |
| Weak point | Core node, 1.75x, only exposed while firing |

**Behaviour.** Holds a position with sight lines. Fires a spread of three slow projectiles that arc toward the player's *current* position (no leading). Between volleys it drifts to maintain roughly 1800cm from the player. If the player closes inside 800cm it withdraws rather than melees.

**Telegraph.** The core node opens and glows for 0.8s before each volley, and stays open for 0.6s after. Projectiles are slow enough to see and outrun. Both windows are the shot opportunity.

**Counterplay vs a movement player.** Projectile speed of 900 cm/s against a sprint of 950 cm/s means **a moving player is never hit by a Lattice from the front.** That is deliberate. Lattices only land damage on a player who has stopped, is reloading, is committed to a slide down a fixed line, or is fighting something else. They convert "stand and aim" into a cost.

**Why it exists.** Lattices make arena geometry matter. They are placed to cover the obvious ground route, so the wall-ride and vertical routes become genuinely faster rather than decorative — without ever *requiring* them, per master sheet 5.4.

### 2.3 SEVERED WARDEN — Altered, anchor/pressure — POST-TURN ONLY

| Property | Value |
|---|---|
| Role | Hold ground, punish approach, protect Lattices |
| Health | 320 (3.2x) |
| Armour | 90 (~47% mitigation) |
| Move speed | 260 cm/s |
| Detection range | 2600 cm |
| Attack A | Shield sweep, 26 damage, 320cm frontal arc |
| Attack B | Ground slam, 34 damage, 650cm radius, on a 7s cooldown |
| Weak point | Exposed back/joint seams, 1.75x — **frontally armoured, rear unarmoured** |
| Stagger | High resistance |

**Behaviour.** Advances slowly and always faces the player. Frontal damage is mitigated by the full 90 armour; rear and flank hits bypass it entirely (treated as armour 0 plus the weak-point multiplier where the seams are hit). Uses the ground slam when the player is inside 650cm and it has not slammed in 7s.

This is the first enemy in the slice that **still wears insignia and still uses cover** (master sheet 1.5, "Stage as art direction" — this is an early-stage Altered). Graybox: give it a distinct rectangular shield primitive and a rank marking decal. It must read as *someone* in a way the Vestiges do not.

**Telegraph.** Sweep: shield arm draws back for 0.5s. Slam: it plants, and a ground ring expands to full radius over 0.9s — the ring *is* the hitbox preview, so the mechanic is legible with zero art.

**Counterplay vs a movement player.** Pure positional. A player who circles it takes almost no damage and does triple the damage; a player who trades with it frontally loses. Slide-under and dash-through are both valid ways to get behind it. This is the enemy that proves movement is a damage stat without ever converting momentum into a damage number (master sheet 3.3 explicitly removed momentum-to-damage conversion, and this respects that — the *positioning* is the multiplier, via armour geometry, not a stat).

### 2.4 Composition rules

The three archetypes are designed as a rock-paper-scissors *for the arena*, not for each other:

- Skitters punish standing still.
- Lattices punish moving predictably.
- Wardens punish approaching from the front.

A pack containing all three has no single correct answer, which is the point. Pack templates:

| Template | Composition | Intent |
|---|---|---|
| Probe | 4 Skitter | Teach the lunge sidestep — the leap is answered by lateral *movement*, never by a defensive input (O1) |
| Emplacement | 1 Lattice + 3 Skitter | Skitters push you into the Lattice line |
| Wall | 1 Warden + 2 Lattice | Warden holds the ground route; go around or over |
| Full | 1 Warden + 1 Lattice + 4 Skitter | The standard slice pack |
| Elite pack | Full, with one member promoted to elite | See §1 |

Never place two Wardens in a pack under 5 players. Two frontal-armour anchors with overlapping facings creates a geometry problem with no clean solution.

---

## 3. The slice boss — the Altered commander

Per master sheet 8.7 and 10.2, the slice boss is an Act II Altered commander: **the first humanoid that demonstrably gives orders.** That fact is the whole point. The mechanics must *show* command, not state it.

**Working name: THE FIELD MARSHAL.** (Placeholder. Do not name it anything using Bastion / Aberrant / Anomalous.)

### 3.1 The design thesis

The boss is not a big Warden. The boss is a Warden that **commands the other three archetypes.** Every phase mechanic is an order given to adds, and the player's job is to read who is being ordered to do what. When the player realises the adds are responding to it, the story fact lands mechanically before any dialogue says it.

Corollary: this boss **must not be a damage sponge**, because its interest lives in the adds. Health should be low for a boss.

### 3.2 Stat block

| Property | Value | Note |
|---|---|---|
| Health | 2400 (24x a Skitter) | Split 800 per phase |
| Armour | 90 frontal / 0 rear | Same geometry rule as the Warden — consistency is a teaching tool |
| Move speed | 300 cm/s | Deliberately slower than the player |
| Stagger | Immune | |
| DoT | Capped at 3 stacks of any physical DoT (master sheet 7.10 risk #5); DoTs still deal full snapshot damage, they just do not stack unbounded |
| Weak point | Command apparatus on the back — 1.75x, and **only exposed during Orders** | |
| Drops | Boss table. **Only slice source of T-1** (master sheet 3.1). One guaranteed Exceptional+, T-1 on a controlled roll | |

**DIVERGENCE [O18]: the boss health figure is not validated against the 20–45s band.** 2400 health (24x a Skitter) was anchored to the same placeholder baseline as every other health number here (see OPEN QUESTION 3), not to a TTK target. Whether 2400 lands inside 20–45s is unknown until wave mode reports. Compounding it: the fight is gated into **three 800-health phases** with fixed-cadence orders (20s DEPLOY in Phase 1, 15s FIRE in Phase 2), so the *phase script* imposes its own floor on fight length independent of health — a player who bursts a phase down in 10s still waits on the order cadence. If the phase cadence alone pushes the encounter past 45s, health cannot fix it. **Not resolved here; no value changed (O2).** The Field Marshal is a plausible candidate for O18's "unless a special enemy" clause, but that must be claimed by the owner, not assumed by this document.

### 3.3 The arena — graybox spec

A single room, 4000cm x 4000cm, three tiers of elevation.

```
        [ N gallery, +600 ]
   ___________________________
  |  .                     .  |     .  = add spawn alcove (4 total, one per corner)
  |        [ pillar ]         |     [] = full-height cover, 400cm square
  |   ___                     |
  |  |_W_|   ( CENTRE )  |_E_||     W/E = ramp to gallery, wall-rideable face
  |            +0             |
  |        [ pillar ]         |
  |  .                     .  |
  |___________________________|
        [ S gallery, +600 ]
```

- **Centre floor (+0):** open, no cover. Where the boss wants you.
- **Two pillars:** full-height, break Lattice sight lines, and are the only hard cover. Placed off-centre so a single pillar never covers both galleries.
- **N and S galleries (+600cm):** reachable by ramp (conventional route) or by a two-wall-ride chain off the W/E faces (fast route). Master sheet 5.4 requires that conventional routes are not punished — the ramp works, it is just slower. Galleries give a clean firing line onto the boss's back when it is facing the centre.
- **Four corner alcoves:** add spawn points, visible from the centre, so incoming adds are always previewed.
- **No pit, no instant death, no grapple points.** Fall damage exists but the tier delta is 600cm, well under a lethal fall.

Arena acceptance: the entire fight must be legible with untextured BSP and four primitive colours. If a mechanic needs a VFX artist to read, redesign it.

### 3.4 Phases

Health gates, not timers. Timers punish low-DPS builds; the master sheet's whole gear thesis needs boss pacing to respond to player power.

---

**PHASE 1 — "Deployment" (100% -> 66%)**

The boss holds the centre and fights like a Warden: sweep and slam, frontal armour, exposed rear.

**Order: DEPLOY.** Every 20s the boss raises the command apparatus for 2.5s and points at an alcove. Two Skitters spawn from *that* alcove, 1.5s later.

- During the 2.5s raise, the apparatus (rear weak point) is exposed **and visible from the front** — it lifts above the shoulder line. This is the punish window and it is generous.
- The pointed alcove is telegraphed before the spawn, so the player can pre-aim or reposition.

**Player lesson:** the adds are not ambient. It chose that corner.

---

**PHASE 2 — "Suppression" (66% -> 33%)**

Two Lattices spawn permanently on the N and S galleries and do not leave. They respawn 12s after death. The boss gains a new order.

**Order: FIRE.** Every 15s the boss signals (apparatus raised, 2.0s) and **both Lattices volley simultaneously** at the player's position, ignoring their own cadence.

- The signal is the same animation as DEPLOY, so the player must read *which* order by where it points: at an alcove (adds) or at a gallery (volley).
- Six projectiles at 900 cm/s converging on one point is trivially avoided by moving, and near-unavoidable if the player is reloading in the open. This is a discipline check, not a reflex check.
- The boss also begins **rotating to face the player continuously** in this phase, so the rear weak point must now be earned by out-turning it — which the galleries and pillars make possible.

**Player lesson:** the ranged units are its guns. Killing the Lattices is worth doing, and they come back, so timing matters.

---

**PHASE 3 — "Commitment" (33% -> 0%)**

The boss stops giving orders and fights. Adds stop spawning; anything alive stays alive.

- Boss move speed +40% (to 420 cm/s — still slower than player sprint).
- Sweep cadence -30%.
- Ground slam cooldown 7s -> 4s, and each slam now leaves a lingering hazard polygon for 8s (the Cascading modifier's behaviour, reused — deliberate: the player has already learned to read it on elites).
- Frontal armour drops from 90 to 45 and **the apparatus stays permanently exposed**, because it has stopped commanding. Its damage output rises and its defence falls.

The arena degrades as slam hazards accumulate. The fight ends because the floor runs out, which is a movement-game ending rather than a DPS-check ending.

**Player lesson (narrative, unstated):** it fights hardest when it stops being a commander. Do not have any NPC say this.

### 3.5 Boss acceptance criteria

- [ ] A solo player at median slice gear, base kit only, clears the fight in 3:00-5:00 on a first successful attempt.
- [ ] A player who never enters a gallery can still win. The galleries are an optimisation, not a gate.
- [ ] A player who never uses wall ride can still win.
- [ ] Every mechanic is readable with untextured geometry and four colours.
- [ ] Phase 3 is survivable at 0% hazard-clear skill for at least 40s — the arena degradation is a pressure, not an instant loss.
- [ ] The DPS floor is such that a deliberately weak build does not soft-lock: no enrage timer, but hazard accumulation in Phase 3 provides a natural, *readable* soft failure.
- [ ] Post-fight, the player can articulate "it was telling them what to do" without being told.

---

## 4. Wave / arena mode for the gym

The Playtest Gym (`Docs/Playtest-Gym-v1.md`) currently has patrol/chase/attack enemies and diagnostic targets. Wave mode makes it a tuning instrument.

### 4.1 Purpose

Wave mode is a **measurement tool**, not content. Its job is to produce the numbers the master sheet says are missing everywhere: real TTK, real drop-rate pressure, real affix value anchoring (master sheet 3.0: "Re-anchor them after the Playtest Gym feedback pass").

**It now measures against the O18 seed targets** (§1.1). The instrument reports **divergence from the designer inputs**, not truth: per-Rank TTK against a little under 1s / ~3s / 20–45s, and TTD against 4–5s bare and substantially higher with resources and sustain invested. A divergence is a finding for the owner, never a licence for this document to re-author a value under the O2 freeze.

### 4.2 Structure

Endless, 12-wave repeating cycle with escalating budget. Each wave is built from a **spawn budget** spent on archetypes.

| Archetype | Budget cost |
|---|---|
| Skitter | 1 |
| Lattice | 3 |
| Warden | 6 |
| Elite promotion (any) | +4 per modifier |

Wave budget: `Budget(n) = 6 + 4n` for wave n, capped at 90 (reached at wave 21).

| Wave | Budget | Example composition | Elites |
|---|---|---|---|
| 1 | 10 | 10 Skitter | 0 |
| 2 | 14 | 8 Skitter + 2 Lattice | 0 |
| 3 | 18 | 6 Skitter + 2 Lattice + 1 Warden | 0 |
| 4 | 22 | 4 Skitter + 3 Lattice + 1 Warden + 1 elite Skitter (1 mod) | 1 |
| 5 | 26 | Full pack x2 | 1 |
| 6 | 30 | **Rest wave** — half budget, loot drop, 20s breather | 0 |
| 7-11 | 34-54 | Escalating, elite count = floor(wave/4) | 1-2 |
| 12 | 58 | **Boss wave** — the Field Marshal, Phase 1+2 only | — |

Then the cycle repeats with budget continuing to climb until the cap.

### 4.3 Rules

- **Rest waves every 6.** Without them, wave mode measures endurance instead of combat.
- **Loot only on rest and boss waves.** Otherwise the gym becomes a farm and pollutes drop-rate data.
- **Composition variety enforcement:** no wave may be more than 70% budget in a single archetype after wave 3.
- **Instrumentation required, per wave:** time to clear, damage taken, damage dealt per archetype, shots fired vs hit, weak-point hit rate, distance travelled, time airborne, time sliding, deaths. These feed the existing clipboard report.
- **Metric that matters most:** *time-airborne-or-sliding as a fraction of combat time*. If it is under 25%, the encounter design has failed the game's premise regardless of how fun the wave felt.

### 4.4 Arena

Reuse the boss arena geometry for wave mode. Same room, four alcoves as spawn points, two pillars, two galleries. One arena that serves both means every tuning insight transfers directly to the boss fight.

---

## 5. Spawn pacing rules

### 5.1 Principles

1. **Spawns are always previewed.** An enemy never appears in the player's current field of view without a 1.0s+ tell at a known location (alcove, rift aperture, door). Ambush-from-nothing is unfair in a game where the player is moving at 950 cm/s and cannot rewind.
2. **Never spawn inside the player's movement corridor.** Compute a 1200cm forward cone from the player's velocity vector; suppress spawns inside it. A movement player crashing into a freshly-spawned enemy reads as a bug.
3. **Minimum spawn distance:** 1500cm from the player. Maximum useful: 4000cm (beyond that they are irrelevant for too long).
4. **Pack cadence, not trickle.** Spawning one enemy every 3 seconds produces a treadmill. Spawn packs of 3-6 with 12-25s between packs. Gaps are where movement is expressive; without gaps the player is pinned.
5. **Density ceiling:** never more than 12 live enemies per player in the arena, and never more than 3 Lattices total regardless of party size (see §5.3).

### 5.2 Pacing curve within an encounter

| Beat | Duration | Content |
|---|---|---|
| Approach | 8-15s | Zero enemies. Establishes the arena and the routes. Non-negotiable — the player must see the room before it is contested. |
| First contact | 20-30s | One pack, no elite. Teaches the room's specific geometry. |
| Escalation | 45-90s | 2-3 packs with 15s gaps, one elite in the last. |
| Peak | 20-40s | Everything alive at once, or the boss. |
| Resolution | 10s | No spawns. Loot, breathe, read the room again. |

### 5.3 Hard caps

| Cap | Value | Reason |
|---|---|---|
| Live enemies, solo | 12 | Above this, a movement player cannot find a lane |
| Live Lattices, any party size | 3 | Four converging projectile sources removes all safe ground; this is the single most dangerous scaling knob |
| Live Wardens, per player | 1 | Frontal-armour anchors overlapping create unsolvable geometry |
| Live elites, solo | 1 | Two elites means two modifier sets to read simultaneously |
| Live elites, 5-player | 3 | |
| Simultaneous ground hazards | 8 | Arena denial past this is an eviction with nowhere to go |

---

## 6. Scaling 1-5 players

Master sheet 11.2 is explicit: **DO NOT SCALE ONLY HEALTH.** This section is the implementation of that instruction.

### 6.1 The scaling philosophy

Every additional player adds a *pair of eyes and a set of lanes*, so the correct response is **more things to look at and fewer free lanes**, not thicker enemies. Concretely: scale count first, role pressure second, health last and least.

A five-player encounter should feel like a *busier* fight, not a *slower* one. If TTK per enemy rises with party size, the design has failed.

### 6.2 The scaling table

| Players | Enemy count | Enemy health | Enemy damage | Elite frequency | Role pressure |
|---|---|---|---|---|---|
| 1 | 1.00x | 1.00x | 1.00x | baseline | baseline |
| 2 | 1.70x | 1.05x | 1.00x | 1.5x | +1 Lattice slot |
| 3 | 2.40x | 1.10x | 1.00x | 2.0x | +1 Warden slot |
| 4 | 3.10x | 1.15x | 1.05x | 2.5x | +1 elite slot, packs overlap |
| 5 | 3.80x | 1.20x | 1.10x | 3.0x | flanking spawns enabled |

Note the shape: **count nearly quadruples while health rises 20%.** That is the entire point.

### 6.3 Role pressure — what actually changes

Count alone is insufficient; four times as many Skitters is still one problem. Each step adds a *kind* of problem:

- **2 players:** an extra Lattice slot opens. One player can no longer hold every sight line alone — the group must split attention.
- **3 players:** a second Warden becomes legal (the solo prohibition lifts, because two players can flank in opposite directions). Ground control now requires coordination.
- **4 players:** packs are allowed to **overlap** — a new pack spawns while the previous is at 40% rather than cleared. This removes the breather gaps that solo play depends on, which is the correct cost of having four people.
- **5 players:** **flanking spawns** enable — packs may spawn from two alcoves on opposite sides simultaneously. No single facing is safe. Five players can cover 360 degrees; one player cannot, which is why this is gated to five.

### 6.4 Boss scaling

The boss scales differently, because its interest is in the adds:

| Players | Boss health | Boss damage | Adds per DEPLOY | Lattices in Phase 2 | Slam radius |
|---|---|---|---|---|---|
| 1 | 2400 | 1.00x | 2 | 2 | 650cm |
| 2 | 3400 | 1.00x | 3 | 2 | 650cm |
| 3 | 4400 | 1.05x | 4 | 3 | 700cm |
| 4 | 5300 | 1.10x | 5 | 3 | 750cm |
| 5 | 6200 | 1.15x | 6 | 3 | 800cm |

Boss health is 2.6x at five players against a party doing roughly 5x the damage — so the boss dies *faster* in a group, and the difficulty is entirely in the add pressure and the shrinking safe floor. Lattices cap at 3 per §5.3 regardless of party size; the extra pressure is delivered through DEPLOY volume instead.

**Phase gates stay at 66% and 33% of scaled health**, so every party sees all three phases.

### 6.5 Downed-player and revive interaction

**EXTENDS** — the master sheet does not cover this and it is load-bearing for party encounter design.

A downed player should not remove a lane permanently. Recommend: downed state with a bleed-out timer, revives with the existing Interaction & Revive Speed affix (master sheet 3.10) as the scaling stat. Critically, **spawn pressure must pause or slow while a revive is in progress**, or reviving becomes strictly incorrect play in a game where standing still is the losing move. Suggested rule: no new pack spawns while any player is downed, and the pack timer resumes on revive or bleed-out.

### 6.6 Scaling acceptance criteria

- [ ] Median TTK per individual normal enemy does not increase by more than 20% from 1 player to 5 players.
- [ ] Total encounter clear time is within +/- 25% across all party sizes at equivalent gear.
- [ ] At 5 players, the number of *distinct simultaneous threats* is at least 3 (e.g. adds + volley + hazard), not one threat multiplied.
- [ ] No party size makes any of the three archetypes irrelevant.
- [ ] Solo remains the tightest tuning; if any party size is easier per-player than solo, scaling is under-tuned.

---

## 7. Implementation notes

- **Data-driven.** Elite modifiers, archetype stat blocks, pack templates, wave budgets, and the scaling table all belong in Data Assets / Data Tables per `Docs/Architecture.md`. C++ owns the roll pipeline, the forbidden-pair validation, the spawn budget solver, and the density caps.
- **`ConfigureElite()` needs replacing** with a modifier-driven path: `ConfigureElite(const TArray<FBreakerEliteModifier>& Modifiers)`, with the stat chassis derived from modifier count. The current fixed 1.5x/3x/2x becomes the zero-modifier fallback.
- **Enemy archetypes are three Data Assets over one `ABreakerEnemy`**, not three C++ classes. The behavioural differences (lunge commit, projectile volley, facing-armour) are three behaviour flags plus tuning, not three codebases.
- **Facing-dependent armour** does not exist in the damage pipeline yet. The Warden and the boss both need it. It is a dot-product check between the hit normal and the enemy's forward vector, applied before the armour step in the master sheet 6.1 order. This is the one genuinely new combat-pipeline requirement in this document. **EXTENDS** master sheet 6.1.
- **Enemy shields** (the Warded modifier) already route through the existing shield step.
- **Elemental modifiers were deliberately excluded** from the elite list. Master sheet 6.1 and 3.7 both flag that no elemental resistance model exists; an elite modifier dealing element damage would have nothing to resolve against. Revisit once resistances ship. **The element set is ruled and final [O19]: Rift / Entropy / Void** (O19 renames Time to Entropy) — not fire/ice/lightning, so any future elemental modifier is authored against those three names and no other. What is still missing is the resistance *model*, not the element names.

---

## OPEN QUESTIONS

1. ~~**Are dodge and block passive rolls or player inputs?**~~ **CLOSED by O1** — passive, and the stamina pool is removed. See §0. Every telegraph window in §2 and §3 was already tuned for passive and stands unchanged. One residual item: the Suppressing modifier's stamina-drain half is now unreferenced and carries a NEEDS-RECOST [O1/O2] tag in §1.2.
2. **Does facing-dependent armour get built?** The Warden and the boss are both designed around "flank it and its defence disappears," which is my main answer to "make encounters about position, not health." Without it, both fall back to being ordinary armoured enemies and lose most of their teaching value.
3. **What is the real TTK baseline?** Every health number here (Skitter 100, boss 2400) is anchored to a placeholder, exactly as the master sheet warns in 3.0. These must be re-derived from the wave-mode instrumentation in §4 before anything is called balanced.
4. Does the boss's T-1 drop use the boss-specific reward table, a guaranteed exalt/corrupt consumable, or both? Master sheet 3.1 allows all three and 9.4 leaves the gating open.
5. Should the Act II turn beat live inside the slice as a scripted moment, or is the Severed Warden simply the slice's third enemy with the turn handled narratively elsewhere? Master sheet 8.2's "no Altered before the beat" rule makes this a content-ordering problem, not just a fiction one.
6. Is enemy level fixed per zone or scaled to player level? Master sheet 6.7 leaves it open; the wave-mode budget model in §4 assumes fixed.
7. Loot distribution in parties (instanced / shared / need-greed) is open per 11.3, and it changes whether elites should drop more or better at higher party sizes.
8. Does the elite loot floor of Exceptional scale with modifier count? A 3-modifier elite is meaningfully harder than a 1-modifier one and currently pays identically.
9. Do enemies get stagger/flinch as a system at all? §1.1 assigns elites stagger resistance, which presupposes a stagger model that does not currently exist.
10. ~~Self-damage from the Rocket archetype~~ **CLOSED by O13** — strong self-damage reduction, full self-knockback control, never immunity; rocket-jumping tolerated, never required. Volatile and Cascading are costed against that in §1.2. The general weapon self-damage *rate* remains unauthored and is frozen by O2.

---

# AS BUILT — 2026-08-14

Last reconciled against: `Decisions.md` (O1-O28), `Story-Source.md` §1.2/§1.5,
`Power-Curve.md` §2, `CONTEXT.md`.

Everything below is **shipped C++ under `Source/RiorsEdge/Combat/`** and proven
by 12 new automation tests (`RiorsEdge.Combat.Modifiers.*`,
`RiorsEdge.Combat.Boss.*`, `RiorsEdge.Combat.Archetypes.*`); the suite is 163
green. **Nothing here is playtested.** Automation proves arithmetic and legality
rules and cannot see whether a telegraph reads on a screen — which is exactly
the limit that let two bad visual passes ship elsewhere in this project. Every
number is `EditAnywhere` and flagged `O2 PLACEHOLDER` at the code.

## A. The modifier system — §1 is now real

`EBreakerMonsterRank::ModifierBearing` existed and multiplied health by 2.5x,
and **nothing anywhere authored a modifier**. The rank was a stat bump wearing
the name of a system, which left trash health as the only place difficulty
could live — the sponge O27 forbids.

**Files.** `BreakerEnemyModifiers.{h,cpp}` is the pure, world-free layer (the
precedent of `BreakerRangedBehavior.h`): the enum, the authored
`FBreakerEnemyModifierParams`, the pressure/weight tables, forbidden pairs,
legality, deterministic selection, and every magnitude.
`BreakerModifierComponent.{h,cpp}` is the runtime: one component on every
`ABreakerEnemy`, replicated modifier list, server-only effects.

**Nine of §1.2's ten, plus Wakeful.** Shipped: Warded, Volatile, Fleetfoot,
Anchored, Splitting, Warding Aura, Reflective, Phasing, Cascading, Wakeful.

- **SUPPRESSING is deliberately absent.** §1.2 tags it NEEDS-RECOST [O1/O2]:
  its stamina-drain half has no referent since O1, its surviving air-control
  half lives in `Movement/` (not this lane), and authoring a replacement second
  effect is a new value decision frozen by O2. Shipping it as a stub would have
  been worse than its absence — an enemy visibly carrying a modifier that does
  nearly nothing teaches the player modifiers do not matter.
- **WAKEFUL (§1.2's optional #12) is promoted into the shipping set.** TETHERED
  (#11) is not: it is a rule about a *pair* of enemies and needs a spawn-time
  pairing contract nothing in `Game/` can express.

**TWO DELIBERATE DEVIATIONS, both forced by O27.** §1.2 authors Volatile at
"45% of **player** max health" and Reflective's cap at "6% of **player** max
health". Both read the player. Re-expressed:

| Modifier | Doc | As built | Why |
|---|---|---|---|
| Volatile | 45% of player max health | `VolatileDamageAsAttackMultiple` x the monster's own chassis damage (9x) | Chassis damage is already a curve in area level, so the detonation scales with the CONTENT |
| Reflective | cap 6% of player max health | `ReflectCapFractionOfMonsterHealth` x the monster's own max health (5%) | The cap grows with area level and never with the player's build |

The cap is the load-bearing half of Reflective either way: §1.2 is right that an
uncapped reflect punishes high-DPS builds for existing.

**Composition (§1.3) is enforced at selection AND checkable after.** Forbidden
pairs: Warded+Reflective, Splitting+Volatile, Phasing+Cascading, any modifier
with itself, **plus one EXTENDS entry — Wakeful+Splitting** (dies, splits,
revives is a three-step death chain a player cannot read; the same reason
Splitting+Volatile is forbidden). The pressure-diversity rule and the
"no two Durability in a 3-set" anti-sponge rule are both live.
`RiorsEdge.Combat.Modifiers.SelectionSweep` runs §1.5's acceptance criterion
verbatim — 10,000 rolls per family, zero illegal sets, every modifier reachable.

**Announcement is a hard requirement, not a nicety.** Granting modifiers always
builds a halo (a coloured sphere plus a point light, engine primitives, sized by
modifier count and coloured by the first modifier) and always publishes a banner
that `ABreakerEnemy::GetEnemyStateLabel()` prefixes onto the existing overhead
readout. Reusing the readout the HUD already prints means a future UI pass that
knows nothing about modifiers cannot drop the announcement. An unmodified
enemy's label is byte-identical to before.

**Rank composition.** O9 names ranks Standard/Veteran/Champion;
`EBreakerMonsterRank` predates the rename. Mapping shipped: 0 modifiers =
`Trash`, 1+ = `ModifierBearing`; `Elite` stays an AUTHORED pack anchor and is
not modifier-derived. §1.1's "+0.35x per modifier beyond the first" composes as
a third multiplier alongside rank and archetype — one product, three inputs, no
second source of truth.

**FINDING, recorded not fixed (O2).** At the shipped rank row (2.5x) and step
(+0.35), the count step contributes MORE health than the rank promotion does:
a 3-modifier Champion is +1.75x-trash above `ModifierBearing`, while
`ModifierBearing` is only +1.5x-trash above `Trash`. §1.1's own 2.0x/+0.35
numbers have the same shape. The bound that does hold, and is tested, is that
three modifiers never doubles a one-modifier enemy's health. Whether the step
should be smaller than the rank row is an owner call.

**Also new, and needed by two of them:** `UBreakerCombatComponent::OnDamageTaken`
— a VICTIM-side `FBreakerHitContext` broadcast. `OnDamageReceived` carries only
a result and cannot say *who* hit you, so Reflective had no way to answer the
attacker and the skirmisher's flinch had no way to tell a bullet from a bleed.
Reflect is dealt as `TrueDamage` and the handler refuses `TrueDamage`, so a
reflect can never reflect a reflect.

## B. Facing-dependent armour — §7's "one genuinely new pipeline requirement"

Built, and built **per-hit** rather than per-frame.
`UBreakerDamageLibrary::GetFacingArmorMultiplier` is pure 2D geometry;
`UBreakerCombatComponent` applies it inside `ReceiveDamage` using the
`SourceLocation`/`bHasSourceLocation` the weapon, both projectiles, the zones
and the melee sweep already fill in. A per-frame "is the player behind me"
approximation would let a shot fired from the front resolve as a rear hit
because the shooter moved during flight — the kind of silent inconsistency that
teaches players a mechanic is random.

`RearArcArmorMultiplier` defaults to **1.0 (off)**, so no existing enemy
changed. `RearArcCosine` (0.15) widens the vulnerable arc slightly onto the
flanks, which is what makes "circle it" the answer rather than "stand precisely
behind it" against something that always turns to face. Z is excluded: getting
*above* an enemy is not flanking it. **This closes OPEN QUESTION 2.**

## C. Archetypes

### C1. SKITTER — the committed leap is now real (§2.1)

The base melee lunge shipped with neither half of what §2.1 specifies: no
wind-up, and a direction re-solved every frame, so it **tracked** the player
through the whole burst and there was nothing to step out of. Now:
`LungeWindupSeconds` 0.55, movement cut to 25%, the weak point **swells** during
the wind-up (`LungeWeakPointSwell` 2.1x), and the direction is locked **once**
at commit and never re-solved. The doc's design lands: "step sideways and shoot
the thing it just showed you."

### C2. SEVERED WARDEN — `ABreakerWardenEnemy` (§2.3)

Health 3.2x / damage 1.86x as chassis ratios (§2.3's 320 and 26 against the
Skitter baseline), 90 frontal armour with an unarmoured rear, move 260,
implacable (no weave, no lunge), always faces the player.

- **Attack A, shield sweep:** 320 cm, 65-degree arc, 0.5 s shield draw-back that
  also heats. The arc is re-checked at RESOLUTION, so walking out of the cone
  during the draw-back beats it — the counterplay under passive defence.
- **Attack B, ground slam:** 650 cm, 7 s cooldown, a ground ring that grows to
  **exactly** the real radius over 0.9 s. The preview is the hitbox. The slam
  exists so standing behind it is not free, or the archetype would teach "get
  behind it and hold the trigger".
- **DIVERGENCE [O18]:** 3.2x health on a Trash-rank enemy is a long fight
  against "trash a little under 1 second", before its frontal armour roughly
  doubles effective health from the front. §2.3's own answer is that it is not
  meant to be fought frontally. Unresolved; O2 freezes it until wave mode
  reports.

### C3. SEVERED SKIRMISHER — `ABreakerSkirmisherEnemy` — NOT IN THIS DOCUMENT

Added from `Story-Source.md` §1.5's severance spectrum, and it fills the largest
gap in the roster. Skitter closes, Lattice holds a band, Warden advances — all
three answer to "keep moving and shoot the thing in front of you", and **nothing
in the project had ever broken line of sight or reacted to being shot.**

§1.5: an early-stage Altered "still wears insignia, still uses cover, still
flinches". This is that sentence. Loop: **Relocating -> InCover -> Exposed ->
(Flinched) -> Relocating.** It picks a cover point whose line from the player is
actually blocked (`WorldStatic` trace at eye height; another enemy is not
cover), crouches behind it, peeks after 1.1 s, stands and fires a **3-round
burst** of fast flat rounds (2600 cm/s, no lead — LATTICE's partial lead exists
because its orb is slow), and goes back down after 2.6 s. Landing a
non-DoT hit while it is exposed **cancels the burst** and sends it back to cover
(0.45 s flinch, 1.2 s cooldown so a high-RPM weapon cannot chain it).

**The response it demands is the only one in the roster that is not about aim:
PUSH.** You cannot out-shoot something behind a wall; waiting loses the
attrition; the answer is to close, take an angle its cover does not face, and
catch it during a relocation — a movement decision under O1.

Pure cover maths is `BreakerCoverBehavior.{h,cpp}` (candidate ring generation
with per-enemy phase desync, range-band rejection, travel-vs-range scoring).
With **no cover in range it correctly finds none and degrades to an open-ground
shooter**, labelling itself `NO COVER` so a playtester can tell "this map has no
cover" from "the cover logic is broken". *Nobody has confirmed the gym has any
geometry it can hide behind — in the open field this is honest but is NOT the
fight.*

### C4. Composition (§2.4) is now four axes, not three

Skitters punish standing still. Lattices punish moving predictably. Wardens
punish approaching from the front. **Skirmishers punish staying at range.**

## D. THE FIELD MARSHAL — `ABreakerBossEnemy` (§3)

**It subclasses the Warden**, which is §3.1's thesis in the type system: "the
boss is not a big Warden. The boss is a Warden that COMMANDS." The sweep, the
slam, the facing armour and the advance are the fight the player has already
learned; everything the class adds is command.

**Not a sponge (§3.1's corollary).** It does not author a health number — rank
`Boss` (x25) comes from the chassis rank table, and `ArchetypeHealthMultiplier`
is **0.35** so the inherited Warden 3.2x does not compound into eighty times a
trash mob. §3.2's literal 2400 was anchored to a baseline that no longer exists;
duplicating it would be the second-source-of-truth bug O27 deleted from
`ConfigureElite`.

**Telegraph vocabulary — three tells, all learnable:**

1. Shield draw-back -> a sweep is coming to your front. *(inherited)*
2. Growing ground ring -> a slam is coming to your feet. *(inherited)*
3. **Apparatus raise -> an ORDER is coming, and where it POINTS says which.**
   §3.4 deliberately reuses one gesture for both orders, so the player reads the
   direction rather than the pose. A second channel (colour: amber DEPLOY, blue
   FIRE) is insurance for a player standing behind it who cannot see the point.

The raise is also **the punish window**: it lifts the rear weak point above the
shoulder line so it is hittable from the front. Outside orders the weak point is
literally untargetable, not a damage filter.

**Phases are health gates (§3.4), and the machine is pure**
(`BreakerBossPhases.{h,cpp}`) so every transition is testable with no world:

| Phase | Gate | What it does |
|---|---|---|
| **Deployment** | 100-66% | Fights like a Warden. **DEPLOY** every 20 s: 2.5 s raise pointing at an alcove (round-robin, so it reads as a *choice*), 2 adds arrive 1.5 s later at that alcove — previewed before anything comes out (§5.1) |
| **Suppression** | 66-33% | Two gallery Lattices spawn and respawn 12 s after death. **FIRE** every 15 s: 2.0 s raise, then every live Lattice volleys simultaneously |
| **Commitment** | 33-0% | **Stops commanding.** +40% speed, sweep cadence -30%, slam 7s -> 4s, **each slam leaves a lingering hazard** (the Cascading modifier's behaviour, reused because the player already learned to read it), frontal armour 90 -> 45, apparatus **permanently exposed** |

Phase advancement is **monotonic**: a heal, a shield, or a chassis rebuild under
the boss cannot walk it back into a phase it left, or the fight has no defined
length. The order clock **resets rather than subtracting**, so a 60-second hitch
deploys one pack and not three — §5.1 forbids unpreviewed spawns.

**FIRE commands a WIND-UP, not a shot.** `ABreakerRangedEnemy::CommandVolley()`
starts the Lattice's own 0.85 s emitter bloom rather than firing. Six
projectiles appearing with no tell has no answer under O1. The boss's 2.0 s
raise is therefore the first half of the telegraph and the Lattices' bloom the
second — over 2.8 s of warning. A doubled order cannot restart a wind-up already
in progress, which would *shorten* the tell.

Also live: DoT capped at 3 stacks (§3.2), `MaximumLiveAdds` enforcing §5.3's
density ceiling **at the source that creates the density**, gallery Lattices
capped at 3 (§5.3's "single most dangerous scaling knob"), no respawn, no chain
detonation, and the galleries die with the boss so the encounter has an end
condition.

**NOT BUILT: the arena (§3.3).** Levels are editor work and `Game/` is not this
lane. Alcove and gallery positions are **authored offsets from the boss's own
spawn point**, so it works in the flat gym today and snaps onto the real
4000x4000 room by retuning six vectors.

**DIVERGENCE [O18] carried forward, unresolved.** The fight has a *script
floor*: a player who bursts a phase down still waits on the order cadence, so
phase cadence imposes a length independent of health. Whether the composition
lands inside 20-45 s is a measurement. O2 freezes the values.

## E. The two families (`Story-Source.md` §1.5)

`EBreakerEnemyFamily` (Vestige / Altered) and `EBreakerSeveranceStage`
(NotApplicable / Early / Mid / Late) are fields on `ABreakerEnemy`, defaulting
to Vestige — everything that shipped before the families existed is rift-native
by fiction and by behaviour. **They are gameplay fields, not lore tags:**

- **Modifier selection is family-scoped.** Splitting, Phasing and Reflective are
  Vestige-only (alien-body rules); Anchored and Warding Aura are Altered-only
  (tactical decisions, and §1.5 says a Vestige has no tactics resembling a
  military). The other five are things a body does rather than things a mind
  chooses and are legal on both. Both families still reach a legal 3-set —
  tested.
- **Stage drives behaviour, not just the model.** `UsesCoverDiscipline()` and
  `FlinchesWhenHit()` are asked of the family/stage pair every frame, so
  re-authoring the skirmisher as late-stage genuinely turns both off and leaves
  a plain open-field shooter. A Vestige can never use cover or flinch whatever
  stage is set on it — a hard family gate, so a content author cannot
  accidentally produce a tactical rift-native creature.
- **The Altered print their stage** over their heads; Vestiges print nothing,
  because a Vestige was never a person and never degraded from anything. This is
  also the only place the militia's engage-on-sight tragedy is visible in
  gameplay: the readout tells you what stage it is and you still have to kill it.

**Roster placement:** Skitter and Lattice are Vestige (unchanged). Warden is
**Altered / Mid** — §2.3 describes an early stage ("still uses cover"), but the
implemented behaviour does not use cover and does not flinch, and labelling it
early while it fights like that would make the stage a lie. Skirmisher and the
Field Marshal are **Altered / Early** — the boss because command is the highest
cognition on the spectrum, which is what makes "the first humanoid that
demonstrably gives orders" land mechanically.

**Naming:** VESTIGE is the formal term and is what the HUD prints; SPILL is
field slang and deliberately does not appear in instrumentation. No Bastion, no
Aberrant, no Anomalous anywhere in this work.

## F. What still needs doing in `Game/` — NOT DONE BY THIS LANE

`Game/BreakerGameMode.cpp` is owned by another agent this wave and was not
touched. Everything below is exposed and ready; it needs spawning:

1. **Modifiers on gym and wave elites.** Call
   `ABreakerEnemy::ConfigureWithModifiers(Seed)` on the arena elite and on
   wave-mode elites. It rolls a legal family-scoped set, promotes the rank, and
   rebuilds the chassis. §1.4's rate is built in; a deterministic seed
   (location hash + wave index) keeps a playtest reproducible.
2. **Wardens.** Spawn `ABreakerWardenEnemy` in the encounter and from wave 3
   (§4.2 budget cost 6). §5.3: never more than one per player, and §2.4: never
   two in one pack under 5 players.
3. **Skirmishers.** Spawn `ABreakerSkirmisherEnemy` **near cover** — its whole
   fight depends on there being geometry, and the gym's combat pockets, ruins
   and watchtowers are the candidates. In the open it degrades to a plain
   shooter and the archetype is wasted.
4. **The boss.** Spawn `ABreakerBossEnemy` — a key binding (F5) for a boss test,
   and §4.2's wave 12 boss wave. It needs `SetAreaLevel()` and room: its default
   alcove offsets are +/-1700 cm and galleries +/-1900 cm from its own location.
   Bind `OnBossDefeated` for the encounter-end.
5. **Wave-mode budget (§4.2).** The budget solver (Skitter 1 / Lattice 3 /
   Warden 6 / +4 per elite modifier), rest waves every 6, loot only on rest and
   boss waves. None of it exists; wave mode still spawns a flat count.
6. **Playtest report.** TTK is bucketed melee/ranged/elite. There is no bucket
   for boss or for modifier-bearing enemies, so a Champion's kill time pollutes
   the elite average. `Playtest/` is not this lane.
