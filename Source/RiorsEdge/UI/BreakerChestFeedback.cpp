#include "UI/BreakerChestFeedback.h"
#include "Audio/BreakerSoundDirector.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
namespace
{
void BreakerPlayChestOpen(AActor* Chest, AActor* Opener)
{
    // The world comes from the chest, or the opener if the chest is already
    // gone — a chest that despawns on open still owes its opener the latch.
    const AActor* Source = IsValid(Chest) ? Chest : Opener;
    UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
    if (!World || World->GetNetMode() == NM_DedicatedServer) return;
    bool bLocalOpener = false;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        if (const auto* PC = It->Get(); PC && PC->IsLocalController())
            bLocalOpener |= IsValid(Opener) && PC->GetPawn() == Opener;
    // Sound only, and only for the one who opened it: another player's chest
    // is silent here, exactly as another player's rift activation is.
    if (!bLocalOpener) return;
    ABreakerSoundDirector* Director = nullptr;
    for (TActorIterator<ABreakerSoundDirector> It(World); It; ++It) { Director = *It; break; }
    if (!Director)
    {
        FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
        Director = World->SpawnActor<ABreakerSoundDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    }
    if (Director) Director->PlayChestOpen();
}
}
void BreakerChestFeedback::PlayOpen(AActor* Chest, AActor* Opener) { BreakerPlayChestOpen(Chest, Opener); }
