#include "Game/BreakerZoneBuilder.h"

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
        for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
        {
            if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
            {
                Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
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
        if (Name == TEXT("flr_riftpad")) return BreakerZoneOffWhite;
        if (BreakerZoneNameHasPrefix(Name, TEXT("wall_"))) return BreakerZoneConcrete;
        if (BreakerZoneNameHasPrefix(Name, TEXT("blk_full_"))) return BreakerZoneStone;
        if (BreakerZoneNameHasPrefix(Name, TEXT("blk_chest_"))) return BreakerZoneRust;
        if (BreakerZoneNameHasPrefix(Name, TEXT("flr_"))) return BreakerZoneEarth;
        return BreakerZoneMoss;
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

TArray<FBreakerCoverPiece> UBreakerZoneBuilder::BuildCoverPieces(const TArray<FBreakerZonePiece>& Pieces,
    const FBreakerZoneMarkers& Markers)
{
    TArray<FBreakerCoverPiece> Cover;
    // Field frame: origin at the player start, forward toward the rift. The
    // frame is DERIVED from the markers rather than assumed to be +X, so a
    // re-export that rotates the yard moves the frame with it and the grammar
    // numbers stay true.
    // THE ENTRY YARD'S frame, explicitly. With one yard this is the zone's
    // frame; with several it is the arrival yard's, and per-yard frames are
    // the grammar split's job (Q3), not this one's. Named here so the
    // single-yard assumption is visible rather than inherited.
    const FBreakerZoneMarker* StartMarker = Markers.Find(EBreakerZoneMarkerRole::PlayerStart);
    if (!StartMarker) return Cover;
    const FBreakerZoneMarker* RiftMarker = Markers.Find(EBreakerZoneMarkerRole::Rift);

    const FVector2D Start(StartMarker->Location.X, StartMarker->Location.Y);
    // A yard with no rift door keeps a frame: forward falls back to +X, which
    // is what the Normalize guard below already did for a degenerate pair.
    FVector2D Forward = RiftMarker
        ? FVector2D(RiftMarker->Location.X, RiftMarker->Location.Y) - Start
        : FVector2D::ZeroVector;
    if (!Forward.Normalize()) Forward = FVector2D(1.0f, 0.0f);
    const FVector2D Right(-Forward.Y, Forward.X);

    int32 ClusterIndex = 0;
    for (const FBreakerZonePiece& Piece : Pieces)
    {
        const bool bFull = BreakerZoneNameHasPrefix(Piece.Name, TEXT("blk_full_"));
        const bool bChest = BreakerZoneNameHasPrefix(Piece.Name, TEXT("blk_chest_"));
        if (!bFull && !bChest) continue;
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

FBreakerCoverFieldParams UBreakerZoneBuilder::FernhallFieldParams()
{
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

bool UBreakerZoneBuilder::BuildFernhallYard(UWorld* World, FBreakerZoneMarkers& OutMarkers)
{
    if (!World) return false;

    TArray<FBreakerZonePiece> Pieces;
    if (!CollectZonePieces(FernhallMeshFolder(), Pieces)) return false;
    if (!ExtractMarkers(Pieces, OutMarkers)) return false;

    int32 Spawned = 0;
    for (const FBreakerZonePiece& Piece : Pieces)
    {
        if (BreakerZoneNameHasPrefix(Piece.Name, TEXT("marker_"))) continue;
        UStaticMesh* Mesh = Cast<UStaticMesh>(Piece.MeshPath.TryLoad());
        if (!Mesh)
        {
            UE_LOG(LogTemp, Error, TEXT("[Zone] %s vanished between collection and spawn."), *Piece.Name);
            return false;
        }
        // Identity transform is the entire assembly step: the composer already
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
