#include "Abilities/BreakerAbility_HardStop.h"

#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

UBreakerAbility_HardStop::UBreakerAbility_HardStop()
{
    FallbackAbilityId = TEXT("Swift.HardStop");
    // Prediction for the same reason as Skim: a server round trip on a
    // movement verb feels broken (spec §4.3).
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_HardStop.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_HardStop.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_HardStop::IncomingModifierKey()
{
    // The key Skim's folded branch used, kept byte-identical: it is what a
    // suite assertion or a live modifier readout keys on, and nothing else
    // pushes it.
    return TEXT("HardStop");
}

FName UBreakerAbility_HardStop::WindowKey()
{
    // Window.Swift. prefix: the HUD draws every window in that family
    // (BreakerPlaytestHUD's prefix scan), so the 0.6s guard gets its bar for
    // free.
    return TEXT("Window.Swift.HardStop");
}

float UBreakerAbility_HardStop::CostMultiplier(bool bHasSpendToLive)
{
    return bHasSpendToLive ? 2.0f : 1.0f;
}

float UBreakerAbility_HardStop::IncomingMultiplier(bool bHasSpendToLive, float ReductionFraction)
{
    if (bHasSpendToLive)
    {
        return 0.0f;
    }
    return 1.0f - FMath::Clamp(ReductionFraction, 0.0f, 1.0f);
}

bool UBreakerAbility_HardStop::OwnerHasSpendToLive() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UBreakerProgressionComponent* Progression = Character ? Character->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SpendToLive.GetTag());
}

float UBreakerAbility_HardStop::GetResourceCost() const
{
    return Super::GetResourceCost() * CostMultiplier(OwnerHasSpendToLive());
}

void UBreakerAbility_HardStop::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerCharacterMovementComponent* Movement = Character ? Character->GetBreakerMovement() : nullptr;
    if (!Movement || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // The verb: all velocity cancelled, instantly. The doubled Spend to Live
    // cost is already committed, which is the node's point — dumping Momentum
    // to stop is a purchase, not a free brake.
    Movement->Velocity = FVector::ZeroVector;

    UWorld* World = Character->GetWorld();

    // S4's protective window, through the incoming-damage chain Combat/
    // exposes (PushIncomingDamageModifier lands at the same stage as
    // gear-rolled physical reduction; 0.0 is immune). Server-side fact, so
    // authority only; the window is strictly shorter than the cooldown, so
    // the timed removal can never strip a newer window's entry.
    if (Character->HasAuthority() && World)
    {
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->PushIncomingDamageModifier(IncomingModifierKey(), IncomingMultiplier(OwnerHasSpendToLive(), DamageReductionFraction));
            TWeakObjectPtr<UBreakerCombatComponent> WeakCombat(Combat);
            FTimerHandle RemoveHandle;
            World->GetTimerManager().SetTimer(RemoveHandle, FTimerDelegate::CreateWeakLambda(this, [WeakCombat]()
            {
                if (UBreakerCombatComponent* Restored = WeakCombat.Get())
                {
                    Restored->RemoveIncomingDamageModifier(IncomingModifierKey());
                }
            }), WindowSeconds, false);
        }
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->StartWindow(WindowKey(), WindowSeconds);
        }
    }

    // The plant, drawn where it happened. Cyan is the movement family's
    // colour (BreakerUIStyle: player/system); the pop is hard (no fade-in)
    // because a dead stop is an impact, not a swell. Figures O2 PLACEHOLDER.
    // (Server-only-vs-client-cosmetic caveat recorded once in
    // BreakerEffectRenderer.h.)
    if (World)
    {
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
            BreakerFX::FEffectTiming PlantTiming;
            PlantTiming.DurationSeconds = 0.30f;
            PlantTiming.FadeInSeconds = 0.0f;
            PlantTiming.FadeOutSeconds = 0.22f;
            Effects->AddGlow(Feet, 55.0f, BreakerUI::Cyan, 3.2f, PlantTiming);
            Effects->AddBlinkLight(Feet + FVector(0.0f, 0.0f, 40.0f), 420.0f, BreakerUI::Cyan, 2400.0f, PlantTiming);
            // Four short strokes bracing outward at the compass points: the
            // stance planting, read from above or the side.
            for (int32 Index = 0; Index < 4; ++Index)
            {
                const float Yaw = 90.0f * Index + 45.0f;
                const FVector Out = FRotator(0.0f, Yaw, 0.0f).Vector();
                Effects->AddStroke(Feet + Out * 25.0f, Feet + Out * 85.0f, 4.0f, BreakerUI::Cyan, 2.4f, PlantTiming);
            }
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
