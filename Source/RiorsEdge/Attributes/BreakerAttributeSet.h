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

    UPROPERTY(BlueprintReadOnly, Category="Vitals") FGameplayAttributeData Health;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Health)
    UPROPERTY(BlueprintReadOnly, Category="Vitals") FGameplayAttributeData MaxHealth;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxHealth)
    UPROPERTY(BlueprintReadOnly, Category="Movement") FGameplayAttributeData MoveSpeed;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MoveSpeed)
};
