#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerEffectMath.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerPreviewHandleRuntimeTest,
    "RiorsEdge.UI.Abilities.PreviewDetectsRecycledStroke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerPreviewHandleRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Renderer = ABreakerEffectRenderer::FindOrSpawn(World);
    if (!TestNotNull(TEXT("real renderer"), Renderer)) return false;
    BreakerFX::FEffectTiming Timing;
    Timing.DurationSeconds = 1;
    const FVector A(0, 0, 0), B(100, 0, 0);
    auto Add = [&]() { return Renderer->AddStroke(A, B, 1, FLinearColor::White, 1, Timing); };
    const int32 Original = Add();
    TestTrue(TEXT("live preview can move"), Renderer->SetStrokeEndpoints(Original, B, A));
    for (int32 Index = 0; Index < Renderer->GetStrokeSlots(); ++Index) Add();
    TestFalse(TEXT("combat recycling asks the preview to reclaim its stroke"), Renderer->SetStrokeEndpoints(Original, A, B));
    const int32 Replacement = Add();
    Renderer->EndEffect(Original, 0);
    TestTrue(TEXT("stale cleanup cannot end the replacement"), Renderer->SetStrokeEndpoints(Replacement, B, A));
    TestFalse(TEXT("empty handle needs allocation"), Renderer->SetStrokeEndpoints(0, A, B));
    return true;
}
#endif
