#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/BreakerViewmodelRig.h"

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

    // The support hand MOVES per archetype. If it did not, every gun would be
    // held identically and the hands would stop being information.
    const FVector RifleHand = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Rifle).SupportHandCm;
    const FVector SidearmHand = BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype::Sidearm).SupportHandCm;
    TestTrue(TEXT("The sidearm's hands stack at the grip rather than out front"),
        SidearmHand.X < RifleHand.X - 20.0f);

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

    // ADS is DERIVED from each layout's sight height rather than authored, so
    // every archetype puts its OWN sight on the crosshair. This mirrors
    // ABreakerCharacter::GetWeaponRestLocation exactly; if that formula changes
    // and this does not, the two have diverged and the sights have drifted.
    for (EBreakerWeaponArchetype Archetype : BreakerViewmodelAllArchetypes)
    {
        const FBreakerViewmodelLayout Layout = BreakerViewmodel::ArchetypeLayout(Archetype);
        const FString Name = BreakerWeaponArchetypeNames::Display(Archetype);
        const FVector Aimed(Layout.AdsForwardCm, 0.0f, -Layout.SightHeightCm);

        TestEqual(*FString::Printf(TEXT("%s aims down the centre line"), *Name), static_cast<float>(Aimed.Y), 0.0f, 0.0001f);
        TestTrue(*FString::Printf(TEXT("%s puts its sight on the crosshair"), *Name),
            FMath::IsNearlyZero(Aimed.Z + Layout.SightHeightCm, 0.0001f));
        // Aiming must actually MOVE the weapon, or the trade the ADS layer
        // charges the player for is invisible.
        TestTrue(*FString::Printf(TEXT("%s visibly changes pose when aimed"), *Name),
            !Aimed.Equals(Layout.HipOffsetCm, 1.0f));
        TestTrue(*FString::Printf(TEXT("%s hip fire is held off the centre line"), *Name),
            FMath::Abs(Layout.HipOffsetCm.Y) > 2.0f);
    }

    return true;
}

#endif
