#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/BreakerEnemyController.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerLocomotionClearanceRuntimeTest,
    "RiorsEdge.AI.Locomotion.BodyClearanceAndBlockedHoldRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLocomotionClearanceRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("collision world with deliberately unavailable navigation"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto Box = [&](FVector At, FVector Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        auto* Shape = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape);
        Shape->SetBoxExtent(Extent);
        Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Shape->SetCollisionObjectType(ECC_WorldStatic);
        Shape->SetCollisionResponseToAllChannels(ECR_Block);
        Shape->RegisterComponent(); Actor->SetActorLocation(At);
        return Actor;
    };
    Box(FVector(0, 0, -20), FVector(2000, 2000, 20));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), FVector(0, 0, 90), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("shipped enemy"), Enemy)) return false;
    auto* Controller = World->SpawnActor<ABreakerEnemyController>();
    Controller->Possess(Enemy);
    auto* Mover = Enemy->GetEnemyMovement();
    const auto* Capsule = Cast<UCapsuleComponent>(Enemy->GetRootComponent());
    if (!TestNotNull(TEXT("shipped capsule"), Capsule)) return false;
    const float Height = Capsule->GetScaledCapsuleHalfHeight();
    Enemy->SetActorLocation(FVector(0, 0, Height));
    const FVector Goal(700, 0, Height);
    auto Drive = [&]() { return Mover->Drive(FVector::ForwardVector, 1, nullptr, 700, 100, 330, true, Goal); };
    TestTrue(TEXT("floor contact allows ordinary steering"), Drive() == EBreakerLocomotionMode::Steer);
    AActor* Ledge = Box(FVector(300, 0, 20), FVector(20, 200, 20));
    FHitResult Ray;
    TestFalse(TEXT("old centre ray misses the 40 cm ledge"), World->LineTraceSingleByObjectType(Ray,
        Enemy->GetActorLocation(), Goal, FCollisionObjectQueryParams(ECC_WorldStatic)));
    Mover->Velocity = FVector(330, 0, 0);
    TestTrue(TEXT("capsule sees ledge and unavailable route holds"), Drive() == EBreakerLocomotionMode::Idle);
    TestTrue(TEXT("failed route removes residual velocity"), Mover->Velocity.IsNearlyZero());
    TestTrue(TEXT("failed route removes queued steering"), Mover->GetPendingInputVector().IsNearlyZero());
    TestTrue(TEXT("held state available to shipped facing caller"), Mover->IsBlockedHold());
    for (int32 I = 0; I < 120; ++I)
        TestTrue(TEXT("repeated blocked frames never fall back to walking"), Drive() == EBreakerLocomotionMode::Idle);
    Ledge->Destroy();
    TestTrue(TEXT("removed obstacle resumes immediately even during retry delay"), Drive() == EBreakerLocomotionMode::Steer);
    TestFalse(TEXT("clear route releases held facing"), Mover->IsBlockedHold());
    AActor* Edge = Box(FVector(300, Capsule->GetScaledCapsuleRadius(), Height), FVector(20, 10, Height));
    TestFalse(TEXT("centre ray also misses a shoulder obstruction"), World->LineTraceSingleByObjectType(Ray,
        Enemy->GetActorLocation(), Goal, FCollisionObjectQueryParams(ECC_WorldStatic)));
    TestTrue(TEXT("shoulder obstruction holds instead of clipping"), Drive() == EBreakerLocomotionMode::Idle);
    Edge->Destroy();

    // A neighbouring capsule blocks the trace channel but is not a floor.
    auto* Neighbour = World->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(),
        FVector(0, 0, Height), FRotator::ZeroRotator, Spawn);
    Enemy->SetActorLocation(FVector(0, 0, Height + 10));
    Mover->StopMovementImmediately(); Mover->ConsumeInputVector();
    Mover->TickComponent(.05f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("crowded ground snap plants on static floor, never a pawn crown"),
        FMath::IsNearlyEqual(Enemy->GetActorLocation().Z, static_cast<double>(Height), .01));
    TestNotNull(TEXT("crowd body exists"), Neighbour);
    Mover->ResetForRevive();
    TestFalse(TEXT("revive clears blocked facing"), Mover->IsBlockedHold());
    return true;
}
#endif
