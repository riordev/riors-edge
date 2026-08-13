#include "Abilities/BreakerAbility_Skim.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Movement/BreakerCharacterMovementComponent.h"

UBreakerAbility_Skim::UBreakerAbility_Skim()
{
    FallbackAbilityId = TEXT("Swift.Skim");
    // Prediction is not optional here: a server round trip on a redirect feels
    // broken (spec §4.3).
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Skim.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_Skim.GetTag());
    SetAssetTags(Tags);
}

FVector UBreakerAbility_Skim::HorizontalDirectionForView(const FRotator& ViewRotation)
{
    FVector Direction = FRotationMatrix(FRotator(0.0, ViewRotation.Yaw, 0.0)).GetUnitAxis(EAxis::X);
    Direction.Z = 0.0;
    if (!Direction.Normalize())
    {
        return FVector::ForwardVector;
    }
    return Direction;
}

void UBreakerAbility_Skim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerCharacterMovementComponent* Movement = Character ? Character->GetBreakerMovement() : nullptr;
    if (!Movement)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // TryDash applies the movement component's own short boosted-speed window,
    // which is the only public speed-boost mechanism available.
    Movement->TryDash(HorizontalDirectionForView(Character->GetControlRotation()));

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
