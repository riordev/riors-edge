#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerZonePalette.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Game/BreakerFernhallPerimeter.h"
#include "Game/BreakerFernhallCourtyardBuilder.h"
#include "EngineUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    bool BreakerZoneNameHasPrefix(const FString& Name, const TCHAR* Prefix)
    {
        return Name.StartsWith(Prefix, ESearchCase::IgnoreCase);
    }

    // O24 palette, same RGB values as BreakerGameMode.cpp's and
    // BreakerHubBuilder.cpp's file-local palettes so the yard reads as the
    // same world — re-declared rather than shared because those palettes are
    // deliberately file-local (see the unity-build note in the hub builder;
    // the Breaker prefix on everything here is the same rule). The zone
    // NEEDS a painted colour at all because the GLB route strips the kit's
    // palette texture — trimesh drops materials in the world-transform bake —
    // so an unpainted piece renders default-surface near-black. No teal:
    // teal is canon-reserved for rift objects, and none of this is one.
    const FLinearColor BreakerZoneConcrete (0.33f, 0.35f, 0.30f);
    const FLinearColor BreakerZoneStone    (0.24f, 0.26f, 0.23f);
    const FLinearColor BreakerZoneRust     (0.34f, 0.20f, 0.09f);
    const FLinearColor BreakerZoneEarth    (0.20f, 0.16f, 0.11f);
    const FLinearColor BreakerZoneOffWhite (0.58f, 0.57f, 0.51f);
    const FLinearColor BreakerZoneMoss     (0.14f, 0.26f, 0.11f);

    // Stock-material-plus-dynamic-instance, the project's zero-content
    // colour idiom. Applied to every slot: kit meshes ship one slot but that
    // is the exporter's business, not a contract.
    void BreakerZoneApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        // SAME MAP, DIFFERENT RULES, SO IT MUST NOT LOOK THE SAME. A rift
        // interior is this yard's own geometry with PendingRift set, and under
        // O268 it is the only one of the two that is finite and concludes. The
        // palette is what tells a player which of them they are standing in
        // before they act on it.
        //
        // The ruin is applied HERE, at the single point every painted surface in
        // the zone passes through, rather than inside BreakerZoneColorFor. The
        // first attempt did it at the lookup and the capture frames caught it:
        // the prefix table paints the composed pieces, but the walk strips,
        // joints, drains and skyline all carry their own literal colours
        // straight to this function — so the buildings went to ruin while the
        // ground the player actually looks at stayed pristine. One choke point
        // cannot be missed by a path that is added later either.
        //
        // PendingRift is transient travel state and IS unset on the way out
        // (ABreakerGameMode, "walking out of a place is not entering it again"),
        // so an ordinary yard cannot inherit a ruin from the rift before it.
        const UWorld* World = Mesh->GetWorld();
        const UBreakerGameInstance* Session = World ? World->GetGameInstance<UBreakerGameInstance>() : nullptr;
        const FLinearColor Painted = Session && Session->PendingRift.IsSet()
            ? BreakerZonePalette::Dilapidate(Color) : Color;
        for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
        {
            if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
            {
                Dynamic->SetVectorParameterValue(TEXT("Color"), Painted);
                Mesh->SetMaterial(Slot, Dynamic);
            }
        }
    }

    // The prefix decides the read: boundary buildings in concrete, line
    // breaks in stone, chest cover as weathered tech (rust), ground in
    // earth, the rift pad off-white so the far end of the lane is visibly
    // SOMEWHERE before the rift actor exists, dressing in moss.
    FLinearColor BreakerZoneColorFor(const FString& Name)
    {
        if (Name.StartsWith(TEXT("flr_riftpad"))) return BreakerZoneOffWhite;
        if (Name == TEXT("flr_yard")) return FLinearColor(.36f,.36f,.31f);
        if (Name == TEXT("flr_yard_sub")) return FLinearColor(.17f,.20f,.19f);
        if (Name.StartsWith(TEXT("flr_seam"))) return FLinearColor(.43f,.42f,.35f);
        if (BreakerZoneNameHasPrefix(Name, TEXT("wall_"))) return BreakerZoneConcrete;
        if (BreakerZoneNameHasPrefix(Name, TEXT("blk_full_"))) return BreakerZoneStone;
        if (BreakerZoneNameHasPrefix(Name, TEXT("blk_chest_"))) return BreakerZoneRust;
        if (BreakerZoneNameHasPrefix(Name, TEXT("flr_"))) return BreakerZoneEarth;
        return BreakerZoneMoss;
    }

    void BreakerZoneBuildSurfaceDetail(UWorld* World, const TArray<FBreakerZonePiece>& Pieces)
    {
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Cube) return;
        auto Strip = [&](FVector Position, FVector Size, FLinearColor Color)
        {
            auto* Actor = World->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
            if (!Actor) return;
            auto* Mesh = Actor->GetStaticMeshComponent();
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetStaticMesh(Cube); Mesh->SetWorldScale3D(Size / 100.0f);
            Mesh->SetCollisionProfileName(TEXT("NoCollision"));
            Mesh->SetCanEverAffectNavigation(false); Mesh->SetCastShadow(false);
            BreakerZoneApplyColor(Mesh, Color);
            Mesh->SetMobility(EComponentMobility::Static);
            Actor->SetActorEnableCollision(false);
            Actor->Tags.Add(TEXT("FernhallSurfaceDetail"));
        };
        for (const auto& Piece : Pieces)
        {
            const bool bEntry = Piece.Name == TEXT("flr_yard");
            const bool bIndustrial = Piece.Name == TEXT("flr_yard_sub");
            if (!bEntry && !bIndustrial) continue;
            const auto* Floor = Cast<UStaticMesh>(Piece.MeshPath.TryLoad());
            if (!Floor) continue;
            const FBox Bounds = Floor->GetBoundingBox();
            const FVector Centre = Bounds.GetCenter();
            const float Z = Bounds.Max.Z + 1.5f;
            // O2 surface treatment: broad service walk, recessed-looking joints,
            // and paired drains. These are visual overlays, never new floor collision.
            const float HalfLength = Bounds.GetExtent().X - 100;
            if (HalfLength <= 0) continue;
            for (float X = -HalfLength + 150; X < HalfLength; X += 300)
                Strip(FVector(Centre.X+X,Centre.Y,Z), FVector(292,760,2),
                    bIndustrial ? FLinearColor(.29f,.31f,.28f) : FLinearColor(.47f,.46f,.39f));
            for (float Side : {-1.0f,1.0f})
            {
                Strip(FVector(Centre.X,Centre.Y+Side*420,Z), FVector(HalfLength*2,34,2), BreakerZoneStone*.55f);
                for (float X = -HalfLength+100; X < HalfLength; X += 400)
                    Strip(FVector(Centre.X+X,Centre.Y+Side*420,Z+1), FVector(9,34,2), BreakerZoneRust);
                if (bIndustrial)
                    for (float X = -HalfLength+250; X < HalfLength; X += 900)
                        Strip(FVector(Centre.X+X,Centre.Y+Side*540,Z), FVector(220,24,2), BreakerZoneOffWhite);
            }
        }
    }

    void BreakerZoneBuildSkyline(UWorld* World, const TArray<FBreakerZonePiece>& Pieces)
    {
        auto FindMesh = [&](const TCHAR* Name) -> UStaticMesh*
        {
            const auto* Piece = Pieces.FindByPredicate([&](const auto& Entry) { return Entry.Name == Name; });
            return Piece ? Cast<UStaticMesh>(Piece->MeshPath.TryLoad()) : nullptr;
        };
        UStaticMesh* EntryFloor = FindMesh(TEXT("flr_yard"));
        UStaticMesh* SubFloor = FindMesh(TEXT("flr_yard_sub"));
        UStaticMesh* Mound = FindMesh(TEXT("dress_mound"));
        UStaticMesh* Building = FindMesh(TEXT("wall_n00"));
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!EntryFloor || !SubFloor || !Mound || !Building || !Cube) return;

        auto Place = [&](UStaticMesh* Mesh, const TCHAR* Label, FVector Centre, FVector Size, FLinearColor Color) -> AStaticMeshActor*
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const FVector Full = Bounds.BoxExtent * 2;
            if (Full.GetMin() <= UE_SMALL_NUMBER) return nullptr;
            const FVector Scale(Size.X / Full.X, Size.Y / Full.Y, Size.Z / Full.Z);
            // Imported Fernhall vertices already contain world placement.
            // Subtract their scaled origin, rather than moving the old yard twice.
            auto* Actor = World->SpawnActor<AStaticMeshActor>(Centre - Bounds.Origin * Scale, FRotator::ZeroRotator);
            if (!Actor) return nullptr;
            auto* Component = Actor->GetStaticMeshComponent();
            Component->SetMobility(EComponentMobility::Movable);
            Component->SetStaticMesh(Mesh); Component->SetWorldScale3D(Scale);
            Component->SetCollisionProfileName(TEXT("NoCollision"));
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetCanEverAffectNavigation(false);
            BreakerZoneApplyColor(Component, Color);
            Component->SetMobility(EComponentMobility::Static);
            Actor->SetActorEnableCollision(false); Actor->SetActorTickEnabled(false);
            Actor->Tags.Add(TEXT("FernhallSkylineDressing"));
#if WITH_EDITOR
            Actor->SetActorLabel(FString::Printf(TEXT("FernhallSkyline_%s"), Label));
#endif
            return Actor;
        };
        const FVector EntryCentre = EntryFloor->GetBounds().Origin;
        const FVector SubCentre = SubFloor->GetBounds().Origin;
        for (int32 Yard = 0; Yard < 2; ++Yard)
        {
            const auto Bounds = (Yard == 0 ? EntryFloor : SubFloor)->GetBounds();
            const FVector Centre = Bounds.Origin;
            // Use only the outward flank of each yard. The inward flanks contain
            // the connecting seam and must remain visually and physically open.
            const float Side = (Yard == 0 ? EntryCentre.Y - SubCentre.Y : SubCentre.Y - EntryCentre.Y) >= 0 ? 1.0f : -1.0f;
            const float Edge = Centre.Y + Side * Bounds.BoxExtent.Y;
            for (int32 Index = 0; Index < 5; ++Index)
            {
                const float X = Centre.X + (Index - 2) * 2200.0f;
                const float Height = 550.0f + (Index % 3) * 220.0f;
                Place(Mound, TEXT("OuterBank"), FVector(X, Edge + Side * 1700, Height * .5f - 80),
                    FVector(3000, 2300, Height), BreakerZoneEarth);
                Place(Mound, TEXT("DistantRidge"), FVector(X + 500, Edge + Side * 3500, Height * .65f - 120),
                    FVector(3800, 2500, Height * 1.3f), BreakerZoneMoss * .72f);
            }
            // Tall intact-material silhouettes behind the outward walls,
            // with lower foliage breaking up their bases. No gameplay collision.
            for (int32 Index = 0; Index < 5; ++Index)
            {
                const float X = Centre.X + (Index - 2) * 1900.0f;
                BreakerPlaceEnvironmentDressing(World, Index % 3 == 1 ? TEXT("DeadTree_1") : TEXT("CommonTree_1"),
                    FVector(X, Edge + Side * 1200, 0), 2200 + (Index % 2) * 300, Index * 67.0f);
                BreakerPlaceEnvironmentDressing(World, TEXT("Bush_Common"),
                    FVector(X + 430, Edge - Side * 600, 0), 180, Index * 41.0f);
                BreakerPlaceEnvironmentDressing(World, TEXT("Fern_1"),
                    FVector(X - 300, Edge - Side * 620, 0), 110, Index * 83.0f);
            }            for (int32 Index = 0; Index < 3; ++Index)
            {
                const float Height = 1100.0f + ((Index + Yard) % 3) * 320.0f;
                const float X = Centre.X + (Index - 1) * 2800.0f;
                auto* Roof = Place(Building, TEXT("Roofline"), FVector(X, Edge + Side * 650, Height * .5f),
                    FVector(1700, 850, Height), BreakerZoneConcrete * .82f);
                auto* Housing = Place(Cube, TEXT("RoofServiceHousing"), FVector(X + 340, Edge + Side * 680, Height + 140),
                    FVector(450, 420, 280), BreakerZoneRust * .65f);
                if (Housing) Housing->SetOwner(Roof);
            }
        }
        // A recognizable industrial destination beyond the far Substation wall:
        // unequal open masts and crossbars, not another solid boundary slab.
        const auto SubBounds = SubFloor->GetBounds();
        const float FarX = SubCentre.X + SubBounds.BoxExtent.X + 650;
        for (int32 Mast = 0; Mast < 2; ++Mast)
        {
            const float Y = SubCentre.Y + (Mast == 0 ? -650 : 650);
            const float Height = Mast == 0 ? 2300.0f : 1950.0f;
            for (float Offset : {-110.0f, 110.0f})
                Place(Cube, TEXT("MastUpright"), FVector(FarX, Y + Offset, Height * .5f), FVector(90, 70, Height), BreakerZoneRust);
            for (int32 Rung = 1; Rung <= 5; ++Rung)
                Place(Cube, TEXT("MastCrossbar"), FVector(FarX, Y, Rung * 330), FVector(100, 330, 45), BreakerZoneConcrete);
        }
        BreakerPlaceEnvironmentDressing(World, TEXT("Column_Pipes"), FVector(FarX - 80, SubCentre.Y - 750, 0), 1700, 0);
        BreakerPlaceEnvironmentDressing(World, TEXT("Column_MetalSupport"), FVector(FarX - 80, SubCentre.Y + 750, 0), 1500, 0);
        Place(Cube, TEXT("BrokenGantry"), FVector(FarX, SubCentre.Y - 120, 1750), FVector(120, 1100, 100), BreakerZoneRust);
    }
}

bool UBreakerZoneBuilder::CollectZonePieces(const FString& MeshFolder, TArray<FBreakerZonePiece>& OutPieces)
{
    OutPieces.Reset();
    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    // The registry may still be scanning when a commandlet asks this early;
    // a synchronous scan of one folder is cheap and makes the answer complete
    // rather than whatever happened to be indexed yet — the difference between
    // "the yard has 58 pieces" and a suite run that measured 13 of them.
    AssetRegistry.Get().ScanPathsSynchronous({ MeshFolder }, /*bForceRescan=*/false);

    TArray<FAssetData> Assets;
    AssetRegistry.Get().GetAssetsByPath(FName(*MeshFolder), Assets, /*bRecursive=*/true);

    for (const FAssetData& Data : Assets)
    {
        if (Data.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName()) continue;
        UStaticMesh* Mesh = Cast<UStaticMesh>(Data.GetAsset());
        if (!Mesh)
        {
            UE_LOG(LogTemp, Error, TEXT("[Zone] %s is registered but would not load; refusing a partial zone."),
                *Data.AssetName.ToString());
            OutPieces.Reset();
            return false;
        }
        FBreakerZonePiece& Piece = OutPieces.AddDefaulted_GetRef();
        Piece.Name = Data.AssetName.ToString();
        Piece.MeshPath = Data.ToSoftObjectPath();
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        Piece.Origin = Bounds.Origin;
        Piece.Extent = Bounds.BoxExtent;
    }

    if (OutPieces.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Zone] no static meshes under %s. The import step (breaker_import_fernhall.py) has not run."),
            *MeshFolder);
        return false;
    }
    return true;
}

const FBreakerZoneMarker* FBreakerZoneMarkers::Find(EBreakerZoneMarkerRole Role, FName Yard) const
{
    for (const FBreakerZoneMarker& Marker : All)
    {
        if (Marker.Role == Role && Marker.Yard == Yard) return &Marker;
    }
    return nullptr;
}

TArray<FBreakerZoneMarker> FBreakerZoneMarkers::OfRole(EBreakerZoneMarkerRole Role) const
{
    TArray<FBreakerZoneMarker> Out;
    for (const FBreakerZoneMarker& Marker : All)
    {
        if (Marker.Role == Role) Out.Add(Marker);
    }
    return Out;
}

TArray<FName> FBreakerZoneMarkers::Yards() const
{
    TArray<FName> Out;
    for (const FBreakerZoneMarker& Marker : All) Out.AddUnique(Marker.Yard);
    return Out;
}

bool FBreakerZoneMarkers::IsComplete(FString& OutReason) const
{
    const int32 Starts = OfRole(EBreakerZoneMarkerRole::PlayerStart).Num();
    if (Starts != 1)
    {
        OutReason = FString::Printf(
            TEXT("a zone needs exactly one player start and this one has %d"), Starts);
        return false;
    }
    // No (role, yard) pair may repeat. Two rift doors in one yard is not two
    // doors — they would spawn on top of each other — it is a naming mistake,
    // and Find() would silently return the first either way.
    for (int32 A = 0; A < All.Num(); ++A)
    {
        for (int32 B = A + 1; B < All.Num(); ++B)
        {
            if (All[A].Role == All[B].Role && All[A].Yard == All[B].Yard)
            {
                OutReason = FString::Printf(TEXT("two '%s' markers in yard '%s'"),
                    UBreakerZoneBuilder::MarkerRoleName(All[A].Role),
                    All[A].Yard.IsNone() ? TEXT("<entry>") : *All[A].Yard.ToString());
                return false;
            }
        }
    }
    // EVERY NAMED YARD IS ANCHORED. The entry yard is exempt because the player
    // start anchors it; any other yard that a door or a giver names needs a
    // frame of its own, and without one its grammar would be measured in the
    // entry yard's frame and pass while meaning nothing.
    for (const FName& Yard : Yards())
    {
        if (Yard.IsNone()) continue;
        if (!Has(EBreakerZoneMarkerRole::Yard, Yard))
        {
            OutReason = FString::Printf(
                TEXT("yard '%s' is named by a marker but has no 'yard' anchor to give it a frame"),
                *Yard.ToString());
            return false;
        }
    }

    OutReason.Reset();
    return true;
}

const TCHAR* UBreakerZoneBuilder::MarkerRoleName(EBreakerZoneMarkerRole Role)
{
    switch (Role)
    {
    case EBreakerZoneMarkerRole::PlayerStart: return TEXT("playerstart");
    case EBreakerZoneMarkerRole::Rift:        return TEXT("rift");
    case EBreakerZoneMarkerRole::NPCContract: return TEXT("npc_contract");
    case EBreakerZoneMarkerRole::Yard:        return TEXT("yard");
    }
    return TEXT("<unknown>");
}

bool UBreakerZoneBuilder::ParseMarkerName(const FString& Name, EBreakerZoneMarkerRole& OutRole, FName& OutYard)
{
    static const TCHAR* Prefix = TEXT("marker_");
    if (!Name.StartsWith(Prefix, ESearchCase::CaseSensitive)) return false;
    const FString Rest = Name.RightChop(FCString::Strlen(Prefix));

    // LONGEST ROLE FIRST. `npc_contract` contains an underscore, so a parse
    // that took the first token would read `marker_npc_contract` as role
    // `npc` in a yard called `contract` — the existing yard would import as
    // a zone with no contract marker and nothing would say why.
    static const EBreakerZoneMarkerRole Roles[] = {
        EBreakerZoneMarkerRole::NPCContract,
        EBreakerZoneMarkerRole::PlayerStart,
        EBreakerZoneMarkerRole::Rift,
        EBreakerZoneMarkerRole::Yard,
    };
    const EBreakerZoneMarkerRole* Best = nullptr;
    int32 BestLength = 0;
    for (const EBreakerZoneMarkerRole& Role : Roles)
    {
        const FString RoleName = MarkerRoleName(Role);
        if (!Rest.StartsWith(RoleName, ESearchCase::CaseSensitive)) continue;
        // The role must end at a boundary: either the whole remainder, or
        // followed by the yard separator. Without this `rift` would match
        // `riftpad` and a floor piece would become a marker.
        if (Rest.Len() != RoleName.Len() && Rest[RoleName.Len()] != TEXT('_')) continue;
        if (RoleName.Len() > BestLength) { Best = &Role; BestLength = RoleName.Len(); }
    }
    if (!Best) return false;

    OutRole = *Best;
    // No suffix means the ENTRY yard, which is what keeps every name authored
    // before yards existed valid with no re-export.
    OutYard = Rest.Len() == BestLength ? NAME_None : FName(*Rest.RightChop(BestLength + 1));
    return true;
}

bool UBreakerZoneBuilder::ExtractMarkers(const TArray<FBreakerZonePiece>& Pieces, FBreakerZoneMarkers& OutMarkers)
{
    OutMarkers = FBreakerZoneMarkers();
    for (const FBreakerZonePiece& Piece : Pieces)
    {
        // Marker positions are the box centre on the ground plane: the
        // composer floors each marker cube to Y=0, so min-Z is the walkable
        // surface the marked thing stands on.
        EBreakerZoneMarkerRole Role;
        FName Yard;
        if (!ParseMarkerName(Piece.Name, Role, Yard))
        {
            // A `marker_`-prefixed name that does not parse is a TYPO, not a
            // piece of scenery: the prefix is the contract and nothing else
            // uses it. Refusing loudly is the difference between "the yard has
            // no contract giver" and "someone spelled it wrong".
            if (Piece.Name.StartsWith(TEXT("marker_"), ESearchCase::CaseSensitive))
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[Zone] '%s' is prefixed marker_ but names no known role; refusing the zone."),
                    *Piece.Name);
                OutMarkers = FBreakerZoneMarkers();
                return false;
            }
            continue;
        }
        FBreakerZoneMarker& Marker = OutMarkers.All.AddDefaulted_GetRef();
        Marker.Role = Role;
        Marker.Yard = Yard;
        Marker.Location = FVector(Piece.Origin.X, Piece.Origin.Y, Piece.Origin.Z - Piece.Extent.Z);
    }

    FString Reason;
    if (!OutMarkers.IsComplete(Reason))
    {
        UE_LOG(LogTemp, Error, TEXT("[Zone] marker set rejected: %s. The export is broken, not the layout."),
            *Reason);
        return false;
    }
    return true;
}

FName UBreakerZoneBuilder::YardForPoint(const FBreakerZoneMarkers& Markers, const FVector& Point)
{
    // A PIECE IS IN THE YARD WHOSE BAND CONTAINS IT, tested in that yard's own
    // frame. The first version of this took the nearest ANCHOR and was wrong
    // for a reason worth keeping: anchors sit at a yard's EDGE — the player
    // start at the entry yard's mouth, the yard marker at the substation's —
    // so the entry yard's far cover is nearer the NEXT yard's anchor than its
    // own. Six of sixteen pieces changed rooms, and the per-yard count caught
    // it. Nearest-anything is a proxy; the band is the actual question.
    FName Best = NAME_None;
    double BestDistanceSq = TNumericLimits<double>::Max();
    bool bFoundContaining = false;

    for (const FBreakerZoneMarker& Marker : Markers.All)
    {
        // Only the two roles that ANCHOR a yard. A rift or a giver sits IN a
        // yard and cannot define one.
        if (Marker.Role != EBreakerZoneMarkerRole::PlayerStart
            && Marker.Role != EBreakerZoneMarkerRole::Yard)
        {
            continue;
        }

        FVector2D Origin;
        FVector2D Forward;
        if (!YardFrame(Markers, Marker.Yard, Origin, Forward)) continue;
        const FVector2D Right(-Forward.Y, Forward.X);
        const FVector2D Offset = FVector2D(Point.X, Point.Y) - Origin;
        const double Fwd = FVector2D::DotProduct(Offset, Forward);
        const double Rgt = FVector2D::DotProduct(Offset, Right);

        const FBreakerCoverFieldParams Params = FernhallFieldParams(Marker.Yard);
        if (Fwd >= Params.BandNearCm && Fwd <= Params.BandFarCm
            && FMath::Abs(Rgt) <= Params.BandHalfWidthCm)
        {
            return Marker.Yard;
        }

        // Not in any band yet: remember the nearest yard CENTRE, so a piece
        // outside every band (dressing, a wall, a piece just past an edge)
        // still lands somewhere defensible rather than nowhere.
        if (!bFoundContaining)
        {
            const FVector2D Centre = Origin + Forward * (0.5 * (Params.BandNearCm + Params.BandFarCm));
            const double DistanceSq = FVector2D::DistSquared(FVector2D(Point.X, Point.Y), Centre);
            if (DistanceSq < BestDistanceSq)
            {
                BestDistanceSq = DistanceSq;
                Best = Marker.Yard;
            }
        }
    }
    return Best;
}

bool UBreakerZoneBuilder::YardFrame(const FBreakerZoneMarkers& Markers, FName Yard,
    FVector2D& OutOrigin, FVector2D& OutForward)
{
    const FBreakerZoneMarker* Anchor = Yard.IsNone()
        ? Markers.Find(EBreakerZoneMarkerRole::PlayerStart)
        : Markers.Find(EBreakerZoneMarkerRole::Yard, Yard);
    if (!Anchor) return false;

    OutOrigin = FVector2D(Anchor->Location.X, Anchor->Location.Y);
    // Forward is what this yard POINTS AT, which is its own rift. A yard with
    // no rift keeps +X, the same fallback the entry yard has always had.
    const FBreakerZoneMarker* Rift = Markers.Find(EBreakerZoneMarkerRole::Rift, Yard);
    OutForward = Rift
        ? FVector2D(Rift->Location.X, Rift->Location.Y) - OutOrigin
        : FVector2D::ZeroVector;
    if (!OutForward.Normalize()) OutForward = FVector2D(1.0f, 0.0f);
    return true;
}

TArray<FBreakerZoneField> UBreakerZoneBuilder::BuildZoneFields(const TArray<FBreakerZonePiece>& Pieces,
    const FBreakerZoneMarkers& Markers)
{
    TArray<FBreakerZoneField> Fields;
    for (const FBreakerZoneMarker& Marker : Markers.All)
    {
        if (Marker.Role != EBreakerZoneMarkerRole::PlayerStart
            && Marker.Role != EBreakerZoneMarkerRole::Yard)
        {
            continue;
        }
        FBreakerZoneField& Field = Fields.AddDefaulted_GetRef();
        Field.Yard = Marker.Yard;
        Field.Params = FernhallFieldParams(Marker.Yard);
        Field.Pieces = BuildCoverPieces(Pieces, Markers, Marker.Yard);
    }
    return Fields;
}

int32 UBreakerZoneBuilder::FernhallYardAreaLevel(FName Yard)
{
    // HOW FAR THE PLAYER HAS WALKED, expressed as a number. A yard reached
    // through another yard is content they earned their way to, and the GAP
    // between the four is the thing being authored rather than any one
    // magnitude. All O2 PLACEHOLDER.
    //
    // THIS IS SEPARATE FROM FernhallRiftFor ON PURPOSE. Three of the four yards
    // have a rift door and carry a definition; the DEPOT has none, and giving
    // it a rift definition purely to read one field off it would be authoring
    // an encounter the player can never reach.
    //
    // THE SIDING IS BESIDE THE ENTRY YARD'S, NOT ABOVE IT (O276). It hangs off
    // the entry plaza's west flank — one seam from the door the player arrives
    // at, the same distance the substation is — but it is the side room, not
    // the road onward, so it sits one step over the entry rather than on the
    // substation's rung. O2 PLACEHOLDER.
    if (Yard == FName(TEXT("breach"))) return 20;
    if (Yard == FName(TEXT("depot"))) return 13;
    if (Yard == FName(TEXT("substation"))) return 9;
    if (Yard == FName(TEXT("siding"))) return 6;
    return 5;
}

FBreakerRiftDefinition UBreakerZoneBuilder::FernhallRiftFor(FName Yard)
{
    FBreakerRiftDefinition Rift;
    // Campaign, all of them: O122 makes a campaign rift free to enter and O82
    // makes respawn unlimited inside it. Consumable endgame rifts arrive with
    // their own entry cost and are not authored here.
    Rift.Tier = EBreakerRiftTier::Campaign;

    if (Yard == FName(TEXT("breach")))
    {
        Rift.EncounterId = TEXT("breach.marshalling");
        Rift.AreaName = FText::FromString(TEXT("Breach Marshalling Yard"));
        Rift.AreaLine = FText::FromString(TEXT("Beyond the altered contact, the Field Marshal gathers the breach forces."));
        Rift.AreaLevel = FernhallYardAreaLevel(Yard);
        return Rift;
    }

    if (Yard == FName(TEXT("substation")))
    {
        Rift.EncounterId = TEXT("fernhall.substation");
        Rift.AreaName = FText::FromString(TEXT("Substation Undercroft"));
        Rift.AreaLine = FText::FromString(
            TEXT("Below the transformer yard, where the rift went looking for power."));
        // DEEPER THAN THE ENTRY YARD'S, because it is further from the door the
        // player arrives at — a yard reached through another yard is content
        // they have walked to. The GAP between the two is the thing being
        // authored; both magnitudes are O2 PLACEHOLDER.
        Rift.AreaLevel = FernhallYardAreaLevel(Yard);
        return Rift;
    }

    if (Yard == FName(TEXT("siding")))
    {
        // THE SIDE RIFT (O276). No mission beat names it: it completes on its
        // generic terminator, BossForRift answers NAME_None for it, and the
        // door's gate asks nothing of the journal. It exists so the player
        // has a rift to run that the story is not standing in front of.
        Rift.EncounterId = TEXT("fernhall.siding");
        Rift.AreaName = FText::FromString(TEXT("Fernhall Siding"));
        Rift.AreaLine = FText::FromString(
            TEXT("A spur off the plaza where the rolling stock was left, and something else arrived."));
        Rift.AreaLevel = FernhallYardAreaLevel(Yard);
        return Rift;
    }

    Rift.EncounterId = TEXT("fernhall.entry");
    Rift.AreaName = FText::FromString(TEXT("Fernhall Substation"));
    Rift.AreaLine = FText::FromString(
        TEXT("The tear under the substation, where the yard stops being quiet."));
    Rift.AreaLevel = FernhallYardAreaLevel(Yard);
    return Rift;
}

TArray<FBreakerZoneConnection> UBreakerZoneBuilder::FernhallConnections()
{
    TArray<FBreakerZoneConnection> Out;

    // THE FIRST SEAM IN THE WORLD. A dog-leg east then north, so nothing at
    // either mouth can see the other: no-through-sight is the term O1 forces,
    // because a straight seam lets a ranged enemy in the far yard hold a player
    // whose cover was never laid for that angle.
    //
    // Every magnitude is O2 PLACEHOLDER and matches what the composer built:
    // a 10 m mouth under the 12 m ceiling, and 29 m walked under the 30 m one.
    // The geometry honours these numbers; it does not derive them.
    FBreakerZoneConnection& Seam = Out.AddDefaulted_GetRef();
    Seam.Name = FName(TEXT("plaza-substation"));
    Seam.FromYard = NAME_None;
    Seam.ToYard = FName(TEXT("substation"));
    Seam.MouthWidthCm = 1000.0f;   // O2 PLACEHOLDER
    Seam.LengthCm = 2900.0f;   // O2 PLACEHOLDER
    Seam.bThroughSight = false;

    // THE SECOND SEAM, out of the substation's north flank and turning east.
    // Same terms, same ceilings, and the same reason: a straight corridor
    // between two yards lets a body in the far one hold a player whose cover
    // was never laid for that angle. 10 m mouth under the 12 m ceiling, 28 m
    // walked under the 30 m one. O2 PLACEHOLDER, and the geometry honours
    // these numbers rather than deriving them.
    FBreakerZoneConnection& Second = Out.AddDefaulted_GetRef();
    Second.Name = FName(TEXT("substation-depot"));
    Second.FromYard = FName(TEXT("substation"));
    Second.ToYard = FName(TEXT("depot"));
    Second.MouthWidthCm = 1000.0f;   // O2 PLACEHOLDER
    Second.LengthCm = 2800.0f;   // O2 PLACEHOLDER
    Second.bThroughSight = false;

    // THE THIRD SEAM (O276), off the entry plaza's WEST flank into the siding.
    // The plaza now has two seams leaving it, and this one turns so that a
    // body in the siding cannot hold the plaza — the same no-through-sight
    // term as the other two, for the same reason. 10 m mouth under the 12 m
    // ceiling, 29 m walked under the 30 m one. THE WALKED FIGURES ARE THE
    // COMPOSER'S: they are what the DATA hand built in compose_fernhall.py,
    // honoured here and not derived from the geometry. O2 PLACEHOLDER.
    FBreakerZoneConnection& Third = Out.AddDefaulted_GetRef();
    Third.Name = FName(TEXT("plaza-siding"));
    Third.FromYard = NAME_None;
    Third.ToYard = FName(TEXT("siding"));
    Third.MouthWidthCm = 1000.0f;   // O2 PLACEHOLDER
    Third.LengthCm = 2900.0f;   // O2 PLACEHOLDER
    Third.bThroughSight = false;
    return Out;
}

TArray<FBreakerCoverPiece> UBreakerZoneBuilder::BuildCoverPieces(const TArray<FBreakerZonePiece>& Pieces,
    const FBreakerZoneMarkers& Markers, FName Yard)
{
    TArray<FBreakerCoverPiece> Cover;
    // Field frame: origin at the player start, forward toward the rift. The
    // frame is DERIVED from the markers rather than assumed to be +X, so a
    // re-export that rotates the yard moves the frame with it and the grammar
    // numbers stay true.
    // THIS YARD'S frame, and only this yard's pieces. The single-yard
    // assumption that used to live here is gone: a zone is a list of yards,
    // each measured in a frame anchored where it was authored.
    FVector2D Start;
    FVector2D Forward;
    if (!YardFrame(Markers, Yard, Start, Forward)) return Cover;
    const FVector2D Right(-Forward.Y, Forward.X);

    int32 ClusterIndex = 0;
    for (const FBreakerZonePiece& Piece : Pieces)
    {
        const bool bFull = BreakerZoneNameHasPrefix(Piece.Name, TEXT("blk_full_"));
        const bool bChest = BreakerZoneNameHasPrefix(Piece.Name, TEXT("blk_chest_"));
        if (!bFull && !bChest) continue;
        // A piece is in the room it stands in.
        if (YardForPoint(Markers, Piece.Origin) != Yard) continue;
        const FVector2D Offset = FVector2D(Piece.Origin.X, Piece.Origin.Y) - Start;
        FBreakerCoverPiece& Out = Cover.AddDefaulted_GetRef();
        Out.Forward = FVector2D::DotProduct(Offset, Forward);
        Out.Right = FVector2D::DotProduct(Offset, Right);
        Out.HeightCm = 2.0f * Piece.Extent.Z;
        Out.HalfLengthCm = FMath::Max(Piece.Extent.X, Piece.Extent.Y);
        Out.HalfDepthCm = FMath::Min(Piece.Extent.X, Piece.Extent.Y);
        Out.YawDegrees = 0.0f;
        Out.Class = bFull ? EBreakerCoverClass::FullHeight : EBreakerCoverClass::ChestHigh;
        Out.ClusterIndex = ClusterIndex++;
    }
    return Cover;
}

FBreakerCoverFieldParams UBreakerZoneBuilder::FernhallFieldParams(FName Yard)
{
    // Both yards are the same size and answer to the same band, so the
    // parameter changes nothing today. It exists because the alternative is a
    // single function every yard silently shares, which is exactly how one
    // place's dimensions end up measuring another's.
    (void)Yard;
    FBreakerCoverFieldParams Params;
    // The yard's combat band, in its own field frame (start marker at X 6 m of
    // a 0-101 m yard, so 1400 cm forward is the 20 m line). The entry plaza
    // and the ground past the last cover pair are deliberately open — the same
    // exclusion the gym's safe ring and instrument corridor claim. All O2
    // PLACEHOLDER.
    Params.BandNearCm = 1400.0f;
    Params.BandFarCm = 8900.0f;
    Params.BandHalfWidthCm = 2000.0f;
    Params.SafeZoneRadiusCm = 1400.0f;
    // The central dash lane is the yard's corridor: kept clear of all cover
    // inside 900 cm of the centreline, chest shoulders on the 1050 cm flank
    // line, no full-height anywhere across it.
    Params.CorridorNearCm = 1400.0f;
    Params.CorridorFarCm = 8900.0f;
    Params.CorridorHalfWidthCm = 900.0f;
    Params.CorridorShoulderOffsetCm = 1050.0f;
    // Gym-only exclusions parked OUT OF RANGE, not zeroed: a zeroed rectangle
    // still sits at the field origin, and this yard has real ground there.
    Params.JumpRunNearCm = 1.0e7f;
    Params.JumpRunFarCm = 1.0e7f;
    Params.SniperLaneRightCm = 1.0e7f;
    Params.WallLaneRightCm = 1.0e7f;
    return Params;
}

const TCHAR* UBreakerZoneBuilder::FernhallFolderFor(bool bRiftInstance)
{
    if (!bRiftInstance) return FernhallMeshFolder();
    // ASKED OF THE REGISTRY DIRECTLY, and QUIETLY, rather than through
    // CollectZonePieces. That function logs an Error on an empty folder and is
    // right to — a missing LIVING yard is a broken build, and the suite treats
    // a logged error as a failure. An absent ruined twin is not broken, it is
    // a checkout that has not imported one, so asking about it must not raise.
    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    AssetRegistry.Get().ScanPathsSynchronous({ FString(FernhallRiftMeshFolder()) }, /*bForceRescan=*/false);
    TArray<FAssetData> Ruined;
    AssetRegistry.Get().GetAssetsByPath(FName(FernhallRiftMeshFolder()), Ruined, /*bRecursive=*/true);
    if (Ruined.Num() > 0)
    {
        return FernhallRiftMeshFolder();
    }
    UE_LOG(LogTemp, Display,
        TEXT("[Zone] no ruined Fernhall imported (%s); the rift builds from the living yard."),
        FernhallRiftMeshFolder());
    return FernhallMeshFolder();
}

bool UBreakerZoneBuilder::BuildFernhallYard(UWorld* World, FBreakerZoneMarkers& OutMarkers, bool bRiftInstance)
{
    if (!World) return false;

    TArray<FBreakerZonePiece> Pieces;
    // The ruined twin when this is a rift and one has been imported; the living
    // yard otherwise. Same piece NAMES either way, so the markers, the
    // courtyard plan and the cover grammar all read the same yard — it is the
    // silhouette that changes, not the layout.
    if (!CollectZonePieces(FernhallFolderFor(bRiftInstance), Pieces)) return false;
    if (!ExtractMarkers(Pieces, OutMarkers)) return false;

    BreakerFernhallCourtyard::FPlan Courtyard;
    FString CourtyardError;
    if (!BreakerFernhallCourtyard::MakePlan(Pieces, Courtyard, CourtyardError)
        || !BreakerFernhallCourtyard::Build(World, Courtyard))
    {
        UE_LOG(LogTemp, Error, TEXT("[Zone] Maintenance courtyard refused: %s"), *CourtyardError);
        return false;
    }

    int32 Spawned = 0;
    for (const FBreakerZonePiece& Piece : Pieces)
    {
        if (BreakerZoneNameHasPrefix(Piece.Name, TEXT("marker_"))) continue;
        if (Piece.Name == Courtyard.ReplacedBoundaryPiece) continue;
        UStaticMesh* Mesh = Cast<UStaticMesh>(Piece.MeshPath.TryLoad());
        if (!Mesh)
        {
            UE_LOG(LogTemp, Error, TEXT("[Zone] %s vanished between collection and spawn."), *Piece.Name);
            return false;
        }
        // Replace only the noncolliding cone-tree dressing, retaining its
        // authored position and height. Cover/floor/boundary meshes are untouched.
        if (BreakerZoneNameHasPrefix(Piece.Name, TEXT("dress_trees")))
        {
            const FBox Bounds = Mesh->GetBoundingBox();
            const FVector Center = Bounds.GetCenter();
            const FVector Ground(Center.X, Center.Y, Bounds.Min.Z);
            if (BreakerPlaceEnvironmentDressing(World, TEXT("CommonTree_1"), Ground, Bounds.GetSize().Z, 25))
            {
                BreakerPlaceEnvironmentDressing(World, TEXT("Bush_Common"), Ground + FVector(130, 0, 0), 110, 65);
                ++Spawned;
                continue;
            }
        }        // Identity transform is the entire assembly step: the composer already
        // baked world placement into the vertices.
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
        if (!Actor) continue;
        UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Mesh);
        BreakerZoneApplyColor(Component, BreakerZoneColorFor(Piece.Name));
        const bool bDressing = BreakerZoneNameHasPrefix(Piece.Name, TEXT("dress_"));
        if (bDressing) Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetMobility(EComponentMobility::Static);
        Actor->SetActorEnableCollision(!bDressing);
        Actor->SetActorTickEnabled(false);
#if WITH_EDITOR
        Actor->SetActorLabel(FString::Printf(TEXT("Fernhall_%s"), *Piece.Name));
#endif
        ++Spawned;
    }

    BreakerZoneBuildSurfaceDetail(World, Pieces);
    BreakerZoneBuildSkyline(World, Pieces);
    BreakerBuildFernhallPerimeter(World, Pieces);
    // Existing outward scenery must not visually bury the new playable route.
    TArray<FBox> ClearAreas = Courtyard.GroundFootprints;
    FBox Approach(ForceInit);
    Approach += Courtyard.At(-800,-600);
    Approach += Courtyard.At(Courtyard.EntranceFloorStart,600);
    ClearAreas.Add(Approach);
    TSet<AActor*> RemovedScenery;
    TMap<AActor*, AActor*> RoofDependents;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        if (It->Tags.Contains(TEXT("FernhallSkylineDressing")) && It->GetOwner()) RoofDependents.Add(*It, It->GetOwner());
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (It->Tags.Contains(TEXT("FernhallMaintenance"))) continue;
        if (!It->Tags.Contains(TEXT("FernhallSkylineDressing"))
            && !It->Tags.Contains(TEXT("BreakerEnvironmentDressing"))) continue;
        const FBox Bounds = It->GetStaticMeshComponent()->Bounds.GetBox();
        for (const FBox& Ground : ClearAreas)
            if (Bounds.Min.X <= Ground.Max.X && Bounds.Max.X >= Ground.Min.X
                && Bounds.Min.Y <= Ground.Max.Y && Bounds.Max.Y >= Ground.Min.Y)
            { RemovedScenery.Add(*It); It->Destroy(); break; }
    }
    for (const auto& Pair : RoofDependents)
        if (RemovedScenery.Contains(Pair.Value) && IsValid(Pair.Key)) Pair.Key->Destroy();

    // The builder measures its own grammar at assembly so a playtest log shows
    // the same numbers the suite asserts — and shouts if the placed yard has
    // drifted out of band, because the suite only runs when someone runs it.
    const TArray<FBreakerCoverPiece> Cover = BuildCoverPieces(Pieces, OutMarkers);
    const FBreakerCoverFieldParams Params = FernhallFieldParams();
    FString Reason;
    if (!UBreakerCoverLayoutLibrary::IsLayoutLegal(Cover, Params, Reason))
    {
        UE_LOG(LogTemp, Error, TEXT("[Zone] Fernhall yard is grammar-ILLEGAL: %s"), *Reason);
    }
    UE_LOG(LogTemp, Log, TEXT("[Zone] Fernhall yard: %d pieces spawned | %s"),
        Spawned, *UBreakerCoverLayoutLibrary::DescribeCoverField(Cover, Params));
    return true;
}
