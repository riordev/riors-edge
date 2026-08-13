#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "BreakerAttributeSet.generated.h"

#define BREAKER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class RIORSEDGE_API UBreakerAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UBreakerAttributeSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Vitals") FGameplayAttributeData Health;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Health)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Vitals") FGameplayAttributeData MaxHealth;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Shield, Category="Defense") FGameplayAttributeData Shield;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Shield)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxShield, Category="Defense") FGameplayAttributeData MaxShield;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxShield)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="Defense") FGameplayAttributeData Armor;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Armor)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ClassResource, Category="Resources") FGameplayAttributeData ClassResource;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, ClassResource)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxClassResource, Category="Resources") FGameplayAttributeData MaxClassResource;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxClassResource)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalChance, Category="Offense") FGameplayAttributeData CriticalChance;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, CriticalChance)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalMultiplier, Category="Offense") FGameplayAttributeData CriticalMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, CriticalMultiplier)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageMultiplier, Category="Offense") FGameplayAttributeData DamageMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, DamageMultiplier)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageOverTimeMultiplier, Category="Offense") FGameplayAttributeData DamageOverTimeMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, DamageOverTimeMultiplier)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeed, Category="Movement") FGameplayAttributeData MoveSpeed;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MoveSpeed)

    // --- Unified attribute application path -------------------------------
    // This attribute set is the ONE owner of the true base value for every
    // attribute in EBreakerAggregatedAttribute. Equipment and progression (and
    // anything added to EBreakerAttributeContributor later) submit a complete
    // contribution and never write those attributes directly; every submission
    // re-derives all of them from bases + all contributions, so order does not
    // matter and removal is exact. See BreakerAttributeAggregation.h.

    // Snapshots the currently authored values as the true bases. Idempotent:
    // whichever component runs first captures pristine values and every later
    // call is a no-op, which is precisely what stops one layer from baking the
    // other layer's bonus into "its" base.
    void CaptureAttributeBases();
    bool HasCapturedAttributeBases() const { return Aggregator.HasCapturedBases(); }

    // Replaces one layer's contribution and re-applies everything.
    void ApplyAttributeContribution(EBreakerAttributeContributor Contributor, const FBreakerAttributeContribution& Contribution);
    // Equivalent to submitting an empty contribution: the layer stops
    // contributing and the composed values return exactly to what they would
    // be had it never contributed at all.
    void ClearAttributeContribution(EBreakerAttributeContributor Contributor);

    float GetAttributeBase(EBreakerAggregatedAttribute Attribute) const { return Aggregator.GetBase(Attribute); }
    float GetComposedAttribute(EBreakerAggregatedAttribute Attribute) const { return Aggregator.Compose(Attribute); }
    const FBreakerAttributeAggregator& GetAttributeAggregator() const { return Aggregator; }

protected:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxShield(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_ClassResource(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxClassResource(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_CriticalChance(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_CriticalMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_DamageMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_DamageOverTimeMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue) const;

private:
    FBreakerAttributeAggregator Aggregator;

    // Re-derives every aggregated attribute from the captured bases and the
    // current contributions. The only place those attributes are written by
    // the aggregation path.
    void RecomputeAggregatedAttributes();
    // Routes through the ability system when there is one, and writes the
    // attribute data directly (same clamp policy) when there is not, so the
    // aggregation is exercisable in tests without an ability system.
    void WriteAggregatedAttribute(const FGameplayAttribute& Attribute, FGameplayAttributeData& Data, float NewValue);
    // Null-safe replacement for GetOwningAbilitySystemComponent(), which
    // CastChecked's the outer to an actor.
    UAbilitySystemComponent* FindOwningAbilitySystemSafe() const;
};
