#include "Misc/AutomationTest.h"
#include "Characters/BreakerViewmodelRig.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS

// The named-gun fit is pure (BreakerViewmodel::FitNamedWeapon), so its rules
// prove without a component: the longest bound scales to the target length
// (PackFitLengthCm's, proved by EveryNamedGunWearsThePackScale below), the
// bounds origin cancels at the fitted scale THROUGH the source-axis
// rotation, and degenerate bounds refuse at identity.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNamedWeaponFitTest,
    "RiorsEdge.Weapons.NamedGun.FitPreservesSilhouetteLength",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNamedWeaponFitTest::RunTest(const FString& Parameters)
{
    float Scale; FVector Location;
    // A gun authored 1 m long (longest half-extent 50) fitting a 75 cm layout.
    BreakerViewmodel::FitNamedWeapon(FVector::ZeroVector, FVector(50.0, 6.0, 12.0),
        75.0f, FVector(30.0, 0.0, 0.0), FQuat::Identity, Scale, Location);
    TestEqual(TEXT("longest bound scales to the layout length"), Scale, 0.75f, 1e-4f);
    TestEqual(TEXT("centred at the requested rig point"), Location, FVector(30.0, 0.0, 0.0), 1e-3f);
    // An off-origin pivot cancels at the fitted scale.
    BreakerViewmodel::FitNamedWeapon(FVector(40.0, -8.0, 4.0), FVector(50.0, 6.0, 12.0),
        75.0f, FVector(30.0, 0.0, 0.0), FQuat::Identity, Scale, Location);
    TestEqual(TEXT("offset cancels scaled"), Location, FVector(0.0, 6.0, -3.0), 1e-3f);
    // The same pivot under the pack's 180° yaw: X and Y flip before the
    // location lands, so the cancel flips with them. This is the case the
    // first fit got wrong — it cancelled unrotated and would have landed
    // this gun 60 cm from where it said.
    BreakerViewmodel::FitNamedWeapon(FVector(40.0, -8.0, 4.0), FVector(50.0, 6.0, 12.0),
        75.0f, FVector(30.0, 0.0, 0.0), FRotator(0.0f, 180.0f, 0.0f).Quaternion(), Scale, Location);
    TestEqual(TEXT("offset cancels through the yaw"), Location, FVector(60.0, -6.0, -3.0), 1e-3f);
    // Degenerate bounds refuse the fit.
    BreakerViewmodel::FitNamedWeapon(FVector::ZeroVector, FVector::ZeroVector,
        75.0f, FVector(30.0, 0.0, 0.0), FQuat::Identity, Scale, Location);
    TestEqual(TEXT("degenerate keeps identity scale"), Scale, 1.0f, 1e-6f);
    return true;
}

// The muzzle-axis read is pure too: a synthetic gun — a fat block at one end
// of the long axis, a thin bar at the other — must point at the bar, on
// whichever axis and in whichever direction it was authored, and a shape
// with no thin end must refuse rather than guess.

namespace
{
    // Eight corners of a box, appended to Out.
    void BreakerGunAxisBox(TArray<FVector3f>& Out, const FVector3f& Centre, const FVector3f& HalfSize)
    {
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            Out.Add(Centre + FVector3f(
                (Corner & 1) ? HalfSize.X : -HalfSize.X,
                (Corner & 2) ? HalfSize.Y : -HalfSize.Y,
                (Corner & 4) ? HalfSize.Z : -HalfSize.Z));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNamedWeaponMuzzleAxisTest,
    "RiorsEdge.Weapons.NamedGun.MuzzleAxisReadsTheThinEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNamedWeaponMuzzleAxisTest::RunTest(const FString& Parameters)
{
    // Receiver-and-stock block from x=-40 to 0, barrel from 0 to +40.
    TArray<FVector3f> Forward;
    BreakerGunAxisBox(Forward, FVector3f(-20.0f, 0.0f, 0.0f), FVector3f(20.0f, 3.0f, 9.0f));
    BreakerGunAxisBox(Forward, FVector3f(20.0f, 0.0f, 0.0f), FVector3f(20.0f, 1.5f, 1.5f));
    TestEqual(TEXT("barrel toward +X reads +X"),
        BreakerViewmodel::MuzzleAxisFromVertices(Forward), FVector(1.0, 0.0, 0.0), 1e-6f);

    // The same gun facing the camera: the pack's import convention.
    TArray<FVector3f> Backward;
    for (const FVector3f& P : Forward) Backward.Add(FVector3f(-P.X, P.Y, P.Z));
    TestEqual(TEXT("barrel toward -X reads -X"),
        BreakerViewmodel::MuzzleAxisFromVertices(Backward), FVector(-1.0, 0.0, 0.0), 1e-6f);

    // Authored down -Y instead, as a side-facing pack would be.
    TArray<FVector3f> Sideways;
    for (const FVector3f& P : Forward) Sideways.Add(FVector3f(P.Y, -P.X, P.Z));
    TestEqual(TEXT("barrel toward -Y reads -Y"),
        BreakerViewmodel::MuzzleAxisFromVertices(Sideways), FVector(0.0, -1.0, 0.0), 1e-6f);

    // A cube has no thin end; a handful of points is not a mesh.
    TArray<FVector3f> Cube;
    BreakerGunAxisBox(Cube, FVector3f::ZeroVector, FVector3f(10.0f));
    TestEqual(TEXT("a cube refuses"), BreakerViewmodel::MuzzleAxisFromVertices(Cube), FVector::ZeroVector, 1e-6f);
    TestEqual(TEXT("too few points refuse"),
        BreakerViewmodel::MuzzleAxisFromVertices(TArrayView<const FVector3f>(Cube.GetData(), 2)),
        FVector::ZeroVector, 1e-6f);
    return true;
}

// Every named gun the layout table authors must LOAD, or the viewmodel
// silently restores the primitives and the swap looks like it never landed —
// a renamed or deleted uasset should fail the suite, not the screenshot.
// Gated on the imported weapons directory existing, the shipped-samples
// test's shape: a clean clone without Content still passes, because there the
// primitive fallback is the design, not a defect.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNamedWeaponResolveTest,
    "RiorsEdge.Weapons.NamedGun.EveryAuthoredGunResolves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNamedWeaponResolveTest::RunTest(const FString& Parameters)
{
    const FString WeaponsDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/weapons/sci-fi");
    if (!IFileManager::Get().DirectoryExists(*WeaponsDir))
    {
        return true;
    }
    int32 Named = 0;
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
    {
        const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        if (!Layout.NamedMeshPath.IsValid()) continue;
        ++Named;
        TestNotNull(*FString::Printf(TEXT("archetype %d's named gun loads: %s"),
                Index, *Layout.NamedMeshPath.ToString()),
            Layout.NamedMeshPath.TryLoad());
    }
    // Every archetype carries a named gun. One fewer is a lost mapping.
    TestEqual(TEXT("every archetype carries a named gun"), Named,
        static_cast<int32>(EBreakerWeaponArchetype::Count));
    return true;
}

// Every named gun, once the layout's source-axis rotation is applied, must
// point its muzzle down rig +X. The rotation was back-solved from ONE
// photograph; this reads the geometry instead, so a re-import under a
// different axis setting, or a pack whose pistol faces the other way, fails
// the suite instead of aiming at the player's face in the next capture.
// Same gate as the resolve test. The bounds are logged per gun so the fit's
// pivot cancel is a measured number, not an assumption.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNamedWeaponFacesForwardTest,
    "RiorsEdge.Weapons.NamedGun.EveryNamedGunFacesForward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNamedWeaponFacesForwardTest::RunTest(const FString& Parameters)
{
    const FString WeaponsDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/weapons/sci-fi");
    if (!IFileManager::Get().DirectoryExists(*WeaponsDir))
    {
        return true;
    }
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
    {
        const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        if (!Layout.NamedMeshPath.IsValid()) continue;
        const FString Name = Layout.NamedMeshPath.GetAssetName();
        UStaticMesh* Mesh = Cast<UStaticMesh>(Layout.NamedMeshPath.TryLoad());
        if (!Mesh) continue; // the resolve test owns that failure

        const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
        if (!TestTrue(*FString::Printf(TEXT("%s has render data"), *Name),
                RenderData && RenderData->LODResources.Num() > 0))
        {
            continue;
        }
        const FPositionVertexBuffer& Buffer = RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
        const uint32 Count = Buffer.GetNumVertices();
        if (!TestTrue(*FString::Printf(TEXT("%s keeps a CPU vertex copy"), *Name), Count > 0))
        {
            continue;
        }
        TArray<FVector3f> Positions;
        Positions.Reserve(Count);
        for (uint32 V = 0; V < Count; ++V) Positions.Add(Buffer.VertexPosition(V));

        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FVector MeshAxis = BreakerViewmodel::MuzzleAxisFromVertices(Positions);
        const FVector RigAxis = Layout.NamedMeshRotation.RotateVector(MeshAxis);
        const FVector RigExtent = Layout.NamedMeshRotation.RotateVector(Bounds.BoxExtent).GetAbs();
        UE_LOG(LogTemp, Display,
            TEXT("[NamedGun] %s: %u verts, bounds origin (%.1f, %.1f, %.1f) extent (%.1f, %.1f, %.1f), muzzle mesh-axis (%.0f, %.0f, %.0f) -> rig (%.0f, %.0f, %.0f)"),
            *Name, Count, Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z,
            Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z,
            MeshAxis.X, MeshAxis.Y, MeshAxis.Z, RigAxis.X, RigAxis.Y, RigAxis.Z);

        TestTrue(*FString::Printf(TEXT("%s has a readable thin end"), *Name), !MeshAxis.IsZero());
        TestTrue(*FString::Printf(TEXT("%s's longest bound lies along rig X after the layout rotation"), *Name),
            RigExtent.X >= RigExtent.Y && RigExtent.X >= RigExtent.Z);
        TestTrue(*FString::Printf(TEXT("%s's muzzle faces rig +X after the layout rotation (got %.2f)"),
                *Name, RigAxis.X),
            RigAxis.X > 0.9);
    }
    return true;
}

// THE PACK-SCALE LAW. Every named gun wears the one scale the Rifle row gives
// Gun_Rifle, and the pack's own proportions carry the order. The rule this
// replaces pinned each mesh to its own row's length: the pack's sniper is
// 1.62x its rifle, the rows are 1.2x, so the sniper drew at 0.559 against the
// rifle's 0.755 — half the width, two-thirds the height (24.9 cm against
// 28.2), a rod. The fit is computed here exactly as the character computes
// it. Same gate as the resolve test; the pure pin below runs without Content.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNamedWeaponPackScaleTest,
    "RiorsEdge.Weapons.NamedGun.EveryNamedGunWearsThePackScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNamedWeaponPackScaleTest::RunTest(const FString& Parameters)
{
    // Pure pin with the measured numbers: the sniper's 85.9 half-extent at
    // the rifle's 80 cm over 53.0 fits to 129.7; the rifle fits to its row.
    TestEqual(TEXT("sniper keeps the pack's 1.62x at the rifle's scale"),
        BreakerViewmodel::PackFitLengthCm(85.9f, 53.0f, 80.0f), 129.7f, 0.1f);
    TestEqual(TEXT("the reference fits to its own row"),
        BreakerViewmodel::PackFitLengthCm(53.0f, 53.0f, 80.0f), 80.0f, 1e-4f);
    TestEqual(TEXT("a degenerate reference keeps the row length"),
        BreakerViewmodel::PackFitLengthCm(85.9f, 0.0f, 80.0f), 80.0f, 1e-4f);
    TestEqual(TEXT("the bounds front is the -X face at the bounds centre"),
        BreakerViewmodel::PackMuzzleFrontMeshCm(FVector(4.0, -1.0, 6.0), FVector(85.9, 3.0, 16.5)),
        FVector(-81.9, -1.0, 6.0), 1e-4f);

    const FString WeaponsDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/weapons/sci-fi");
    if (!IFileManager::Get().DirectoryExists(*WeaponsDir))
    {
        return true;
    }

    struct FFitted
    {
        FString Name;
        float Scale = 0.0f;
        float DrawnLengthCm = 0.0f;
        float DrawnHeightCm = 0.0f;
        FVector MuzzleMeshCm = FVector::ZeroVector;
        FVector BoundsFrontCm = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
    };
    const FBreakerViewmodelLayout RifleRow = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle);
    UStaticMesh* Reference = Cast<UStaticMesh>(RifleRow.NamedMeshPath.TryLoad());
    if (!TestNotNull(TEXT("Gun_Rifle, the pack's reference, loads"), Reference)) return false;
    const float ReferenceHalf = static_cast<float>(Reference->GetBounds().BoxExtent.GetMax());

    TArray<FFitted> Fitted;
    for (const EBreakerWeaponArchetype Archetype :
        {EBreakerWeaponArchetype::Sidearm, EBreakerWeaponArchetype::Rifle, EBreakerWeaponArchetype::Sniper})
    {
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        FFitted& F = Fitted.AddDefaulted_GetRef();
        F.Name = Layout.NamedMeshPath.GetAssetName();
        F.Rotation = Layout.NamedMeshRotation;
        UStaticMesh* Mesh = Cast<UStaticMesh>(Layout.NamedMeshPath.TryLoad());
        if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *F.Name), Mesh)) return false;
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        // The character's computation, step for step.
        const float TargetLengthCm = BreakerViewmodel::PackFitLengthCm(
            static_cast<float>(Bounds.BoxExtent.GetMax()), ReferenceHalf, RifleRow.OverallLengthCm());
        FVector Location;
        BreakerViewmodel::FitNamedWeapon(Bounds.Origin, Bounds.BoxExtent, TargetLengthCm,
            FVector(Layout.MuzzleCm.X * 0.5f, 0.0f, -4.0f), Layout.NamedMeshRotation.Quaternion(),
            F.Scale, Location);
        const FVector RigExtent = Layout.NamedMeshRotation.RotateVector(Bounds.BoxExtent).GetAbs();
        F.DrawnLengthCm = 2.0f * static_cast<float>(Bounds.BoxExtent.GetMax()) * F.Scale;
        F.DrawnHeightCm = 2.0f * static_cast<float>(RigExtent.Z) * F.Scale;
        F.BoundsFrontCm = BreakerViewmodel::PackMuzzleFrontMeshCm(Bounds.Origin, Bounds.BoxExtent);
        F.MuzzleMeshCm = (F.Name == TEXT("Gun_Rifle"))
            ? BreakerViewmodel::RifleMuzzleMeshCm : F.BoundsFrontCm;
        UE_LOG(LogTemp, Display,
            TEXT("[NamedGun] %s wears scale %.3f: drawn %.1f cm long, %.1f cm high, muzzle mesh (%.1f, %.1f, %.1f)"),
            *F.Name, F.Scale, F.DrawnLengthCm, F.DrawnHeightCm,
            F.MuzzleMeshCm.X, F.MuzzleMeshCm.Y, F.MuzzleMeshCm.Z);
    }
    const FFitted& Pistol = Fitted[0];
    const FFitted& Rifle = Fitted[1];
    const FFitted& Sniper = Fitted[2];

    // One scale for the pack.
    TestEqual(TEXT("the reference wears its row's scale"),
        Rifle.Scale, RifleRow.OverallLengthCm() / (2.0f * ReferenceHalf), 1e-3f);
    TestEqual(TEXT("the pistol wears the rifle's scale"), Pistol.Scale, Rifle.Scale, 1e-3f);
    TestEqual(TEXT("the sniper wears the rifle's scale"), Sniper.Scale, Rifle.Scale, 1e-3f);

    // The pack's proportions carry the order.
    TestTrue(*FString::Printf(TEXT("pistol (%.1f) draws shorter than rifle (%.1f)"), Pistol.DrawnLengthCm, Rifle.DrawnLengthCm),
        Pistol.DrawnLengthCm < Rifle.DrawnLengthCm);
    TestTrue(*FString::Printf(TEXT("rifle (%.1f) draws shorter than sniper (%.1f)"), Rifle.DrawnLengthCm, Sniper.DrawnLengthCm),
        Rifle.DrawnLengthCm < Sniper.DrawnLengthCm);
    // The sniper is not a rod: its drawn height stands against the rifle's.
    // The row-length rule gave 0.65 here; the pack gives 0.88.
    TestTrue(*FString::Printf(TEXT("sniper height (%.1f) is at least 0.8x rifle height (%.1f)"), Sniper.DrawnHeightCm, Rifle.DrawnHeightCm),
        Sniper.DrawnHeightCm >= 0.8f * Rifle.DrawnHeightCm);

    // The flash: the rifle keeps its measured cap, the others take the front.
    TestEqual(TEXT("rifle muzzle is the measured cap"), Rifle.MuzzleMeshCm, BreakerViewmodel::RifleMuzzleMeshCm, 1e-4f);
    TestEqual(TEXT("pistol muzzle is its bounds front"), Pistol.MuzzleMeshCm, Pistol.BoundsFrontCm, 1e-4f);
    TestEqual(TEXT("sniper muzzle is its bounds front"), Sniper.MuzzleMeshCm, Sniper.BoundsFrontCm, 1e-4f);
    // The front lands ahead of the hand once the layout's yaw is applied.
    for (const FFitted& F : {Pistol, Sniper})
    {
        const FVector RigMuzzle = F.Rotation.RotateVector(F.MuzzleMeshCm * F.Scale);
        TestTrue(*FString::Printf(TEXT("%s's muzzle sits forward of the hand in rig space (%.1f)"), *F.Name, RigMuzzle.X),
            RigMuzzle.X > 0.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerRifleMuzzleGeometryTest,
    "RiorsEdge.Weapons.NamedGun.RifleMuzzleMatchesGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerRifleMuzzleGeometryTest::RunTest(const FString& Parameters)
{
    const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle);
    UStaticMesh* Mesh = Cast<UStaticMesh>(Layout.NamedMeshPath.TryLoad());
    if (!TestNotNull(TEXT("shipped rifle"), Mesh)) return false;
    const FStaticMeshRenderData* Render = Mesh->GetRenderData();
    if (!TestTrue(TEXT("rifle LOD0 geometry available to editor test"), Render && Render->LODResources.Num() > 0)) return false;
    const FPositionVertexBuffer& Vertices = Render->LODResources[0].VertexBuffers.PositionVertexBuffer;
    const FBox Bounds = Mesh->GetBoundingBox();
    const double CapEnd = Bounds.Min.X + Bounds.GetSize().X * 0.02;
    FVector Sum = FVector::ZeroVector;
    int32 Count = 0;
    for (uint32 Index = 0; Index < Vertices.GetNumVertices(); ++Index)
    {
        const FVector Position(Vertices.VertexPosition(Index));
        if (Position.X <= CapEnd) { Sum += Position; ++Count; }
    }
    if (!TestTrue(TEXT("actual front cap contains vertices"), Count > 0)) return false;
    TestTrue(TEXT("packaged muzzle datum still matches shipped geometry after reimport"),
        (Sum / Count).Equals(BreakerViewmodel::RifleMuzzleMeshCm, 0.01));
    return true;
}

#endif
