# Ability Implementation Spec — engineering design for every class ability

Status: engineering design. **No balance values are authored here.** Every number in this
document is quoted from `Docs/Design/Class-Kits.md` or `Docs/Design/Core-Constellations.md`.
Where an ability cannot be built without a number that neither document supplies, the gap is
flagged **GAP [O2]** and left unfilled — O2 (`Docs/Design/Decisions.md`) freezes value
authoring until wave-mode instrumentation reports.

Scope: 30 class abilities (6 × 5 classes), 5 ultimates, 15 branch-keystone ultimate rewrites,
and the two tree-granted verbs (Air Jump — Kinesis K4; Parry — Bulwark B4).

Detail level mirrors the prototyping order in Class-Kits §0 and Master 7.5:
**Swift authored in full, Caster authored in full, Gunsmith / Tank / Support one-page.**
That is deliberate: the source design doc is one-page for those three, and authoring a full
engineering spec against a one-page design would mean inventing design, not implementing it.

---

## 0. How to read an ability section

Every ability section has the same five blocks so it can be lifted straight into a ticket:

1. **Design source** — cost, cooldown, behavior, quoted from Class-Kits with the row reference.
2. **GAS mapping** — the `UGameplayAbility` subclass (or the explicit decision that it is a
   GameplayEffect or a plain component call), activation policy, tag block, and the two
   GameplayEffects every costed ability owns (cost + cooldown).
3. **Runtime systems driven** — which shipped component the ability actually manipulates, named
   by class and function, and **MISSING HOOKS** with a proposed signature.
4. **Replication** — server-auth, predicted, cosmetic.
5. **Task checklist** — the ordered work items.

---

## 1. Locked engineering decisions

These are decisions this document makes. They apply everywhere and are not re-litigated per
ability.

### 1.1 D1 — The keystone ultimate-rewrite pattern: **tag-driven ability variants**

**Decision: one `UGameplayAbility` asset per class ultimate. Branch keystones grant a passive
infinite-duration GameplayEffect whose only job is to add one `Keystone.<Class>.<Name>` tag to
the owner's ASC. The ultimate reads its own owner's tag container at `ActivateAbility` and
selects a variant row from the class's ultimate data asset.** No keystone ever grants,
replaces, or blocks an ability.

Rejected alternative — *data-driven modifiers*: each keystone contributes a bag of typed
modifier structs (`+Duration`, `×CostScalar`, `bFreezeResource`) that the ultimate composes.
Rejected because the fifteen rewrites are not parametric. Bloodrhythm adds an *early-exit
condition*; Terminal Velocity changes *dash charge availability*; Standing Wave changes
*projectile range treatment*; Detonation adds a *player-triggered early end that spawns a radial
event*. Expressing those as modifiers means inventing a modifier type per keystone, which is the
variant pattern with worse ergonomics and no type safety.

Rejected alternative — *one ability asset per rewrite* (15 extra ultimate abilities): rejected
because it breaks the "one ultimate" lock in Class-Kits §0.2 at the data layer — the loadout
would hold a different `Ultimate` FName depending on keystone, so `FBreakerAbilityLoadout.Ultimate`
would stop being stable across a respec and save/load would have to migrate it. This is Class-Kits
Open Question 8, and this document answers it: **it does not become a separate asset.**

Why tag-driven wins:

- The loadout already stores `FName Ultimate` only (`FBreakerAbilityLoadout`). The ultimate id
  never changes, so save/load, respec, and the HUD slot are all untouched by keystones.
- Respec is already "a full rebuild, not a decrement" (Core-Constellations §2.3.6). Removing the
  keystone node removes its passive GE, which removes the tag, which reverts the ultimate. No
  bespoke revert code per keystone.
- A character can hold **at most one class keystone** (Class-Kits §0.2). So variant selection is
  a lookup, not a merge. If that ceiling ever changes (Class-Kits OQ3), the selector becomes a
  priority list — a one-function change.
- It matches the existing codebase pattern: tags already drive class identity
  (`Class.Swift`, `Resource.Momentum` in `Config/DefaultGameplayTags.ini`).

Required new tags (add to `Config/DefaultGameplayTags.ini`):

```
Keystone.Swift.Bloodrhythm / Swift.TerminalVelocity / Swift.StandingWave
Keystone.Caster.Edgework   / Caster.LongDark       / Caster.Cascade
Keystone.Gunsmith.Machinist / Gunsmith.Foundry     / Gunsmith.Minefield
Keystone.Tank.Vein          / Tank.Wall            / Tank.Detonation
Keystone.Support.Triage     / Support.Downbeat     / Support.Blackout
```

Required new asset — `UBreakerUltimateDefinition : UPrimaryDataAsset`:

```cpp
USTRUCT(BlueprintType)
struct FBreakerUltimateVariant
{
    UPROPERTY(EditDefaultsOnly) FGameplayTag KeystoneTag;      // empty = base behavior
    UPROPERTY(EditDefaultsOnly) float DurationSeconds = 0.f;   // GAP [O2] where unspecified
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> WindowEffect;
    UPROPERTY(EditDefaultsOnly) FGameplayTagContainer WindowGrantedTags;
    UPROPERTY(EditDefaultsOnly) TArray<TSubclassOf<UGameplayEffect>> AdditionalEffects;
};

UCLASS(BlueprintType)
class UBreakerUltimateDefinition : public UPrimaryDataAsset
{
    UPROPERTY(EditDefaultsOnly) FName UltimateId;
    UPROPERTY(EditDefaultsOnly) EBreakerClassId ClassId;
    UPROPERTY(EditDefaultsOnly) TArray<FBreakerUltimateVariant> Variants; // index 0 = base
};
```

Selector, on the shared ultimate base class:

```cpp
// UBreakerGameplayAbility_Ultimate
const FBreakerUltimateVariant& ResolveVariant(const UAbilitySystemComponent& ASC) const;
```

The *behavioral* differences that are not expressible as a GE (Bloodrhythm's hit-timeout exit,
Detonation's early-end trigger) live in the ultimate's C++ as branches guarded by
`Variant.KeystoneTag`. That is intentional: fifteen branches in five files is honest code, and
each branch is one named condition.

### 1.2 D2 — Ability class hierarchy

```
UBreakerGameplayAbility : UGameplayAbility        // base: cost/CD wiring, class-resource cost, tag defaults
├── UBreakerGameplayAbility_Ultimate              // variant resolution (D1), no cooldown by default
├── UBreakerGameplayAbility_Window                // grants a tagged, timed state; the most common shape
├── UBreakerGameplayAbility_Instant               // fire-and-forget: impulse, blink, detonate
├── UBreakerGameplayAbility_Zone                  // spawns an ABreakerZoneActor
├── UBreakerGameplayAbility_Deployable            // spawns an ABreakerDeployable, density-capped
└── UBreakerGameplayAbility_Channel               // ticking channel with a break condition
```

Nineteen of the thirty abilities are `_Window` or `_Instant`. Building those two well is most of
the work.

### 1.3 D3 — Cost and cooldown are always GameplayEffects, never inline code

Per ability, two assets:

- `GE_Cost_<Class>_<Ability>` — Instant, one Additive modifier of `−Cost` on
  `UBreakerAttributeSet::ClassResource`. Magnitude comes from a `SetByCaller` tag
  `Data.AbilityCost` fed from the ability's data row, so the value lives in data, not the GE.
- `GE_CD_<Class>_<Ability>` — Duration, no modifiers, `GrantedTags = Cooldown.<Class>.<Ability>`.
  Duration is `SetByCaller` `Data.AbilityCooldown`.

Caster and Gunsmith-deployable abilities have **no** cooldown GE at all (Class-Kits §0.3:
"Mana *is* the cooldown"). Do not author an empty one — the HUD must be able to tell "no
cooldown" from "cooldown of zero".

**Cost must not be routed through `UBreakerCombatComponent::SpendClassResource`.** That function
stays for non-GAS callers (the Momentum component, dodge refunds). Ability costs go through GAS
so that `CanActivateAbility` cost-checking, prediction, and the HUD's "can I afford this" query
all work from one source.

**Overcast blocks naive GAS cost checking** (Class-Kits OQ4). See §1.8.

### 1.4 D4 — Standard tag block

Every ability declares:

| Slot | Convention |
|---|---|
| `AbilityTags` | `Ability.Class.<Class>.<AbilityId>` |
| `CancelAbilitiesWithTag` | empty by default. Abilities are short and non-exclusive. |
| `BlockAbilitiesWithTag` | `Ability.Class` on channels only (Siphon), so a channel is not interrupted by a second cast |
| `ActivationOwnedTags` | `State.Ability.<AbilityId>` while active |
| `ActivationBlockedTags` | `State.Dead`, `Cooldown.<Class>.<AbilityId>`, and `State.Overcast.Locked` for Caster |
| `ActivationRequiredTags` | `Class.<Class>` — enforces that a class ability cannot run on the wrong class |

### 1.5 D5 — Replication posture, matching the shipped pattern

The codebase pattern (`UBreakerWeaponComponent`) is: **client calls → `Server*` reliable RPC →
server mutates replicated state → `NetMulticast` unreliable for cosmetics only.** Abilities keep
that shape inside GAS:

- **Activation policy: `EGameplayAbilityNetExecutionPolicy::LocalPredicted` for every ability
  whose local effect is cosmetic-or-input-feel** (window states, cooldown start, animation).
- **`ServerOnly` for anything that spawns an actor or mutates another pawn**: zones, deployables,
  marks, blinks that need a server-side collision resolve, all damage.
- **All damage is server-only, always.** No ability writes to `FBreakerDamageRequest` on a
  client. Damage submission continues to go through the existing server path into
  `UBreakerCombatComponent::ReceiveDamage`.
- **Class-resource spend is server-authoritative.** `ClassResource` is a replicated
  `FGameplayAttributeData`; the client predicts the cost so the HUD bar does not rubber-band, and
  the server is truth. Predicted spend is safe because a rejected activation rolls back the
  prediction key.
- **Cosmetics are `NetMulticast, Unreliable`**, mirroring `MulticastShotCosmetics`. New:
  `UBreakerAbilityComponent::MulticastAbilityCosmetic(FGameplayTag AbilityTag, FVector Origin, FVector Direction)`.
- **Nothing about a keystone variant is replicated separately.** The keystone tag arrives via the
  passive GE, which GAS replicates; every client resolves the same variant.

### 1.6 D6 — Statuses are applied through `UBreakerStatusComponent`, not through GAS

Bleed / Poison / Void already run on `UBreakerStatusComponent` with snapshot semantics and O10's
tick-interval-in-snapshot rule. Caster abilities call
`ApplyStatus(const FBreakerStatusApplicationSpec&, EBreakerDamageFamily)` server-side. Do **not**
build a parallel GAS periodic-GE DoT path — two DoT systems is the single most expensive mistake
available here.

### 1.7 D7 — More multipliers do not live on abilities

O3: Mores compose as an unordered product, max 3, authored only on branch keystones and
constellation Convergence/Keystone nodes. Therefore **no ability in this document authors a
More.** Keystone Mores (Bloodrhythm 1.20×, Terminal Velocity 1.25×, Standing Wave 1.25×,
Edgework 1.30×, Long Dark 1.30×, Cascade 1.25×) are passive conditional GEs granted by the
keystone node, evaluated in the damage pipeline.

**MISSING HOOK (blocking every keystone More):** `FBreakerDamageRequest` has
`SourceDamageMultiplier` — a single float. An unordered product of up to three conditional Mores
cannot be represented as one pre-multiplied float without losing the order-independence property
Core-Constellations §2.4.2 requires to be *visible in the damage log*.

```cpp
// Combat/BreakerCombatTypes.h — add to FBreakerDamageRequest
USTRUCT(BlueprintType)
struct FBreakerMoreMultiplier
{
    UPROPERTY() FGameplayTag SourceTag;  // e.g. Keystone.Swift.Bloodrhythm
    UPROPERTY() float Multiplier = 1.0f;
};
UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerMoreMultiplier> MoreMultipliers;
```

and in `UBreakerDamageLibrary`:

```cpp
static float ComposeMoreProduct(const TArray<FBreakerMoreMultiplier>& Mores); // unordered product
static bool  ValidateMoreCap(const TArray<FBreakerMoreMultiplier>& Mores);    // O3 hard cap 3
```

This is a structural addition, not a balance value. It is **shared infrastructure** (§2, SI-7).

### 1.8 D8 — Overcast and negative resource (Class-Kits OQ4, answered) — **IMPLEMENTED**

> **STATUS: LANDED.** `ClassResourceFloor` exists on `UBreakerAttributeSet` (replicated, default
> **0** for every class, so Swift's Momentum and every pre-existing test are bit-identical).
> `PreAttributeChange` clamps `ClassResource` to `[ClassResourceFloor, MaxClassResource]`, which
> is what makes an ordinary GAS cost GameplayEffect able to drive the bank negative — Overcast is
> reachable in play for the first time. `UBreakerManaComponent` owns the floor: it publishes the
> Overcast floor while the owner's permanent class is Caster and 0 the moment it is not, closing
> it on class change (bound to `OnProgressionChanged` *and* polled from the loop, because
> `DevForceClass` does not broadcast) and lifting a bank stranded below the new floor.
> `SetOvercastFloor` is the SB4/MS10 hook.
> `UBreakerCasterAbility::CheckCost` is overridden to compare against the floor, and it is the
> first time the Caster affordability rule is actually on the activation path — `CanCastAt`
> existed but nothing called it. A cast that would breach the floor is **refused**, never
> truncated to the floor: a partial spend is a silent discount, which is worse than a refused
> cast. Nothing may be cast at all while the bank is below zero.
> Deviations from the shape proposed below: (a) the floor is deliberately **not** part of the
> `FBreakerAttributeAggregator` set — it is an authored single-writer value, and folding
> `(Base + flat) * (1 + Increased)` over a base of 0 would annihilate every percentage anyway;
> (b) the `State.Overcast.Locked` tag/GE form is not built — the same rule is enforced in the
> shared floor-aware predicate (`UBreakerGameplayAbility::IsAffordableWithFloor`) that CheckCost
> and the HUD's `CanAffordSlot` both call, so there is still no per-ability special-casing. The
> tag remains the nicer expression if a node ever needs to *read* the locked state.
> **Still unspiked:** client-side cost prediction across zero. All three Caster abilities are
> `ServerOnly`, so nothing predicts a Caster cost today; the spike is owed before any Caster
> ability becomes `LocalPredicted`.

Caster's Overcast drives `ClassResource` to −20 (deeper with SB4/MS10). Two problems:

1. `UBreakerAttributeSet::PreAttributeChange` clamps `ClassResource` at 0 today.
2. GAS `CheckCost` refuses activation when the cost exceeds the current value.

Resolution — **do not make the attribute signed at the clamp level for every class.** Instead:

- Add an `OvercastFloor` concept to the attribute set as a second attribute
  `ClassResourceFloor` (default 0; Caster sets −20, SB4 lowers it). `PreAttributeChange` clamps
  to `[ClassResourceFloor, MaxClassResource]`. Every other class keeps a 0 floor and is
  unaffected.
- Override `UBreakerGameplayAbility::CheckCost` for the Caster base class to compare against
  `ClassResourceFloor` rather than 0.
- The "no further ability until Mana ≥ 0" rule is a tag: while `ClassResource < 0`, a passive
  GE-driven check adds `State.Overcast.Locked`, which every Caster ability lists in
  `ActivationBlockedTags`. That is the clean expression and it costs no special-casing per ability.

**Technical spike still required before Caster prototyping** (Class-Kits OQ4 says so, and this
resolution is the proposed shape to spike against): confirm client-side cost prediction does not
desync when the predicted spend crosses zero.

### 1.9 D9 — What "GAP [O2]" means here

A GAP flag marks a value the *implementation* needs and no design doc supplies. The ability is
still fully specified structurally: build it, expose the number as a `SetByCaller` / data-asset
field, ship it with a `TODO_O2` placeholder that fails a content-validation test until filled.
Do not guess the number. Full list in §11.

---

## 2. Shared infrastructure — build before any ability

Nothing in §3 onward can start until these land. This is the critical path.

| # | Item | New/Changed | Why it blocks everything |
|---|---|---|---|
| **SI-1** | `UBreakerAbilityComponent` (new, `Source/RiorsEdge/Abilities/`) | new | Owns granting, slot binding, cooldown queries, cosmetic multicast. The single seam between progression and GAS. |
| **SI-2** | Ability granting from the progression loadout | new | `FBreakerAbilityLoadout` holds three FNames and nothing reads them. |
| **SI-3** | Input slot bindings for 2 + 1 | changed | `UBreakerInputConfig` has no ability actions. |
| **SI-4** | Cooldown/cost feed to the HUD ability slots | changed | `ABreakerPlaytestHUD::DrawAbilitySlot` is presentation-only placeholder art. |
| **SI-5** | `UBreakerGameplayAbility` hierarchy (D2) | new | Every ability derives from it. |
| **SI-6** | Cost/cooldown GE templates + `Data.AbilityCost` / `Data.AbilityCooldown` tags | new | D3. |
| **SI-7** | `FBreakerMoreMultiplier` array on the damage request + unordered-product compose | changed | D7; blocks all 15 keystones. |
| **SI-8** | Attacker-side hit event | changed | See §2.6 — the single highest-fan-in missing hook. |
| **SI-9** | `UBreakerAbilityStateComponent` — tagged timed windows and per-target counters | new | See §2.7. |
| **SI-10** | Ability data asset (`UBreakerAbilityDefinition`) | new | Cost/CD/duration values must live in data (Class-Kits §7: "All tuning must live here, not in C++"). |

### 2.1 SI-1 / SI-2 — `UBreakerAbilityComponent` and granting

```cpp
// Source/RiorsEdge/Abilities/BreakerAbilityComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerAbilitySlotChanged, EBreakerAbilitySlot, Slot, FName, AbilityId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBreakerAbilityCooldownChanged, EBreakerAbilitySlot, Slot, float, Remaining, float, Duration);

UCLASS(ClassGroup=Abilities, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerAbilityComponent : public UActorComponent
{
public:
    // Server-only. Reads UBreakerProgressionComponent::GetProgressionState().AbilityLoadout,
    // resolves each FName against the class definition, and reconciles granted specs:
    // revokes what is no longer equipped, grants what is newly equipped, leaves the rest alone.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Abilities")
    void RefreshGrantedAbilities();

    UFUNCTION(BlueprintCallable, Category="Abilities") bool TryActivateSlot(EBreakerAbilitySlot Slot);
    UFUNCTION(BlueprintPure, Category="Abilities") FName GetAbilityIdForSlot(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownRemaining(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownDuration(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") bool  CanAffordSlot(EBreakerAbilitySlot Slot) const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetResourceCostForSlot(EBreakerAbilitySlot Slot) const;

    UPROPERTY(BlueprintAssignable) FBreakerAbilitySlotChanged     OnSlotChanged;
    UPROPERTY(BlueprintAssignable) FBreakerAbilityCooldownChanged OnCooldownChanged;

protected:
    UFUNCTION(Server, Reliable) void ServerActivateSlot(EBreakerAbilitySlot Slot);
    UFUNCTION(NetMulticast, Unreliable) void MulticastAbilityCosmetic(FGameplayTag AbilityTag, FVector Origin, FVector Direction);

private:
    UPROPERTY() TMap<EBreakerAbilitySlot, FGameplayAbilitySpecHandle> GrantedBySlot;
};
```

**MISSING HOOK — `UBreakerProgressionComponent`** must announce loadout and node changes:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerProgressionChanged);
UPROPERTY(BlueprintAssignable, Category="Progression") FBreakerProgressionChanged OnProgressionChanged;
// Broadcast at the end of: EquipAbility, PurchaseNode, RespecAtForge, LoadProgressionState,
// ChoosePermanentClass/ById. UBreakerAbilityComponent binds to it and calls RefreshGrantedAbilities.
```

`UBreakerClassDefinition` needs the ability catalogue it currently lacks:

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
TArray<TObjectPtr<UBreakerAbilityDefinition>> ClassAbilities;   // all six
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
TObjectPtr<UBreakerUltimateDefinition> Ultimate;
```

Grant order on `RefreshGrantedAbilities`: revoke-then-grant, never grant-then-revoke, or a
re-equip of the same ability momentarily double-grants and the cooldown GE gets clobbered.

Respec correctness (Core-Constellations §10.3.4): `RespecAtForge` already rebuilds; the ability
component must revoke abilities whose granting node is gone **and** clear the loadout slot that
referenced them, or the player keeps an ability they no longer own. `EquipAbility` already
validates via `IsAbilityUnlocked` — reuse it as the post-respec filter.

### 2.2 SI-3 — Input bindings

```cpp
// Input/BreakerInputConfig.h — add
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> AbilityOne;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> AbilityTwo;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> Ultimate;
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Abilities") TObjectPtr<UInputAction> Parry;
```

`ABreakerCharacter::SetupPlayerInputComponent` binds each to
`UBreakerAbilityComponent::TryActivateSlot`. Suggested defaults (bindings, not balance):
Q / E / X, Parry on right mouse or F. **Parry's key is Core-Constellations OQ7 and is not settled
here** — the input action must exist and be rebindable; which physical key ships is an owner call.

Ability input must route through GAS's input-id path (`AbilitySpec.InputID`) rather than a bare
`TryActivateAbility`, so that held/released input and prediction keys behave.

### 2.3 SI-4 — HUD cooldown feed

`ABreakerPlaytestHUD::DrawAbilitySlot(const FString& Label, float X, float Y, const FLinearColor& Accent)`
is presentation-only (BreakerPlaytestHUD.cpp:239). Extend to:

```cpp
void DrawAbilitySlot(const FString& Label, float X, float Y, const FLinearColor& Accent,
                     float CooldownRemaining, float CooldownDuration,
                     float ResourceCost, bool bAffordable);
```

Draw rules: radial-or-bar sweep on `Remaining/Duration`; dim the slot when `!bAffordable`; show
the label from `GetAbilityIdForSlot`, not a hardcoded "ABILITY 1". Caster and Gunsmith deployables
pass `CooldownDuration <= 0` and must render as *cost-gated*, never as "ready" with a phantom
zero cooldown.

Also needed on the HUD, driven by the same component: the **Momentum band** (Settled / Running /
**Redline**) — `UBreakerMomentumComponent::GetMomentumState()` already exists and already
broadcasts `OnMomentumStateChanged`; the HUD simply does not consume it yet. Class-Kits §1.1
requires the three bands be "displayed on the HUD as distinct states."

And for Multispell: Fracture's cycle position must be visible (Class-Kits C5: "the cycle order is
visible on the HUD"). See §5.5.

### 2.4 SI-5 / SI-6 / SI-10 — base classes, GE templates, data asset

```cpp
UCLASS(BlueprintType)
class UBreakerAbilityDefinition : public UPrimaryDataAsset
{
    UPROPERTY(EditDefaultsOnly) FName AbilityId;
    UPROPERTY(EditDefaultsOnly) EBreakerClassId ClassId;
    UPROPERTY(EditDefaultsOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UBreakerGameplayAbility> AbilityClass;
    UPROPERTY(EditDefaultsOnly) float ResourceCost = 0.f;
    UPROPERTY(EditDefaultsOnly) float CooldownSeconds = 0.f;   // 0 = no cooldown, cost-gated
    UPROPERTY(EditDefaultsOnly) float WindowDuration = 0.f;
    UPROPERTY(EditDefaultsOnly) FGameplayTag GrantingNodeTag; // for the "did the player unlock it" audit
};
```

Content validation test to add to `Source/RiorsEdge/Tests/`: every
`UBreakerAbilityDefinition` referenced by a shipped `UBreakerClassDefinition` has non-placeholder
cost and cooldown, **or** is on the §11 GAP list. This is how O2 stays enforced rather than
aspirational.

### 2.5 SI-9 — `UBreakerAbilityStateComponent`

Eleven abilities and a dozen nodes need "a named state that lasts N seconds" or "a counter
per target that resets on miss." Building that once is the difference between a clean ability
layer and eleven bespoke timers.

```cpp
// Source/RiorsEdge/Abilities/BreakerAbilityStateComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerWindowChanged, FGameplayTag, WindowTag, bool, bOpen);

UCLASS(ClassGroup=Abilities, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerAbilityStateComponent : public UActorComponent
{
public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void OpenWindow(FGameplayTag WindowTag, float Duration);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ExtendWindow(FGameplayTag WindowTag, float ExtraSeconds);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void CloseWindow(FGameplayTag WindowTag);
    UFUNCTION(BlueprintPure) bool  IsWindowOpen(FGameplayTag WindowTag) const;
    UFUNCTION(BlueprintPure) float GetWindowRemaining(FGameplayTag WindowTag) const;

    // Per-target streak state. Core-Constellations P4 explicitly asks for real state,
    // "not a counter on the weapon".
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) int32 AddStreak(AActor* Target, FGameplayTag StreakTag, float TimeoutSeconds);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void  ResetStreak(FGameplayTag StreakTag);
    UFUNCTION(BlueprintPure) int32 GetStreak(AActor* Target, FGameplayTag StreakTag) const;

    UPROPERTY(BlueprintAssignable) FBreakerWindowChanged OnWindowChanged;
};
```

Server-authoritative. Windows replicate as a compact replicated array so the HUD and cosmetics on
simulated proxies can read them; streak state does **not** replicate (owner-only relevance, and
it changes every shot).

### 2.6 SI-8 — the attacker-side hit event (highest fan-in missing hook)

`UBreakerWeaponComponent::OnShot` fires with an `FBreakerShotResult`, which is close — but it
fires for the *shot*, not per damage instance, it carries no proc coefficient, and it does not fire
for melee (Cleave), zones (Rot), DoT ticks, or deployables. Roughly two-thirds of the abilities
and nodes in Class-Kits key off "when I dealt damage to something."

```cpp
// Combat/BreakerCombatComponent.h — attacker-side, on the DEALER's component
USTRUCT(BlueprintType)
struct FBreakerHitContext
{
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadOnly) FBreakerDamageResult Result;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer SourceTags; // Ability.*, Damage.*, Combat.WeakPoint
    UPROPERTY(BlueprintReadOnly) float ProcCoefficient = 1.0f;
    UPROPERTY(BlueprintReadOnly) float Distance = 0.0f;            // for M1, S6, Standing Wave
    UPROPERTY(BlueprintReadOnly) bool bMelee = false;
    UPROPERTY(BlueprintReadOnly) bool bDamageOverTime = false;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerHitDealt, const FBreakerHitContext&, Hit);
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHitDealt OnHitDealt;
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHitDealt OnKillDealt;
```

Every damage submission path — weapon, rocket, zone, status tick, deployable, melee — routes
through one server-side helper that raises this. Doing anything else means each new damage source
silently fails to feed half the class trees.

### 2.7 Shared infrastructure not on the critical path but needed early

| Item | Consumers |
|---|---|
| `UBreakerMarkComponent` (target-mark registry, §4.6) | S6 Lead, Support Mark, Warden branch, Blackout |
| `ABreakerZoneActor` + `UBreakerZoneComponent` (§5.3) | C3 Rot, C4 Siphon area, Suppress, Disruptor |
| `ABreakerDeployable` + density cap (§6) | all Gunsmith Field Tech / Tinkerer, Tank Anchor Point |
| Outgoing-damage modifier chain (`UBreakerCombatComponent::BuildOutgoingDamage`) | pierce, armour-ignore, weak-point forcing, flat stack bonuses |

---

## 3. Implementation order

Aligned with Class-Kits §0 / Master 7.5 and the vertical-slice override in Class-Kits §7
(Swift only, Kinetic + Marksman, Tiers 1–3).

**Phase 0 — Shared infrastructure.** SI-1 … SI-10. No ability content.
Exit test: an empty debug ability grants from the loadout, binds to Q, spends class resource,
starts a cooldown, and the HUD slot sweeps.

**Phase 1 — Swift, movement boundary.** S3 Skim → S4 Hard Stop → S1 Slipcut → S2 Cadence Break.
Skim first because it is the smallest ability that proves the whole chain, and it is the one that
tests the movement boundary Master 7.5 wants tested first.
Exit test: Class-Kits §1.7 acceptance criteria 1–4 still hold with abilities equipped.

**Phase 2 — Swift, projectile framework.** S5 Sightline → S6 Lead → Mark component →
Overdrive base → the three Swift keystone variants.
Exit test: Class-Kits §1.7 criteria 5–7.

**Phase 3 — Caster, statuses and reactions.** C1 Cleave → C3 Rot (zone actor) → C5 Fracture →
C6 Resonance → C4 Siphon (channel) → C2 Closequarter → Unmake + three variants.
Cleave first because it is the only melee verb in the game and melee damage submission does not
exist yet.
**PARTLY DONE, out of the listed order:** Cleave, Closequarter, and Unmake shipped together
because all three were buildable without new `Combat/` systems, while Rot / Siphon / Fracture /
Resonance each need one (zones, healing, a projectile base, status consumption). D8 is now
resolved, so Overcast is live for the three shipped abilities — see the status block at the top
of §5.
Exit test: Class-Kits §2.7 criteria 1–7.

**Phase 4 — Verbs.** Air Jump (Kinesis K4) → Parry (Bulwark B4). Deliberately after Swift and
Caster: they are Core Tree grants, not class abilities, and the slice's fifteen-node set
(Core-Constellations §10.1) needs the granting machinery from Phase 0 to already be proven.
Exit test: Core-Constellations §10.3 criteria 4, 5, 6, 10.

**Phase 5 — Gunsmith / Tank / Support.** One-page treatments only. Deployable framework is the
long pole; build it once for Gunsmith and reuse it for Tank's Anchor Point.
**Do not start Phase 5 until the three one-page classes get a full design pass.** The §6–§8
sections below are scaffolding, not build-ready tickets.

---

# 4. SWIFT — Momentum

Resource: `ClassResource` gated by `Class.Swift`, driven by the shipped
`UBreakerMomentumComponent`. Swift abilities cost Momentum **and** carry a cooldown
(Class-Kits §1.1 Spending), so every Swift ability has both a cost GE and a cooldown GE.

## 4.0 Swift-wide prerequisites

**MISSING HOOK — `UBreakerMomentumComponent`** must expose spend-side awareness and let nodes
modify the loop. Today the component only generates.

```cpp
// Classes/BreakerMomentumComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerMomentumSpent, float, Amount);
UPROPERTY(BlueprintAssignable, Category="Momentum") FBreakerMomentumSpent OnMomentumSpent;   // F6 Feed reads "cost most recently paid"

// Suspend decay and rescale generation — Overdrive, M9 Reserve, K11 No Ground.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Momentum")
void PushLoopOverride(FGameplayTag SourceTag, bool bSuspendDecay, float GenerationScalar, float GlobalCapOverride);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Momentum")
void PopLoopOverride(FGameplayTag SourceTag);
UFUNCTION(BlueprintPure, Category="Momentum") bool IsDecaySuspended() const;

// Flat grants that bypass the per-second cap (F4 Rhythm, K3 Carry, M-nodes).
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Momentum")
void GrantMomentum(float Amount, bool bIgnoreGlobalCap);
```

Stack-based override (push/pop by tag) rather than a boolean, because Overdrive + M9 Reserve +
K11 No Ground can all be live at once and each must revert independently.

**MISSING HOOK — dodge event.** `UBreakerCombatComponent` resolves dodge inside `ReceiveDamage`
and refunds resource silently. Momentum's "+15 on successful dodge" source and K5 Evade Conversion
both need the event, and so does the whole Tank Grit loop for blocks:

```cpp
// Combat/BreakerCombatComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerDefenseRoll, const FBreakerDamageResult&, Result);
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDefenseRoll OnDodgeEvaded;
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDefenseRoll OnBlockRolled;
```

> **Note on Class-Kits OQ1 (block/dodge model).** O1 settled it: **passive chance layers**, no
> stamina. `UBreakerCombatComponent` already implements the passive model
> (`BlockChance` / `DodgeChance` / `BlockMitigation`, no inputs). Class-Kits §8.1 lists this as an
> open conflict; it is not one any more. Every node and ability below is written against the
> passive model, per O1.

---

## 4.1 S1 — Slipcut *(starter, Frenzy)*

**Design source.** Class-Kits §1.2 S1. Cost 20 Momentum, CD 4s. "0.4s window in which every
weapon hit has its cadence cost halved (fires at 2× rate, consumes ammo normally). Ends early on
reload."

**GAS mapping.**
- Class: `UBreakerGameplayAbility_Window`, `InstancingPolicy = InstancedPerActor`.
- Net policy: `LocalPredicted`. The window is a local firing-feel change; the shots inside it are
  still server-validated individually.
- `AbilityTags`: `Ability.Class.Swift.Slipcut`. `ActivationOwnedTags`: `State.Ability.Slipcut`.
- `ActivationBlockedTags`: `Cooldown.Swift.Slipcut`, `State.Dead`.
- Cost: `GE_Cost_Swift_Slipcut` (−20 ClassResource). Cooldown: `GE_CD_Swift_Slipcut` (4s).
- Window: `UBreakerAbilityStateComponent::OpenWindow(Window.Swift.Slipcut, 0.4f)`.

**Runtime systems driven.**
- `UBreakerWeaponComponent` — fire cadence. **MISSING HOOK:** cadence is computed inside
  `FireOnce` / the `AutomaticFireTimer` from the weapon definition with no modifier seam.

```cpp
// Weapons/BreakerWeaponComponent.h
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon")
void PushCadenceMultiplier(FGameplayTag SourceTag, float IntervalScalar); // 0.5 = fires twice as fast
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon")
void PopCadenceMultiplier(FGameplayTag SourceTag);
UFUNCTION(BlueprintPure, Category="Weapon") float GetEffectiveShotInterval() const;
```
  Note "consumes ammo normally" — the modifier must scale the *interval*, not the ammo cost, and
  must reschedule the in-flight `AutomaticFireTimer` on push/pop or the change lands one shot late.

- **MISSING HOOK — reload start event** (window "ends early on reload"). `OnReloadChanged` exists
  and fires with `bReloading`. Bind to it; no new hook needed. ✅ Existing.
- F7 Slipcut Mastery ("window extends by 0.15s for each ability cooldown currently active") needs
  `UBreakerAbilityComponent::GetActiveCooldownCount() const` — add it alongside the other slot
  queries in SI-1.

**Replication.** Server-auth activation; cost predicted; window replicated (SI-9) so remote
clients can play the muzzle-cadence cosmetic. Cadence multiplier is applied on the server and on
the autonomous proxy so local fire feel is immediate; simulated proxies do not need it.

**Tasks.** (1) `PushCadenceMultiplier`/`Pop` + timer reschedule. (2) `_Window` base class.
(3) GA + cost/CD GEs + data row. (4) Bind `OnReloadChanged` → `CloseWindow`.
(5) `GetActiveCooldownCount` for F7. (6) Test: 0.4s window fires exactly double the shots of an
unbuffed 0.4s and consumes the same ammo per shot.

## 4.2 S2 — Cadence Break *(Frenzy, granted by F7)*

**Design source.** §1.2 S2. Cost 35, CD 8s. "Instantly completes the current reload and grants a
3s state: each consecutive hit on the same target adds a stacking flat damage bonus (10 stacks
max, resets on miss or target swap). Explicitly the *flat* bucket, not Increased."

**GAS mapping.** `UBreakerGameplayAbility_Window`, `LocalPredicted`.
`ActivationOwnedTags: State.Ability.CadenceBreak`. Cost 35 / CD 8s GEs.
Window `Window.Swift.CadenceBreak` for 3s; streak `Streak.Swift.CadenceBreak` on SI-9.

**Runtime systems driven.**
- **MISSING HOOK — instant reload completion.** `FinishReload()` is private and timer-driven.

```cpp
// Weapons/BreakerWeaponComponent.h
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon")
void CompleteReloadImmediately();   // no-op when not reloading; clears ReloadTimer, calls FinishReload
```
- **MISSING HOOK — flat outgoing damage contribution.** `FBreakerDamageRequest.BaseDamage` is set
  at the call site; nothing lets an ability add a flat bucket. This is the outgoing-damage
  modifier chain from §2.7:

```cpp
// Combat/BreakerCombatComponent.h (attacker side)
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PushFlatDamageBonus(FGameplayTag SourceTag, float FlatAmount, AActor* RestrictToTarget);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PopFlatDamageBonus(FGameplayTag SourceTag);
```
  It must land in the **flat sum** stage, before the additive Increased bucket, per the locked
  aggregation rule in Item-Foundation. Add a damage-log assertion for that ordering — the design
  note explicitly warns against double-dipping with Damage Ramp.
- Streak driver: `OnHitDealt` (SI-8), incrementing per target; `ResetStreak` on a miss.
  **MISSING HOOK — miss event.** `FBreakerShotResult` has `bHit`, and `OnShot` fires on misses
  too, so binding `OnShot` and checking `!bHit` covers the weapon case. ✅ Existing.

**GAP [O2]:** the flat bonus per stack is not specified. Structure is complete; the magnitude is
a placeholder.

**Replication.** Server-auth. Reload completion is server state (`bReloading` is
`ReplicatedUsing=OnRep_Reloading`) — already correct. Stack count is owner-only; broadcast a
cosmetic multicast at stacks 5 and 10 only, not per stack.

**Tasks.** (1) `CompleteReloadImmediately`. (2) Flat-bonus push/pop in the flat stage.
(3) Streak wiring on SI-9. (4) GA + GEs. (5) F9 Second Wind variant: a passive GE-granted tag
`Node.Swift.SecondWind` that the ability reads to skip the target-swap reset. (6) Test: 11th
consecutive hit does not exceed 10 stacks; a miss zeroes it; damage log shows the bonus in the
flat bucket.

## 4.3 S3 — Skim *(starter, Kinetic)* — **build this first**

**Design source.** §1.2 S3. Cost 15, CD 3s. "Directional impulse that preserves current
horizontal speed and converts it into a lateral or backward vector. Not a dash — Skim has no speed
floor and *cannot* increase speed. It redirects. Usable airborne once per airtime."

**GAS mapping.** `UBreakerGameplayAbility_Instant`, `LocalPredicted` — this one genuinely needs
prediction; a server-round-trip redirect feels broken. Cost 15 / CD 3s.
`ActivationOwnedTags: State.Ability.Skim` for one frame.

**Runtime systems driven.**
- `UBreakerCharacterMovementComponent`. **MISSING HOOK** — `TryDash` is the only impulse verb and
  it has a speed floor and bonus (`DashSpeedFloor`, `DashSpeedBonus`), which Skim must not have.

```cpp
// Movement/BreakerCharacterMovementComponent.h
// Rotates existing horizontal velocity onto RequestedDirection with NO magnitude change.
// Returns false if there is no horizontal speed to redirect.
UFUNCTION(BlueprintCallable, Category="Movement") bool TryRedirect(const FVector& RequestedDirection);
UFUNCTION(BlueprintPure, Category="Movement") int32 GetAirborneActionsUsed() const;
UFUNCTION(BlueprintCallable, Category="Movement") bool ConsumeAirborneAction(int32 MaxPerAirtime);
```
  `ConsumeAirborneAction` resets on ground contact and is the shared counter for "once per
  airtime" (Skim; K7 Skim Discipline raises the max to 2; and Air Jump uses the same reset points).
  **Master 5.4 guardrail is structural here:** `TryRedirect` must assert
  `Velocity.Size2D()` is non-increasing across the call. Write that as a unit test, not a comment.

**Replication.** `LocalPredicted` with a `FGameplayAbilityTargetData` carrying the requested
direction. Movement itself replicates through the existing `CharacterMovementComponent` network
model — do **not** hand-roll a multicast for the velocity change. Cosmetic (whoosh, camera roll)
is a `MulticastAbilityCosmetic`.

**Tasks.** (1) `TryRedirect` + non-increasing-speed test. (2) Airborne-action counter with reset
on `OnLanded`. (3) `_Instant` base. (4) GA + GEs. (5) K4 Redirect node hook: needs
"horizontal facing changed >90° while airborne" — add
`UBreakerCharacterMovementComponent::GetAirtimeFacingDelta() const` and a cooldown-reduction call
`UBreakerAbilityComponent::ReduceCooldown(EBreakerAbilitySlot, float Seconds)`.

## 4.4 S4 — Hard Stop *(Kinetic, granted by K7)*

**Design source.** §1.2 S4. Cost 30, CD 6s. "Cancels all velocity instantly and grants 0.6s of
Damage Reduction While Airborne treatment on the ground."

**GAS mapping.** `UBreakerGameplayAbility_Window`, `LocalPredicted`. Cost 30 / CD 6s.
Window `Window.Swift.HardStop` 0.6s.

**Runtime systems driven.**
- Movement: `Velocity = FVector::ZeroVector` — needs a guarded entry point, not a raw write:
```cpp
UFUNCTION(BlueprintCallable, Category="Movement") void CancelVelocity(bool bIncludeVertical);
```
- Defense: the ability grants the *Damage Reduction While Airborne* affix value while grounded.
  **MISSING HOOK — affix value query.** `UBreakerEquipmentComponent` folds affixes into the
  attribute set; nothing exposes "what is my rolled value for affix X". K9 Momentum Shield needs
  the same thing, and K8 Air Work needs the *tier*:

```cpp
// Items/BreakerEquipmentComponent.h
UFUNCTION(BlueprintPure, Category="Equipment") float GetAffixValue(FGameplayTag AffixTag) const;
UFUNCTION(BlueprintPure, Category="Equipment") int32 GetAffixTier(FGameplayTag AffixTag) const;
```
  Then apply it as an incoming-damage reduction for the window. **THIS HOOK NOW EXISTS** — built
  for Caster's Overcast penalty and keyed by `FName` rather than `FGameplayTag`, matching the
  outgoing chain's existing convention in the same file:
```cpp
// Combat/BreakerCombatComponent.h — SHIPPED
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Incoming")
void PushIncomingDamageModifier(FName Key, float Multiplier); // 1.0 = none; 0.0 = immune
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Incoming")
void RemoveIncomingDamageModifier(FName Key);
UFUNCTION(BlueprintPure, Category="Combat|Incoming") float GetComposedIncomingDamageMultiplier() const;
```
  Entries compose multiplicatively into `FBreakerDefenseState::IncomingDamageMultiplier` at the
  top of `ReceiveDamage` — the same stage gear-rolled physical reduction already occupies, so
  before armour, shields, and the passive rolls. Re-pushing a key replaces rather than stacks.
  There is deliberately **no expiry**: an incoming modifier reflects a state whose owner is
  responsible for removing it, and a silently expiring defence is worse than a visibly stuck one.
  K10 Spend to Live turns the window into full immunity — that is `Multiplier = 0.0f` through the
  same hook plus a cost override to 60, both read from the node's data row. **No new code path.**
  Master 7.10.4's invulnerability-loop risk is bounded by the existing 6s cooldown; add a test that
  immunity uptime cannot exceed 0.6s per 6s under any node combination.

**Replication.** Server-auth for the damage modifier (it is defense, never predict it).
Velocity cancel is predicted through the movement component's own model.

**Tasks.** (1) `CancelVelocity`. (2) `GetAffixValue`/`GetAffixTier`. (3) incoming-damage
push/pop. (4) GA + GEs. (5) K10 data variant. (6) Immunity-uptime test.

## 4.5 S5 — Sightline *(Marksman, granted by M7)*

**Design source.** §1.2 S5. Cost 25, CD 6s. "Next shot fired within 2s pierces all targets in a
line and cannot be blocked by cover-state enemies. Pierce here is a granted rule, distinct from the
Pierce affix, and stacks additively with it (max +3 total)."

**GAS mapping.** `UBreakerGameplayAbility_Window`, `LocalPredicted`. Cost 25 / CD 6s.
Window `Window.Swift.Sightline` 2s, consumed by the next shot rather than by time.

**Runtime systems driven.**
- **MISSING HOOK — per-shot modifier.** The weapon's hitscan resolves in `FireOnce` with no seam
  for "the next shot behaves differently." Several abilities need this shape (Sightline,
  Gunsmith's Sidearm Rig).

```cpp
// Weapons/BreakerWeaponComponent.h
USTRUCT(BlueprintType)
struct FBreakerPendingShotModifier
{
    UPROPERTY() FGameplayTag SourceTag;
    UPROPERTY() int32 AdditionalPierce = 0;
    UPROPERTY() bool bIgnoreCoverState = false;
    UPROPERTY() bool bIgnoreArmorAfterFirstTarget = false; // M7's own upgrade
    UPROPERTY() bool bForceWeakPoint = false;              // reused by S6 Lead
    UPROPERTY() float FlatBonusDamage = 0.f;               // reused by Gunsmith Sidearm Rig
    UPROPERTY() int32 ShotsRemaining = 1;
};
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Weapon")
void PushPendingShotModifier(const FBreakerPendingShotModifier& Modifier);
UFUNCTION(BlueprintPure, Category="Weapon") int32 GetEffectivePierceCount() const; // clamped to +3 total
```
- The hitscan itself must gain multi-target resolution. Today `FireOnce` resolves one impact.
  Pierce requires an ordered multi-hit trace with a falloff per target — and M10 Overpenetration
  rewrites that falloff. Build the multi-hit trace once, with a `PierceIndex` on
  `FBreakerDamageRequest` so downstream rules (M7 armour-ignore after target 1, M6 per-target
  Momentum, M10 falloff) can all read position in the chain.

```cpp
// Combat/BreakerCombatTypes.h — add to FBreakerDamageRequest
UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PierceIndex = 0;
```
- "cover-state enemies": **no cover state exists** on `ABreakerEnemy`. This is a real dependency,
  not a value gap.

**Replication.** Server-auth: piercing decides who takes damage. The pending modifier is set on
the server; the client predicts only the window UI. `ShotsRemaining` decrements server-side.

**Tasks.** (1) Multi-hit pierce trace with `PierceIndex`. (2) `FBreakerPendingShotModifier`.
(3) Pierce cap (+3 total) shared with the Pierce affix — one clamp, one place.
(4) Enemy cover state (or descope the clause and flag it). (5) GA + GEs.

## 4.6 S6 — Lead *(Marksman, granted by M8)*

**Design source.** §1.2 S6. Cost 40, CD 10s. "Marks the target under the crosshair for 6s. Shots
that hit the mark from more than 25 m away are treated as weak-point hits regardless of impact
location."

**GAS mapping.** `UBreakerGameplayAbility_Instant` with a targeting step, `ServerOnly` — the mark
mutates another actor, so there is nothing worth predicting except the reticle.
Cost 40 / CD 10s.

**Runtime systems driven.**
- **MISSING SYSTEM — marks.** Nothing in the codebase marks a target. Needed by S6, Support's
  Mark, the entire Warden branch, and Blackout.

```cpp
// Source/RiorsEdge/Combat/BreakerMarkComponent.h  (lives on the TARGET)
USTRUCT(BlueprintType)
struct FBreakerMark
{
    UPROPERTY(BlueprintReadOnly) FGameplayTag MarkTag;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Instigator;
    UPROPERTY(BlueprintReadOnly) float RemainingDuration = 0.f;
    UPROPERTY(BlueprintReadOnly) float MinimumRangeForEffect = 0.f; // S6's 25 m gate; M11 lowers it
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerMarkEvent, const FBreakerMark&, Mark);

UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerMarkComponent : public UActorComponent
{
public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ApplyMark(const FBreakerMark& Mark);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void RemoveMark(FGameplayTag MarkTag, AActor* Instigator);
    UFUNCTION(BlueprintPure) bool HasMarkFrom(FGameplayTag MarkTag, const AActor* Instigator) const;
    UFUNCTION(BlueprintPure) const TArray<FBreakerMark>& GetMarks() const;
    UPROPERTY(BlueprintAssignable) FBreakerMarkEvent OnMarkApplied;
    UPROPERTY(BlueprintAssignable) FBreakerMarkExpired OnMarkExpired;  // M5 hooks this to jump the mark
private:
    UPROPERTY(Replicated) TArray<FBreakerMark> ActiveMarks;
};
```
  Replicated, because the HUD must draw a mark indicator on remote clients.
- Weak-point forcing reuses `FBreakerPendingShotModifier::bForceWeakPoint` — but conditionally, at
  damage-build time, on distance. So the check belongs in the outgoing-damage builder:
  `if (Target has my Lead mark && Hit.Distance > Mark.MinimumRangeForEffect) Request.bWeakPointHit = true;`
- M8 "may be held on two targets": a mark count per instigator, from the node's data row.
- M5 Mark Economy (mark jumps on death, proc coefficient 0): bind `OnMarkExpired` plus the
  target's death, re-apply with `ProcCoefficient = 0`. Add a `bIsPropagated` flag on `FBreakerMark`
  so a jumped mark cannot itself jump — the same anti-recursion discipline as Affliction's A9.

**Replication.** `ServerOnly` activation. `ActiveMarks` replicated. Mark VFX from `OnRep`.

**Tasks.** (1) `UBreakerMarkComponent` + replication. (2) Attach it to `ABreakerEnemy` and
`ABreakerTargetDummy`. (3) Distance-gated weak-point forcing in the outgoing-damage builder.
(4) GA + GEs. (5) HUD mark indicator. (6) M5/M8/M11 data variants.

## 4.7 ULTIMATE — Overdrive + three keystone rewrites

**Design source.** §1.2. Cost 100 Momentum (full bar). **No cooldown.** Base: for 8s, Momentum
does not decay and generation is doubled against the per-second cap (cap raised to 40/s); player
locked at minimum Redline.

**GAS mapping.** `UBreakerGameplayAbility_Ultimate`. `ServerOnly` — an 8s global state change is
not worth predicting and a mispredicted ultimate is the worst possible feel.
Cost `GE_Cost_Swift_Overdrive` (−100). **No cooldown GE** — "the cost *is* the cooldown."
`ActivationOwnedTags: State.Ultimate.Overdrive`.
Variant resolution per D1 against `DA_Ult_Swift_Overdrive`.

**Base behavior wiring.** One call:
`Momentum->PushLoopOverride(Tag_Overdrive, /*bSuspendDecay*/ true, /*GenerationScalar*/ 2.0f, /*GlobalCapOverride*/ 40.0f)`
plus a floor enforcement (`minimum Redline`) — **MISSING HOOK**:
`UBreakerMomentumComponent::PushMomentumFloor(FGameplayTag, float FloorValue)` / `Pop`.

| Keystone | Tag | Implementation |
|---|---|---|
| **Bloodrhythm** (F12) | `Keystone.Swift.Bloodrhythm` | Bind `OnHitDealt` for the window: `GrantMomentum(1.f, /*bIgnoreGlobalCap*/ true)` per hit. Early-exit timer reset on each hit; if it elapses (1.5s), `EndAbility`. Pure C++ branch in the ultimate — this is exactly the non-parametric case D1 was chosen for. |
| **Terminal Velocity** (K12) | `Keystone.Swift.TerminalVelocity` | **MISSING HOOK:** `UBreakerCharacterMovementComponent::PushDashChargeOverride(FGameplayTag, int32 Charges /* -1 = unlimited */)` / `Pop`, and `PushWallRideDurationOverride(FGameplayTag, float Seconds /* <0 = untimed */)` / `Pop`. Master 5.4 guardrails unchanged — availability rewrite only; add a test that wall ride still generates zero speed under the override. |
| **Standing Wave** (M12) | `Keystone.Swift.StandingWave` | `PushLoopOverride(..., bSuspendDecay=true, GenerationScalar=0.f, ...)` freezes the bar in both directions. "Shots behave as if fired at point-blank" = **MISSING HOOK:** `UBreakerWeaponComponent::PushRangeTreatmentOverride(FGameplayTag, float TreatAllDistancesAsMetres)` — the falloff curve and projectile-speed lookup read the override instead of the true distance. |

**Acceptance test (Class-Kits §1.7.5).** Overdrive cannot be re-cast within 8s of ending under
any node combination. With no cooldown GE, this is enforced purely by the 100-cost and the
generation cap: verify against Bloodrhythm + F6 Feed + F4 Rhythm, the fastest known refill. If it
fails, the fix is a cooldown GE and that is a **design change**, not an implementation choice —
escalate rather than adding one silently.

**Tasks.** (1) `_Ultimate` base + variant resolution. (2) `PushLoopOverride`/`PushMomentumFloor`.
(3) Three variant branches. (4) Dash-charge and wall-ride overrides. (5) Range-treatment override.
(6) The §1.7.5 re-cast test.

---

# 5. CASTER — Mana

Resource: `ClassResource` gated by `Class.Caster`. **No cooldowns on any Caster ability** — cost
only (Class-Kits §2.1). Do not author cooldown GEs for this class.

> **IMPLEMENTATION STATUS (this section is now partly built).**
> Shipped: `UBreakerCasterAbility` (the shared base carrying the two class-wide rules — no
> cooldowns ever, and Unmake's cost rewrite), **C1 Cleave**, **C2 Closequarter**, and the
> **Unmake** ultimate with its four variant rows. `UBreakerManaComponent` exists and its
> Overcast incoming-damage penalty is wired into `UBreakerCombatComponent`.
> Not built: C3 Rot, C4 Siphon, C5 Fracture, C6 Resonance, and the Edgework-on-Closequarter
> and Cascade keystone halves. Each is blocked on a Combat/ system that does not exist
> (zone actor, partial healing, projectile base, status consumption).
>
> **D8 IS RESOLVED — OVERCAST IS REACHABLE.** `ClassResourceFloor` (default 0) landed on the
> attribute set, `PreAttributeChange` clamps `ClassResource` to `[Floor, Max]`, the Mana
> component publishes the floor for Casters only, and `UBreakerCasterAbility::CheckCost` compares
> against it. Ability costs are still ordinary GameplayEffects — no second, non-GAS spend path
> (D3). A cast that would breach the floor is refused rather than truncated, and nothing may be
> cast while the bank is below zero. See the status block on §1.8 for what was deliberately not
> built (the `State.Overcast.Locked` tag form) and what is still owed (the prediction spike).

**Mana loop component EXISTS**: `Source/RiorsEdge/Classes/BreakerManaComponent`. Beyond the shape
described below it now also carries `GrantMana(Amount, bIgnoreGlobalCap)` (used by Closequarter's
refund) and keyed `PushGenerationSuspension`/`PopGenerationSuspension` (used by Unmake).

```cpp
// Source/RiorsEdge/Classes/BreakerManaComponent.h — mirrors the Momentum component's shape
// Generation: passive regen, weapon hit (Multishot at 1/n — MANDATORY), weak-point (replaces
// weapon-hit, does not stack), kill, status application (0.4s ICD per status type, ticks
// generate NOTHING), reload completed. Global cap 20/s. Never decays.
UFUNCTION(BlueprintPure) float GetMana() const;
UFUNCTION(BlueprintPure) bool  IsOvercast() const;                       // ClassResource < 0
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void SetOvercastFloor(float Floor); // SB4, MS10
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void GrantMana(float Amount, bool bIgnoreGlobalCap);
```

The anti-Multishot 1/n rule needs the pellet count at generation time — bind to `OnHitDealt`
(SI-8) and divide by `FBreakerHitContext::ProcCoefficient`-adjacent pellet data. **MISSING HOOK:**
`FBreakerHitContext` needs `int32 ProjectilesInVolley = 1;` so the 1/n rule has an input.
Class-Kits §2.7.2 makes this a hard acceptance criterion.

## 5.1 C1 — Cleave *(starter, Spellblade)* — **BUILT**

> **As built** (`Abilities/BreakerAbility_Cleave.{h,cpp}`, `Abilities/BreakerMeleeSweep.{h,cpp}`).
> Faithful to the ticket below with three recorded deviations:
> - The melee library lives at `Abilities/BreakerMeleeSweep.h`, not `Combat/BreakerMeleeLibrary.h`.
>   Cleave is its only caller and the implementing agent did not own `Combat/`. Move it when a
>   second melee source appears; the API is this spec's.
> - The animation lock is a GAS activation lock, not a timer plus a flag: the ability stays
>   active for the lock and lists its own `State.Ability.Cleave` in `ActivationBlockedTags`.
>   Edgework zeroes the duration through `AnimationLockFor`, so the lock is variant-readable as
>   required.
> - `FBreakerHitContext::bMelee` was **not** added (Combat/ is owned elsewhere). Instead the
>   damage request carries the new `Damage.Melee` source tag, which is what the Spellblade 1.30x
>   More and Melee Damage % affixes actually need. SB1 Contact Charge will still want `bMelee`
>   on the hit context.
> Targets resolve deterministically (centre-most first, distance breaks ties), range is measured
> horizontally so a target on a crate is still reachable, and the sweep refuses to swing through
> world geometry. Bleed applies at 100% with a snapshot critical roll, matching the weapon path.
> O2 PLACEHOLDER values: arc 120°, weapon coefficient 1.5x, unarmed fallback 20, lock 0.45s,
> Bleed 6/tick for 4s at 1s intervals.

**Design source.** §2.2 C1. Cost 20 Mana, no CD. "Short forward melee arc, 3 m, physical damage
scaled by weapon damage. Applies Bleed at a 100% base chance."

**GAS mapping.** `UBreakerGameplayAbility_Instant`, `ServerOnly` (it deals damage in a volume).
Cost GE only. `ActivationOwnedTags: State.Ability.Cleave` for the animation lock — note
**Edgework removes that lock**, so the lock must be a variant-readable duration, not hardcoded.

**Runtime systems driven.**
- **MISSING SYSTEM — melee.** No melee damage path exists at all. This is the single largest new
  system in the Caster set.

```cpp
// Source/RiorsEdge/Combat/BreakerMeleeLibrary.h
struct FBreakerMeleeSweepParams
{
    FVector Origin; FVector Forward;
    float RangeCm = 0.f; float ArcDegrees = 0.f;   // SB8 Edge widens to 180
    int32 MaxTargets = 0;
};
// Server-only. Deterministic ordering by angle then distance so replays and tests match.
static TArray<AActor*> SweepMeleeTargets(const UWorld* World, AActor* Instigator,
                                         const FBreakerMeleeSweepParams& Params);
```
  Damage requests built per target with `SourceTags` carrying `Damage.Melee` (new tag) so the
  Edgework 1.30× melee More can gate on it, and `bMelee = true` on `FBreakerHitContext` so SB1
  Contact Charge can pay the weak-point Mana rate for melee hits.
- Bleed: `UBreakerStatusComponent::ApplyStatus` with a `Status.Bleed` spec at 100% chance. ✅ Existing.
- SB2 Follow Through bypasses the 0.4s per-type ICD for melee only — the ICD lives in the Mana
  component, so it needs `GrantMana(..., bIgnoreInternalCooldown)`. Fold into the existing
  `GrantMana` signature rather than adding another.

**GAP [O2]:** Cleave's damage coefficient against weapon damage; Bleed spec magnitude/duration for
the Cleave application.

**Replication.** `ServerOnly`. Sweep, damage, and status are all server. Swing animation and hit
sparks via `MulticastAbilityCosmetic`.

**Tasks.** (1) `SweepMeleeTargets` + determinism test. (2) `Damage.Melee` tag + `bMelee` on the
hit context. (3) GA + cost GE. (4) SB8 arc-widening data variant. (5) SB2 ICD bypass.

## 5.2 C2 — Closequarter *(Spellblade, granted by SB7)* — **BUILT**

> **As built** (`Abilities/BreakerAbility_Closequarter.{h,cpp}`). Deviations:
> - `TryBlinkTo` was not added to `UBreakerCharacterMovementComponent` (Movement/ owned
>   elsewhere). The blink is a swept `SetActorLocation` with `ETeleportType::TeleportPhysics`,
>   which has exactly the contract this section asks for — it stops at the last non-penetrating
>   position rather than depositing the player in geometry. Velocity is zeroed explicitly, per
>   "no velocity carried". Lift it onto the movement component when a second blink consumer
>   appears.
> - `bBypassDefensiveRolls` (SB5 Momentum Transfer) was not added; that is a Combat/ change.
> - Targeting is committed **before** the cost: with nothing valid under the crosshair the cast
>   is refused rather than charged. Skim deliberately charges on a failed redirect because a
>   moving player can always redirect; Closequarter with no target provably cannot move the
>   player at all, so charging for it would be a dead key. Flagging the asymmetry rather than
>   quietly matching Skim.
> - The refund reads the target's health fraction from its attribute set and calls
>   `GrantMana(15, bIgnoreGlobalCap=true)` — payback is not generation and must not be metered
>   through the 20/s cap. SB10's 100% threshold is `RefundHealthFraction`, a data field.

**Design source.** §2.2 C2. Cost 35 Mana, no CD. "Blink to the target under the crosshair within
12 m, arriving 2 m short of it. Not a dash and not a grapple — instantaneous, no travel, no tether,
no velocity carried. Landing refunds 15 Mana if the target is at or below 40% health."

**GAS mapping.** `UBreakerGameplayAbility_Instant`, **`ServerOnly`.** A predicted teleport that
gets corrected is the worst desync in the game; eat the latency.
Cost 35, no cooldown GE.

**Runtime systems driven.**
- **MISSING HOOK — validated blink.** `SetActorLocation` is not acceptable: it must resolve
  against geometry and refuse to place the player inside a wall.

```cpp
// Movement/BreakerCharacterMovementComponent.h
// Sweeps from current location toward Destination, stops at the last non-penetrating position,
// zeroes velocity (no carry, per the design), returns false if no valid position exists.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Movement")
bool TryBlinkTo(const FVector& Destination, FVector& OutFinalLocation);
```
- Target acquisition: a crosshair trace capped at 12 m; arrival point = target location − 2 m along
  the approach vector. SB7's own upgrade allows a no-target blink 12 m along aim.
- Refund: read target health fraction from its `UBreakerCombatComponent` / attribute set,
  `GrantMana(15.f)`. SB10 No Distance moves the threshold to 100% and raises cost to 50 — both are
  data-row fields, no new code.
- SB5 Momentum Transfer ("next melee hit cannot be blocked or dodged by the target") — **MISSING
  HOOK:** an outgoing-damage flag that suppresses the target's passive rolls:
  `FBreakerDamageRequest::bBypassDefensiveRolls` (new bool), honoured in
  `UBreakerCombatComponent::ReceiveDamage` before the dodge/block rolls.

**Replication.** `ServerOnly`. The teleport replicates through the movement component's correction
path (a large `ClientAdjustPosition`); blink-in/out VFX at both endpoints via
`MulticastAbilityCosmetic`.

**Verb-compliance note (Class-Kits §6.3 / §2.7.7):** Closequarter must remain an *ability
occupying a loadout slot*. Do not add it to base kit, and do not let any node grant it outside
SB7.

**Tasks.** (1) `TryBlinkTo` + geometry-refusal test. (2) Crosshair target trace.
(3) `bBypassDefensiveRolls`. (4) GA + cost GE. (5) SB10 data variant. (6) Test: blink never places
the player outside navigable space or through a wall.

## 5.3 C3 — Rot *(starter, Void Whisperer)*

**Design source.** §2.2 C3. Cost 25 Mana, no CD. "4 m radius zone at the aim point, 6s duration.
Enemies inside take Poison and have their Armour reduced by a flat 40."

**GAS mapping.** `UBreakerGameplayAbility_Zone`, `ServerOnly`. Cost 25.

**Runtime systems driven.**
- **MISSING SYSTEM — zones.** Needed by Rot, Support's Suppress, Gunsmith's Disruptor.

```cpp
// Source/RiorsEdge/Combat/BreakerZoneActor.h
UCLASS()
class RIORSEDGE_API ABreakerZoneActor : public AActor
{
public:
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ConfigureZone(const FBreakerZoneSpec& Spec, AActor* Owner);
    UFUNCTION(BlueprintPure) int32 GetOccupantCount() const;          // VW2 needs "at least one", count-INdependent
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void RefreshDuration(float NewDuration); // VW4: refresh, never stack
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void SetFollowActor(AActor* Follow);     // VW8 Wellspring
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void SetExpiryPaused(bool bPaused);      // Long Dark keystone
    UPROPERTY(BlueprintAssignable) FBreakerZoneOccupancy OnOccupantEntered;
    UPROPERTY(BlueprintAssignable) FBreakerZoneOccupancy OnOccupantExited;
};
```
  `bReplicates = true`, replicated `FBreakerZoneSpec` so clients can render it. Occupancy is
  server-only (an overlap query per tick on the server, never on clients).
  **VW4's anti-stack rule is a property of the spawner:** before spawning, query for an overlapping
  zone of the same tag owned by the same instigator and call `RefreshDuration` instead. Put that
  check in `UBreakerGameplayAbility_Zone`, once, not per ability.
- Poison: `ApplyStatus` on occupants at the zone's tick. ✅ Existing.
- **MISSING HOOK — flat armour reduction with a floor.** Armour is an attribute; a naive
  −40 additive GE stacks into negative armour when two Rots overlap, and VW7 adds another −40.

```cpp
// Combat/BreakerCombatComponent.h
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PushArmorReduction(FGameplayTag SourceTag, float FlatAmount);  // clamps effective armour at 0
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
void PopArmorReduction(FGameplayTag SourceTag);
```
  Tag-keyed so the same source cannot double-apply. Flat, never percentage — Class-Kits VW7 is
  explicit that this protects the boss cap (Master 7.10.5).

**Replication.** `ServerOnly` activation, replicated zone actor, cosmetic decal/particles driven
from the client-side spec. Occupant damage and status: server only.

**Tasks.** (1) `ABreakerZoneActor` + spec + replication. (2) Overlap-refresh (VW4) in the zone
ability base. (3) `PushArmorReduction` with a zero floor. (4) GA + cost GE.
(5) VW7 / VW8 data variants. (6) Test: two overlapping Rots do not double-reduce armour and do not
drive it negative.

## 5.4 C4 — Siphon *(Void Whisperer, granted by VW7)*

**Design source.** §2.2 C4. Cost 30 Mana, no CD. "5s channel on one target: deals Void damage over
time and heals the caster for a portion. Channel breaks on the caster taking damage above a
threshold."

**GAS mapping.** `UBreakerGameplayAbility_Channel`, `ServerOnly`.
`BlockAbilitiesWithTag: Ability.Class` while channelling. `ActivationOwnedTags: State.Channeling`.
Cost 30, no cooldown.

**Runtime systems driven.**
- Channel loop: a server timer submitting a damage request per tick with `bIsDamageOverTime = true`
  and `DamageTypeTag = Damage.Elemental.Void`. **Note O5:** Void is one of the three real elements
  (Rift / Time / Void), and per the O5 implementation note `EBreakerDamageFamily::Elemental`
  remains the pipeline family until the resistance model lands. Siphon ships as Elemental/Void and
  gains resistance interaction later, no rewrite.
- **MISSING HOOK — healing.** `UBreakerCombatComponent` has `RestoreVitals` (full restore) and
  nothing else. Leech, Support's entire Medic branch, and Siphon all need partial healing with
  overheal reporting.

```cpp
// Combat/BreakerCombatComponent.h
USTRUCT(BlueprintType)
struct FBreakerHealResult
{
    UPROPERTY(BlueprintReadOnly) float HealthHealed = 0.f;
    UPROPERTY(BlueprintReadOnly) float Overheal = 0.f;      // Support Charge MUST generate 0 from this
    UPROPERTY(BlueprintReadOnly) float ShieldGranted = 0.f; // Leech overheal→shield routing
};
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
FBreakerHealResult ApplyHealing(float Amount, AActor* Healer, FGameplayTag SourceTag);
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHealEvent OnHealed;
```
  Reporting overheal separately is not optional: Support's Charge loop has "overheal generates
  nothing" as a hard acceptance criterion (Class-Kits §5, criterion 4).
- Break condition: bind `UBreakerCombatComponent::OnDamageReceived` on the caster; break when
  `Result.HealthDamage` exceeds the threshold. VW6 Drain raises the threshold — data row.

**GAP [O2]:** Siphon's damage per tick, tick interval, and the heal *portion* are all unspecified.
The break threshold has a VW6-relative form ("below 15% of max health") but no base value.

**Replication.** `ServerOnly`. Channel beam is a cosmetic multicast started and stopped by the
ability; the beam must be driven by a replicated "channelling target" pointer so late joiners see
it, or accept that it is unreliable-cosmetic and can be missed — prefer the latter, consistent
with `MulticastShotCosmetics`.

**Tasks.** (1) `_Channel` base with break conditions. (2) `ApplyHealing` + `FBreakerHealResult`.
(3) GA + cost GE. (4) VW6 data variant. (5) Test: channel breaks exactly at the threshold, and
Mana generation from Void ticks is **zero** (§2.7.3).

## 5.5 C5 — Fracture *(Multispell, granted by MS7)*

**Design source.** §2.2 C5. Cost 30 Mana, no CD. "Projectile that applies one status, cycling
deterministically through the caster's available status types on each cast. The cycle order is
visible on the HUD."

**GAS mapping.** `UBreakerGameplayAbility_Instant` (spawns a projectile), `ServerOnly`. Cost 30.

**Runtime systems driven.**
- Projectile: reuse `ABreakerRocketProjectile`'s replicated-projectile pattern. Generalize it —
  **MISSING HOOK:** extract `ABreakerProjectileBase` from the rocket so Fracture is not a
  copy-paste of a rocket with the explosion removed. This also unblocks Gunsmith's Mine Cluster.
- **MISSING SYSTEM — the status cycle.**

```cpp
// Source/RiorsEdge/Classes/BreakerStatusCycleComponent.h  (on the caster)
UFUNCTION(BlueprintPure) FGameplayTag PeekNext(int32 Lookahead = 0) const;   // MS2 R2 previews 1 ahead
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) FGameplayTag AdvanceCycle();
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void SetAdvanceOnHit(bool bOnHit); // MS2 R1
UFUNCTION(BlueprintPure) const TArray<FGameplayTag>& GetAvailableStatusTypes() const;
UPROPERTY(BlueprintAssignable) FBreakerCycleChanged OnCycleChanged;          // HUD feed
```
  "The caster's available status types" is derived, not authored: Bleed and Poison ship now; Void
  ships with Siphon; Rift and Time arrive with O5's resistance model. The cycle must be built from
  what the character can actually apply, and must be **deterministic and replicated to the owner**
  so the HUD preview is truthful.
  Multispell ships physical-only per Class-Kits §2.5's BLOCKED note. Every node is authored against
  "distinct status types," never named elements — the implementation must honour that, so no code
  path may hardcode Bleed/Poison.
- MS7's upgrade applies **two** cycle positions per cast: `AdvanceCycle()` twice, apply both,
  second at proc coefficient per the node.

**Replication.** `ServerOnly` activation. Projectile replicates. Cycle state replicates to owner
only for the HUD.

**Tasks.** (1) `ABreakerProjectileBase` extraction. (2) `UBreakerStatusCycleComponent`.
(3) HUD cycle readout (SI-4). (4) GA + cost GE. (5) MS2 / MS7 data variants.

## 5.6 C6 — Resonance *(Multispell, granted by MS8)*

**Design source.** §2.2 C6. Cost 40 Mana, no CD. "Detonates every status currently on the target
for a burst of damage, consuming them. Damage scales with the *number* of distinct status types,
not their stacks — an explicit anti-stacking rule."

**GAS mapping.** `UBreakerGameplayAbility_Instant`, `ServerOnly`. Cost 40.

**Runtime systems driven.**
- **MISSING HOOKS — `UBreakerStatusComponent`.** It exposes `GetActiveStatuses` and `HasStatus`
  and nothing that consumes or counts distinctly.

```cpp
// Combat/BreakerStatusComponent.h
UFUNCTION(BlueprintPure, Category="Combat|Status") int32 GetDistinctStatusTypeCount() const;
// Removes and returns what was removed, so the detonator can compute damage from the same data.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
TArray<FBreakerActiveStatus> ConsumeAllStatuses();
// MS8's rewrite: do not consume, halve remaining durations instead.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
void ScaleRemainingDurations(float Scalar);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
void SetStackCapDelta(int32 Delta);   // Affliction A1 Deepen also needs this
```
- Damage from **distinct count**, and MS9 Interference reshapes that curve to fixed-per-status plus
  a flat bonus at 3+. Implement the scaling as a small strategy on the ability
  (`Linear` / `FixedPlusThreshold`) selected by the MS9 node tag — the same tag-driven pattern as
  D1, applied one level down.

**GAP [O2]:** the burst damage per distinct status, and MS9's fixed-per-status and 3+ flat values.
Class-Kits §2.7.5 sets the *bound* (≤2.2× from 2 statuses to 6) but not the values.

**Replication.** `ServerOnly`. Detonation VFX scaled by distinct count via `MulticastAbilityCosmetic`.

**Tasks.** (1) The four status-component hooks. (2) Distinct-count damage with a pluggable curve.
(3) GA + cost GE. (4) MS8 / MS9 data variants. (5) Test for §2.7.5's 2.2× bound with placeholder
values — the test is written now and starts passing when O2 lifts.

## 5.7 ULTIMATE — Unmake + three keystone rewrites — **BASE + LONG DARK BUILT**

> **As built** (`Abilities/BreakerAbility_Unmake.{h,cpp}`).
> - The cost override is a **window payload**, not a tag check. `UBreakerAbilityStateComponent`
>   gained an optional float on each window; Unmake opens `Window.Caster.Unmake` carrying the
>   cost scalar, and `UBreakerCasterAbility::GetResourceCost` multiplies by it. Both `CheckCost`
>   and `ApplyCost` read through that one virtual, so affordability and spend can never
>   disagree. This is strictly better than the tag form the ticket proposed: Long Dark's 50% is
>   the same code path as the base 0%, with no branch and no second GE.
> - Generation suspension is `UBreakerManaComponent::PushGenerationSuspension(FName)` /
>   `Pop`, keyed rather than boolean so overlapping sources revert independently. Queued
>   credits are discarded on push, not banked, so the bar cannot leap when the window closes.
> - Teardown is unconditional in `EndAbility` — including cancel and death. A Caster left with
>   free casts because the ultimate was interrupted is this design's worst failure mode.
> - Long Dark is fully built: its variant row carries 12s and 0.5, and nothing branches.
> - **Edgework is built only for Cleave** (the lock removal). Its Closequarter half needs a
>   line-of-sight range override and is not built.
> - **Cascade is not built**: it needs Fracture's status cycle, which does not exist. Its
>   variant row exists and resolves so the keystone is visibly unfinished rather than silently
>   inert.
> - Overcast-into-Unmake (the §2.2 interaction) is now **reachable**: D8 landed, so a Caster can
>   overdraft into the ultimate, and a cost of 0 under the Unmake window is castable at any bank
>   level including deep in debt (`CheckCost` returns true before it ever looks at the bank).
>   Unverified in play — no one has playtested the interaction.

**Design source.** §2.2. Cost 80 Mana, no cooldown. Base: for 6s all Caster abilities cost 0 Mana
and Mana generation is suspended.

**GAS mapping.** `UBreakerGameplayAbility_Ultimate`, `ServerOnly`. Cost 80. No cooldown GE.
Base variant grants `State.Unmake` for 6s.

**Base wiring.** "All Caster abilities cost 0" is a cost *override*, not a cost refund:
`UBreakerGameplayAbility::CheckCost`/`ApplyCost` on the Caster base returns early when the owner
has `State.Unmake`. Do not implement it as a 0-cost GE swap per ability — that is thirty extra
assets to keep in sync. Generation suspension: `UBreakerManaComponent::PushLoopOverride` mirroring
the Momentum component's API, for symmetry.

Overcast interaction is explicitly supported (§2.2): a Caster can Overcast into Unmake and spend
the debt during the free window. So `State.Overcast.Locked` must **not** block activation of
Unmake itself — add Unmake to the exception, or more cleanly, give Unmake
`ActivationBlockedTags` without `State.Overcast.Locked`.

| Keystone | Tag | Implementation |
|---|---|---|
| **Edgework** (SB12) | `Keystone.Caster.Edgework` | Cleave's animation-lock duration → 0 for the window (already a variant-readable field, §5.1). Closequarter's range limit → unbounded within line of sight: pass `RangeCm = TNumericLimits<float>::Max()` and require an unobstructed trace. **MISSING HOOK:** none beyond §5.2's `TryBlinkTo`, which already sweeps. |
| **Long Dark** (VW12) | `Keystone.Caster.LongDark` | Duration 6s → 12s and cost override 0% → 50% (a scalar in `CheckCost`, not a separate GE). Zones placed during the window: `ABreakerZoneActor::SetExpiryPaused(true)`, released at window end. |
| **Cascade** (MS12) | `Keystone.Caster.Cascade` | Bind `UBreakerStatusComponent::OnStatusApplied` for the window; on each application, apply `StatusCycle->PeekNext()` with **`ProcCoefficient = 0`**. That zero is load-bearing (Class-Kits §2.2: without it this is the recursion bomb in Master 7.10.1). Enforce it with a re-entrancy guard as well as the coefficient — belt and braces, matching Affliction A9's "all three are required, not alternatives" discipline. |

**Acceptance test (§2.7.6).** Cascade + MS4 Chain must not produce unbounded propagation: 20
enemies in a 10 m radius, propagation terminates within one generation. Write this as an
automation test, not a manual check.

**Tasks.** (1) Caster cost-override in `CheckCost`/`ApplyCost`. (2) Mana loop override.
(3) Three variant branches. (4) `SetExpiryPaused`. (5) Cascade re-entrancy guard + the §2.7.6 test.

---

# 6. GUNSMITH — Scrap *(one-page treatment)*

Source design is one page (Class-Kits §3). This section is **scaffolding for a future design
pass**, not a build-ready ticket set. Costs, cooldowns, and behaviors below are only what §3
states.

**Resource shape.** Event-driven only, no decay, no passive regen, global cap 15/s.
Needs `UBreakerScrapComponent` mirroring the Momentum component. Its generation sources bind to:
`OnKillDealt` (SI-8), `OnReloadChanged` completion, magazine-emptied
(**MISSING HOOK:** `UBreakerWeaponComponent::OnMagazineEmptied` — a delegate fired when the last
round leaves the magazine; also needed by Swift F5 Dry Fire), deployable destruction, and
deployable damage dealt (routes through `OnHitDealt` with the deployable as instigator).

**Split rule (§3):** deployables cost Scrap and have **no cooldown**; personal abilities have
cooldowns and **no cost**. Two different GE profiles inside one class — the ability data asset
already supports it (`ResourceCost` or `CooldownSeconds`, one of them zero).

**MISSING SYSTEM — deployables.** The largest new system after melee.

```cpp
// Source/RiorsEdge/Combat/BreakerDeployable.h
UCLASS() class RIORSEDGE_API ABreakerDeployable : public AActor { /* replicated, owned, lifetimed */ };

// Source/RiorsEdge/Classes/BreakerDeployableComponent.h  (on the player)
// Density cap enforced by the OWNING component, per Character-Progression-Architecture.
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
ABreakerDeployable* PlaceDeployable(TSubclassOf<ABreakerDeployable> Type, const FTransform& Where, float ScrapCost);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void PushDensityCapOverride(FGameplayTag SourceTag, int32 TotalCap, int32 PerTypeCap);
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void PopDensityCapOverride(FGameplayTag SourceTag);
UFUNCTION(BlueprintPure) int32 GetActiveCount(TSubclassOf<ABreakerDeployable> Type) const;
UPROPERTY(BlueprintAssignable) FBreakerDeployableEvent OnDeployablePlaced;
UPROPERTY(BlueprintAssignable) FBreakerDeployableEvent OnDeployableDestroyed;  // 50% Scrap refund
```
Cap: 4 active, 2 per type. Placing a fifth destroys the oldest **and refunds it** (§3) — implement
as "destroy-with-refund", so the refund path is the same one used when an enemy kills it.
Placement validation (§3 acceptance criterion 4): never inside geometry, never outside line of
sight of the placement point. That is a server-side sweep + LOS trace in `PlaceDeployable`, and it
must fail loudly rather than silently relocating.

| # | Ability | GAS class | Notes / missing hooks |
|---|---|---|---|
| G1 | **Sidearm Rig** *(starter, Armory)* — 10s CD, no cost | `_Window` | `FBreakerPendingShotModifier` with `FlatBonusDamage` + `AdditionalPierce`, `ShotsRemaining` = magazine size. Reuses S5's hook entirely. **GAP [O2]:** bonus damage value. |
| G2 | **Overhaul** *(Armory)* | `_Window` | Converts reserve into magazine capacity for 10s. **MISSING HOOK:** `UBreakerWeaponComponent::PushMagazineCapacityOverride(FGameplayTag, int32 Delta)` / `Pop`, with reserve debited on push and settled on pop. **GAP [O2]:** cooldown, conversion ratio. |
| G3 | **Turret** *(starter, Field Tech)* — 40 Scrap, 30s lifetime | `_Deployable` | Autonomous target acquisition + firing; damage routed through `OnHitDealt` with the *player* as the ultimate instigator so Scrap generation and affixes apply. **GAP [O2]:** turret damage, fire rate, range, health. |
| G4 | **Ammo Crate** *(Field Tech)* — 30 Scrap | `_Deployable` | Interact-to-refill reserve. Reuses the NPC interaction prompt pattern in `ABreakerCharacter::FindNearbyNPC`. **GAP [O2]:** refill amount, charges, lifetime. |
| G5 | **Mine Cluster** *(Tinkerer)* — 35 Scrap, 3 proximity charges | `_Deployable` | Proximity trigger + radial damage; reuses `ABreakerRocketProjectile`'s radial falloff. Tinkerer nodes rewrite trigger conditions and rearm — so trigger logic must be a data-driven condition, not hardcoded. **GAP [O2]:** damage, radius, arming delay. |
| G6 | **Disruptor** *(Tinkerer)* — 45 Scrap | `_Deployable` + `_Zone` | Field that slows and strips Armour. Slow = **MISSING HOOK:** `UBreakerCharacterMovementComponent::PushSpeedMultiplier(FGameplayTag, float)` / `Pop` on the *enemy* movement component. Armour strip reuses `PushArmorReduction` (§5.3). **GAP [O2]:** slow magnitude, armour strip, radius, lifetime. |

**ULTIMATE — Field Assembly.** 100 Scrap. Deploys all unlocked deployable types at once at no
individual cost; density cap → 8 for 20s (`PushDensityCapOverride`).
Keystones: **Machinist** — applies every deployable's effect to the player's weapon instead (the
no-deployable ultimate; needs an "effect as weapon modifier" mapping per deployable type — the
most bespoke of all fifteen rewrites and the one most likely to need its own design pass);
**Foundry** — deployables placed during the window never expire (`SetLifetimePaused`);
**Minefield** — deployables placed during it are invisible until triggered (visibility flag +
a server-side perception exclusion so AI does not target them either).

---

# 7. TANK — Grit *(one-page treatment)*

**Resource shape.** Generation from **post-mitigation** damage taken (mandatory — §4), self-damage
at 25% rate, melee kills, passive Block roll firing, and proximity. Decay −5/s after 6s. Cap 20/s.
`UBreakerGritComponent`, binding `OnDamageReceived` (✅ exists, and `FBreakerDamageResult` already
carries `HealthDamage`/`MitigatedDamage` so post-mitigation is directly available) and
`OnBlockRolled` (§4.0 missing hook).

**MISSING HOOK — self-damage attribution.** The 25%-rate rule needs to know a damage instance came
from the victim. `FBreakerDamageRequest` has no instigator field at all — it has
`SourceLocation`/`bHasSourceLocation` only. Add:
```cpp
// Combat/BreakerCombatTypes.h — FBreakerDamageRequest
UPROPERTY(EditAnywhere, BlueprintReadWrite) TWeakObjectPtr<AActor> Instigator;
```
This is broadly useful (kill attribution, assists for Support's Charge, Grit self-damage rate) and
is arguably the most conspicuous omission in the current damage contract. `ABreakerRocketProjectile`
currently "ignores its instigator" as a stopgap; with this field plus O13's ruling (strong
self-damage reduction, full self-knockback control, **never** immunity) the rocket can stop
ignoring itself and start applying the reduction properly.

| # | Ability | GAS class | Notes / missing hooks |
|---|---|---|---|
| T1 | **Rend** *(starter, Leech)* | `_Instant` | Melee that heals for a portion of damage; overheal → shield. Needs §5.1's melee sweep and §5.4's `ApplyHealing` with `ShieldGranted` routing. **GAP [O2]:** cost, CD, damage, leech portion. |
| T2 | **Bloodline** *(Leech)* — 8s | `_Window` | All Life on Hit doubled and applies to DoT ticks at proc coefficient. Needs a Life-on-Hit stat read (`GetAffixValue`, §4.4) and the DoT tick to route through `OnHitDealt` with its proc coefficient intact. **GAP [O2]:** cost, CD. |
| T3 | **Anchor Point** *(starter, Bastion)* — 12s | `_Deployable` | Frontal cover. Reuses the Gunsmith deployable framework — build it once. **GAP [O2]:** cost, CD, cover health/size. |
| T4 | **Provoke** *(Bastion)* — 10 m, 4s | `_Instant` | **MISSING HOOK:** `ABreakerEnemy::ForceTarget(AActor* Target, float Duration)` — the enemy AI has no threat override. Solo conversion (stacking flat damage per enemy provoked) reuses `PushFlatDamageBonus` (§4.2). **GAP [O2]:** cost, CD, per-enemy damage bonus. |
| T5 | **Breach Charge** *(Demolitionist)* | `_Instant` (projectile) | Thrown explosive, strong self-knockback, heavily reduced self-damage. Governed by **O13**: reduction up to 80%, full self-knockback control, never immunity. Needs the `Instigator` field above. **GAP [O2]:** cost, CD, damage, knockback impulse. |
| T6 | **Ground Zero** *(Demolitionist)* | `_Instant` | Downward slam from airborne, radial damage and stagger. **MISSING HOOK:** stagger — `UBreakerCombatComponent::ApplyStagger(float Duration)`, also required by Parry (§9.2) and Bulwark B6 Unyielding. Build it once, here or in Phase 4, whichever lands first. **GAP [O2]:** cost, CD, damage, radius, stagger duration. |

**ULTIMATE — Hold.** 100 Grit. 10s: incoming damage reduced to a fixed maximum per hit
(`PushIncomingDamageModifier` is a multiplier — this needs a *cap*, so extend it:
`PushIncomingDamageCap(FGameplayTag, float MaxPerHit)`), Grit generation tripled.
Keystones: **Vein** — converts incoming damage into healing at reduced rate;
**Wall** — extends mitigation to allies within 8 m and doubles it solo (needs an ally query and a
solo check — `UBreakerPartyPolicy` exists, use it); **Detonation** — ends early on command,
releasing absorbed damage as a radial explosion (needs an accumulator on the ultimate and a
**second input binding for "end ultimate early"** — the only ability in the game that needs one;
flag it for the input design pass).

**Acceptance criterion 4 (§4)** — no combination of Leech nodes, Hold, and the passive Block layer
produces indefinite survivability — should be an automation test against Master 7.10.4, written
alongside the immunity-uptime test from §4.4.

---

# 8. SUPPORT — Charge *(one-page treatment)*

**Resource shape.** Every group source has a self-facing twin at an **identical rate** — this is
the anti-7.10.6 clause and §5 calls it non-negotiable. No decay, cap 18/s.
`UBreakerChargeComponent` binding `OnHealed` (§5.4 missing hook, with `Overheal` reported so it can
generate zero), buff uptime (count-independent), damage to marked targets
(`UBreakerMarkComponent`, §4.6), and assists.

**MISSING SYSTEM — buffs with count-independent uptime tracking.**
```cpp
// Source/RiorsEdge/Classes/BreakerBuffComponent.h  (on the Support)
UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
void ApplyBuff(FGameplayTag BuffTag, const TArray<AActor*>& Targets, float Duration);
// TRUE if at least one buff from this Support is live on ANY target including self.
// Count-independent by construction — the Charge component must call THIS, never a target count.
UFUNCTION(BlueprintPure) bool HasAnyBuffActive() const;
```
Writing it as a boolean rather than a count is how §5's acceptance criterion 3 ("buff-uptime
generation is provably count-independent") becomes structurally true instead of a thing to test for.

**MISSING HOOK — assists.** "+8 for damage to an enemy killed by an ally within 5s" needs a
recent-damager ledger on the victim: `UBreakerCombatComponent::GetRecentDamagers(float WithinSeconds) const`,
populated in `ReceiveDamage` using the new `Instigator` field (§7).

| # | Ability | GAS class | Notes / missing hooks |
|---|---|---|---|
| U1 | **Patch** *(starter, Medic)* | `_Instant` | Instant heal, **applies to self at full value**. `ApplyHealing`. **GAP [O2]:** cost, CD, heal amount. |
| U2 | **Purge** *(Medic)* — 3s status immunity | `_Window` | **MISSING HOOK:** `UBreakerStatusComponent::CleanseAll()` and `PushStatusImmunity(FGameplayTag, float Duration)`. Self-castable. **GAP [O2]:** cost, CD. |
| U3 | **Cadence** *(starter, Conductor)* — 8s aura | `_Window` + buff | Reload speed and swap tempo; applies to self. **MISSING HOOK:** `UBreakerWeaponComponent::PushReloadSpeedMultiplier` / `PushSwapSpeedMultiplier` (tag-keyed push/pop, same shape as `PushCadenceMultiplier`). **GAP [O2]:** cost, CD, magnitudes. |
| U4 | **Metronome** *(Conductor)* | `_Window` + buff | Stacking cadence bonus per consecutive hit, allies including self. Reuses SI-9 streaks and `PushCadenceMultiplier`. **GAP [O2]:** cost, CD, per-stack value, stack cap. |
| U5 | **Mark** *(starter, Warden)* — 10s | `_Instant` | `UBreakerMarkComponent` (§4.6) — same component as Swift's Lead, different `MarkTag`. Marked targets take increased damage and generate Charge when damaged. **GAP [O2]:** cost, CD, damage increase. |
| U6 | **Suppress** *(Warden)* | `_Zone` | Slows and reduces enemy accuracy. Slow reuses Gunsmith's `PushSpeedMultiplier`; **MISSING HOOK:** enemy accuracy — `ABreakerEnemy::PushAccuracyMultiplier(FGameplayTag, float)` / `Pop`. **GAP [O2]:** cost, CD, magnitudes, radius. |

**ULTIMATE — Conduit.** 100 Charge. 12s: all Support abilities affect every valid target in 15 m
simultaneously and cost no Charge. Solo, every self-buff runs at once — so the "valid targets"
query must include self unconditionally, or the solo case silently does nothing.
Keystones: **Triage** — continuous healing + prevents one lethal hit per target (**MISSING HOOK:**
`UBreakerCombatComponent::PushLethalDamagePrevention(FGameplayTag, int32 Charges)`, resolved in
`ReceiveDamage` before death is broadcast); **Downbeat** — cadence effects doubled and extended to
weapon damage as a **flat** contribution (`PushFlatDamageBonus`, §4.2 — flat, so it stays out of
the Increased bucket); **Blackout** — marks and suppresses every enemy in radius, and the Support's
own damage against marked targets is this class's More multiplier.

**GAP [O2]:** every Support ability cost and cooldown (§5 gives the band "4–10s cooldowns" and
nothing per-ability), all magnitudes, and Gunsmith/Tank/Support's nine More multiplier values
(Class-Kits §6.1 marks them "TBD — to be authored with the full treatments").

---

# 9. The two tree-granted verbs

These are **Core Tree** grants, not class abilities. They do not occupy a class ability slot and
they are not in `FBreakerAbilityLoadout` — which means the granting path is different from
everything above and must be built deliberately.

**Shared requirement.** Both are granted by a `UBreakerProgressionNode` purchase in the Core Tree,
must survive save/load (the save stores IDs and ranks only, and the verbs are re-granted on load —
Core-Constellations §10.3.6), and must be revoked correctly on respec (§10.3.4). The granting
mechanism: `UBreakerAbilityComponent::RefreshGrantedAbilities` also walks `CoreNodeRanks` and
grants any node carrying a `GrantedAbility` reference — so extend `UBreakerProgressionNode` with:

```cpp
// Progression/BreakerProgressionNode.h
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grants")
TObjectPtr<UBreakerAbilityDefinition> GrantedAbility;   // null for the vast majority of nodes
```

**The inert-node test (§10.3.5)** is the one most likely to be silently broken: Bulwark's B3 Read
(+parry window) and Kinesis's K3 Loft (+air jump speed retention) are purchasable *before* their
verb and must then produce no observable effect and no error. Implement both as passive GEs
modifying an attribute that simply has no consumer until the verb exists — never as a direct call
into the verb's ability. Write the test first.

## 9.1 AIR JUMP — Kinesis K4 (Notable, 2 points)

**Design source.** Core-Constellations §8 K4. "Unlocks a single mid-air jump, refreshed on landing,
on wall contact, and on a successful Dodge."

**GAS mapping.** This one is **not** a `UGameplayAbility`. It is a **movement component verb**
plus a grant flag. Making it an ability means routing a jump through GAS activation latency and
fighting `UCharacterMovementComponent`'s network prediction, which already handles jumps correctly.

- Grant: a passive GE from K4 adds `Verb.AirJump.Granted` to the ASC.
- `ABreakerCharacter::HandleJumpInput` (already exists) checks, in order: grounded jump → wall jump
  (`TryWallJump`, exists) → **air jump if granted and available**.

**MISSING HOOK — `UBreakerCharacterMovementComponent`:**
```cpp
UFUNCTION(BlueprintCallable, Category="Movement") bool TryAirJump(float SpeedRetentionFraction);
UFUNCTION(BlueprintPure, Category="Movement")     bool IsAirJumpAvailable() const;
UFUNCTION(BlueprintCallable, Category="Movement") void RefreshAirJump();   // called on land / wall contact / dodge
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement") bool bAirJumpGranted = false;
```
Refresh points, and **only** these three (§10.3.10 makes this an explicit criterion — "specifically
it must not refresh on dash"):
1. `OnLanded` / ground contact.
2. Wall contact — hook `BeginWallRide` and the wall-jump path.
3. Successful dodge — bind `UBreakerCombatComponent::OnDodgeEvaded` (§4.0 missing hook).
   K7 Link Weave makes an airborne dodge refresh *immediately* rather than on landing; the base
   rule already refreshes on dodge, so K7's difference is the timing — model it as
   `RefreshAirJump()` being called at dodge time vs. queued to next landing.

`SpeedRetentionFraction` comes from K3 Loft ranks plus the `Air Jump Speed Retention %` gear affix
(§3.3) — read via `GetAffixValue` (§4.4). Master 5.4 guardrail: air jump must not exceed the
existing `MomentumHardCap` and must not self-accelerate past sprint horizontally.

**Replication.** Handled entirely by `UCharacterMovementComponent`'s existing prediction — which
means `bAirJumpGranted` and the availability flag **must be part of the saved move / compressed
flags**, or the client predicts a jump the server refuses. This is the single most likely bug in
this feature. Add it as a custom saved-move flag alongside the existing dash/slide handling.

**Tasks.** (1) `TryAirJump` + availability + saved-move flag. (2) Three refresh points, and a test
that dash does **not** refresh. (3) `Verb.AirJump.Granted` tag + K4 passive GE.
(4) K3 Loft as an inert-until-K4 attribute. (5) Save/load round trip test (§10.3.6).

## 9.2 PARRY — Bulwark B4 (Notable, 2 points)

**Design source.** Core-Constellations §7 B4. "A timed defensive input with a base 0.18s window. A
successful Parry fully negates the incoming hit, staggers the attacker for 1.2s, and refunds its
own cost." Per **O1**, Parry "uses its own short cooldown" and is the **only defensive input in the
game** (block and dodge are passive).

**GAS mapping.** This one **is** a `UGameplayAbility` — `UBreakerGameplayAbility_Window`,
`LocalPredicted`, granted by B4's node.
- Cost: **none.** O1 deleted stamina; §7's "refunds its own cost" is now vacuous. Do not invent a
  resource cost. Cooldown only.
- Cooldown: `GE_CD_Core_Parry`. **GAP [O2]:** O1 says "its own short cooldown" and names no
  duration. This is the single most load-bearing missing number in the document — parry cooldown
  determines whether the only defensive input in the game is spammable.
- Window: `Window.Core.Parry`, base 0.18s, extended by B3 Read ranks (+0.04 / 0.08 / 0.12s).
- `ActivationBlockedTags`: `Cooldown.Core.Parry`, `State.Dead`.

**Runtime systems driven.**
- **MISSING HOOK — parry resolution inside the damage pipeline.** `ReceiveDamage` currently rolls
  dodge then block. Parry must resolve **before both**, as a deterministic check, not a roll:

```cpp
// Combat/BreakerCombatTypes.h — FBreakerDamageResult
UPROPERTY(BlueprintReadOnly) bool bParried = false;
// Combat/BreakerCombatComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerParryEvent, AActor*, Attacker);
UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerParryEvent OnParrySucceeded;
```
  Resolution order becomes: **parry (deterministic) → dodge (roll) → block (roll) → armour → shield
  → health.** Parry does not apply to DoTs, consistent with dodge and block.
- **MISSING HOOK — stagger** (shared with Tank's Ground Zero, §7):
  `UBreakerCombatComponent::ApplyStagger(float Duration)`, plus `ABreakerEnemy` honouring it by
  suspending its behavior for the duration. B6 Unyielding (cannot be staggered while shield intact)
  is a check inside `ApplyStagger`, so it works for both directions.
- B7 Link Riposte ("a successful Block within 0.5s of a *failed* Parry re-opens the window once")
  needs a failed-parry timestamp — store it on the ability state component, and bind
  `OnBlockRolled`.
- B8 Link Plate (parry grants 20 Armour for 6s, stacking to 5) is a plain stacking GE. ✅ No new hook.

**Replication.** `LocalPredicted` activation so the window opens on the input frame — parry feel is
entirely about that. **But the parry *result* is server-authoritative:** the server decides whether
the incoming hit landed inside the window, using the server's window state. Client-side hit
negation is not acceptable. The client shows an optimistic parry flash on activation and a
confirm/deny cosmetic from `OnParrySucceeded`. This is the same shape as the existing hit-marker
flow.

**Input.** `UBreakerInputConfig::Parry` (§2.2). Core-Constellations OQ7 is still open — a character
with no other defensive input having a dedicated defensive key is unusual and may want a design
pass. The implementation should not presume the key.

**Tasks.** (1) Parry step in `ReceiveDamage` + `bParried`. (2) `ApplyStagger` + enemy honouring.
(3) `_Window` GA + cooldown GE (value GAP-flagged). (4) Input action + binding.
(5) B3 Read as an inert-until-B4 window extension. (6) B7 / B8 nodes.
(7) Test: parry negates exactly one hit inside the window and zero outside it, under 200ms of
simulated latency.

---

# 10. Master table — every ability × GAS class × missing hooks × dependency risk

Risk key: **L** = existing systems only · **M** = one new hook or a bounded new system ·
**H** = a new subsystem or an unresolved design dependency.

| # | Ability | Class | GAS class | Net policy | Cost / CD | Key missing hooks | Risk |
|---|---|---|---|---|---|---|---|
| S1 | Slipcut | Swift | `_Window` | LocalPredicted | 20 / 4s | `PushCadenceMultiplier`, `GetActiveCooldownCount` | M |
| S2 | Cadence Break | Swift | `_Window` | LocalPredicted | 35 / 8s | `CompleteReloadImmediately`, `PushFlatDamageBonus`, SI-8, SI-9 | M |
| S3 | Skim | Swift | `_Instant` | LocalPredicted | 15 / 3s | `TryRedirect`, `ConsumeAirborneAction`, `GetAirtimeFacingDelta` | M |
| S4 | Hard Stop | Swift | `_Window` | LocalPredicted | 30 / 6s | `CancelVelocity`, `GetAffixValue`, `PushIncomingDamageModifier` | M |
| S5 | Sightline | Swift | `_Window` | LocalPredicted | 25 / 6s | multi-hit pierce trace, `FBreakerPendingShotModifier`, **enemy cover state** | H |
| S6 | Lead | Swift | `_Instant` | ServerOnly | 40 / 10s | **`UBreakerMarkComponent`**, distance-gated weak-point forcing | H |
| — | **Overdrive** | Swift | `_Ultimate` | ServerOnly | 100 / — | `PushLoopOverride`, `PushMomentumFloor`, dash-charge + wall-ride + range overrides | H |
| C1 | Cleave | Caster | `_Instant` | ServerOnly | 20 / — | **melee sweep system**, `Damage.Melee` tag | H |
| C2 | Closequarter | Caster | `_Instant` | ServerOnly | 35 / — | `TryBlinkTo`, `bBypassDefensiveRolls` | M |
| C3 | Rot | Caster | `_Zone` | ServerOnly | 25 / — | **`ABreakerZoneActor`**, `PushArmorReduction` | H |
| C4 | Siphon | Caster | `_Channel` | ServerOnly | 30 / — | `ApplyHealing` + `FBreakerHealResult`, channel-break binding | M |
| C5 | Fracture | Caster | `_Instant` | ServerOnly | 30 / — | `ABreakerProjectileBase`, **`UBreakerStatusCycleComponent`**, HUD cycle feed | H |
| C6 | Resonance | Caster | `_Instant` | ServerOnly | 40 / — | `GetDistinctStatusTypeCount`, `ConsumeAllStatuses`, `ScaleRemainingDurations` | M |
| — | **Unmake** | Caster | `_Ultimate` | ServerOnly | 80 / — | Caster cost override, Mana loop override, `SetExpiryPaused`, Cascade re-entrancy guard | H |
| G1 | Sidearm Rig | Gunsmith | `_Window` | LocalPredicted | — / 10s | reuses `FBreakerPendingShotModifier` | L |
| G2 | Overhaul | Gunsmith | `_Window` | LocalPredicted | — / GAP | `PushMagazineCapacityOverride` | M |
| G3 | Turret | Gunsmith | `_Deployable` | ServerOnly | 40 / — | **deployable system**, autonomous targeting, instigator attribution | H |
| G4 | Ammo Crate | Gunsmith | `_Deployable` | ServerOnly | 30 / — | deployable system, interact prompt | M |
| G5 | Mine Cluster | Gunsmith | `_Deployable` | ServerOnly | 35 / — | deployable system, data-driven trigger conditions | H |
| G6 | Disruptor | Gunsmith | `_Deployable`+`_Zone` | ServerOnly | 45 / — | `PushSpeedMultiplier` (enemy), `PushArmorReduction` | M |
| — | **Field Assembly** | Gunsmith | `_Ultimate` | ServerOnly | 100 / — | `PushDensityCapOverride`, `SetLifetimePaused`, **Machinist's per-deployable weapon mapping** | H |
| T1 | Rend | Tank | `_Instant` | ServerOnly | GAP | melee sweep, `ApplyHealing` with shield routing | H |
| T2 | Bloodline | Tank | `_Window` | LocalPredicted | GAP | Life-on-Hit read, DoT ticks through `OnHitDealt` | M |
| T3 | Anchor Point | Tank | `_Deployable` | ServerOnly | GAP | deployable system | M |
| T4 | Provoke | Tank | `_Instant` | ServerOnly | GAP | **`ABreakerEnemy::ForceTarget`** (no threat system exists) | H |
| T5 | Breach Charge | Tank | `_Instant` | ServerOnly | GAP | `FBreakerDamageRequest::Instigator`, O13 self-damage reduction + knockback control | M |
| T6 | Ground Zero | Tank | `_Instant` | ServerOnly | GAP | **`ApplyStagger`**, radial damage from airborne | M |
| — | **Hold** | Tank | `_Ultimate` | ServerOnly | 100 / — | `PushIncomingDamageCap`, ally query, absorbed-damage accumulator, **early-end input** | H |
| U1 | Patch | Support | `_Instant` | ServerOnly | GAP | `ApplyHealing` | L |
| U2 | Purge | Support | `_Window` | ServerOnly | GAP | `CleanseAll`, `PushStatusImmunity` | M |
| U3 | Cadence | Support | `_Window` | ServerOnly | GAP | **`UBreakerBuffComponent`**, `PushReloadSpeedMultiplier`, `PushSwapSpeedMultiplier` | H |
| U4 | Metronome | Support | `_Window` | ServerOnly | GAP | buff component, SI-9 streaks, `PushCadenceMultiplier` | M |
| U5 | Mark | Support | `_Instant` | ServerOnly | GAP | `UBreakerMarkComponent` (shared with S6) | M |
| U6 | Suppress | Support | `_Zone` | ServerOnly | GAP | zone actor, `PushSpeedMultiplier`, `PushAccuracyMultiplier` | M |
| — | **Conduit** | Support | `_Ultimate` | ServerOnly | 100 / — | radius target query incl. self, `PushLethalDamagePrevention`, `PushFlatDamageBonus` | H |
| V1 | **Air Jump** | Kinesis | *movement verb* | CMC prediction | — / — | `TryAirJump`, `RefreshAirJump`, **saved-move flag**, `OnDodgeEvaded` | H |
| V2 | **Parry** | Bulwark | `_Window` | LocalPredicted | — / GAP | **parry step in `ReceiveDamage`**, `ApplyStagger`, `OnParrySucceeded`, input action | H |

### 10.1 The fifteen keystone rewrites

All fifteen use the D1 tag-driven variant pattern. All fifteen depend on SI-7 (the `MoreMultiplier`
array) for their More.

| Keystone | Class | Rewrite mechanism | Extra hook |
|---|---|---|---|
| Bloodrhythm | Swift/Frenzy | `OnHitDealt` refund + hit-timeout early exit | — |
| Terminal Velocity | Swift/Kinetic | dash-charge + wall-ride-timer overrides | `PushDashChargeOverride`, `PushWallRideDurationOverride` |
| Standing Wave | Swift/Marksman | freeze loop + range treatment | `PushRangeTreatmentOverride` |
| Edgework | Caster/Spellblade | animation-lock → 0, blink range → LOS | — |
| Long Dark | Caster/Void Whisperer | duration ×2, cost scalar 0→0.5, zone expiry paused | `SetExpiryPaused` |
| Cascade | Caster/Multispell | `OnStatusApplied` → apply next cycle position at proc 0 | re-entrancy guard |
| Machinist | Gunsmith/Armory | deployable effect → weapon modifier mapping | per-type mapping table (H) |
| Foundry | Gunsmith/Field Tech | lifetime paused | `SetLifetimePaused` |
| Minefield | Gunsmith/Tinkerer | visibility + AI perception exclusion | perception hook |
| Vein | Tank/Leech | incoming damage → healing conversion | `ApplyHealing` |
| Wall | Tank/Bastion | mitigation to allies in 8 m; ×2 solo | ally query, `UBreakerPartyPolicy` |
| Detonation | Tank/Demolitionist | absorbed-damage accumulator + early end | **second input binding** |
| Triage | Support/Medic | continuous heal + lethal prevention | `PushLethalDamagePrevention` |
| Downbeat | Support/Conductor | cadence ×2 + flat weapon damage | `PushFlatDamageBonus` |
| Blackout | Support/Warden | mass mark + suppress; the class More | `UBreakerMarkComponent` |

### 10.2 Top missing hooks by fan-in

| Rank | Hook | Abilities/nodes needing it |
|---|---|---|
| 1 | `UBreakerCombatComponent::OnHitDealt` / `OnKillDealt` + `FBreakerHitContext` (SI-8) | ~24 abilities and the majority of all 60 class nodes |
| 2 | Outgoing-damage modifier chain (`PushFlatDamageBonus`, `PushArmorReduction`, `PushIncomingDamageModifier`, `bBypassDefensiveRolls`, `PierceIndex`, `MoreMultipliers`) | ~18 |
| 3 | `UBreakerAbilityComponent` grant/slot/cooldown + `OnProgressionChanged` (SI-1/SI-2) | all 35 — nothing activates without it |
| 4 | `UBreakerAbilityStateComponent` windows + per-target streaks (SI-9) | ~13 |
| 5 | `UBreakerCombatComponent::ApplyHealing` + `FBreakerHealResult` | ~9 (all of Medic, Leech, Siphon, Triage, Vein) |
| 6 | `UBreakerMarkComponent` | ~7 (S6, M5, M8, M11, Support Mark, Warden branch, Blackout) |
| 7 | Deployable system + density cap | ~7 (five Gunsmith, Anchor Point, Field Assembly) |
| 8 | Movement push/pop overrides (`TryRedirect`, `CancelVelocity`, `TryBlinkTo`, `TryAirJump`, dash/wall-ride/speed overrides) | ~7 |
| 9 | `FBreakerDamageRequest::Instigator` | Grit self-damage, Support assists, rocket self-damage (O13), all kill attribution |
| 10 | `ApplyStagger` | Ground Zero, Parry, B6 Unyielding |

---

# 11. GAP [O2] register

Every number the implementation needs that no design document supplies. Per O2 these stay unfilled
until wave-mode instrumentation reports. Structure is specified; only magnitudes are missing.

**Blocking a shipping ability:**

1. **Parry cooldown.** O1 says "its own short cooldown," names no value. Parry is the only
   defensive input in the game; this number defines it. *Highest-priority gap in this document.*
2. **S2 Cadence Break** — flat damage per stack (10 stacks max is given; the per-stack value is not).
3. **C1 Cleave** — damage coefficient vs. weapon damage; the Bleed spec it applies.
4. **C4 Siphon** — damage per tick, tick interval, heal portion, base channel-break threshold.
5. **C6 Resonance** — burst damage per distinct status; MS9's fixed-per-status and 3+ flat values.
6. **C3 Rot** — Poison spec magnitude (radius 4 m, duration 6s, armour −40 are given).
7. **S4 Hard Stop** — depends on the *Damage Reduction While Airborne* affix value, which lives in
   the affix tables and is itself placeholder. Structurally fine; numerically downstream.

**Blocking the one-page classes entirely** (these need a design pass, not just a number):

8. Every **Gunsmith** ability's cooldown except Sidearm Rig's 10s; all damage/health/range values
   for Turret, Mine Cluster, Disruptor; Ammo Crate's refill amount and charge count; Overhaul's
   conversion ratio and duration beyond "10s".
9. Every **Tank** ability's cost and cooldown (the band "5–12s" is given, nothing per-ability); Rend's
   leech portion; Provoke's per-enemy damage bonus; Breach Charge's self-damage reduction within
   O13's ≤80% ceiling; Ground Zero's stagger duration and radius; Hold's fixed per-hit damage cap.
10. Every **Support** ability's cost and cooldown (band "4–10s" only); all heal, buff, mark, slow,
    and accuracy-reduction magnitudes; Conduit's healing rate under Triage.
11. **Nine More multiplier values** — Gunsmith, Tank, and Support, one per branch keystone.
    Class-Kits §6.1 marks all nine "TBD — to be authored with the full treatments."

**Cross-cutting:**

12. **Momentum's 25/s and Mana's 20/s global caps** are placeholders authored against no TTK
    (Class-Kits OQ7) — every Swift and Caster ability's economy sits on top of them.
13. **Rift and Time statuses do not exist.** O5 names the three elements; only Void has a design.
    Multispell's cycle and the Elements constellation both widen when they land. Nothing in this
    document is blocked by it — the cycle is built from available types by construction (§5.5).

**Explicitly NOT gaps — resolved by O-rulings, contrary to Class-Kits' own open-questions list:**

- Class-Kits OQ1 (block/dodge model) — **resolved by O1**: passive chance, no stamina. The shipped
  `UBreakerCombatComponent` already matches. Class-Kits §8.1 and Core-Constellations §7's CONFLICT
  block are both stale.
- Class-Kits OQ2 (More ordering) — **resolved by O3**: unordered product, hard cap 3. SI-7
  implements it.
- Class-Kits OQ8 (do keystone rewrites become separate ability assets) — **resolved by D1**: no.
- Class-Kits OQ9 (Tick Frequency blocking VW11) — **resolved by O10**: tick interval is part of the
  DoT snapshot, discrete intervals. VW11 therefore applies **at application time only**, exactly as
  its own CONFLICT note requires. VW11 is unblocked.
- Class-Kits OQ4 (does Overcast break GAS cost prediction) — **proposed resolution in D8**, still
  needs a technical spike before Caster prototyping starts.

---

# 12. Test plan summary

Every acceptance criterion in Class-Kits §1.7 / §2.7 and Core-Constellations §10.3 should land in
`Source/RiorsEdge/Tests/` as automation tests alongside the existing 19. The ones that are pure
structure and can be written **before** O2 lifts:

| Test | Source |
|---|---|
| No input pattern generates > global cap Momentum/s | Class-Kits §1.7.3 |
| Overdrive cannot re-cast within 8s under any node combination | §1.7.5 |
| Class-layer More never exceeds one multiplier, ≤ hard cap 3, order-independent | O3 / SI-7 |
| Shotgun and rifle Casters generate Mana within 15% over 30s (Multishot 1/n) | §2.7.2 |
| DoT ticks generate zero Mana under every node combination | §2.7.3 |
| Cascade + MS4 terminates within one generation, 20 enemies in 10 m | §2.7.6 |
| Two overlapping Rots do not double-reduce armour, never drive it negative | §5.3 |
| Hard Stop / K10 immunity uptime ≤ 0.6s per 6s | Master 7.10.4 |
| `TryRedirect` never increases horizontal speed | Master 5.4 |
| Blink never places the player inside geometry | §5.2 |
| Air jump refreshes on land / wall / dodge and **not** on dash | Core-Constellations §10.3.10 |
| Read at rank 3 without Parry, Loft at rank 3 without Air Jump: no effect, no error | §10.3.5 |
| Verbs survive save/load; respec revokes them | §10.3.4, §10.3.6 |
| Parry negates exactly one hit inside the window, zero outside, under 200ms latency | §9.2 |
| Deployable density never exceeds the cap under Field Assembly + any nodes | Class-Kits §3 |
| Overheal generates zero Charge under all node combinations | Class-Kits §5 |
| Content validation: every shipped ability definition has real values or is on the §11 GAP list | O2 enforcement |
