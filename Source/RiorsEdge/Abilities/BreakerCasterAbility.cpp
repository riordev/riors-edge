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
    return ComposeResourceCost(AuthoredCost, 1.0f, WindowScalar);
}

float UBreakerCasterAbility::ComposeResourceCost(float AuthoredCost, float CostMultiplier, float WindowScalar)
{
    return FMath::Max(0.0f, AuthoredCost * FMath::Max(0.0f, CostMultiplier) * FMath::Max(0.0f, WindowScalar));
}

float UBreakerCasterAbility::GetResourceCostMultiplier() const
{
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    if (!Attributes) return 1.0f;

    // BRIDGE, and it is meant to be short-lived. The Items/ lane owns
    // EBreakerAggregatedAttribute::ResourceCostMultiplier and the attribute
    // that carries it; this lane owns the CONSUMER and had to land first. The
    // attribute is resolved by reflection so that this code is correct both
    // before the attribute exists (no property found -> 1.0, every cost is the
    // authored one, nothing changes) and the moment it does, with no second
    // commit needed to switch it on.
    //
    // WHEN THE ATTRIBUTE LANDS this whole body collapses to
    //     return FMath::Max(MinimumResourceCostMultiplier, Attributes->GetResourceCostMultiplier());
    // and the lookup below should be deleted. Leaving it would be a slow,
    // reflective read of something with a generated accessor.
    static const FStructProperty* CostMultiplierProperty = CastField<FStructProperty>(
        UBreakerAttributeSet::StaticClass()->FindPropertyByName(TEXT("ResourceCostMultiplier")));
    if (!CostMultiplierProperty || CostMultiplierProperty->Struct != FGameplayAttributeData::StaticStruct())
    {
        return 1.0f;
    }
    const FGameplayAttributeData* Data = CostMultiplierProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(Attributes);
    // Floored here as well as on the aggregator's side: a cost of exactly zero
    // reached by gear would delete the class's only pacing mechanism.
    return Data ? FMath::Max(MinimumResourceCostMultiplier, Data->GetCurrentValue()) : 1.0f;
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
    // Read live, never cached (owner ruling 2026-08-14): the player re-gears
    // mid-fight and a stale efficiency would quote a price the bank is not
    // being charged. 1.0 until the affix layer supplies otherwise.
    const float CostMultiplier = GetResourceCostMultiplier();
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UBreakerAbilityStateComponent* State = Character ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    // A window with no payload authored would silently make everything free;
    // default to full price so a mis-authored Unmake fails safe.
    const float WindowScalar = (State && State->IsWindowActive(UnmakeWindowKey()))
        ? State->GetWindowPayload(UnmakeWindowKey(), 1.0f)
        : 1.0f;
    return ComposeResourceCost(Authored, CostMultiplier, WindowScalar);
}
