#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/BreakerCharacterSheetMath.h"

// ---------------------------------------------------------------------------
// THE CHARACTER SHEET'S ARITHMETIC.
//
// The sheet exists because the owner could not tell what a build was doing.
// A sheet that prints a WRONG number is worse than no sheet: it is the same
// failure as an instrument that returns a false negative, and the reader files
// a bug against working code. So every derived figure it prints is asserted
// here, world-free, against values worked by hand rather than read back out of
// the implementation.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCharacterSheetMathTest,
    "RiorsEdge.UI.CharacterSheet.Math",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCharacterSheetMathTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSheet;

    // --- Shot damage -------------------------------------------------------
    TestEqual(TEXT("A single-pellet shot is base times the additive bucket"),
        ShotDamage(50.0f, 1, 1.5f), 75.0f, 0.001f);
    TestEqual(TEXT("Pellets multiply the shot"),
        ShotDamage(10.0f, 8, 1.0f), 80.0f, 0.001f);
    // A definition that forgets to set its pellet count must not delete the
    // weapon's damage.
    TestEqual(TEXT("Zero pellets is treated as one, never as no damage"),
        ShotDamage(10.0f, 0, 1.0f), 10.0f, 0.001f);

    // --- Crit is an EXPECTATION, not a best case ---------------------------
    TestEqual(TEXT("No crit chance is no crit contribution"),
        CritFactor(0.0f, 2.0f), 1.0f, 0.001f);
    TestEqual(TEXT("Half the hits at double damage is one and a half"),
        CritFactor(0.5f, 2.0f), 1.5f, 0.001f);
    TestEqual(TEXT("Certain crit is the multiplier itself"),
        CritFactor(1.0f, 2.5f), 2.5f, 0.001f);
    // The bug this forbids: a crit multiplier below 1 would REDUCE expected
    // damage, so a mis-authored 0.5x would read as a downside on the sheet.
    TestEqual(TEXT("A sub-1 multiplier cannot lower expected damage"),
        CritFactor(1.0f, 0.5f), 1.0f, 0.001f);
    TestEqual(TEXT("Chance is clamped, so 150% crit is not 1.5 crits"),
        CritFactor(1.5f, 2.0f), 2.0f, 0.001f);

    // --- Burst against sustained -------------------------------------------
    // 600 RPM = 10 rounds a second. 100 damage a shot, no crit: 1000 burst.
    const float Shot = 100.0f;
    const float NoCrit = CritFactor(0.0f, 2.0f);
    TestEqual(TEXT("Ten rounds a second at 100 is 1000 burst"),
        BurstDps(Shot, NoCrit, 600.0f), 1000.0f, 0.01f);

    // A 30-round magazine at 10 rps empties in 3s; a 2s reload makes the cycle
    // 5s for 3000 damage = 600 sustained. Worked by hand, not read back.
    TestEqual(TEXT("Sustained folds the reload into the cycle"),
        SustainedDps(Shot, NoCrit, 600.0f, 30, 2.0f), 600.0f, 0.01f);
    TestTrue(TEXT("Sustained never exceeds burst"),
        SustainedDps(Shot, NoCrit, 600.0f, 30, 2.0f) <= BurstDps(Shot, NoCrit, 600.0f) + 0.001f);
    TestEqual(TEXT("A zero reload makes the two equal"),
        SustainedDps(Shot, NoCrit, 600.0f, 30, 0.0f), BurstDps(Shot, NoCrit, 600.0f), 0.01f);
    // Degenerate inputs report zero rather than dividing by zero and printing
    // an infinity onto the screen.
    TestEqual(TEXT("A weapon that cannot fire has no DPS"),
        BurstDps(Shot, NoCrit, 0.0f), 0.0f, 0.001f);
    TestEqual(TEXT("A weapon that cannot fire has no sustained DPS either"),
        SustainedDps(Shot, NoCrit, 0.0f, 30, 2.0f), 0.0f, 0.001f);

    // --- Defence -----------------------------------------------------------
    TestEqual(TEXT("No armour is no mitigation"), ArmourMitigation(0.0f), 0.0f, 0.001f);
    TestEqual(TEXT("Armour equal to K is half mitigation"),
        ArmourMitigation(100.0f, 100.0f), 0.5f, 0.001f);
    TestEqual(TEXT("Mitigation is capped, however much armour is stacked"),
        ArmourMitigation(1000000.0f, 100.0f, 0.8f), 0.8f, 0.001f);

    // 100 health + 100 shield behind 50% mitigation absorbs 400 raw.
    TestEqual(TEXT("Effective health is the pool over what gets through"),
        EffectiveHealthPool(100.0f, 100.0f, 0.5f), 400.0f, 0.01f);
    TestEqual(TEXT("With no mitigation, effective health is the raw pool"),
        EffectiveHealthPool(100.0f, 50.0f, 0.0f), 150.0f, 0.01f);
    // THE POINT OF THE WHOLE PANEL: a defensive commitment has to show up as a
    // bigger number here, or the sheet cannot answer the question it exists for.
    TestTrue(TEXT("Mitigation strictly raises effective health"),
        EffectiveHealthPool(100.0f, 0.0f, 0.5f) > EffectiveHealthPool(100.0f, 0.0f, 0.0f));

    TestEqual(TEXT("Four hits of 100 into 400 effective health"),
        HitsSurvived(400.0f, 100.0f), 4);
    // A hit that leaves the character standing has not killed them: the count
    // rounds UP, and a 401-point pool survives a fifth hit.
    TestEqual(TEXT("A partial hit still has to land"),
        HitsSurvived(401.0f, 100.0f), 5);
    TestEqual(TEXT("A hit that deals nothing kills nobody"),
        HitsSurvived(400.0f, 0.0f), 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
