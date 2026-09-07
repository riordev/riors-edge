#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Weapons/BreakerRocketProjectile.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRocketRangeRuntimeTest, "RiorsEdge.Weapons.RocketRangeRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRocketRangeRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("isolated flight world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    auto Spawn = [&](float Lane, float Range, bool bInitializeAfterBeginPlay)
    {
        ABreakerRocketProjectile* Rocket = World->SpawnActor<ABreakerRocketProjectile>(FVector(0, Lane, 1000), FRotator::ZeroRotator);
        if (!Rocket) return Rocket;
        Rocket->MaximumLifetime = 1.2f;
        if (bInitializeAfterBeginPlay) Rocket->DispatchBeginPlay();
        Rocket->InitializeRocket(FBreakerDamageRequest(), 1000.0f, 100.0f, Range);
        if (!bInitializeAfterBeginPlay) Rocket->DispatchBeginPlay();
        return Rocket;
    };
    ABreakerRocketProjectile* Short = Spawn(0, 300, false);
    ABreakerRocketProjectile* Extended = Spawn(500, 700, true);
    ABreakerRocketProjectile* Default = Spawn(1000, 0, false);
    ABreakerRocketProjectile* Invalid = Spawn(1500, std::numeric_limits<float>::infinity(), true);
    if (!TestNotNull(TEXT("short flight"), Short) || !TestNotNull(TEXT("extended flight"), Extended)
        || !TestNotNull(TEXT("default flight"), Default) || !TestNotNull(TEXT("invalid-range flight"), Invalid)) return false;
    // Exercise the unchanged three-argument API, not just an explicit zero.
    Default->InitializeRocket(FBreakerDamageRequest(), 1000.0f, 100.0f);
    auto Advance = [&](int32 Steps) { for (int32 I = 0; I < Steps; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.025f); } };
    Advance(8);
    TestTrue(TEXT("short rocket really travels before expiry"), Short->GetActorLocation().X > 150.0f);
    TestTrue(TEXT("extended rocket really travels before expiry"), Extended->GetActorLocation().X > 150.0f);
    TestFalse(TEXT("short flight still alive below cap"), Short->IsActorBeingDestroyed());
    Advance(8);
    TestTrue(TEXT("short range expires without impact"), Short->IsActorBeingDestroyed());
    TestFalse(TEXT("expiry does not manufacture an explosion"), Short->HasExploded());
    TestFalse(TEXT("extended range survives original cap"), Extended->IsActorBeingDestroyed());
    TestTrue(TEXT("extended flight crosses original range"), Extended->GetActorLocation().X > 300.0f);
    Advance(16);
    TestTrue(TEXT("extended range eventually expires"), Extended->IsActorBeingDestroyed());
    TestFalse(TEXT("extended expiry remains a miss"), Extended->HasExploded());
    TestFalse(TEXT("three-argument caller retains longer default lifetime"), Default->IsActorBeingDestroyed());
    TestFalse(TEXT("nonfinite range retains default lifetime"), Invalid->IsActorBeingDestroyed());
    Advance(20);
    TestTrue(TEXT("default lifetime still expires"), Default->IsActorBeingDestroyed());
    TestTrue(TEXT("invalid range does not produce immortal rocket"), Invalid->IsActorBeingDestroyed());
    return true;
}
#endif
