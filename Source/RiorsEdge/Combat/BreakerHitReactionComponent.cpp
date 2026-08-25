#include "Combat/BreakerHitReactionComponent.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace
{
    // TIMINGS only. Every colour this component paints now lives in
    // BreakerBodyPaint.h, because a colour split between the resolver and its
    // caller is the two-owners shape O128 exists to delete. All O2
    // PLACEHOLDER, tuned by eye against capture stills; prefixed for the
    // unity build as always.
    constexpr float BreakerReactionHitFlashSeconds = 0.07f;
    constexpr float BreakerReactionDeathPopSeconds = 0.12f;
    constexpr float BreakerReactionDeathBeatSeconds = 0.45f;
    constexpr float BreakerReactionDeathBeatWeakPointSeconds = 0.60f;
    constexpr float BreakerReactionDeathPopScale = 0.12f;
    constexpr float BreakerReactionDeathPopWeakPointScale = 0.24f;
}

UBreakerHitReactionComponent::UBreakerHitReactionComponent()
{
    // Ticks only for the death beat; idle frames early-out on one float.
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerHitReactionComponent::ApplyBodyPaint()
{
    // The dynamic instance is resolved every time rather than cached. A
    // cached POINTER would be a snapshot with a lifetime, which is the thing
    // this rewrite removed; six casts on a repaint is not a cost worth
    // reintroducing one for.
    const FLinearColor Resolved = BreakerBodyPaint::Resolve(Paint);
    for (const TWeakObjectPtr<UStaticMeshComponent>& Part : Parts)
    {
        if (!Part.IsValid()) continue;
        if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Resolved);
        }
    }
}

void UBreakerHitReactionComponent::RegisterPart(UStaticMeshComponent* Part)
{
    if (!Part) return;
    const int32 Before = Parts.Num();
    Parts.AddUnique(Part);
    // A part joining late lands on the CURRENT resolved colour, not on
    // whatever its constructor left behind.
    if (Parts.Num() != Before) ApplyBodyPaint();
}

void UBreakerHitReactionComponent::SetFamilyPaint(const FLinearColor& InFamilyPaint)
{
    // Early-out like the other three. The enemy pushes all four layers on
    // every damage event, and three of them have not moved — without this a
    // hit costs two full repaints of six parts where it used to cost one.
    if (Paint.FamilyPaint == InFamilyPaint) return;
    Paint.FamilyPaint = InFamilyPaint;
    ApplyBodyPaint();
}

void UBreakerHitReactionComponent::SetRank(EBreakerMonsterRank InRank)
{
    if (Paint.Rank == InRank) return;
    Paint.Rank = InRank;
    ApplyBodyPaint();
}

void UBreakerHitReactionComponent::SetHealthRampEnabled(bool bEnabled)
{
    if (Paint.bHealthRamp == bEnabled) return;
    Paint.bHealthRamp = bEnabled;
    ApplyBodyPaint();
}

void UBreakerHitReactionComponent::SetHealthFraction(float Fraction)
{
    const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);
    if (Paint.HealthFraction == Clamped) return;
    Paint.HealthFraction = Clamped;
    // Only worth a repaint when the ramp is the layer that would move.
    if (Paint.bHealthRamp) ApplyBodyPaint();
}

void UBreakerHitReactionComponent::NotifyHit(bool bWeakPoint)
{
    // Never over the death beat: the crumple owns the colour once it runs,
    // and a finished corpse does not blink.
    if (DeathPresentationElapsed >= 0.0f || bDeathPresentationRan || !GetWorld()) return;
    // A flash landing mid-flash used to be the dangerous case, because the
    // second one would capture the first one's colour as the base. There is
    // no base now: it re-arms the timer and the resting colour underneath is
    // whatever the layers currently say.
    Paint.Reaction = BreakerBodyPaint::EReaction::Flash;
    Paint.bReactionWeakPoint = bWeakPoint;
    ApplyBodyPaint();
    GetWorld()->GetTimerManager().SetTimer(HitFlashTimer, this,
        &UBreakerHitReactionComponent::EndHitFlash, BreakerReactionHitFlashSeconds, false);
}

void UBreakerHitReactionComponent::EndHitFlash()
{
    if (Paint.Reaction == BreakerBodyPaint::EReaction::Rest) return;
    Paint.Reaction = BreakerBodyPaint::EReaction::Rest;
    Paint.ReactionAlpha = 0.0f;
    ApplyBodyPaint();
}

void UBreakerHitReactionComponent::StartDeathPresentation(bool bWeakPointKill)
{
    AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return;
    // Settle any flash in flight so the beat starts from a known phase. It
    // costs one repaint that the next line overwrites, and it is worth it:
    // the timer would otherwise fire mid-beat and reset the reaction to Rest.
    GetWorld()->GetTimerManager().ClearTimer(HitFlashTimer);
    EndHitFlash();
    DeathBaseScale = Owner->GetActorScale3D();
    bDeathBeatWeakPoint = bWeakPointKill;
    DeathPresentationElapsed = 0.0f;
    bDeathPresentationRan = true;
}

void UBreakerHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateDeathPresentation(DeltaTime);
}

void UBreakerHitReactionComponent::UpdateDeathPresentation(float DeltaSeconds)
{
    if (DeathPresentationElapsed < 0.0f) return;
    AActor* Owner = GetOwner();
    if (!Owner) return;
    DeathPresentationElapsed += DeltaSeconds;
    const float Total = bDeathBeatWeakPoint ? BreakerReactionDeathBeatWeakPointSeconds : BreakerReactionDeathBeatSeconds;
    Paint.bReactionWeakPoint = bDeathBeatWeakPoint;
    if (DeathPresentationElapsed >= Total)
    {
        // The crumple landed. The OWNER hides its own body here — an enemy
        // and a dummy hide different sets of things — then the scale is
        // settled and the colour goes back to whatever the layers resolve to,
        // computed rather than restored.
        DeathPresentationElapsed = -1.0f;
        Owner->SetActorScale3D(DeathBaseScale);
        Paint.Reaction = BreakerBodyPaint::EReaction::Rest;
        Paint.ReactionAlpha = 0.0f;
        ApplyBodyPaint();
        OnDeathPresentationFinished.Broadcast();
        return;
    }

    if (DeathPresentationElapsed <= BreakerReactionDeathPopSeconds)
    {
        // Beat one, THE POP: the whole assembly (actor scale, so owner
        // dressing and the elite multiplier ride along) swells and lands
        // back, painted in the flash colour. Weak-point kills pop harder
        // and gold.
        const float Alpha = DeathPresentationElapsed / BreakerReactionDeathPopSeconds;
        const float PopScale = bDeathBeatWeakPoint ? BreakerReactionDeathPopWeakPointScale : BreakerReactionDeathPopScale;
        Owner->SetActorScale3D(DeathBaseScale * (1.0f + PopScale * FMath::Sin(Alpha * PI)));
        Paint.Reaction = BreakerBodyPaint::EReaction::Flash;
        Paint.ReactionAlpha = 0.0f;
    }
    else
    {
        // Beat two, THE CRUMPLE: squash toward the ground, spreading
        // slightly, while the flash colour burns down to ash. Ease-in on the
        // squash so the collapse accelerates like a fall rather than a slide.
        const float Alpha = FMath::Clamp(
            (DeathPresentationElapsed - BreakerReactionDeathPopSeconds)
            / FMath::Max(Total - BreakerReactionDeathPopSeconds, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        const float Eased = Alpha * Alpha;
        const float SquashZ = FMath::Max(0.08f, 1.0f - Eased);
        const float SpreadXY = 1.0f + 0.30f * Eased;
        Owner->SetActorScale3D(DeathBaseScale * FVector(SpreadXY, SpreadXY, SquashZ));
        Paint.Reaction = BreakerBodyPaint::EReaction::DeathCrumple;
        Paint.ReactionAlpha = Eased;
    }
    ApplyBodyPaint();
}

void UBreakerHitReactionComponent::ResetDeathPresentation()
{
    // Settle any reaction first, then the scale. THE COLOUR NEEDS NO RESTORE:
    // dropping back to Rest and repainting resolves the family paint, the
    // rank blend and the health ramp as they currently stand, which is what
    // the pool's revive was previously getting only because its callers
    // happened to run a chassis pass afterwards (O128).
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitFlashTimer);
    Paint.Reaction = BreakerBodyPaint::EReaction::Rest;
    Paint.ReactionAlpha = 0.0f;
    Paint.bReactionWeakPoint = false;
    bDeathPresentationRan = false;
    // Scale is only ever touched by the beat, so it is only restored when a
    // beat ran — an elite's authored scale must never be stamped with the
    // default just because this was called defensively.
    if (DeathPresentationElapsed >= 0.0f)
    {
        DeathPresentationElapsed = -1.0f;
        if (AActor* Owner = GetOwner()) Owner->SetActorScale3D(DeathBaseScale);
    }
    ApplyBodyPaint();
}
