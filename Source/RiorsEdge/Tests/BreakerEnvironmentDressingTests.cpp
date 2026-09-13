#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Game/BreakerZoneBuilder.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
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

// ---------------------------------------------------------------------------
// A SURFACE SHOWS WHAT IT IS MADE OF (O279). Three kinds of slot leave the
// builder three ways: a kit piece that was imported with a textured material
// keeps it, untouched; a composer slab (flr_) wears the tiled ground material
// tinted by role; a piece that carries nothing gets the flat shape colour it
// always had. Asserted on the assembled yard, by mesh name, the way the
// builder itself decides — not on a fixture that would pass with any paint.
// ---------------------------------------------------------------------------
namespace
{
    // The finding's own definition of "carries a material": an instance with
    // at least one texture bound. A texture-less instance (the kit's colormap,
    // MI_Trim_02) shows its parent's grey and is the flat colour's to paint.
    bool BreakerSurfaceSlotIsTexturedImport(const UStaticMesh* Mesh)
    {
        if (!Mesh || Mesh->GetStaticMaterials().Num() == 0) return false;
        const UMaterialInstance* Instance = Cast<UMaterialInstance>(Mesh->GetStaticMaterials()[0].MaterialInterface);
        return Instance && Instance->TextureParameterValues.Num() > 0;
    }

    // A slot the paint is FOR: nothing at all, or an instance binding no
    // texture. A plain authored UMaterial is neither and is deliberately not
    // matched here, so the witness picked for the fallback is one the builder
    // has no reason to leave alone.
    bool BreakerSurfaceSlotIsBare(const UStaticMesh* Mesh)
    {
        if (!Mesh || Mesh->GetStaticMaterials().Num() == 0) return true;
        const UMaterialInterface* Material = Mesh->GetStaticMaterials()[0].MaterialInterface;
        if (!Material) return true;
        const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
        return Instance && Instance->TextureParameterValues.Num() == 0;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFernhallSurfacesShowTheirMaterialTest,
    "RiorsEdge.World.Fernhall.SurfacesShowTheirMaterial", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFernhallSurfacesShowTheirMaterialTest::RunTest(const FString& Parameters)
{
    // SHIPPED CONFIGURATION FIRST. The ground material is content, not code:
    // a checkout that has not built it falls back to the flat floor, and the
    // slab assertions below would then be asserting the fallback rather than
    // the rule. Refuse before the yard is built so the red names the asset.
    UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Breaker/Materials/M_BreakerGround.M_BreakerGround"));
    if (!TestNotNull(TEXT("Shipped ground material M_BreakerGround exists"), Ground)) return false;
    // The expression graph's references, not the compiled shader's used
    // list: the suite runs under nullrhi, where no shader map is built.
    bool bSamplesGrain = false;
    for (const UObject* Referenced : Ground->GetReferencedTextures())
        if (Referenced && Referenced->GetName() == TEXT("T_BreakerGround")) bSamplesGrain = true;
    TestTrue(TEXT("M_BreakerGround samples the tiled grain T_BreakerGround"), bSamplesGrain);
    UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!TestNotNull(TEXT("Engine flat shape material exists"), Shape)) return false;

    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("surface world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    FBreakerZoneMarkers Markers;
    if (!TestTrue(TEXT("Actual district assembles"), UBreakerZoneBuilder::BuildFernhallYard(World, Markers))) return false;

    // The malformed facade has a structural replacement; intact imported
    // cover keeps its own material. Verify both through the assembled world.
    const UStaticMeshComponent* Facade = nullptr;
    const UStaticMeshComponent* Imported = nullptr;
    const UStaticMeshComponent* Yard = nullptr;
    const UStaticMeshComponent* Block = nullptr;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (!IsValid(*It) || It->Tags.Contains(TEXT("FernhallSkylineDressing"))) continue;
        const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
        const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
        if (It->GetActorLabel() == TEXT("Fernhall_wall_n03")) Facade = Component;
        if (Mesh && Mesh->GetName() == TEXT("Fern_1") && Mesh->GetPathName().Contains(TEXT("EnvironmentKit"))) Imported = Component;
        if (!Mesh || !Mesh->GetPathName().StartsWith(UBreakerZoneBuilder::FernhallMeshFolder())) continue;
        const FString Name = Mesh->GetName();
        if (Name == TEXT("flr_yard")) Yard = Component;
        if (Name.StartsWith(TEXT("blk_")) && BreakerSurfaceSlotIsTexturedImport(Mesh)) Imported = Component;
        if (Name.StartsWith(TEXT("blk_full_")) && BreakerSurfaceSlotIsBare(Mesh) && !Block) Block = Component;
    }
    if (TestNotNull(TEXT("The repaired facade was spawned through the real builder"), Facade))
    {
        TestEqual(TEXT("Facade uses the coherent structural cube"),Facade->GetStaticMesh()->GetName(),FString(TEXT("Cube")));
        const auto* Surface = Cast<UMaterialInstanceDynamic>(Facade->GetMaterial(0));
        if (TestNotNull(TEXT("Facade binds a textured surface instance"),Surface))
        {
            auto* Wall = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Breaker/Materials/M_BreakerWall.M_BreakerWall"));
            TestNotNull(TEXT("Authored wall material exists"),Wall);
            TestTrue(TEXT("Facade is parented to the wall material"),Surface->Parent == Wall);
            bool Grain = false;
            if (Wall) for (const UObject* Texture : Wall->GetReferencedTextures())
                if (Texture && Texture->GetName() == TEXT("T_BreakerGround")) Grain = true;
            TestTrue(TEXT("Wall material references its grain"),Grain);
        }
    }
    if (TestNotNull(TEXT("An intact imported foliage piece was spawned"),Imported))
        TestTrue(TEXT("Intact foliage retains its imported material"),
            Imported->GetMaterial(0) == Imported->GetStaticMesh()->GetStaticMaterials()[0].MaterialInterface);

    // (b) A COMPOSER SLAB WEARS THE GROUND MATERIAL, TINTED BY ROLE. The yard
    // floor's shipped tint is the value the palette test holds as "yard floor".
    if (TestNotNull(TEXT("flr_yard was spawned"), Yard))
    {
        const UMaterialInstanceDynamic* Slab = Cast<UMaterialInstanceDynamic>(Yard->GetMaterial(0));
        if (TestNotNull(TEXT("flr_yard slot 0 is a dynamic instance"), Slab))
        {
            TestTrue(TEXT("flr_yard is parented to M_BreakerGround"), Slab->Parent == Ground);
            FLinearColor Tint = FLinearColor::Black;
            TestTrue(TEXT("flr_yard exposes its Color tint"),
                Slab->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Tint));
            const FLinearColor Expected(.25f, .27f, .25f);   // O2 PLACEHOLDER
            TestEqual(TEXT("flr_yard tint R"), Tint.R, Expected.R, .01f);
            TestEqual(TEXT("flr_yard tint G"), Tint.G, Expected.G, .01f);
            TestEqual(TEXT("flr_yard tint B"), Tint.B, Expected.B, .01f);
        }
    }

    // (c) A PIECE THAT CARRIES NOTHING GETS THE FLAT COLOUR. The fallback is
    // still the engine shape material, not the ground: cover is not floor.
    if (TestNotNull(TEXT("A blk_full_ piece carrying no material was spawned"), Block))
    {
        AddInfo(FString::Printf(TEXT("Fallback witness %s"), *Block->GetStaticMesh()->GetName()));
        const UMaterialInstanceDynamic* Paint = Cast<UMaterialInstanceDynamic>(Block->GetMaterial(0));
        if (TestNotNull(TEXT("blk_full_ slot 0 is a dynamic instance"), Paint))
            TestTrue(TEXT("blk_full_ fallback is parented to the flat shape material"), Paint->Parent == Shape);
    }
    return true;
}
#endif
