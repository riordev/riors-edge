#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerModifierComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPhaseLandingRuntimeTest,
    "RiorsEdge.Combat.Modifiers.PhaseLandingFitsBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPhaseLandingRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    auto* Enemy = World->SpawnActor<ABreakerEnemy>(FVector::ZeroVector, FRotator::ZeroRotator);
    auto* Target = World->SpawnActor<AActor>(FVector(2000, 0, 0), FRotator::ZeroRotator);
    if (!Enemy || !Target) return false;
    auto* TargetRoot = NewObject<USceneComponent>(Target);
    Target->SetRootComponent(TargetRoot);
    TargetRoot->RegisterComponent();
    Target->SetActorLocation(FVector(2000, 0, 0));
    auto* Modifier = Enemy->GetModifierComponent();
    if (!TestNotNull(TEXT("shipped enemy has modifiers"), Modifier)) return false;
    if (!TestTrue(TEXT("Phasing equips"), Modifier->SetModifiers({EBreakerEnemyModifier::Phasing}))) return false;
    Modifier->SetTrackedTarget(Target);
    const FVector Destination = UBreakerEnemyModifierLibrary::GetPhaseDestination(Enemy->GetActorLocation(), Target->GetActorLocation(), Modifier->Params);
    TestTrue(TEXT("fixture asks for a real displacement"), Destination.X > 0);
    auto* Wall = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(80, 250, 250));
    Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent();
    Wall->SetActorLocation(Destination);
    auto Blink = [&]()
    {
        Modifier->AdvanceModifiers(Modifier->Params.PhaseIntervalSeconds + .01f);
        TestTrue(TEXT("blink announces itself"), Modifier->IsPhaseTelegraphing());
        Modifier->AdvanceModifiers(Modifier->Params.PhaseTelegraphSeconds + .01f);
    };
    Blink();
    TestTrue(TEXT("blocked endpoint leaves the enemy in place"), Enemy->GetActorLocation().Equals(FVector::ZeroVector, .01f));
    TestFalse(TEXT("failed blink gives no untargetability"), Modifier->IsBlinking());
    Wall->SetActorLocation(FVector(400, 0, 0));
    Blink();
    TestTrue(TEXT("clear endpoint allows crossing an intervening wall"), Enemy->GetActorLocation().Equals(Destination, .01f));
    TestTrue(TEXT("successful blink grants its authored window"), Modifier->IsBlinking());
    Modifier->AdvanceModifiers(Modifier->Params.PhaseBlinkSeconds + .01f);
    TestFalse(TEXT("blink window ends"), Modifier->IsBlinking());
    return true;
}
#endif
