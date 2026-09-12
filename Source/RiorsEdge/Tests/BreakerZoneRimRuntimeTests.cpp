#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Combat/BreakerZoneActor.h"
#include "UI/BreakerEffectRenderer.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerZoneRimRuntimeTest, "RiorsEdge.World.ZoneRimLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerZoneRimRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!TestNotNull(TEXT("isolated effect world"),World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 InitialFrame = GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = InitialFrame; };
    World->InitializeActorsForPlay(FURL());
    World->SetBegunPlay(true);
    auto* Renderer = ABreakerEffectRenderer::FindOrSpawn(World);
    if (!TestNotNull(TEXT("actual renderer"),Renderer)) return false;
    auto Advance = [&](float Seconds)
    {
        for (float Left=Seconds; Left>UE_SMALL_NUMBER;)
        {
            const float Step=FMath::Min(.05f,Left);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All,Step);
            Left-=Step;
        }
    };
    auto Visible = [&]()
    {
        int32 Count=0;
        TInlineComponentArray<UStaticMeshComponent*> Meshes(Renderer);
        for (auto* Mesh : Meshes)
            if (Mesh->GetName().StartsWith(TEXT("EffectStroke")) && Mesh->IsVisible() && !Mesh->bHiddenInGame) ++Count;
        return Count;
    };
    auto Spawn = [&]()
    {
        auto* Zone=World->SpawnActor<ABreakerZoneActor>();
        if (!Zone) return Zone;
        FBreakerZoneSpec Spec;
        Spec.Duration=2; Spec.RadiusCm=300; Spec.bShowFilledFootprint=false;
        Zone->ConfigureZone(Spec,nullptr);
        return Zone;
    };
    auto Centres = [&]()
    {
        TArray<FVector> Out;
        TInlineComponentArray<UStaticMeshComponent*> Meshes(Renderer);
        for (auto* Mesh : Meshes)
            if (Mesh->GetName().StartsWith(TEXT("EffectStroke")) && Mesh->IsVisible() && !Mesh->bHiddenInGame) Out.Add(Mesh->GetComponentLocation());
        return Out;
    };
    auto* Zone=Spawn();
    if (!TestNotNull(TEXT("static configured zone"),Zone)) return false;
    Advance(.4f);
    TestEqual(TEXT("actual pooled ring meshes visible"),Visible(),BreakerFX::GroundRingStrokes);
    // O282: the rim turns each tick. Same sixteen strokes, and at least one of
    // them is somewhere it was not 0.3 s ago. Compared as a set, not by slot,
    // so a renderer that re-submits into fresh slots still counts as moving.
    const TArray<FVector> Before=Centres();
    Advance(.3f);
    TestEqual(TEXT("the turning rim is still one ring"),Visible(),BreakerFX::GroundRingStrokes);
    {
        bool bMoved=false;
        for (const FVector& After : Centres())
        {
            bool bNear=false;
            for (const FVector& Was : Before)
                if (FVector::Dist(After,Was)<=1.0) { bNear=true; break; }
            if (!bNear) { bMoved=true; break; }
        }
        TestTrue(TEXT("a rim stroke moved more than a centimetre in 0.3 s"),bMoved);
    }
    Zone->RefreshDuration(3);
    Advance(2);
    TestTrue(TEXT("refreshed zone survives original deadline"),IsValid(Zone) && !Zone->IsActorBeingDestroyed());
    TestEqual(TEXT("refreshed strokes survive original deadline"),Visible(),BreakerFX::GroundRingStrokes);
    Zone->SetExpiryPaused(true);
    const float Paused=Zone->GetRemainingDuration();
    ++GFrameCounter;
    World->Tick(LEVELTICK_All,2.0f);
    TestEqual(TEXT("single hitch preserves paused zone rim before renderer expiry"),Visible(),BreakerFX::GroundRingStrokes);
    Advance(2);
    TestEqual(TEXT("pause holds actual zone clock"),Zone->GetRemainingDuration(),Paused,.01f);
    TestEqual(TEXT("pause holds actual visible strokes"),Visible(),BreakerFX::GroundRingStrokes);
    TestTrue(TEXT("actual radius growth succeeds"),Zone->GrowRadiusOnce(100));
    Advance(.3f);
    TestEqual(TEXT("growth replaces ring without duplicate strokes"),Visible(),BreakerFX::GroundRingStrokes);
    TInlineComponentArray<UStaticMeshComponent*> Meshes(Renderer);
    for (auto* Mesh : Meshes)
        if (Mesh->GetName().StartsWith(TEXT("EffectStroke")) && !Mesh->bHiddenInGame)
            TestTrue(TEXT("grown stroke centers track new circumference"),FVector::Dist2D(Mesh->GetComponentLocation(),Zone->GetActorLocation())>380);
    Zone->SetExpiryPaused(false);
    Advance(Paused+.3f);
    TestEqual(TEXT("natural expiry removes all strokes"),Visible(),0);
    Zone=Spawn();
    if (!Zone) return false;
    Advance(.3f);
    TestEqual(TEXT("second real zone renders"),Visible(),BreakerFX::GroundRingStrokes);
    Zone->Destroy();
    Advance(.1f);
    TestEqual(TEXT("early destruction removes rim immediately"),Visible(),0);
    BreakerFX::FEffectTiming Short;
    Short.DurationSeconds=.3f;
    Short.FadeInSeconds=0;
    Short.FadeOutSeconds=0;
    const int32 Stale = Renderer->AddStroke(FVector::ZeroVector,FVector(100,0,0),5,FLinearColor::White,1,Short);
    // More submissions than the fixed pool force the old slot to be recycled.
    for (int32 Index=0; Index<ABreakerEffectRenderer::GetStrokeSlots()+1; ++Index)
        Renderer->AddStroke(FVector(0,Index,0),FVector(100,Index,0),5,FLinearColor::White,1,Short);
    Renderer->SetEffectRemaining(Stale,20);
    Advance(.5f);
    TestEqual(TEXT("stale serial cannot extend recycled replacement"),Visible(),0);
    return true;
}
#endif
