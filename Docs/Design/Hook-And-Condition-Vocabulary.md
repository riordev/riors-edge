# Hook and Condition Vocabulary — where a node attaches, and what it may ask

**Scope:** slice (see `Vertical-Slice.md`). The vocabulary is authored whole;
§6 stages which parts of it are wired, and in what order.
**Last reconciled against: O40**

Authority chain per O28: `Decisions.md` -> `Design-Overview.md` (map, not law) ->
this document.

Status: Tier 0 spec. This is **S4**, recorded open in `Design-Overview.md:666`
as *"the largest unwritten technical document in the project"* and named by
`CONTEXT.md` as the single highest-leverage unblock in the progression layer.
No balance values are authored here; every number cited is either an existing
ruling or flagged `O2 PLACEHOLDER`.

---

## 0. Why this document exists, in one number

**53 of the 97 authored tree nodes ship with no stat effect at all.** They are
`GrantedTags` and a `WAITING ON:` comment. Per tree:

| Tree | nodes | inert | |
|---|---|---|---|
| Core Slice | 30 | 7 | |
| Swift Kinetic | 14 | 8 | |
| Swift Marksman | 13 | 9 | |
| Swift Frenzy | 13 | 3 | |
| Caster Spellblade | 9 | **9** | 100% |
| Caster Void Whisperer | 9 | 8 | |
| Caster Multispell | 9 | **9** | 100% |

Two facts sharpen it. **Caster's entire O3 keystone budget produces zero
damage**: `Edgework` and `Cascade` are cornerstone nodes costing 3 points with
no effects at all, and `Long Dark` authors a legal `DamageOverTime` +
`MorePercent` pair that `AggregateStats` drops with a warning because only
`Damage` composes a More product. And **two tier-1 gateway nodes are inert** —
`Core.Volley.TriggerDiscipline` and `Core.Affliction.OpenWound`, the cheapest
and most-purchased nodes of their constellations.

Two enums are named as the cause in more of those comments than anything else:

- `EBreakerNodeStatTarget` had 11 entries and no way to say resource
  generation, resource decay, maximum resource, ability cost, ability cooldown,
  ability radius, ability duration, incoming damage reduction, pierce,
  projectile behaviour, recoil, or spread.
- `EBreakerBuildCondition` had 6 entries and was **movement-only**. O30's own
  implementation note: *"`EBreakerBuildCondition` is movement-only today, which
  is why no node can key off combat or status state. Several axes in the
  taxonomy (ailment, crit, stacking) need it widened before they can be
  authored honestly."*

This document is the enumeration O30 asked for and `Class-Kits.md` §7 asked for
before it. §1 is the hook vocabulary, §2 the conditions, §4 the stat targets,
§5 the layering law they all obey, §6 the ordered wiring roadmap, §7 what is
deliberately out.

### 0.1 The four rulings this is built on

Owner, verbatim, and all four are load-bearing:

1. *"conditions do compose yes"* — an effect may require several conditions at
   once. §2.5.
2. **Target-side conditions: YES.** A node may key off the state of what it is
   hitting. §3 — and §3.2 is honest about what it costs.
3. *"yes trees modify abilities i want that to be an avenue players can scale
   not just weapons or character (think caster for instance should be heavily
   spell oriented over weapons)"* — §4.2.
4. *"conditionals should be flavour that add spice overall"* — §5.3, and it is
   the one most easily got wrong.

---

## 1. The HOOK vocabulary

A **hook** is a point in the frame where a tree node may attach. Each is listed
with what it can READ, what it may MODIFY, and where it lives in code today.

The hooks divide into two kinds, and confusing them is the standing failure
mode:

- **COMPOSITION hooks** run *before* a number is fixed and may change it. There
  are two, and only two.
- **REACTION hooks** run *after* and may only cause something else to happen.
  A reaction hook that tries to change the number it was told about is a bug;
  the number has already been applied.

### 1.1 Composition hooks

| Hook | Where | Reads | May modify |
|---|---|---|---|
| **H1 — Attribute submission** | `UBreakerProgressionComponent::AggregateStats` → `FBreakerAttributeContribution` | Owned node ranks; `FBreakerBuildConditionState` for SELF conditions | Any `EBreakerAggregatedAttribute`, via `AddFlat` / `AddIncreasedPercent` / `ComposeMore` |
| **H2 — Outgoing damage** | `UBreakerCombatComponent::ApplyOutgoingModifiers` | The mutable `FBreakerDamageRequest`: `SourceTags`, `DamageFamily`, `Instigator`, `bWeakPointHit`, crit fields | `BaseDamage` (flat) and `SourceDamageMultiplier` (More) |
| **H3 — Incoming damage** | `UBreakerCombatComponent::ReceiveDamage`, before `ResolveDamage` | `FBreakerDefenseState`, `Request.Instigator`, **and the target — `GetOwner()`** | `Defense.IncomingDamageMultiplier`, `Defense.Armor`, dodge/block chances |

H1 is the workhorse and the only one a tree touches today. It is a *standing*
offer re-derived whenever anything changes, not a per-event callback — which is
why every SELF-conditional line already works: the component re-submits when the
condition state changes.

**H2 is the hook with the structural hole.** `ApplyOutgoingModifiers` takes only
the request and runs on the attacker's component. It has **no reference to the
target** — no parameter, no member, and `FBreakerDamageRequest` carries an
`Instigator` but no target field. Every one of the ~14 damage submission sites
has the target in local scope and none of them pass it. This is why "more damage
to bleeding enemies" is not expressible, and §3.2 is about fixing it.

**H3 already knows both parties.** `ReceiveDamage` runs on the victim, so
`GetOwner()` is the target and `Request.Instigator` is the attacker. That
asymmetry — the incoming path knows both, the outgoing path knows one — is the
single most important structural fact in this document.

### 1.2 Reaction hooks

All exist today as delegates. None has a tree-side consumer.

| Hook | Delegate | Side | Payload |
|---|---|---|---|
| **H4 — Hit dealt** | `UBreakerCombatComponent::OnHitDealt` | attacker | `FBreakerHitContext` — Instigator, Target, Result, bFromDoT, bWeakPoint, DamageFamily, WorldLocation |
| **H5 — Kill dealt** | `::OnKillDealt` | attacker | same |
| **H6 — Damage taken** | `::OnDamageTaken` | victim | same, with Instigator |
| **H7 — Death** | `::OnDeath` | victim | none |
| **H8 — Healing dealt** | `::OnHealingDealt` | healer | `FBreakerHealContext` |
| **H9 — Status applied / expired / consumed** | `UBreakerStatusComponent::OnStatusApplied` / `OnStatusExpired` / `OnStatusConsumed` | target | the status |
| **H10 — Shot fired** | `UBreakerWeaponComponent::OnShot` | shooter | `FBreakerShotResult` |
| **H11 — Ability window ended** | `UBreakerAbilityStateComponent` | caster | window key |
| **H12 — Zone occupancy / expiry** | `UBreakerZoneActor` | zone | occupancy |

`FBreakerHitContext` is the only struct in the pipeline holding both actors —
but it is built *after* resolution, so it can react and never modify. A node
that wants to change a number must use H1, H2 or H3.

**The H9 family has zero listeners anywhere.** So does `HasStatus`, which exists
on the status component and is called by nothing outside tests. The query
primitives for the whole ailment axis are built and unwired.

### 1.3 Resource and ability hooks

| Hook | Where | Notes |
|---|---|---|
| **H13 — Resource loop override** | `PushLoopOverride` / `PopLoopOverride` on Momentum, Charge, Grit, Scrap; `PushGenerationSuspension` on Mana | Takes `bSuspendDecay` and a generation multiplier. **The valve `ClassResourceDecay` and `ClassResourceRegen` plug into** — it already exists on all five loops. |
| **H14 — Ability cost** | `virtual UBreakerGameplayAbility::GetResourceCost()` | The one ability modifier hook that exists. `UBreakerCasterAbility` overrides it and reads `ResourceCostMultiplier` off the attribute set. |
| **H15 — Ability cooldown** | `UBreakerGameplayAbility::ApplyCooldown` | **No hook.** Reads `Definition->CooldownSeconds` raw. Needs one. |
| **H16 — Ability geometry** | per-subclass `UPROPERTY` floats | **No hook and no shared shape.** `Cleave::RangeCm`, `Cleave::ArcDegrees`, `Rot::RadiusCm`, `Rot::DurationSeconds` — differently named on every class. |
| **H17 — Ability variant** | `UBreakerAbilityDefinition::ResolveVariant(OwnerTags)` | Keystone-gated parametric override. The existing precedent for a tag changing an ability's numbers, and the cheapest route for several inert nodes. |

### 1.4 The hook a node may NOT have

There is no "on stat recomputed", no "every frame", and no arbitrary
script hook. A node is a *declaration* — ranks in, contribution out — and the
aggregation is re-derivable from owned ranks alone. That property is what makes
respec, save-load and the tooltip preview all use one code path. A hook that let
a node accumulate hidden state across frames would end it.

---

## 2. The CONDITION vocabulary

### 2.1 What a condition is, and the line that keeps this enum small

A condition is a **predicate on live state**: true or false of an actor at an
instant, independent of any particular event.

It is **not a property of a hit**. "Was this a critical", "was this a weak
point", "did this kill", "was this melee" are outcomes *of* an event, and they
are deliberately absent from the enum:

- **Per-event outcomes** (crit, weak point, kill) belong to the **hook payload**
  — `FBreakerHitContext` already carries them.
- **Delivery method** (melee / weapon / ability) belongs to the **stat target
  vocabulary** as a partition of Damage — §4.2 — discriminated by `SourceTags`,
  which already carries `Damage.Melee`.

This line is not pedantry. It is what keeps a 32-bit budget affordable, and it
stops two systems giving two answers to one question.

### 2.2 The budget

The mask is `uint32`. **32 is a hard ceiling** — `1u << 32` is undefined
behaviour, not a wrap. Index 0 is `Always`.

**Final set: 24 entries. 8 bits unspent.**

### 2.3 SELF conditions (0–16)

| # | Name | Reads | Evaluable today |
|---|---|---|---|
| 0 | `Always` | — | yes (definitionally) |
| 1 | `Airborne` | `IsFalling()` | yes |
| 2 | `Sliding` | `IsSliding()` | yes |
| 3 | `WallRiding` | `IsWallRiding()` | yes |
| 4 | `Redline` | `GetMomentumState()` + `IsActiveForOwner()` | yes |
| 5 | `RecentlyDashed` | `GetLastDashTime()` | yes |
| 6 | `Grounded` | `!IsFalling()`, same frame | **new, yes** |
| 7 | `Stationary` | `GetHorizontalSpeed()` vs threshold | **new, yes** |
| 8 | `Aiming` | `UBreakerWeaponComponent::IsAiming()` | **new, yes** |
| 9 | `HealthHigh` | health fraction ≥ `HighVitalFraction` | **new, yes** |
| 10 | `HealthLow` | health fraction ≤ `LowVitalFraction` | **new, yes** |
| 11 | `ResourceLow` | class-resource fraction ≤ `LowVitalFraction` | **new, yes** |
| 12 | `ResourceDepleted` | class-resource fraction ≤ 0 | **new, yes** |
| 13 | `RecentlyKilled` | last-kill time | **new, NO — needs a recorder** |
| 14 | `RecentlyTookDamage` | last-damage-taken time | **new, NO** |
| 15 | `RecentlyCastAbility` | last-activation time | **new, NO** |
| 16 | `RecentlyAppliedStatus` | last-status-application time | **new, NO** |

`Grounded` exists as its own bit because **there is no negation operator** —
composition is AND only (§2.5) — so "while grounded" is otherwise unauthorable.
It is written as the same frame's negation of the same read, so the two can never
disagree.

`ResourceLow`/`ResourceDepleted` work across all five loops: Momentum, Mana,
Charge, Grit and Scrap all expose a `GetXFraction()` and exactly one is active
per class. `ResourceDepleted` is separate from `ResourceLow` because Caster's
Overcast drives Mana **negative**, and `Spellblade.Debt` and `Spellblade.Bloodprice`
are authored against the debt state specifically.

**The `Recently*` family (13–16) is the one place the vocabulary outruns the
code on the self side.** Each needs a small recorder storing the world time of
its last event, bound to delegates that already exist (H4, H5, H6) and read the
way `RecentlyDashed` reads `GetLastDashTime()`. That is one component, four
fields, four bindings. Until it lands they are **loud, not silent** — §2.7.

### 2.4 TARGET conditions (17–23)

Owner ruling 2. **None is populated by `EvaluateForActor` and none ever will
be**: that function takes one actor, and target state is a two-actor fact.

| # | Name | Reads | Query exists |
|---|---|---|---|
| 17 | `TargetAiling` | `GetDistinctStatusTypeCount() > 0` | yes |
| 18 | `TargetBleeding` | `HasStatus(Status.Bleed)` | yes |
| 19 | `TargetPoisoned` | `HasStatus(Status.Poison)` | yes |
| 20 | `TargetMultiStatus` | `GetDistinctStatusTypeCount() >= MultiStatusThreshold` | yes |
| 21 | `TargetLowHealth` | target health fraction | yes |
| 22 | `TargetElite` | `ABreakerEnemy::GetMonsterRank() != Trash` | yes |
| 23 | `TargetAtCloseRange` | distance vs `CloseRangeCm` | yes |

Every underlying query already exists. What does not exist is the **call site**
— §3.2.

`TargetMultiStatus` is O30's **stacking** axis, and it is the predicate
`Multispell.Cascade`'s reserved More and `Multispell.Chain` are both written
against. It is a boolean against a named threshold because a condition is a
boolean; the threshold is a visible constant rather than a magic number.

`TargetElite` is the honest way to author a strong conditional without inflating
trash clear — which O27 wants trivialized anyway.

### 2.5 Composition

Owner ruling 1. An effect carries a primary `Condition` plus an `AlsoRequires`
array, and pays only while **all** of them hold.

**AND only. No OR, no NOT.**

- **OR** is two effects. It reads more honestly too: a tooltip has to print both
  lines regardless.
- **NOT** is a trap. "Not airborne" and "grounded" differ on ladders, in water
  and mid-teleport, and a player cannot tell which they bought. Where a
  complement is genuinely wanted it gets its own entry — that is what `Grounded`
  is, and it is the only one the content needs.

A separate array rather than a widened field keeps `Condition`'s serialized
value and meaning intact, so every authored node row and every affix row that
predates composition is untouched.

### 2.6 What was left out of the 32, and why

Budgeted out deliberately. Each would have been a bit:

- **`Sprinting`** — no authored node needs it; `Stationary` and `Grounded`
  already cover the posture axis.
- **`ResourceHigh`** — the twin of `ResourceLow`. Mana *inverted* (it starts
  full and drains), so "high resource" means opposite things to Caster and to
  Swift, and one bit cannot honestly carry both readings.
- **`TargetFullHealth`** — the opener predicate. No authored node wants it, and
  `TargetLowHealth` is the half the content actually uses.
- **`TargetImpaired` / `TargetSlowed` / `TargetStunned`** — **no slow or stun
  tag exists**. The nearest thing, `Status.Frost` ("frost buildup and slow"), is
  elemental and therefore post-slice under O38. Adding this would mean inventing
  a tag or keying off an element; both are somebody else's ruling.
- **All elemental predicates** — `TargetBurning`, `TargetChilled`,
  `TargetShocked`, and any Rift/Entropy/Void state. **O38: Elements are
  post-slice.** Of the six status tags in `DefaultGameplayTags.ini`, only Bleed
  and Poison are non-elemental, and only three of the six are ever applied by
  any code at all.
- **Per-hit outcome predicates** — crit, weak point, kill, melee. §2.1: they are
  hook payload or stat-target partition, not state.
- **Minion / deployable predicates** — O30's MINIONS axis. Nothing exists in any
  form (O30's own note), so a condition would be a bit spent on a fantasy.

Eight bits remain. The obvious future claimants are the elemental target states
(four or five, when O38 lifts) and a minion-alive predicate — which is very
close to exactly the remaining budget, and is the reason the trimming above was
worth doing.

### 2.7 Dead conditions must be loud

The project has now shipped this bug four times: the third jump gated on a level
nothing awarded; the phantom ability grants resolving to ids that did not exist;
the phantom keystones no node granted; the `MorePercent` silently dropped on
every stat target but `Damage`. Every one compiled, purchased, displayed, and
did nothing quietly.

So: `SatisfiesAll` / `SatisfiesOne` **warn once per condition** when a
requirement names something that cannot be evaluated in the state it was given —
a `Recently*` entry with no recorder, or a `Target*` entry on a state whose
target half was never supplied.

Three details that make it a signal rather than noise:

- **The bit wins first.** If a condition is set, it holds and nothing warns.
  This is what stops `All()` — the tooltip's hypothetical — from reporting the
  `Recently*` family as false and understating every potential figure.
- **"Asked and got no" ≠ "never asked."** `HasTargetState()` is a separate flag,
  set even when the target is null, so a DoT ticking after its victim was
  destroyed is an ordinary miss rather than a warning storm.
- **Warn once per condition, not per call.** A conditional effect is evaluated
  on every stat recomputation, which on a moving character is many times a
  second. One line is a signal; the same line at 60 Hz trains people to filter
  the channel.

`SatisfiesAll` deliberately does **not** short-circuit, so a node requiring two
dead conditions reports both instead of looking half-fixed.

---

## 3. Target-side conditions: what they actually cost

Owner ruling 2 is the expensive one. This section is the honest accounting.

### 3.1 Why the obvious fix does not work

The obvious fix is to pass the target into `ApplyOutgoingModifiers` (H2). It
fails for three reasons:

1. **~14 call sites**, each of which would have to be found and changed.
2. **Projectiles have no target at fire time.** The rocket path composes its
   request when the weapon fires. A homing rocket that acquires a bleeding enemy
   two seconds later would have composed against nothing.
3. **DoT ticks have no live source composition at all.** They use the
   application snapshot by design (O10), so there is no outgoing pass to hook.

### 3.2 The design: resolve on the target side

**Target-conditional lines are composed in `ReceiveDamage` (H3), not at the
source.** That site already has both actors, it is reached by every damage event
including projectiles and DoT ticks, and it is **one** call site.

The mechanism:

1. `UBreakerProgressionComponent` publishes, alongside its attribute
   contribution, a small **rider table**: the node effects for which
   `FBreakerNodeEffect::RequiresTargetState()` is true, as
   (requirement, stat target, percent) rows. It is re-derived with everything
   else, so nothing new needs invalidating.
2. The submission site attaches the attacker's rider table to the request —
   or, cheaper, `ReceiveDamage` reads it off `Request.Instigator`'s progression
   component, which it already has.
3. `ReceiveDamage` builds the condition state for the event: the attacker's
   SELF state (which the attacker's component already caches, via
   `GetActiveConditions()`) plus `SupplyTargetState(GetOwner(), Instigator)`.
4. Each rider whose requirement is satisfied contributes its percent.

### 3.3 The part that is genuinely expensive

Step 4 cannot simply multiply. `Request.SourceDamageMultiplier` arrives
**already composed** as `(1 + Increased/100) × MoreProduct`. Folding a target
rider into it would make the rider a *multiplier* — a second More in everything
but name, outside the O3 budget, and a direct violation of the one-additive-
bucket law (§5).

To keep target-conditional lines in the same additive bucket as everything else,
the request must carry the source's Increased bucket **separately**:

```
FBreakerDamageRequest gains:
    float SourceIncreasedPercent   // the additive bucket, whole percent
    float SourceMoreProduct        // the composed More product
    // SourceDamageMultiplier stays, as the composed convenience value, so
    // every existing call site and every test keeps working unchanged.

ReceiveDamage then recomposes, only when riders fired:
    Effective = (1 + (SourceIncreasedPercent + RiderPercent)/100) * SourceMoreProduct
```

**This is the real cost of ruling 2, and it should be read before it is
approved:** three fields on the damage request, one recomposition in
`ReceiveDamage`, a rider table on the progression component, and a conformance
test — because O34 §4a is explicit that *"a new lane requires a canon row plus a
conformance test before it may merge"*, and this is a new lane touching outgoing
damage.

Two consequences worth stating plainly:

- **A target-conditional `MorePercent` is not supported and should not be.**
  It would need the More *selection* (strongest three, O3) to be re-run per
  event per target, which is both expensive and unexplainable to a player.
  Target-side lines are Increased-bucket only.
- **The tooltip's "potential" figure becomes an upper bound**, not a best case.
  `All()` now means "everything live, against the most convenient possible
  enemy". The skill screen should label it as such.

### 3.4 Conditional healing, statuses, and everything else

Out of scope for the first wiring. The rider mechanism generalizes, but O40(c)
says a feature merges with its reachability, and one axis piloted end to end
(CONTEXT.md's *"pilot ONE axis end-to-end before authoring at scale"*) is worth
more than four half-wired ones.

---

## 4. The STAT TARGET vocabulary

`EBreakerNodeStatTarget` goes from 10 entries to **31**. Serialized by value into
Data Assets, so: **append only, never insert or reorder.**

**Adding an entry does not make it pay.** A stat target needs an aggregation
lane in `AggregateStats`. `BreakerStatTargetHasAggregationLane()` is the
hand-maintained register of which have one; it is pinned by
`RiorsEdge.Progression.ConditionVocabulary.StatTargets` so the count can only
move deliberately, and so §6 has a progress bar rather than a promise.

### 4.1 The original ten (values 0–9)

`CriticalChance`, `CriticalDamage`, `MoveSpeed`, `SlideSpeed`, `AirControl`,
`DodgeChance`, `BlockChance`, `Health`, `DamageOverTime`, `Damage`. Unchanged,
and their values must stay put.

### 4.2 Ability scaling (10–16) — O30's ABILITIES axis, owner ruling 3

| # | Name | Bucket | Attribute exists? | Named by |
|---|---|---|---|---|
| 10 | `AbilityDamage` | Increased | no | ruling 3; O35 made ability damage ride gear depth, this makes it ride the tree |
| 11 | `AbilityCost` | Increased (of the reduction) | **yes** — `ResourceCostMultiplier` | `Frenzy.NoSafety`, `Kinetic.SpendToLive`, `Marksman.Ledger`, all of Caster |
| 12 | `AbilityCooldown` | Increased (as divisor) | no | `Kinetic.Redirect` |
| 13 | `AbilityArea` | Increased | no | `Spellblade.Edge`, `Marksman.CalledShot`, Rot |
| 14 | `AbilityDuration` | Increased | no | `VoidWhisperer.LongDark`, `.Lingering` |
| 15 | `WeaponDamage` | Increased | no | O30's GUNS axis |
| 16 | `MeleeDamage` | Increased | no | `Spellblade.Edgework`'s reserved More — *"melee-only is the tax"* |

`AbilityDamage` / `WeaponDamage` / `MeleeDamage` **partition** `Damage` rather
than replacing it. `Damage` stays the everything line; these are the narrower,
more characterful lines a Caster or Gunsmith tree wants. A node authors one of
the three, never `Damage` plus a partition — that would double-dip one bucket
from one node. The discriminator is `SourceTags`, which already carries
`Damage.Melee`.

**`AbilityCooldown`, `AbilityArea` and `AbilityDuration` are the three with real
build cost.** Cost has a hook (H14); cooldown does not (H15 reads
`Definition->CooldownSeconds` raw); and radius/duration have **no shared
representation at all** (H16) — they are differently named `UPROPERTY` floats on
each subclass. Each needs a base-class accessor before its lane can land
anywhere. `ResolveVariant` (H17) is the cheap precedent to follow.

### 4.3 Defence (17–19)

| # | Name | Bucket | Notes |
|---|---|---|---|
| 17 | `IncomingDamageReduction` | Increased | The node layer has never authored defence beyond flat Health. Stored as a reduction percentage joining **one** additive bucket, never a multiplier per source — stacking multiplier sources is how a build reaches immunity by accident. Named by `Kinetic.MomentumShield`. |
| 18 | `Armor` | Flat | Attribute exists and its comment invites this entry by name: *"the moment a second layer does (a Bulwark node) the two are additive from day one."* |
| 19 | `Lifesteal` | Increased | `Spellblade.Bloodprice` |

### 4.4 Class resource (20–22)

| # | Name | Bucket | Attribute exists? | Named by |
|---|---|---|---|---|
| 20 | `MaxClassResource` | Flat | **yes** | `Multispell.Reservoir` — flagged in Class-Kits as *"the one intentional stat node in the Caster kit"* that the enum could not carry. The single clearest case in the content. |
| 21 | `ClassResourceRegen` | Flat + Increased | **yes** | `VoidWhisperer.Patience`, `.Seep`, `.StandingWater` |
| 22 | `ClassResourceDecay` | Increased | no, and cannot simply get one | `Kinetic.NoGround`, `Marksman.Reserve`, `Frenzy.NoSafety` |

`ClassResourceDecay` is the interesting one. Decay is a **per-loop rule** computed
inside each class component (`UBreakerMomentumComponent::DecayRateForSpeed` and
its four siblings), not a shared stat, so it cannot become an aggregated
attribute. But all five components already expose `PushLoopOverride` /
`PushGenerationSuspension` (H13) — so this is plumbing to an existing valve, not
a new system. It unblocks the three Swift tier-4 nodes with real downsides,
which are the nodes O2 most wants shipped whole rather than upside-only.

### 4.5 Weapon handling (23–28) — O30's GUNS axis

| # | Name | Bucket | Attribute exists? | Named by |
|---|---|---|---|---|
| 23 | `FireRate` | Increased | **yes** | O34 calls fire rate a *"named, watched, currently-uncapped lane"* — a tree line here is watched too |
| 24 | `DashCooldown` | Increased (as divisor) | **yes** | `AggregateStats` already carries a comment asking for exactly this entry |
| 25 | `RecoilRecovery` | Increased | no | `Core.Volley.TriggerDiscipline` — an inert **tier-1 gateway** node |
| 26 | `WeaponSpread` | Increased | no | `Marksman.Steady` |
| 27 | `ProjectileCount` | **Flat only** | no | `Core.Volley.LastRound`, Salvo/Barrage |
| 28 | `Pierce` | Flat | no | `Marksman.PierceDiscipline`, `.Overpenetration`, `.Sightline` |

`ProjectileCount` is Flat-bucket **only** and rounds down. "+50% projectiles" is
meaningless on a single-shot weapon, and its lane must refuse an Increased
authoring rather than silently firing 1.5 bullets. Note also that the proc
coefficient law (Damage-Pipeline §3) says added projectiles proc at 0 — that law
is currently unenforced, and `ProjectileCount` is the entry that will make it
matter.

### 4.6 Status (29–30)

| # | Name | Bucket | Named by |
|---|---|---|---|
| 29 | `StatusDuration` | Increased | `Multispell.Resonance` |
| 30 | `StatusChance` | Flat | The magnitude twin of `Core.Affliction.OpenWound`, whose own rewrite — guaranteed application on weak point — stays a tag, because bypassing a roll is not a percentage. |

### 4.7 What is NOT a stat target, and stays a tag

A large share of the 53 inert nodes are **correctly** tags and always will be.
`Class-Kits` §7 and the Swift block comment say it directly: *"That is not a
shortfall to fix later, it is what the tier IS."* Rule rewrites —
`Kinetic.SkimDiscipline` (already consumed by `UBreakerAbility_Skim`),
`Multispell.Cycle`'s advance-on-hit, `VoidWhisperer.Lingering`'s refresh-instead-
of-stack, `Marksman.Angle`'s homing ricochets, `Spellblade.Blink`'s no-target
cast — change *which rule applies*, not a magnitude. They need a **consumer**,
not an enum entry, and §6 lists them separately for that reason.

---

## 5. Layering — how all of this obeys the existing law

### 5.1 The law is unchanged

Widening the vocabulary does **not** widen the aggregation law. From
`Power-Curve.md`, and locked:

```
value = (Base + sum(Flat)) * (1 + sum(IncreasedPercent)/100) * prod(More)
```

Flat sums first. Then **one additive Increased bucket per stat**, shared across
gear and tree. Then More multipliers as an unordered product.

**Every new stat target in §4 states its bucket, and that is a required
property, not documentation.** An entry without a declared bucket cannot be
wired, because the lane would have to guess.

### 5.2 The single More ceiling (O3 / O34)

`SingleMoreCeiling = 1.30`, `MaxComposedMoreSources = 3`, so the composed
ceiling is `1.30³ ≈ 2.197`, derived rather than restated. Enforced in three
places with one authority: the progression component picks its strongest three
and clamps each; the aggregator clamps the composed product globally; the combat
chain spends only the headroom the attribute side left. O34 deleted the second
budget and ruled that temporary ability windows count inside this one.

Consequences for the new vocabulary:

- **`IsMoreCappedAttribute` covers only `DamageMultiplier`.** Any new More lane
  is uncapped by default. **No stat target added in §4 may author a More
  without a canon row and a cap decision** — that is O34 §4a's gate, and it
  applies to `AbilityDamage` first, since a Caster keystone is the obvious place
  someone will want one.
- **`DamageOverTime` + `MorePercent` still does not compose.** `Long Dark`
  authors it legally today and the aggregator drops it with a warning. That is
  O34's explicitly-unruled DoT bucket question, not a bug to fix in passing.
- **Target-side lines are Increased-bucket only** (§3.3).

### 5.3 How much a conditional line should pay — owner ruling 4

*"conditionals should be flavour that add spice overall."*

This is the ruling most easily got wrong, and the wrong version is seductive:
each added condition makes a line pay less often, so the arithmetic says price
it higher, and composition says price a two-condition line higher still. **Do
not.**

The design standard:

- **The backbone is unconditional and ability scaling.** A build must be able to
  be strong while satisfying no conditions at all. O33 is explicit: *"a baseline
  player is viable understanding none of them … innovation is rewarded through
  conditions and rewrites, never required for viability."*
- **A conditional line is texture, not the route to power.** A build whose only
  path to competitiveness is holding three predicates at once is the failure
  mode, not the goal.
- **Composition does not multiply the payout.** A two-condition line is *more
  characterful*, not *more powerful*. If it needs to be worth more than a
  one-condition line at all, it should be by a little.

**`Power-Curve.md` authors no explicit conditional-payout ratio, and this
document does not either** — O2 freezes values. What is authored here is the
*direction*, so that when wave-mode measurement reports, whoever prices these
lines is pricing them against the right intent.

**A caution the numbers will create.** The band is 8–10x at cap. Conditional
lines with generous multipliers are the classic way a band quietly becomes 15x
in the hands of a player who satisfies everything at once, while the baseline
player sees none of it. `PowerBand` measures with `All()`, i.e. the best case —
so with target conditions in the set, that fixture now measures against a
hypothetical most-convenient enemy. **The fixture should be read as an upper
bound from the day the first target-conditional line is authored**, and that
is a change in what the number means, not just its value.

---

## 6. Wiring roadmap

Ordered. Each step is independently shippable and independently observable, and
the register in §4 makes progress countable.

**Stage 1 — composition and loudness (no new lanes).**
`AggregateStats` moves from `Conditions.IsActive(Effect.Condition)` to
`Conditions.SatisfiesAll(Effect.Condition, Effect.AlsoRequires)`. One line. It
buys composition and the warn-once path at once. The affix layer adopts
`SatisfiesOne` for the loudness alone.

**Stage 2 — the free lanes.** Six stat targets whose aggregated attribute
already exists and where the lane is one line each in `AggregateStats`:
`AbilityCost`, `MaxClassResource`, `ClassResourceRegen`, `FireRate`,
`DashCooldown`, `Armor`. This is the cheapest large unblock in the list and it
lights up `Multispell.Reservoir` — the node the design explicitly wanted a
number on.

**Stage 3 — the label switches.** `UI/BreakerMenu.cpp`'s two stat-label
switches both end in `default: return TEXT("STAT")`. Twenty-one new entries all
fall through it today. This is the same silent-fallthrough class as everything
else in §2.7 and should land with Stage 2, not after.

**Stage 4 — pilot ONE axis end to end.** CONTEXT.md's own instruction. The
recommended axis is **ability cost**, because it has an attribute, a hook (H14),
a real consumer, and four named inert nodes.

**Stage 5 — the recorder.** One component, four fields, four bindings to
existing delegates. Unblocks conditions 13–16 and the on-kill half of several
Caster nodes.

**Stage 6 — target-side.** §3.2 in full: the request fields, the rider table,
the `ReceiveDamage` recomposition, the O34 canon row and the conformance test.
This is the one that needs a ruling read before it starts.

**Stage 7 — the loop valve.** `ClassResourceDecay` through `PushLoopOverride`.
Unblocks the three Swift tier-4 downside nodes.

**Stage 8 — ability geometry.** `AbilityArea` / `AbilityDuration` /
`AbilityCooldown` need base-class accessors first (H15, H16). The largest
refactor in the list; deliberately last.

**Separately, and not gated on any of the above:** the rule-rewrite tags of
§4.7 need *consumers*, not enum entries. Those live in `Abilities/`,
`Weapons/` and `Classes/` and are tracked by the "STILL INERT" tag list in
`Abilities/BreakerAbilityStateComponent.h`.

---

## 7. Not in scope, and why

- **Elemental conditions and stat targets.** O38: Elements are post-slice. Four
  of six status tags are elemental; the affix pool already refuses elemental
  lines. Roughly five of the eight remaining condition bits are informally
  reserved for them.
- **Minion / deployable hooks and stat targets.** O30's third axis. Nothing
  exists in any form.
- **Stamina, and block/dodge as verbs.** O1: stamina is deleted forever, block
  and dodge are passive chance layers. There is no "while blocking" condition
  and there will not be one.
- **Per-hit outcome conditions.** §2.1 — hook payload or stat-target partition.
- **A general scripting hook.** §1.4 — it would end the re-derivability that
  respec, save-load and tooltip preview all depend on.
- **Balance values.** O2. Every constant introduced here is marked
  `O2 PLACEHOLDER`: `RecentEventSeconds`, `StationarySpeedThreshold`,
  `HighVitalFraction`, `LowVitalFraction`, `MultiStatusThreshold`,
  `CloseRangeCm`.
- **The DoT additive-bucket question.** O34 deliberately did not rule whether
  Increased Damage and Increased DoT share one bucket for DoT ticks. Until it
  does, `DamageOverTime` + `MorePercent` stays dropped-with-a-warning, and
  `Long Dark` stays unpaid.
- **Conditional healing and conditional status application.** §3.4 — the rider
  mechanism generalizes, but one axis piloted whole beats four half-wired.

---

## 8. Where the rulings pull against each other

Recorded rather than resolved. These are the owner's to settle.

1. **Ruling 3 (trees scale abilities) vs O3 (three Mores, one ceiling).** Making
   abilities a real scaling avenue invites an ability-damage More on a Caster
   keystone. But `IsMoreCappedAttribute` budgets only `DamageMultiplier`, so an
   `AbilityDamage` More would sit **outside** the 2.197 ceiling unless it is
   explicitly folded in. Either it joins the budget — and then it competes with
   the tree Mores a Caster already wants — or the single-ceiling claim of O34
   quietly stops being true. **Needs a ruling before Stage 4.**

2. **Ruling 4 (conditionals are spice) vs ruling 2 (target conditions).** Target
   conditions are the most *expensive* to build and the most *situational* to
   satisfy, which is precisely the combination that argues for pricing them
   high — and ruling 4 says do not. The resolution taken here is that target
   conditions buy **character**, not power, and that the cost in §3.3 is paid
   for expressiveness. If they end up priced as a power source, ruling 4 has
   been quietly reversed.

3. **Ruling 4 vs O27's "choices beat accumulation".** O27 moved power out of
   per-point accumulation and into node choices; the cleanest way to make a
   choice matter is to make it conditional. Ruling 4 caps how far that can go.
   Both are satisfiable — the choice can be *which unconditional axis* rather
   than *which condition* — but only if the widened stat targets carry the
   differentiation. **This is the argument for Stage 2 and Stage 4 preceding any
   large conditional authoring pass.**

4. **Ruling 2 vs O34's multiplier canon.** Target-side composition is a new lane
   touching outgoing damage. O34 requires a canon row and a conformance test
   before such a lane merges. Not a conflict — a gate, and §6 Stage 6 owns it.

5. **The widened enums vs O40(c) reachability.** Twenty-one stat targets and
   nineteen conditions now exist ahead of their consumers, which is exactly the
   shape O40(c) legislates against. The defence is that they are **loud** rather
   than silent (§2.7, and `BreakerStatTargetHasAggregationLane`), and that the
   alternative — widening the enum one entry at a time — would mean twenty-one
   separate append-only migrations of a value-serialized enum. It is a
   deliberate, recorded exception, and the two audit tests are the toll paid for
   it.

6. **`PowerBand` changes meaning.** §5.3: `All()` now includes target
   conditions, so the fixture measures against a hypothetical most-convenient
   enemy. The number will not move until a target-conditional line is authored —
   but on the day one is, the band figure silently becomes an upper bound. It
   should be relabelled before then, not after.
