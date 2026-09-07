#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/BreakerErasedEarthBuilder.h"
#include "Interaction/BreakerSurvivor.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSurvivorRuntimeTest, "RiorsEdge.Campaign.SurvivorPhysicalEscort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSurvivorRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/Survivor_%s/Lvl_ErasedEarth"), *FGuid::NewGuid().ToString()));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Lvl_ErasedEarth"), Package, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated actual destination world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    const auto Layout = UBreakerErasedEarthBuilder::Build(World);
    TestEqual(TEXT("Three finite encounter pockets"), Layout.PocketCount, 3);
    TestEqual(TEXT("Fifteen ordinary Vestige spawn specifications"), Layout.Enemies.Num(), 15);
    FVector Previous = Layout.SurvivorShelter;
    int32 Gates = 0;
    for (const auto& Point : Layout.Route)
    {
        FHitResult Hit;
        TestFalse(TEXT("Full Survivor capsule can sweep each authored route leg"), World->SweepSingleByChannel(
            Hit, Previous, Point.Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34, 88)));
        TestTrue(TEXT("Every checkpoint has a real floor"), World->LineTraceSingleByChannel(
            Hit, Point.Location, Point.Location - FVector(0, 0, 150), ECC_Visibility));
        if (Point.RequiredPocket != INDEX_NONE) ++Gates;
        Previous = Point.Location;
    }
    TestEqual(TEXT("Each pocket has a physical waiting checkpoint"), Gates, 3);
    for (const auto& Spawn : Layout.Enemies)
    {
        TestTrue(TEXT("Only ordinary melee or Lattice enemies"), Spawn.EnemyClass == ABreakerEnemy::StaticClass()
            || Spawn.EnemyClass == ABreakerRangedEnemy::StaticClass());
        TestFalse(TEXT("Enemy capsule clears authored settlement geometry"), World->OverlapBlockingTestByChannel(
            Spawn.Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(45, 90)));
        FHitResult Ground;
        TestTrue(TEXT("Each enemy stands over actual floor"), World->LineTraceSingleByChannel(
            Ground, Spawn.Location, Spawn.Location - FVector(0, 0, 180), ECC_Visibility));
    }
    ABreakerCharacter* Player = World->SpawnActor<ABreakerCharacter>(Layout.SurvivorShelter + FVector(0, 120, 0), FRotator::ZeroRotator);
    ABreakerSurvivor* Survivor = World->SpawnActor<ABreakerSurvivor>(Layout.SurvivorShelter, FRotator::ZeroRotator);
    if (!Player || !Survivor) return false;
    Player->GetAbilitySystemComponent()->InitAbilityActorInfo(Player, Player);
    Player->GetAbilitySystemComponent()->AddAttributeSetSubobject(Player->GetAttributes());
    Player->GetCombat()->BindAttributes(Player->GetAttributes());
    // No Character BeginPlay: this component fixture never reads or writes owner saves.
    Survivor->DispatchBeginPlay(); Survivor->SetActorTickEnabled(false);
    Survivor->ConfigureEscort(Layout);
    TestFalse(TEXT("Unaccepted escort cannot start"), Survivor->TryBeginEscort(Player));
    Survivor->Tick(1);
    TestEqual(TEXT("Lucidity does not run before acceptance"), Survivor->GetLucidityRemaining(), 0.0f);
    // Explicit prerequisite fixture; real earned dialogue/deaths are covered by the travel probe.
    Player->GetQuestJournal()->SetFlag(TEXT("Quest.Breach.TurnedIn"));
    Player->GetQuestJournal()->SetFlag(TEXT("Quest.Survivor.Accepted"));
    TestTrue(TEXT("First meet starts before UI writes Met"), Survivor->TryBeginEscort(Player));
    Player->GetQuestJournal()->SetFlag(TEXT("Quest.Survivor.Met"));
    int32 Failed = 0, Ready = 0;
    Survivor->OnEscortFailed.AddLambda([&](ABreakerSurvivor*) { ++Failed; });
    Survivor->OnEscortReadyForExtraction.AddLambda([&](ABreakerSurvivor* Actor, ABreakerCharacter* Escort) {
        TestTrue(TEXT("Ready reports the physical actor and living escort"), Actor == Survivor && Escort == Player); ++Ready;
    });
    const float PausedBudget = Survivor->GetLucidityRemaining();
    Survivor->SetDialoguePaused(true); Survivor->Tick(5);
    TestEqual(TEXT("Dialogue pauses lucidity"), Survivor->GetLucidityRemaining(), PausedBudget);
    Survivor->SetDialoguePaused(false);
    auto FollowTick = [&]() {
        // Move only the test player alongside. Survivor always uses its actual swept movement.
        Player->SetActorLocation(Survivor->GetActorLocation() + FVector(0, 150, 0));
        Survivor->Tick(0.05f);
    };
    for (int32 Step = 0; Step < 250; ++Step) FollowTick();
    TestEqual(TEXT("Uncleared first pocket stops route progress"), Survivor->GetRouteIndex(), 2);
    const FVector Waiting = Survivor->GetActorLocation();
    for (int32 Step = 0; Step < 10; ++Step) FollowTick();
    TestTrue(TEXT("Waiting never teleports past enemies"), Survivor->GetActorLocation().Equals(Waiting));
    Survivor->Tick(Survivor->GetLucidityRemaining() + 1);
    TestEqual(TEXT("Timeout broadcasts one failure"), Failed, 1);
    TestTrue(TEXT("Failure resets the transient attempt to shelter"), Survivor->GetActorLocation().Equals(Layout.SurvivorShelter));
    TestFalse(TEXT("Failure never writes extraction"), Player->GetQuestJournal()->HasFlag(TEXT("Quest.Survivor.Extracted")));
    Player->SetActorLocation(Layout.SurvivorShelter + FVector(0, 120, 0));
    TestTrue(TEXT("Persistent Met still allows retry"), Survivor->TryBeginEscort(Player));
    for (int32 Pocket = 0; Pocket < 3; ++Pocket) Survivor->SetPocketCleared(Pocket);
    for (int32 Step = 0; Step < 1600 && Survivor->IsEscortActive(); ++Step) FollowTick();
    TestEqual(TEXT("Complete physical route broadcasts exactly once"), Ready, 1);
    TestTrue(TEXT("Survivor actually stands at extraction"), Survivor->IsAtExtraction());
    TestTrue(TEXT("Player also physically reaches extraction"), FVector::Dist(Player->GetActorLocation(), Layout.Extraction) <= Survivor->ExtractionRadius);
    Survivor->Tick(1);
    TestEqual(TEXT("Extraction callback is not repeated"), Ready, 1);
    TestFalse(TEXT("Actor does not award quest credit itself"), Player->GetQuestJournal()->HasFlag(TEXT("Quest.Survivor.Extracted")));
    return true;
}
#endif
