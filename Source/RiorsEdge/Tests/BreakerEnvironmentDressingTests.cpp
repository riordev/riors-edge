#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerEnvironmentDressingTest,
    "RiorsEdge.World.EnvironmentDressingBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerEnvironmentDressingTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("dressing world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    for (const TCHAR* Name : { TEXT("CommonTree_1"), TEXT("DeadTree_1"), TEXT("Bush_Common"),
        TEXT("Fern_1"), TEXT("Column_Pipes"), TEXT("Column_MetalSupport"), TEXT("Door_Metal") })
    {
        for (float Yaw : {0.0f, 90.0f})
        {
            const FVector Ground(1000, 2000, 100);
            AStaticMeshActor* Actor = BreakerPlaceEnvironmentDressing(World, Name, Ground, 400, Yaw);
            if (!TestNotNull(FString::Printf(TEXT("%s actual imported placement"), Name), Actor)) return false;
            UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
            const FBox Box = Component->CalcBounds(Component->GetComponentTransform()).GetBox();
            TestEqual(FString::Printf(TEXT("%s actual height"), Name), Box.GetSize().Z, 400.0, .1);
            TestEqual(FString::Printf(TEXT("%s bottom alignment"), Name), Box.Min.Z, Ground.Z, .1);
            TestTrue(FString::Printf(TEXT("%s cannot form an oversized plane"), Name), Box.GetSize().GetMax() <= 1600);
            TestFalse(TEXT("decorative actor cannot block routes"), Actor->GetActorEnableCollision());
            TestEqual(TEXT("decorative component has no query collision"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            TestFalse(TEXT("decorative component cannot alter navigation"), Component->CanEverAffectNavigation());
            UStaticMesh* Mesh = Component->GetStaticMesh();
            for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
                TestTrue(FString::Printf(TEXT("%s retains imported material slot %d"), Name, Slot),
                    Component->GetMaterial(Slot) == Mesh->GetStaticMaterials()[Slot].MaterialInterface);
            Actor->Destroy();
        }
    }
    return true;
}
#endif
