# Class Kit — GUNSMITH (Scrap) — full treatment

> STATUS 2026-08-16: UNBUILT TREATMENT — nothing here reaches a player (this doc's own header audit stands); the owner authorized building all five classes as O2-placeholder implementations in chat on 2026-08-16, pending ratification — see Docs/Owner-Rulings-Pending-Ratification.md.

**Scope:** post-slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

> ## NOTHING IN THIS DOCUMENT IS BUILT. Not one line of it reaches a player.
>
> Verified against the code on 2026-08-14, because a full-depth treatment reads
> like a specification and this one is not yet one:
>
> - **The Gunsmith is selectable and grants nothing.**
>   `UBreakerProgressionLibrary::GetFallbackClassDefinition` returns `nullptr`
>   for every class but Swift, so a locked Gunsmith has no class definition, no
>   starting abilities, no ultimate and no branch trees.
> - **No Gunsmith tree exists.** `GetAllFallbackTrees` returns exactly four:
>   the Core slice and Swift's Frenzy, Kinetic and Marksman.
> - **No Gunsmith ability exists.** The ability fallback registry carries Swift
>   and Caster only.
> - **The Scrap loop does not exist.** `Source/RiorsEdge/Classes/` contains two
>   components — Momentum (Swift) and Mana (Caster). There is no Scrap
>   component and `ClassResource` is inert for a Gunsmith.
> - **Deployables and minions do not exist in any form** — no component, no
>   actor, no density cap, no lifetime, no targeting. **O30 records this
>   explicitly**: *"minions/deployables do not exist in any form. The Gunsmith
>   kit designs them; nothing is built."*
>
> **That last point makes this document unusually important and unusually
> exposed.** **O30** puts MINIONS — drones, turrets, deployables — on the Core
> tree's build-axis taxonomy alongside GUNS and ABILITIES, and **this document
> is the only place in the entire corpus that designs them.** So the minion axis
> O30 asks the Core tree to be organised around has its design here and its
> implementation nowhere, and `Core-Tree-Redesign.md` costs it as the single
> most expensive part of the redesign. Nothing here should be trusted as
> validated; everything here should be read as the standing proposal.

Status: design draft, **UNBUILT** (see above), authored to the depth of Class-Kits §1 (Swift) and §2 (Caster).
This document **extends** Class-Kits §3's one-page Gunsmith treatment. It does not
contradict it: §3's resource table, anti-farm rules, density cap, branch identities, six
abilities, ultimate, keystone names, and acceptance criteria are all carried forward
verbatim or widened. Where this document adds a rule §3 did not state, the addition is
marked **EXTENDS**.

> **EVERY MAGNITUDE IN THIS DOCUMENT IS AN O2 PLACEHOLDER.** Flagged once, here, and not
> repeated per line. No number below is authored — every one is a plausible seed chosen to
> sit against O18's targets (trash a little under 1s, scaling exponentially with difficulty;
> rare/elite ~3s; boss 20–45s unless special; TTD 4–5s with no resources/sustain,
> substantially higher with sustain invested) and is re-anchored only when wave-mode
> instrumentation reports. Structure is the deliverable; magnitudes are the freeze.

**Rulings this document obeys.** O1 (no stamina; block/dodge are passive chance layers;
Parry is the only defensive input). O2 (value freeze, above). O3 (More multipliers are an
unordered product; build-wide hard cap 3; trees author them only on branch keystones;
one per branch). O11 (Aberrant signatures may not author a More). O13 (self-damage
reduction, never immunity). O18 (seed targets). O19 (elements are Rift / Entropy / Void).
Layer-Ownership (**no node in this document is a flat percentage** — every node is a rule
rewrite or a resource-loop modifier; percentages belong to the affix layer).

**Three later rulings this document has not been written against.**

- **O28.** `Master-Sheet-Import.txt` is **superseded** and has lost its standing
  — historical source material, never authority. Every "Master N.N" citation
  below records provenance, not law. The chain is `Decisions.md` ->
  `Design-Overview.md` (map) -> this document.
- **O30.** The minion axis is this document's, and it is unimplemented (see the
  banner). One structural consequence beyond that: **`EBreakerBuildCondition`
  is movement-only**, so no node anywhere — Core or class — can key off combat
  or status state. Several Field Tech and Tinkerer nodes below assume a
  deployable-state condition exists. It does not, in any form.
- **O31.** Every build must be able to make an impact; no encounter may have a
  build that cannot participate. **The Gunsmith is the class most exposed to
  this**, because it is the game's one *pre-commitment* class: its power is
  placed before contact. An encounter that moves the fight, denies placement
  time, or destroys deployables faster than they pay for themselves does not
  weaken a Field Tech build — it deletes it. **Armory exists partly as the
  answer** (§0: "you were never committed; your power was on your gun the
  whole time"), and under O31 that stops being a flavour distinction and
  becomes the class's participation floor. Recorded, not designed against.

**Engineering home.** `Docs/Design/Ability-Implementation-Spec.md` — D1 (tag-driven
keystone ultimate variants), D2 (ability archetype hierarchy), D3 (cost/cooldown as
GameplayEffects), D5 (replication posture), D7 (Mores are not authored on abilities),
§2 (SI-1…SI-10 shared infrastructure), §6 (the Gunsmith scaffold and the deployable
system). Every "needs" line below names hooks from that document's missing-hook list.

---

## 0. Fantasy and the shape of the class

**Fantasy (Class-Kits §3, carried):** the battlefield is a workshop. The Gunsmith's power
is *placed* rather than held, and their weakness is that placement takes time they may not
have.

Stated as a play pattern: every other class's power arrives when the button is pressed.
The Gunsmith's power arrives *where they predicted the fight would be*. A Gunsmith who
reads the room correctly fights a battle that was already half-won before contact; a
Gunsmith who reads it wrong has spent their whole resource pool on furniture behind them.
The class is the game's only **pre-commitment** class, and the tree is three answers to the
question "what do you do when you were wrong":

- **Armory** — you were never committed. Your power was on your gun the whole time.
- **Field Tech** — you commit, but the commitment is durable and pays its own bills.
- **Tinkerer** — you commit *cheaply and often*, and being wrong costs one trap, not a
  fight.

**Legibility in the first hour** (Master 7.5's requirement, since class choice is
permanent): a level-1 Gunsmith holds Sidearm Rig and Turret. Press one, your gun gets
better for a magazine. Press the other, a machine appears and shoots for you. The class
reads immediately and the two starters sit on opposite sides of the class's central
tension.

---

## 1. The Scrap loop

Scrap is a 0–100 bar with **no decay and no passive regeneration**. It is the only resource
in the game with zero idle generation, and the reason is structural, not flavour:
**deployables persist until destroyed or expired.** Idle generation on a class whose spend
converts into persistent world objects means free permanent power accumulating out of
combat. Scrap is therefore strictly a *conversion* of combat activity into placed
structures.

The Caster's Mana is a wallet. Swift's Momentum is a state. **Scrap is neither — it is a
ledger of work already done.** You cannot save toward a fight you have not started.

### 1.1 Generation table

Carried from Class-Kits §3, with per-source ICDs and anti-farm rules made explicit.
Every generation event carries a **proc coefficient** per Master 6.4 and the shared
resource grammar in Class-Kits §0.3.

| Source | Rate | ICD | Cap / anti-farm rule |
|---|---|---|---|
| **Kill** | +12 | none | Flat, per kill. Proc coefficient applies to the *killing instance* — a DoT tick that lands the kill credits at its proc coefficient, not 1.0, so Tick Frequency cannot become a Scrap engine. Deployable kills credit at the deployable's own proc coefficient (§1.3). |
| **Reload completed** | +4 | once per reload | Cancelled reloads credit nothing. A reload interrupted and restarted credits once, on completion. **Anti-farm:** reloading a full magazine credits nothing — at least one round must have left the magazine since the last credited reload. |
| **Emptying a magazine before reloading** | +8 *additional* | once per magazine | Fires on the last round leaving the magazine, not on the reload. Requires the magazine to have been *full at the start of the cycle* — topping off at 1/30 and firing one round does not re-arm this. Rewards commitment to a full dump. |
| **Deployable destroyed (yours)** | +50% of its Scrap cost | none | **Refund, not profit.** Applies identically whether the deployable expired, was destroyed by an enemy, was replaced by the density cap, or was recalled by a node. There is exactly one destruction path so there is exactly one refund path. |
| **Damage dealt by your deployables** | +1 per 500 damage dealt | 0.5s, **per deployable** | The solo self-sufficiency line: a placed turret pays for the next one. Per-deployable ICD, not global, so four turrets are four independent trickles — but the global cap below still binds them. **Anti-farm:** overkill damage does not count; a hit is credited at the damage actually applied to health/shield, matching the post-mitigation principle O-ruled for Tank's Grit. |
| **Ammo reserve consumed by an Ammo Crate refill** | 0 | — | Explicitly zero. Named here so nobody adds it later: a crate that generated Scrap when used would be a loop with no enemy in it. |

**Global generation cap: 15 Scrap/s from all sources combined.** Lower than Swift's 25/s
and Caster's 20/s because Gunsmith generation is kill- and damage-weighted and a dense pack
would otherwise refund a full deployable set mid-wave.

**Sources that deliberately do not exist**, each named so the absence is a decision rather
than an omission:

- No generation from *placing* a deployable, from *time* a deployable is alive, or from a
  deployable merely existing. Uptime must never be income.
- No generation from damage *taken*. That is Grit's loop and Gunsmith must not shade into
  it.
- No generation from ally actions. Scrap is ally-free by construction (§5).
- No generation from Ammo Crate self-use, per the table.

### 1.2 Cap, banking, and decay behaviour

- **Cap: 100 base.** Raised by the universal `Maximum Resource` core affix (Master 3.2),
  which is the affix layer's business and not a node's.
- **No decay, ever** — in or out of combat. Scrap earned in one encounter is spendable in
  the next. This is deliberate and is the compensation for zero idle generation: the class
  cannot *build* out of combat but it does not *lose* what it built.
- **Overflow is discarded, not banked.** Generation above 100 is lost. There is no
  stored-surplus mechanic and no node grants one — a bank-above-cap rule would let a
  Gunsmith arrive at a boss with two full deployable sets, which is the failure mode
  Master 7.10.1 names.
- **Scrap is not refunded on death or on leaving a rift.** It persists at its current
  value. Deployables do not (§4.6).
- `Resource on Kill`, `Resource on Damage Taken`, and `Resource Cost Reduction` (Master 3.9)
  apply as they do to all five classes. Cost reduction is the additive Increased bucket and
  never creates a More.

### 1.3 Deployable damage attribution — EXTENDS

A rule the one-page treatment implied but did not state, and every Field Tech node depends
on it:

**Damage dealt by a Gunsmith's deployable is the Gunsmith's damage.** The player is the
ultimate instigator. The player's affixes, crit stats, statuses, and elemental affinities
apply. The deployable is a *delivery mechanism*, not a pet with its own stat block.

Three consequences, all load-bearing:

1. **Proc coefficient.** Deployable damage instances carry a proc coefficient below 1.0
   (seed: 0.5 for continuous sources such as Turret fire, 1.0 for one-shot sources such as
   a Mine detonation). Without this, a four-turret field is a proc engine for every
   on-hit affix in the game.
2. **Scrap generation.** The deployable-damage source in §1.1 is the same event, so a
   deployable cannot out-generate the player's own gun per unit of damage.
3. **Kill credit.** Deployable kills are the player's kills for the +12 Kill source, for
   XP, and for `Resource on Kill`. They credit at the deployable's proc coefficient.

**Acceptance rule (from §3, restated as an invariant):** over a deployable's full lifetime,
against a stationary target, its damage must not generate more Scrap than the deployable
cost. Placement is an investment with a positive *combat* return and a negative *economic*
one. If that inverts, the class farms itself.

### 1.4 Scrap states — EXTENDS

Swift reads bands (Settled / Running / Redline) and several nodes key off them. Gunsmith's
fantasy wants bands too, but for the opposite reason: Swift's bands describe *how fast you
are going*; Gunsmith's describe *how much you can still afford to be wrong*. Three states,
displayed on the HUD as distinct (same HUD requirement as Momentum's bands, SI-4):

| Band | Range | Name | Reads as |
|---|---|---|---|
| Low | 0–24 | **Dry** | You cannot place anything meaningful. Armory play only. |
| Mid | 25–59 | **Stocked** | One deployable in hand, or two cheap ones. The normal state. |
| High | 60–100 | **Surplus** | A full field, or the ultimate is within reach. |

Bands are read by nodes (AR2, FT4, TK1, TK10 below) and by the HUD. They are display and
threshold only — no band grants a stat.

**Design intent:** the Gunsmith should spend out of Surplus into Stocked constantly.
A Gunsmith who sits at 100 is playing the class wrong and the tree should make that
uncomfortable, not punish it mechanically. No node applies a penalty for being full;
several nodes reward the moment of spending.

### 1.5 Spending

Carried from §3 and made a hard split:

- **Deployable abilities cost Scrap (25–60) and have NO cooldown.** Scrap is the cooldown.
- **Personal abilities have cooldowns (8–20s) and cost NO Scrap.**

This is the class's defining ergonomic and it is why Gunsmith is listed alongside Caster in
Class-Kits §0.3 as a "spending is the only gate" class — for half its kit. The split means a
Gunsmith's two equipped abilities are gated by *two different clocks*, and choosing one of
each is a real loadout shape rather than a default.

Per the ability spec D3: deployable abilities author **no cooldown GameplayEffect at all**.
Do not author an empty one — the HUD must distinguish "no cooldown, cost-gated" from
"cooldown of zero."

### 1.6 Solo generation check

Every source in §1.1 requires enemies and none requires an ally. Kill, reload, magazine
dump, deployable damage, and deployable destruction are all producible by one player alone
in an empty room with one target dummy. Class-Kits §6.5 already records Gunsmith as
"no idle generation, no target-free generation, **yes** ally-free generation," and this
document does not change that row. Full treatment in §5.

---

## 2. The deployable system — design rules

Deployables are Gunsmith's signature. Engineering home is Ability-Implementation-Spec §6
(`ABreakerDeployable`, `UBreakerDeployableComponent`, `PlaceDeployable`,
`PushDensityCapOverride`, `OnDeployablePlaced` / `OnDeployableDestroyed`). This section is
the *design* those systems implement.

### 2.1 Density cap

**4 active total, 2 of any one type.** Carried verbatim from §3. Enforced by the owning
component (per Character-Progression-Architecture), never by the ability.

- Placing a fifth **destroys the oldest and refunds it** at the standard 50% rate. It does
  not fail, and it does not prompt. The class should never be blocked by its own furniture.
- "Oldest" is placement order, not remaining lifetime. A recently-refreshed turret is still
  old.
- The per-type cap of 2 is what stops turret-stacking from being the only build. Every
  Field Tech node that touches density touches the *total*, never the per-type cap —
  **no node in this document raises the per-type cap above 2.** The ultimate raises the
  total to 8; per-type stays 2. That is the invariant the acceptance criteria test.

### 2.2 Lifetime

| Class of deployable | Seed lifetime | Expiry behaviour |
|---|---|---|
| Turret (active damage) | 30s | Expires, refunds 50%. |
| Ammo Crate (utility) | 45s or on charges exhausted, whichever first | Expires, refunds 50%. |
| Mine Cluster (armed trap) | 60s or on all charges triggered | Expires, refunds 50%. |
| Disruptor (zone) | 20s | Expires, refunds 50%. |

Traps live longest and cost least; active damage lives shortest and costs most. That
gradient is the branch identity expressed in the lifetime column: Field Tech pays for
presence, Tinkerer pays for patience.

**Lifetime is paused, never extended, by the Foundry keystone** (`SetLifetimePaused`).
"Never expires" is a pause on the lifetime clock, so the deployable still dies to damage
and still refunds through the one destruction path.

### 2.3 Placement and targeting

- **Placement is a server-side sweep plus a line-of-sight trace from the placement point.**
  Never inside geometry; never outside LOS of where the player stood. It **fails loudly**
  rather than silently relocating — a deployable that quietly slides two metres is worse
  than one that refuses.
- A failed placement **costs nothing**. No Scrap, no ability activation.
- Placement range: seed 8 m along the aim ray, snapping to the nearest valid floor within
  1.5 m of the impact point. Deployables are floor objects; there is no wall or ceiling
  placement in this design.
- **Placement has a cast time** (seed 0.4s deploy animation, uninterruptible but not
  action-locking — the player may move, not fire). This is the mechanical expression of
  "placement takes time they may not have." Without it the class's stated weakness does not
  exist.
- Deployables **do not move**. Nothing in the tree makes them move. A moving turret is a
  pet, and pets are a different class fantasy with a different AI budget.

### 2.4 How enemies interact with deployables

This is the half of the system that keeps deployables from being free.

- **Deployables are damageable, targetable actors** with their own health pool. They do not
  inherit the player's health, armour, or resistances; their durability is authored per
  type (all values O2 placeholders).
- **Enemies target deployables opportunistically, not preferentially.** A deployable is a
  valid target when it is the nearest threat or when the enemy has no line of sight to the
  player. It never *pulls* aggro off the player as a threat mechanic — threat manipulation
  is the Tank's verb (Provoke, `ForceTarget`), and Gunsmith does not get it for free through
  furniture. This is a deliberate boundary against Bastion.
- **Deployables do not block enemy movement or projectiles.** They have a small collision
  footprint for placement validation only. Cover is Anchor Point's job (Tank/Bastion), and
  a deployable that doubles as cover would take that branch's identity.
- **Bosses and Champions ignore deployables entirely** unless a deployable is the only
  valid target. A boss stopping to shoot a crate is a 20–45s fight becoming a 60s fight for
  the wrong reason (O18).
- **AoE cleaves deployables.** Any enemy attack with a radius damages deployables in it at
  full value. That is the main way a field dies, and it is what makes placement *position*
  matter rather than just placement *count*.
- **Deployables do not take DoT or status effects.** They take direct damage only. No
  bleeding turrets; the status system is not asked to model objects.

### 2.5 Feedback requirements

The HUD (SI-4) must show, per the density cap: active count / cap, per-type count, and each
active deployable's remaining lifetime. A class whose power is placed must be able to see
what it has placed without turning around. Deployables carry a persistent world marker
visible through geometry at reduced opacity, owner-only.

---

## 3. Abilities (6) + ultimate

Starters, free at level 1: **Sidearm Rig** (Armory) and **Turret** (Field Tech). Both
ability slots are filled at level 1 and the two starters sit on opposite sides of the
class tension (§0).

GAS archetypes are from Ability-Implementation-Spec D2:
`_Instant` / `_Window` / `_Zone` / `_Deployable` / `_Channel` / `_Ultimate`.

| # | Ability | Branch | Cost | CD | Archetype |
|---|---|---|---|---|---|
| G1 | Sidearm Rig *starter* | Armory | — | 10s | `_Window` |
| G2 | Overhaul | Armory | — | 18s | `_Window` |
| G3 | Turret *starter* | Field Tech | 40 Scrap | — | `_Deployable` |
| G4 | Ammo Crate | Field Tech | 30 Scrap | — | `_Deployable` |
| G5 | Mine Cluster | Tinkerer | 35 Scrap | — | `_Deployable` |
| G6 | Disruptor | Tinkerer | 45 Scrap | — | `_Deployable` + `_Zone` |

### G1 — Sidearm Rig *(starter, Armory)*

**Cost:** none. **Cooldown:** 10s. **Archetype:** `UBreakerGameplayAbility_Window`,
`LocalPredicted`.

The next magazine's worth of shots deal bonus **flat** damage and gain +1 Pierce. The
window ends when the magazine is emptied or on reload, whichever comes first — it is
counted in *shots*, not in seconds, which is what makes it a magazine-economy ability
rather than a burst window.

The flat bonus lands in the **flat sum** stage, before the additive Increased bucket, per
Item-Foundation's locked aggregation order. It must not be expressible as a percentage and
must not double-dip with Damage Ramp (the same warning S2 Cadence Break carries).

Pierce granted here is a *rule*, stacking additively with the Pierce affix to the same
max +3 total that Sightline (S5) obeys. One pierce ceiling for the whole game.

**Engine hooks:** `FBreakerPendingShotModifier` with `FlatBonusDamage`, `AdditionalPierce`,
and `ShotsRemaining` = current magazine size (the per-shot modifier hook from spec §4.5 —
Sidearm Rig reuses it entirely, no new hook). `OnReloadChanged` to close the window early
(exists). SI-9 window state.

### G2 — Overhaul *(Armory, granted by AR7)*

**Cost:** none. **Cooldown:** 18s. **Archetype:** `_Window`, `LocalPredicted`.

For 10s, reserve ammunition is converted into magazine capacity at a fixed ratio (seed
3 reserve : 1 magazine round, drawn on activation up to a cap of +100% of base magazine
size). Rounds are debited from reserve on push and any unspent conversion is settled back
to reserve on pop.

The ability is a bet: you trade your *sustain* for your *burst*, and if the fight ends
before the window does you gave up reserve for nothing. That is Armory's whole grammar —
personal, immediate, and self-limiting — and it is the Gunsmith ability that most rewards
knowing how long an encounter has left.

**Engine hooks:** **MISSING** — `UBreakerWeaponComponent::PushMagazineCapacityOverride(FGameplayTag, int32 Delta)`
/ `Pop`, with reserve debited on push and settled on pop (spec §6 G2). Must interact
correctly with an in-flight reload: a reload completing during the window fills to the
overridden capacity; a reload completing after pop fills to base.

### G3 — Turret *(starter, Field Tech)*

**Cost:** 40 Scrap. **Cooldown:** none. **Archetype:** `_Deployable`, `ServerOnly`.

Autonomous emplacement, 30s lifetime. Acquires the nearest valid target within its range
(seed 18 m) with line of sight, fires at a fixed cadence, re-acquires on target death or
LOS loss after a short reacquire delay (seed 0.4s). It does not lead moving targets
perfectly and it does not prioritise weak points — a turret is *consistent*, never
*optimal*. That gap is the reason the player still holds a gun.

All damage is the player's damage per §1.3, at proc coefficient 0.5.

**Engine hooks:** deployable system (spec §6, the largest new system after melee).
Autonomous target acquisition. Instigator attribution so `OnHitDealt` fires with the
player as source (SI-8). `FBreakerDamageRequest::Instigator` (spec §7 missing hook,
shared with Tank).

### G4 — Ammo Crate *(Field Tech, granted by FT8)*

**Cost:** 30 Scrap. **Cooldown:** none. **Archetype:** `_Deployable`, `ServerOnly`.

Places a crate with a fixed number of interact charges (seed 4). Each interact refills a
portion of reserve ammunition (seed 40% of base reserve) to the interacting player. Fully
solo-usable — the Gunsmith is a valid interactor with their own crate, at full value, with
no self-penalty. A shared charge pool means a party drains it faster, which is an
efficiency difference and never a solo penalty (the Support §5 rule generalised).

Generates **no Scrap** on use, per §1.1.

**Engine hooks:** deployable system; interact prompt (reuses the NPC interaction prompt
pattern in `ABreakerCharacter::FindNearbyNPC`); a reserve-ammo grant path on
`UBreakerWeaponComponent`.

### G5 — Mine Cluster *(Tinkerer, granted by TK7)*

**Cost:** 35 Scrap. **Cooldown:** none. **Archetype:** `_Deployable`, `ServerOnly`.

Scatters 3 proximity charges around the placement point (seed 2 m spread). Each arms after
a delay (seed 1.0s), then detonates on an enemy entering its trigger radius (seed 2.5 m),
dealing radial damage with falloff. The cluster counts as **one** deployable against the
density cap, not three — the cap counts *placements*, not *entities*, and Tinkerer's whole
identity depends on that.

Trigger logic is **data-driven, not hardcoded**: Tinkerer nodes rewrite trigger conditions
(proximity → line-of-sight → damage-taken → manual) and rearm behaviour, so the condition
must be a swappable data field on the deployable definition from day one.

**Engine hooks:** deployable system; data-driven trigger conditions; radial damage with
falloff (reuses `ABreakerRocketProjectile`'s falloff).

### G6 — Disruptor *(Tinkerer, granted by TK8)*

**Cost:** 45 Scrap. **Cooldown:** none. **Archetype:** `_Deployable` + `_Zone`,
`ServerOnly`.

Places an emitter projecting a field (seed 6 m radius, 20s lifetime). Enemies inside are
slowed and have their Armour reduced by a **flat** amount. Flat, not percentage — the same
protection Rot's armour reduction uses against the boss-scaling failure in Master 7.10.5.
Armour never goes negative, and two Disruptors do not stack their reduction (the larger
applies; the anti-stack rule Rot already carries).

The Gunsmith's only crowd-control ability, and it is the answer to "Tinkerer requires
knowing where the enemy will be" — a Disruptor makes the prediction easier by making the
enemy slower, which is why it is the branch's expensive ability rather than its cheap one.

**Engine hooks:** deployable + `ABreakerZoneActor`. **MISSING** —
`UBreakerCharacterMovementComponent::PushSpeedMultiplier(FGameplayTag, float)` / `Pop` on
the *enemy* movement component. `PushArmorReduction` with a floor (spec §5.3, shared with
Rot).

### ULTIMATE — FIELD ASSEMBLY

**Cost:** 100 Scrap (full bar). **Cooldown:** none — the cost is the cooldown.
**Archetype:** `UBreakerGameplayAbility_Ultimate`, `ServerOnly`.

**Base behaviour** (carried from §3): deploys **all currently unlocked deployable types at
once**, each at no individual Scrap cost, at valid positions around the player, and raises
the density cap to **8 total for 20s** (per-type cap stays 2). Deployables placed by Field
Assembly are ordinary deployables in every other respect: they expire, they die, they
refund.

"Currently unlocked" means types the character has purchased the granting node for — not
types equipped. This is the one place the class rewards tree breadth over loadout choice,
and it is why a Gunsmith with nodes in all three branches has a distinctly different
ultimate from a specialist.

When the 20s window ends, the cap returns to 4. Deployables **over** the cap at that moment
are not destroyed — the cap is checked on *placement*, not continuously — but the next
placement destroys the oldest as normal until the field is back inside 4. Stated explicitly
because "does the field get culled at window end" is the first question implementation will
ask, and culling would make the ultimate feel like a punishment at its own expiry.

**Keystone rewrites** (D1 tag-driven variants — one ability asset, variant selected from
the owner's `Keystone.Gunsmith.*` tag; a character holds at most one keystone, so selection
is a lookup):

- **Machinist** (Armory, `Keystone.Gunsmith.Machinist`) — Field Assembly places nothing.
  Instead, for 20s, **every unlocked deployable type's effect is applied to the player's
  own weapon and person**: Turret becomes a weapon-damage rider, Ammo Crate becomes
  continuous reserve regeneration, Mine Cluster becomes an on-kill radial detonation,
  Disruptor becomes an aura on the player. The solo / no-deployable ultimate, and the
  ultimate that makes a zero-placement Armory build complete rather than partial.
- **Foundry** (Field Tech, `Keystone.Gunsmith.Foundry`) — deployables placed during the
  window **never expire** (lifetime clock paused, `SetLifetimePaused`). They still die to
  damage and still refund. The cap still returns to 4 at window end, so the field is
  permanent but bounded — and every subsequent placement culls one of the permanent ones,
  which is the pressure that stops Foundry from being pure accumulation.
- **Minefield** (Tinkerer, `Keystone.Gunsmith.Minefield`) — deployables placed during the
  window are **invisible until triggered** (or until they first deal damage), and are
  excluded from enemy perception while invisible. They cannot be shot before they act. The
  ambush ultimate.

**Engine hooks:** `PushDensityCapOverride` / `Pop` (spec §6). `SetLifetimePaused` (Foundry).
Visibility flag plus a server-side AI perception exclusion (Minefield). **Machinist's
per-deployable-type "effect as weapon modifier" mapping table is the most bespoke of all
fifteen keystone rewrites** and the spec already flags it as most likely to need its own
design pass — this document authors the mapping's *shape* (one entry per deployable type,
authored on the deployable definition itself so a future deployable cannot ship without
one) but not its magnitudes.

---

## 4. Branches

Shared five-tier grammar from Class-Kits §0.2, unchanged:

| Tier | Gate | Nodes | Cost/rank | Max ranks |
|---|---|---|---|---|
| 1 — Entry | 0 | 3 | 1 | 2 |
| 2 — Loop | 3 | 3 | 1 | 2 |
| 3 — Ability | 6 | 2 | 2 | 1 |
| 4 — Rewrite | 10 | 3 | 2 | 1 |
| 5 — Keystone | 16 | 1 | 4 | 1 |

12 nodes, 26 points per branch. Exactly one keystone per branch, at Tier 5, and it is the
only node in the branch permitted to author a More (O3). All three Gunsmith Mores are
authored below; **the class's budget of three is fully spent and no further Gunsmith node
may author one.**

### 4.1 ARMORY

**Identity.** Armory is the branch that never places anything. It is the Gunsmith's
personal weapon: magazine, reserve, reload, and the rules governing how ammunition converts
into damage. Where the other two branches answer "what did you build," Armory answers
"what did you build it *into*" — the gun in your hands is the project.

Armory exists for a structural reason as much as a fantasy one: it is the **solo baseline**
and the proof that a Gunsmith with zero deployables placed is still a complete shooter
(Class-Kits §3, acceptance criterion 1). Every other branch may assume a field; Armory may
assume nothing but a magazine. It is also the branch that most rewards weapon-archetype
identity from the affix layer, because every node in it reads magazine size, reserve, or
reload as a rule rather than as a number.

The tension inside Armory is **magazine versus reserve**. Nearly every node moves value
across that boundary in one direction, and a full Armory build is a decision about which
side of it you want to be rich on.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **AR1 — Field Stripping** | 1 | 2 | 1 | Reload-completed Scrap generation also fires on a reload that began with rounds still in the magazine, provided at least one round was fired since the last credit. R2: the "magazine was full at cycle start" requirement on the magazine-dump source is removed. Loop modifier that opens the reload economy to tap-fire play. |
| **AR2 — Working Stock** | 1 | 2 | 1 | While **Dry**, weapon reload speed is treated as one tier faster for any affix that reads reload tier; if the player has no such affix, treat as tier 1. R2: also while **Stocked**. Reads a band and an affix tier; grants no percentage. *(Same shape as Swift K8 — see the compliance note in §7.)* |
| **AR3 — Chambered** | 1 | 2 | 1 | The first shot fired after a completed reload consumes no ammunition. R2: and cannot be interrupted by the weapon's own cadence gate — it fires the instant the reload ends. A rule rewrite of the reload-to-fire boundary. |
| **AR4 — Deep Pockets** | 2 | 2 | 1 | Reserve ammunition picked up above your maximum reserve is converted into Scrap at a fixed rate instead of being discarded (R2: doubled rate). Respects the 15/s global cap and cannot fire more than once per pickup. **Anti-farm:** requires the reserve to be at maximum at pickup time, so it cannot be gamed by dumping ammo. |
| **AR5 — Last Round** | 2 | 2 | 1 | The magazine-dump Scrap source also fires when the magazine drops to its **last round** rather than to zero, and Sidearm Rig's window does not end on that round. R2: the last round is not consumed by firing. Rewrites where the "end of magazine" boundary sits. |
| **AR6 — Cold Barrel** | 2 | 2 | 1 | Sidearm Rig's cooldown is reduced by 1.5s each time you complete a reload with an empty magazine (R2: 2.5s), once per reload. Ties the personal-ability clock to the ammunition loop rather than to real time. |
| **AR7 — Bench Work** | 3 | 1 | 2 | **Grants G2 Overhaul.** Overhaul's conversion also applies to the *next* magazine loaded after the window ends, at half the converted capacity. The window has a tail, so ending it is not a cliff. |
| **AR8 — Rig Discipline** | 3 | 1 | 2 | Sidearm Rig's window is measured in shots rather than the magazine: it persists across one reload, ending only when its shot count is spent. Rewrites the window's boundary condition — the node that makes Armory's starter scale with build rather than with magazine size. |
| **AR9 — Reciprocal** | 4 | 1 | 2 | `Ammo Returned on Kill` triggers also generate Scrap, and Scrap generated this way ignores the global per-second cap. The explicit affix-to-class bridge (the F8 pattern): the class layer *reads* the affix layer without duplicating it. If the player has no such affix, this node does nothing — and that is correct. |
| **AR10 — Overpressure** | 4 | 1 | 2 | Overhaul's conversion ratio inverts: magazine capacity is converted into **reserve** instead, and while the window is active every shot fired restores a portion of reserve. The bet reversed — the sustain ultimate for a build that has burst elsewhere. |
| **AR11 — No Reserve** | 4 | 1 | 2 | Your maximum reserve ammunition is halved, and all Scrap generation from reload and magazine sources is doubled. The cost-for-power rewrite with a real downside, and the node that makes Armory read as a class choice rather than a bonus (the F11 pattern). |
| **AR12 — MACHINIST (keystone)** | 5 | 1 | 4 | Rewrites Field Assembly (§3, Machinist). **More multiplier (1 of 3):** while you have **no deployables active**, weapon damage is multiplied by **1.25**. The condition is the tax and it is also the thesis: Armory's More is the only one in the game that is *turned off by using your own class's signature system*, which is exactly what makes a placement-free Gunsmith a real build instead of a handicap. |

### 4.2 FIELD TECH

**Identity.** Field Tech builds machines that work while you work. It owns turrets, crates,
and pylons — the deployables that *produce* rather than *punish*. Its nodes rewrite
lifetime, targeting, density, and the refund economy: the branch's whole argument is that
a placement should pay for itself and then some, in effect if never in Scrap.

Field Tech is the branch that most changes how a fight is *shaped*. Armory changes your
damage; Tinkerer changes where the enemy can go; Field Tech changes how many things are
shooting. It is also the branch with the highest skill ceiling in *positioning*, because
every node makes a well-placed emplacement better and none of them makes a badly-placed one
survive.

The tension inside Field Tech is **density versus durability**: nearly every node offers
either more machines or better machines, and the density cap means you cannot take every
one of both.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **FT1 — Salvage** | 1 | 2 | 1 | Deployable destruction refunds 65% of cost instead of 50% (R2: 80%). Refund, never profit — the ceiling is hard-capped below 100% and no node in this document raises it above 80%. Loop modifier on the refund economy. |
| **FT2 — Overwatch** | 1 | 2 | 1 | Your turrets prioritise the target *you* last damaged, if it is in range and line of sight, instead of the nearest target (R2: and re-acquire instantly on that target's death rather than after the reacquire delay). Rewrites targeting; grants no damage. |
| **FT3 — Second Shift** | 1 | 2 | 1 | Deployables gain 8s of lifetime each time you complete a reload within their radius (R2: 14s), once per deployable per reload, and never beyond double their base lifetime. The cap is the anti-farm rule — reload-cycling in a corner cannot make a permanent field. |
| **FT4 — Tithe** | 2 | 2 | 1 | While **Surplus**, deployable-damage Scrap generation ignores the global per-second cap (R2: and its per-deployable ICD drops from 0.5s to 0.3s). Band-gated loop modifier — it accelerates the top of the bar, where the ultimate lives, not the bottom. |
| **FT5 — Requisition** | 2 | 2 | 1 | When a deployable is destroyed by an **enemy** (not by expiry, not by the density cap), the refund is paid immediately and the next deployable of that type placed within 8s costs 10 less Scrap (R2: 18 less). Compensates for being punished, never for being efficient. |
| **FT6 — Foreman** | 2 | 2 | 1 | Ammo Crate charges also restore a portion of the interactor's health (R2: doubled portion), and the crate's charges are consumed at half rate when the interactor is at full reserve. Field Tech's only sustain and the reason the branch is playable solo without an Armory dip. |
| **FT7 — Emplacement** | 3 | 1 | 2 | Turret's targeting range and reacquire behaviour are rewritten: it acquires through your own crosshair's target priority and holds a target through brief LOS breaks (seed 1.2s of grace). Sharpens the starter rather than granting a new ability. |
| **FT8 — Logistics** | 3 | 1 | 2 | **Grants G4 Ammo Crate.** Ammo Crate no longer counts against the density cap. The branch's utility deployable stops competing with its damage deployables — the single most build-enabling node in Field Tech. |
| **FT9 — Redundancy** | 4 | 1 | 2 | The density cap rises from 4 to 5 total. Per-type stays 2. The only node in this document that touches the total cap, deliberately priced at Tier 4 in one branch. |
| **FT10 — Automation** | 4 | 1 | 2 | When a turret's target dies, the turret immediately fires a free burst at its next acquired target, at the proc coefficient of a one-shot source. Rewrites the reacquire dead time into a payoff; bounded by requiring a kill. |
| **FT11 — Deadman** | 4 | 1 | 2 | A deployable destroyed by an enemy detonates for radial damage before refunding, at the *player's* proc-coefficient rules. Turns the branch's failure state into its own payoff. Cannot chain: a Deadman detonation that destroys another deployable does not trigger that deployable's Deadman. |
| **FT12 — FOUNDRY (keystone)** | 5 | 1 | 4 | Rewrites Field Assembly (§3, Foundry). **More multiplier (2 of 3):** damage dealt by your deployables is multiplied by **1.30**. Deployable-only is the tax; it is the largest of the three because it applies to the least of the player's own output, and because a Foundry Gunsmith spends their whole loadout paying for it. |

### 4.3 TINKERER

**Identity.** Tinkerer is prediction made mechanical. It owns traps, mines, and disruption:
deployables that do nothing at all until an enemy makes a mistake, and everything at once
when they do. It is the branch that requires knowing where the enemy will be, and the
branch whose nodes rewrite *trigger conditions* and *rearm behaviour* rather than damage or
lifetime.

Tinkerer is the cheapest branch to be wrong in and the most rewarding to be right in. A
mis-placed Tinkerer field costs one trap; a well-placed one deletes an approach. It is also
the branch with the most explicit anti-synergy with the Gunsmith's own weakness — placement
takes time — because Tinkerer's placements are made *before* the fight, in the corridor
behind you, at the door you know they use.

The tension inside Tinkerer is **arm time versus reward**: nodes that make traps trigger
faster and rearm cheaper compete with nodes that make a single, patient, fully-armed
trigger devastating.

| Node | Tier | Ranks | Cost/rank | Effect |
|---|---|---|---|---|
| **TK1 — Cheap Work** | 1 | 2 | 1 | While **Dry**, Tinkerer deployables cost 10 less Scrap (R2: 18 less), to a floor of 10. Cost reduction folds into the additive Increased bucket and never creates a More; the band gate means it rescues a broke Gunsmith rather than subsidising a rich one. |
| **TK2 — Quick Set** | 1 | 2 | 1 | Mine Cluster's arming delay is halved (R2: removed entirely, but a charge armed with no delay has its trigger radius reduced by 1 m until 1s has passed). Rewrites the arm/trigger boundary with an explicit trade at R2. |
| **TK3 — Tripwire** | 1 | 2 | 1 | Mine charges may be triggered by **line of sight** rather than proximity: a charge fires when an enemy crosses its facing within its range (R2: the player may choose the condition per placement, cycling proximity / line-of-sight / on-damage-taken). The data-driven trigger condition made a player-facing choice — the node the deployable definition's swappable trigger field exists for. |
| **TK4 — Rearm** | 2 | 2 | 1 | A Mine Cluster that has spent all its charges rearms one charge every 6s (R2: every 4s) for the remainder of its lifetime, up to its original count. Loop modifier on the trap economy that does not extend lifetime. |
| **TK5 — Attrition Field** | 2 | 2 | 1 | Enemies killed inside a Disruptor field refund 8 Scrap (R2: 14), ignoring the global cap but not the per-kill Scrap source's proc coefficient. The branch's economy node and the one that makes an expensive Disruptor recover its own cost in a dense fight. |
| **TK6 — Overlap** | 2 | 2 | 1 | Disruptor fields that overlap do not stack their armour reduction (unchanged) but **do** extend each other's remaining lifetime to the longer of the two (R2: and the overlapping region's slow applies at the larger value plus a fixed step). The explicit anti-stack rule stated as a benefit, the MS11 pattern. |
| **TK7 — Ordnance** | 3 | 1 | 2 | **Grants G5 Mine Cluster.** Mine Cluster scatters 4 charges instead of 3, and charges detonated within 1s of each other are treated as a single damage instance for proc-coefficient purposes. The second clause is the anti-explosion clause and it is not optional. |
| **TK8 — Interdiction** | 3 | 1 | 2 | **Grants G6 Disruptor.** Disruptor's field also suppresses the *rearm* and *wind-up* of enemy telegraphed attacks begun inside it, delaying them rather than cancelling them. Delay, never cancel — cancellation is a control verb the class does not own. |
| **TK9 — Patience** | 4 | 1 | 2 | A Tinkerer deployable that has been armed and untriggered for 10s or longer triggers at increased effect: its detonation is treated as if it had one additional charge, and its Disruptor equivalent applies its armour reduction at double the flat value on the first enemy to enter. Rewards the pre-placed field over the panic-placed one. |
| **TK10 — Dead Ground** | 4 | 1 | 2 | Your Tinkerer deployables are placed instantly with no deploy cast time while you are **Dry** or **Stocked**; while **Surplus** the cast time doubles. The Gunsmith's stated weakness inverted for the branch that most needs it, and re-imposed on the branch state that least needs it. |
| **TK11 — Command Detonation** | 4 | 1 | 2 | Your Mine charges may be manually detonated by re-activating the ability with no charges left to place; doing so detonates every armed charge you own simultaneously and refunds nothing. Adds a *timing* input to a *placement* class without adding a base-kit verb — it re-uses the existing ability input on an ability the player already equipped. |
| **TK12 — MINEFIELD (keystone)** | 5 | 1 | 4 | Rewrites Field Assembly (§3, Minefield). **More multiplier (3 of 3):** damage dealt to enemies inside one of your Disruptor fields, or by a mine charge that had been armed and untriggered for 10s or longer, is multiplied by **1.20**. The lowest of the three because it can apply to the player's own weapon damage; the setup requirement is the tax. Gunsmith's three Mores are now spent. |

### 4.4 Worked builds against 30 points

| Build | Spend | Reads as |
|---|---|---|
| **Pure Armory** | AR1–AR12 = 26, +4 into FT1 / FT3 | A shooter with a resource bar and a 1.25x that turns off if you place anything. Machinist makes the ultimate a personal buff. The build for a player who chose Gunsmith for the gun. |
| **Pure Field Tech** | FT1–FT12 = 26, +4 into AR1 / AR6 | Five-deployable field (FT9), crates free of the cap (FT8), 1.30x on everything the machines do. Slowest to start, strongest once established. Boss-facing. |
| **Pure Tinkerer** | TK1–TK12 = 26, +4 into FT1 / FT5 | Pre-placed ambush. Cheap while broke, devastating when patient. The highest variance build in the class. |
| **Armory / Field Tech hybrid** | AR to 16 (AR1–AR8 + one Tier-4), FT to 14 (FT1–FT6 + FT8) = 30 | Two abilities, one rewrite each side, no keystone, no More. Turret and Sidearm Rig — the two starters, mastered. The most legible hybrid and a real competitor. |
| **Field Tech / Tinkerer** | FT to 16, TK1–TK3 + TK7 to 14 = 30 | Machines plus traps. The widest Field Assembly (most unlocked types) of any 30-point build. |
| **Triple splash** | Each branch to Tier 3 (10 each) = 30 | Four abilities unlocked, two equippable, and the widest possible Field Assembly. Deliberately the flattest — no rewrite, no keystone — but the ultimate is genuinely different here, which is more consolation than the other four classes give a splash build. |

---

## 5. Solo viability

Solo is the primary balance target (Master 11.1). Gunsmith's solo problem is not damage —
it is that the class's economy is **strictly downstream of combat**, so the first ten
seconds of an encounter are always played at a resource disadvantage that no other class
carries.

**The four solo guarantees:**

1. **A Gunsmith with an empty Scrap bar is still a functional shooter.** Armory's starter
   (Sidearm Rig) costs no Scrap and is cooldown-gated. Both Armory abilities cost nothing.
   A Gunsmith who has never placed anything and has 0 Scrap has a full, working kit. This
   is the acceptance criterion §3 already names and it is the class's floor.
2. **Every generation source is ally-free.** Kill, reload, magazine dump, deployable
   damage, deployable destruction — all producible alone. Verified against Class-Kits §6.5,
   which this document does not change.
3. **The deployable-damage source closes the economy without a party.** A placed turret
   pays toward the next placement. That is what makes Field Tech, the branch that looks
   most like a party-support branch, solo-complete.
4. **Ammo Crate is fully self-usable at full value** with no self-penalty (§G4). Field
   Tech's only utility deployable does not require an ally to justify itself.

**Where solo Gunsmith is genuinely weaker, and why that is accepted:** the class's power
ramps within an encounter rather than being available at its start. Against O18's trash
target (a little under 1s), a Gunsmith kills trash with their gun, not their field — the
field is not up yet, and it should not be. Against the elite target (~3s), a Gunsmith may
get one placement in. Against the boss target (20–45s), the field is the fight. **That
ramp is the class**, and it is the correct answer to "why would I pick the pre-commitment
class": because long fights are where pre-commitment pays.

**The failure mode to watch in wave mode:** if trash TTK measures well above 1s for a
Gunsmith relative to the other four classes, the ramp is a tax rather than a shape, and
Armory's floor (guarantee 1) is too low rather than the field being too slow. The fix in
that case is Armory, not deployables.

**Recommended starting branch: Armory.** It is the branch that functions with the fewest
prerequisites, the one that teaches the ammunition loop the whole class runs on, and the
one whose value does not depend on having learned the map.

---

## 6. Deployable rules summary (design contract)

Consolidated for the engineering read. All of it restates §2.

| Rule | Value / behaviour |
|---|---|
| Density cap | 4 total, 2 per type. Ultimate → 8 total for 20s; per-type stays 2. FT9 → 5 total. **No node raises per-type above 2.** |
| Over-cap placement | Destroys the oldest by placement order and refunds it. Never fails, never prompts. |
| Refund | 50% base; FT1 → up to 80%. One destruction path, one refund path, all causes identical. Hard ceiling below 100%. |
| Lifetime | Turret 30s · Ammo Crate 45s or charges · Mine Cluster 60s or charges · Disruptor 20s. FT3 extends to at most 2x base. Foundry pauses the clock. |
| Placement | Server sweep + LOS trace from the placement point. Never inside geometry, never out of LOS. Fails loudly; a failed placement costs nothing. Floor only; no wall/ceiling placement. |
| Cast time | Seed 0.4s, movement allowed, firing not. TK10 removes it conditionally. This is the class's stated weakness and it must exist. |
| Movement | Deployables never move. No node makes them move. |
| Durability | Own health pool, authored per type. Does not inherit player defences. |
| Enemy targeting | Opportunistic only — nearest threat, or when the player is not visible. Never pulls aggro as a threat mechanic. Bosses and Champions ignore deployables unless nothing else is targetable. |
| Collision | Placement-validation footprint only. Does not block enemy movement or projectiles. Never cover. |
| Damage taken | Direct damage only. AoE cleaves them at full value. No DoT, no status. |
| Damage dealt | The player's damage (§1.3). Player's affixes, crit, statuses. Proc coefficient 0.5 continuous / 1.0 one-shot. Kill credit is the player's. |
| Persistence | Destroyed on player death and on leaving a rift. Scrap persists; furniture does not. |

---

## 7. Compliance audit

### 7.1 Crit policy (Master 6.3)

No node, ability, or loop in this document rolls a chance to multiply damage. There is no
"chance to double-hit," no parallel roll-and-multiply, and no node grants Critical Chance
or Critical Multiplier at all. Deployable damage inherits the player's crit stats (§1.3) —
that is *the existing roll applied to a new delivery mechanism*, not a second roll.
**CONFIRMED.**

The one place to keep an eye on: TK7's clause treating charges detonated within 1s as a
single damage instance for proc-coefficient purposes must not be read as merging crit
rolls into one. Each charge rolls crit independently; only the *proc coefficient* is
merged. That distinction is a test, not a comment.

### 7.2 Verb ownership (Master 5.2 / 7.6, O1, O25)

No Gunsmith node grants walk, sprint, jump, crouch, dash, slide, wall ride, wall jump, or
Parry. **[O25-SUPERSEDED]** the line here used to read "Air Jump (Kinesis) and Parry (Bulwark)
remain the only tree-granted verbs" — **O25** (2026-08-13) rules two jumps base kit for every
character (`JumpMaxCount = 2`, not a tree grant) and makes Swift's third jump a class-innate
Swift-kit unlock (unimplemented at O25's ruling), not a Core Tree or Gunsmith grant. **Parry
(Bulwark) is now the only tree-granted verb in the game, and it is not here.**

**Closest calls, both examined and cleared:**

- **TK11 Command Detonation** adds a manual detonation *input*. It is not a new verb: it
  re-uses the existing ability-slot input of an ability the player has already equipped, in
  a state where that input would otherwise do nothing (no charges left to place). It grants
  no base-kit capability and disappears if the ability is unequipped. Compare Tank's
  Detonation keystone, which the ability spec already notes needs a second input binding —
  TK11 needs no such binding, which is why it is authored this way.
- **G6 Disruptor / TK8 Interdiction** apply slow and delay enemy wind-ups. Slow is a stat
  effect on the enemy movement component, not a control verb. TK8 explicitly **delays,
  never cancels** — cancellation is hard CC and the Gunsmith does not own it. Support's
  Warden owns suppression; Gunsmith owns *area denial*, which is the same effect authored
  from the ground rather than from the target.

**CONFIRMED.**

### 7.3 Layer ownership — no flat percentages

Every one of the 36 nodes is a rule rewrite or a resource-loop modifier. There are **zero
flat-stat nodes** in this document — Gunsmith takes no equivalent of Caster's MS3
(Reservoir), which Class-Kits flags as its one knowing exception. Nodes that touch numbers
touch *loop rates* (AR1, AR4, FT1, FT3, FT4, FT5, TK4, TK5), *rule boundaries* (AR3, AR5,
AR8, TK2, TK3, TK6, TK9), *costs* (TK1, AR11), or *caps* (FT9). None is "+X% damage."

**Two nodes read the affix layer and are flagged as the closest to the line:**

- **AR2 Working Stock** grants a floor tier of a reload-speed affix to a player who has
  none — structurally identical to Swift's K8 (Air Work), which Class-Kits §6.4 already
  flags as its closest call and Open Question 5 already asks about. **AR2 does not create a
  new question; it is the second instance of the existing one and its answer is whatever
  K8's answer is.** Recorded in Open Questions.
- **AR9 Reciprocal** reads `Ammo Returned on Kill` and does nothing without it. That is the
  F8 pattern (class layer reading the affix layer) and is clean — it duplicates no affix
  and grants no capability.

**CONFIRMED with the AR2/K8 flag carried.**

### 7.4 More-multiplier compliance (O3, Core-Constellations §2.4)

| Branch | Keystone | Multiplier | Condition |
|---|---|---|---|
| Armory | MACHINIST | **1.25x** weapon damage | While no deployables are active |
| Field Tech | FOUNDRY | **1.30x** deployable damage | Deployable damage only |
| Tinkerer | MINEFIELD | **1.20x** | Targets inside your Disruptor field, or from a charge armed ≥10s |

- Exactly **one More per branch**, each on the branch's Tier-5 keystone, each ≤1.30x. O3's
  authoring-site rule satisfied.
- A character holds **at most one class keystone** (§0.2), so the class layer contributes
  **at most one** More, maximum 1.30x, always conditional.
- All three conditions are **mutually exclusive or nearly so** by construction: Machinist
  requires no deployables; Foundry applies only to deployable damage; Minefield requires a
  deployable-created condition. Even if the one-keystone ceiling were ever raised
  (Class-Kits OQ3), two Gunsmith Mores could not both apply to the same damage instance
  except in the Foundry/Minefield overlap (a mine inside a Disruptor field), which would be
  1.30 × 1.20 = 1.56x on deployable damage only — inside O3's cap of 3 Mores, and recorded
  here as the number that pass would need to look at.
- Composition is the **unordered product** required by O3 and by Core-Constellations §2.4
  rule 2, implemented through SI-7's `FBreakerMoreMultiplier` array. Order-independence must
  be visible in the damage log.
- **No ability authors a More** (D7). **No Aberrant signature may author one** (O11).

**CONFIRMED.**

### 7.5 Anti-explosion audit (Master 7.10.1 / 7.10.5)

The recursion and stacking risks specific to a deployable class, each with its clamp:

| Risk | Clamp |
|---|---|
| Deployables generating Scrap that buys more deployables | §1.3 invariant: lifetime damage generation < placement cost. Density cap 4. Global 15/s cap. |
| Deployable damage proccing on-hit affixes at weapon rates | Proc coefficient 0.5 continuous / 1.0 one-shot (§1.3). |
| Mine charges as a multi-instance proc engine | TK7's 1s merge window for proc-coefficient purposes. Cluster counts as one deployable. |
| FT11 Deadman chain detonation | Explicit: a Deadman detonation that destroys another deployable does not trigger that one's Deadman. Terminates in one generation, the MS4/Cascade pattern. |
| Refund loops (place → destroy → refund → place) | Refund is capped at 80% (FT1) and never reaches 100%. Every cycle is net-negative Scrap. Placement has a cast time. |
| Disruptor armour reduction stacking to negative armour | Flat, not percentage. Never negative. Overlapping fields do not stack (TK6, the Rot pattern). |
| Field Assembly cap escape | Cap check is on placement; per-type cap never exceeds 2; cap returns to 4 at window end. FT9's +1 and the ultimate's override compose to 9 max, and that combination is the acceptance test. |
| FT3 lifetime farming via reload cycling | Hard ceiling at 2x base lifetime, once per deployable per reload. |
| Machinist's per-type weapon mapping accumulating | One mapping entry per deployable type, authored on the deployable definition; four types unlocked = four riders for 20s, bounded by the type count, which is bounded at six abilities. |

### 7.6 O-ruling audit

| Ruling | Status |
|---|---|
| O1 (no stamina, passive block/dodge, Parry only defensive input) | No Gunsmith node reads a block or dodge *input*; no node grants a defensive verb. Gunsmith has no block/dodge-keyed generation source at all, so O1's re-expression pass has nothing to touch here. **CONFIRMED.** |
| O2 (value freeze) | Flagged in the header; every magnitude is a placeholder seed. No value is authored. **CONFIRMED.** |
| O3 (More rules) | §7.4. **CONFIRMED.** |
| O11 (Aberrant signatures may not author a More) | Nothing in this document authors an item-layer More. **N/A, noted.** |
| O13 (self-damage: reduction, never immunity) | Gunsmith has no self-damage source. Mine Cluster and FT11 Deadman deal radial damage — **both must exclude the owning player entirely**, which is not "self-damage immunity" in O13's sense because the player is never a valid target of their own deployable. Stated explicitly so it is not implemented as a 100% reduction, which *would* violate O13's shape. |
| O18 (TTK/TTD seeds) | Every seed magnitude is chosen to sit plausibly against trash <1s / elite ~3s / boss 20–45s / TTD 4–5s. §5 records the specific failure mode to watch. **CONFIRMED as intent, unmeasured.** |
| O19 (elements are Rift / Entropy / Void) | Gunsmith is element-agnostic. No ability or node in this document names, requires, or grants an element; deployables inherit whatever elemental damage the player's weapon and affixes provide (§1.3). The class ships complete without the resistance model. **CONFIRMED — not blocked.** |

### 7.7 Build-time compliance (O4)

Class identity completes at 30 points. A Gunsmith is viable by mid-campaign (~level 25,
O4's requirement) with roughly 20 points: a full branch to Tier 4 plus a splash reads as a
complete character. Levels 31–50 add nothing to this document's systems. **CONFIRMED.**

---

## 8. Acceptance criteria

The four from Class-Kits §3 are carried verbatim as 1–4 and are not renumbered.

1. **A Gunsmith who places nothing is still a functional shooter via Armory.** Measure a
   zero-placement Armory build's trash and elite TTK against the five-class median; it must
   sit within the median's band, not below it.
2. **Deployable density never exceeds the cap under Field Assembly + any node combination.**
   Test the maximum: FT9 (5) + Field Assembly (8) + every placement ability, and verify
   per-type never exceeds 2 in any configuration.
3. **Deployable damage cannot generate more Scrap than the deployable cost, over the
   deployable's full lifetime, against a stationary target.** Run each type to natural
   expiry against a dummy.
4. **No deployable can be placed inside geometry or outside line of sight of the placement
   point.** Failure is loud and costs nothing.
5. **No input pattern generates more than 15 Scrap/s.** Drive every source simultaneously:
   kill, reload, magazine dump, four deployables damaging, and one destroyed, within the
   same second. Verify with FT4 (which ignores the cap while Surplus) and AR9 (which ignores
   it unconditionally) both active — those two are the only cap-ignoring sources and their
   combination is the test.
6. **A full Scrap bar cannot be refilled by refund loops.** Place, destroy, replace, repeat
   for 60 seconds with no enemies present, at maximum FT1: the bar must be strictly
   monotonically decreasing.
7. **Field Assembly cannot be re-cast within a bounded window.** With the fastest known
   refill (FT4 + AR9 + TK5 + a dense pack), measure the minimum time between two casts and
   record it. There is no cooldown to violate — this criterion exists to discover the number,
   not to enforce one, and it is the Gunsmith equivalent of Swift's criterion 5.
8. **A Gunsmith's class-layer damage multiplier never exceeds 1.30x.** Verify no second More
   has crept in via an ability, a deployable, or the ultimate.
9. **Deployables never pull aggro from the player as a threat mechanic.** With a Champion
   and a full field, the Champion targets the player. Verify the boss/Champion ignore rule.
10. **Equipping two Armory abilities alongside a Field Tech keystone is legal and produces a
    coherent character.** Cross-branch loadouts must not be punished by tree topology
    (the Swift criterion 7 generalised).
11. **Scrap persists across encounters, deployables do not.** Leave a rift with a field
    placed and a partial bar; the bar survives, the field does not.
12. **Overkill damage does not generate Scrap.** Fire a rocket into a 1-health dummy;
    verify the deployable-damage and kill sources credit against damage applied, not damage
    dealt.
13. **Machinist's mapping is total.** Content validation: every shipped deployable
    definition has a Machinist weapon-modifier mapping entry, or the build fails. A future
    deployable must not be able to ship without one.

---

## 9. Engineering dependencies, ranked

Everything here is already in the ability spec's missing-hook register; this is the
Gunsmith-specific ordering.

| Rank | Dependency | Spec ref | Blocks |
|---|---|---|---|
| 1 | **Deployable system** — `ABreakerDeployable`, `UBreakerDeployableComponent`, `PlaceDeployable`, density cap, refund path, `OnDeployablePlaced` / `OnDeployableDestroyed` | §6, §2.7 | 4 of 6 abilities, the ultimate, all 3 keystones, ~20 of 36 nodes. The long pole. Build once; Tank's Anchor Point reuses it. |
| 2 | **`UBreakerScrapComponent`** — the loop, mirroring `UBreakerMomentumComponent`, with a `DA_ScrapPolicy` data asset holding every rate, ICD, and cap | §6 | The entire class. All tuning lives in the data asset, not in C++. |
| 3 | **SI-8 `OnHitDealt` / `OnKillDealt` + `FBreakerHitContext`** with instigator attribution | §2.6 | Every generation source except reload; every deployable-damage node; kill credit. Highest fan-in hook in the whole game. |
| 4 | **`FBreakerDamageRequest::Instigator`** | §7 | Deployable damage attribution (§1.3), kill credit, and the O13 self-exclusion rule for Mine Cluster / Deadman. |
| 5 | **SI-7 `FBreakerMoreMultiplier` array + unordered product** | §1.7 | All three keystones. |
| 6 | **`UBreakerWeaponComponent::OnMagazineEmptied`** | §6 | The magazine-dump generation source; AR5; also Swift F5, so it is shared. |
| 7 | **`PushMagazineCapacityOverride` / `Pop`** with reserve settlement | §6 G2 | G2 Overhaul, AR7, AR10. |
| 8 | **`PushSpeedMultiplier` on enemy movement** + `PushArmorReduction` with a floor | §6 G6, §5.3 | G6 Disruptor, TK6, TK8, TK9. Armour hook shared with Caster's Rot. |
| 9 | **`SetLifetimePaused`** + AI perception exclusion + visibility flag | §6 | Foundry and Minefield keystones. |
| 10 | **Machinist per-deployable weapon-modifier mapping table** | §6, §10.1 (flagged H) | The Machinist keystone. The most bespoke of the fifteen rewrites and the one this document most expects to change under implementation. |
| 11 | **Data-driven deployable trigger conditions** (proximity / LOS / on-damage / manual) | §6 G5 | G5, TK3, TK11. Must be a swappable data field from day one — retrofitting it is expensive. |
| 12 | **SI-1 / SI-2 / SI-9 / SI-10** — ability component, granting from loadout, window+streak state, ability data asset | §2 | Nothing activates without them. Shared with all five classes. |

---

## 10. OPEN QUESTIONS

1. **Does the deployable ramp make Gunsmith the worst trash-clear class in the game, and is
   that acceptable?** §5 argues the ramp *is* the class and Armory is the floor. But O18's
   trash target is under a second, which is a fight that ends before a placement finishes
   its cast time. If wave mode reports Gunsmith trash TTK materially above the median, the
   answer is "raise Armory's floor," not "make deployables faster" — but that is an
   assertion this document cannot verify. **The single most important measurement for this
   class.**

2. **Is the personal/deployable cost split (cooldown-only vs. Scrap-only) legible, or does
   it read as two half-classes?** It is the class's most distinctive ergonomic and its
   biggest risk. A player whose two equipped abilities are both Armory has a class resource
   bar they never spend; a player with two deployables has two abilities with no cooldown
   UI at all. Both are intended and both look broken at a glance. Does the HUD carry it, or
   does the split need a design answer?

3. **AR2 Working Stock grants a floor tier of an affix the player may not have — the same
   action as Swift's K8 (Air Work), which is Class-Kits OQ5.** This document does not
   re-open the question; it records that the answer now binds two nodes in two classes and
   is therefore a *pattern* decision, not a one-node exception. If K8 is ruled illegal, AR2
   dies with it and Armory needs a replacement Tier-1 node.

4. **Should the ultimate's "all currently unlocked deployable types" clause read unlocked
   or equipped?** Unlocked (as authored) makes tree breadth matter and gives the triple-
   splash build a genuinely distinct ultimate — which is more than any other class offers
   its splash build. Equipped would be simpler and would keep loadout the only thing that
   matters. This is the one place Gunsmith's ultimate rewards a shape the other four classes
   punish, and it deserves a ruling rather than a default.

5. **Is Machinist's "every deployable's effect applied to your weapon" mapping authorable at
   all, or does it need to become a hand-authored buff set?** The ability spec flags it as
   the most bespoke of the fifteen rewrites. This document authors the mapping's *shape*
   (one entry per deployable definition, validated by acceptance criterion 13) but a Turret
   "applied to your weapon" and a Disruptor "applied to your weapon" are not the same kind
   of transformation, and pretending they are may be the wrong abstraction. **Most likely
   part of this document to change under implementation.**

6. **Does FT9 (density cap 4 → 5) belong in the tree at all?** It is the only node that
   touches the total cap, and cap changes are the least interesting kind of power — more of
   the same. The alternative is to give Field Tech a fourth Tier-4 rewrite of a different
   character and leave the cap to the ultimate alone. Kept for now because "one more machine"
   is the single most requested thing a deployable player asks for, and denying it entirely
   may be worse than granting it flatly.

7. **Are deployables the right home for the Gunsmith's crowd control, or does Disruptor
   overlap Support/Warden's Suppress too closely?** Both slow, both are zones, both reduce
   enemy effectiveness. This document draws the line at *ground vs. target* (Gunsmith denies
   area, Warden debuffs enemies) and at *delay vs. cancel* (TK8). That line is defensible on
   paper and untested in play.

8. **Should Scrap really never decay, given that it also never idles?** The combination
   means a Gunsmith can bank a full bar at the end of one encounter and open the next with a
   full field, which partially defeats the ramp that §5 argues is the class's identity. The
   alternatives — slow out-of-combat decay, or a cap that lowers out of combat — both feel
   punitive and both violate Class-Kits §0.3's "no resource decays in a menu, at a Forge, or
   in the Anchor." Kept as authored; flagged because the interaction with §5's ramp argument
   is real and unresolved.

9. **TK11 Command Detonation adds a timing input to a placement class.** §7.2 clears it as
   verb-compliant because it re-uses an equipped ability's own input. But it is the only
   node in five classes that changes what an ability's input *does* based on state. Is that
   a pattern worth having, or a precedent worth refusing?

10. **What is the proc coefficient for deployable damage, actually?** §1.3 seeds 0.5
    continuous / 1.0 one-shot. That single number governs whether a four-turret field is a
    proc engine for the entire affix layer, and it is the Gunsmith value most likely to be
    wrong. Frozen under O2; first on this class's measurement list.

### Top three, if only three get answered

1. **OQ1** — does the ramp make Gunsmith the worst trash class. Shapes whether the class
   fantasy survives contact with O18.
2. **OQ5** — is Machinist authorable. Shapes whether Armory has a keystone at all.
3. **OQ2** — is the cost/cooldown split legible. Shapes the whole class's ergonomics and
   the HUD.
