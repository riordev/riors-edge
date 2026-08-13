#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
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
};
