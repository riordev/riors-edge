#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/Package.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerWaveBudget.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerCombatComponent.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerRiftRewardMath.h"
#include "Save/BreakerAccountSave.h"
#include "Save/BreakerMissionContent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftRuntimeLoopTest,
    "RiorsEdge.Zone.Rift.RuntimeBossRewardReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRuntimeLoopTest::RunTest(const FString& Parameters)
{
    UBreakerAccountSave* Account = NewObject<UBreakerAccountSave>();
    Account->bNeverPersist = true;
    UBreakerAccountSave::InjectForTesting(Account);
    ON_SCOPE_EXIT { UBreakerAccountSave::ResetCacheForTesting(); };
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    // GetMapName reads the package's short name, not the UWorld object's
    // name. Keep the production map role inside a unique transient package.
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/RiftLoop_%s/Lvl_Fernhall"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Package->SetFlags(RF_Transient);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("Lvl_Fernhall")), Package,
        true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated Fernhall world"), World)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    UBreakerGameInstance* Session = NewObject<UBreakerGameInstance>();
    World->SetGameInstance(Session);
    if (!TestTrue(TEXT("Fixture package selects the real Fernhall map branch"),
        UBreakerGameInstance::IsFernhallMap(World))) return false;
    Context.OwningGameInstance = Session;
    Session->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(FName(TEXT("substation")));
    TestEqual(TEXT("Undercroft resolves its authored boss before accepting the mission"),
        UBreakerMissionLibrary::BossForRift(Session->PendingRift), FName(TEXT("Holdfast")));
    World->GetWorldSettings()->DefaultGameMode = ABreakerGameMode::StaticClass();
    if (!TestTrue(TEXT("Real authority game mode installed"), World->SetGameMode(FURL()))) return false;
    World->InitializeActorsForPlay(FURL());
    ABreakerGameMode* Mode = World->GetAuthGameMode<ABreakerGameMode>();
    if (!TestNotNull(TEXT("Rift game mode"), Mode)) return false;
    Mode->DispatchBeginPlay();
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter = SavedFrame; };
    if (!TestTrue(TEXT("Production wave pacing actor is registered and enabled"),
        Mode->PrimaryActorTick.IsTickFunctionRegistered() && Mode->PrimaryActorTick.IsTickFunctionEnabled())) return false;
    // A plain pawn carries the real components without Character's save-slot
    // load/EndPlay persistence. No owner character or account file is touched.
    ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Player"), Pawn) || !TestNotNull(TEXT("Controller"), Controller)) return false;
    Controller->Possess(Pawn);
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Pawn);
    Pawn->AddInstanceComponent(Progression);
    Progression->RegisterComponent();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Pawn);
    Pawn->AddInstanceComponent(Equipment);
    Equipment->RegisterComponent();
    // Install the production subscription first. UE's native multicast
    // implementation broadcasts in reverse registration order, so the later
    // observer snapshots after kill loot/XP but before the completion purse.
    // The test never calls HandleRiftCompleted or grants XP.
    Pawn->DispatchBeginPlay();
    int32 Completions = 0, XpBeforePurse = 0, GlassBeforePurse = 0;
    Mode->OnRiftCompleted.AddLambda([&](const FBreakerRiftDefinition&, APawn* CompletedBy)
    {
        ++Completions;
        TestTrue(TEXT("Completion carries the actual player"), CompletedBy == Pawn);
        XpBeforePurse = Progression->GetProgressionState().TotalExperience;
        GlassBeforePurse = Equipment->GetForgeWallet().Get();
    });
    Mode->HandleStartingNewPlayer_Implementation(Controller);
    if (!TestTrue(TEXT("Actual Fernhall startup entered a rift"), Mode->IsRiftInstance())) return false;
    bool bReturnToFernhall = false, bReturnToAnchor = false;
    for (TActorIterator<ABreakerTravelPoint> It(World); It; ++It)
        for (const FBreakerTravelDestination& Destination : It->GetAvailableDestinations())
        {
            bReturnToFernhall |= Destination.Id == ABreakerTravelPoint::FernhallDestinationId;
            bReturnToAnchor |= Destination.Id == ABreakerTravelPoint::HubDestinationId;
        }
    TestTrue(TEXT("Spawned rift exit offers Fernhall"), bReturnToFernhall);
    TestTrue(TEXT("Spawned rift exit also offers Anchor"), bReturnToAnchor);
    ABreakerBossEnemy* Boss = nullptr;
    // Clear production waves through actual damage/death handling. No wave
    // counter, completion latch, boss pointer, or payout is forced by a fixture.
    for (int32 Wave = 0; Wave < 4 && !Boss; ++Wave)
    {
        TArray<ABreakerEnemy*> Enemies;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (It->IsDeadEnemy()) continue;
            if (ABreakerBossEnemy* Found = Cast<ABreakerBossEnemy>(*It)) { Boss = Found; break; }
            Enemies.Add(*It);
        }
        if (Boss) break;
        for (ABreakerEnemy* Enemy : Enemies)
        {
            if (!Enemy->HasActorBegunPlay()) Enemy->DispatchBeginPlay();
            // Structural clears happen in the spawn frame, inside the emergence
            // window, so the window is ended before the hit. This fixture owns
            // the wave cadence; RiorsEdge.Combat.Emergence.Window owns the
            // window, and ticking it out here would spend the breather clock
            // this test measures.
            Enemy->EndEmergenceWindow();
            FBreakerDamageRequest Hit;
            Hit.BaseDamage = 100000000.0f;
            Hit.bCanCritical = false;
            Hit.bBypassShield = true;
            Hit.SetInstigator(Pawn);
            Enemy->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(Hit);
        }
        // Structural lethal hits above test the clear gate, not player DPS.
        // From here the actual registered mode tick must own the breather and
        // next spawn: no manual StartNextWave, private countdown or timer calls.
        const int32 ClearedWave = Mode->GetCurrentWave();
        float Breather = 0.0f;
        if (!TestTrue(TEXT("Shipped wave cadence defines this non-boss clear"),
            UBreakerWaveBudgetLibrary::GetAutoAdvanceDelay(ClearedWave, Mode->WaveBudget, Breather))) return false;
        if (!TestTrue(TEXT("Clearing all native guards ends the current wave"), !Mode->IsWaveActive())) return false;
        constexpr float ClockStep = .05f;
        const int32 BeforeBoundarySteps = FMath::Max(0, FMath::FloorToInt(Breather / ClockStep) - 1);
        for (int32 Step = 0; Step < BeforeBoundarySteps; ++Step)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, ClockStep);
        }
        TestEqual(TEXT("Actual world clock preserves the shipped breather before its boundary"), Mode->GetCurrentWave(), ClearedWave);
        // Up to three final ticks accommodate float partitioning at the
        // authored boundary; the fixture does not alter the pacing budget.
        for (int32 Step = 0; Step < 3 && Mode->GetCurrentWave() == ClearedWave; ++Step)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, ClockStep);
        }
        if (!TestEqual(TEXT("Waiting naturally starts exactly the next production wave"), Mode->GetCurrentWave(), ClearedWave + 1)) return false;
        if (!TestTrue(TEXT("Natural advance spawns a real live encounter"), Mode->IsWaveActive())) return false;
    }
    if (!TestNotNull(TEXT("Cleared waves culminate in a real boss"), Boss)) return false;
    TestTrue(TEXT("Undercroft spawned Holdfast, not the gym Marshal"), Boss->IsA<ABreakerHoldfastEnemy>());
    const FVector BeforeReset = Boss->GetActorLocation();
    Mode->ResetBossEncounter();
    Boss = nullptr;
    for (TActorIterator<ABreakerBossEnemy> It(World); It; ++It)
        if (IsValid(*It) && !It->IsDeadEnemy()) { Boss = *It; break; }
    if (!TestNotNull(TEXT("Campaign boss reset respawns a live boss"), Boss)) return false;
    TestTrue(TEXT("Reset preserves the reachable rift arena"), FVector::DistSquared2D(BeforeReset, Boss->GetActorLocation()) < 1.0f);
    TestTrue(TEXT("Reset preserves authored boss class"), Boss->IsA<ABreakerHoldfastEnemy>());
    TestEqual(TEXT("Reset is not a completion"), Completions, 0);
    FHitResult Floor;
    FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(RiftBossFloor), false, Boss);
    TestTrue(TEXT("Boss has actual Fernhall floor below it"), World->LineTraceSingleByChannel(Floor,
        Boss->GetActorLocation(), Boss->GetActorLocation() - FVector(0, 0, 1000), ECC_WorldStatic, FloorQuery));
    if (!Boss->HasActorBegunPlay()) Boss->DispatchBeginPlay();
    FBreakerDamageRequest KillingHit;
    KillingHit.BaseDamage = 100000000.0f;
    KillingHit.bCanCritical = false;
    KillingHit.bBypassShield = true;
    KillingHit.SetInstigator(Pawn);
    Boss->FindComponentByClass<UBreakerCombatComponent>()->ReceiveDamage(KillingHit);
    TestTrue(TEXT("Actual boss damage killed the terminator"), Boss->IsDeadEnemy());
    TestEqual(TEXT("Terminator death broadcasts completion exactly once"), Completions, 1);
    const int32 Level = Session->PendingRift.EffectiveAreaLevel();
    TestEqual(TEXT("Production subscription awards the exact first-clear XP purse"),
        Progression->GetProgressionState().TotalExperience - XpBeforePurse, BreakerRiftReward::XpForCompletion(Level));
    TestEqual(TEXT("Production subscription awards the exact first-clear currency purse"),
        Equipment->GetForgeWallet().Get() - GlassBeforePurse, BreakerRiftReward::RiftglassForCompletion(Level));
    TestEqual(TEXT("First clear advances isolated account"), Account->HighestClearedAreaLevel, Level);
    const int32 PaidXp = Progression->GetProgressionState().TotalExperience;
    Mode->CompleteRiftRun(Pawn);
    Mode->StartNextWave();
    TestEqual(TEXT("Repeated completion cannot pay twice"), Completions, 1);
    TestEqual(TEXT("Repeated completion preserves paid XP"), Progression->GetProgressionState().TotalExperience, PaidXp);
    TestFalse(TEXT("Completed rift cannot restart waves through the manual wave command"), Mode->IsWaveActive());

    // THE WAY HOME. ReturnFromRift is the debrief's verb: the Fernhall travel
    // with the door's entry transform left on the session for the yard's
    // build to land the player on. The session writes are the observable
    // boundary here — the load itself is suppressed through the world-local
    // sandbox guard the retry test uses, added AFTER the build so it cannot
    // change what was built. Cross-map loading is not simulated, so the
    // yard's consumption of the transform (the teleport in the non-rift
    // Fernhall build) is not asserted in this fixture.
    //
    // The transform is seeded rather than written by the door: this rig's
    // pawn is a DefaultPawn by design (no save-slot load), and the door's
    // gate (ABreakerRiftDoor::CanEnterRift) refuses anything that is not a
    // living ABreakerCharacter, so HandleRiftEntryRequested — private to the
    // game mode and reached only through a door's delegate — cannot fire
    // here. The entry write is therefore not asserted in this file either;
    // it is one line beside PendingRift's, in the same function.
    World->URL.AddOption(TEXT("BreakerCoopCombat=1"));
    const FTransform EntryTransform(FRotator(0.0f, 135.0f, 0.0f), FVector(2600.0f, -840.0f, 112.0f));
    Session->RiftEntryTransform = EntryTransform;
    Session->bRiftEntryTransformSet = true;
    Mode->ReturnFromRift(Pawn);
    TestEqual(TEXT("Return from a rift is the Fernhall travel"),
        Session->PendingDestinationId, ABreakerTravelPoint::FernhallDestinationId);
    TestFalse(TEXT("Return from a rift clears the pending rift so the load builds the yard, not a run"),
        Session->PendingRift.IsSet());
    TestTrue(TEXT("Return from a rift leaves the entry transform for the yard to consume"),
        Session->bRiftEntryTransformSet);
    TestTrue(TEXT("Return from a rift leaves the entry transform's value untouched"),
        Session->RiftEntryTransform.Equals(EntryTransform));
    TestEqual(TEXT("Return from a rift resets the death counter with the rift it counted for"),
        Session->EndgameDeathsRemaining, UBreakerRiftLibrary::SoloEndgameDeathBudget);
    // A travel that does NOT go back to the yard drops the transform: a
    // later ordinary walk into Fernhall must land at the yard's PlayerStart,
    // not at the door of a run that ended somewhere else.
    Session->bRiftEntryTransformSet = true;
    Mode->ReturnToAnchor(Pawn);
    TestEqual(TEXT("Return to the Anchor is the hub travel"),
        Session->PendingDestinationId, ABreakerTravelPoint::HubDestinationId);
    TestFalse(TEXT("A travel anywhere but Fernhall drops the entry transform"),
        Session->bRiftEntryTransformSet);
    AddInfo(TEXT("Validated actual Fernhall build, naturally clock-advanced waves, boss damage, completion subscription, purse, exit offers and the return travel's session writes; cross-map loading is not simulated."));
    return true;
}

#endif
