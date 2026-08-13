#include "Abilities/BreakerCasterAbility.h"

#include "Abilities/BreakerAbilityStateComponent.h"
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

bool UBreakerCasterAbility::CanCastAt(float CurrentMana, float Cost)
{
    // Strict until the ClassResourceFloor attribute exists (see the header).
    return Cost <= 0.0f || CurrentMana >= Cost;
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
