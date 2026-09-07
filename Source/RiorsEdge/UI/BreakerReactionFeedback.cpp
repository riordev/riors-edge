#include "UI/BreakerReactionFeedback.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "UI/BreakerRiftFeedback.h"
#include "Audio/BreakerSoundDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
void BreakerReactionFeedback::Play(AActor* Target, AActor* Applier, FGameplayTag ReactionTag)
{
    const bool bCollapse = ReactionTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"));
    const bool bWither = ReactionTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"));
    const bool bTear = ReactionTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear"));
    if (!bCollapse && !bWither && !bTear) return;
    UWorld* World = IsValid(Target) ? Target->GetWorld() : nullptr;
    if (!World || World->GetNetMode() == NM_DedicatedServer) return;
    bool bLocalTarget = false, bLocalApplier = false;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (const auto* PC = It->Get(); PC && PC->IsLocalController())
        {
            bLocalTarget |= PC->GetPawn() == Target;
            bLocalApplier |= PC->GetPawn() == Applier;
        }
    // O2 small, finite cues: collapse inward, wither downward, tear apart.
    // A local victim hears the cue without strokes enclosing the camera.
    if (!bLocalTarget && FApp::CanEverRender())
        if (auto* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            FVector Origin, Extent; Target->GetActorBounds(false, Origin, Extent);
            const float Radius = FMath::Clamp(FMath::Max(Extent.X, Extent.Y), 25.0f, 65.0f);
            const FLinearColor Color = bWither ? BreakerUI::Violet : BreakerRiftFeedback::Color;
            BreakerFX::FEffectTiming Timing;
            Timing.DurationSeconds = .35f; Timing.FadeInSeconds = .02f; Timing.FadeOutSeconds = .3f;
            for (int32 I = 0; I < 4; ++I)
            {
                const FVector Radial = FVector::ForwardVector.RotateAngleAxis(I * 90.0f + 45, FVector::UpVector);
                const FVector From = Origin + Radial * (bTear ? Radius * .25f : Radius);
                const FVector To = bCollapse ? Origin + Radial * (Radius * .2f)
                    : bWither ? From - FVector(0, 0, 45) : Origin + Radial * (Radius + 40);
                Effects->AddStroke(From, To, 2.0f, Color, 1.8f, Timing);
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
        if (Director) Director->PlayReaction(ReactionTag);
    }
}
