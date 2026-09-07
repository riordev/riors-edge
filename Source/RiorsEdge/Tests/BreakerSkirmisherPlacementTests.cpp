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
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Game/BreakerCoverRegistry.h"
#include "Save/BreakerAccountSave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSkirmisherPlacementTest,
    "RiorsEdge.Encounters.SkirmisherSafePlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSkirmisherPlacementTest::RunTest(const FString& Parameters)
{
    for (const FName Yard : {FName(NAME_None), FName(TEXT("substation")), FName(TEXT("breach"))})
    {
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
    Session->PendingRift = UBreakerZoneBuilder::FernhallRiftFor(Yard);
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
    const FVector Start = Pawn->GetActorLocation();
    const FVector AxisF = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector AxisR = Pawn->GetActorRightVector().GetSafeNormal2D();
    const auto Field = UBreakerZoneBuilder::FernhallFieldParams();
    auto CheckSkirmishers = [&]()
    {
        int32 Checked = 0;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            if (It->IsDeadEnemy() || !It->IsA<ABreakerSkirmisherEnemy>()) continue;
            const FVector Offset = It->GetActorLocation() - Start;
            const float F = FVector::DotProduct(Offset, AxisF);
            const float R = FVector::DotProduct(Offset, AxisR);
            const float Radius = It->GetBodyCapsuleRadius();
            TestTrue(TEXT("actual body clears near floor edge"), F - Radius >= Field.BandNearCm - 1);
            TestTrue(TEXT("actual body clears far floor edge"), F + Radius <= Field.BandFarCm + 1);
            TestTrue(TEXT("actual body clears both side edges"), FMath::Abs(R) + Radius <= Field.BandHalfWidthCm + 1);
            const auto* Body = It->FindComponentByClass<UCapsuleComponent>();
            if (!TestNotNull(TEXT("real Skirmisher body"), Body)) continue;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SkirmisherPlacementTest), false, *It);
            TestFalse(TEXT("spawned capsule is clear of blocking geometry"),
                World->OverlapBlockingTestByChannel(It->GetActorLocation(), FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(Radius, Body->GetScaledCapsuleHalfHeight()), Query));
            FHitResult Floor;
            const FVector Feet = It->GetActorLocation() - FVector(0, 0, Body->GetScaledCapsuleHalfHeight());
            TestTrue(TEXT("actual capsule is seated on walkable floor"),
                World->LineTraceSingleByObjectType(Floor, Feet + FVector(0, 0, 5), Feet - FVector(0, 0, 5),
                    FCollisionObjectQueryParams(ECC_WorldStatic), Query) && Floor.ImpactNormal.Z >= .7f);
            ++Checked;
        }
        TestEqual(TEXT("all authored Skirmishers actually spawned safely"), Checked, Mode->GetWaveComposition(2).Skirmishers);
    };

    // Actual second-wave spawning from the side edge, facing outward. Destroy
    // completed-wave actors only: placement fixture, not earned combat claim.
    TArray<ABreakerEnemy*> Previous;
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It) Previous.Add(*It);
    for (ABreakerEnemy* Enemy : Previous) Enemy->Destroy();
    Pawn->SetActorLocation(Start + AxisF * (Field.BandFarCm - 100) + AxisR * (Field.BandHalfWidthCm - 100));
    Pawn->SetActorRotation(AxisR.Rotation());
    Mode->StartNextWave();
    TestEqual(TEXT("actual later profile advances"), Mode->GetCurrentWave(), 2);
    TestTrue(TEXT("shipped second wave requires a Skirmisher"), Mode->GetWaveComposition(2).Skirmishers > 0);
    CheckSkirmishers();
    }
    return true;
}
#endif
