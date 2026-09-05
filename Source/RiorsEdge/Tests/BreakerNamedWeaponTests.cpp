#include "Misc/AutomationTest.h"
#include "Characters/BreakerViewmodelRig.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS

// The named-gun fit is pure (BreakerViewmodel::FitNamedWeapon), so its rules
// prove without a component: the longest bound scales to the layout's overall
// length — which is how the silhouette-ordering law survives the swap from
// primitives to intake meshes — the bounds origin cancels at the fitted
// scale THROUGH the source-axis rotation, and degenerate bounds refuse at
// identity.

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
    const FString WeaponsDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/weapons/gun-pack");
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
    // Six named, two deliberately primitive (Shotgun, Rocket — no vendored
    // candidate). A seventh named gun is fine; a fifth is a lost mapping.
    TestTrue(TEXT("at least six archetypes carry a named gun"), Named >= 6);
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
    const FString WeaponsDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/weapons/gun-pack");
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

#endif
