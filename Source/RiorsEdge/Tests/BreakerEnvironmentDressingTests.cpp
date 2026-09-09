#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Game/BreakerZoneBuilder.h"
#include "Materials/MaterialInterface.h"
#include "NaniteSceneProxy.h"
#include "StaticMeshResources.h"

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
            bool bUnsupportedBlend = false;
            for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
            {
                const UMaterialInterface* Material = Slot.MaterialInterface;
                if (Material && Material->GetNaniteOverride()) Material = Material->GetNaniteOverride();
                if (Material && !Nanite::IsSupportedBlendMode(*Material)) bUnsupportedBlend = true;
            }
            const bool bNeedsFallback = Mesh->IsNaniteEnabled() && bUnsupportedBlend;
            TestEqual(FString::Printf(TEXT("%s uses fallback only for unsupported imported blend modes"), Name),
                Component->IsForceDisableNanite(), bNeedsFallback);
            if (bNeedsFallback)
                TestTrue(FString::Printf(TEXT("%s has real fallback LOD geometry"), Name), Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() > 0
                    && Mesh->GetRenderData()->LODResources[0].GetNumVertices() > 0);
            AddInfo(FString::Printf(TEXT("Dressing %s Nanite=%d unsupportedBlend=%d componentFallback=%d"),
                Name, Mesh->IsNaniteEnabled(), bUnsupportedBlend, Component->IsForceDisableNanite()));
            for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
                TestTrue(FString::Printf(TEXT("%s retains imported material slot %d"), Name, Slot),
                    Component->GetMaterial(Slot) == Mesh->GetStaticMaterials()[Slot].MaterialInterface);
            Actor->Destroy();
        }
    }
    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("Actual district assembles"), UBreakerZoneBuilder::BuildFernhallYard(World, Markers))) return false;
    TArray<FBreakerZonePiece> Pieces;
    if (!UBreakerZoneBuilder::CollectZonePieces(UBreakerZoneBuilder::FernhallMeshFolder(), Pieces)) return false;
    TArray<FBox> YardBounds;
    for (const auto& Piece : Pieces)
        if (Piece.Name == TEXT("flr_yard") || Piece.Name == TEXT("flr_yard_sub"))
            if (const auto* Mesh = Cast<UStaticMesh>(Piece.MeshPath.TryLoad())) YardBounds.Add(Mesh->GetBoundingBox());
    if (!TestEqual(TEXT("Both authored yard floors exist"), YardBounds.Num(), 2)) return false;
    int32 YardCounts[2] = {0,0};
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(TEXT("FernhallSurfaceDetail"))) continue;
        auto* Component = It->GetStaticMeshComponent();
        TestFalse(TEXT("District overlays preserve combat collision"), It->GetActorEnableCollision());
        TestEqual(TEXT("District overlays never answer traces"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestFalse(TEXT("District overlays preserve navigation"), Component->CanEverAffectNavigation());
        const FBox Box = Component->CalcBounds(Component->GetComponentTransform()).GetBox();
        bool bContained = false;
        for (int32 Yard = 0; Yard < YardBounds.Num(); ++Yard)
        {
            const FBox& Floor = YardBounds[Yard];
            if (Box.Min.X >= Floor.Min.X && Box.Max.X <= Floor.Max.X
                && Box.Min.Y >= Floor.Min.Y && Box.Max.Y <= Floor.Max.Y)
            {
                ++YardCounts[Yard]; bContained = true; break;
            }
        }
        TestTrue(TEXT("Every overlay lies within its authored floor footprint"), bContained);
        TestTrue(TEXT("Surface dressing stays a shallow overlay"), Box.GetSize().Z <= 2.1);
        TestNotNull(TEXT("Surface dressing has an assigned material"), Component->GetMaterial(0));
    }
    TSet<FName> Landmarks;
    for(TActorIterator<AStaticMeshActor> It(World);It;++It)
    {
        if(!IsValid(*It)||!It->ActorHasTag(TEXT("FernhallPerimeterPass")))continue;
        const auto* Component=It->GetStaticMeshComponent();
        TestFalse(TEXT("New perimeter cannot block authored routes"),It->GetActorEnableCollision());
        TestEqual(TEXT("New perimeter cannot change combat trace outcomes"),Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        TestFalse(TEXT("New perimeter cannot change navigation"),Component->CanEverAffectNavigation());
        TestFalse(TEXT("New perimeter has no per-frame work"),It->IsActorTickEnabled());
        for(const FName Tag:{FName(TEXT("Fernhall.Landmark.ReclaimedGrove")),FName(TEXT("Fernhall.Landmark.PipeWorks"))})
            if(It->ActorHasTag(Tag))Landmarks.Add(Tag);
        const UStaticMesh* Mesh=Component->GetStaticMesh();
        TestTrue(TEXT("Landmarks use actual installed meshes"),Mesh&&Mesh->GetPathName().StartsWith(TEXT("/Game/Breaker/EnvironmentKit/")));
    }
    TestEqual(TEXT("Both distinct pocket silhouettes survive courtyard clearance"),Landmarks.Num(),2);
    TestTrue(TEXT("First authored yard has surface treatment"), YardCounts[0] > 0);
    TestTrue(TEXT("Second authored yard has surface treatment"), YardCounts[1] > 0);
    return true;
}
#endif
