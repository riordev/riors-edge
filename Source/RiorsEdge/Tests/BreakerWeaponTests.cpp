#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponFeel.h"
#include "Weapons/BreakerWeaponMath.h"
#include "UI/BreakerTracerMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponCadenceTest,
    "RiorsEdge.Weapons.Cadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponCadenceTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("600 RPM fires every 100 ms"), FBreakerWeaponMath::FireInterval(600.0f), 0.1f);
    TestEqual(TEXT("RPM clamps safely"), FBreakerWeaponMath::FireInterval(0.0f), 60.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponFalloffTest,
    "RiorsEdge.Weapons.Falloff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponFalloffTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponDefinition* Definition = NewObject<UBreakerWeaponDefinition>();
    Definition->FalloffStart = 2000.0f;
    Definition->FalloffEnd = 6000.0f;
    Definition->MinimumFalloffMultiplier = 0.5f;
    TestEqual(TEXT("Full damage before falloff"), FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 1500.0f), 1.0f);
    TestEqual(TEXT("Halfway distance interpolates damage"), FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 4000.0f), 0.75f);
    TestEqual(TEXT("Minimum damage after falloff"), FBreakerWeaponMath::DamageMultiplierAtDistance(Definition, 7000.0f), 0.5f);
    return true;
}

// ---------------------------------------------------------------------------
// Owner playtest report: "dmg fall off is too high". The gym is an open field
// whose ranged enemy holds 9-19 m, so the ordinary fight now happens where a
// small-arena curve had already started biting. These tests pin the SHAPE the
// softening must keep: the archetypes stay different, and the ordering of how
// hard each one falls off is the identity, not the severity.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArchetypeFalloffTest,
    "RiorsEdge.Weapons.ArchetypeFalloff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArchetypeFalloffTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    auto DefinitionFor = [Weapon](EBreakerWeaponArchetype Archetype)
    {
        Weapon->EquipArchetype(Archetype);
        return Weapon->GetActiveDefinition();
    };
    // Damage lost per centimetre across the ramp: the honest measure of "how
    // hard does this weapon fall off", independent of where the ramp sits.
    auto Severity = [](const UBreakerWeaponDefinition* Definition)
    {
        return (1.0f - Definition->MinimumFalloffMultiplier) / (Definition->FalloffEnd - Definition->FalloffStart);
    };

    const UBreakerWeaponDefinition* Rifle = DefinitionFor(EBreakerWeaponArchetype::Rifle);
    const UBreakerWeaponDefinition* SMG = DefinitionFor(EBreakerWeaponArchetype::SMG);
    const UBreakerWeaponDefinition* Sniper = DefinitionFor(EBreakerWeaponArchetype::Sniper);
    const UBreakerWeaponDefinition* Shotgun = DefinitionFor(EBreakerWeaponArchetype::Shotgun);

    // The mechanic is not being removed: every one of them still falls off.
    for (const UBreakerWeaponDefinition* Definition : { Rifle, SMG, Sniper, Shotgun })
    {
        TestTrue(TEXT("Every archetype still loses damage at range"), Definition->MinimumFalloffMultiplier < 1.0f);
        TestTrue(TEXT("Every falloff ramp has a positive length"), Definition->FalloffEnd > Definition->FalloffStart);
    }

    // And they are still five different weapons, hardest-falling to softest.
    TestTrue(TEXT("The shotgun falls off hardest of all"),
        Severity(Shotgun) > Severity(SMG) && Severity(Shotgun) > Severity(Rifle) && Severity(Shotgun) > Severity(Sniper));
    TestTrue(TEXT("The SMG falls off harder than the rifle"), Severity(SMG) > Severity(Rifle));
    TestTrue(TEXT("The rifle falls off harder than the sniper"), Severity(Rifle) > Severity(Sniper));
    TestTrue(TEXT("The sniper barely falls off at all"), Sniper->MinimumFalloffMultiplier >= 0.85f);
    TestTrue(TEXT("The shotgun's severity is at least triple the rifle's"), Severity(Shotgun) > Severity(Rifle) * 3.0f);

    // The engagement band the gym actually produces (the ranged enemy holds
    // 900-1900 cm). The primary must be untouched across all of it, so a
    // falloff pass cannot silently move the measured trash/elite TTK.
    TestEqual(TEXT("The rifle is at full damage where the ranged enemy opens"),
        FBreakerWeaponMath::DamageMultiplierAtDistance(Rifle, 900.0f), 1.0f);
    TestEqual(TEXT("The rifle is still at full damage at the far edge of the band"),
        FBreakerWeaponMath::DamageMultiplierAtDistance(Rifle, 1900.0f), 1.0f);
    // The secondary is where the band actually hurt, and it must now hold on
    // to more than half its damage out to the far edge of it.
    TestTrue(TEXT("The scattergun keeps over two thirds of its damage at 19 m"),
        FBreakerWeaponMath::DamageMultiplierAtDistance(Shotgun, 1900.0f) > 0.67f);
    // But not so much that the shotgun becomes a rifle.
    TestTrue(TEXT("The scattergun is still clearly punished at 19 m"),
        FBreakerWeaponMath::DamageMultiplierAtDistance(Shotgun, 1900.0f) < 0.80f);
    return true;
}

// ---------------------------------------------------------------------------
// Owner playtest report: "weakpoints dont feel forgiving as they should".
// The head is a 20 cm sphere and the shot is a zero-radius line, so acceptance
// is a binary a player cannot feel the edges of. The halo is world-space, so
// the generosity is the same physical size at 5 m and 50 m.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeakPointToleranceTest,
    "RiorsEdge.Weapons.WeakPointTolerance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeakPointToleranceTest::RunTest(const FString& Parameters)
{
    const FVector Muzzle = FVector::ZeroVector;
    const FVector Forward = FVector::ForwardVector;
    // A head-sized sphere ten metres downrange, at the enemy's head height.
    const FVector Head(1000.0f, 0.0f, 78.0f);
    const float Radius = 20.0f;
    const float Tolerance = 14.0f;

    auto RayAtHeight = [](float Height) { return FVector(1000.0f, 0.0f, Height).GetSafeNormal(); };

    TestEqual(TEXT("A ray straight at the centre is at zero distance"),
        FBreakerWeaponMath::DistanceFromRayToPoint(Muzzle, RayAtHeight(78.0f), Head), 0.0f, 0.01f);

    // Dead centre and a clean clip of the sphere both count, as they did.
    TestTrue(TEXT("A centre hit is a weak point"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, RayAtHeight(78.0f), Head, Radius, Tolerance));
    TestTrue(TEXT("A shot inside the sphere is a weak point"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, RayAtHeight(62.0f), Head, Radius, Tolerance));

    // The point of the change: a near-miss that used to read as a body shot.
    TestFalse(TEXT("Without tolerance a near-miss is only a body shot"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, RayAtHeight(50.0f), Head, Radius, 0.0f));
    TestTrue(TEXT("With tolerance the same near-miss reads as a weak point"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, RayAtHeight(50.0f), Head, Radius, Tolerance));

    // Generosity has an edge, and it is where the authored number puts it.
    TestFalse(TEXT("A chest shot is still a chest shot"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, RayAtHeight(20.0f), Head, Radius, Tolerance));

    // World space, not screen space: the halo is the same physical size at
    // every range, so aiming does not get easier by walking backwards.
    const FVector FarHead(6000.0f, 0.0f, 78.0f);
    const FVector NearHead(300.0f, 0.0f, 78.0f);
    auto OffsetRay = [](float Distance, float Offset)
    {
        return FVector(Distance, 0.0f, 78.0f + Offset).GetSafeNormal();
    };
    TestTrue(TEXT("A 30 cm near-miss counts at 60 m"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, OffsetRay(6000.0f, 30.0f), FarHead, Radius, Tolerance));
    TestTrue(TEXT("The same 30 cm near-miss counts at 3 m"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, OffsetRay(300.0f, 30.0f), NearHead, Radius, Tolerance));
    TestFalse(TEXT("A 40 cm miss counts at neither range"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, OffsetRay(6000.0f, 40.0f), FarHead, Radius, Tolerance)
        || FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, OffsetRay(300.0f, 40.0f), NearHead, Radius, Tolerance));

    // A weak point behind the muzzle is not shootable, however close the
    // infinite line would pass to it.
    TestFalse(TEXT("A weak point behind the shooter is never a weak point"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, Forward, FVector(-1000.0f, 0.0f, 0.0f), Radius, Tolerance));
    TestEqual(TEXT("Distance behind the muzzle clamps to the muzzle"),
        FBreakerWeaponMath::DistanceFromRayToPoint(Muzzle, Forward, FVector(-500.0f, 0.0f, 0.0f)), 500.0f, 0.01f);

    // Zero tolerance restores the exact geometric test, so the owner can turn
    // the whole change off with one number.
    TestTrue(TEXT("Zero tolerance still accepts the sphere itself"),
        FBreakerWeaponMath::IsWithinWeakPointTolerance(Muzzle, RayAtHeight(78.0f), Head, Radius, 0.0f));
    return true;
}

// ---------------------------------------------------------------------------
// Owner playtest report: "hip firing feels worse than ads". It was, at every
// range, because ADS tightened four axes at once and cost nothing. ADS now
// pays in TIME (it ramps in) and in MOBILITY (movement widens an aimed shot
// harder than a hip shot). The decision is: plant and aim, or move and hip.
// These tests prove the trade exists without deleting it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHipFireTradeTest,
    "RiorsEdge.Weapons.HipFireTrade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHipFireTradeTest::RunTest(const FString& Parameters)
{
    FBreakerRecoilProfile Profile;
    Profile.AimRecoilMultiplier = 0.5f;
    Profile.AimBloomMultiplier = 0.5f;
    Profile.AimViewmodelMultiplier = 0.5f;
    Profile.MoveSpreadDegrees = 0.4f;
    Profile.AimMoveSpreadMultiplier = 2.0f;

    // ---- ADS costs time -------------------------------------------------
    const FBreakerRecoilProfile Hip = FBreakerWeaponFeel::ProfileAtAimAlpha(Profile, 0.0f);
    const FBreakerRecoilProfile Half = FBreakerWeaponFeel::ProfileAtAimAlpha(Profile, 0.5f);
    const FBreakerRecoilProfile Full = FBreakerWeaponFeel::ProfileAtAimAlpha(Profile, 1.0f);

    TestEqual(TEXT("Before the sights come up there is no aim benefit at all"), Hip.AimRecoilMultiplier, 1.0f);
    TestEqual(TEXT("Halfway into the sights buys half the benefit"), Half.AimRecoilMultiplier, 0.75f, 0.0001f);
    TestEqual(TEXT("Halfway into the sights buys half the bloom benefit"), Half.AimBloomMultiplier, 0.75f, 0.0001f);
    TestEqual(TEXT("Halfway into the sights buys half the viewmodel benefit"), Half.AimViewmodelMultiplier, 0.75f, 0.0001f);
    TestEqual(TEXT("Fully sighted is the authored profile, unchanged"), Full.AimRecoilMultiplier, Profile.AimRecoilMultiplier);

    // A shot snapped off the instant the aim button goes down kicks like a hip
    // shot, because that is what it is.
    const FBreakerRecoilKick SnapShot = FBreakerWeaponFeel::ComputeShotKick(Hip, 2, 9, true);
    const FBreakerRecoilKick TrueHip = FBreakerWeaponFeel::ComputeShotKick(Profile, 2, 9, false);
    const FBreakerRecoilKick Settled = FBreakerWeaponFeel::ComputeShotKick(Full, 2, 9, true);
    TestEqual(TEXT("Snapping to sights and firing gets no recoil benefit"), SnapShot.PitchDegrees, TrueHip.PitchDegrees, 0.0001f);
    TestTrue(TEXT("Waiting for the sights is still strictly better"), Settled.PitchDegrees < SnapShot.PitchDegrees);

    // ---- ADS costs mobility ---------------------------------------------
    TestEqual(TEXT("Standing still costs nothing, aimed or not"),
        FBreakerWeaponFeel::MovementSpreadDegrees(Profile, 0.0f, 1.0f), 0.0f);
    const float HipMoving = FBreakerWeaponFeel::MovementSpreadDegrees(Profile, 1.0f, 0.0f);
    const float AimedMoving = FBreakerWeaponFeel::MovementSpreadDegrees(Profile, 1.0f, 1.0f);
    TestEqual(TEXT("Hip fire pays the authored movement cone"), HipMoving, 0.4f, 0.0001f);
    TestEqual(TEXT("Aimed movement costs the authored multiple of it"), AimedMoving, 0.8f, 0.0001f);
    TestTrue(TEXT("Movement punishes the sights harder than the hip"), AimedMoving > HipMoving);
    TestTrue(TEXT("Half speed costs less than full speed"),
        FBreakerWeaponFeel::MovementSpreadDegrees(Profile, 0.5f, 0.0f) < HipMoving);

    // ---- The decision itself --------------------------------------------
    // Planted: ADS wins, as it must. Moving: hip fire wins the first shot.
    // Neither of these is allowed to become the other.
    const float PlantedHip = FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, 1.2f, 0.3f, 3, 0.0f);
    const float PlantedAimed = FBreakerWeaponFeel::EffectiveSpreadDegrees(Full, 0.25f, 0.3f, 3, 0.0f);
    TestTrue(TEXT("Planted, the sights are still the accurate option"), PlantedAimed < PlantedHip);

    const float MovingHipFirst = FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, 1.2f, 0.0f, 0, HipMoving);
    const float MovingAimedFirst = FBreakerWeaponFeel::EffectiveSpreadDegrees(Full, 0.25f, 0.0f, 0, AimedMoving);
    TestTrue(TEXT("On the move, the first hip shot is tighter than the first aimed shot"),
        MovingHipFirst < MovingAimedFirst);

    // Movement is not forgiven by first-shot accuracy: a runner does not get a
    // free perfect shot for letting the trigger rest.
    TestEqual(TEXT("A moving first shot still pays the movement cone"), MovingHipFirst, HipMoving, 0.0001f);
    TestEqual(TEXT("A planted first shot is still dead accurate"),
        FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, 1.2f, 0.0f, 0, 0.0f), 0.0f);

    // ---- And the archetype table honours all of it -----------------------
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    const EBreakerWeaponArchetype Archetypes[] = {
        EBreakerWeaponArchetype::Rifle, EBreakerWeaponArchetype::SMG, EBreakerWeaponArchetype::Sniper,
        EBreakerWeaponArchetype::Shotgun, EBreakerWeaponArchetype::Rocket };
    for (const EBreakerWeaponArchetype Archetype : Archetypes)
    {
        Weapon->EquipArchetype(Archetype);
        const FBreakerRecoilProfile Live = Weapon->GetRecoilProfile();
        TestTrue(TEXT("Every archetype makes ADS cost time"), Live.AimInSeconds > 0.0f);
        TestTrue(TEXT("Every archetype makes movement cost cone"), Live.MoveSpreadDegrees > 0.0f);
        TestTrue(TEXT("Every archetype punishes aimed movement harder than hip movement"),
            Live.AimMoveSpreadMultiplier > 1.0f);
    }

    Weapon->EquipArchetype(EBreakerWeaponArchetype::Sniper);
    const FBreakerRecoilProfile LiveSniper = Weapon->GetRecoilProfile();
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Shotgun);
    const FBreakerRecoilProfile LiveShotgun = Weapon->GetRecoilProfile();
    Weapon->EquipArchetype(EBreakerWeaponArchetype::SMG);
    const FBreakerRecoilProfile LiveSMG = Weapon->GetRecoilProfile();

    // Five weapons, five relationships with standing still.
    TestTrue(TEXT("The sniper is the weapon that must be planted"),
        LiveSniper.MoveSpreadDegrees > LiveShotgun.MoveSpreadDegrees * 3.0f);
    TestTrue(TEXT("The sniper is the slowest into its sights"),
        LiveSniper.AimInSeconds > LiveSMG.AimInSeconds && LiveSniper.AimInSeconds > LiveShotgun.AimInSeconds);
    TestTrue(TEXT("The SMG is the fastest into its sights"), LiveSMG.AimInSeconds <= LiveShotgun.AimInSeconds);
    TestTrue(TEXT("The shotgun is the least punished for moving"),
        LiveShotgun.MoveSpreadDegrees < LiveSMG.MoveSpreadDegrees);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponSpreadTest,
    "RiorsEdge.Weapons.DeterministicSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponSpreadTest::RunTest(const FString& Parameters)
{
    const FVector Direction = FVector::ForwardVector;
    const FVector First = FBreakerWeaponMath::ApplyConeSpread(Direction, 2.0f, 42);
    const FVector Second = FBreakerWeaponMath::ApplyConeSpread(Direction, 2.0f, 42);
    TestTrue(TEXT("Same seed returns same direction"), First.Equals(Second));
    TestTrue(TEXT("Spread remains normalized"), FMath::IsNearlyEqual(First.Size(), 1.0f));
    TestTrue(TEXT("Spread remains inside requested cone"), FMath::RadiansToDegrees(FMath::Acos(Direction.Dot(First))) <= 2.0f + KINDA_SMALL_NUMBER);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponArchetypeTest,
    "RiorsEdge.Weapons.Archetypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponArchetypeTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    TestEqual(TEXT("Rifle is the default archetype"), Weapon->GetArchetype(), EBreakerWeaponArchetype::Rifle);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Shotgun);
    TestEqual(TEXT("Shotgun can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("SHOTGUN")));
    TestEqual(TEXT("Shotgun receives its eight-round magazine"), Weapon->GetMagazineAmmo(), 8);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Sniper);
    TestEqual(TEXT("Sniper can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("SNIPER")));
    TestEqual(TEXT("Sniper receives its eight-round magazine"), Weapon->GetMagazineAmmo(), 8);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::SMG);
    TestEqual(TEXT("SMG can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("SMG")));
    TestEqual(TEXT("SMG receives its thirty-five round magazine"), Weapon->GetMagazineAmmo(), 35);
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Rocket);
    TestEqual(TEXT("Rocket launcher can be equipped"), Weapon->GetArchetypeName(), FString(TEXT("ROCKET")));
    TestEqual(TEXT("Rocket receives its four-round magazine"), Weapon->GetMagazineAmmo(), 4);
    UBreakerWeaponComponent* Loadout = NewObject<UBreakerWeaponComponent>();
    Loadout->EquipSlot(2);
    TestEqual(TEXT("Players can equip the secondary slot"), Loadout->GetCurrentSlot(), 2);
    TestEqual(TEXT("The prototype secondary slot carries the shotgun"), Loadout->GetArchetypeName(), FString(TEXT("SHOTGUN")));
    Loadout->SetSlotArchetype(2, EBreakerWeaponArchetype::Rocket);
    TestEqual(TEXT("Slot archetypes are assignable"), Loadout->GetArchetypeName(), FString(TEXT("ROCKET")));
    TestEqual(TEXT("Reassignment grants the new weapon's magazine"), Loadout->GetMagazineAmmo(), 4);
    Loadout->SetSlotArchetype(2, EBreakerWeaponArchetype::Shotgun);
    Loadout->EquipSlot(1);
    TestEqual(TEXT("Players can return to the primary slot"), Loadout->GetCurrentSlot(), 1);
    TestEqual(TEXT("The prototype primary slot carries the rifle"), Loadout->GetArchetypeName(), FString(TEXT("RIFLE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRecoilPatternTest,
    "RiorsEdge.Weapons.RecoilPattern",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRecoilPatternTest::RunTest(const FString& Parameters)
{
    FBreakerRecoilProfile Profile;
    Profile.VerticalKickDegrees = 1.0f;
    Profile.HorizontalKickDegrees = 0.5f;
    Profile.HorizontalRandomDegrees = 0.0f;
    Profile.VerticalRandomFraction = 0.0f;
    Profile.HorizontalPatternPeriod = 8;
    Profile.ClimbRampShots = 4.0f;
    Profile.ClimbRampMultiplier = 2.0f;

    const FBreakerRecoilKick First = FBreakerWeaponFeel::ComputeShotKick(Profile, 0, 1234, false);
    const FBreakerRecoilKick Repeat = FBreakerWeaponFeel::ComputeShotKick(Profile, 0, 1234, false);
    TestEqual(TEXT("Same seed and shot index reproduce the same kick"), First.PitchDegrees, Repeat.PitchDegrees);
    TestEqual(TEXT("Same seed and shot index reproduce the same drift"), First.YawDegrees, Repeat.YawDegrees);
    TestTrue(TEXT("Recoil climbs upward"), First.PitchDegrees > 0.0f);
    TestTrue(TEXT("The first shot has no horizontal component"), FMath::IsNearlyZero(First.YawDegrees));

    // The climb ramp: the fifth shot of a held burst kicks twice as hard.
    const FBreakerRecoilKick Ramped = FBreakerWeaponFeel::ComputeShotKick(Profile, 4, 1234, false);
    TestEqual(TEXT("The ramp doubles the kick by its fourth shot"), Ramped.PitchDegrees, 2.0f, 0.001f);

    // The horizontal pattern is a learnable curve, not noise: it drifts one
    // way for half the period and back for the other half.
    const FBreakerRecoilKick Quarter = FBreakerWeaponFeel::ComputeShotKick(Profile, 2, 1234, false);
    const FBreakerRecoilKick ThreeQuarter = FBreakerWeaponFeel::ComputeShotKick(Profile, 6, 1234, false);
    TestTrue(TEXT("The pattern drifts one way"), Quarter.YawDegrees > 0.0f);
    TestTrue(TEXT("The pattern drifts back the other way"), ThreeQuarter.YawDegrees < 0.0f);

    // The random component is small, present, and independent of the pattern.
    Profile.VerticalRandomFraction = 0.2f;
    const FBreakerRecoilKick SeedA = FBreakerWeaponFeel::ComputeShotKick(Profile, 0, 11, false);
    const FBreakerRecoilKick SeedB = FBreakerWeaponFeel::ComputeShotKick(Profile, 0, 99, false);
    TestTrue(TEXT("Different seeds perturb the kick"), !FMath::IsNearlyEqual(SeedA.PitchDegrees, SeedB.PitchDegrees));
    TestTrue(TEXT("The perturbation stays inside the authored fraction"),
        FMath::Abs(SeedA.PitchDegrees - 1.0f) <= 0.2f + KINDA_SMALL_NUMBER);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRecoilAccumulationTest,
    "RiorsEdge.Weapons.RecoilAccumulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRecoilAccumulationTest::RunTest(const FString& Parameters)
{
    FBreakerRecoilProfile Profile;
    Profile.VerticalKickDegrees = 1.0f;
    Profile.HorizontalKickDegrees = 0.5f;
    Profile.VerticalRandomFraction = 0.0f;
    Profile.HorizontalRandomDegrees = 0.0f;
    Profile.ClimbRampMultiplier = 1.0f;
    Profile.MaxVerticalDegrees = 3.5f;
    Profile.MaxHorizontalDegrees = 1.0f;

    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float AppliedTotal = 0.0f;
    for (int32 Shot = 0; Shot < 10; ++Shot)
    {
        const FBreakerRecoilKick Kick = FBreakerWeaponFeel::ComputeShotKick(Profile, Shot, 7, false);
        const FBreakerRecoilKick Applied = FBreakerWeaponFeel::AccumulateKick(Profile, Kick, Pitch, Yaw);
        AppliedTotal += Applied.PitchDegrees;
        TestTrue(TEXT("Accumulated recoil never exceeds the vertical ceiling"), Pitch <= Profile.MaxVerticalDegrees + KINDA_SMALL_NUMBER);
        TestTrue(TEXT("Accumulated recoil never exceeds the horizontal ceiling"), FMath::Abs(Yaw) <= Profile.MaxHorizontalDegrees + KINDA_SMALL_NUMBER);
    }
    TestEqual(TEXT("Held fire accumulates up to the ceiling"), Pitch, Profile.MaxVerticalDegrees, 0.001f);
    // The applied deltas are exactly the movement of the aim. If these ever
    // diverge from the accumulator, recovery would return the wrong amount and
    // the crosshair would drift away from the trace.
    TestEqual(TEXT("Applied deltas sum to the accumulated budget"), AppliedTotal, Pitch, 0.001f);

    // A partial recovery fraction leaves a permanent share of each kick behind.
    FBreakerRecoilProfile Partial = Profile;
    Partial.RecoveryFraction = 0.5f;
    float PartialPitch = 0.0f;
    float PartialYaw = 0.0f;
    const FBreakerRecoilKick PartialKick = FBreakerWeaponFeel::ComputeShotKick(Partial, 0, 7, false);
    const FBreakerRecoilKick PartialApplied = FBreakerWeaponFeel::AccumulateKick(Partial, PartialKick, PartialPitch, PartialYaw);
    TestEqual(TEXT("Half the kick stays in the recovery budget"), PartialPitch, PartialApplied.PitchDegrees * 0.5f, 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRecoilRecoveryTest,
    "RiorsEdge.Weapons.RecoilRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRecoilRecoveryTest::RunTest(const FString& Parameters)
{
    FBreakerRecoilProfile Profile;
    Profile.RecoveryInterpSpeed = 9.0f;
    Profile.RecoveryConstantDegreesPerSecond = 14.0f;

    // Recovery settles toward the original aim rather than snapping to it.
    float Value = 5.0f;
    const float AfterOneFrame = FBreakerWeaponFeel::RecoverAxis(Profile, Value, 1.0f / 60.0f);
    TestTrue(TEXT("Recovery moves toward zero"), AfterOneFrame < Value);
    TestTrue(TEXT("Recovery does not snap to zero in one frame"), AfterOneFrame > 0.0f);

    int32 Frames = 0;
    while (Value > 0.0f && Frames < 600)
    {
        const float Next = FBreakerWeaponFeel::RecoverAxis(Profile, Value, 1.0f / 60.0f);
        TestTrue(TEXT("Recovery never overshoots below zero"), Next >= 0.0f);
        TestTrue(TEXT("Recovery is monotonic"), Next <= Value);
        Value = Next;
        ++Frames;
    }
    TestEqual(TEXT("Recovery reaches the original aim exactly"), Value, 0.0f);
    TestTrue(TEXT("Recovery finishes in well under a second"), Frames < 60);

    // Negative drift recovers upward with the same rules.
    float Negative = -2.0f;
    for (int32 Frame = 0; Frame < 600 && Negative < 0.0f; ++Frame)
    {
        const float Next = FBreakerWeaponFeel::RecoverAxis(Profile, Negative, 1.0f / 60.0f);
        TestTrue(TEXT("Negative recovery never overshoots above zero"), Next <= 0.0f);
        Negative = Next;
    }
    TestEqual(TEXT("Negative drift recovers to the original aim"), Negative, 0.0f);

    // Manual compensation spends the budget instead of being undone by it.
    TestEqual(TEXT("Pulling down consumes upward recoil"), FBreakerWeaponFeel::ConsumeCompensation(3.0f, -1.0f), 2.0f);
    TestEqual(TEXT("Over-compensating cannot invert the budget"), FBreakerWeaponFeel::ConsumeCompensation(3.0f, -9.0f), 0.0f);
    TestEqual(TEXT("Compensating leftward consumes rightward drift"), FBreakerWeaponFeel::ConsumeCompensation(-2.0f, 1.5f), -0.5f);
    TestEqual(TEXT("Aiming with the recoil is not credited"), FBreakerWeaponFeel::ConsumeCompensation(3.0f, 2.0f), 3.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponBloomTest,
    "RiorsEdge.Weapons.FirstShotAndBloom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponBloomTest::RunTest(const FString& Parameters)
{
    FBreakerRecoilProfile Profile;
    Profile.FirstShotSpreadMultiplier = 0.0f;
    Profile.BloomPerShotDegrees = 0.2f;
    Profile.MaxBloomDegrees = 1.0f;
    Profile.BloomRecoveryDegreesPerSecond = 2.0f;
    Profile.AimBloomMultiplier = 0.5f;

    TestEqual(TEXT("The first shot of a burst is dead accurate"),
        FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, 1.2f, 0.0f, 0), 0.0f);
    TestEqual(TEXT("Later shots pay the base cone plus bloom"),
        FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, 1.2f, 0.4f, 3), 1.6f, 0.001f);
    TestEqual(TEXT("Bloom cannot widen the cone past its ceiling"),
        FBreakerWeaponFeel::EffectiveSpreadDegrees(Profile, 1.2f, 9.0f, 3), 2.2f, 0.001f);

    // Weapons whose identity is their cone keep it on the first shot.
    FBreakerRecoilProfile Scattergun = Profile;
    Scattergun.FirstShotSpreadMultiplier = 1.0f;
    TestEqual(TEXT("A shotgun's first shot keeps its pellet cone"),
        FBreakerWeaponFeel::EffectiveSpreadDegrees(Scattergun, 4.5f, 0.0f, 0), 4.5f);

    float Bloom = 0.0f;
    for (int32 Shot = 0; Shot < 3; ++Shot) Bloom = FBreakerWeaponFeel::BloomAfterShot(Profile, Bloom, false);
    TestEqual(TEXT("Held fire degrades accuracy"), Bloom, 0.6f, 0.001f);
    for (int32 Shot = 0; Shot < 10; ++Shot) Bloom = FBreakerWeaponFeel::BloomAfterShot(Profile, Bloom, false);
    TestEqual(TEXT("Bloom saturates at its ceiling"), Bloom, 1.0f, 0.001f);
    Bloom = FBreakerWeaponFeel::BloomAfterTime(Profile, Bloom, 0.25f);
    TestEqual(TEXT("Bloom bleeds off while the trigger rests"), Bloom, 0.5f, 0.001f);
    Bloom = FBreakerWeaponFeel::BloomAfterTime(Profile, Bloom, 5.0f);
    TestEqual(TEXT("Bloom returns to zero, never below it"), Bloom, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAimDownSightsTest,
    "RiorsEdge.Weapons.AimDownSights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAimDownSightsTest::RunTest(const FString& Parameters)
{
    // ADS is worth using because it tightens the weapon's behaviour on every
    // axis at once: kick, drift, bloom growth, and viewmodel motion.
    FBreakerRecoilProfile Profile;
    Profile.VerticalRandomFraction = 0.0f;
    Profile.HorizontalRandomDegrees = 0.0f;
    Profile.AimRecoilMultiplier = 0.5f;
    Profile.AimBloomMultiplier = 0.5f;
    Profile.AimViewmodelMultiplier = 0.5f;

    const FBreakerRecoilKick Hip = FBreakerWeaponFeel::ComputeShotKick(Profile, 3, 5, false);
    const FBreakerRecoilKick Aimed = FBreakerWeaponFeel::ComputeShotKick(Profile, 3, 5, true);
    TestEqual(TEXT("Aiming halves the vertical kick"), Aimed.PitchDegrees, Hip.PitchDegrees * 0.5f, 0.0001f);
    TestEqual(TEXT("Aiming halves the horizontal drift"), Aimed.YawDegrees, Hip.YawDegrees * 0.5f, 0.0001f);
    TestTrue(TEXT("Aiming still kicks: it tightens the weapon, it does not remove it"), Aimed.PitchDegrees > 0.0f);

    TestEqual(TEXT("Aiming halves bloom growth"),
        FBreakerWeaponFeel::BloomAfterShot(Profile, 0.0f, true),
        FBreakerWeaponFeel::BloomAfterShot(Profile, 0.0f, false) * 0.5f, 0.0001f);

    FBreakerViewmodelState HipView;
    FBreakerViewmodelState AimedView;
    FBreakerWeaponFeel::AddViewmodelKick(Profile, HipView, 1.0f, false);
    FBreakerWeaponFeel::AddViewmodelKick(Profile, AimedView, 1.0f, true);
    TestEqual(TEXT("Aiming halves the viewmodel kick"), AimedView.BackOffset, HipView.BackOffset * 0.5f, 0.0001f);

    // And the archetype table honours it: every weapon's aimed kick is
    // strictly smaller than its hip kick.
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    const EBreakerWeaponArchetype Archetypes[] = {
        EBreakerWeaponArchetype::Rifle, EBreakerWeaponArchetype::SMG, EBreakerWeaponArchetype::Sniper,
        EBreakerWeaponArchetype::Shotgun, EBreakerWeaponArchetype::Rocket };
    for (const EBreakerWeaponArchetype Archetype : Archetypes)
    {
        Weapon->EquipArchetype(Archetype);
        const FBreakerRecoilProfile Live = Weapon->GetRecoilProfile();
        TestTrue(TEXT("Every archetype rewards aiming with less recoil"), Live.AimRecoilMultiplier < 1.0f);
        const FBreakerRecoilKick LiveHip = FBreakerWeaponFeel::ComputeShotKick(Live, 2, 21, false);
        const FBreakerRecoilKick LiveAimed = FBreakerWeaponFeel::ComputeShotKick(Live, 2, 21, true);
        TestTrue(TEXT("Aimed kick is smaller than hip kick on every archetype"), LiveAimed.PitchDegrees < LiveHip.PitchDegrees);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArchetypeRecoilTest,
    "RiorsEdge.Weapons.ArchetypeRecoil",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArchetypeRecoilTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    auto ProfileFor = [Weapon](EBreakerWeaponArchetype Archetype)
    {
        Weapon->EquipArchetype(Archetype);
        return Weapon->GetRecoilProfile();
    };

    const FBreakerRecoilProfile Rifle = ProfileFor(EBreakerWeaponArchetype::Rifle);
    const FBreakerRecoilProfile SMG = ProfileFor(EBreakerWeaponArchetype::SMG);
    const FBreakerRecoilProfile Sniper = ProfileFor(EBreakerWeaponArchetype::Sniper);
    const FBreakerRecoilProfile Shotgun = ProfileFor(EBreakerWeaponArchetype::Shotgun);
    const FBreakerRecoilProfile Rocket = ProfileFor(EBreakerWeaponArchetype::Rocket);

    // Five weapons, five characters: the SMG buzzes, the heavies shove.
    TestTrue(TEXT("The SMG kicks less per shot than the rifle"), SMG.VerticalKickDegrees < Rifle.VerticalKickDegrees);
    TestTrue(TEXT("The SMG wanders sideways more than it climbs"), SMG.HorizontalKickDegrees > SMG.VerticalKickDegrees * 0.5f);
    TestTrue(TEXT("The sniper kicks far harder than the rifle"), Sniper.VerticalKickDegrees > Rifle.VerticalKickDegrees * 3.0f);
    TestTrue(TEXT("The shotgun kicks far harder than the rifle"), Shotgun.VerticalKickDegrees > Rifle.VerticalKickDegrees * 3.0f);
    TestTrue(TEXT("The rocket settles slowest of the five"),
        Rocket.RecoveryInterpSpeed < Rifle.RecoveryInterpSpeed && Rocket.RecoveryInterpSpeed < SMG.RecoveryInterpSpeed);
    TestTrue(TEXT("The SMG settles fastest of the five"), SMG.RecoveryInterpSpeed > Rifle.RecoveryInterpSpeed);
    TestTrue(TEXT("The shotgun keeps its pellet cone on the first shot"), Shotgun.FirstShotSpreadMultiplier >= 1.0f);
    TestEqual(TEXT("The rifle is dead accurate on the first shot"), Rifle.FirstShotSpreadMultiplier, 0.0f);
    TestTrue(TEXT("The heavies shove the viewmodel hardest"),
        Rocket.ViewmodelKickUnits > Rifle.ViewmodelKickUnits && Sniper.ViewmodelKickUnits > Rifle.ViewmodelKickUnits);

    // The global trim is a real dial and only touches the kick.
    Weapon->EquipArchetype(EBreakerWeaponArchetype::Rifle);
    Weapon->RecoilScale = 2.0f;
    const FBreakerRecoilProfile Trimmed = Weapon->GetRecoilProfile();
    TestEqual(TEXT("RecoilScale scales the kick"), Trimmed.VerticalKickDegrees, Rifle.VerticalKickDegrees * 2.0f, 0.0001f);
    TestEqual(TEXT("RecoilScale leaves recovery alone"), Trimmed.RecoveryInterpSpeed, Rifle.RecoveryInterpSpeed);

    // A per-instance override beats the archetype table with no recompile.
    FBreakerRecoilProfile Override;
    Override.VerticalKickDegrees = 12.34f;
    Weapon->RecoilScale = 1.0f;
    Weapon->RecoilOverrides.Add(EBreakerWeaponArchetype::Rifle, Override);
    TestEqual(TEXT("Editor overrides win over the archetype table"), Weapon->GetRecoilProfile().VerticalKickDegrees, 12.34f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerViewmodelKickTest,
    "RiorsEdge.Weapons.ViewmodelKick",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerViewmodelKickTest::RunTest(const FString& Parameters)
{
    FBreakerRecoilProfile Profile;
    Profile.ViewmodelKickUnits = 4.0f;
    Profile.ViewmodelKickLateralUnits = 1.0f;
    Profile.ViewmodelKickPitchDegrees = 3.0f;
    Profile.MaxViewmodelKickUnits = 9.0f;
    Profile.MaxViewmodelKickPitchDegrees = 7.0f;

    FBreakerViewmodelState State;
    TestTrue(TEXT("A weapon that has not fired sits at rest"), State.IsAtRest());
    FBreakerWeaponFeel::AddViewmodelKick(Profile, State, 1.0f, false);
    TestEqual(TEXT("Firing drives the weapon back"), State.BackOffset, 4.0f);
    TestEqual(TEXT("Firing lifts the muzzle"), State.PitchOffset, 3.0f);
    TestEqual(TEXT("Lateral kick follows the horizontal recoil sign"), State.LateralOffset, 1.0f);

    FBreakerViewmodelState Leftward;
    FBreakerWeaponFeel::AddViewmodelKick(Profile, Leftward, -1.0f, false);
    TestEqual(TEXT("Leftward drift shoves the weapon left"), Leftward.LateralOffset, -1.0f);

    for (int32 Shot = 0; Shot < 20; ++Shot) FBreakerWeaponFeel::AddViewmodelKick(Profile, State, 1.0f, false);
    TestTrue(TEXT("Sustained fire cannot walk the weapon off screen"), State.BackOffset <= Profile.MaxViewmodelKickUnits + KINDA_SMALL_NUMBER);
    TestTrue(TEXT("Sustained fire cannot walk the muzzle off screen"), State.PitchOffset <= Profile.MaxViewmodelKickPitchDegrees + KINDA_SMALL_NUMBER);

    int32 Frames = 0;
    while (!State.IsAtRest() && Frames < 600)
    {
        FBreakerWeaponFeel::IntegrateViewmodel(Profile, State, 1.0f / 60.0f);
        ++Frames;
    }
    TestTrue(TEXT("The weapon springs back to rest"), State.IsAtRest());
    TestTrue(TEXT("The recovery is fast, not floaty"), Frames < 90);
    return true;
}

// ---------------------------------------------------------------------------
// Tracer flight. The shot is still hitscan — these tests are about the round
// APPEARING to travel, which is the whole difference between a bullet and a
// laser. The drawing is untestable; the maths behind it is not.
//
// UPDATED with the second visual pass. Three of the old assertions are gone
// because the things they asserted are gone, not because they were in the way:
//   * ImpactBasis — the impact was a six-spoke star drawn in the plane
//     perpendicular to travel. It is a point flash now, which has no plane and
//     therefore no basis to test.
//   * WorldRadiusToPixels — the round was a canvas line whose PIXEL width was
//     derived from its depth. It is a world primitive now, so it has a world
//     thickness and the depth maths belongs to the renderer; the replacement
//     is TracerThicknessCm, tested below.
// What replaces them is the head/trail split, the shortened streak, the
// screen-width floor, and tracer cadence.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTracerFlightTest,
    "RiorsEdge.Weapons.TracerFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTracerFlightTest::RunTest(const FString& Parameters)
{
    const BreakerHUD::FTracerFlight Flight;
    const FVector Muzzle(0.0f, 0.0f, 0.0f);

    // Flight time is clamped into the readable band at both extremes: a
    // point-blank shotgun round still occupies frames, a sniper round across
    // the field does not float.
    const float PointBlank = BreakerHUD::TracerFlightSeconds(Flight, 150.0f);
    const float LongShot = BreakerHUD::TracerFlightSeconds(Flight, 12000.0f);
    TestTrue(TEXT("A point-blank round is on screen for at least the floor"),
        PointBlank >= Flight.MinFlightSeconds - KINDA_SMALL_NUMBER);
    TestTrue(TEXT("A long round never exceeds the flight ceiling"),
        LongShot <= Flight.MaxFlightSeconds + KINDA_SMALL_NUMBER);
    TestTrue(TEXT("A longer shot still takes longer than a short one"), LongShot > PointBlank);

    // A mid-range shot: the streak must actually move downrange between two
    // samples, and never reach the wall early.
    const FVector Impact(3000.0f, 0.0f, 0.0f);
    const float Total = BreakerHUD::TracerFlightSeconds(Flight, 3000.0f);
    const BreakerHUD::FTracerSample Early = BreakerHUD::SampleTracer(Flight, Muzzle, Impact, Total * 0.25f);
    const BreakerHUD::FTracerSample Late = BreakerHUD::SampleTracer(Flight, Muzzle, Impact, Total * 0.75f);
    TestTrue(TEXT("The round is visible while it is in flight"), Early.bVisible && Late.bVisible);
    TestTrue(TEXT("The round travels downrange"), Late.Head.X > Early.Head.X);
    TestTrue(TEXT("The head never overshoots the impact"), Late.Head.X <= Impact.X + KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The tail never trails behind the muzzle"), Early.Tail.X >= -KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The streak is short, not a beam from muzzle to wall"),
        (Late.Head - Late.Tail).Size() <= Flight.LengthCm + KINDA_SMALL_NUMBER);

    // Arrival hands over to the impact burst rather than lingering.
    const BreakerHUD::FTracerSample Landed = BreakerHUD::SampleTracer(Flight, Muzzle, Impact, Total + 0.01f);
    TestTrue(TEXT("The round reports arrival"), Landed.bArrived);
    TestFalse(TEXT("A landed round draws nothing"), Landed.bVisible);

    // Muzzle and impact on top of one another would be a dot with a random
    // direction; that draws nothing at all.
    const BreakerHUD::FTracerSample Contact =
        BreakerHUD::SampleTracer(Flight, Muzzle, FVector(20.0f, 0.0f, 0.0f), 0.01f);
    TestFalse(TEXT("A contact-range shot draws no streak"), Contact.bVisible);

    // --- Head and trail -----------------------------------------------------
    // The streak is not a uniform bar. Most of its brightness is in a short
    // head at the front, with a dim trail behind; the renderer draws those as
    // two primitives, so the split has to be well ordered at every age.
    TestTrue(TEXT("The head section sits between the tail and the head"),
        Late.HeadStart.X >= Late.Tail.X - KINDA_SMALL_NUMBER &&
        Late.HeadStart.X <= Late.Head.X + KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The bright head is never longer than its authored length"),
        (Late.Head - Late.HeadStart).Size() <= Flight.HeadLengthCm + KINDA_SMALL_NUMBER);
    TestTrue(TEXT("A settled round has a trail behind its head"),
        BreakerHUD::TracerHasTrail(Late));

    // On the first frames the whole round IS the head: the trail has not been
    // paid out of the muzzle yet, and drawing a degenerate one would be a
    // flickering sliver at the barrel.
    const BreakerHUD::FTracerSample JustLeft =
        BreakerHUD::SampleTracer(Flight, Muzzle, Impact, Flight.HeadLengthCm * 0.5f / Flight.SpeedCms);
    if (JustLeft.bVisible)
    {
        TestFalse(TEXT("A round that has only just left has no trail yet"),
            BreakerHUD::TracerHasTrail(JustLeft));
    }

    // The streak is SHORT. Nine metres was the previous authored length and it
    // read as a rod; this is the assertion that stops it drifting back.
    TestTrue(TEXT("The whole streak is under three metres"), Flight.LengthCm <= 300.0f);
    TestTrue(TEXT("Most of the streak is trail, not head"),
        Flight.HeadLengthCm < Flight.LengthCm * 0.5f);

    // --- Screen-width floor -------------------------------------------------
    // The round is a world primitive now, so its thickness is world
    // centimetres and a far round would be sub-pixel and strobe. The floor
    // widens it in world space only as far as it must, and leaves near rounds
    // exactly as authored.
    const BreakerHUD::FTracerLook Look;
    const float HalfFOV = FMath::DegreesToRadians(30.0f);
    const float NearThickness = BreakerHUD::TracerThicknessCm(Look.ThicknessCm, 200.0f, Look.MinScreenFraction, HalfFOV);
    const float FarThickness = BreakerHUD::TracerThicknessCm(Look.ThicknessCm, 8000.0f, Look.MinScreenFraction, HalfFOV);
    TestTrue(TEXT("A near round keeps its authored world thickness"),
        FMath::IsNearlyEqual(NearThickness, Look.ThicknessCm, 0.01f));
    TestTrue(TEXT("A far round is widened rather than left sub-pixel"), FarThickness > NearThickness);
    // Once the floor is doing the work, thickness is linear in distance, which
    // is exactly what holds the on-screen width constant.
    const float FarerThickness = BreakerHUD::TracerThicknessCm(Look.ThicknessCm, 16000.0f, Look.MinScreenFraction, HalfFOV);
    TestTrue(TEXT("Beyond the floor, world thickness scales with distance"),
        FMath::IsNearlyEqual(FarerThickness / FarThickness, 2.0f, 0.01f));

    // --- Cadence ------------------------------------------------------------
    // Every round used to leave a streak, which is what made held automatic
    // fire read as one continuous beam. Fast weapons now trace one in three;
    // slow ones trace every round, because a bolt-action that skipped two
    // shots in three would read as broken rather than as restrained.
    const int32 RifleCadence = BreakerHUD::TracerRoundsPerTracer(600.0f);
    const int32 SniperCadence = BreakerHUD::TracerRoundsPerTracer(55.0f);
    TestTrue(TEXT("A fast weapon traces a fraction of its rounds"), RifleCadence > 1);
    TestEqual(TEXT("A slow weapon traces every round"), SniperCadence, 1);

    int32 Traced = 0;
    for (int32 Round = 0; Round < 30; ++Round)
    {
        if (BreakerHUD::ShouldTraceRound(Round, RifleCadence)) ++Traced;
    }
    TestEqual(TEXT("Thirty rifle rounds leave ten streaks"), Traced, 10);
    TestTrue(TEXT("The first round of a burst always traces"),
        BreakerHUD::ShouldTraceRound(0, RifleCadence));

    Traced = 0;
    for (int32 Round = 0; Round < 8; ++Round)
    {
        if (BreakerHUD::ShouldTraceRound(Round, SniperCadence)) ++Traced;
    }
    TestEqual(TEXT("Every slow round leaves a streak"), Traced, 8);
    return true;
}

// ---------------------------------------------------------------------------
// Power-Curve.md §3 — base weapon damage as a function of item level.
//
// The owner's report was that full level 50 gear does not feel significant.
// The confirmed cause: Weapons/ contained no reference to ItemLevel at all, so
// the multiplicand every affix, node and crit multiplies was an archetype
// constant. These tests pin the SHAPE of the fix, never its balance numbers:
// the curve is geometric, item level 1 is exactly the authored archetype
// number, and the five archetypes stay five different weapons at every level.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponItemLevelCurveTest,
    "RiorsEdge.Weapons.ItemLevelCurve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponItemLevelCurveTest::RunTest(const FString& Parameters)
{
    const float W = 0.09f;

    // Item level 1 is the anchor: the archetype table's numbers keep meaning
    // exactly what they mean today, for any growth rate at all.
    TestEqual(TEXT("Item level 1 is exactly the authored base"), FBreakerWeaponMath::ItemLevelDamageScalar(1, W), 1.0f);
    TestEqual(TEXT("A zero growth restores the flat pre-curve behaviour"), FBreakerWeaponMath::ItemLevelDamageScalar(50, 0.0f), 1.0f);

    // Geometric, not linear: (1 + w)^(ilvl - 1), checked against the closed form.
    for (const int32 Level : { 2, 10, 25, 50 })
    {
        TestEqual(*FString::Printf(TEXT("The curve is (1+w)^(ilvl-1) at level %d"), Level),
            FBreakerWeaponMath::ItemLevelDamageScalar(Level, W),
            FMath::Pow(1.0f + W, static_cast<float>(Level - 1)),
            0.001f);
    }

    // Strictly monotone across the whole authored 1-50 range, so a higher item
    // level is never a downgrade.
    float Previous = FBreakerWeaponMath::ItemLevelDamageScalar(1, W);
    for (int32 Level = 2; Level <= 50; ++Level)
    {
        const float Current = FBreakerWeaponMath::ItemLevelDamageScalar(Level, W);
        TestTrue(TEXT("Every item level is a strict improvement"), Current > Previous);
        Previous = Current;
    }

    // At w = 9% the fifty-level climb is roughly 67x, matching the monster
    // health curve's own stated span. This is the number the doc's "baseline
    // TTK holds constant" claim rests on.
    TestTrue(TEXT("Fifty levels is a large multiple, not a rounding error"),
        FBreakerWeaponMath::ItemLevelDamageScalar(50, W) > 50.0f);

    // Garbage in cannot produce garbage out: sub-1 levels clamp to the anchor
    // and the ceiling keeps an absurd input finite.
    TestEqual(TEXT("Item level 0 clamps to the level 1 anchor"), FBreakerWeaponMath::ItemLevelDamageScalar(0, W), 1.0f);
    TestEqual(TEXT("A negative item level clamps to the level 1 anchor"), FBreakerWeaponMath::ItemLevelDamageScalar(-40, W), 1.0f);
    TestTrue(TEXT("An absurd item level stays finite"), FMath::IsFinite(FBreakerWeaponMath::ItemLevelDamageScalar(1000000, W)));

    // The base-damage wrapper is the scalar times the archetype constant.
    TestEqual(TEXT("Base damage is the archetype number at level 1"), FBreakerWeaponMath::WeaponBaseDamage(72.0f, 1, W), 72.0f);
    TestEqual(TEXT("Base damage rides the scalar"),
        FBreakerWeaponMath::WeaponBaseDamage(72.0f, 30, W), 72.0f * FBreakerWeaponMath::ItemLevelDamageScalar(30, W), 0.01f);
    TestEqual(TEXT("A negative archetype base clamps to zero"), FBreakerWeaponMath::WeaponBaseDamage(-10.0f, 30, W), 0.0f);
    return true;
}

// The reason `w` exists at all: it is chosen to track the monster health growth
// `g` from the same document. This test states the consequence rather than the
// value — with w == g a baseline build's shots-to-kill is level-invariant, and
// any divergence between them is a drift in baseline TTK per level.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponItemLevelTracksMonsterHealthTest,
    "RiorsEdge.Weapons.ItemLevelTracksMonsterHealth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponItemLevelTracksMonsterHealthTest::RunTest(const FString& Parameters)
{
    // `g`, the monster health growth, is owned by Combat/ and is not read from
    // here on purpose: this test asserts the RELATIONSHIP, so it keeps its
    // meaning whatever the two layers are eventually tuned to.
    const float G = 0.09f;
    const float W = 0.09f;
    const float BaseHealth = 220.0f;   // the current flat trash health
    const float BaseDamage = 13.0f;    // the rifle's archetype constant

    auto ShotsToKill = [&](int32 Level)
    {
        const float Health = BaseHealth * FMath::Pow(1.0f + G, static_cast<float>(Level - 1));
        return Health / FBreakerWeaponMath::WeaponBaseDamage(BaseDamage, Level, W);
    };

    const float AtOne = ShotsToKill(1);
    TestEqual(TEXT("Baseline shots-to-kill holds at level 25"), ShotsToKill(25), AtOne, AtOne * 0.01f);
    TestEqual(TEXT("Baseline shots-to-kill holds at level 50"), ShotsToKill(50), AtOne, AtOne * 0.01f);

    // And the failure modes, stated as tests so a future retune cannot drift
    // one curve without noticing what it does to the other. If w < g the game
    // outruns the player and a baseline build slowly stops being able to kill
    // anything; if w > g baseline TTK falls with level and the multiplier band
    // has nothing left to add.
    auto ShotsToKillWith = [&](int32 Level, float LocalW)
    {
        const float Health = BaseHealth * FMath::Pow(1.0f + G, static_cast<float>(Level - 1));
        return Health / FBreakerWeaponMath::WeaponBaseDamage(BaseDamage, Level, LocalW);
    };
    TestTrue(TEXT("w below g makes baseline TTK climb with level"), ShotsToKillWith(50, G - 0.02f) > AtOne);
    TestTrue(TEXT("w above g makes baseline TTK fall with level"), ShotsToKillWith(50, G + 0.02f) < AtOne);
    return true;
}

// The archetypes must remain five different weapons at every point on the
// curve. A single shared exponent guarantees it, and this pins that: a common
// ratio preserves ordering, so a sniper out-hits an SMG per shot at level 1 and
// at level 50 by the same factor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponArchetypeOrderingAcrossLevelsTest,
    "RiorsEdge.Weapons.ArchetypeOrderingAcrossLevels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponArchetypeOrderingAcrossLevelsTest::RunTest(const FString& Parameters)
{
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>();
    const float W = Weapon->ItemLevelDamageGrowth;

    // Per SHOT, not per pellet: the shotgun's 10 is eight pellets of it, and
    // comparing pellet to bullet would be comparing the wrong things.
    auto DamagePerShot = [Weapon, W](EBreakerWeaponArchetype Archetype, int32 Level)
    {
        Weapon->EquipArchetype(Archetype);
        const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition();
        return FBreakerWeaponMath::WeaponBaseDamage(Definition->Damage, Level, W)
            * static_cast<float>(FMath::Max(1, Definition->PelletsPerShot));
    };

    for (int32 Level = 1; Level <= 50; ++Level)
    {
        const float Rifle = DamagePerShot(EBreakerWeaponArchetype::Rifle, Level);
        const float SMG = DamagePerShot(EBreakerWeaponArchetype::SMG, Level);
        const float Sniper = DamagePerShot(EBreakerWeaponArchetype::Sniper, Level);
        const float Shotgun = DamagePerShot(EBreakerWeaponArchetype::Shotgun, Level);
        const float Rocket = DamagePerShot(EBreakerWeaponArchetype::Rocket, Level);

        TestTrue(TEXT("A sniper out-hits an SMG per shot at every item level"), Sniper > SMG);
        TestTrue(TEXT("A sniper out-hits a rifle per shot at every item level"), Sniper > Rifle);
        TestTrue(TEXT("A shotgun blast out-hits a rifle round at every item level"), Shotgun > Rifle);
        TestTrue(TEXT("A rocket out-hits an SMG round at every item level"), Rocket > SMG);
        TestTrue(TEXT("Every archetype does more damage than it did at level 1"),
            Level == 1 || SMG > DamagePerShot(EBreakerWeaponArchetype::SMG, 1));
    }

    // The ratio is level-invariant, which is the actual property: a shared
    // exponent scales the table without reshaping it.
    const float RatioAtOne = DamagePerShot(EBreakerWeaponArchetype::Sniper, 1) / DamagePerShot(EBreakerWeaponArchetype::SMG, 1);
    const float RatioAtFifty = DamagePerShot(EBreakerWeaponArchetype::Sniper, 50) / DamagePerShot(EBreakerWeaponArchetype::SMG, 50);
    TestEqual(TEXT("The archetype table keeps its shape across fifty levels"), RatioAtFifty, RatioAtOne, 0.001f);
    return true;
}

// How item level actually reaches the weapon, and what an unequipped weapon is.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponEquippedItemLevelTest,
    "RiorsEdge.Weapons.EquippedItemLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWeaponEquippedItemLevelTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerWeaponComponent* Weapon = NewObject<UBreakerWeaponComponent>(Owner);
    const float SlotOneBase = Weapon->GetActiveDefinition()->Damage;

    // No equipment component at all — the clean-clone, zero-setup case. A
    // weapon with nothing equipped is a level 1 weapon, so the archetype
    // numbers are unchanged and every previously measured TTK still holds.
    TestEqual(TEXT("An unequipped weapon is item level 1"), Weapon->GetEquippedItemLevel(), 1);
    TestEqual(TEXT("An unequipped weapon deals exactly its archetype damage"), Weapon->GetScaledBaseDamage(), SlotOneBase);

    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);

    // An equipment component with empty weapon slots is still the level 1 case.
    TestEqual(TEXT("An empty Primary slot is still item level 1"), Weapon->GetEquippedItemLevel(), 1);

    auto MakeWeaponItem = [](EBreakerEquipSlot Slot, int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = EBreakerItemRarity::Exceptional;
        Item.ItemLevel = ItemLevel;
        return Item;
    };

    // Weapon loadout slot 1 reads the Primary equipment slot, slot 2 the
    // Secondary. That correspondence is positional and is the only link the two
    // layers have today.
    TestTrue(TEXT("A Primary weapon equips"), Equipment->EquipItem(MakeWeaponItem(EBreakerEquipSlot::Primary, 50)));
    TestTrue(TEXT("A Secondary weapon equips"), Equipment->EquipItem(MakeWeaponItem(EBreakerEquipSlot::Secondary, 20)));

    TestEqual(TEXT("Weapon slot 1 reads the Primary item's level"), Weapon->GetEquippedItemLevel(), 50);
    TestEqual(TEXT("A level 50 weapon hits far harder than a level 1 one"),
        Weapon->GetScaledBaseDamage(),
        SlotOneBase * FBreakerWeaponMath::ItemLevelDamageScalar(50, Weapon->ItemLevelDamageGrowth),
        0.01f);
    TestTrue(TEXT("Full level 50 gear is a large, felt base increase"), Weapon->GetScaledBaseDamage() > SlotOneBase * 10.0f);

    // Swapping to the second loadout slot reads the second item, so the two
    // guns are not silently the same power.
    Weapon->EquipSlot(2);
    TestEqual(TEXT("Weapon slot 2 reads the Secondary item's level"), Weapon->GetEquippedItemLevel(), 20);
    TestEqual(TEXT("The stowed weapon's item level is genuinely its own"),
        Weapon->GetItemLevelDamageScalar(),
        FBreakerWeaponMath::ItemLevelDamageScalar(20, Weapon->ItemLevelDamageGrowth),
        0.01f);

    // Unequipping falls back to the archetype's own level, not to the last
    // item's, so a stale reading cannot survive an empty slot.
    Weapon->EquipSlot(1);
    TestTrue(TEXT("The Primary unequips"), Equipment->UnequipSlot(EBreakerEquipSlot::Primary));
    TestEqual(TEXT("An emptied slot returns to item level 1"), Weapon->GetEquippedItemLevel(), 1);
    return true;
}

#endif
