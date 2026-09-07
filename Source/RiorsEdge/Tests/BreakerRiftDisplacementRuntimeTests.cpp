#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Combat/BreakerRiftDisplacement.h"
#include "Combat/BreakerEnemy.h"
#include "Characters/BreakerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRiftDisplacementRuntimeTest, "RiorsEdge.Combat.RiftDisplacementRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRiftDisplacementRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto Box = [&](FVector Location, FVector Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        if (!Actor) return static_cast<AActor*>(nullptr);
        auto* Shape = NewObject<UBoxComponent>(Actor);
        Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape);
        Shape->SetBoxExtent(Extent); Shape->SetCollisionObjectType(ECC_WorldStatic);
        Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Actor;
    };
    int32 Lane = 0;
    for (UClass* Class : {ABreakerEnemy::StaticClass(), ABreakerCharacter::StaticClass()})
        for (int32 Case = 0; Case < 3; ++Case)
        {
            const FVector Origin(0, ++Lane * 2000, 0);
            if (!Box(Origin - FVector(0, 0, 25), FVector(Case == 2 ? 100 : 500, 500, 25))) return false;
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Target = World->SpawnActor<APawn>(Class, Origin + FVector(0, 0, 200), FRotator::ZeroRotator, Spawn);
            if (!Target) return false;
            // Native actor capsule, no Character BeginPlay or owner save load.
            auto* Capsule = Cast<UCapsuleComponent>(Target->GetRootComponent());
            if (!Capsule) return false;
            const float Radius = Capsule->GetScaledCapsuleRadius();
            const FVector Start = Origin + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2);
            Target->SetActorLocation(Start);
            if (Case == 1 && !Box(Origin + FVector(120, 0, 150), FVector(10, 500, 150))) return false;
            const float Moved = BreakerRiftDisplacement::Apply(Target, Start - FVector(500, 0, 0), 200);
            TestTrue(TEXT("displacement never exceeds authored distance"), Moved <= 200.01f);
            TestEqual(TEXT("displacement stays horizontal"), Target->GetActorLocation().Z, Start.Z, .01);
            TestEqual(TEXT("source-away displacement has no lateral drift"), Target->GetActorLocation().Y, Start.Y, .01);
            if (Case == 0) TestEqual(TEXT("real capsule travels full distance on open floor"), Moved, 200.0f, .01f);
            if (Case == 1)
            {
                TestTrue(TEXT("wall permits approach without teleporting through"), Moved > 0 && Moved < 200);
                TestTrue(TEXT("whole capsule remains before real wall"), Target->GetActorLocation().X + Radius <= 110.01f);
            }
            if (Case == 2)
            {
                TestTrue(TEXT("ledge truncates actual travel"), Moved < 200);
                TestTrue(TEXT("capsule footprint stays backed by ledge floor"), Target->GetActorLocation().X + Radius <= 102.01f);
            }
            const FVector After = Target->GetActorLocation();
            TestEqual(TEXT("coincident source supplies no invented direction"), BreakerRiftDisplacement::Apply(Target, After, 200), 0.0f);
            TestEqual(TEXT("zero distance cannot move capsule"), BreakerRiftDisplacement::Apply(Target, Start, 0), 0.0f);
        }
    return true;
}
#endif
