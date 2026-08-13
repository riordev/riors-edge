#include "Abilities/BreakerCasterAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"

UBreakerCasterAbility::UBreakerCasterAbility()
{
    // Class-Kits §2.1: Caster abilities are cost-gated only. Clearing the
    // cooldown effect class means an accidentally authored CooldownSeconds
    // cannot quietly grow a cooldown onto the class that is defined by not
    // having one.
    CooldownGameplayEffectClass = nullptr;
}

FName UBreakerCasterAbility::UnmakeWindowKey()
{
    return TEXT("Window.Caster.Unmake");
}

float UBreakerCasterAbility::CostUnderWindow(float AuthoredCost, float WindowScalar)
{
    return FMath::Max(0.0f, AuthoredCost * FMath::Max(0.0f, WindowScalar));
}

bool UBreakerCasterAbility::CanCastAt(float CurrentMana, float Cost, float Floor)
{
    // One rule for every class, parameterised by the floor (spec D8). Caster is
    // the only class that passes a negative one.
    return IsAffordableWithFloor(CurrentMana, Cost, Floor);
}

bool UBreakerCasterAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    const float Cost = GetResourceCost();
    if (Cost <= 0.0f)
    {
        // Free under Unmake, and a free cast is castable at any bank level —
        // including deep in Overcast, which is the whole point of the
        // Overcast-into-Unmake interaction (Class-Kits §2.2).
        return true;
    }
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
    {
        return false;
    }
    bool bFoundResource = false;
    const float Current = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceAttribute(), bFoundResource);
    bool bFoundFloor = false;
    const float Floor = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceFloorAttribute(), bFoundFloor);
    // A missing floor attribute means a closed floor, never an open one: a
    // lookup failure must not accidentally grant an overdraft.
    return bFoundResource && CanCastAt(Current, Cost, bFoundFloor ? Floor : 0.0f);
}

UBreakerManaComponent* UBreakerCasterAbility::GetManaComponent() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    return Character ? Character->GetMana() : nullptr;
}

float UBreakerCasterAbility::GetResourceCost() const
{
    const float Authored = Super::GetResourceCost();
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UBreakerAbilityStateComponent* State = Character ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    if (!State || !State->IsWindowActive(UnmakeWindowKey()))
    {
        return Authored;
    }
    // A window with no payload authored would silently make everything free;
    // default to full price so a mis-authored Unmake fails safe.
    return CostUnderWindow(Authored, State->GetWindowPayload(UnmakeWindowKey(), 1.0f));
}
