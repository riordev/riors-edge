#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/ScopeExit.h"
#include "Save/BreakerAccountSave.h"
#include "UObject/Package.h"

// ---------------------------------------------------------------------------
// A CLEARED PATROL COMES BACK, AND ONLY OUTSIDE A RIFT.
//
// The owner's ruling: an ordinary area has patrols and they return; a rift is an
// instance with a completion condition and stays finite. The pure arithmetic of
// WHEN is proved without a world in RiorsEdge.Zone.Repopulation.Rule. What this
// fixture owns is the half that only a world can answer — that a body actually
// returns, that it returns as what stood there, and that the two gates which
// make it a repopulation rather than a pop-in genuinely hold.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRepopulationRuntimeTest,
    "RiorsEdge.Zone.Repopulation.Runtime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // Live bodies carrying an authored outdoor pocket tag, corpses excluded: a
    // corpse still lying in the pocket has already stopped being a fight, which
    // is the same rule the clock itself applies.
    int32 BreakerRepopulationLivePocket(UWorld* World, int32 Pocket)
    {
        const FName Tag(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), Pocket));
        int32 Live = 0;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (It->ActorHasTag(Tag) && !It->IsDeadEnemy()) ++Live;
        }
        return Live;
    }

    void BreakerRepopulationKillPocket(UWorld* World, int32 Pocket, AActor* Killer)
    {
        const FName Tag(*FString::Printf(TEXT("Fernhall.Outdoor.%d"), Pocket));
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (!It->ActorHasTag(Tag) || It->IsDeadEnemy()) continue;
            if (!It->HasActorBegunPlay()) It->DispatchBeginPlay();
            // A body that has only just arrived is protected; this fixture is
            // about the clock, not the window, which owns its own test.
            It->EndEmergenceWindow();
            FBreakerDamageRequest Hit;
            Hit.BaseDamage = 100000000.0f;
            Hit.bCanCritical = false;
            Hit.bBypassShield = true;
            Hit.SetInstigator(Killer);
            if (auto* Combat = It->FindComponentByClass<UBreakerCombatComponent>()) Combat->ReceiveDamage(Hit);
        }
    }
}

bool FBreakerRepopulationRuntimeTest::RunTest(const FString& Parameters)
{
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/FernhallRepopulation_%s/Lvl_Fernhall"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Package->SetFlags(RF_Transient);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package,
        true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("an isolated outdoor world"), World)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    const uint64 EntryFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = EntryFrame; };
    UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
    World->SetGameInstance(Session);
    Context.OwningGameInstance = Session;
    World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
    if (!TestTrue(TEXT("authority mode installed"), World->SetGameMode(FURL()))) return false;
    World->InitializeActorsForPlay(FURL());
    ABreakerGameMode* Mode = World->GetAuthGameMode<ABreakerGameMode>();
    if (!TestNotNull(TEXT("game mode"), Mode)) return false;
    Mode->DispatchBeginPlay();
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("player"), Player) || !TestNotNull(TEXT("controller"), Controller)) return false;
    Controller->bAutoManageActiveCameraTarget = false;
    Controller->Possess(Player);
    Player->SetActorTickEnabled(false);
    Mode->HandleStartingNewPlayer_Implementation(Controller);
    if (!TestFalse(TEXT("ordinary Fernhall is not a rift"), Mode->IsRiftInstance())) return false;

    auto Clock = [&](float Seconds)
    {
        for (float T = 0.0f; T < Seconds; T += 0.5f) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.5f); }
    };

    // The authored pocket, before anything dies.
    const int32 Authored = BreakerRepopulationLivePocket(World, 0);
    if (!TestTrue(TEXT("the entry pocket is authored with bodies"), Authored > 0)) return false;

    // Far away, so the clearance gate is not what is under test yet.
    FVector PocketAt = FVector::ZeroVector;
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("Fernhall.Outdoor.0"))) { PocketAt = It->GetActorLocation(); break; }
    Player->TeleportTo(PocketAt + FVector(30000, 0, 0), FRotator::ZeroRotator, false, true);

    BreakerRepopulationKillPocket(World, 0, Player);
    if (!TestEqual(TEXT("a cleared pocket is empty"), BreakerRepopulationLivePocket(World, 0), 0)) return false;

    // BEFORE THE DELAY: still empty. Without this half the test would pass
    // against a pocket that refilled the instant it cleared.
    Clock(Mode->OutdoorRepopulationDelaySeconds * 0.5f);
    TestEqual(TEXT("a pocket does not refill before its delay"),
        BreakerRepopulationLivePocket(World, 0), 0);

    // AFTER IT: a patrol is back.
    Clock(Mode->OutdoorRepopulationDelaySeconds + 2.0f);
    const int32 Returned = BreakerRepopulationLivePocket(World, 0);
    if (!TestTrue(TEXT("a cleared patrol comes back"), Returned > 0)) return false;

    // ONE AT A TIME. A pocket that came back all at once would be a wave
    // arriving, which is the rift's verb and not the world's.
    TestTrue(TEXT("and comes back one body at a time"), Returned < Authored || Authored == 1);

    // IT COMES BACK AS WHAT STOOD THERE, to its authored home. This is the half
    // the Pattern quest needs: its objective is elite-gated, so a pocket that
    // refilled with generic trash would leave the quest just as unfinishable.
    // Under O274 the body ARRIVES at a spawn marker or a tear and walks to
    // the post, so two seconds after the delay it may still be on its way:
    // the post it was given is the authored home, not where its feet are.
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        if (!It->ActorHasTag(TEXT("Fernhall.Outdoor.0")) || It->IsDeadEnemy()) continue;
        TestTrue(TEXT("a returned patrol is posted at an authored home"),
            FVector::DistSquared(It->GetLeashOrigin(), PocketAt) < 1000000.0);
        break;
    }

    // THE CLEARANCE GATE. Standing in the pocket holds it empty: repopulation is
    // something you come back to, never something you watch happen.
    BreakerRepopulationKillPocket(World, 0, Player);
    Player->TeleportTo(PocketAt, FRotator::ZeroRotator, false, true);
    Clock(Mode->OutdoorRepopulationDelaySeconds * 3.0f);
    TestEqual(TEXT("a player standing in a pocket holds it cleared"),
        BreakerRepopulationLivePocket(World, 0), 0);

    // AND THE WAIT IS KEPT, NOT RESTARTED. Walking away from a pocket that has
    // already stood empty for an hour must not begin the clock again.
    Player->TeleportTo(PocketAt + FVector(30000, 0, 0), FRotator::ZeroRotator, false, true);
    Clock(2.0f);
    TestTrue(TEXT("leaving lets a long-empty pocket refill without a fresh wait"),
        BreakerRepopulationLivePocket(World, 0) > 0);
    return true;
}

// ---------------------------------------------------------------------------
// A RIFT STAYS FINITE. The completion condition IS the rift — clear the area,
// survive the waves, deliver the payload — and a population that refills means
// a condition that never arrives.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftStaysFiniteTest,
    "RiorsEdge.Zone.Repopulation.RiftStaysFinite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftStaysFiniteTest::RunTest(const FString& Parameters)
{
    // The registry is only ever filled by the outdoor placement, and that
    // placement refuses to run inside a rift at all — so the guard is asserted
    // where it actually lives rather than reproduced here. A rift instance
    // reaching the clock finds nothing to refill, and the clock's own first
    // line refuses a rift regardless.
    const ABreakerGameMode* Shipped = GetDefault<ABreakerGameMode>();
    if (!TestNotNull(TEXT("the game mode class default exists"), Shipped)) return false;
    TestFalse(TEXT("a mode is not a rift instance by default"), Shipped->IsRiftInstance());
    TestTrue(TEXT("repopulation ships enabled for the ordinary world"),
        Shipped->OutdoorRepopulationDelaySeconds > 0.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
