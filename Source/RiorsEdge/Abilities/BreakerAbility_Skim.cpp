#include "Abilities/BreakerAbility_Skim.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"

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

FName UBreakerAbility_Skim::HardStopWindowKey()
{
    return TEXT("HardStop");
}

bool UBreakerAbility_Skim::ShouldHardStop(bool bHasSkimDiscipline, float ViewPitchDegrees, float PitchThresholdDegrees)
{
    return bHasSkimDiscipline && FRotator::NormalizeAxis(ViewPitchDegrees) <= PitchThresholdDegrees;
}

int32 UBreakerAbility_Skim::MaxAirborneUses(bool bHasSkimDiscipline)
{
    // Class-Kits §1.2 S3: "Usable airborne once per airtime." §1.4 K7:
    // "Skim may be used twice per airtime instead of once." Transcribed.
    return bHasSkimDiscipline ? 2 : 1;
}

float UBreakerAbility_Skim::HardStopCostMultiplier(bool bWouldHardStop, bool bHasSpendToLive)
{
    // Node text (Swift.Kinetic.SpendToLive): "it costs twice the Momentum."
    // Only a cast that resolves as Hard Stop pays it; a redirect is untouched.
    return (bWouldHardStop && bHasSpendToLive) ? 2.0f : 1.0f;
}

float UBreakerAbility_Skim::HardStopIncomingMultiplier(bool bHasSpendToLive, float ReductionFraction)
{
    // K10: "Hard Stop's window becomes true immunity" — the incoming chain's
    // own 0.0 (UBreakerCombatComponent documents 0.0 as immune). Base S4:
    // a reduction for the window, fraction O2 PLACEHOLDER (see header).
    if (bHasSpendToLive)
    {
        return 0.0f;
    }
    return 1.0f - FMath::Clamp(ReductionFraction, 0.0f, 1.0f);
}

bool UBreakerAbility_Skim::WouldResolveAsHardStop() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character)
    {
        return false;
    }
    const UBreakerProgressionComponent* Progression = Character->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bHasSkimDiscipline = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SkimDiscipline.GetTag());
    return ShouldHardStop(bHasSkimDiscipline, static_cast<float>(Character->GetControlRotation().Pitch), HardStopPitchDegrees);
}

float UBreakerAbility_Skim::GetResourceCost() const
{
    const float BaseCost = Super::GetResourceCost();
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UBreakerProgressionComponent* Progression = Character ? Character->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const bool bHasSpendToLive = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SpendToLive.GetTag());
    return BaseCost * HardStopCostMultiplier(WouldResolveAsHardStop(), bHasSpendToLive);
}

void UBreakerAbility_Skim::HandleLanded(const FHitResult& Hit)
{
    AirborneUsesThisAirtime = 0;
}

void UBreakerAbility_Skim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerCharacterMovementComponent* Movement = Character ? Character->GetBreakerMovement() : nullptr;
    if (!Movement)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // The airtime counter's reset half, bound once per instance (the ability
    // is InstancedPerActor and has no BeginPlay of its own).
    if (!bLandedResetBound)
    {
        Character->LandedDelegate.AddDynamic(this, &UBreakerAbility_Skim::HandleLanded);
        bLandedResetBound = true;
    }

    const UBreakerProgressionComponent* Progression = Character->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bHasSkimDiscipline = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SkimDiscipline.GetTag());

    // Once per airtime, twice with K7 (Class-Kits §1.2 S3 / §1.4 K7). Refused
    // BEFORE the commit, so a spent airtime costs no Momentum and starts no
    // cooldown — the ability simply is not usable again until landing.
    const bool bAirborne = Movement->IsFalling();
    if (bAirborne && AirborneUsesThisAirtime >= MaxAirborneUses(bHasSkimDiscipline))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (bAirborne)
    {
        ++AirborneUsesThisAirtime;
    }

    const FRotator ViewRotation = Character->GetControlRotation();

    if (ShouldHardStop(bHasSkimDiscipline, static_cast<float>(ViewRotation.Pitch), HardStopPitchDegrees))
    {
        // Hard Stop: cancels all velocity instantly. The cost is already
        // committed — doubled by GetResourceCost when Spend to Live is owned —
        // which is the whole point of the node: dumping Momentum to stop is a
        // tactical option, not a free brake.
        Movement->Velocity = FVector::ZeroVector;

        // S4's protective window, through the incoming-damage chain Combat/
        // already exposes (PushIncomingDamageModifier lands at the same stage
        // as gear-rolled physical reduction; 0.0 is immune). Base: 0.6s of the
        // Damage-Reduction-While-Airborne treatment on the ground (§1.2 S4;
        // the affix value is an O2 PLACEHOLDER until the affix exists). With
        // K10 Spend to Live: true immunity for the same 0.6s. Server-side
        // fact, so authority only; the window is strictly shorter than Skim's
        // cooldown, so the timed removal can never strip a newer window.
        const bool bHasSpendToLive = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SpendToLive.GetTag());
        UWorld* World = Character->GetWorld();
        if (Character->HasAuthority() && World)
        {
            if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->PushIncomingDamageModifier(HardStopWindowKey(), HardStopIncomingMultiplier(bHasSpendToLive, HardStopDamageReductionFraction));
                TWeakObjectPtr<UBreakerCombatComponent> WeakCombat(Combat);
                FTimerHandle RemoveHandle;
                World->GetTimerManager().SetTimer(RemoveHandle, FTimerDelegate::CreateWeakLambda(this, [WeakCombat]()
                {
                    if (UBreakerCombatComponent* Restored = WeakCombat.Get())
                    {
                        Restored->RemoveIncomingDamageModifier(HardStopWindowKey());
                    }
                }), HardStopWindowSeconds, false);
            }
        }
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
