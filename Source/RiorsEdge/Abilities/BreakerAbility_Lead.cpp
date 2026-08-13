#include "Abilities/BreakerAbility_Lead.h"

#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

UBreakerAbility_Lead::UBreakerAbility_Lead()
{
    FallbackAbilityId = TEXT("Swift.Lead");
    // Spec §4.6: the mark mutates another actor, so there is nothing worth
    // predicting except the reticle.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Lead.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_Lead.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Lead::WindowKey()
{
    return TEXT("Window.Swift.Lead");
}

bool UBreakerAbility_Lead::ShouldTreatAsWeakPoint(bool bTargetIsMarked, float DistanceCm, float MinimumRangeCm)
{
    return bTargetIsMarked && DistanceCm > MinimumRangeCm;
}

void UBreakerAbility_Lead::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerLeadMark), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MarkTraceDistanceCm;
    MarkedTarget = nullptr;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params) && Hit.GetActor())
    {
        MarkedTarget = Hit.GetActor();
    }

    // The window opens whether or not a target was found: the cost is already
    // committed and a missed mark should not silently become a free cast. The
    // HUD reads the window; the target read is what gates the weak-point rule.
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    const float Duration = Definition ? Definition->WindowDuration : 6.0f;
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }

    // GAP: nothing consumes the mark yet. The distance-gated weak-point forcing
    // belongs in the outgoing-damage builder (Combat/), which is owned
    // elsewhere; ShouldTreatAsWeakPoint is the rule it should call, and
    // UBreakerMarkComponent (spec §4.6) replaces MarkedTarget when it lands.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
