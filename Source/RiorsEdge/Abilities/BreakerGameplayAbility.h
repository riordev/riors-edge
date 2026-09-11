#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerGameplayAbility.generated.h"

enum class EBreakerAbilityDurationKind : uint8 { Generic, Zone, Window, Buff };
struct FBreakerNodeStats;

class ABreakerCharacter;
class UBreakerAbilityDefinition;
class UBreakerAttributeSet;

// Instant, one Additive modifier of the SetByCaller Data.AbilityCost magnitude
// on ClassResource. The caller passes a negative value; the amount lives in the
// ability definition, never in the effect (spec D3).
UCLASS()
class RIORSEDGE_API UBreakerAbilityCostEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UBreakerAbilityCostEffect();
};

// Duration, no modifiers. The cooldown tag is added to the spec dynamically so
// one effect class serves every ability.
UCLASS()
class RIORSEDGE_API UBreakerAbilityCooldownEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UBreakerAbilityCooldownEffect();
};

// Base for every class ability (spec D2/SI-5). Cost and cooldown are driven
// entirely by the ability's UBreakerAbilityDefinition; subclasses implement
// behavior only.
UCLASS(Abstract)
class RIORSEDGE_API UBreakerGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerGameplayAbility();
    virtual void PostInitProperties() override;
    // Timed buffs are not interrupted merely because their caster staggers.
    // Channels and pending casts opt in explicitly.
    virtual bool IsStaggerInterruptible() const { return false; }
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    // Resolved definition: the explicitly assigned asset if present, otherwise
    // the C++ fallback registry entry for FallbackAbilityId.
    UFUNCTION(BlueprintPure, Category="Abilities") const UBreakerAbilityDefinition* GetAbilityDefinition() const;
    // Virtual because a live window may rewrite the price of a cast: Caster's
    // Unmake makes every Caster ability free for its duration (Class-Kits §2.2).
    // CheckCost and ApplyCost both read through this, so there is exactly one
    // answer to "what does this cost right now".
    // WHAT COLOUR THIS ABILITY DRAWS IN THE WORLD (O179: colour by VERB, never
    // by class or slot; the ultimate slot is violet whatever its verb).
    //
    // Every ability used to hand-pick a token at its own draw site, and three
    // of them picked wrong: Cleave's swing, Siphon's beam and Resonance's
    // detonation were all painted BreakerUI::Cyan, which is a literal alias of
    // VerbMove — the MOVEMENT colour — on a weapon, a leech and an explosion.
    // The HUD has read the verb table for its ability rails since O179; this is
    // the same table, reachable from the world draw, so the tile on the HUD and
    // the effect in the world cannot disagree about what an ability is.
    UFUNCTION(BlueprintPure, Category="Abilities") FLinearColor GetPresentationColor() const;

    UFUNCTION(BlueprintPure, Category="Abilities") float GetResourceCost() const;
    virtual float GetUnmodifiedResourceCost() const;
    virtual float GetAuthoredResourceCost() const;
    float GetLastPaidResourceCost() const { return LastPaidResourceCost; }
    virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

    // --- The cast (O266) ----------------------------------------------------
    // Pure rule: the wind-up an ability actually serves. The multiplier is the
    // Cast Speed lane's divisor and is 1.0 until that lane exists, so this is
    // the seam the lane plugs into rather than a second place to author time.
    UFUNCTION(BlueprintPure, Category="Abilities|Cast")
    static float EffectiveCastSeconds(float AuthoredSeconds, float CastSpeedMultiplier);

    // The HUD window key a pending cast holds. Prefixed so the existing
    // ability-window bar draws it with no HUD change at all (O179: a window is
    // a HUD bar), and per-ability so two casts can never share a countdown.
    static FName CastWindowKey(FName AbilityId);

    // THE GATE, called as the first line of an ability's ActivateAbility.
    //
    // Returns TRUE when the ability should resolve NOW — either it authors no
    // cast time, or its cast has just finished and this is the re-entry.
    // Returns FALSE when it has started a cast: cost is already paid (O266
    // spends on the keypress), the window is open, and the timer will call
    // ActivateAbility again when the wind-up completes.
    //
    // Deliberately a one-line gate rather than a ResolveCast refactor: every
    // ability's body stays exactly where it is and keeps its own commit,
    // which is what makes this safe to apply across a whole class in one pass.
    bool BeginCastIfNeeded(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo);

    // THE TWO CAST-START SEAMS, and both exist because a wind-up moves WHEN an
    // ability's decisions happen, not just when its effect lands.
    //
    // PrepareCast runs BEFORE the cost is committed and may REFUSE. Without it
    // every "this key would do nothing, do not charge for it" guard an ability
    // owns silently moved to the far side of the payment: Resonance refuses to
    // charge 40 Mana for a target with no statuses, and once a wind-up exists
    // that check runs after the Mana is gone. A refusal here ends the ability
    // exactly as a failed commit does.
    //
    // OnCastBegan runs once the wind-up is actually running — cost paid, window
    // open. It is for the part of an ability that must start at the CAST rather
    // than at the landing: Unmake suspends Mana generation, and a suspension
    // that waited for the landing would let the bank refill during the cast,
    // which is the debt interaction the ultimate is built on.
    //
    // Both are no-ops by default, so an ability that authors no cast time and
    // an ability that overrides neither behave exactly as they did.
    virtual bool PrepareCast() { return true; }
    virtual void OnCastBegan() {}

    // --- The queue (O271) ---------------------------------------------------
    // A press during a wind-up queues ONE cast, fired when the wind-up
    // resolves; the aim is solved at the queued press. GAS refuses a second
    // activation of an active InstancedPerActor instance, so before this the
    // second press was swallowed and the owner's next puddle needed a third
    // press timed after a landing he could not see.
    //
    // PrepareQueuedCast runs AT THE QUEUED PRESS and holds whatever the press
    // decides into a second slot — Rot solves its aim there. It may refuse,
    // and a refusal is a dropped press, never a queued one. PromoteQueuedCast
    // runs at the first landing, after the body has consumed its own snapshot,
    // and moves the queued decision into the active slot; the fresh cast that
    // follows skips PrepareCast so the promoted snapshot is used as-is.
    // Both are no-ops by default: an ability that overrides neither still
    // queues (the second press is not lost) and solves nothing early.
    virtual bool PrepareQueuedCast() { return true; }
    virtual void PromoteQueuedCast() {}
    // The press that GAS refused. True when it was queued; false when nothing
    // is casting or one press is already waiting — a third press is dropped.
    bool QueuePressDuringCast();
    bool HasQueuedCast() const { return bCastQueued; }

    // Damage interrupts a cast (O266). No refund — the Mana went on the
    // keypress and an interrupted cast is a loss, which is the whole point of
    // a wind-up being a risk.
    UFUNCTION() void HandleCastInterrupt(const FBreakerDamageResult& Result);
    bool IsCasting() const { return bCastPending; }

    // A CANCELLED CAST MUST NOT STILL LAND. The wind-up timer was cleared on
    // exactly one path — a damage interrupt — and this class overrode nothing,
    // so any OTHER way an ability ends while winding up (CancelAllAbilities, a
    // class swap, death, a cancel tag) left the timer running: it fired on its
    // own clock, re-entered ActivateAbility with bCastPending still set, and
    // the ability resolved in full after it had been cancelled. Every ability
    // that authors a wind-up was exposed, not only the two that gained one.
    // Teardown is unconditional here, in the precedent Unmake's own EndAbility
    // sets, because a cast that resolves after its cancel is the same species
    // of failure as a Caster left with free casts.
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    void EndCastBinding();
    // The wind-up timer's landing. A member rather than the lambda's own body
    // because a queued cast re-arms CastTimer from INSIDE this callback, and
    // the timer manager destroys the executing timer's delegate — the closure
    // — the moment its handle is re-set. Everything here is on the stack.
    void ResolveCast(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        FGameplayAbilityActivationInfo ActivationInfo);
public:
    virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
    virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    // The LIVE cooldown: the definition's authored seconds divided by the
    // owner's composed AbilityCooldown reduction. ApplyCooldown and the HUD
    // both read through this, so the timer started and the timer displayed
    // are one number.
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownSeconds() const;
    UFUNCTION(BlueprintPure, Category="Abilities") ABreakerCharacter* GetBreakerCharacter() const;
    UFUNCTION(BlueprintPure, Category="Abilities") UBreakerAttributeSet* GetBreakerAttributes() const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCurrentClassResource() const;
    // The owner's ClassResourceFloor (spec D8). Zero for every class that has
    // not opened a debt allowance, which is all of them but an Overcasting
    // Caster.
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCurrentClassResourceFloor() const;

    // O35: ABILITIES RIDE GEAR DEPTH. The item-level damage scalar every class
    // ability's damage multiplies by — the EQUIPPED WEAPON's
    // (1 + w)^(ilvl - 1), read off the owner's weapon component so `w` is the
    // component's live ItemLevelDamageGrowth, never a copy. Unequipped (or no
    // weapon component at all) is item level 1, where the scalar is EXACTLY
    // 1.0 — the anchor that keeps every authored ability number bit-identical
    // at the bottom of the curve. Without this, weapon base damage grows
    // (1 + w)^(ilvl - 1) while every flat ability number stands still, and the
    // whole ABILITIES axis (O30) decays x74 against the curve by ilvl 50.
    // Static and null-safe so application sites and tests share one seam.
    static float AbilityDamageScalarFor(const AActor* OwnerActor);

    // ---- Ability geometry seam (2026-08-16) -------------------------------
    // The base-class accessors EBreakerNodeStatTarget's AbilityArea /
    // AbilityDuration / AbilityCooldown comments ask for: radius, arc, range
    // and duration are differently-named UPROPERTYs per subclass
    // (Cleave::RangeCm / ArcDegrees, Rot::RadiusCm / DurationSeconds), so the
    // composed tree multiplier is read HERE, once, and each geometry-owning
    // subclass applies it to its own numbers through its Effective* accessors.
    // All three read the owner's progression component's aggregated node
    // stats; null-safe (no owner, no progression, no ranks) is exactly 1.0,
    // so every authored number is bit-identical for a build that owns none.
    // Static and actor-parameterised like AbilityDamageScalarFor, for the
    // same reason: application sites and tests share one seam.
    static float AbilityAreaMultiplierFor(const AActor* OwnerActor);
    static float AbilityCastRateMultiplierFor(const AActor* OwnerActor);
    static float AbilityChannelRateMultiplierFor(const AActor* OwnerActor);
    static float ComposeAbilityDurationMultiplier(const FBreakerNodeStats& Stats, EBreakerAbilityDurationKind Kind);
    static float AbilityDurationMultiplierFor(const AActor* OwnerActor, EBreakerAbilityDurationKind Kind = EBreakerAbilityDurationKind::Generic);
    // O252: the shared ability skill level this owner carries, 1-15. Public
    // so the HUD can print it — a chase the player cannot see is dead content.
    UFUNCTION(BlueprintPure, Category="Abilities") static int32 SkillLevelFor(const AActor* OwnerActor);
    static float AbilityBaseDamageFor(const AActor* OwnerActor, float ScaledAuthoredBase);
    // The cooldown DIVISOR (DashCooldownReduction's convention: 1.20 == 20%
    // shorter). Never at or below zero — the aggregator floors it.
    static float AbilityCooldownReductionFor(const AActor* OwnerActor);

    // Instance forms, reading the current avatar. Virtual so a future variant
    // ability can re-site WHICH multiplier its geometry answers to; every
    // subclass today uses the owner's composed values unchanged.
    UFUNCTION(BlueprintPure, Category="Abilities") virtual float GetAbilityAreaMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Abilities") virtual float GetAbilityDurationMultiplier() const;

    // Pure rule: authored cooldown seconds under a reduction divisor. A
    // non-positive authored cooldown stays non-positive — Caster abilities
    // author none at all (Mana IS the cooldown, T8), and no divisor may
    // invent one for them.
    static float ScaledCooldownSeconds(float AuthoredSeconds, float ReductionDivisor);

    // Pure rule, exposed for tests: an ability is affordable when the owner's
    // class resource is at or above its cost. Free abilities are always
    // affordable.
    static bool IsAffordable(float CurrentResource, float Cost);
    // The same rule, aware of a negative floor (spec D8). With Floor == 0 it is
    // literally the expression above, so no zero-floor class can observe a
    // difference. With a negative floor a cast may drive the bank down TO the
    // floor and no further, and nothing may be cast while already below zero:
    // Overcast is a debt, not a spiral. Refusal is deliberate — truncating the
    // spend at the floor would hand the player a silent discount, which is
    // worse than a refused cast.
    static bool IsAffordableWithFloor(float CurrentResource, float Cost, float Floor);

    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual const FGameplayTagContainer* GetCooldownTags() const override;

protected:
    // Explicit definition asset. Left null in the zero-setup path.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities") TObjectPtr<UBreakerAbilityDefinition> AbilityDefinition;
    // Fallback registry key, used when AbilityDefinition is null.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities") FName FallbackAbilityId = NAME_None;

private:
    mutable FGameplayTagContainer CachedCooldownTags;
    bool bCostSnapshotActive = false;
    float CommitCostSnapshot = 0.0f;
    float LastPaidResourceCost = 0.0f;
    // Cast state. bCastPending says a wind-up is running; bCastCommitted says
    // its price is already paid, so the re-entry's own CommitAbility must not
    // charge a second time.
    bool bCastPending = false;
    bool bCastCommitted = false;
    // O271: one press waiting behind the running wind-up. Cleared by the
    // landing that fires it and by every interrupt and cancel.
    bool bCastQueued = false;
    // The fresh cast a promoted queue starts must not re-run PrepareCast:
    // that would solve the aim at the landing, which is exactly the drift the
    // queue exists to avoid. Read and cleared by the next BeginCastIfNeeded.
    bool bSkipPrepareOnce = false;
    FGameplayAbilitySpecHandle CastHandle;
    FGameplayAbilityActivationInfo CastActivationInfo;
    FTimerHandle CastTimer;
};
