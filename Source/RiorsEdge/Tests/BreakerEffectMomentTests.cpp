#include "Misc/AutomationTest.h"
#include "UI/BreakerEffectMomentMath.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

#if WITH_DEV_AUTOMATION_TESTS

// The four moments (GLASS-1). What a headless suite can prove: the colour law
// is O179's, the four assets resolve to four distinct places under one
// directory, and the pooled fallback that stands in for an unauthored system
// has a schedule a frame can sample. Whether a Niagara system PLAYS is the
// capture harness's job once the owner has authored one.

namespace
{
    const EBreakerEffectMoment BreakerMomentAll[BreakerFX::EffectMomentCount] = {
        EBreakerEffectMoment::Muzzle, EBreakerEffectMoment::Impact,
        EBreakerEffectMoment::Cast, EBreakerEffectMoment::Death,
    };

    bool BreakerMomentSameColor(const FLinearColor& A, const FLinearColor& B)
    {
        return A.Equals(B, 1.0e-4f);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentColourLawTest,
    "RiorsEdge.UI.EffectMoment.ColourLaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentColourLawTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFX;

    // O179: weapon economy is the weapon/heat role.
    TestTrue(TEXT("Muzzle is Orange"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Muzzle, false), BreakerUI::Orange));
    TestTrue(TEXT("A muzzle never promotes to Gold"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Muzzle, true), BreakerUI::Orange));
    TestTrue(TEXT("Impact is Orange"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Impact, false), BreakerUI::Orange));
    // O179: the weak-point promise is the reward role.
    TestTrue(TEXT("A weak-point impact is Gold"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Impact, true), BreakerUI::Gold));
    // Death wears the kill confirm's colours (BreakerPlaytestHUD's crosshair
    // confirm): one event, one colour on the HUD and in the world.
    TestTrue(TEXT("Death is Harm"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Death, false), BreakerUI::Harm));
    TestTrue(TEXT("A weak-point kill is Gold"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Death, true), BreakerUI::Gold));
    // Cast's default is the player/system role; the site supplies the verb.
    TestTrue(TEXT("Cast defaults to Cyan"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Cast, false), BreakerUI::Cyan));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentAssetPathsTest,
    "RiorsEdge.UI.EffectMoment.AssetPathsDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentAssetPathsTest::RunTest(const FString& Parameters)
{
    TSet<FString> Seen;
    for (EBreakerEffectMoment Moment : BreakerMomentAll)
    {
        const FString Path = BreakerFX::MomentAssetPath(Moment);
        TestTrue(FString::Printf(TEXT("%s lives under /Game/Breaker/FX/"), *Path),
            Path.StartsWith(TEXT("/Game/Breaker/FX/NS_")));
        // Object path form: /Game/Dir/Name.Name, what LoadObject takes.
        const FString Name = BreakerFX::MomentAssetName(Moment);
        TestTrue(FString::Printf(TEXT("%s names its object"), *Path),
            Path.EndsWith(FString::Printf(TEXT("/%s.%s"), *Name, *Name)));
        TestFalse(FString::Printf(TEXT("%s is not shared with another moment"), *Path), Seen.Contains(Path));
        Seen.Add(Path);
    }
    TestEqual(TEXT("Four moments, four assets"), Seen.Num(), BreakerFX::EffectMomentCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentFallbackTest,
    "RiorsEdge.UI.EffectMoment.FallbackSchedule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentFallbackTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFX;

    // O282: the impact draws a shard burst from primitives until NS_Impact is
    // authored. The tracer's spark is a point; the burst is the hit's
    // direction, and the two do not double-draw because the burst has no disc.
    TestTrue(TEXT("Impact draws a shard burst"), MomentFallback(EBreakerEffectMoment::Impact).bDrawn);
    TestTrue(TEXT("Death draws a fallback"), MomentFallback(EBreakerEffectMoment::Death).bDrawn);

    for (EBreakerEffectMoment Moment : BreakerMomentAll)
    {
        const FMomentFallback F = MomentFallback(Moment);
        if (!F.bDrawn) continue;
        TestTrue(TEXT("A drawn fallback lasts"), F.Timing.DurationSeconds > 0.0f);
        // A drawn fallback is SOMETHING: a disc the renderer will not hide
        // (it drops anything under half a centimetre), a tongue, or a burst
        // of shards. The muzzle is the tongue case and the impact the shard
        // case — their discs are zero on purpose.
        TestTrue(TEXT("A drawn fallback has size"), F.RadiusCm >= 0.5f || F.TongueCm > 0.0f || F.ShardCount > 0);
        TestTrue(TEXT("A drawn fallback has light"), F.Intensity > 0.0f);
        TestTrue(TEXT("Fades fit inside the clip"),
            F.Timing.FadeInSeconds + F.Timing.FadeOutSeconds <= F.Timing.DurationSeconds + KINDA_SMALL_NUMBER);
        // A frame in the middle of the clip sees it.
        const FEffectSample Mid = SampleEffect(F.Timing, F.Timing.DurationSeconds * 0.5f);
        TestTrue(TEXT("Visible at half life"), Mid.bVisible && Mid.Alpha > 0.0f);
        // And a frame after it does not.
        TestTrue(TEXT("Gone after the clip"), SampleEffect(F.Timing, F.Timing.DurationSeconds + 0.001f).bFinished);
        // A blink light, when asked for, is a real light.
        if (F.LightRadiusCm > 0.0f) TestTrue(TEXT("A lit fallback has intensity"), F.LightIntensity > 0.0f);
    }

    // Shipped configuration: the pools exist and the pending ring can hold a
    // full spread's landed pellets plus the kill they made.
    TestTrue(TEXT("Moment pool is not empty"), ABreakerEffectRenderer::GetMomentSlots() > 0);
    TestTrue(TEXT("Pending ring holds eight pellets and a death"), ABreakerEffectRenderer::GetPendingMomentSlots() >= 9);
    return true;
}

// The muzzle draws a tongue and a light until NS_Muzzle is authored; no disc
// stand-in. The muzzle is the one moment drawn at a fixed offset from the
// player's own camera, so its size is a screen question: a flash must not
// reach the reticle at either shipped muzzle offset (GLASS-5, O179's camera
// law). MuzzleFallbackRadiusCeilingCm is the size law any future fallback
// must pass; this test proves the law on synthetic offsets and that the
// shipped configuration draws nothing at the muzzle.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentMuzzleReticleTest,
    "RiorsEdge.UI.EffectMoment.MuzzleClearsReticle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentMuzzleReticleTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFX;
    const float Clearance = MuzzleReticleClearanceRadians;
    TestTrue(TEXT("Clearance is a real gap"), Clearance > 0.0f);

    // The rule, on synthetic offsets.
    TestEqual(TEXT("A muzzle dead down the axis has no room"),
        MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 0.0f, 0.0f), Clearance), 0.0f);
    TestEqual(TEXT("A muzzle behind the camera has no room"),
        MuzzleFallbackRadiusCeilingCm(FVector(-95.0f, 18.0f, -18.0f), Clearance), 0.0f);
    // An offset sitting exactly at the clearance angle leaves nothing to draw.
    const float AtClearanceLateral = 95.0f * FMath::Tan(Clearance);
    TestTrue(TEXT("A muzzle on the clearance edge has no room"),
        MuzzleFallbackRadiusCeilingCm(FVector(95.0f, AtClearanceLateral, 0.0f), Clearance) <= KINDA_SMALL_NUMBER);
    // Further off axis at the same distance: more room.
    const float Near = MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 6.0f, 0.0f), Clearance);
    const float Far = MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 18.0f, 0.0f), Clearance);
    TestTrue(TEXT("Off-axis muzzle earns room"), Near > 0.0f && Far > Near);
    // Same angle, twice the distance: twice the room (the disc subtends the same angle).
    const float Once = MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 18.0f, -18.0f), Clearance);
    const float Twice = MuzzleFallbackRadiusCeilingCm(FVector(190.0f, 36.0f, -36.0f), Clearance);
    TestTrue(TEXT("Room scales with distance at a fixed angle"), FMath::IsNearlyEqual(Twice, Once * 2.0f, 1.0e-3f));
    // The lateral axes are interchangeable: right and down are one distance off axis.
    TestTrue(TEXT("Right and down are the same offset"), FMath::IsNearlyEqual(
        MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 18.0f, 0.0f), Clearance),
        MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 0.0f, -18.0f), Clearance), 1.0e-3f));

    // Shipped configuration: the muzzle DRAWS now (a tongue and a light), and
    // it draws NO DISC, because the largest disc that clears the reticle at
    // the aimed muzzle offset (95, 2, -6) is under four centimetres. A
    // tongue down the barrel from an off-axis muzzle converges toward the
    // axis and never reaches it; the death fallback is untouched by any of
    // this.
    const FMomentFallback Muzzle = MomentFallback(EBreakerEffectMoment::Muzzle);
    TestTrue(TEXT("The muzzle fallback is on"), Muzzle.bDrawn);
    TestEqual(TEXT("and draws no disc"), Muzzle.RadiusCm, 0.0f);
    TestTrue(TEXT("but a tongue"), Muzzle.TongueCm > 0.0f && Muzzle.TongueThicknessCm > 0.0f);
    TestTrue(TEXT("and a light"), Muzzle.LightRadiusCm > 0.0f);
    // A disc would not have cleared: the ceiling at the aimed offset is tiny.
    TestTrue(TEXT("The aimed offset leaves no room for a disc"),
        MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 2.0f, -6.0f), Clearance) < 5.0f);
    // The tongue's thickness is inside even that, so its near end cannot
    // reach the reticle at the aimed offset either.
    TestTrue(TEXT("The tongue is thinner than the aimed ceiling"),
        Muzzle.TongueThicknessCm * 0.5f < MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 2.0f, -6.0f), Clearance));
    // O282: the flare at the muzzle's root is scaled by the weapon's kick.
    // The largest shipped kick is the sniper's ten units; even that flare,
    // at its base radius, has to sit inside the same ceiling the disc failed.
    TestTrue(TEXT("The flare base is a real size"), Muzzle.FlareBaseRadiusCm > 0.0f && Muzzle.FlareLengthCm > 0.0f);
    TestTrue(TEXT("Kick grows the flare"), MuzzleFlashScale(10.0f) > MuzzleFlashScale(0.0f));
    TestTrue(TEXT("A gun that kicks nothing still flashes"), MuzzleFlashScale(0.0f) > 0.0f);
    TestTrue(TEXT("The heaviest kick's flare clears the reticle at the aimed offset"),
        MuzzleFlashScale(10.0f) * Muzzle.FlareBaseRadiusCm < MuzzleFallbackRadiusCeilingCm(FVector(95.0f, 2.0f, -6.0f), Clearance));
    // And it is over before the next shot at any shipped cadence (900 RPM is
    // 0.067 s).
    TestTrue(TEXT("A flash does not outlive a shot interval"), Muzzle.Timing.DurationSeconds <= 60.0f / 900.0f);
    TestTrue(TEXT("Death keeps its fallback"), MomentFallback(EBreakerEffectMoment::Death).bDrawn);
    return true;
}

// O282: the impact's shard burst is a pure function of (normal, index, age),
// so the whole flight is provable on floats: every shard leaves the hit
// point, leaves it on the hit's side, inside the cone, and falls under the
// same gravity from then on. Whether it READS as a hit is the capture
// harness's job.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentShardBurstTest,
    "RiorsEdge.UI.EffectMoment.ShardBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentShardBurstTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFX;
    const FMomentFallback Impact = MomentFallback(EBreakerEffectMoment::Impact);
    TestTrue(TEXT("The impact has shards"), Impact.ShardCount > 0);
    TestTrue(TEXT("The shards fly"), Impact.ShardSpeedCms > 0.0f);
    TestTrue(TEXT("The shards are drawn"), Impact.ShardLengthCm > 0.0f && Impact.ShardThicknessCm > 0.0f);
    TestTrue(TEXT("The cone is a real half-space wedge"), Impact.ShardConeDegrees > 0.0f && Impact.ShardConeDegrees <= 90.0f);

    // A floor hit, a wall hit, and a slanted one: the rule holds for any
    // surface, not only the one a floor test would pick.
    const FVector Normals[] = {
        FVector::UpVector,
        FVector(-1.0f, 0.0f, 0.0f),
        FVector(0.6f, 0.0f, 0.8f).GetSafeNormal(),
    };
    // ShardConeDegrees is the FULL included angle, so a launch direction is
    // within half of it of the normal.
    const float ConeCos = FMath::Cos(FMath::DegreesToRadians(Impact.ShardConeDegrees * 0.5f));
    // The gravity dial is a magnitude here whatever sign the header stores
    // it with: the assertion is that the shard FALLS.
    const float Gravity = FMath::Abs(Impact.ShardGravityCms2);
    TestTrue(TEXT("The shards fall"), Gravity > 0.0f);
    for (const FVector& Normal : Normals)
    {
        for (int32 Index = 0; Index < Impact.ShardCount; ++Index)
        {
            // Born at the hit.
            FVector BirthPos, BirthDir;
            ShardPose(Normal, Index, Impact.ShardCount, 0.0f, Impact.ShardSpeedCms, Impact.ShardGravityCms2, Impact.ShardConeDegrees, BirthPos, BirthDir);
            TestTrue(FString::Printf(TEXT("Shard %d starts at the hit"), Index), BirthPos.IsNearlyZero(0.01));
            TestTrue(FString::Printf(TEXT("Shard %d has a direction"), Index), BirthDir.IsNormalized());
            // Leaves on the hit's side, inside the cone.
            const float Dot = static_cast<float>(FVector::DotProduct(BirthDir, Normal));
            TestTrue(FString::Printf(TEXT("Shard %d leaves the surface"), Index), Dot >= -KINDA_SMALL_NUMBER);
            TestTrue(FString::Printf(TEXT("Shard %d stays inside the cone"), Index), Dot >= ConeCos - 1.0e-3f);

            // Ballistic from then on: the straight-line flight minus half g t
            // squared, sampled across the clip.
            for (float Age = 0.05f; Age <= Impact.ShardSeconds + KINDA_SMALL_NUMBER; Age += 0.05f)
            {
                FVector Pos, Dir;
                ShardPose(Normal, Index, Impact.ShardCount, Age, Impact.ShardSpeedCms, Impact.ShardGravityCms2, Impact.ShardConeDegrees, Pos, Dir);
                const FVector Line = BirthDir * Impact.ShardSpeedCms * Age;
                TestTrue(FString::Printf(TEXT("Shard %d flies straight in the ground plane at %.2fs"), Index, Age),
                    FMath::IsNearlyEqual(static_cast<float>(Pos.X), static_cast<float>(Line.X), 0.01f)
                    && FMath::IsNearlyEqual(static_cast<float>(Pos.Y), static_cast<float>(Line.Y), 0.01f));
                TestTrue(FString::Printf(TEXT("Shard %d falls under gravity at %.2fs"), Index, Age),
                    FMath::IsNearlyEqual(static_cast<float>(Pos.Z), static_cast<float>(Line.Z) - 0.5f * Gravity * Age * Age, 0.01f));

                // Deterministic per index: the same call twice is the same shard.
                FVector Again, AgainDir;
                ShardPose(Normal, Index, Impact.ShardCount, Age, Impact.ShardSpeedCms, Impact.ShardGravityCms2, Impact.ShardConeDegrees, Again, AgainDir);
                TestTrue(FString::Printf(TEXT("Shard %d repeats at %.2fs"), Index, Age), Again.Equals(Pos, 0.0) && AgainDir.Equals(Dir, 0.0));
            }
        }
    }

    // Shipped configuration: the pool holds a full shotgun spread's worth of
    // bursts landing in one frame, and a burst is over before the eye has
    // finished reading the hit.
    TestTrue(TEXT("The shard pool holds eight bursts"), ABreakerEffectRenderer::GetShardSlots() >= 8 * Impact.ShardCount);
    TestTrue(TEXT("A burst is brief"), Impact.ShardSeconds > 0.0f && Impact.ShardSeconds <= 0.5f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
