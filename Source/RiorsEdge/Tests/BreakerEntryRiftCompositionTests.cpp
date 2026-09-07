#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerWaveBudget.h"
#include "Game/BreakerZoneBuilder.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Save/BreakerAccountSave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEntryRiftCompositionTest,
    "RiorsEdge.Encounters.EntryRift.AuthoredOpening",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEntryRiftCompositionTest::RunTest(const FString& Parameters)
{
    const auto Profile = UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(3);
    const auto Opening = UBreakerWaveBudgetLibrary::MakeEntryRiftOpening(Profile);
    TestEqual(TEXT("one ordinary melee pack"), Opening.Skitters, 4);
    TestEqual(TEXT("one ranged priority"), Opening.Lattices, 1);
    TestEqual(TEXT("five simultaneous bodies instead of twelve"), Opening.TotalEnemies(), 5);
    TestEqual(TEXT("existing first-wave budget retained"), Opening.Budget, 15);
    TestEqual(TEXT("authored formation pays real archetype costs"), Opening.SpentBudget, 7);
    TestEqual(TEXT("unused budget is reported, not filled"), Opening.UnspentBudget, 8);
    TestEqual(TEXT("opening carries no elite promotions"), Opening.Elites, 0);
    TestTrue(TEXT("ordinary Rift kill loot remains enabled"), Opening.bDropsLoot);
    FString Reason;
    TestTrue(TEXT("formation satisfies existing density and budget rules"),
        UBreakerWaveBudgetLibrary::IsCompositionLegal(Opening, 1, Profile, Reason));
    TestEqual(TEXT("generic solver is unchanged"), UBreakerWaveBudgetLibrary::SolveWave(1, 1, Profile).TotalEnemies(), 12);
    const auto Breach = UBreakerWaveBudgetLibrary::MakeBreachWaveBudget();
    TestEqual(TEXT("Breach still opens with its actual Warden"), UBreakerWaveBudgetLibrary::SolveWave(1, 1, Breach).Wardens, 1);

    auto* Account = NewObject<UBreakerAccountSave>(); Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* Package = CreatePackage(*FString::Printf(TEXT("/Temp/EntryFormation_%s/Lvl_Fernhall"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Package->SetFlags(RF_Transient);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated real map-role world"), World)) return false;
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Session = NewObject<UBreakerGameInstance>(); World->SetGameInstance(Session); Context.OwningGameInstance = Session;
    Session->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(NAME_None);
    World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
    if (!TestTrue(TEXT("production game mode installed"), World->SetGameMode(FURL()))) return false;
    World->InitializeActorsForPlay(FURL());
    auto* Mode = World->GetAuthGameMode<ABreakerGameMode>();
    if (!TestNotNull(TEXT("actual Rift mode"), Mode)) return false;
    Mode->DispatchBeginPlay();
    // A native pawn is sufficient for formation spawning; no character save,
    // invented equipment, paid progression, or combat-performance claim.
    auto* Pawn = World->SpawnActor<ADefaultPawn>();
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!Pawn || !Controller) return false;
    Controller->Possess(Pawn);
    Mode->HandleStartingNewPlayer_Implementation(Controller);
    TestTrue(TEXT("real startup entered the Rift"), Mode->IsRiftInstance());
    TestEqual(TEXT("actual wave tracks five live bodies"), Mode->GetWaveEnemiesAlive(), 5);
    int32 Melee = 0, Ranged = 0;
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        if (!It->IsDeadEnemy())
        {
            if (It->GetClass() == ABreakerEnemy::StaticClass()) ++Melee;
            if (It->IsA<ABreakerRangedEnemy>()) ++Ranged;
        }
    TestEqual(TEXT("real spawned ordinary melee count"), Melee, 4);
    TestEqual(TEXT("real spawned Lattice count"), Ranged, 1);
    for (int32 Wave : {2, 3})
        TestEqual(TEXT("later wave and boss composition remain original"),
            UBreakerWaveBudgetLibrary::DescribeComposition(Mode->GetWaveComposition(Wave)),
            UBreakerWaveBudgetLibrary::DescribeComposition(UBreakerWaveBudgetLibrary::SolveWave(Wave, 1, Profile)));
    Session->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(TEXT("substation"));
    TestEqual(TEXT("Substation identity keeps generic opening"),
        UBreakerWaveBudgetLibrary::DescribeComposition(Mode->GetWaveComposition(1)),
        UBreakerWaveBudgetLibrary::DescribeComposition(UBreakerWaveBudgetLibrary::SolveWave(1, 1, Profile)));
    return true;
}
#endif
