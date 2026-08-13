#include "Abilities/BreakerAbility_Closequarter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PawnMovementComponent.h"

UBreakerAbility_Closequarter::UBreakerAbility_Closequarter()
{
    FallbackAbilityId = TEXT("Caster.Closequarter");
    // Spec §5.2: a predicted teleport that gets corrected is the worst desync
    // in the game. Eat the latency.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Closequarter.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Closequarter.GetTag());
    SetAssetTags(Tags);
}

FVector UBreakerAbility_Closequarter::ArrivalPoint(const FVector& CasterLocation, const FVector& TargetLocation, float StandoffCm)
{
    FVector Approach = TargetLocation - CasterLocation;
    const double Distance = Approach.Size();
    const double Standoff = FMath::Max(0.0f, StandoffCm);
    if (Distance <= Standoff + KINDA_SMALL_NUMBER)
    {
        // Already inside the standoff: there is nothing to close, and stepping
        // "2 m short" would mean stepping away from the target.
        return CasterLocation;
    }
    Approach /= Distance;
    return CasterLocation + Approach * (Distance - Standoff);
}

bool UBreakerAbility_Closequarter::ShouldRefund(float TargetHealthFraction, float Threshold)
{
    // "at or below" — the boundary refunds.
    return TargetHealthFraction >= 0.0f && TargetHealthFraction <= Threshold;
}

void UBreakerAbility_Closequarter::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World)
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

    // Target acquisition happens BEFORE the commit: unlike Skim, a
    // Closequarter with nothing under the crosshair has no effect at all, and
    // charging 35 Mana for a cast that provably cannot move the player is a
    // dead key, not a risk the design asked for.
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerClosequarter), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaximumRangeCm;
    AActor* Target = nullptr;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, Params) && Hit.GetActor())
    {
        // Only things that can be fought: blinking to a wall is a movement
        // verb the Caster is explicitly not given (Class-Kits §2.7.7).
        if (Hit.GetActor()->FindComponentByClass<UBreakerCombatComponent>())
        {
            Target = Hit.GetActor();
        }
    }

    if (!Target || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FVector Destination = ArrivalPoint(Character->GetActorLocation(), Target->GetActorLocation(), StandoffCm);

    // Swept, so the blink stops at the last non-penetrating position instead of
    // depositing the player inside geometry. bSweep is the whole safety
    // argument here — never replace it with a raw teleport.
    FHitResult BlinkHit;
    Character->SetActorLocation(Destination, /*bSweep*/ true, &BlinkHit, ETeleportType::TeleportPhysics);

    // "no velocity carried" is part of the design, not an implementation
    // detail: the arrival must not fling the player past the target.
    if (UPawnMovementComponent* Movement = Character->GetMovementComponent())
    {
        Movement->Velocity = FVector::ZeroVector;
    }

    float HealthFraction = 1.0f;
    if (const IAbilitySystemInterface* TargetAbilities = Cast<IAbilitySystemInterface>(Target))
    {
        if (const UAbilitySystemComponent* TargetASC = TargetAbilities->GetAbilitySystemComponent())
        {
            if (const UBreakerAttributeSet* TargetAttributes = TargetASC->GetSet<UBreakerAttributeSet>())
            {
                const float MaxHealth = TargetAttributes->GetMaxHealth();
                HealthFraction = MaxHealth > 0.0f ? TargetAttributes->GetHealth() / MaxHealth : 1.0f;
            }
        }
    }

    if (ShouldRefund(HealthFraction, RefundHealthFraction))
    {
        if (UBreakerManaComponent* Mana = GetManaComponent())
        {
            // The refund bypasses the per-second generation cap: it is a
            // payback, not generation, and metering it would make the finisher
            // read as broken.
            Mana->GrantMana(RefundMana, /*bIgnoreGlobalCap*/ true);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
