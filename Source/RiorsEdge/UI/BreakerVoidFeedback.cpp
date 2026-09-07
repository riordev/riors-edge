#include "UI/BreakerVoidFeedback.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "Audio/BreakerSoundDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
namespace
{
void BreakerPlayVoid(AActor* Target, AActor* Applier, bool bBurst)
{
    UWorld* World = IsValid(Target) ? Target->GetWorld() : nullptr;
    if (!World || World->GetNetMode() == NM_DedicatedServer) return;
    bool bLocalTarget = false, bLocalApplier = false;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (const auto* PC = It->Get(); PC && PC->IsLocalController())
        {
            bLocalTarget |= PC->GetPawn() == Target;
            bLocalApplier |= PC->GetPawn() == Applier;
        }
    // O2 art: four small brackets on acceptance; a closing cross at payout.
    // Incoming marks use the vitals row and sound, never geometry around the camera.
    if (!bLocalTarget && FApp::CanEverRender())
        if (auto* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            FVector Origin, Extent; Target->GetActorBounds(false, Origin, Extent);
            const float Radius = FMath::Clamp(FMath::Max(Extent.X, Extent.Y), 25.0f, 65.0f);
            BreakerFX::FEffectTiming Timing;
            Timing.DurationSeconds = bBurst ? .25f : .5f;
            Timing.FadeInSeconds = .02f; Timing.FadeOutSeconds = bBurst ? .2f : .35f;
            for (int32 I = 0; I < 4; ++I)
            {
                const FVector Radial = FVector::ForwardVector.RotateAngleAxis(I * 90.0f + 45, FVector::UpVector);
                const FVector From = Origin + Radial * Radius;
                const FVector To = bBurst ? Origin + Radial * (Radius * .2f) : From + FVector(0, 0, 25);
                Effects->AddStroke(From, To, bBurst ? 2.0f : 1.5f, BreakerUI::Violet, 1.8f, Timing);
            }
        }
    if (bLocalTarget || bLocalApplier)
    {
        ABreakerSoundDirector* Director = nullptr;
        for (TActorIterator<ABreakerSoundDirector> It(World); It; ++It) { Director = *It; break; }
        if (!Director)
        {
            FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
            Director = World->SpawnActor<ABreakerSoundDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        }
        if (Director) Director->PlayVoidCue(bBurst);
    }
}
}
void BreakerVoidFeedback::PlayActivation(AActor* Target, AActor* Applier) { BreakerPlayVoid(Target, Applier, false); }
void BreakerVoidFeedback::PlayBurst(AActor* Target, AActor* Applier) { BreakerPlayVoid(Target, Applier, true); }
