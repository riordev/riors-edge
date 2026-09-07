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
5. **Armour composition**, then the mitigation curve.
6. **Per-element resistance.**
7. **Shield routing**, unless the event bypasses shields. Facing selects
   whether the bearer's front pool stands in this step: a frontal hit pays
   the front pool, then the ward, then health; a rear-arc hit pays the ward,
   then health. A front pool spent to zero is broken for the fight.
8. **Remaining damage to health.**
9. **Shield-break, damage, dodge, block and death events.**

True damage skips 5 and 6.

**Armour composition, with an explicit floor:**

```
Armour       = the enemy's armour attribute
AfterFlat    = max(Armour - flat reductions, 0)
AfterBypass  = AfterFlat * (1 - bypass fraction)
Mitigation   = AfterBypass / (AfterBypass + K), capped
BossClamp    = on a boss, total REDUCTION may not exceed the boss cap
```

Armour shred stacking into negative armour is the classic failure and the floor
at zero forbids it. **The boss cap clamps the reduction, not the mitigation**,
so status builds still function against bosses instead of being deleted by the
one rule meant to protect them.

**Facing-dependent defence is the front shield pool on every shielded enemy**, not a gimmick on one: frontal hits spend a pool of 15 % of the bearer's max health until it breaks, then the front is open for the fight, and the rear was never covered. Positioning is a damage stat without converting momentum into damage.

**Block and dodge are passive chance layers.** No stamina pool exists and none
may be authored. A dodge is full evasion and returns immediately, raising
nothing that keys off being hit — anything watching for a hit correctly sees
nothing. **Parry is the only defensive input**, on its own short cooldown.

Purchased Parry defaults to V and can be rebound. Its O2 timing is a 0.25-second
window on a 2-second cooldown; Read adds 0.10 seconds. The first positive direct
hit from the forward hemisphere is negated. Rear hits, damage over time,
self-damage and hits without a source position do not consume the window.
A parry grants no hit, block or dodge procs. Counterweight's existing 16%
Increased weapon damage applies only for 2 seconds after a successful parry.

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
sprint; dash solves a
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

`Status.Void` erases part of the target's armour and incoming healing for a
timed window. It deals no periodic damage and produces no damage-tick events.
Its percentage reductions follow existing flat armour strips and healing
modifiers; repeated applications refresh one effect. Immunity, avoidance,
cleanse, consumption and expiry use the ordinary status lifecycle. Siphon's
successful hits and Fracture's Bleed → Poison → Void cycle apply it.

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

**Damage Ramp is a Primary prefix with at most ten consecutive-hit stacks.**
A damaging shot adds one stack for subsequent shots; pellets and splash victims
do not each add a stack. Purchased Redline Trigger adds two while at Redline.
Its per-stack percentage joins Increased weapon damage, including weapon DoT
snapshots. A miss, weapon swap, removal or death clears the streak. Projectiles
advance or break the streak in impact order; an old weapon's late impact cannot rebuild it.

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
| Effective armour never goes negative, at any combination of flat and bypass reduction | `Combat.Armor.Floor` |
| The boss cap clamps total reduction, not mitigation | `Combat.Armor.BossCap` |
| A dodge returns zero damage and raises no on-hit effect | `Combat.Defense.DodgeShortCircuits` |
| Facing armour applies on every armoured enemy, including to damage over time | `Combat.FacingArmor.Coverage` |
| Multishot projectiles proc at zero; ricochets at half and never chain | `Combat.ProcCoefficient.Law` |
| Spread ancestry stops at depth two with a normalized payload | `Combat.ProcCoefficient.SpreadDepth` |
| A damage-over-time snapshot survives reapplication unchanged | `Combat.Status.SnapshotStability` |
| The trace follows the aim after recoil, at every archetype and accumulation level | `Weapons.RecoilPattern`, `Weapons.TraceFollowsAim` |
| Melee coefficients read the full weapon base | `Combat.AbilityScaling.MeleeSwingsTheFullBlast` |
| Healing refuses a dead actor and reports overheal at full value | `Combat.Healing.ThroughContract` |
| Every damage submission passes through the outgoing-modifier chain | `Combat.Ceiling.AbilitySubmissionConformance` |
| A reaction applies no status, and no target takes two inside one interval | `Combat.Elements.ReactionMatrix` |

## Stagger and landing

Stagger is an authoritative binary interrupt. Resistance scales its duration
by `1 - resistance`, with resistance clamped to 0–1. Each enemy can explicitly
opt into immunity. Overlapping staggers keep the later expiry. Death and
revival clear the state. Warden has 0.5 resistance and its successful slam
applies 0.5 seconds of stagger (O2 tuning).

Stagger stops held fire, enemy attack windups and active channels, pending
placements and plunges. It blocks new movement actions, attacks and casts;
gravity continues. Existing timed buffs and deployed objects remain active.

Ordinary falls are safe through six metres. Each excess metre deals 5% of
maximum health, capped at 100% per landing (O2 tuning). This environmental
damage has no attacker, crit or proc, ignores armor and avoidance, and is
absorbed by shields before health. Kinetic Recovery cancels this landing
damage after an owned Breach launch within three seconds and grants 1.5
seconds of stagger immunity. Foreign launches, teleports, traversal and death
invalidate the launch; a later class or node change cannot retain protection.

## Open questions

- Lag-compensation tolerance: how far back the server rewinds to validate a
  shot. Deferred rather than answered, because the current topology makes it a
  small-number-of-players problem.
- Whether enemies deal elemental damage. If they never do, elemental
  resistance is a stat with nothing to resist.
- The resistance formula and its value ranges.
