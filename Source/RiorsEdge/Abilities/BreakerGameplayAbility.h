#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "BreakerGameplayAbility.generated.h"

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

    // Resolved definition: the explicitly assigned asset if present, otherwise
    // the C++ fallback registry entry for FallbackAbilityId.
    UFUNCTION(BlueprintPure, Category="Abilities") const UBreakerAbilityDefinition* GetAbilityDefinition() const;
    // Virtual because a live window may rewrite the price of a cast: Caster's
    // Unmake makes every Caster ability free for its duration (Class-Kits §2.2).
    // CheckCost and ApplyCost both read through this, so there is exactly one
    // answer to "what does this cost right now".
    UFUNCTION(BlueprintPure, Category="Abilities") virtual float GetResourceCost() const;
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
    static float AbilityDurationMultiplierFor(const AActor* OwnerActor);
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
};
