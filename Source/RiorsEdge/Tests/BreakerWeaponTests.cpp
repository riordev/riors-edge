#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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

#endif
