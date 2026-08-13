#include "Abilities/BreakerAbility_Unmake.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBreakerAbility_Unmake::UBreakerAbility_Unmake()
{
    FallbackAbilityId = TEXT("Caster.Unmake");
    // Spec §4.7's reasoning applies unchanged: a multi-second global state
    // change is not worth predicting, and a mispredicted ultimate is the worst
    // possible feel.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ultimate_Unmake.GetTag());
    ActivationBlockedTags.AddTag(BreakerAbilityTags::State_Ultimate_Unmake.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Unmake.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Unmake::GenerationSuspensionKey()
{
    return TEXT("Unmake");
}

float UBreakerAbility_Unmake::ResolveCostScalar(float VariantCostMultiplier)
{
    return FMath::Clamp(VariantCostMultiplier, 0.0f, 1.0f);
}

float UBreakerAbility_Unmake::ResolveDuration(float VariantDuration, float DefinitionDuration)
{
    if (VariantDuration > 0.0f)
    {
        return VariantDuration;
    }
    return FMath::Max(0.0f, DefinitionDuration);
}

void UBreakerAbility_Unmake::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    FGameplayTagContainer OwnerTags;
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(OwnerTags);
    }
    const FBreakerAbilityVariant Variant = Definition
        ? Definition->ResolveVariant(OwnerTags)
        : FBreakerAbilityVariant();

    const float Duration = ResolveDuration(Variant.WindowDuration, Definition ? Definition->WindowDuration : 6.0f);
    const float CostScalar = ResolveCostScalar(Variant.AbilityCostMultiplier);

    if (Duration <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // The window IS the ultimate: every Caster ability reads its payload as
    // the cost scalar through UBreakerCasterAbility::GetResourceCost.
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindowWithPayload(UnmakeWindowKey(), Duration, CostScalar);
    }

    // "Mana generation is suspended" (Class-Kits §2.2). Keyed push/pop so an
    // early end reverts exactly its own entry.
    if (UBreakerManaComponent* Mana = GetManaComponent())
    {
        Mana->PushGenerationSuspension(GenerationSuspensionKey());
    }

    TWeakObjectPtr<UBreakerAbility_Unmake> WeakThis(this);
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateLambda([WeakThis]()
    {
        if (UBreakerAbility_Unmake* Ability = WeakThis.Get())
        {
            Ability->EndAbility(Ability->CurrentSpecHandle, Ability->CurrentActorInfo, Ability->CurrentActivationInfo, true, false);
        }
    }), Duration, false);
}

void UBreakerAbility_Unmake::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UWorld* World = Character->GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
        // Teardown is unconditional, including on cancel and on death: a
        // Caster left with free casts because the ultimate was interrupted is
        // the worst possible failure mode of this design.
        if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
        {
            State->CloseWindow(UnmakeWindowKey());
        }
        if (UBreakerManaComponent* Mana = Character->GetMana())
        {
            Mana->PopGenerationSuspension(GenerationSuspensionKey());
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
