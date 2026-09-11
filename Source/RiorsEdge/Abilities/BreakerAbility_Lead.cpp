#include "Abilities/BreakerAbility_Lead.h"

#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerEffectCompositions.h"
#include "UI/BreakerUIStyle.h"

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

float UBreakerAbility_Lead::DefaultMinimumRangeCm()
{
    const UBreakerAbility_Lead* Defaults = GetDefault<UBreakerAbility_Lead>();
    return Defaults ? Defaults->MarkMinimumRangeCm : 2500.0f;
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
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, Params) && Hit.GetActor())
    {
        const UBreakerCombatComponent* Combat = Hit.GetActor()->FindComponentByClass<UBreakerCombatComponent>();
        if (Combat && !Combat->IsDead()) MarkedTarget = Hit.GetActor();
    }

    // The window opens whether or not a target was found: the cost is already
    // committed and a missed mark should not silently become a free cast. The
    // HUD reads the window; the target read is what gates the weak-point rule.
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    const float Duration = Definition ? Definition->WindowDuration : 6.0f;
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
        // Publish the mark alongside the window so anything outside this
        // ability instance — starting with the HUD's target diamond — can see
        // *which* actor was marked, not merely that a mark is running.
        const UBreakerProgressionComponent* Progression = Character->GetProgression();
        const int32 Capacity = Progression && Progression->GetNodeRank(TEXT("Swift.Marksman.Lead"), EBreakerPointCurrency::DoctrinePoints) > 0 ? 2 : 1;
        if (Capacity == 1) State->SetMark(MarkedTarget.Get(), Duration);
        else State->AddMark(MarkedTarget.Get(), Duration, Capacity);
    }

    // The painting, drawn once at cast: a thin gold line from the eye to
    // where the mark landed (gold is the weak-point family, which is what a
    // mark promises), and a small glow at the point. The mark's LIFETIME is
    // the HUD diamond's job — a six-second world primitive on a moving target
    // would lie about where the target is. Figures O2 PLACEHOLDER.
    // (Server-vs-client caveat recorded once in BreakerEffectRenderer.h.)
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        const FVector MarkPoint = MarkedTarget.IsValid() ? Hit.ImpactPoint : TraceEnd;
        BreakerFX::FEffectTiming PaintTiming;
        PaintTiming.DurationSeconds = 0.30f;
        PaintTiming.FadeInSeconds = 0.03f;
        PaintTiming.FadeOutSeconds = 0.22f;
        // The start hangs off the weapon side, not the eye: a stroke fired
        // from the camera's own origin is seen end-on — a dot — by the one
        // player it is for (caught by the ability probe's first photograph).
        const FVector PaintSide = FVector::CrossProduct(ViewRotation.Vector(), FVector::UpVector).GetSafeNormal();
        Effects->AddStroke(ViewLocation + ViewRotation.Vector() * 90.0f + PaintSide * 25.0f - FVector(0.0f, 0.0f, 20.0f),
            MarkPoint, 2.0f, GetPresentationColor(), 2.6f, PaintTiming);
        if (MarkedTarget.IsValid())
        {
            const FLinearColor Paint = GetPresentationColor();
            Effects->AddGlow(MarkPoint, 28.0f, Paint, 3.2f, PaintTiming);
            // THE MARK IS A RING AROUND THE BODY, not a dot on it: a gold band
            // at the mark's height that opens round the target in a tenth of a
            // second and holds for most of a second, so "that one is marked" is
            // readable from across the yard and not only at the instant of
            // the cast. The mark itself lasts longer; this is its arrival.
            BreakerFX::FEffectTiming BandTiming;
            BandTiming.DurationSeconds = 0.9f;
            BandTiming.FadeInSeconds = 0.02f;
            BandTiming.FadeOutSeconds = 0.5f;
            BreakerFXCompose::GroundRing(Effects, MarkedTarget->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f),
                70.0f, Paint, 3.0f, 2.8f, BandTiming, 0.1f);
        }
    }

    // The mark is consumed in UBreakerWeaponComponent::FireOnce: a hit on the
    // marked actor beyond the range gate forces bWeakPointHit on the damage
    // request, through ShouldTreatAsWeakPoint. UBreakerMarkComponent (spec
    // §4.6) replaces MarkedTarget when it lands; the consumer reads the state
    // component, not this instance, so that swap does not touch the weapon.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
