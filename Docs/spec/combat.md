# Combat

## What this system is for

To resolve every damage event in the game — weapon, ability, status tick,
hazard, enemy — through one ordered pipeline, so that a number the player sees
can be explained by a rule rather than by whichever system happened to touch it
last.

It fails quietly. Two systems each adding a step to the order, and nobody
composing the result, is how armour shred stacks into negative armour and a
status build stops working against bosses. The failure is never a crash; it is
a number that is wrong in a way no one can trace, in a game where the only
feedback channel for whether gear is doing anything is that number.

## The rules

**Every damage event resolves in exactly this order.** A system may not insert
a step without writing it here first.

1. **Base and source scaling.** Flat sums, then the additive bucket for the
   pool that applies, then the More product. Damage over time uses its
   application snapshot, including the tick interval.
2. **Weak point**, where applicable.
3. **Critical**, rolled or taken from the snapshot.
4. **Passive dodge**, then **passive block**. Neither applies to damage over
   time.
5. **Facing selects the base armour value**, then armour composition, then the
   mitigation curve.
6. **Per-element resistance.**
7. **Shield routing**, unless the event bypasses shields.
8. **Remaining damage to health.**
9. **Shield-break, damage, dodge, block and death events.**

True damage skips 5 and 6.

**Armour composition, with an explicit floor:**

```
FacingArmour = facing selects the base value
AfterFlat    = max(FacingArmour - flat reductions, 0)
AfterBypass  = AfterFlat * (1 - bypass fraction)
Mitigation   = AfterBypass / (AfterBypass + K), capped
BossClamp    = on a boss, total REDUCTION may not exceed the boss cap
```

Armour shred stacking into negative armour is the classic failure and the floor
at zero forbids it. **The boss cap clamps the reduction, not the mitigation**,
so status builds still function against bosses instead of being deleted by the
one rule meant to protect them.

**Facing-dependent armour is real armour on every armoured enemy**, not a
gimmick on one. It is the mechanism that makes positioning a damage stat
without converting momentum into damage, and a rear arc that pays on one enemy
and nothing else teaches the player that flanking does not work.

**Block and dodge are passive chance layers.** No stamina pool exists and none
may be authored. A dodge is full evasion and returns immediately, raising
nothing that keys off being hit — anything watching for a hit correctly sees
nothing. **Parry is the only defensive input**, on its own short cooldown.

**Crit and weak point are the two site multipliers.** Crit is build-gated,
weak point is skill-gated and archetype-bounded, and nothing else multiplies at
the hit site.

**The proc coefficient law**, governing weapons, classes and items alike:

- Multishot-generated projectiles carry **0** for status application, on-hit
  effects and node triggers, and **1** for damage.
- Ricochets carry **0.5** and cannot chain — a ricochet never spawns another.
- Spread and transfer ancestry caps at **depth 2** with a normalized payload: a
  spread copy carries the original's remaining budget, never a fresh full
  application.
- A damage-over-time tick triggers only effects that declare compatibility
  with it.

**Damage over time can crit, and snapshots at application** — source power, the
crit result, the multiplier and the tick interval. Reapplication adds a stack
and refreshes duration but keeps the original snapshot. Tick intervals are
discrete, so stacking has visibly diminishing steps rather than an invisible
ceiling.

**Recoil moves the aim, and the trace follows the aim**, so the round always
goes to the crosshair. The viewmodel kick is applied after the trace resolves.
A gun that shoots somewhere other than where it points is the one feel bug that
cannot be tuned away.

**Advanced movement is never required** to land a routine shot or avoid a
baseline attack. Ordinary forward movement does not self-accelerate past
sprint; wall riding preserves flow but generates no speed; dash solves a
positioning problem rather than being the fastest way to travel; sliding has a
clear beginning and end.

**Healing resolves through the same contract as damage** — health, then
overheal, then optionally overheal to shield — and **healing is not revival**:
a dead actor is refused.

**Elements are rules, not percentages.** An element that is "your damage but
tinted" competes for a budget that is already spent, and three interchangeable
damage types are accumulation wearing three hats.

**Combat resolves on the server.** That is what makes the More ceiling and the
proc coefficient law enforceable rather than advisory — a client that could
resolve its own damage could lie about it, and a loot game whose drop table is
downstream of damage dealt cannot afford that. Movement, recoil and the
viewmodel are client-predicted and server-reconciled, because a movement
shooter with server-round-trip aiming is not a shippable feel regardless of
topology.

## The model

### The three elements

A rift is a hole in time, so the elements are the three things a hole in time
does. Each owns a verb no other element has.

| Element | Verb |
|---|---|
| **Rift** | Displace |
| **Entropy** | Accelerate decay |
| **Void** | Erase |

Three elements give exactly three pairs, which is small enough to memorise and
large enough to be a rotation — that is why the count is three rather than
four. Each pair has one reaction. **One reaction per target per interval**, and
**a reaction may never itself apply a status**, or the matrix recurses.

Severance — the degradation that turns a refugee into a hostile — is Entropy
happening slowly to a person, which is why the element set and the enemy
families are the same idea at two scales. The elements do not need explaining
in a tutorial; they are already the plot.

### Weapon archetypes

Eight, differentiated on damage, cadence, magazine, spread, falloff, reload and
swap tempo rather than on damage alone. Swap tempo is a real axis: a sidearm
that comes up fast is a different weapon from a machinegun that does not.

Distance falloff is per-pellet geometry evaluated at the pellet, not a
stat-layer multiplier. Weak point multipliers are per archetype and bounded.
Fire rate is a named, watched, uncapped lane.

**Melee coefficients read the full weapon base, never a per-pellet share.**
Reading the pellet makes the shotgun the worst melee stat stick in the game and
the sniper the best, which inverts what every player expects from both.

### Enemies

Two families. **Vestiges** are rift-native, with no design intent and no
readable anatomy. **The Altered** are refugees from finished timelines, and
severance degrades them until nothing is left but the shape.

Three orthogonal fields describe any enemy: **Archetype** is behaviour,
**Rank** is reward tier, and **Modifiers** are zero to three. Modifier count
drives rank; boss is authored rather than derived. Pack rarity is a composition
template over the three, never a fourth field.

Normals shape the player's route rather than testing reflexes: a closer, a
ranged enemy that holds a band and denies ground, and an anchor that punishes
frontal approach. Each is a different reason to move.

## Boundaries

This spec owns resolution, the pipeline, weapons, movement rules and the
element model. It does not own:

- what a multiplier is worth or how the curves compose — **power and scaling**;
- which affix or rewrite feeds a step — **items and crafting**;
- which conditions exist and how a node authors one — **progression and
  trees**;
- what an ability does before it submits damage — **classes and abilities**;
- modifier selection, spawn pacing and encounter composition — **content and
  modes**;
- how a damage number is drawn — **art and UI**.

## Asserted invariants

| Invariant | Test |
|---|---|
| Effective armour never goes negative, at any combination of flat and bypass reduction | `Combat.Armour.Floor` |
| The boss cap clamps total reduction, not mitigation | `Combat.Armour.BossCap` |
| A dodge returns zero damage and raises no on-hit effect | `Combat.Dodge.ShortCircuits` |
| Facing armour applies on every armoured enemy, including to damage over time | `Combat.FacingArmour.Coverage` |
| Multishot projectiles proc at zero; ricochets at half and never chain | `Combat.ProcCoefficient.Law` |
| Spread ancestry stops at depth two with a normalized payload | `Combat.ProcCoefficient.SpreadDepth` |
| A damage-over-time snapshot survives reapplication unchanged | `Combat.Status.SnapshotStability` |
| The trace follows the aim after recoil, at every archetype and accumulation level | `Weapons.RecoilPattern`, `Weapons.TraceFollowsAim` |
| Melee coefficients read the full weapon base | `Weapons.MeleeCoefficient` |
| Healing refuses a dead actor and reports overheal at full value | `Combat.Healing.Contract` |
| Every damage submission passes through the outgoing-modifier chain | `Combat.AbilitySubmissionConformance` |
| A reaction applies no status, and no target takes two inside one interval | `Combat.Elements.ReactionMatrix` |

## Open

- Whether a stagger and interrupt model is built as a binary state with a
  resistance stat and a per-enemy immunity flag. Four systems already assume
  one exists.
- Lag-compensation tolerance: how far back the server rewinds to validate a
  shot. Deferred rather than answered, because the current topology makes it a
  small-number-of-players problem.
- Whether enemies deal elemental damage. If they never do, elemental
  resistance is a stat with nothing to resist.
- The resistance formula and its value ranges.
