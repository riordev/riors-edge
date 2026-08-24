#include "Abilities/BreakerAbility_Closequarter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

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

float UBreakerAbility_Closequarter::EffectiveRefundGate(bool bHasNoDistance, float BaseThreshold)
{
    // 100% means "always", because a health fraction cannot exceed one. Stated
    // as the gate rather than as a bool so the caller keeps ONE code path and
    // the node is a data change, exactly as this ability's header has promised
    // since it was written.
    return bHasNoDistance ? 1.0f : BaseThreshold;
}

bool UBreakerAbility_Closequarter::ShouldRefund(float TargetHealthFraction, float Threshold)
{
    // "at or below" — the boundary refunds.
    return TargetHealthFraction >= 0.0f && TargetHealthFraction <= Threshold;
}

bool UBreakerAbility_Closequarter::ShouldBlinkUntargeted(bool bTargetFound, bool bHasBlinkNode)
{
    return !bTargetFound && bHasBlinkNode;
}

FVector UBreakerAbility_Closequarter::UntargetedBlinkDestination(const FVector& CasterLocation, const FVector& AimDirection, float RangeCm)
{
    const FVector Direction = AimDirection.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        return CasterLocation;
    }
    return CasterLocation + Direction * FMath::Max(0.0f, RangeCm);
}

float UBreakerAbility_Closequarter::EffectiveRangeCm(bool bEdgeworkDuringUnmake, float AuthoredRangeCm, float UnrestrictedRangeCm)
{
    // Max, not replacement: an unrestricted range authored below the base
    // range must never SHORTEN the blink.
    return bEdgeworkDuringUnmake ? FMath::Max(AuthoredRangeCm, UnrestrictedRangeCm) : AuthoredRangeCm;
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

    // Edgework's Closequarter half (Class-Kits §2.2): "during Unmake,
    // Closequarter has no range limit within line of sight." Two-part gate,
    // the exact idiom Cleave's animation-lock half uses: the keystone tag says
    // the rewrite is owned, the live Unmake window says it is currently
    // rewriting. The tag alone is permanent from node purchase and gating on
    // it alone already shipped one bug (D10). The trace IS the line-of-sight
    // clause — a wall still stops it at any range.
    const bool bHasEdgework = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
        && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Edgework.GetTag());
    const UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    const bool bDuringUnmake = State && State->IsWindowActive(UnmakeWindowKey());
    const float TraceRangeCm = EffectiveRangeCm(bHasEdgework && bDuringUnmake, MaximumRangeCm, UnrestrictedRangeCm);

    // Target acquisition happens BEFORE the commit: unlike Skim, a
    // Closequarter with nothing under the crosshair has no effect at all, and
    // charging 35 Mana for a cast that provably cannot move the player is a
    // dead key, not a risk the design asked for. (SB7 below is the one
    // exception, and it moves the player every time.)
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerClosequarter), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceRangeCm;
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

    if (!Target)
    {
        // SB7 "Blink": "Closequarter may be cast with no target to blink 12 m
        // in the aim direction." Same verb, same machinery: the destination is
        // where the targeted blink would have gone, the teleport is the same
        // swept move, the arrival carries no velocity. Without the node the
        // empty-crosshair cast stays a refused, uncharged dead key.
        const UBreakerProgressionComponent* Progression = Character->FindComponentByClass<UBreakerProgressionComponent>();
        const bool bHasBlinkNode = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SB_Blink.GetTag());
        if (!ShouldBlinkUntargeted(false, bHasBlinkNode) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }

        // The node text says 12 m — which is C2's own MaximumRangeCm, so the
        // distance is transcribed, not authored here. Deliberately NOT the
        // Edgework trace range: "no range limit within line of sight" needs a
        // target to sight; the free-aimed blink stays 12 m.
        const FVector BlinkDestination = UntargetedBlinkDestination(Character->GetActorLocation(), ViewRotation.Vector(), MaximumRangeCm);
        FHitResult UntargetedHit;
        Character->SetActorLocation(BlinkDestination, /*bSweep*/ true, &UntargetedHit, ETeleportType::TeleportPhysics);
        if (UPawnMovementComponent* Movement = Character->GetMovementComponent())
        {
            Movement->Velocity = FVector::ZeroVector;
        }
        // No target, no refund: the refund gate reads target health.
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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

    // SB10 "No Distance", the rule half: the refund gate moves from 40% target
    // health to 100%, which turns Closequarter from an execute tool into a
    // traversal tool -- every cast pays back, so the blink is affordable as
    // movement rather than only as a finisher. The header has described this as
    // "a data change to RefundHealthFraction, not a branch" since the ability
    // was written; this is that data change, and it is the first of the Caster
    // tier-4 rules to have a reader.
    //
    // The cost half rides with it. A gate that always opens and a cost that
    // never rises would be a straight upgrade, and SB10 is authored as a
    // rewrite: the refund is universal and the cast is expensive.
    const UBreakerProgressionComponent* RefundProgression = Character->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bNoDistance = RefundProgression && RefundProgression->HasNodeTag(BreakerNodeTags::Node_SB_NoDistance.GetTag());

    if (ShouldRefund(HealthFraction, EffectiveRefundGate(bNoDistance, RefundHealthFraction)))
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
