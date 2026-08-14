# XP and Pacing

Domain: the experience curve to the level 50 hard stop, intended solo hours,
act breakpoints, XP sources, catch-up mechanics, the ~15 world-content Core
Points, the vertical slice's compressed level-10 curve, and the enemy-level /
item-level relationship.

**Scope:** post-slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

> **O29 INVALIDATES §8's ITEM-LEVEL ARITHMETIC. Read §8.1 before using any tier
> number in this document.** O29 runs item level to **120**, past the character
> cap and past the area-level ceiling, and widens the affix ladder from
> T8..T-1 to **T12..T-1**. §8's formula clamps item level at **50** and its
> worked example maps ilvl 30 to T5 and level 50 to "opening T1". Both are now
> wrong, and the ladder is built — this is not a pending change. **The
> zone-anchored model itself (O6) survives intact; only the ceiling and the
> ilvl→tier mapping moved.** Full correction in §8.1.

**Authority.** `Docs/Design/Decisions.md` is law and supersedes anything here.
This pass propagates O4 (§10.1), O6 (§8), O7 (§7), O8 (Frontier naming), and
O9 (§5.1 Rank vocabulary), O18 (§3 TTK/TTD seed targets), and O23 (§5.1
Veteran flag). No new numeric values were authored — O2 freeze. The O18 figures
are recorded verbatim as an owner ruling, not authored here.

**Campaign consumer.** `Docs/Design/Campaign-And-Story.md` (reconciled against
O28) consumes this document's act bands, the §7 Core Point list (CANON [O7]) and
the §8 area-level rule to author the story mission list. It authors no curve
value and changes nothing here; where it needed a reading rather than a change,
it is labelled RECONCILED there. Two such readings are worth knowing about while
reading §7: entry #2 (First Forge interaction) is read as first *respec*, and
entry #14 (the Survivor) is read as *bring them to an Anchor* at 44, with the
first meeting happening in early Act III per Master §13.5. Neither reading moves
a row.

This document resolves four items marked OPEN in the master sheet:

- Progression 7.11 — "Experience curve shape — flat, accelerating, or plateauing near 30"
- Progression 7.11 — "Intended hours to level 50 solo"
- Progression 7.11 — "Exact world-content sources for the ~15 non-level Core Points"
- Loot 4.9 / Scaling 6.5 — "Item level source — enemy level, zone, or both"

Everything here obeys the locked constraints: cap 50 hard stop, no post-cap
power track, gear is the entire endgame, Class Points 1-30, Core Points 1-50
plus ~15 from world content, act breaks at 15/30/50, solo as the primary
balance target.

---

## 1. Product intent for the curve

The curve has one job: make the *mechanical* arc and the *narrative* arc land
on the same beat, three times, without the player ever feeling they are
grinding to reach a story gate.

Three properties follow from that:

1. **Accelerating, not plateauing.** Per-level cost rises monotonically. A
   plateau near 30 would be the classic "the game slows down right as your
   class finishes" failure, and it collides with the Act II→III transition,
   which is the single most important narrative turn in the campaign.
2. **Act-scoped multipliers, not a single global exponent.** The three acts
   have different content densities and different player capability. A single
   exponent tuned for Act III makes Act I crawl; tuned for Act I it makes Act
   III evaporate.
3. **One deliberate discontinuity — the seam relief.** Levels 28-31 are
   discounted. This is the only non-monotonic thing in the design and it is
   intentional; see §4.

### Curve shape — LOCKED RECOMMENDATION

**Accelerating power curve with per-act multipliers and a seam relief.**

```
F(L)        = 300 * L^1.6 + 900 * L          (shape function)
ActMult(L)  = 0.80   for L in  1..15         (Act I)
              1.00   for L in 16..30         (Act II)
              1.30   for L in 31..49         (Act III)
SeamRelief  = 0.86   for L in 28..31         (applied on top of ActMult)

XPToNext(L) = round_to_50( F(L) * ActMult(L) * SeamRelief(L) )
```

`L` is the level the player is currently on; `XPToNext(49)` is the last entry.
There is no `XPToNext(50)` — 50 is a hard stop and the XP bar should be
removed from the HUD entirely at cap rather than filling forever. See §8.

**Implementation note.** Per master sheet 7.9, the cap and curve live in a
Data Asset. C++ must not hardcode 50, 30, or 65. Author the table below as a
baked `UCurveTable` / `TArray<int64>` rather than evaluating `pow()` at
runtime, so designers can hand-edit individual levels without changing the
formula.

---

## 2. The XP table

`Kill-equiv` is the number of *level-appropriate Standard enemy kills* the
level costs, using the kill values in §5. `Est. min` is the projected solo
minutes at the throughput model in §3. Both are derived, not authored.

| Lvl | XP to next | Cumulative | Kill-equiv | Est. min |
|---|---|---|---|---|
| 1 | 950 | 950 | 68 | 9 |
| 2 | 2,150 | 3,100 | 119 | 16 |
| 3 | 3,550 | 6,650 | 161 | 21 |
| 4 | 5,100 | 11,750 | 196 | 24 |
| 5 | 6,750 | 18,500 | 225 | 27 |
| 6 | 8,550 | 27,050 | 251 | 29 |
| 7 | 10,450 | 37,500 | 275 | 31 |
| 8 | 12,450 | 49,950 | 296 | 33 |
| 9 | 14,550 | 64,500 | 316 | 34 |
| 10 | 16,750 | 81,250 | 335 | 35 |
| 11 | 19,050 | 100,300 | 353 | 36 |
| 12 | 21,450 | 121,750 | 370 | 37 |
| 13 | 23,900 | 145,650 | 385 | 37 |
| 14 | 26,450 | 172,100 | 401 | 38 |
| **15** | **29,100** | **201,200** | 416 | 38 |
| 16 | 39,750 | 240,950 | 537 | 48 |
| 17 | 43,200 | 284,150 | 554 | 48 |
| 18 | 46,800 | 330,950 | 571 | 49 |
| 19 | 50,450 | 381,400 | 587 | 49 |
| 20 | 54,200 | 435,600 | 602 | 49 |
| 21 | 58,050 | 493,650 | 618 | 50 |
| 22 | 61,950 | 555,600 | 632 | 50 |
| 23 | 66,000 | 621,600 | 647 | 50 |
| 24 | 70,050 | 691,650 | 661 | 50 |
| 25 | 74,250 | 765,900 | 675 | 50 |
| 26 | 78,500 | 844,400 | 689 | 50 |
| 27 | 82,800 | 927,200 | 702 | 50 |
| 28 | 75,000 | 1,002,200 | 615 | 43 |
| 29 | 78,850 | 1,081,050 | 626 | 43 |
| **30** | **82,800** | **1,163,850** | 637 | 43 |
| 31 | 112,800 | 1,276,650 | 842 | 56 |
| 32 | 137,300 | 1,413,950 | 995 | 65 |
| 33 | 143,500 | 1,557,450 | 1,011 | 65 |
| 34 | 149,800 | 1,707,250 | 1,026 | 65 |
| 35 | 156,200 | 1,863,450 | 1,041 | 65 |
| 36 | 162,650 | 2,026,100 | 1,056 | 65 |
| 37 | 169,250 | 2,195,350 | 1,071 | 64 |
| 38 | 175,900 | 2,371,250 | 1,086 | 64 |
| 39 | 182,650 | 2,553,900 | 1,100 | 64 |
| 40 | 189,500 | 2,743,400 | 1,115 | 64 |
| 41 | 196,400 | 2,939,800 | 1,129 | 64 |
| 42 | 203,400 | 3,143,200 | 1,143 | 64 |
| 43 | 210,500 | 3,353,700 | 1,157 | 64 |
| 44 | 217,650 | 3,571,350 | 1,170 | 63 |
| 45 | 224,900 | 3,796,250 | 1,184 | 63 |
| 46 | 232,250 | 4,028,500 | 1,197 | 63 |
| 47 | 239,650 | 4,268,150 | 1,210 | 63 |
| 48 | 247,150 | 4,515,300 | 1,224 | 63 |
| 49 | 254,750 | **4,770,050** | 1,237 | 63 |
| 50 | — (hard stop) | — | — | — |

**Total XP 1→50: 4,770,050.**

### Prologue override — EXTENDS

Levels 1→3 read as slow above (9/16/21 minutes) because the throughput model
assumes an equipped, mobile player and level 1 has neither. The tutorial rift
should hand out a scripted XP bolus of **3,000 XP** across its scripted beats,
which puts the player at level 3 on exit in roughly 6-8 minutes of play. This
is a content grant, not a curve change — do not flatten the first three rows
of the table to achieve it, because the same rows are reused by the catch-up
system in §7 and by returning alt characters.

---

## 3. Intended solo hours — LOCKED RECOMMENDATION

**~40 hours, solo, campaign-focused, to level 50.**

| Act | Levels | XP in act | Target hours | Share |
|---|---|---|---|---|
| I | 1 → 15 | 172,100 | **6.8** | 17% |
| II | 15 → 30 | 908,950 | **12.0** | 30% |
| III | 30 → 50 | 3,689,000 | **20.8** | 53% |
| **Total** | 1 → 50 | 4,770,050 | **39.6** | 100% |

### Why 40

- Short enough that a second character to 50 is a plausible thing to do, which
  matters a great deal when class selection is permanent. Nobody rolls a
  second Breaker to see what Tank feels like if it costs 90 hours.
- Long enough that the Core Tree decision at 31+ is made by a player who has
  actually learned the combat systems, which is the stated reason the
  schedules diverge (master sheet 7.3).
- Level 50 is a hard stop and gear is the entire endgame. The campaign is the
  *tutorial for the endgame*, not the product. A 40-hour tutorial is already
  generous. Erring long here directly delays the loot chase that carries the
  whole game.

### Throughput model behind `Est. min`

Time estimates are not authored; they fall out of an explicit model that
should be replaced with telemetry after the gym feedback pass.

```
KillValueUnit B(L) = 10 + 4*L        -- XP of one level-appropriate Standard kill
SoloThroughput(L)  = 7 + 0.26*L      -- Standard-kill-equivalents cleared per minute
```

Throughput ramps with level because player capability compounds: kit
completeness, movement mastery, gear affixes, and denser enemy placement in
later zones all push it. At level 1 the player clears ~7 equivalents/minute;
at level 50, ~20. A representative Act III rift run (~9 min) is roughly 55
Standard + 14 Veteran + 4 Champion (pack-leader band) + 1 Champion (mini-boss
band) + 1 Boss + completion bonus,
which lands inside this model.

**This model is the single largest source of error in this document.** It is a
placeholder until a real time-to-kill exists, per the master sheet's standing
warning on placeholder values. Re-derive it from Playtest Gym telemetry and
then re-solve the act multipliers to preserve the 7 / 12 / 21 split. The
*shape* is the design commitment; the *multipliers* are the tuning knob.

### TTK / TTD seed targets — DESIGNER INPUTS [O18]

O18 supplies the seed targets the wave-mode instrument measures *against*.
These are designer inputs, not measurements and not authored values: the
instrument reports **divergence** from them, it does not report truth, and a
divergence is a signal to re-examine either the chassis or the target.

| Target | Seed value [O18] |
|---|---|
| Trash TTK | a little under 1 second, scaling exponentially with enemy difficulty |
| Rare / elite TTK | ~3 seconds |
| Boss TTK | 20–45 seconds, unless a special enemy |
| TTD, no resources/sustain | 4–5 seconds |
| TTD, resources and sustain invested | substantially higher |

The stat chassis solves backwards from these (see `Encounter-Design.md` §1.1).
No value in this document is changed to fit them — O2's freeze still holds, and
the throughput model above remains the placeholder it was. When wave mode
reports, the seed targets and the throughput model get reconciled together.

---

## 4. Act breakpoints

The breakpoints are dictated, not chosen. Class Points stop at 30; Core Points
run to 50; the acts are built on that (master sheet 8.1).

### Level 15 — Act I → Act II

**Mechanical meaning.** First branch commitment is complete. The player has
~15 Class Points and 15 Core Points; enough to have picked a branch and
started a constellation, not enough to have finished either.

**Curve treatment.** Act II costs jump 37% at the boundary (29,100 → 39,750).
This is deliberate and should be *felt*. It marks the moment the game stops
being a tutorial. The player has just been gated through The Breach and the
world got bigger; the cost stepping up reads as scope, not as a wall.

**Pacing guardrail.** The player must reach 15 through main-path content alone
with no optional detours. Act gates that require optional content are the
fastest way to make an ARPG feel like a chore. Optional content in Act I
should push the player to 16-17 by the gate instead, which is fine — a level
or two of headroom entering a new act is pleasant.

### Level 30 — Act II → Act III

**Mechanical meaning.** Class Points are exhausted. The player's class is
finished and will never change again. This is the largest single emotional
beat in the progression system and it coincides with the Act II turn — the
first Altered in a uniform.

**Curve treatment — the seam relief.** Levels 28, 29, 30, and 31 are
discounted by 14%. Reasoning:

- Level 30 is the last level that grants a Class Point. It must not be the
  most expensive level in the act, or the player's final class decision
  arrives at the end of the longest grind in Act II.
- Level 31 is the first Act III level. Without relief it would cost
  147,700 — a 78% jump from level 30 — landing exactly where the game also
  changes zone type, enemy family, and point currency. Four simultaneous
  changes is one too many.
- The relief tapers off at 32, where the Act III multiplier applies in full
  and the curve resumes its normal shape.

Net effect: the player crosses the single most loaded transition in the game
slightly faster than the raw curve would allow, and then Act III's real cost
arrives one level later, when they are already inside the new content.

### Level 50 — hard stop

**Mechanical meaning.** Core Points from levels are exhausted (50 of them,
plus ~15 from world content = ~65, per 7.2). The XP bar is removed. All
further power comes from gear.

**Curve treatment.** Level 49→50 must be the largest single level in the game
(254,750, ~63 minutes) and it must be *visibly* so, because it is the last
one. Do not soften it. It is the only place in the design where a wall is the
correct feeling: the player should arrive at 50 having earned the stop.

**Forbidden.** No XP accumulation past 50, no hidden overflow, no paragon
substitute, no "mastery" bar. Master sheet 7.1 is unambiguous and 9.1 states
the bet explicitly: the T-1 tier and the Anomalous slot carry the whole
endgame. Any post-cap trickle competes with that and worsens the
multiplicative-stacking risk.

**What replaces the bar.** At 50, the XP bar's screen real estate should be
given to a **build-completion readout** — equipped Aberrant count (n/3),
Anomalous (n/1), and count of T0+ affixes on equipped gear. This converts the
player's progression attention from levels to gear at the exact moment the
design intends, and costs no new systems.

---

## 5. XP sources

### 5.1 Kill XP by enemy Rank — RANK VOCABULARY [O9]

All kill XP derives from one unit so a single knob retunes the whole game.

```
B(L) = 10 + 4*L        -- L is the ENEMY's level, not the player's
```

**Taxonomy ownership.** O9 fixes the enemy taxonomy at three fields:
**Archetype** (behavior), **Rank** (Standard / Veteran / Champion / Boss), and
**Modifiers** (0–3). **Rank is owned by this document** — it is the XP and
reward axis. **Archetype and Modifiers are owned by `Encounter-Design.md`.**
**Modifier count drives Rank:** 0 modifiers = Standard, 1 = Veteran, 2–3 =
Champion. Boss is authored, not modifier-derived.

| Rank | XP band | Multiplier | XP at enemy L15 | XP at enemy L30 | XP at enemy L49 | Notes |
|---|---|---|---|---|---|---|
| Standard | Trash | 0.35 | 25 | 46 | 72 | Swarm filler, dies to one burst. Use sparingly — trash exists for feel, not reward |
| **Standard** | **Standard** | **1.00** | **70** | **130** | **206** | The unit. Three normal enemies of the slice sit here. 0 modifiers |
| Veteran | Veteran | 3.0 | 210 | 390 | 618 | Exactly 1 modifier applied to a Standard. **FLAGGED [O23]** — see below |
| Champion | Pack leader | 8.0 | 560 | 1,040 | 1,648 | Named pack leader, 2 modifiers |
| Champion | Mini-boss | 25.0 | 1,750 | 3,250 | 5,150 | 3 modifiers, guaranteed rare drop |
| Boss | Rift boss | 120.0 | 8,400 | 15,600 | 24,720 | End of an instanced rift |
| Boss | Act boss | 400.0 | 28,000 | 52,000 | 82,400 | One per act. ~one-third of a level |

The four Ranks are coarser than the seven XP bands; the bands are a payout
curve *inside* a Rank, not a fifth and sixth Rank. The former tier names
"Trash" and "Elite" are retired as taxonomy terms per O9 — "Trash" survives
only as the label of the sub-Standard XP band, and everything previously
called "Elite" is a Champion.

**FLAG — band-to-Rank granularity.** Two XP bands sit under Champion (8.0 and
25.0) and two under Boss (120.0 and 400.0). Which band a given 2-modifier or
3-modifier encounter pays is an encounter-authoring choice, not a taxonomy
one; if the owner wants one payout per Rank, bands must be collapsed and that
is a value decision this pass does not make.

**FLAGGED [O23]: anchored to the deleted 3x-health chassis; over-rewards ~1/3
vs the canonical 2.0x chassis; frozen under O2; on wave mode's report list.**
The multiplier is left at 3.0 deliberately — this pass records the flag and
does not recost it.

**Veteran anchor — reconciled to `Encounter-Design.md` §1.1.** The gym's
existing `ConfigureElite` maps to **Veteran** (one modifier). Its stat chassis
is the Encounter §1.1 elite chassis, not the gym's current values: **1.25x
scale, 2.0x health (+0.35x per modifier beyond the first), 1.5x damage**,
loot floor Exceptional. The gym's shipped `1.5x / 3.0x / 2.0x` is superseded.
Because health scales with modifier count, the same chassis carries a Champion
at 2 or 3 modifiers without a second stat block — which is exactly why
modifier count is allowed to drive Rank.

### 5.2 Level-difference falloff

Prevents both low-level farming and over-level trivializing.

| Enemy level − player level | XP multiplier |
|---|---|
| +5 or more | 1.25 (capped) |
| +3 to +4 | 1.15 |
| +2 to −2 | 1.00 |
| −3 | 0.80 |
| −4 | 0.55 |
| −5 | 0.30 |
| −6 | 0.12 |
| −7 or more | 0.02 (never exactly zero) |

Never zero: a player revisiting an early zone for a fragment or a Core Point
should not see "0 XP" floaters, which reads as a bug and feels punitive.

The +1.25 cap on over-level kills is deliberate. Uncapped over-level bonuses
create a "pull a level-45 pack at level 35" degenerate strategy that
completely bypasses the intended act pacing.

### 5.3 Rift completion

Rift completion, not kills, should be the dominant XP source. Kills alone
reward clearing every room; completion rewards *running the loop*, which is
the loop the endgame is built on.

| Component | Value | Notes |
|---|---|---|
| Rift closure | 45 × B(RiftLevel) | The base payout |
| First-clear bonus | +100% | Once per rift per character |
| Full clear (all packs) | +25% | Optional thoroughness reward |
| No-death clear | +15% | Small; must not make deaths feel expensive |
| Under par time | +20% | Par is generous; this is the movement reward |

A level-appropriate rift with a boss therefore pays roughly:
`45×B + 120×B (boss) + ~180×B (kills en route)` ≈ **345 × B(L)**, or about
45,000 XP at level 30. That is roughly 55% of a level per run in mid Act II,
which is the right texture: visible progress, not a level per run.

**Target: ~60% of all XP earned comes from rift completion and rift bosses.**
If telemetry shows open-world kill XP exceeding 40%, the completion payouts
are too low and players are farming rather than running content.

### 5.4 Discovery and world XP

Discovery XP exists to make the erased Earths worth walking through and to
give movement mastery a non-combat payoff. It is deliberately a small share of
the total (target ≤8%) — it must never be an efficient farm.

| Source | Value | Repeatable |
|---|---|---|
| Zone region discovered | 20 × B(ZoneLevel) | No |
| Landmark / vista reached | 12 × B(ZoneLevel) | No |
| Traversal challenge completed | 30 × B(ZoneLevel) | No |
| Rior fragment recovered | 200 × B(ZoneLevel) | No |
| Lore terminal / field note | 6 × B(ZoneLevel) | No |
| Anchor NPC first conversation | 10 × B(ZoneLevel) | No |
| World event participation | 40 × B(EventLevel) | Yes, with a diminishing-returns tail |

**Traversal challenges** deserve a specific note. They are the only place in
the game where movement mastery is rewarded on its own terms, and the
Movement guardrails forbid making advanced movement mandatory for combat.
Optional, XP-bearing traversal routes resolve that tension cleanly: a
conventional player loses nothing they need, and a Kinetic player gets a
reason to exist outside of fights. Every traversal challenge must be
completable with base kit only (no air jump), because air jump is a Kinesis
tree grant and gating world XP behind a constellation violates layer
ownership.

### 5.5 Experience Gain %

The `Experience Gain %` affix (Helmet/Necklace, 3% → 14% → 25%) already exists
in the master sheet. Per the locked aggregation rule it is an **Increased**
bucket: all sources sum additively and apply once. Two Necklace-and-Helmet
T-1 rolls give +50%, not ×1.25². Cap the total additive bucket at **+100%** so
a lucky drop cannot halve the campaign.

---

## 6. Rested and catch-up mechanics — RECOMMENDATION

### Rested XP — NO

Do not ship a rested XP system.

Rested XP solves a problem this game does not have. It exists to make
intermittent play feel efficient in games with 200-hour level curves and
subscription retention pressure. At 40 hours to cap with a hard stop and no
post-cap track, rested XP would compress the campaign for exactly the players
who play it most casually, which is backwards — those are the players who most
need the levels to teach them the systems.

It also actively fights the pacing design. A returning player with a full
rested bar blows through the level 28-31 seam without registering that their
class just finished.

### Alt catch-up — YES, account-wide, flat

Class selection is permanent per character. That is a locked decision, and it
means the game *will* be asking players to level a second Breaker. Making that
second run identical in length to the first is the design's biggest retention
risk.

**Veteran's Path.** Once any character on the account has reached 50:

| Character level range | XP multiplier |
|---|---|
| 1 → 15 | ×2.00 |
| 16 → 30 | ×1.60 |
| 31 → 45 | ×1.35 |
| 46 → 50 | ×1.00 |

Result: a second character reaches 50 in roughly **26 hours** instead of 40,
with Act I compressed hardest (the part the player has definitively already
seen) and the final five levels uncompressed (so the arrival at cap still
feels earned).

Properties that make this safe:
- Flat multiplier, not a stored pool. Nothing to bank, nothing to waste.
- Does not stack with itself; it stacks additively with `Experience Gain %`
  into the same Increased bucket, and the +100% cap does *not* apply to
  Veteran's Path (it is a separate, explicit multiplier applied after).
  **EXTENDS** — this is the one sanctioned exception to affix-bucket
  uniformity and it exists outside the item system entirely.
- Account-wide and permanent. No timers, no weekly resets, no currency.

### Death penalty — NO XP LOSS

No XP loss on death, at any level. It punishes exactly the experimentation the
permanent class choice already discourages. Death costs time-to-return and
whatever rift completion bonuses were forfeited; that is sufficient.

### Party XP

Solo is the primary balance target, so party XP must be *neutral*, not
generous, or party play becomes the fast lane and solo becomes the punishment.

- Each party member receives full individual kill XP for anything they were
  present for (within ~60m and within 15s of the kill).
- Rift completion XP is **not** divided; each member receives the full amount.
- **No party size bonus.** Master sheet 11.2 requires enemy count and role
  pressure to scale with party size, not just health — so a five-player group
  is already killing more things per minute. That is their efficiency
  advantage and it is enough. Adding a headcount multiplier on top makes solo
  the objectively worst way to level, contradicting 11.1.

---

## 7. The ~15 world-content Core Points — CANON [O7]

**CANON [O7].** This list is the ratified source of the 15 world-content Core
Points. It supersedes any other enumeration in Docs/. Two changes were made in
ratifying it:

1. The three cumulative "close N local rifts (lifetime)" counters are replaced
   by **rift-archetype first-clears**, per `Game-Modes.md` §2 and §3.4 (the
   eight archetypes: Clear, Hold, Sever, Escort, Carry, Collapse, Hunt,
   Silence). First-clear is one-time per archetype, not per instance, so it
   satisfies design rule 1 below in a way a lifetime counter never did.
2. **Nothing is endgame-gated.** The old overflow entry (#16, "close 75 rifts
   lifetime", explicitly completable after level 50) is cut. The list is
   exactly 15 and every entry is reachable inside the campaign.

**FLAG — archetype point budget.** `Game-Modes.md` §2 budgets the
archetype first-clears at **8 points** (one per archetype). This list allots
them **2 slots**. Both cannot be true at 15 total. This document's slot count
is canon per O7; the reconciliation — 8 one-point grants versus 2 grouped
grants — is an owner choice and is **not resolved here**.

Master sheet 7.2 budgets ~15 Core Points from world content, on top of the 50
from levels, for ~65 total — the number validated against "two constellations
fully developed plus a third partially" (7.4).

Design rules applied to this list:

1. **Every one is one-time and permanent.** No repeatable Core Point sources.
   A repeatable source is an infinite power track and 7.1 forbids that.
2. **They are spread across all three acts**, so the Core Tree is never
   entirely gated behind level pace.
3. **None are missable.** A permanently missed Core Point on a character with
   a permanent class is unrecoverable and unacceptable.
4. **None require party content.** Solo is the primary balance target.
5. **None require optional traversal mastery.** Movement tax (risk #7).

| # | Source | Act | Level ~ | Type | Rationale |
|---|---|---|---|---|---|
| 1 | Complete the tutorial rift | I | 3 | Main path | Teaches that the Core Tree exists at all, before level 5 |
| 2 | First Forge interaction (Kess) | I | 5 | Main path | Front-loads the respec NPC the player will use constantly |
| 3 | First-clear of each rift archetype available in Act I | I | 8-12 | Archetype | Rewards the core loop directly and teaches the objective vocabulary; progress is visible per archetype |
| 4 | Act I boss | I | 15 | Main path | Act completion |
| 5 | Rior fragment #1 reconstructed | I/II | 15 | Fragment | Fragments unlock real capability (1.7); one is Core |
| 6 | The Breach — first entry | II | 16 | Main path | Marks the act transition mechanically |
| 7 | First-clear of the remaining rift archetypes | II | 20-24 | Archetype | Completes the archetype set; the later archetypes are the ones Act II introduces |
| 8 | Rior fragment #2 reconstructed | II | 22 | Fragment | Paces Vosk's thread |
| 9 | Defeat the Altered commander | II | 26 | Main path | The slice boss; the "rifts are directed" beat |
| 10 | Act II boss | II | 30 | Main path | Lands on the Class Point exhaustion beat — the player gets a *Core* Point at the exact level their class stops growing |
| 11 | Erased Earth 1 — zone completion | III | 34 | Zone | One per Act III zone |
| 12 | Erased Earth 2 — zone completion | III | 40 | Zone | The zone that proves Rior right |
| 13 | Rior fragment #3 reconstructed | III | 42 | Fragment | Third and last Core-bearing fragment |
| 14 | Meet the Survivor / bring them to an Anchor | III | 44 | Main path | Ties the emotional load-bearing NPC to a mechanical reward |
| 15 | Erased Earth 3 — the Earth where Rior lost | III | 48 | Zone | Act III climax location |

**Exactly fifteen, none endgame-gated [O7].** The former #16 overflow entry is
cut. The "~" in "~15" now describes uncertainty in the budget, not a
sixteenth entry a capped player is still chasing — O7 requires every world
Core Point to be obtainable inside the campaign. Every remaining entry is
load-bearing on a story beat or on the rift-archetype set.

**Distribution: 5 in Act I, 5 in Act II, 5 in Act III.** The list stays
act-distributed by rule 2 above, and §10.1 gives a second, independent reason
the distribution cannot be back-loaded.

**Validation against 7.4.** 50 + 15 = 65. If constellation node costs change,
re-run the "two full plus one partial" check. If that check fails, move the
node costs, not this list — this list is now pinned to story beats and moving
it desynchronizes the narrative and mechanical arcs, which 8.1 explicitly
protects.

---

## 8. Enemy level and item level — RESOLVING THE OPEN ITEM

Master sheet 4.9 and 6.5 both leave "item level source — enemy level, zone, or
both" open. `Docs/Item-Foundation.md` records that the gym currently uses enemy
level (`ABreakerEnemy::EnemyLevel`) as an implementation expedient, with zone
sourcing noted as still open.

### The three candidates

**Enemy level only.** Item level = the level of the thing that dropped it.
- Pro: intuitive, already implemented, naturally rewards fighting harder things.
- Con: creates a farming incentive to seek out the highest-level enemy in a
  zone and ignore everything else. Chest and world drops have no source level
  at all and need a fallback anyway. Champion/Boss item level becomes a balance
  lever by accident rather than by design.

**Zone level only.** Item level = the zone's authored level.
- Pro: perfectly predictable for the player ("this zone drops ilvl 34"),
  trivially controllable for designers, and every drop source — enemy, chest,
  container, quest reward — resolves identically.
- Con: makes the boss's drop no better than a trash mob's, which removes the
  reason to fight anything difficult. Kills the entire Veteran/Champion fantasy.

**Hybrid.** Zone establishes the floor; enemy tier adds a bounded bonus.

### RATIFIED [O6] — HYBRID, zone-anchored with a bounded enemy-tier bonus

**RATIFIED [O6].** This is no longer a recommendation. O6 ratifies the hybrid:
`ZoneLevel` lives on the **rift/zone Data Asset**, the tier bonus and ±1
variance apply as written below, and the **enemy-level fallback is sanctioned**
so the gym keeps working before zones exist. The open item at master sheet 4.9
/ 6.5 is closed.

```
ItemLevel = clamp( ZoneLevel + TierBonus + VarianceRoll, 1, 50 )
```

| Enemy Rank / band [O9] | TierBonus |
|---|---|
| Standard (trash and standard bands) | 0 |
| Veteran | +1 |
| Champion — pack-leader band | +2 |
| Champion — mini-boss band | +3 |
| Boss — rift boss | +4 |
| Boss — act boss | +5 |

```
VarianceRoll: -1 (25%), 0 (50%), +1 (25%)
Non-enemy sources (chests, containers, quest rewards): TierBonus = 0
Named/guaranteed chests: TierBonus = +2
```

**Why hybrid wins.**

- **Zone as the anchor makes the system authorable.** A designer sets one
  integer per zone and knows exactly what tier band that zone can produce. The
  ilvl → affix tier mapping (`BestTierForItemLevel`, one tier per ~7 levels,
  level 50 opening T1) is only meaningful if ilvl is predictable, and enemy
  level alone is not — it varies per spawn, per pack, per scaling rule.
- **The tier bonus preserves the reason to fight hard things**, which pure
  zone level destroys. +5 across the whole range is roughly *two thirds of one
  affix tier band* — enough to matter at the margins, small enough that it
  never lets an early-zone boss produce late-game gear.
- **The bonus is bounded and small on purpose.** The big reward for killing a
  boss should be **rarity and drop count**, not item level. Rarity gates affix
  *count*; item level gates affix *tier* (4.2). Bosses should push the rarity
  knob hard (the gym Veteran's "never below Exceptional" loot floor is the
  right instinct)
  and the item-level knob barely at all. This keeps the two knobs doing
  genuinely different jobs, which is the whole point of separating them.
- **It resolves the "do enemies scale to player level in open zones" open
  item favourably** (6.7). With zone-anchored ilvl, the answer can be "enemies
  scale within a narrow band around the zone level" without item level
  drifting, because ilvl no longer reads enemy level directly.
- **±1 variance** stops every drop in a zone from being an identical ilvl,
  which makes the ilvl number on the tooltip meaningful to read.

**Worked example.** Zone level 30 (mid Act II):

| Source | ilvl range | Best natural tier |
|---|---|---|
| Standard enemy | 29-31 | T5 |
| Veteran | 30-32 | T5 |
| Champion (pack leader) | 31-33 | T4 |
| Champion (mini-boss) | 32-34 | T4 |
| Boss (rift boss) | 33-35 | T4 |
| World chest | 29-31 | T5 |

The boss's advantage over trash is one tier band plus a far better rarity roll.
That is the correct ratio.

**Endgame note [CORRECTED — O29, O36; full derivation in §8.1].** This
paragraph originally claimed `ZoneLevel` is pinned at 50 for all endgame
content and that "item level stops being a progression axis at cap, by
design" — the exact inverse of the ruled model, corrected in place here.
**Item level running to 120 — past the character cap of 50 — IS the endgame
progression axis** [O29]: cap is where it starts being the only progression
axis, not where it stops mattering. Endgame content is CONTENT-scaled, never
player-scaled [O27], and carries **area level up to a ceiling of 100** [O27,
O36] — not "everything is level 50." Item levels 101–120 are sourced from the
**endgame tier bonus**: Frontier tiers extend O6's `TierBonus` past its
campaign +0..+5 span [O36], so pushing Frontier tiers pushes the ladder;
**the Forge is the secondary source**. Rarity, affix tier depth (now
T12..T-1 — see §8.1), and the Anomalous chase remain the shape of the endgame
loop; that much of the original note held.

**RESOLVED [O6] — was CONFLICT with `Docs/Item-Foundation.md` and
`Game-Modes.md` §3.7.** `Item-Foundation.md` records item level source as
enemy level; Game-Modes §3.7 tiers rifts on its own terms. O6 settles the
model — hybrid, `ZoneLevel` on the rift/zone Data Asset, enemy-level fallback
— so there is nothing left to arbitrate at the design layer. **The
implementation home is the damage/loot pipeline docs, not this document nor
Game-Modes:** `Docs/Item-Foundation.md` (loot roll and `UBreakerLootLibrary`,
which needs a `ZoneLevel` input defaulting to the enemy's level when unset) and
`Docs/Combat-Foundation.md` (the pipeline the roll hangs off). Rift tiering in
Game-Modes §3.7 consumes `ZoneLevel`; it does not define it. This is a small
change now and an expensive one later, consistent with 4.8's warning.

### 8.1 CORRECTION [O29] — the ceiling is 120 and the tier mapping is two-slope

**Added 2026-08-14, read directly from
`Source/RiorsEdge/Items/BreakerAffixLibrary.{h,cpp}` and
`Source/RiorsEdge/Weapons/BreakerWeaponMath.h`. O29 is BUILT, not pending.**

**What O29 changed and this section did not.**

| Quantity | §8 as written | Shipped under O29 |
|---|---|---|
| Item level ceiling | `clamp(..., 1, 50)` | **120** (`MaxItemLevel`, `MaxSupportedItemLevel`, pinned equal by `RiorsEdge.Items.TierLadder`) |
| Affix tier ladder | T8..T-1 implied; "level 50 opening T1" | **T12..T-1** |
| ilvl → tier mapping | "one tier per ~7 levels" | **two slopes**: ~8.2 levels/tier from ilvl 1 (T12) to ilvl 50 (T6), then ~14 levels/tier from ilvl 50 (T6) to ilvl 120 (T1) |
| Tier at the character cap | T1 | **T6** — half the ladder, not a third |
| Drop item level at high area level | clamped to 50 | **unclamped**; `GetDropItemLevel` returns area level unchanged |

**The zone-anchored model is untouched.** `ZoneLevel` on the rift/zone Data
Asset, the Rank TierBonus table, the ±1 variance, the enemy-level fallback —
all of O6 stands exactly as ratified. **Only the clamp and the tier mapping
moved.** The formula reads:

```
ItemLevel = clamp( ZoneLevel + TierBonus + VarianceRoll, 1, 120 )
```

**The §8 worked example, recomputed.** Same zone level 30, same TierBonus
table, same variance — run through the shipped two-slope mapping instead of
the retired one-tier-per-7-levels rule. This is arithmetic from code, not a
re-authored value:

| Source | ilvl range | Best natural tier — §8 said | Best natural tier — **shipped** |
|---|---|---|---|
| Standard enemy | 29-31 | T5 | **T9** |
| Veteran | 30-32 | T5 | **T9** |
| Champion (pack leader) | 31-33 | T4 | **T9** |
| Champion (mini-boss) | 32-34 | T4 | **T9-T8** |
| Boss (rift boss) | 33-35 | T4 | **T9-T8** |
| World chest | 29-31 | T5 | **T9** |

Derivation, so the numbers are checkable rather than trusted:
`Steps = floor((ilvl - 1) * (12 - 6) / 49)`, `Tier = 12 - Steps`. The single
tier boundary anywhere in the 29-36 range sits at **ilvl 34**. So across the
entire TierBonus table — Standard through act boss, a spread of five item
levels plus variance — a zone-30 drop is T9 or T8, and usually T9.

**The conclusion §8 drew from that table is WEAKER than it was, and that is the
finding.** §8 argued "the boss's advantage over trash is one tier band plus a
far better rarity roll — that is the correct ratio." Under the twelve-tier
ladder the ±5 TierBonus spread mostly **does not cross a tier boundary at all**
at zone level 30, because the campaign slope is ~8.2 item levels per tier and
the whole bonus range is 5. **The tier bonus has become close to inert in the
campaign band.** Whether that is acceptable — §8's own argument was that the
bonus should be "bounded and small," and it is now smaller than bounded — is an
owner call. **No value is re-authored here (O2).**

**The endgame note in §8 was backwards; it has been corrected in place above
[O29, O36].** It originally read: *"In Rior's Frontier, `ZoneLevel` is pinned
at 50 for all content and the entire progression axis moves to rarity, affix
tier via crafting, and the T-1 / Anomalous chase. Item level stops being a
progression axis at cap, by design."*

**O29 rules the exact opposite.** Item level running to 120 — past the character
cap of 50 and past the area-level ceiling — **is** the endgame power source, and
is what makes the locked "all endgame character power comes from gear" rule
function rather than merely be stated. It is O29's answer to the 74x gap
recorded at the end of `Power-Curve.md`. Item level does not stop being a
progression axis at cap; **cap is where it starts being the only one.**

**Two consequences worth stating plainly:**

1. **§4's "Level 50 — hard stop" section survives, and is strengthened.** No XP
   past 50, no paragon, no mastery bar — O29 explicitly **rejects** a
   paragon-style post-cap tree as colliding with that rule and as pure
   accumulation against O27. What §4 could not say, and now can, is *what*
   replaces the bar: seventy more item levels and six more affix tiers.
2. **§4's build-completion readout recommendation needs one more field.** It
   proposes Aberrant count, Anomalous count, and T0+ affix count. Under a
   T12..T-1 ladder, **T0+ is no longer the meaningful threshold** — the player
   spends the whole campaign between T12 and T6 and never sees a T0. The
   readout wants an average or best equipped tier, or an item-level average.
   **Not authored here**; it is a UI decision and belongs to `UI-UX-Spec.md`.

**RESOLVED [O36] — was "what is NOT resolved, and needs the owner."** §8
originally authored `ZoneLevel` capped at 50 for endgame content, against
O27's **area level** (authored 1-100 on the content) and O29's item level
(120) — three ceilings with no stated relationship. O36 closes the gap:
**area level stays capped at 100** (not 50) for endgame content, and the
remaining ilvl 101–120 range does not come from area level at all — it is
**sourced from the endgame tier bonus**, Frontier tiers extending O6's
`TierBonus` past its campaign +0..+5 span, with **the Forge as the secondary
source**. `GetDropItemLevel` returning area level unchanged is therefore the
area-level component of a roll, not the whole of it; the tier-bonus component
carries a drop the rest of the way to 120. One narrow question survives: O36
does not say whether `ZoneLevel` as an authored field is retired in favour of
O27's area level, or continues alongside it under a different name — a naming
question, not a progression-design one, and not resolved here.

### Enemy level scaling curve — EXTENDS

6.7 leaves "enemy level scaling curve" open. It is out of this document's
domain to set enemy stat values, but the *level assignment* rule is in scope:

```
ZoneLevel is authored per zone and never scales to the player.
EnemyLevel = ZoneLevel + PackModifier, where PackModifier is in [-1, +3].
```

Enemies never scale to player level in campaign zones. Combined with the level
difference falloff in §5.2, this means an over-levelled player gets reduced XP
but keeps a real power fantasy, and an under-levelled player gets a bonus and
a real threat. **In endgame Frontier content this still holds — content is
CONTENT-scaled, never player-scaled [O27].** What changes is the ceiling:
Frontier area level climbs to **100**, not "everything is level 50" [O27,
O36], and difficulty is expressed through area level and modifiers together,
not through `CharacterLevel`, which stays capped at 50.

**Level-gate hygiene [O40(b)].** Nothing above should be read as licensing a
new `CharacterLevel` check to gate Frontier/endgame access or any other
content. **Until an XP loop exists, no system may gate on `CharacterLevel`**
[O40(b)] — this is literally the bug class that made Swift's third jump's
level gate unreachable. The day a loop lands, `CharacterLevel` gates re-open
by ruling.

---

## 9. The vertical slice — compressed level-10 curve

Master sheet 7.9's vertical slice override: the slice ships ~15 skill nodes,
so **SLICE CAP: 10, compressed curve**. Slice value only, not a balance
signal, and cheap because cap and curve live in a Data Asset.

### Slice curve

```
F_slice(L)      = 120 * L^1.35 + 240 * L
XPToNext(L)     = round_to_50( F_slice(L) )     for L in 1..9
```

| Lvl | XP to next | Cumulative | Grants |
|---|---|---|---|
| 1 | 350 | 350 | 1 Class Pt, 1 Core Pt |
| 2 | 800 | 1,150 | 1 Class Pt, 1 Core Pt |
| 3 | 1,250 | 2,400 | 1 Class Pt, 1 Core Pt |
| 4 | 1,750 | 4,150 | 1 Class Pt, 1 Core Pt |
| 5 | 2,250 | 6,400 | 1 Class Pt, 1 Core Pt |
| 6 | 2,800 | 9,200 | 1 Class Pt, 1 Core Pt |
| 7 | 3,350 | 12,550 | 1 Class Pt, 1 Core Pt |
| 8 | 3,900 | 16,450 | 1 Class Pt, 1 Core Pt |
| 9 | 4,500 | 20,950 | 1 Class Pt, 1 Core Pt |
| 10 | — (slice cap) | — | 1 Class Pt, 1 Core Pt |

**Total: 20,950 XP.** Plus 2 Core Points from slice world content (see below):
**10 Class Points and 12 Core Points at slice cap**, against ~15 nodes. That
ratio is intentional — the player must leave nodes unbought, so the slice
actually tests whether the choice is interesting.

### Slice XP sources

Slice enemy levels: Standard L4, Veteran L6, Boss L8. Using `B(L) = 10+4L`:

| Source | Level | Value | XP |
|---|---|---|---|
| Standard enemy (three archetypes) | 4 | ×1.0 | 26 |
| Veteran (`ConfigureElite`, 1 modifier) | 6 | ×3.0 | 102 |
| Champion, pack-leader band | 6 | ×8.0 | 272 |
| Boss — slice boss (Altered commander) | 8 | ×120 | 5,040 |
| Arena clear | 6 | 45 × B | 1,530 |
| First-clear bonus | — | +100% | +1,530 |
| Region discovered (×3) | 6 | 20 × B | 680 each |
| Traversal challenge (×2) | 6 | 30 × B | 1,020 each |

**Slice pacing target: 45-70 minutes to slice cap 10.**

A representative slice loop: clear the arena (~35 Standards, 4 Veterans, 1
Champion, 1 arena clear) ≈ 910 + 408 + 272 + 1,530 = **3,120 XP** in roughly
6-8 minutes. Seven such loops plus the boss, discovery, and traversal reaches
20,950. That is the right length for a slice: long enough to feel the
progression system, short enough to replay in an evening while tuning.

### Slice Core Points from world content

Two, both one-time, both mapping to entries in the §7 table so the slice tests
the real system:

- Complete the arena for the first time (maps to §7 #1, tutorial rift).
- Defeat the Altered commander (maps to §7 #9, and is already the recommended
  slice boss per 8.7).

### Slice acceptance criteria

The slice's job is to falsify the pacing model before it is applied to 50
levels. It passes if:

- [ ] Cap, curve, and per-level grants are read from a Data Asset. No `10`,
      no `50`, no XP value appears in C++.
- [ ] Reaching slice cap 10 from a fresh character takes 45-70 minutes for a
      player of median skill, measured, not estimated.
- [ ] The player reaches level 2 within the first 3 minutes of combat.
- [ ] No single level in the slice takes more than 2.5× the shortest level.
- [ ] At slice cap the player has 10 Class Points and 12 Core Points and
      strictly fewer points than nodes available.
- [ ] The XP bar is removed from the HUD on reaching slice cap; no XP
      accumulates past it, including internally.
- [ ] Rift/arena completion accounts for 45-65% of total XP earned. Measured
      via a telemetry counter, not asserted.
- [ ] Killing a level-4 normal at player level 9 awards non-zero XP
      (the −5 case: 0.30 × 26 = 8).
- [ ] Item level on drops is derived from an authored zone level plus tier
      bonus, not from `EnemyLevel` directly, and a zone level override in the
      gym visibly shifts the affix tiers that roll.
- [ ] Two Core Points are granted by world content, not by levels, and both
      persist across save/load.
- [ ] `Experience Gain %` from equipped gear resolves through the single
      additive Increased bucket. Two +14% rolls produce +28%, not ×1.14².
- [ ] Death awards no XP penalty and does not roll back the level.
- [ ] The XP curve Data Asset can be swapped for the full 50-level table
      without a code change, and the game boots with the 50-level table
      selected.

---

## 10. Acceptance criteria — full game

### 10.1 Halfway-viability criterion — TESTABLE [O4]

O4 rules that a build must **feel viable and playable by mid-campaign (~level
25)**, and that the 300–400 hour figure describes *optimization*, not
*viability*. That is a product claim; this is its test.

> **By level 25, a player must hold one Core keystone, one class keystone, and
> at least one equipped Aberrant.**

**Supporting arithmetic CHECK** (over values already written elsewhere; no new
values authored here):

```
Core Points from levels, 1→50 ...................... ~50
Core Points from world content (§7) ................ 15
Core Point total ................................... ~65
Keystone investment gate ........................... 18
Core Points held by a level-25 player .............. ~32
    = 25 from levels
    + 7 world points earned at or before ~L25
      (§7 entries #1,2,3,4,5,6,7 — the #8 fragment at L22
       makes it 8 if taken on schedule)
```

**CHECK PASSES.** 32 ≥ 18 with 14 points of headroom, so a level-25 player
reaches **one Core keystone comfortably** — not marginally, and not only on an
optimal route. The **class keystone comes out of the separate Class Point
budget** (Class Points 1–30; a level-25 player holds ~25 of them), so the two
keystones do not compete for the same currency. The Aberrant requirement is
satisfied by drops: O11 allows up to 3 equipped and the criterion asks for 1.

**Second independent reason the §7 list must stay act-distributed.** The first
reason is design rule 2 in §7 (the Core Tree must never be entirely gated
behind level pace). The second is this criterion: the ~32-point figure is only
reachable at level 25 because roughly half the world points land in Acts I–II.
Back-load the §7 list into Act III and the level-25 player holds ~27 points
instead — still over the gate, but the headroom that makes the keystone
*comfortable* rather than *forced* evaporates, and with it the O4 claim. Any
future reshuffle of §7 must re-run this check.

**Identity versus optimization — the O4 framing.**

| | Build **identity** — earned by ~level 25 | Build **optimization** — the 300–400 hours |
|---|---|---|
| Core Tree | First Core keystone | Second keystone; a third-constellation dip |
| Class Tree | First class keystone | — (Class Points exhaust at 30) |
| Gear | One equipped Aberrant | Affix tiers, T-1, the Anomalous slot, 2nd and 3rd Aberrant |

Identity is what the build *is* and it must be complete by mid-campaign.
Optimization is how far the same build can be pushed, and it is the entire
endgame. A player who cannot name their build at level 25 is a failure of this
criterion; a player whose level-25 build is still improving at hour 300 is the
design working.

**FLAG — engineering, not design.** Wiring this criterion into the wave-mode
report (so a gym run asserts keystone and Aberrant state at the level-25
equivalent) is a **Tier 1 engineering item** and is **not done in this pass**.
Until it is instrumented, the criterion is a design commitment, not a measured
gate.

### 10.2 Checklist

- [ ] By level 25, a player holds one Core keystone, one class keystone, and
      at least one equipped Aberrant (see §10.1). Measured in the wave-mode
      report once instrumented.
- [ ] `XPToNext` is a baked table in a Data Asset, not a runtime `pow()`.
- [ ] Total XP 1→50 equals 4,770,050 with the shipped table.
- [ ] Median solo time to 50, measured by telemetry, falls in 34-46 hours.
- [ ] Act split falls within ±20% of 17 / 30 / 53 percent.
- [ ] A player following only main-path content reaches 15 before the Act I
      gate and 30 before the Act II gate, with no optional content required.
- [ ] Level 49→50 is the largest single level in the game.
- [ ] No XP is stored, displayed, or accumulated after level 50.
- [ ] Rift completion + rift bosses account for 55-65% of lifetime XP.
- [ ] Discovery XP accounts for ≤8% of lifetime XP.
- [ ] All ~15 world Core Points are obtainable solo and none are missable.
- [ ] Sum of level Core Points and world Core Points is 65 (50 + the 15 of
      §7, which is now exactly 15 per O7) and still satisfies "two
      constellations full plus one partial."
- [ ] Veteran's Path reduces a second character's time to 50 to 22-30 hours.
- [ ] No rested XP system ships.
- [ ] Party members do not level faster per-hour than a solo player of equal
      skill in equivalent content.
- [ ] `ItemLevel` derives from zone level; no code path reads `EnemyLevel`
      into `ItemLevel` directly.
- [ ] The level-difference XP falloff never returns exactly zero.

---

## 11. Risks

1. **The throughput model is the weak joint.** Every hour figure in this
   document depends on `SoloThroughput(L) = 7 + 0.26*L`, which was authored
   before a real time-to-kill exists. If actual throughput is flat rather than
   ramping, Act III balloons from 21 hours to 30+. Re-derive from gym
   telemetry before committing content volume.
2. **Act III is 20 levels across three zones.** Master sheet 8.8 already flags
   this as a known, accepted problem. This document makes it concrete: 20.8
   hours across three zones is ~7 hours per zone, which is a lot of zone. The
   curve cannot fix a content-volume problem; either the zones get bigger, or
   repeatable rift content carries more of Act III than the act structure
   implies.
3. **Hybrid item level adds a `ZoneLevel` dependency** to the loot pipeline
   before zones exist. Mitigation is the enemy-level fallback described in §8,
   but the fallback must not become permanent by neglect.
4. **Veteran's Path is a second global XP multiplier** living outside the affix
   bucket system. It is small and explicit, but it is a precedent. Do not add
   a third.
5. **The seam relief is a hand-authored discontinuity.** If anyone later
   re-derives the table from the formula without the `SeamRelief` term, levels
   28-31 silently get 16% more expensive at the worst possible place. Comment
   it in the Data Asset.

---

## 12. Notes on adjacent locked constraints

Recorded here only because this document's numbers assume them:

- **Dodge and block as passive chances.** This brief specifies dodge as a
  passive chance to fully evade and block as a passive chance to reduce damage
  — defensive layers certain classes lean into, not player inputs. The
  throughput model in §3 assumes passive defensive layers with no
  input-skill variance. **RESOLVED [O1]** — passive block/dodge are ratified
  and the stamina pool is removed entirely, so the previously recorded
  conflict with the implemented `UBreakerCombatComponent` (held frontal block
  stance, input dodge window, shared stamina) is closed in favour of this
  document's assumption. The throughput model in §3 therefore stands as
  written. Parry, when built, uses its own short cooldown rather than stamina.
- **Crit as the only multiplier of its kind.** Nothing in this document adds a
  damage multiplier. `Experience Gain %` is an Increased bucket on a
  non-combat stat.
- **Flat → one Increased bucket → More reserved for trees/Anomalous.**
  `Experience Gain %` obeys this. Veteran's Path is explicitly outside the
  item system.
- **No verbs granted here.** Traversal challenges are base-kit-completable by
  requirement; nothing in §5.4 grants or requires air jump or parry.

---

## OPEN QUESTIONS

1. **Does the throughput model survive contact with real TTK?** Every hour
   number here is downstream of `SoloThroughput(L) = 7 + 0.26*L`. This is the
   highest-value thing to measure in the Playtest Gym feedback pass, and until
   it is measured, the 40-hour target is a hypothesis, not a design.

2. **Does Act III have 21 hours of content?** The curve says Act III is 53% of
   the campaign across three erased Earths. 8.8 already accepts Act III pacing
   as a known problem. Either the three zones are much larger than currently
   implied, or a fourth zone is needed, or repeatable rift content must
   explicitly carry ~40% of Act III's XP — which is a content-plan decision,
   not a curve decision.

3. ~~**Is the hybrid item level accepted, and who owns `ZoneLevel`?**~~
   **CLOSED [O6].** Hybrid is ratified; `ZoneLevel` lives on the rift/zone
   Data Asset; the enemy-level fallback is sanctioned for the gym. Risk 3
   below still stands: the fallback must not become permanent by neglect.

4. **Should the level 28-31 seam relief exist at all, or should Act III simply
   start cheaper?** The relief is the only discontinuity in the design and it
   is hand-authored. An alternative is to lower the Act III multiplier from
   1.30 to ~1.22 and lengthen Act III's level range feel differently. The
   relief is recommended because it targets the specific transition rather
   than the whole act, but it is worth one playtest of each.

5. **Is 65 Core Points still "two full plus one partial"?** 7.4 makes this the
   validating ratio, but constellation node costs are not authored. If they
   land differently, §7's list is CANON per O7, pinned to story beats and to
   the rift-archetype set, and should not be the thing that moves. Note that
   §10.1's keystone gate arithmetic also depends on node costs holding.

6. ~~**Does the elite/veteran/champion tier vocabulary in §5.1 match the enemy
   design plan?**~~ **CLOSED [O9].** Rank is Standard / Veteran / Champion /
   Boss, owned by this document; Archetype and Modifiers are owned by
   `Encounter-Design.md`; modifier count drives Rank. The remaining
   band-to-Rank granularity question is FLAGged in §5.1.

7. **Does XP exist at all in endgame content?** At 50 there is no XP bar. Do
   Rior's Frontier runs award nothing, or does XP convert into a crafting
   currency at a fixed rate? A conversion would be a clean sink and would keep
   the kill-value tables meaningful at cap, but it edges toward a post-cap
   progression track and needs an explicit ruling against 7.1.

8. ~~**[O29] Which ceiling governs where — zone level 50, area level 100, or
   item level 120?**~~ **RESOLVED [O36]**, corrected in §8.1. Area level stays
   capped at **100** (not 50) for endgame content; item levels 101–120 are
   sourced from the **endgame tier bonus** (Frontier tiers extending O6's
   `TierBonus` past its campaign +0..+5 span), with **the Forge as the
   secondary source**. `GetDropItemLevel` returning area level unchanged is
   the area-level component of a roll, not the whole of it. Whether
   `ZoneLevel` as an authored field is retired in favour of area level, or
   continues alongside it under a different name, is a naming question and
   still open — but it no longer blocks endgame reward-band authoring.

9. **[O29] Has the tier bonus become inert in the campaign band?** §8's Rank
   TierBonus table spans +0 to +5 item levels. The shipped campaign slope is
   ~8.2 item levels per tier, so at zone level 30 the entire spread crosses
   **one** tier boundary (at ilvl 34) and usually none — a boss and a trash mob
   drop the same affix tier. §8's own argument was that the bonus should be
   "bounded and small"; it is now smaller than bounded. Either the bonus grows,
   or §8 accepts that rarity and drop count carry the whole boss reward and says
   so. **Not re-authored (O2).**

10. **[O29] What does the level-50 build-completion readout show now?** §4
    proposes Aberrant count, Anomalous count, and **T0+ affix count**. On a
    T12..T-1 ladder a levelling player finishes at **T6** and never sees a T0,
    so the third field reads zero for the entire campaign and for a good part of
    the endgame. It needs to become a best or average equipped tier, or an
    item-level average. A `UI-UX-Spec.md` decision, not a curve one.
