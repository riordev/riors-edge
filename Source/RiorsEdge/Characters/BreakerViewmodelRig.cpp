#include "Characters/BreakerViewmodelRig.h"

float FBreakerViewmodelLayout::OverallLengthCm() const
{
    float Front = -FLT_MAX;
    float Back = FLT_MAX;
    for (const FBreakerProxyPart& Part : Parts)
    {
        if (!Part.IsUsed()) continue;
        // Half-length along rig X. Author rotation is ignored here on purpose:
        // this is a silhouette-ordering measure, not a bounding box, and the
        // rakes involved are all under 20 degrees.
        const float Half = 0.5f * Part.SizeCm.X;
        Front = FMath::Max(Front, Part.LocationCm.X + Half);
        Back = FMath::Min(Back, Part.LocationCm.X - Half);
    }
    return (Front > Back) ? (Front - Back) : 0.0f;
}

namespace
{
    using namespace BreakerViewmodel;

    FBreakerProxyPart Part(EBreakerProxyShape Shape, const FVector& Location, const FVector& Size,
        const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        FBreakerProxyPart Out;
        Out.Shape = Shape;
        Out.LocationCm = Location;
        Out.SizeCm = Size;
        Out.Color = Color;
        Out.Rotation = Rotation;
        return Out;
    }
}

// ---------------------------------------------------------------------------
// THE TABLE.
//
// Proportions are read off the weapon's own mechanical character in
// Weapons/BreakerWeaponComponent.cpp rather than invented, because a proxy that
// expresses nothing is just a differently-shaped grey box. The rule applied
// throughout: MAGAZINE SIZE, CADENCE and SWAP TIME are the three things the
// player already feels, so those are the three the silhouette states.
//
//   Rifle       35 rnd, 620 rpm, 0.50 s swap  -> the reference. Everything
//                                               else is described against it.
//   SMG         35 rnd, 900 rpm, 0.35 s swap  -> shortest long gun, no stock,
//                                               visibly cheap polymer.
//   Sniper       8 rnd, 150 rpm, 0.70 s swap  -> longest by far, thinnest, an
//                                               oversized scope breaking the
//                                               top line, taped dope card.
//   Shotgun      8 rnd,  85 rpm, 8 pellets    -> widest and blunt, tube
//                                               magazine under the barrel, a
//                                               bright pump you can watch.
//   Rocket       4 rnd,  55 rpm, 0.80 s swap  -> a tube, worn high, the only
//                                               weapon that breaks the
//                                               shoulder line, hazard amber.
//   Burst Rifle 27 rnd, 3-rnd burst, 0.55 s   -> rifle length but a marksman's
//                                               gun: longer thinner barrel, a
//                                               tall dedicated optic, an
//                                               in-line stock.
//   Machinegun 120 rnd, 700 rpm, 4.2 s reload -> the heaviest object in the
//                                               game: a drum you cannot miss,
//                                               a shrouded barrel, and a BIPOD,
//                                               because "only while planted" is
//                                               the archetype's actual rule.
//   Sidearm     14 rnd, 420 rpm, 0.18 s swap  -> smallest silhouette in the
//                                               game by a factor of three. The
//                                               swap is the point, so it has to
//                                               read as nothing to bring up.
//
// EVERY NUMBER BELOW IS O2 PLACEHOLDER and none of it is playtested; automation
// checks the ordering and the palette law, and a screenshot pass checked that
// each one reads. Nothing here can check how it FEELS.
// ---------------------------------------------------------------------------
FBreakerViewmodelLayout BreakerViewmodel::ArchetypeLayout(EBreakerWeaponArchetype Archetype)
{
    FBreakerViewmodelLayout L;
    static_assert(static_cast<int32>(EBreakerWeaponArchetype::Count) == 8,
        "Add a viewmodel layout row when you add an archetype. A missing row is "
        "the bug this pass exists to fix: three archetypes silently wore the "
        "rifle's proportions for a whole milestone.");

    switch (Archetype)
    {
    case EBreakerWeaponArchetype::SMG:
        // Front-heavy, stockless, smallest long gun. The magazine is the only
        // long thing on it, which is the correct read for 35 rounds in a gun
        // this size.
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector(10.0f, 0.0f,  1.0f), FVector(22.0f, 5.0f, 6.5f),   PolymerCheap),
            Part(EBreakerProxyShape::CylinderX, FVector(25.0f, 0.0f,  1.5f), FVector(12.0f, 2.4f, 2.4f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(32.5f, 0.0f,  1.5f), FVector( 3.0f, 3.2f, 3.2f),   GunmetalDark),
            Part(EBreakerProxyShape::Box,       FVector(12.0f, 0.0f,  5.2f), FVector( 6.0f, 3.0f, 2.6f),   Gunmetal),
            Part(EBreakerProxyShape::Box,       FVector( 2.0f, 0.0f,-11.0f), FVector( 4.5f, 3.4f,21.0f),   Polymer, FRotator(-4.0f, 0.0f, 0.0f)),
            Part(EBreakerProxyShape::Box,       FVector(-4.0f, 0.0f, -7.0f), FVector( 4.5f, 3.8f,10.0f),   Polymer, FRotator(14.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(46.0f, 14.0f, -13.0f);
        L.SightHeightCm = 6.5f;
        L.AdsForwardCm = 46.0f;
        L.SupportHandCm = FVector(21.0f, 0.0f, -4.5f);
        L.FiringHandCm = FVector(-4.0f, 0.0f, -6.0f);
        L.MuzzleCm = FVector(34.5f, 0.0f, 1.5f);
        break;

    case EBreakerWeaponArchetype::Sniper:
        // Longest silhouette in the game and the thinnest. The oversized
        // cylindrical optic breaking the top line is the single feature no
        // other archetype is allowed to use; the off-white band at the comb is
        // the hand-marked dope card that makes it the most PERSONAL weapon.
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector(14.0f, 0.0f,  1.5f), FVector(30.0f, 4.5f, 6.0f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(46.0f, 0.0f,  2.0f), FVector(44.0f, 2.6f, 2.6f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(70.5f, 0.0f,  2.0f), FVector( 5.0f, 3.6f, 3.6f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(18.0f, 0.0f,  8.5f), FVector(26.0f, 7.0f, 7.0f),   Gunmetal),
            Part(EBreakerProxyShape::Box,       FVector( 4.0f, 0.0f, -7.0f), FVector( 5.0f, 3.6f,10.0f),   Polymer),
            Part(EBreakerProxyShape::Box,       FVector(-11.0f,0.0f,  0.5f), FVector(24.0f, 4.5f, 7.5f),   Polymer),
            Part(EBreakerProxyShape::Box,       FVector(-11.0f,0.0f,  4.7f), FVector( 7.0f, 4.8f, 1.2f),   TapeOffWhite),
            Part(EBreakerProxyShape::Box,       FVector(-2.0f, 0.0f, -7.5f), FVector( 5.0f, 4.0f,10.0f),   Polymer, FRotator(12.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(44.0f, 14.0f, -13.0f);
        L.SightHeightCm = 12.0f;
        L.AdsForwardCm = 44.0f;
        L.SupportHandCm = FVector(30.0f, 0.0f, -3.5f);
        L.FiringHandCm = FVector(-2.0f, 0.0f, -6.5f);
        L.MuzzleCm = FVector(73.0f, 0.0f, 2.0f);
        break;

    case EBreakerWeaponArchetype::Shotgun:
        // Widest and thickest. Two features carry the whole read: the TUBE
        // MAGAZINE slung under the barrel, which no other archetype has, and a
        // bright steel pump — Art-And-Modelling-Plan.md §5 asks for "a visible
        // mechanism the player can watch move", and value contrast is the only
        // way a blockout can promise that.
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector(10.0f, 0.0f,  1.0f), FVector(26.0f, 8.0f, 9.0f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(35.0f, 0.0f,  3.0f), FVector(36.0f, 5.0f, 5.0f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(54.5f, 0.0f,  3.0f), FVector( 3.0f, 5.8f, 5.8f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(34.0f, 0.0f, -3.0f), FVector(32.0f, 4.2f, 4.2f),   GunmetalDark),
            Part(EBreakerProxyShape::Box,       FVector(30.0f, 0.0f, -3.0f), FVector( 9.0f, 6.5f, 6.0f),   SteelBright),
            Part(EBreakerProxyShape::Box,       FVector(22.0f, 0.0f,  6.2f), FVector( 3.0f, 2.4f, 2.0f),   SteelBright),
            Part(EBreakerProxyShape::Box,       FVector(-9.0f, 0.0f,  0.0f), FVector(22.0f, 5.0f, 8.0f),   Polymer),
            Part(EBreakerProxyShape::Box,       FVector(-1.0f, 0.0f, -8.0f), FVector( 5.5f, 4.5f,10.0f),   Polymer, FRotator(14.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(46.0f, 15.0f, -14.0f);
        L.SightHeightCm = 7.2f;
        L.AdsForwardCm = 46.0f;
        // The support hand goes on the PUMP, not the handguard. It is the one
        // hand position in the table that is about the action rather than the
        // barrel, and it puts the hand where the mechanism is.
        L.SupportHandCm = FVector(30.0f, 0.0f, -6.0f);
        L.FiringHandCm = FVector(-1.0f, 0.0f, -7.0f);
        L.MuzzleCm = FVector(57.0f, 0.0f, 3.0f);
        break;

    case EBreakerWeaponArchetype::Rocket:
        // The only weapon that breaks the player's shoulder line, so the tube
        // is mounted HIGH and the sight is thrown off to the left: asymmetry is
        // the read. Hazard amber is permitted here and nowhere else — this is
        // the field-fabricated one, and §5 asks for hand-painted striping.
        L.Parts = {
            Part(EBreakerProxyShape::CylinderX, FVector(14.0f, 0.0f,  6.0f), FVector(66.0f,11.0f,11.0f),   GunmetalDark),
            Part(EBreakerProxyShape::ConeX,     FVector(51.0f, 0.0f,  6.0f), FVector( 9.0f,14.0f,14.0f),   Gunmetal),
            Part(EBreakerProxyShape::ConeX,     FVector(-23.0f,0.0f,  6.0f), FVector( 9.0f,12.5f,12.5f),   Gunmetal, FRotator(0.0f, 180.0f, 0.0f)),
            Part(EBreakerProxyShape::CylinderX, FVector(26.0f, 0.0f,  6.0f), FVector( 5.0f,11.6f,11.6f),   HazardAmber),
            Part(EBreakerProxyShape::Box,       FVector( 8.0f,-7.5f, 11.0f), FVector(10.0f, 3.0f, 6.0f),   Gunmetal),
            Part(EBreakerProxyShape::Box,       FVector( 2.0f, 0.0f, -4.0f), FVector( 5.5f, 4.5f,12.0f),   Polymer, FRotator(12.0f, 0.0f, 0.0f)),
            Part(EBreakerProxyShape::Box,       FVector(27.0f, 0.0f, -4.0f), FVector( 5.0f, 4.0f,10.0f),   Polymer, FRotator(-10.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(72.0f, 19.0f, -22.0f);
        L.SightHeightCm = 15.0f;
        L.AdsForwardCm = 72.0f;
        L.SupportHandCm = FVector(24.0f, 0.0f, -6.0f);
        L.FiringHandCm = FVector(2.0f, 0.0f, -6.0f);
        L.MuzzleCm = FVector(56.0f, 0.0f, 6.0f);
        break;

    case EBreakerWeaponArchetype::BurstRifle:
        // Rifle length, marksman's build. Distinguished from the rifle by the
        // three things the discipline weapon would actually have: a longer,
        // thinner barrel, a TALL dedicated optic on a visible riser, and a
        // straight in-line stock. Whoever carries this pre-aims the ladder.
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector(14.0f, 0.0f,  1.5f), FVector(32.0f, 4.8f, 6.4f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(48.0f, 0.0f,  2.0f), FVector(34.0f, 2.6f, 2.6f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(67.0f, 0.0f,  2.0f), FVector( 4.0f, 3.4f, 3.4f),   GunmetalDark),
            Part(EBreakerProxyShape::Box,       FVector(16.0f, 0.0f,  6.0f), FVector( 4.0f, 3.0f, 4.5f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(16.0f, 0.0f,  9.8f), FVector(16.0f, 5.2f, 5.2f),   Gunmetal),
            Part(EBreakerProxyShape::Box,       FVector( 5.0f, 0.0f, -8.0f), FVector( 5.5f, 4.0f,15.0f),   Polymer, FRotator(-6.0f, 0.0f, 0.0f)),
            Part(EBreakerProxyShape::Box,       FVector(-9.0f, 0.0f,  1.5f), FVector(22.0f, 4.2f, 5.4f),   Polymer),
            Part(EBreakerProxyShape::Box,       FVector(-1.0f, 0.0f, -8.0f), FVector( 5.0f, 4.0f,11.0f),   Polymer, FRotator(12.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(46.0f, 14.0f, -13.0f);
        L.SightHeightCm = 12.4f;
        L.AdsForwardCm = 46.0f;
        L.SupportHandCm = FVector(33.0f, 0.0f, -3.5f);
        L.FiringHandCm = FVector(-1.0f, 0.0f, -7.0f);
        L.MuzzleCm = FVector(69.5f, 0.0f, 2.0f);
        break;

    case EBreakerWeaponArchetype::Machinegun:
        // The heaviest object in the game and it has to look it. A 120-round
        // magazine is four SMG magazines in one trigger pull, so it is a DRUM
        // and it is the biggest single part on any weapon here. The bipod is
        // not decoration: the archetype's recoil is designed to be unrideable
        // past a point, so the gun's own silhouette should say "planted".
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector(12.0f, 0.0f,  1.0f), FVector(34.0f, 8.0f,10.0f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(42.0f, 0.0f,  2.0f), FVector(30.0f, 7.0f, 7.0f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(59.5f, 0.0f,  2.0f), FVector( 5.0f, 5.4f, 5.4f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderY, FVector( 7.0f, 0.0f,-11.0f), FVector(17.0f, 7.0f,17.0f),   GunmetalDark),
            Part(EBreakerProxyShape::Box,       FVector(46.0f,-4.0f, -8.0f), FVector( 2.2f, 2.2f,14.0f),   SteelBright, FRotator(-20.0f, 0.0f,-22.0f)),
            Part(EBreakerProxyShape::Box,       FVector(46.0f, 4.0f, -8.0f), FVector( 2.2f, 2.2f,14.0f),   SteelBright, FRotator(-20.0f, 0.0f, 22.0f)),
            Part(EBreakerProxyShape::Box,       FVector(14.0f, 0.0f,  7.6f), FVector( 8.0f, 3.2f, 3.0f),   Gunmetal),
            Part(EBreakerProxyShape::Box,       FVector(-10.0f,0.0f,  0.5f), FVector(22.0f, 5.0f, 7.0f),   Polymer),
            Part(EBreakerProxyShape::Box,       FVector(-2.0f, 0.0f, -8.0f), FVector( 5.5f, 4.5f,11.0f),   Polymer, FRotator(12.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(48.0f, 16.0f, -15.0f);
        L.SightHeightCm = 9.1f;
        L.AdsForwardCm = 47.0f;
        L.SupportHandCm = FVector(30.0f, 0.0f, -6.5f);
        L.FiringHandCm = FVector(-2.0f, 0.0f, -7.0f);
        L.MuzzleCm = FVector(62.5f, 0.0f, 2.0f);
        break;

    case EBreakerWeaponArchetype::Sidearm:
        // Smallest silhouette in the game and it must not be close. The whole
        // archetype is a 0.18 s swap-in against the machinegun's 0.95, so the
        // read the moment it comes up is "there is almost nothing in my hands".
        // No stock, no separate magazine — the rounds live in the grip.
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector( 7.0f, 0.0f,  1.0f), FVector(17.0f, 3.4f, 5.4f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(16.5f, 0.0f,  0.6f), FVector( 3.0f, 2.4f, 2.4f),   GunmetalDark),
            Part(EBreakerProxyShape::Box,       FVector(13.5f, 0.0f,  3.6f), FVector( 1.6f, 2.2f, 1.4f),   SteelBright),
            Part(EBreakerProxyShape::Box,       FVector( 0.0f, 0.0f,  3.6f), FVector( 2.0f, 3.0f, 1.4f),   SteelBright),
            Part(EBreakerProxyShape::Box,       FVector(-1.5f, 0.0f, -7.0f), FVector( 4.6f, 3.6f,12.0f),   Polymer, FRotator(16.0f, 0.0f, 0.0f)),
        };
        // Held closer to the centre line and higher than any long gun, which is
        // both how a pistol is actually carried and a second, positional tell.
        L.HipOffsetCm = FVector(40.0f, 11.0f, -11.0f);
        L.SightHeightCm = 4.4f;
        L.AdsForwardCm = 40.0f;
        // Both hands stack at the grip: there is nothing out front to hold.
        L.SupportHandCm = FVector(-2.5f, -3.5f, -8.0f);
        L.FiringHandCm = FVector(-1.5f, 1.0f, -6.5f);
        L.MuzzleCm = FVector(18.5f, 0.0f, 0.6f);
        break;

    case EBreakerWeaponArchetype::Rifle:
    default:
        // THE REFERENCE. "The one gun that looks issued" (§5): uniform matte,
        // nothing personalised, a boxy top-mounted optic, everything balanced
        // about the grip. Every other row above is described against this one.
        L.Parts = {
            Part(EBreakerProxyShape::Box,       FVector(14.0f, 0.0f,  1.5f), FVector(34.0f, 5.0f, 7.0f),   Gunmetal),
            Part(EBreakerProxyShape::CylinderX, FVector(44.0f, 0.0f,  2.5f), FVector(26.0f, 3.0f, 3.0f),   GunmetalDark),
            Part(EBreakerProxyShape::CylinderX, FVector(59.0f, 0.0f,  2.5f), FVector( 4.0f, 4.2f, 4.2f),   GunmetalDark),
            Part(EBreakerProxyShape::Box,       FVector(16.0f, 0.0f,  6.6f), FVector(12.0f, 3.6f, 3.4f),   Gunmetal),
            Part(EBreakerProxyShape::Box,       FVector( 6.0f, 0.0f, -9.0f), FVector( 5.5f, 4.0f,17.0f),   Polymer, FRotator(-8.0f, 0.0f, 0.0f)),
            Part(EBreakerProxyShape::Box,       FVector(-8.0f, 0.0f,  0.5f), FVector(22.0f, 4.5f, 6.5f),   Polymer),
            Part(EBreakerProxyShape::Box,       FVector(-1.0f, 0.0f, -8.0f), FVector( 5.0f, 4.0f,11.0f),   Polymer, FRotator(12.0f, 0.0f, 0.0f)),
        };
        L.HipOffsetCm = FVector(46.0f, 14.0f, -13.0f);
        L.SightHeightCm = 8.3f;
        L.AdsForwardCm = 46.0f;
        L.SupportHandCm = FVector(34.0f, 0.0f, -3.5f);
        L.FiringHandCm = FVector(-1.0f, 0.0f, -7.0f);
        L.MuzzleCm = FVector(61.5f, 0.0f, 2.5f);
        break;
    }

    return L;
}

void BreakerViewmodel::ResolvePartTransform(const FBreakerProxyPart& Part, FVector& OutScale, FRotator& OutRotation)
{
    // The engine's BasicShapes are all 100 cm on their longest authored axis:
    // Cube is 100^3, Cylinder and Cone are 100 tall along their own local Z
    // with a 100 diameter. Dividing by 100 is what turns the centimetre
    // authoring above into a component scale.
    constexpr float Unit = 100.0f;
    FRotator Intrinsic = FRotator::ZeroRotator;

    switch (Part.Shape)
    {
    case EBreakerProxyShape::CylinderX:
    case EBreakerProxyShape::ConeX:
        // Pitch +90 maps the mesh's local +Z onto rig +X, so a cylinder
        // authored "26 cm long, 3 cm across" points down the barrel line.
        Intrinsic = FRotator(90.0f, 0.0f, 0.0f);
        OutScale = FVector(Part.SizeCm.Y, Part.SizeCm.Z, Part.SizeCm.X) / Unit;
        break;
    case EBreakerProxyShape::CylinderY:
        // Roll -90 maps local +Z onto rig +Y: a drum lying on its side.
        Intrinsic = FRotator(0.0f, 0.0f, -90.0f);
        OutScale = FVector(Part.SizeCm.X, Part.SizeCm.Z, Part.SizeCm.Y) / Unit;
        break;
    case EBreakerProxyShape::Box:
    default:
        OutScale = Part.SizeCm / Unit;
        break;
    }

    // Author rotation composes ON TOP of the intrinsic one; quaternion
    // multiplication rather than rotator addition because rotator addition is
    // wrong the moment two axes are involved (the bipod legs are both pitched
    // and rolled, and adding them splays them into the floor).
    OutRotation = (Part.Rotation.Quaternion() * Intrinsic.Quaternion()).Rotator();
}

void BreakerViewmodel::ResolveLimb(const FVector& AnchorCm, const FVector& HandCm,
    FVector& OutCentreCm, FRotator& OutRotation, float& OutLengthCm)
{
    const FVector Delta = HandCm - AnchorCm;
    OutLengthCm = Delta.Size();
    OutCentreCm = AnchorCm + Delta * 0.5f;
    // A box authored along its own X, aimed at the hand. Degenerate case kept
    // explicit: a zero-length limb would produce a NaN rotation and a component
    // that renders as a single corrupt triangle.
    OutRotation = (OutLengthCm > KINDA_SMALL_NUMBER) ? Delta.Rotation() : FRotator::ZeroRotator;
}
