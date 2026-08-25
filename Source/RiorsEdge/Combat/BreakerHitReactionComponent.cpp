#include "Combat/BreakerHitReactionComponent.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace
{
    // All O2 PLACEHOLDER, tuned by eye against capture stills. Moved
    // verbatim from BreakerEnemy.cpp with the extraction; prefixed for the
    // unity build as always.
    constexpr float BreakerReactionHitFlashSeconds = 0.07f;
    constexpr float BreakerReactionDeathPopSeconds = 0.12f;
    constexpr float BreakerReactionDeathBeatSeconds = 0.45f;
    constexpr float BreakerReactionDeathBeatWeakPointSeconds = 0.60f;
    constexpr float BreakerReactionDeathPopScale = 0.12f;
    constexpr float BreakerReactionDeathPopWeakPointScale = 0.24f;
    // Ash: near-black with a breath of the body's violet, so the corpse
    // reads as burnt out rather than as painted black.
    const FLinearColor BreakerReactionDeathAshColor(0.05f, 0.045f, 0.06f);
    const FLinearColor BreakerReactionHitFlashColor(1.35f, 1.30f, 1.25f);
    const FLinearColor BreakerReactionHitFlashWeakPointColor(1.60f, 1.15f, 0.35f);
}

UBreakerHitReactionComponent::UBreakerHitReactionComponent()
{
    // Ticks only for the death beat; idle frames early-out on one float.
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerHitReactionComponent::RegisterPart(UStaticMeshComponent* Part)
{
    if (Part) Parts.AddUnique(Part);
}

void UBreakerHitReactionComponent::CaptureBodyMaterials()
{
    BodyMaterialBases.Reset();
    for (const TWeakObjectPtr<UStaticMeshComponent>& Part : Parts)
    {
        UMaterialInstanceDynamic* Dynamic = Part.IsValid()
            ? Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) : nullptr;
        if (!Dynamic) continue;
        // The CURRENT colour, not the constructor's: owners repaint their
        // bodies (the Altered's severance tint, the Warden's plate) and the
        // restore must return exactly that.
        FLinearColor Base = FLinearColor::White;
        Dynamic->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Base);
        BodyMaterialBases.Emplace(Dynamic, Base);
    }
}

void UBreakerHitReactionComponent::NotifyHit(bool bWeakPoint)
{
    // Never over the death beat: the crumple owns the materials once it
    // runs, and a finished corpse does not blink.
    if (DeathPresentationElapsed >= 0.0f || bDeathPresentationRan || !GetWorld()) return;
    // Capture only from a rested body, so a flash landing mid-flash cannot
    // capture the flash colour as the base and stick the body white.
    if (!bHitFlashActive) CaptureBodyMaterials();
    bHitFlashActive = true;
    const FLinearColor Flash = bWeakPoint ? BreakerReactionHitFlashWeakPointColor : BreakerReactionHitFlashColor;
    for (const auto& Pair : BodyMaterialBases)
    {
        if (Pair.Key.IsValid()) Pair.Key->SetVectorParameterValue(TEXT("Color"), Flash);
    }
    GetWorld()->GetTimerManager().SetTimer(HitFlashTimer, this,
        &UBreakerHitReactionComponent::EndHitFlash, BreakerReactionHitFlashSeconds, false);
}

void UBreakerHitReactionComponent::EndHitFlash()
{
    if (!bHitFlashActive) return;
    bHitFlashActive = false;
    for (const auto& Pair : BodyMaterialBases)
    {
        if (Pair.Key.IsValid()) Pair.Key->SetVectorParameterValue(TEXT("Color"), Pair.Value);
    }
}

void UBreakerHitReactionComponent::StartDeathPresentation(bool bWeakPointKill)
{
    AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return;
    // A flash in flight would have captured true bases; settle it first so
    // the beat's own capture below is honest.
    GetWorld()->GetTimerManager().ClearTimer(HitFlashTimer);
    EndHitFlash();
    CaptureBodyMaterials();
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
    if (DeathPresentationElapsed >= Total)
    {
        // The crumple landed. The OWNER hides its own body here — an enemy
        // and a dummy hide different sets of things — then the scale and
        // colours are settled so a later revive restores from truth.
        DeathPresentationElapsed = -1.0f;
        Owner->SetActorScale3D(DeathBaseScale);
        for (const auto& Pair : BodyMaterialBases)
        {
            if (Pair.Key.IsValid()) Pair.Key->SetVectorParameterValue(TEXT("Color"), Pair.Value);
        }
        OnDeathPresentationFinished.Broadcast();
        return;
    }

    const FLinearColor Flash = bDeathBeatWeakPoint ? BreakerReactionHitFlashWeakPointColor : BreakerReactionHitFlashColor;
    if (DeathPresentationElapsed <= BreakerReactionDeathPopSeconds)
    {
        // Beat one, THE POP: the whole assembly (actor scale, so owner
        // dressing and the elite multiplier ride along) swells and lands
        // back, painted in the flash colour. Weak-point kills pop harder
        // and gold.
        const float Alpha = DeathPresentationElapsed / BreakerReactionDeathPopSeconds;
        const float PopScale = bDeathBeatWeakPoint ? BreakerReactionDeathPopWeakPointScale : BreakerReactionDeathPopScale;
        Owner->SetActorScale3D(DeathBaseScale * (1.0f + PopScale * FMath::Sin(Alpha * PI)));
        for (const auto& Pair : BodyMaterialBases)
        {
            if (Pair.Key.IsValid()) Pair.Key->SetVectorParameterValue(TEXT("Color"), Flash);
        }
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
        const FLinearColor Burn = FMath::Lerp(Flash, BreakerReactionDeathAshColor, Eased);
        for (const auto& Pair : BodyMaterialBases)
        {
            if (Pair.Key.IsValid()) Pair.Key->SetVectorParameterValue(TEXT("Color"), Burn);
        }
    }
}

void UBreakerHitReactionComponent::ResetDeathPresentation()
{
    // Settle any hit flash first; its own restore path handles the colours.
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitFlashTimer);
    EndHitFlash();
    bDeathPresentationRan = false;
    if (DeathPresentationElapsed < 0.0f) return;
    DeathPresentationElapsed = -1.0f;
    // Scale is only ever touched by the beat, so it is only restored when a
    // beat ran — an elite's authored scale must never be stamped with the
    // default just because this was called defensively.
    if (AActor* Owner = GetOwner()) Owner->SetActorScale3D(DeathBaseScale);
    for (const auto& Pair : BodyMaterialBases)
    {
        if (Pair.Key.IsValid()) Pair.Key->SetVectorParameterValue(TEXT("Color"), Pair.Value);
    }
}
