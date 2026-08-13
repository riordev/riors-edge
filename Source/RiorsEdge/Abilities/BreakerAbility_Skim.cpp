#include "Abilities/BreakerAbility_Skim.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

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

FName UBreakerAbility_Skim::BurstKey()
{
    return TEXT("Skim");
}

bool UBreakerAbility_Skim::ShouldHardStop(bool bHasSkimDiscipline, float ViewPitchDegrees, float PitchThresholdDegrees)
{
    return bHasSkimDiscipline && FRotator::NormalizeAxis(ViewPitchDegrees) <= PitchThresholdDegrees;
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

    const FRotator ViewRotation = Character->GetControlRotation();
    const UBreakerProgressionComponent* Progression = Character->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bHasSkimDiscipline = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SkimDiscipline.GetTag());

    if (ShouldHardStop(bHasSkimDiscipline, static_cast<float>(ViewRotation.Pitch), HardStopPitchDegrees))
    {
        // Hard Stop: cancels all velocity instantly. The cost is already
        // committed, which is the whole point of the node — dumping Momentum
        // to stop is a tactical option, not a free brake.
        // GAP: the 0.6s Damage-Reduction-While-Airborne treatment on the ground
        // (S4's other half) needs a defensive-modifier push on Combat/.
        Movement->Velocity = FVector::ZeroVector;
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // Pure redirect: no speed floor, no bonus, and no dash charge consumed.
    // TryRedirect is allowed to fail (below walk speed) after the cost is
    // committed; that matches the design — Skim is a verb for a moving player,
    // and refunding here would make it a free "am I fast enough" probe.
    if (Movement->TryRedirect(HorizontalDirectionForView(ViewRotation)))
    {
        // The redirect itself cannot add speed (Master 5.4), so the burst is
        // pushed as a max-speed multiplier the player accelerates into rather
        // than as an impulse. That keeps the no-self-acceleration rule intact
        // while making the new line read as a commitment, not a shrug.
        Movement->PushSpeedMultiplier(BurstKey(), BurstSpeedMultiplier, BurstSeconds);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
