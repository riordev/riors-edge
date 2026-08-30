#include "Misc/AutomationTest.h"
#include "Characters/BreakerViewmodelRig.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// The named-gun fit is pure (BreakerViewmodel::FitNamedWeapon), so its rules
// prove without a component: the longest bound scales to the layout's overall
// length — which is how the silhouette-ordering law survives the swap from
// primitives to intake meshes — the bounds origin cancels at the fitted
// scale, and degenerate bounds refuse at identity.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNamedWeaponFitTest,
    "RiorsEdge.Weapons.NamedGun.FitPreservesSilhouetteLength",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNamedWeaponFitTest::RunTest(const FString& Parameters)
{
    float Scale; FVector Location;
    // A gun authored 1 m long (longest half-extent 50) fitting a 75 cm layout.
    BreakerViewmodel::FitNamedWeapon(FVector::ZeroVector, FVector(50.0, 6.0, 12.0),
        75.0f, FVector(30.0, 0.0, 0.0), Scale, Location);
    TestEqual(TEXT("longest bound scales to the layout length"), Scale, 0.75f, 1e-4f);
    TestEqual(TEXT("centred at the requested rig point"), Location, FVector(30.0, 0.0, 0.0), 1e-3);
    // An off-origin pivot cancels at the fitted scale.
    BreakerViewmodel::FitNamedWeapon(FVector(40.0, -8.0, 4.0), FVector(50.0, 6.0, 12.0),
        75.0f, FVector(30.0, 0.0, 0.0), Scale, Location);
    TestEqual(TEXT("offset cancels scaled"), Location, FVector(0.0, 6.0, -3.0), 1e-3);
    // Degenerate bounds refuse the fit.
    BreakerViewmodel::FitNamedWeapon(FVector::ZeroVector, FVector::ZeroVector,
        75.0f, FVector(30.0, 0.0, 0.0), Scale, Location);
    TestEqual(TEXT("degenerate keeps identity scale"), Scale, 1.0f, 1e-6f);
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

#endif
