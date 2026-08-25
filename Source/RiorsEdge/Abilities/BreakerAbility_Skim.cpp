#include "Abilities/BreakerAbility_Skim.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

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

int32 UBreakerAbility_Skim::MaxAirborneUses(bool bHasSkimDiscipline)
{
    // Class-Kits §1.2 S3: "Usable airborne once per airtime." §1.4 K7:
    // "Skim may be used twice per airtime instead of once." Transcribed.
    return bHasSkimDiscipline ? 2 : 1;
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

    // The pitch-gated Hard Stop branch that used to live here is retired
    // (O177): Hard Stop is its own equippable ability with S4's own cost and
    // cooldown (UBreakerAbility_HardStop). Skim is the redirect, whole.

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

        // The cut, drawn where it happened: two cyan chevron strokes opening
        // along the NEW line at the feet — the departure point, deliberately
        // left behind so the player reads the corner they just made. Only a
        // redirect that RAN draws; a refused one (below walk speed) showing a
        // flash would be the ability lying about itself. Figures O2
        // PLACEHOLDER. (Server-vs-client caveat in BreakerEffectRenderer.h.)
        if (UWorld* World = Character->GetWorld())
        {
            if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
            {
                const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
                const FVector Line = HorizontalDirectionForView(ViewRotation);
                const FVector Side = FVector::CrossProduct(Line, FVector::UpVector).GetSafeNormal();
                BreakerFX::FEffectTiming CutTiming;
                CutTiming.DurationSeconds = 0.28f;
                CutTiming.FadeInSeconds = 0.0f;
                CutTiming.FadeOutSeconds = 0.22f;
                Effects->AddStroke(Feet - Side * 45.0f - Line * 20.0f, Feet + Line * 110.0f, 3.5f, BreakerUI::Cyan, 2.6f, CutTiming);
                Effects->AddStroke(Feet + Side * 45.0f - Line * 20.0f, Feet + Line * 110.0f, 3.5f, BreakerUI::Cyan, 2.6f, CutTiming);
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
