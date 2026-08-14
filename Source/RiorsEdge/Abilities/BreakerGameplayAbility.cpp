#include "Abilities/BreakerGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerAbilityCostEffect::UBreakerAbilityCostEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UBreakerAttributeSet::GetClassResourceAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat CostMagnitude;
    CostMagnitude.DataTag = BreakerAbilityTags::Data_AbilityCost.GetTag();
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(CostMagnitude);
    Modifiers.Add(Modifier);
}

UBreakerAbilityCooldownEffect::UBreakerAbilityCooldownEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    FSetByCallerFloat DurationSetByCaller;
    DurationSetByCaller.DataTag = BreakerAbilityTags::Data_AbilityCooldown.GetTag();
    DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);
}

UBreakerGameplayAbility::UBreakerGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // Spec D5: window/impulse abilities predict locally; anything that spawns
    // an actor or mutates another pawn overrides this to ServerOnly.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    CostGameplayEffectClass = UBreakerAbilityCostEffect::StaticClass();
    CooldownGameplayEffectClass = UBreakerAbilityCooldownEffect::StaticClass();
}

const UBreakerAbilityDefinition* UBreakerGameplayAbility::GetAbilityDefinition() const
{
    if (AbilityDefinition)
    {
        return AbilityDefinition;
    }
    return UBreakerAbilityDefinition::FindFallback(FallbackAbilityId);
}

float UBreakerGameplayAbility::GetResourceCost() const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    return Definition ? Definition->ResourceCost : 0.0f;
}

float UBreakerGameplayAbility::GetCooldownSeconds() const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    return Definition ? Definition->CooldownSeconds : 0.0f;
}

float UBreakerGameplayAbility::AbilityDamageScalarFor(const AActor* OwnerActor)
{
    // The weapon component owns both halves of the reading: the equipped item
    // level (unequipped = 1) and the growth `w`. Asking it, rather than
    // recomputing here, is what keeps an editor retune of ItemLevelDamageGrowth
    // reaching abilities and weapon rounds in the same frame.
    const UBreakerWeaponComponent* Weapon = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerWeaponComponent>() : nullptr;
    return Weapon ? Weapon->GetItemLevelDamageScalar() : 1.0f;
}

ABreakerCharacter* UBreakerGameplayAbility::GetBreakerCharacter() const
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    return Info ? Cast<ABreakerCharacter>(Info->AvatarActor.Get()) : nullptr;
}

UBreakerAttributeSet* UBreakerGameplayAbility::GetBreakerAttributes() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    return Character ? Character->GetAttributes() : nullptr;
}

float UBreakerGameplayAbility::GetCurrentClassResource() const
{
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerGameplayAbility::GetCurrentClassResourceFloor() const
{
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    return Attributes ? FMath::Min(0.0f, Attributes->GetClassResourceFloor()) : 0.0f;
}

bool UBreakerGameplayAbility::IsAffordable(float CurrentResource, float Cost)
{
    return IsAffordableWithFloor(CurrentResource, Cost, 0.0f);
}

bool UBreakerGameplayAbility::IsAffordableWithFloor(float CurrentResource, float Cost, float Floor)
{
    if (Cost <= 0.0f) return true;
    const float DebtAllowance = FMath::Min(0.0f, Floor);
    // The closed floor case is written as the original expression rather than
    // as "0 - Cost >= 0" so that every existing class keeps bit-identical
    // affordability, float edges included.
    if (DebtAllowance == 0.0f) return CurrentResource >= Cost;
    // Class-Kits §2.1: "No further ability may be cast until Mana is at or
    // above zero."
    if (CurrentResource < 0.0f) return false;
    return CurrentResource - Cost >= DebtAllowance - KINDA_SMALL_NUMBER;
}

bool UBreakerGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    const float Cost = GetResourceCost();
    if (Cost <= 0.0f)
    {
        return true;
    }
    const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
    {
        return false;
    }
    bool bFound = false;
    const float Current = ASC->GetGameplayAttributeValue(UBreakerAttributeSet::GetClassResourceAttribute(), bFound);
    return bFound && IsAffordable(Current, Cost);
}

void UBreakerGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const float Cost = GetResourceCost();
    if (Cost <= 0.0f || !CostGameplayEffectClass)
    {
        return;
    }
    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(CostGameplayEffectClass, GetAbilityLevel());
    if (!Spec.IsValid())
    {
        return;
    }
    Spec.Data->SetSetByCallerMagnitude(BreakerAbilityTags::Data_AbilityCost.GetTag(), -Cost);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UBreakerGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    // Cost-gated abilities author no cooldown effect at all, so the HUD can
    // tell "no cooldown" from "cooldown of zero" (spec D3).
    if (!Definition || Definition->CooldownSeconds <= 0.0f || !Definition->CooldownTag.IsValid() || !CooldownGameplayEffectClass)
    {
        return;
    }
    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(CooldownGameplayEffectClass, GetAbilityLevel());
    if (!Spec.IsValid())
    {
        return;
    }
    Spec.Data->SetSetByCallerMagnitude(BreakerAbilityTags::Data_AbilityCooldown.GetTag(), Definition->CooldownSeconds);
    Spec.Data->DynamicGrantedTags.AddTag(Definition->CooldownTag);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

const FGameplayTagContainer* UBreakerGameplayAbility::GetCooldownTags() const
{
    CachedCooldownTags.Reset();
    if (const UBreakerAbilityDefinition* Definition = GetAbilityDefinition())
    {
        if (Definition->CooldownTag.IsValid() && Definition->CooldownSeconds > 0.0f)
        {
            CachedCooldownTags.AddTag(Definition->CooldownTag);
        }
    }
    return CachedCooldownTags.IsEmpty() ? nullptr : &CachedCooldownTags;
}
