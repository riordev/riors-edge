#include "Data/BreakerArmsCommandlet.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Characters/BreakerViewmodelRig.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "LODUtilities.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    constexpr const TCHAR* BreakerArmsSourcePath =
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
    constexpr const TCHAR* BreakerArmsOutputPackage = TEXT("/Game/Breaker/Characters/SKM_FirstPersonArms");
    constexpr const TCHAR* BreakerArmsOutputPath =
        TEXT("/Game/Breaker/Characters/SKM_FirstPersonArms.SKM_FirstPersonArms");
    // O2 geometry cut: blended shoulder edges end behind the camera. Both the
    // dominant influence and >=90% aggregate weight must belong to ONE arm.
    constexpr float BreakerArmsMinimumArmWeight = .90f;
    TArray<int32> BreakerArmsArmSides(const FReferenceSkeleton& Ref)
    {
        TArray<int32> Result;
        Result.Init(0, Ref.GetRawBoneNum());
        const int32 Left = Ref.FindBoneIndex(TEXT("upperarm_l")), Right = Ref.FindBoneIndex(TEXT("upperarm_r"));
        if (Left == INDEX_NONE || Right == INDEX_NONE)
            return {};
        for (int32 Bone = 0; Bone < Result.Num(); ++Bone)
            for (int32 Parent = Bone; Parent != INDEX_NONE; Parent = Ref.GetRawParentIndex(Parent))
                if (Parent == Left || Parent == Right)
                {
                    Result[Bone] = Parent == Left ? 1 : 2;
                    break;
                }
        return Result;
    }
    int32 BreakerArmsVertexSide(const FSkinWeightsVertexAttributesConstRef& Weights, FVertexID Vertex,
                                const TArray<int32>& Sides)
    {
        float Sum[3] = {0, 0, 0};
        float Greatest = -1;
        int32 Dominant = 0;
        for (const auto Weight : Weights.Get(Vertex))
        {
            const int32 Bone = Weight.GetBoneIndex();
            if (!Sides.IsValidIndex(Bone))
                return 0;
            const int32 Side = Sides[Bone];
            Sum[Side] += Weight.GetWeight();
            if (Weight.GetWeight() > Greatest)
            {
                Greatest = Weight.GetWeight();
                Dominant = Side;
            }
        }
        return Dominant != 0 && Sum[Dominant] >= BreakerArmsMinimumArmWeight ? Dominant : 0;
    }
    void BreakerArmsFinish(USkeletalMesh* Mesh)
    {
        USkinnedAsset* Assets[] = {Mesh};
        FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    }
    void BreakerArmsLogPose(USkeletalMesh* Mesh)
    {
        UAnimSequence* Idle = nullptr;
        for (const TCHAR* Name : {TEXT("MF_Rifle_Idle_ADS"), TEXT("MM_Rifle_Fire"), TEXT("MM_Rifle_Reload")})
        {
            const FString Path = FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Rifle/%s.%s"), Name, Name);
            UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, *Path);
            if (!Animation)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ArmsPose] Missing %s"), Name);
                continue;
            }
            UE_LOG(LogTemp, Display, TEXT("[ArmsPose] %s length=%.4f additive=%d compatible=%d"), Name,
                   Animation->GetPlayLength(), static_cast<int32>(Animation->AdditiveAnimType),
                   Mesh->GetSkeleton() && Mesh->GetSkeleton()->IsCompatibleForEditor(Animation->GetSkeleton()));
            if (FString(Name) == TEXT("MF_Rifle_Idle_ADS"))
                Idle = Animation;
        }
        for (FName Name : {FName(TEXT("HandGrip_R")), FName(TEXT("HandGrip_L"))})
            if (const auto* Socket = Mesh->FindSocket(Name))
                UE_LOG(LogTemp, Display, TEXT("[ArmsPose] socket=%s parent=%s location=%s rotation=%s scale=%s"),
                       *Name.ToString(), *Socket->BoneName.ToString(), *Socket->RelativeLocation.ToString(),
                       *Socket->RelativeRotation.ToString(), *Socket->RelativeScale.ToString());
        if (Idle && !Idle->IsValidAdditive())
        {
            UWorld::InitializationValues Init;
            Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            UWorld* World =
                UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World)
                return;
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            ON_SCOPE_EXIT
            {
                World->DestroyWorld(false);
                GEngine->DestroyWorldContext(World);
            };
            AActor* Owner = World->SpawnActor<AActor>();
            USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Owner);
            Owner->AddInstanceComponent(Component);
            Owner->SetRootComponent(Component);
            Component->SetSkeletalMesh(Mesh);
            Component->SetCollisionProfileName(TEXT("NoCollision"));
            Component->RegisterComponent();
            Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Component->PlayAnimation(Idle, false);
            Component->SetPosition(0, false);
            Component->TickAnimation(0, false);
            Component->RefreshBoneTransforms();
            if (!Component->GetSingleNodeInstance() || Component->GetSingleNodeInstance()->GetCurrentAsset() != Idle)
            {
                UE_LOG(LogTemp, Error, TEXT("[ArmsPose] Registered idle evaluation failed; transforms omitted"));
                return;
            }
            for (FName Bone : {FName(TEXT("hand_l")), FName(TEXT("hand_r")), FName(TEXT("ik_hand_gun")),
                               FName(TEXT("weapon_r")), FName(TEXT("weapon_r_muzzle"))})
                if (Component->GetBoneIndex(Bone) != INDEX_NONE)
                    UE_LOG(LogTemp, Display, TEXT("[ArmsPose] idle bone=%s component=%s"), *Bone.ToString(),
                           *Component->GetSocketTransform(Bone, RTS_Component).ToString());
        }
        if (UStaticMesh* Gun =
                LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Breaker/Meshes/weapons/sci-fi/Gun_Rifle.Gun_Rifle")))
        {
            UE_LOG(LogTemp, Display, TEXT("[ArmsPose] Gun_Rifle origin=%s extent=%s"),
                   *Gun->GetBounds().Origin.ToString(), *Gun->GetBounds().BoxExtent.ToString());
            const FStaticMeshRenderData* Render = Gun->GetRenderData();
            if (Render && Render->LODResources.Num() > 0)
            {
                const FPositionVertexBuffer& Buffer = Render->LODResources[0].VertexBuffers.PositionVertexBuffer;
                TArray<FVector3f> Positions;
                Positions.Reserve(Buffer.GetNumVertices());
                for (uint32 Vertex = 0; Vertex < Buffer.GetNumVertices(); ++Vertex)
                {
                    Positions.Add(Buffer.VertexPosition(Vertex));
                }
                const FVector Axis = BreakerViewmodel::MuzzleAxisFromVertices(Positions);
                UE_LOG(LogTemp, Display, TEXT("[ArmsPose] Gun_Rifle vertices=%d measuredMuzzleAxis=%s"),
                       Positions.Num(), *Axis.ToString());
                const FBoxSphereBounds Bounds = Gun->GetBounds();
                const double FrontLimit = Bounds.Origin.X - Bounds.BoxExtent.X + Bounds.BoxExtent.X * 0.04;
                FVector FrontSum = FVector::ZeroVector;
                int32 FrontCount = 0;
                for (const FVector3f& Position : Positions)
                {
                    if (Position.X <= FrontLimit)
                    {
                        FrontSum += FVector(Position);
                        ++FrontCount;
                    }
                }
                if (FrontCount > 0)
                {
                    UE_LOG(LogTemp, Display, TEXT("[ArmsPose] Gun_Rifle minusXFrontCapCentre=%s vertices=%d bAllowCPUAccess=%d"),
                           *(FrontSum / FrontCount).ToString(), FrontCount, Gun->bAllowCPUAccess);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ArmsPose] Gun_Rifle front 2%% cap has no vertices; bAllowCPUAccess=%d"), Gun->bAllowCPUAccess);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("[ArmsPose] Gun_Rifle has no LOD0 render positions for axis measurement"));
            }
        }
    }
    bool BreakerArmsAuditFailure(int32 Line)
    {
        UE_LOG(LogTemp, Error, TEXT("[ArmsAudit] Validation failed at BreakerArmsCommandlet.cpp:%d"), Line);
        return false;
    }
    bool BreakerArmsAudit(USkeletalMesh* Mesh, USkeletalMesh* Source)
    {
        BreakerArmsFinish(Mesh);
        const FMeshDescription* Desc = Mesh->GetMeshDescription(0);
        const auto Sides = BreakerArmsArmSides(Mesh->GetRefSkeleton());
        UE_LOG(LogTemp, Display,
               TEXT("[ArmsAudit] Metadata desc=%d armBones=%d lods=%d skeletonEqual=%d materials=%d/%d bones=%d/%d"),
               Desc != nullptr, Sides.Num(), Mesh->GetLODNum(), Mesh->GetSkeleton() == Source->GetSkeleton(),
               Mesh->GetMaterials().Num(), Source->GetMaterials().Num(), Mesh->GetRefSkeleton().GetRawBoneNum(),
               Source->GetRefSkeleton().GetRawBoneNum());
        if (!Desc || Sides.IsEmpty() || Mesh->GetLODNum() != 1 || Mesh->GetSkeleton() != Source->GetSkeleton() ||
            Mesh->GetMaterials().Num() != Source->GetMaterials().Num() ||
            Mesh->GetRefSkeleton().GetRawBoneNum() != Source->GetRefSkeleton().GetRawBoneNum())
            return BreakerArmsAuditFailure(__LINE__);
        for (int32 Material = 0; Material < Mesh->GetMaterials().Num(); ++Material)
            if (Mesh->GetMaterials()[Material].MaterialInterface != Source->GetMaterials()[Material].MaterialInterface)
                return BreakerArmsAuditFailure(__LINE__);
        const FSkeletalMeshConstAttributes Attributes(*Desc);
        const auto Weights = Attributes.GetVertexSkinWeights();
        int32 Counts[3] = {0, 0, 0};
        for (FVertexID Vertex : Desc->Vertices().GetElementIDs())
        {
            const int32 Side = BreakerArmsVertexSide(Weights, Vertex, Sides);
            if (Side == 0)
            {
                for (const auto Weight : Weights.Get(Vertex))
                    UE_LOG(LogTemp, Display,
                           TEXT("[ArmsAudit] Rejected vertex=%d triangles=%d bone=%u side=%d weight=%.9f"),
                           Vertex.GetValue(), Desc->GetVertexConnectedTriangles(Vertex).Num(),
                           static_cast<uint32>(Weight.GetBoneIndex()),
                           Sides.IsValidIndex(Weight.GetBoneIndex()) ? Sides[Weight.GetBoneIndex()] : -1,
                           Weight.GetWeight());
                UE_LOG(LogTemp, Error, TEXT("[ArmsAudit] Source vertex %d fails arm membership at %s"),
                       Vertex.GetValue(), *Desc->GetVertexPosition(Vertex).ToString());
                return BreakerArmsAuditFailure(__LINE__);
            }
            ++Counts[Side];
        }
        if (Counts[1] == 0 || Counts[2] == 0 || Desc->Triangles().Num() == 0)
            return BreakerArmsAuditFailure(__LINE__);
        const FSkeletalMeshRenderData* Render = Mesh->GetResourceForRendering();
        if (!Render || Render->LODRenderData.Num() != 1)
            return BreakerArmsAuditFailure(__LINE__);
        const auto& LOD = Render->LODRenderData[0];
        int32 RenderCount = 0;
        for (const auto& Section : LOD.RenderSections)
            for (uint32 Vertex = Section.BaseVertexIndex; Vertex < Section.BaseVertexIndex + Section.NumVertices;
                 ++Vertex)
            {
                uint32 Sum[3] = {0, 0, 0}, Total = 0, Greatest = 0;
                int32 Dominant = 0;
                for (uint32 Influence = 0; Influence < LOD.SkinWeightVertexBuffer.GetMaxBoneInfluences(); ++Influence)
                {
                    const uint32 Weight = LOD.SkinWeightVertexBuffer.GetBoneWeight(Vertex, Influence);
                    if (Weight == 0)
                        continue;
                    const uint32 LocalBone = LOD.SkinWeightVertexBuffer.GetBoneIndex(Vertex, Influence);
                    if (!Section.BoneMap.IsValidIndex(LocalBone) || !Sides.IsValidIndex(Section.BoneMap[LocalBone]))
                        return BreakerArmsAuditFailure(__LINE__);
                    const int32 Side = Sides[Section.BoneMap[LocalBone]];
                    Sum[Side] += Weight;
                    Total += Weight;
                    if (Weight > Greatest)
                    {
                        Greatest = Weight;
                        Dominant = Side;
                    }
                }
                // One quantization unit of tolerance for the cooked weight format.
                if (!Total || Dominant == 0 ||
                    static_cast<float>(Sum[Dominant]) + 1 < Total * BreakerArmsMinimumArmWeight)
                {
                    UE_LOG(
                        LogTemp, Error,
                        TEXT("[ArmsAudit] Render membership vertex=%u total=%u left=%u right=%u other=%u dominant=%d"),
                        Vertex, Total, Sum[1], Sum[2], Sum[0], Dominant);
                    return BreakerArmsAuditFailure(__LINE__);
                }
                ++RenderCount;
            }
        if (RenderCount == 0 || RenderCount != static_cast<int32>(LOD.GetNumVertices()))
            return BreakerArmsAuditFailure(__LINE__);
        UE_LOG(
            LogTemp, Display,
            TEXT(
                "[ArmsAudit] PASS left=%d right=%d triangles=%d renderVertices=%d LODs=1 skeleton/materials preserved"),
            Counts[1], Counts[2], Desc->Triangles().Num(), RenderCount);
        return true;
    }
} // namespace
#endif

UBreakerArmsCommandlet::UBreakerArmsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UBreakerArmsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, BreakerArmsSourcePath);
    if (!Source)
        return 1;
    BreakerArmsFinish(Source);
    BreakerArmsLogPose(Source);
    if (FParse::Param(*Params, TEXT("AuditOnly")))
    {
        USkeletalMesh* Existing = LoadObject<USkeletalMesh>(nullptr, BreakerArmsOutputPath);
        if (!Existing || !BreakerArmsAudit(Existing, Source))
            return 1;
        return 0;
    }
    const FMeshDescription* SourceDescription = Source->GetMeshDescription(0);
    const auto Sides = BreakerArmsArmSides(Source->GetRefSkeleton());
    if (!SourceDescription || Sides.IsEmpty())
        return 1;
    // Work on a transient duplicate until every crop/build audit has passed.
    USkeletalMesh* Mesh = DuplicateObject<USkeletalMesh>(Source, GetTransientPackage());
    if (!Mesh)
        return 1;
    Mesh->SetFlags(RF_Transient);
    Mesh->SetLODSettings(nullptr);
    FSkeletalMeshUpdateContext Context;
    Context.SkeletalMesh = Mesh;
    TArray<int32> ExtraLODs;
    for (int32 LOD = 1; LOD < Mesh->GetLODNum(); ++LOD)
        ExtraLODs.Add(LOD);
    if (!ExtraLODs.IsEmpty())
        FLODUtilities::RemoveLODs(Context, ExtraLODs);
    FMeshDescription Desc(*SourceDescription);
    const FSkeletalMeshConstAttributes Attributes(Desc);
    const auto Weights = Attributes.GetVertexSkinWeights();
    TArray<FTriangleID> Remove;
    for (FTriangleID Triangle : Desc.Triangles().GetElementIDs())
    {
        int32 Arm = 0;
        bool Keep = true;
        for (FVertexID Vertex : Desc.GetTriangleVertices(Triangle))
        {
            const int32 Side = BreakerArmsVertexSide(Weights, Vertex, Sides);
            if (Side == 0 || (Arm != 0 && Arm != Side))
            {
                Keep = false;
                break;
            }
            Arm = Side;
        }
        if (!Keep)
            Remove.Add(Triangle);
    }
    if (Remove.IsEmpty() || Remove.Num() == Desc.Triangles().Num())
        return 1;
    const int32 Removed = Remove.Num();
    Desc.DeleteTriangles(Remove);
    // Imported descriptions can already contain disconnected vertices. The
    // triangle deletion helper only cleans vertices orphaned by this deletion.
    // Remove all pre-existing disconnected geometry as well; no retained
    // triangle or its skin weights are changed by this cleanup.
    TArray<FVertexID> Orphans;
    for (FVertexID Vertex : Desc.Vertices().GetElementIDs())
        if (Desc.IsVertexOrphaned(Vertex))
            Orphans.Add(Vertex);
    for (FVertexID Vertex : Orphans)
    {
        // Snapshot each view before deletion mutates the underlying adjacency.
        const TArray<FVertexInstanceID> Instances(Desc.GetVertexVertexInstanceIDs(Vertex));
        for (FVertexInstanceID Instance : Instances)
            Desc.DeleteVertexInstance(Instance);
        const TArray<FEdgeID> Edges(Desc.GetVertexConnectedEdgeIDs(Vertex));
        for (FEdgeID Edge : Edges)
            Desc.DeleteEdge(Edge);
        Desc.DeleteVertex(Vertex);
    }
    UE_LOG(LogTemp, Display, TEXT("[ArmsAudit] Crop removedTriangles=%d disconnectedVertices=%d remainingVertices=%d"),
           Removed, Orphans.Num(), Desc.Vertices().Num());
    for (FVertexID Vertex : Desc.Vertices().GetElementIDs())
    {
        const int32 Side = BreakerArmsVertexSide(Weights, Vertex, Sides);
        if (Side == 0)
        {
            UE_LOG(LogTemp, Error,
                   TEXT("[ArmsAudit] Precommit retained vertex=%d connectedTriangles=%d has invalid arm weights"),
                   Vertex.GetValue(), Desc.GetVertexConnectedTriangles(Vertex).Num());
            return 1;
        }
    }
    FElementIDRemappings Remap;
    Desc.Compact(Remap);
    Mesh->CreateMeshDescription(0, MoveTemp(Desc));
    if (!Mesh->CommitMeshDescription(0))
        return 1;
    Mesh->SetPhysicsAsset(nullptr);
    Mesh->PostEditChange();
    if (!BreakerArmsAudit(Mesh, Source))
    {
        UE_LOG(LogTemp, Error, TEXT("[ArmsAudit] Failed; output not saved"));
        return 1;
    }
    UPackage* Package = CreatePackage(BreakerArmsOutputPackage);
    Package->FullyLoad();
    // Replace only the fixed generated object, after the new asset passed its
    // audit. The mannequin and its source package are never renamed or saved.
    if (auto* Previous = FindObject<USkeletalMesh>(Package, TEXT("SKM_FirstPersonArms")))
    {
        BreakerArmsFinish(Previous);
        if (!Previous->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional))
            return 1;
        Previous->ClearFlags(RF_Public | RF_Standalone);
        Previous->SetFlags(RF_Transient);
    }
    if (!Mesh->Rename(TEXT("SKM_FirstPersonArms"), Package, REN_DontCreateRedirectors | REN_NonTransactional))
        return 1;
    Mesh->ClearFlags(RF_Transient);
    Mesh->SetFlags(RF_Public | RF_Standalone);
    const FString Filename =
        FPackageName::LongPackageNameToFilename(BreakerArmsOutputPackage, FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Mesh, *Filename, Save))
        return 1;
    FAssetRegistryModule::AssetCreated(Mesh);
    UE_LOG(
        LogTemp, Display,
        TEXT("[ArmsAudit] Saved %s; removed %d non-arm triangles. Run -AuditOnly in a fresh process to verify reload."),
        BreakerArmsOutputPath, Removed);
    return 0;
#else
    return 1;
#endif
}
