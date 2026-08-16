#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemy.h"
#include "Progression/BreakerExperience.h"

// ---------------------------------------------------------------------------
// THE XP LOOP — the condition O40(b) was waiting on
// ---------------------------------------------------------------------------
// O40(b): "until an XP loop exists, no system may gate on CharacterLevel". The
// ruling exists because Swift's third jump shipped gated at level 20 against a
// field NOTHING wrote, so the feature was unreachable by construction and the
// only instrument that caught it was the owner saying "i never could do a 3rd
// jump". These tests are what make the loop's existence assertable rather than
// asserted, so the gates can be re-opened by ruling against something real.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerExperienceCurveTest,
    "RiorsEdge.Progression.Experience.Curve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerExperienceCurveTest::RunTest(const FString& Parameters)
{
    const FBreakerExperienceCurve Curve;
    using XP = UBreakerExperienceLibrary;

    // Monotonic and strictly increasing: every level must cost more than the
    // one before it, or the ladder has a flat spot a player can feel as the
    // game briefly speeding up for no reason.
    for (int32 Level = 1; Level < XP::MaxCharacterLevel - 1; ++Level)
    {
        TestTrue(*FString::Printf(TEXT("Level %d costs more than level %d"), Level + 1, Level),
            XP::XpToNextLevel(Level + 1, Curve) > XP::XpToNextLevel(Level, Curve));
    }

    // THE HARD STOP. The cap is a locked decision with no post-cap character
    // power, so it must fall out of the arithmetic rather than being enforced
    // by whoever remembers to check.
    TestEqual(TEXT("The cap costs nothing to exceed"), XP::XpToNextLevel(XP::MaxCharacterLevel, Curve), 0);
    TestEqual(TEXT("Past the cap costs nothing either"), XP::XpToNextLevel(XP::MaxCharacterLevel + 5, Curve), 0);
    TestEqual(TEXT("A level below 1 is refused"), XP::XpToNextLevel(0, Curve), 0);

    // Round trip: the forward sum and the inverse walk must agree at EVERY
    // level, including the boundaries. They are separate implementations of
    // the same ladder and the whole reason the inverse walks rather than
    // solving in closed form is that a closed form disagrees at the rounding
    // edges — which strands a character one XP short of a level the UI has
    // already shown them.
    for (int32 Level = 1; Level <= XP::MaxCharacterLevel; ++Level)
    {
        const int32 Total = XP::TotalXpToReachLevel(Level, Curve);
        TestEqual(*FString::Printf(TEXT("Exactly enough XP reaches level %d"), Level),
            XP::LevelForTotalXp(Total, Curve), Level);
        if (Level < XP::MaxCharacterLevel)
        {
            TestEqual(*FString::Printf(TEXT("One XP short stays at level %d"), Level - 1 > 0 ? Level - 1 : 1),
                XP::LevelForTotalXp(Total - 1, Curve), FMath::Max(1, Level - 1));
        }
    }

    // Degenerate inputs answer sanely rather than asserting.
    TestEqual(TEXT("Zero XP is level 1"), XP::LevelForTotalXp(0, Curve), 1);
    TestEqual(TEXT("Negative XP is level 1"), XP::LevelForTotalXp(-500, Curve), 1);
    TestEqual(TEXT("Absurd XP clamps to the cap"), XP::LevelForTotalXp(TNumericLimits<int32>::Max(), Curve), XP::MaxCharacterLevel);

    // The bar reads FULL at the cap, not empty.
    TestEqual(TEXT("Progress is full at the cap"),
        XP::LevelProgressFraction(XP::TotalXpToReachLevel(XP::MaxCharacterLevel, Curve), Curve), 1.0f);
    TestEqual(TEXT("Progress is empty at a fresh level"),
        XP::LevelProgressFraction(XP::TotalXpToReachLevel(5, Curve), Curve), 0.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerExperienceKillValueTest,
    "RiorsEdge.Progression.Experience.KillValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerExperienceKillValueTest::RunTest(const FString& Parameters)
{
    const FBreakerExperienceCurve Curve;
    using XP = UBreakerExperienceLibrary;

    // Rank ladder is monotonic, and agrees in SHAPE with the drop table's rank
    // ladder — a player's two reward channels must not disagree about what a
    // boss is worth relative to a trash mob.
    const int32 Trash = XP::XpForKill(EBreakerMonsterRank::Trash, 10, Curve);
    const int32 Elite = XP::XpForKill(EBreakerMonsterRank::Elite, 10, Curve);
    const int32 Bearing = XP::XpForKill(EBreakerMonsterRank::ModifierBearing, 10, Curve);
    const int32 Boss = XP::XpForKill(EBreakerMonsterRank::Boss, 10, Curve);
    TestTrue(TEXT("Elite pays more than trash"), Elite > Trash);
    TestTrue(TEXT("Modifier-bearing pays more than elite"), Bearing > Elite);
    TestTrue(TEXT("Boss pays more than modifier-bearing"), Boss > Bearing);

    // The reward tracks the CONTENT, not the character: a higher area level
    // pays more for the same rank. This is what stops a capped player farming
    // area level 1 as the fastest route to anything.
    TestTrue(TEXT("A higher area level pays more for the same rank"),
        XP::XpForKill(EBreakerMonsterRank::Trash, 50, Curve) > XP::XpForKill(EBreakerMonsterRank::Trash, 1, Curve));

    return true;
}

// ---------------------------------------------------------------------------
// AUDIT #4: XP IS PAID FROM THE AREA LEVEL, NOT THE DROP ITEM LEVEL
// ---------------------------------------------------------------------------
// ABreakerEnemy::GrantExperience passed EnemyLevel — the DROP item level,
// GetDropItemLevel(AreaLevel) plus the elite bonus, clamped to 120 against the
// area ladder's 100 — into AwardKillExperience, under a comment claiming area
// level. Rank already pays the elite premium via EliteXpMultiplier, so the
// elite level bonus double-charged it (elite at area 10: 102 XP off level 15
// against 83 off the area's own 10), and past area 100 the two clamps diverge
// by up to 20 levels. GrantExperience itself needs a world and a player pawn,
// so this pins the SEAM on a real actor: the two fields genuinely diverge for
// an elite, and the number the call site now passes (GetAreaLevel) prices the
// kill differently from the number it used to pass (GetEnemyLevel).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerExperiencePaysTheAreaLevelTest,
    "RiorsEdge.Progression.Experience.PaysTheAreaLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerExperiencePaysTheAreaLevelTest::RunTest(const FString& Parameters)
{
    const FBreakerExperienceCurve Curve;
    using XP = UBreakerExperienceLibrary;

    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();
    if (!Enemy)
    {
        AddError(TEXT("Could not construct an enemy to read its levels from."));
        return false;
    }
    Enemy->SetMonsterRank(EBreakerMonsterRank::Elite);
    Enemy->SetAreaLevel(10);

    // The divergence is real on the actor: an elite's drop level sits ABOVE
    // its area level, so the two are not interchangeable XP inputs.
    TestEqual(TEXT("An elite in area 10 fights at area level 10"), Enemy->GetAreaLevel(), 10);
    TestTrue(TEXT("An elite's drop item level sits above its area level"),
        Enemy->GetEnemyLevel() > Enemy->GetAreaLevel());

    // The kill now pays the area's own level. The old input priced the same
    // kill strictly higher — the elite premium charged twice, once by rank
    // and once by the level scalar.
    const int32 PaidNow = XP::XpForKill(Enemy->GetMonsterRank(), Enemy->GetAreaLevel(), Curve);
    const int32 PaidBefore = XP::XpForKill(Enemy->GetMonsterRank(), Enemy->GetEnemyLevel(), Curve);
    TestTrue(*FString::Printf(TEXT("The drop-level input over-paid the elite (%d XP off the area's %d)"),
        PaidBefore, PaidNow), PaidBefore > PaidNow);

    // The endgame divergence: the area ladder clamps at 100 while the drop
    // ladder runs to 120, so past area 100 an EnemyLevel-priced XP curve kept
    // climbing content that does not exist. The area-priced one cannot.
    Enemy->SetAreaLevel(100);
    TestEqual(TEXT("The area ladder tops out at 100"), Enemy->GetAreaLevel(), 100);
    TestTrue(TEXT("An endgame elite's drop level exceeds the area ladder's cap"),
        Enemy->GetEnemyLevel() > Enemy->GetAreaLevel());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerExperiencePaceTest,
    "RiorsEdge.Progression.Experience.Pace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerExperiencePaceTest::RunTest(const FString& Parameters)
{
    const FBreakerExperienceCurve Curve;
    const FBreakerKillRateSample Kills;
    using XP = UBreakerExperienceLibrary;

    const FBreakerLevellingProjection Early = XP::ProjectLevellingRate(Kills, 1, Curve);
    const FBreakerLevellingProjection Mid = XP::ProjectLevellingRate(Kills, 25, Curve);
    const FBreakerLevellingProjection Late = XP::ProjectLevellingRate(Kills, 50, Curve);

    AddInfo(FString::Printf(TEXT("area 1  | %.0f xp/h | first level in %d trash kills | %.1f h to cap"),
        Early.XpPerHour, Early.KillsForFirstLevel, Early.HoursToCap));
    AddInfo(FString::Printf(TEXT("area 25 | %.0f xp/h | %.1f h to cap"), Mid.XpPerHour, Mid.HoursToCap));
    AddInfo(FString::Printf(TEXT("area 50 | %.0f xp/h | %.1f h to cap"), Late.XpPerHour, Late.HoursToCap));

    // O2'S SECOND HALF, asserted rather than hoped for: a placeholder must be
    // PERCEPTIBLE in the gym. A curve that is arithmetically defensible and
    // invisible in a playtest fails the ruling exactly as hard as a wrong one.
    // The first level must land inside a couple of minutes of ordinary
    // fighting, or the owner's first report is "XP does nothing".
    TestTrue(*FString::Printf(TEXT("The first level arrives within ~40 trash kills (measures %d)"),
        Early.KillsForFirstLevel), Early.KillsForFirstLevel > 0 && Early.KillsForFirstLevel <= 40);

    // And the whole ladder is not a second job. Wide band deliberately: the
    // SHAPE is what this pins, and the exact figure is the owner's to move.
    TestTrue(*FString::Printf(TEXT("Cap is reachable in a sane number of hours (measures %.1f)"), Mid.HoursToCap),
        Mid.HoursToCap > 1.0f && Mid.HoursToCap < 400.0f);

    // Higher area level levels faster, which is the incentive that points a
    // player at content their power actually tracks.
    TestTrue(TEXT("Later content levels faster than early content"), Late.XpPerHour > Early.XpPerHour);

    // The projection must agree with the sum of the parts it claims to model.
    // Same discipline as the drop table's simulation-vs-projection pairing:
    // the documented pace cannot drift from the shipped one.
    const float Recomputed =
        Kills.TrashKillsPerHour * XP::XpForKill(EBreakerMonsterRank::Trash, 25, Curve)
        + Kills.EliteKillsPerHour * XP::XpForKill(EBreakerMonsterRank::Elite, 25, Curve)
        + Kills.ModifierBearingKillsPerHour * XP::XpForKill(EBreakerMonsterRank::ModifierBearing, 25, Curve)
        + Kills.BossKillsPerHour * XP::XpForKill(EBreakerMonsterRank::Boss, 25, Curve);
    TestTrue(TEXT("The projection equals the sum of its rank terms"),
        FMath::Abs(Recomputed - Mid.XpPerHour) < 1.0f);

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
