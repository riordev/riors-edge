#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/BreakerCharacter.h"
#include "Characters/BreakerViewmodelRig.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

// The first-person blockout layout table. It is pure data plus two pure
// transforms, which is exactly why it lives in its own header away from
// ABreakerCharacter: what MUST be provable is that every archetype has a row,
// that the rows are meaningfully different from each other, and that none of
// them breaks the object-chroma law. Nothing here can prove it looks good —
// that is what the screenshot harness is for, and it was used.
//
// Values are frozen under O2, so every assertion below pins an ORDERING or a
// RULE, never a number. Retuning the table must not turn this suite red.

namespace BreakerViewmodelTest
{
    // Prefixed rather than bare: a unity build concatenates translation units
    // and a bare helper name would collide.
    static const EBreakerWeaponArchetype BreakerViewmodelAllArchetypes[] =
    {
        EBreakerWeaponArchetype::Rifle,
        EBreakerWeaponArchetype::SMG,
        EBreakerWeaponArchetype::Sniper,
        EBreakerWeaponArchetype::Shotgun,
        EBreakerWeaponArchetype::Rocket,
        EBreakerWeaponArchetype::BurstRifle,
        EBreakerWeaponArchetype::Machinegun,
        EBreakerWeaponArchetype::Sidearm
    };

    // Reads one float UPROPERTY off an object by name; -1 when missing, which
    // no viewmodel dial ships at, so a rename fails loudly.
    float BreakerViewmodelReadDial(const UObject* Object, const TCHAR* PropertyName)
    {
        if (!Object) return -1.0f;
        const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
        if (!Property) return -1.0f;
        return Property->GetPropertyValue_InContainer(Object);
    }

    // ABreakerCharacter::GetWeaponRestLocation's aimed pose, mirrored: the
    // rig comes forward to AdsForwardCm and its SCALED sight line is
    // cancelled on Y and Z, so that line lands on the camera axis. If the
    // character's formula changes and this does not, the two have diverged
    // and the sights have drifted.
    FVector BreakerViewmodelAimedPose(float AdsForwardCm, const FVector& SightLineRigCm, float ViewmodelScale)
    {
        return FVector(AdsForwardCm, -SightLineRigCm.Y * ViewmodelScale, -SightLineRigCm.Z * ViewmodelScale);
    }

    // A rig-space point in camera space at a rest pose: the rig root sits at
    // the pose, uniformly scaled, unrotated while the recoil spring rests.
    FVector BreakerViewmodelRigToCamera(const FVector& RestPose, const FVector& RigCm, float ViewmodelScale)
    {
        return RestPose + RigCm * ViewmodelScale;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelCoverageTest,
    "RiorsEdge.Characters.ViewmodelCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelCoverageTest::RunTest(const FString& Parameters)
{
    using namespace BreakerViewmodelTest;

    // The bug this whole pass exists to fix: three archetypes were added to the
    // enum and ApplyWeaponPresentation was never extended, so Burst Rifle,
    // Machinegun and Sidearm silently wore the rifle's proportions. A row that
    // is merely PRESENT is not enough — it has to be its own row.
    const FBreakerViewmodelLayout Rifle = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle);

    for (EBreakerWeaponArchetype Archetype : BreakerViewmodelAllArchetypes)
    {
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        const FString Name = BreakerWeaponArchetypeNames::Display(Archetype);

        TestTrue(*FString::Printf(TEXT("%s has proxy parts"), *Name), Layout.Parts.Num() > 0);
        TestTrue(*FString::Printf(TEXT("%s fits the pooled component budget"), *Name),
            Layout.Parts.Num() <= BreakerViewmodel::MaxProxyParts);
        TestTrue(*FString::Printf(TEXT("%s has a sighting line above the rig origin"), *Name),
            Layout.SightHeightCm > 0.0f);
        TestTrue(*FString::Printf(TEXT("%s has a muzzle forward of the grip"), *Name),
            Layout.MuzzleCm.X > 0.0f);
        TestTrue(*FString::Printf(TEXT("%s sits clear of the camera near plane"), *Name),
            Layout.HipOffsetCm.X > 20.0f);

        for (const FBreakerProxyPart& Part : Layout.Parts)
        {
            TestTrue(*FString::Printf(TEXT("%s parts are all used slots"), *Name), Part.IsUsed());
            TestTrue(*FString::Printf(TEXT("%s parts have positive size"), *Name),
                Part.SizeCm.X > 0.0f && Part.SizeCm.Y > 0.0f && Part.SizeCm.Z > 0.0f);
        }

        if (Archetype == EBreakerWeaponArchetype::Rifle) continue;
        TestTrue(*FString::Printf(TEXT("%s is not the rifle wearing a different name"), *Name),
            !FMath::IsNearlyEqual(Layout.OverallLengthCm(), Rifle.OverallLengthCm(), 1.0f) ||
            Layout.Parts.Num() != Rifle.Parts.Num());
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelSilhouetteOrderTest,
    "RiorsEdge.Characters.ViewmodelSilhouetteOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelSilhouetteOrderTest::RunTest(const FString& Parameters)
{
    // The ORDERING is the design claim; the lengths are O2-frozen placeholders.
    // Each of these mirrors a sentence in Art-And-Modelling-Plan.md §5 or a
    // mechanical fact in Weapons/, so a retune that inverts one has broken the
    // read the proxy exists to carry.
    auto Length = [](EBreakerWeaponArchetype A)
    {
        return BreakerViewmodel::ArchetypeLayout(A).OverallLengthCm();
    };

    const float Sidearm = Length(EBreakerWeaponArchetype::Sidearm);
    const float SMG = Length(EBreakerWeaponArchetype::SMG);
    const float Rifle = Length(EBreakerWeaponArchetype::Rifle);
    const float Sniper = Length(EBreakerWeaponArchetype::Sniper);
    const float Machinegun = Length(EBreakerWeaponArchetype::Machinegun);

    // "Longest silhouette by 40%" is the plan's wording; 15% is the floor this
    // pins, so the table can be retuned toward it without going red.
    TestTrue(TEXT("The sniper is the longest weapon in the game"),
        Sniper > Rifle * 1.15f);
    // The sidearm's entire archetype identity is a 0.18 s swap-in, and a proxy
    // that does not read as nothing to bring up is lying about it. The plan's
    // "smaller by a factor of three" is about SCREEN AREA, and length alone
    // cannot carry that without making the pistol unrealistically stubby — a
    // real sidearm is about half a real SMG end to end, and the rest of the
    // difference is carried by cross-section, which is checked below.
    TestTrue(TEXT("The sidearm is far shorter than any long gun"),
        Sidearm * 1.7f < SMG);
    {
        auto LargestCross = [](EBreakerWeaponArchetype A)
        {
            float Largest = 0.0f;
            for (const FBreakerProxyPart& Part : BreakerViewmodel::ArchetypeLayout(A).Parts)
            {
                Largest = FMath::Max(Largest, static_cast<float>(Part.SizeCm.Y * Part.SizeCm.Z));
            }
            return Largest;
        };
        TestTrue(TEXT("The sidearm presents far less frontal area than an SMG"),
            LargestCross(EBreakerWeaponArchetype::Sidearm) * 1.5f < LargestCross(EBreakerWeaponArchetype::SMG));
    }
    // "Shortest long gun, no stock."
    TestTrue(TEXT("The SMG is the shortest long gun"), SMG < Rifle);

    // The machinegun's 120-round magazine is four SMG magazines in one trigger
    // pull and it reloads in 4.2 seconds, so it has to be the heaviest object
    // in the frame by a distance. Summed part volume rather than any single
    // dimension, because "heavy" is mass, not length — the sniper is longer
    // and must NOT read as heavier.
    auto Bulk = [](EBreakerWeaponArchetype A)
    {
        float Total = 0.0f;
        for (const FBreakerProxyPart& Part : BreakerViewmodel::ArchetypeLayout(A).Parts)
        {
            Total += static_cast<float>(Part.SizeCm.X * Part.SizeCm.Y * Part.SizeCm.Z);
        }
        return Total;
    };
    TestTrue(TEXT("The machinegun is visibly the heaviest thing in the game"),
        Bulk(EBreakerWeaponArchetype::Machinegun) > Bulk(EBreakerWeaponArchetype::Rifle) * 2.0f);
    TestTrue(TEXT("The sidearm is the lightest thing in the game"),
        Bulk(EBreakerWeaponArchetype::Machinegun) > Bulk(EBreakerWeaponArchetype::Sidearm) * 8.0f);
    // Longer is not heavier: the sniper outreaches the machinegun and must
    // still read as the thinner weapon.
    TestTrue(TEXT("The sniper is long without being bulky"),
        Bulk(EBreakerWeaponArchetype::Sniper) < Bulk(EBreakerWeaponArchetype::Machinegun) &&
        Sniper > Machinegun);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelSharedArmsTest,
    "RiorsEdge.Characters.ViewmodelSharedArms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelSharedArmsTest::RunTest(const FString& Parameters)
{
    // Every archetype wears the same skeletal arms, and those arms were fitted
    // at ONE hip offset, ONE aim distance and ONE pair of hand points: the
    // arms component scales itself by the hand span and seats itself at the
    // hip, so a row with its own numbers would shrink the arms or push the
    // shoulder cuts in front of the camera. The rows are pinned EQUAL to the
    // rifle's rather than to a value, so the four can be retuned together
    // without turning this red. Every archetype also authors a textured gun
    // from the sci-fi pack: a row without one falls back to grey primitives,
    // which is the bug this rule exists to keep out.
    const FBreakerViewmodelLayout Rifle = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle);
    const FString PackRoot = TEXT("/Game/Breaker/Meshes/weapons/sci-fi/");

    for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
    {
        const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        const FString Name = BreakerWeaponArchetypeNames::Display(Archetype);

        TestTrue(*FString::Printf(TEXT("%s authors a named gun"), *Name), Layout.NamedMeshPath.IsValid());
        TestTrue(*FString::Printf(TEXT("%s's named gun comes from the textured sci-fi pack: %s"),
                *Name, *Layout.NamedMeshPath.ToString()),
            Layout.NamedMeshPath.ToString().StartsWith(PackRoot));

        TestTrue(*FString::Printf(TEXT("%s holds the rig at the rifle's hip offset"), *Name),
            Layout.HipOffsetCm.Equals(Rifle.HipOffsetCm, 0.001f));
        TestEqual(*FString::Printf(TEXT("%s aims at the rifle's distance"), *Name),
            Layout.AdsForwardCm, Rifle.AdsForwardCm, 0.001f);
        TestTrue(*FString::Printf(TEXT("%s puts the support hand where the rifle does"), *Name),
            Layout.SupportHandCm.Equals(Rifle.SupportHandCm, 0.001f));
        TestTrue(*FString::Printf(TEXT("%s puts the firing hand where the rifle does"), *Name),
            Layout.FiringHandCm.Equals(Rifle.FiringHandCm, 0.001f));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelChromaLawTest,
    "RiorsEdge.Characters.ViewmodelChromaLaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelChromaLawTest::RunTest(const FString& Parameters)
{
    using namespace BreakerViewmodelTest;

    // O24 and the object-chroma law: saturated teal is reserved for rift and
    // suppression objects. The player is militia hardware and may never wear
    // it. A cyan-dominant colour is one whose green AND blue both clear red by
    // a wide margin — which is precisely the reserved band, and precisely what
    // no gunmetal, olive polymer or hazard amber can accidentally become.
    for (EBreakerWeaponArchetype Archetype : BreakerViewmodelAllArchetypes)
    {
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        for (const FBreakerProxyPart& Part : Layout.Parts)
        {
            const FLinearColor& C = Part.Color;
            const bool bCyanDominant = (C.G > C.R * 1.5f) && (C.B > C.R * 1.5f);
            TestFalse(*FString::Printf(TEXT("%s carries no reserved teal"),
                *BreakerWeaponArchetypeNames::Display(Archetype)), bCyanDominant);
            TestTrue(TEXT("Blockout colours stay dark enough to read against gym concrete"),
                C.GetLuminance() < 0.25f);
        }
    }

    // The arms are subject to the same law.
    for (const FLinearColor& C : { BreakerViewmodel::GloveOlive, BreakerViewmodel::SleeveSlate })
    {
        TestFalse(TEXT("Arms carry no reserved teal"), (C.G > C.R * 1.5f) && (C.B > C.R * 1.5f));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelTransformTest,
    "RiorsEdge.Characters.ViewmodelTransform",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelTransformTest::RunTest(const FString& Parameters)
{
    // The whole point of authoring in centimetres is that a barrel authored
    // "26 cm long, 3 cm across" comes out 26 cm long down the bore. The engine's
    // cylinder runs along its own local Z, so the builder folds in a pitch that
    // maps local Z onto rig X. Get that backwards and every barrel points at
    // the floor.
    FBreakerProxyPart Barrel;
    Barrel.Shape = EBreakerProxyShape::CylinderX;
    Barrel.SizeCm = FVector(26.0f, 3.0f, 3.0f);

    FVector Scale;
    FRotator Rotation;
    BreakerViewmodel::ResolvePartTransform(Barrel, Scale, Rotation);
    // A cylinder has no front and no back, so the assertion is that its AXIS
    // lies on rig X — not which way along it. Direction only matters for the
    // cone, checked below.
    const FVector Along = Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
    TestTrue(TEXT("An X cylinder lies down the bore"), FMath::Abs(Along.X) > 0.99f);
    TestEqual(TEXT("Its length lands on the mesh's own axis"), static_cast<float>(Scale.Z) * 100.0f, 26.0f, 0.01f);
    TestEqual(TEXT("Its diameter lands on the other two"), static_cast<float>(Scale.X) * 100.0f, 3.0f, 0.01f);

    FBreakerProxyPart Drum;
    Drum.Shape = EBreakerProxyShape::CylinderY;
    Drum.SizeCm = FVector(17.0f, 7.0f, 17.0f);
    BreakerViewmodel::ResolvePartTransform(Drum, Scale, Rotation);
    const FVector DrumAxis = Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
    TestTrue(TEXT("A Y cylinder lies on its side"), FMath::Abs(DrumAxis.Y) > 0.99f);
    TestEqual(TEXT("The drum is as thick as authored"), static_cast<float>(Scale.Z) * 100.0f, 7.0f, 0.01f);

    // The cone is the one shape whose direction is load-bearing: the rocket's
    // muzzle is a FLARE (wide end forward), and getting it backwards turns the
    // launcher into a spear.
    FBreakerProxyPart Flare;
    Flare.Shape = EBreakerProxyShape::ConeX;
    Flare.SizeCm = FVector(9.0f, 14.0f, 14.0f);
    BreakerViewmodel::ResolvePartTransform(Flare, Scale, Rotation);
    const FVector Tip = Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
    TestTrue(TEXT("A cone presents its base forward"), Tip.X < -0.99f);

    FBreakerProxyPart Box;
    Box.Shape = EBreakerProxyShape::Box;
    Box.SizeCm = FVector(34.0f, 5.0f, 7.0f);
    BreakerViewmodel::ResolvePartTransform(Box, Scale, Rotation);
    TestTrue(TEXT("A box takes no intrinsic rotation"), Rotation.IsNearlyZero());
    TestEqual(TEXT("A box is exactly its authored size"), static_cast<float>(Scale.X) * 100.0f, 34.0f, 0.01f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelLimbTest,
    "RiorsEdge.Characters.ViewmodelLimb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelLimbTest::RunTest(const FString& Parameters)
{
    FVector Centre;
    FRotator Rotation;
    float Length = 0.0f;

    const FVector Anchor(0.0f, -20.0f, -30.0f);
    const FVector Hand(40.0f, 0.0f, 0.0f);
    BreakerViewmodel::ResolveLimb(Anchor, Hand, Centre, Rotation, Length);

    TestEqual(TEXT("The limb spans anchor to hand"), Length, static_cast<float>((Hand - Anchor).Size()), 0.01f);
    TestTrue(TEXT("The limb is centred between them"), Centre.Equals((Anchor + Hand) * 0.5f, 0.01f));
    const FVector Aim = Rotation.RotateVector(FVector::ForwardVector);
    TestTrue(TEXT("The limb points at the hand"), Aim.Equals((Hand - Anchor).GetSafeNormal(), 0.001f));

    // The degenerate case is kept explicit because a NaN rotation renders as a
    // single corrupt triangle across the whole screen rather than as nothing.
    BreakerViewmodel::ResolveLimb(Hand, Hand, Centre, Rotation, Length);
    TestEqual(TEXT("A zero-length limb has zero length"), Length, 0.0f, 0.0001f);
    TestTrue(TEXT("A zero-length limb has a finite rotation"), !Rotation.ContainsNaN());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelAimPoseTest,
    "RiorsEdge.Characters.ViewmodelAimPose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelAimPoseTest::RunTest(const FString& Parameters)
{
    using namespace BreakerViewmodelTest;

    // ADS is DERIVED, not authored: the rig comes forward and drops by the
    // sight line of the gun it is actually holding. For a named gun that line
    // is the fitted mesh's bounds top-centre in rig space
    // (NamedSightLineRigCm); for the primitive fallback it is the row's
    // SightHeightCm over the rig origin. Either way the SAME aimed pose must
    // land it on the camera axis. The old assertion here compared
    // -SightHeightCm + SightHeightCm with zero, which no fit could fail.
    const float ShippedScale = BreakerViewmodelReadDial(GetDefault<ABreakerCharacter>(), TEXT("ViewmodelScale"));
    TestEqual(TEXT("ViewmodelScale ships at 0.9"), ShippedScale, 0.9f, 0.0001f);
    const float OnAxisToleranceCm = 0.5f;

    // --- Pure sibling: a synthetic box under the shipped fit -----------------
    // A 1 m gun (longest half-extent 50) pivoted off its bounds centre, worn
    // the way the Rifle row wears Gun_Rifle: fitted to the row's length with
    // itself as the pack reference, seated at the firing hand, under the
    // pack's 180-degree yaw. The sight line is the bounds top-centre carried
    // through that fit, hand-computed here so the function's claim is pinned
    // to a number rather than to itself.
    {
        const FBreakerViewmodelLayout RifleRow = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle);
        const FVector Origin(40.0, -8.0, 4.0);
        const FVector Extent(50.0, 6.0, 12.0);
        const float TargetLengthCm = BreakerViewmodel::PackFitLengthCm(
            static_cast<float>(Extent.GetMax()), static_cast<float>(Extent.GetMax()), RifleRow.OverallLengthCm());
        float FitScale; FVector FitLocation;
        BreakerViewmodel::FitNamedWeapon(Origin, Extent, TargetLengthCm,
            FVector(RifleRow.MuzzleCm.X * 0.5f, 0.0f, -4.0f), RifleRow.NamedMeshRotation.Quaternion(),
            FitScale, FitLocation);
        FitLocation = RifleRow.FiringHandCm;   // the character's fitted grip offset
        TestEqual(TEXT("the synthetic box wears the row's scale"), FitScale, RifleRow.OverallLengthCm() / 100.0f, 1e-4f);

        const FVector SightLine = BreakerViewmodel::NamedSightLineRigCm(Origin, Extent, FitScale, RifleRow.NamedMeshRotation, FitLocation);
        const FVector TopCentreMesh = Origin + FVector(0.0, 0.0, Extent.Z);
        const FVector Expected = FitLocation + RifleRow.NamedMeshRotation.RotateVector(TopCentreMesh * FitScale);
        TestEqual(TEXT("the sight line is the bounds top-centre carried through the fit"), SightLine, Expected, 1e-3f);
        TestTrue(TEXT("the synthetic sight line sits above the firing hand"), SightLine.Z > FitLocation.Z);

        const FVector Aimed = BreakerViewmodelAimedPose(RifleRow.AdsForwardCm, SightLine, ShippedScale);
        const FVector Camera = BreakerViewmodelRigToCamera(Aimed, SightLine, ShippedScale);
        TestTrue(*FString::Printf(TEXT("the synthetic sight line lands on the camera axis when aimed (Y %.3f, Z %.3f)"), Camera.Y, Camera.Z),
            FMath::Abs(Camera.Y) <= OnAxisToleranceCm && FMath::Abs(Camera.Z) <= OnAxisToleranceCm);
    }

    // --- Every archetype, as the character fits it --------------------------
    // Gated on the imported weapons directory the way EveryAuthoredGunResolves
    // is: without Content every row takes the primitive fallback, which is the
    // design there, not a defect.
    const FString WeaponsDir = FPaths::ProjectContentDir() / TEXT("Breaker/Meshes/weapons/sci-fi");
    const bool bHaveNamedGuns = IFileManager::Get().DirectoryExists(*WeaponsDir);
    const FBreakerViewmodelLayout RifleRow = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle);
    const UStaticMesh* Reference = (bHaveNamedGuns && RifleRow.NamedMeshPath.IsValid())
        ? Cast<UStaticMesh>(RifleRow.NamedMeshPath.TryLoad()) : nullptr;

    for (EBreakerWeaponArchetype Archetype : BreakerViewmodelAllArchetypes)
    {
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        const FString Name = BreakerWeaponArchetypeNames::Display(Archetype);
        UStaticMesh* Mesh = (bHaveNamedGuns && Layout.NamedMeshPath.IsValid())
            ? Cast<UStaticMesh>(Layout.NamedMeshPath.TryLoad()) : nullptr;

        FVector SightLine;
        if (Mesh)
        {
            // ABreakerCharacter::RebuildViewmodelParts, step for step.
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const float TargetLengthCm = Reference
                ? BreakerViewmodel::PackFitLengthCm(
                    static_cast<float>(Bounds.BoxExtent.GetMax()),
                    static_cast<float>(Reference->GetBounds().BoxExtent.GetMax()),
                    RifleRow.OverallLengthCm())
                : Layout.OverallLengthCm();
            float FitScale; FVector FitLocation;
            BreakerViewmodel::FitNamedWeapon(Bounds.Origin, Bounds.BoxExtent, TargetLengthCm,
                FVector(Layout.MuzzleCm.X * 0.5f, 0.0f, -4.0f), Layout.NamedMeshRotation.Quaternion(),
                FitScale, FitLocation);
            FitLocation = Layout.FiringHandCm;
            SightLine = BreakerViewmodel::NamedSightLineRigCm(Bounds.Origin, Bounds.BoxExtent, FitScale, Layout.NamedMeshRotation, FitLocation);
            UE_LOG(LogTemp, Display, TEXT("[NamedGun] %s (%s) scale %.3f: sight line rig (%.2f, %.2f, %.2f)"),
                *Name, *Layout.NamedMeshPath.GetAssetName(), FitScale, SightLine.X, SightLine.Y, SightLine.Z);
            TestTrue(*FString::Printf(TEXT("%s's named sight line is finite"), *Name), !SightLine.ContainsNaN());
            TestTrue(*FString::Printf(TEXT("%s's named sight line sits above the firing hand"), *Name),
                SightLine.Z > Layout.FiringHandCm.Z);
        }
        else
        {
            // The primitive fallback: the row's own sighting line over the
            // rig origin.
            SightLine = FVector(0.0, 0.0, Layout.SightHeightCm);
            TestTrue(*FString::Printf(TEXT("%s has a sighting line above the rig origin"), *Name), Layout.SightHeightCm > 0.0f);
        }

        const FVector Aimed = BreakerViewmodelAimedPose(Layout.AdsForwardCm, SightLine, ShippedScale);
        const FVector Camera = BreakerViewmodelRigToCamera(Aimed, SightLine, ShippedScale);
        TestTrue(*FString::Printf(TEXT("%s puts its sight on the camera axis when aimed (Y %.3f, Z %.3f)"), *Name, Camera.Y, Camera.Z),
            FMath::Abs(Camera.Y) <= OnAxisToleranceCm && FMath::Abs(Camera.Z) <= OnAxisToleranceCm);
        // Aiming must actually MOVE the weapon, or the trade the ADS layer
        // charges the player for is invisible.
        TestTrue(*FString::Printf(TEXT("%s visibly changes pose when aimed"), *Name),
            !Aimed.Equals(Layout.HipOffsetCm, 1.0f));
        TestTrue(*FString::Printf(TEXT("%s hip fire is held off the centre line"), *Name),
            FMath::Abs(Layout.HipOffsetCm.Y) > 2.0f);
    }
    // GAP: the character's live ActiveSightLineCm is set in
    // RebuildViewmodelParts and is not readable off a CDO, so this test
    // recomputes the fit rather than reading the value the pawn holds. A
    // runtime fixture that builds the rig and reads GetWeaponRestLocation
    // under a full aim alpha would close it; it is not faked here.

    return true;
}

#endif
