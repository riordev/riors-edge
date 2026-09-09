#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Interaction/BreakerTravelPoint.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftRetryBudgetRuntimeTest,
    "RiorsEdge.Game.DeathBudgetDirectRetryAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRetryBudgetRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated retry world"), World)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
    World->SetGameInstance(Session);
    Context.OwningGameInstance = Session;
    // Suppress map transport through the existing world-local sandbox guard.
    // RetryRift itself still executes; its destination write is the observable
    // request boundary, with permitted calls below as positive controls.
    World->URL.AddOption(TEXT("BreakerCoopCombat=1"));
    ABreakerGameMode* Mode = World->SpawnActor<ABreakerGameMode>();
    ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>();
    if (!TestNotNull(TEXT("Actual game mode"), Mode) || !TestNotNull(TEXT("Requesting pawn"), Pawn)) return false;
    Session->PendingRift.AreaLevel = 17;
    Session->PendingRift.EncounterId = TEXT("retry.native");
    Session->PendingRift.Tier = EBreakerRiftTier::Endgame;
    const FName Untouched(TEXT("retry.unmodified"));
    for (const int32 Budget : {0, -1, 1, UBreakerRiftLibrary::SoloEndgameDeathBudget})
    {
        Session->PendingDestinationId = Untouched;
        Session->EndgameDeathsRemaining = Budget;
        Mode->RetryRift(Pawn); // Direct authority call, without the death menu.
        TestEqual(FString::Printf(TEXT("Endgame %d gates the actual retry request"), Budget),
            Session->PendingDestinationId, Budget > 0 ? ABreakerTravelPoint::RiftDestinationId : Untouched);
        TestEqual(TEXT("Retry never resets or spends the allowance"), Session->EndgameDeathsRemaining, Budget);
        TestEqual(TEXT("Retry preserves the instance identity"), Session->PendingRift.EncounterId, FName(TEXT("retry.native")));
        TestEqual(TEXT("Retry preserves its area level"), Session->PendingRift.AreaLevel, 17);
    }
    Session->PendingRift.Tier = EBreakerRiftTier::Campaign;
    for (const int32 Budget : {0, -1, UBreakerRiftLibrary::SoloEndgameDeathBudget})
    {
        Session->PendingDestinationId = Untouched;
        Session->EndgameDeathsRemaining = Budget;
        Mode->RetryRift(Pawn);
        TestEqual(TEXT("Campaign direct retry remains free at every counter"),
            Session->PendingDestinationId, ABreakerTravelPoint::RiftDestinationId);
        TestEqual(TEXT("Campaign retry leaves the counter unchanged"), Session->EndgameDeathsRemaining, Budget);
    }
    Session->PendingDestinationId = Untouched;
    Mode->RetryRift(nullptr);
    TestEqual(TEXT("Null requester cannot retry"), Session->PendingDestinationId, Untouched);
    AddInfo(TEXT("Direct retry authority and session writes tested; no cross-map loading or consumable entry is simulated."));
    return true;
}
#endif
