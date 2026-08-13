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
    UFUNCTION(BlueprintPure, Category="Abilities") float GetResourceCost() const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCooldownSeconds() const;
    UFUNCTION(BlueprintPure, Category="Abilities") ABreakerCharacter* GetBreakerCharacter() const;
    UFUNCTION(BlueprintPure, Category="Abilities") UBreakerAttributeSet* GetBreakerAttributes() const;
    UFUNCTION(BlueprintPure, Category="Abilities") float GetCurrentClassResource() const;

    // Pure rule, exposed for tests: an ability is affordable when the owner's
    // class resource is at or above its cost. Free abilities are always
    // affordable.
    static bool IsAffordable(float CurrentResource, float Cost);

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
