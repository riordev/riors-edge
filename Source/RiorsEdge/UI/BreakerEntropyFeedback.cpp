#include "UI/BreakerEntropyFeedback.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "Audio/BreakerSoundDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void BreakerEntropyFeedback::PlayActivation(AActor* Target, AActor* Applier)
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
    // No screen-sized burst around the first-person camera. Incoming Rot is
    // announced by the vitals readout and this same distinct audio cue.
    if (!bLocalTarget && FApp::CanEverRender())
        if (auto* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            FVector Origin, Extent; Target->GetActorBounds(false, Origin, Extent);
            const float Radius = FMath::Clamp(FMath::Max(Extent.X, Extent.Y), 25.0f, 65.0f); // O2 PLACEHOLDER
            const float Height = FMath::Clamp(Extent.Z, 35.0f, 100.0f); // O2 PLACEHOLDER
            BreakerFX::FEffectTiming Timing;
            Timing.DurationSeconds = .65f; Timing.FadeInSeconds = .04f; Timing.FadeOutSeconds = .4f; // O2 PLACEHOLDER
            for (int32 I = 0; I < 6; ++I)
            {
                const float Angle = 2 * PI * I / 6;
                const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0);
                Effects->AddStroke(Origin + Radial * Radius + FVector(0, 0, Height * .35f),
                    Origin + Radial * (Radius * .75f) - FVector(0, 0, Height * .55f),
                    1.5f, BreakerUI::Orange, 1.8f, Timing, I * .025f); // O2 PLACEHOLDER: small staggered decay scratches.
            }
#if !UE_BUILD_SHIPPING
            if (bLocalApplier && FParse::Param(FCommandLine::Get(), TEXT("BreakerCaptureEntropyFeedback")))
            {
                FTimerHandle Capture;
                World->GetTimerManager().SetTimer(Capture, []
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/rot_activation.png"), false, false);
                }, .12f, false);
            }
#endif
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
        if (Director && Director->PlayEntropyActivation())
            UE_LOG(LogTemp, Verbose, TEXT("[BreakerEntropyFX] Rot activation cue: %s"), *GetNameSafe(Target));
    }
}
