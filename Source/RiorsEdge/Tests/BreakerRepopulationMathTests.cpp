#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BreakerEnemy.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerRepopulationMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRepopulationMathTest,
    "RiorsEdge.Zone.Repopulation.Rule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRepopulationMathTest::RunTest(const FString& Parameters)
{
    using namespace BreakerRepopulation;

    // WHEN. A slot that has not waited out the delay is not due, and the
    // boundary is inclusive so a slot cannot sit one float short forever.
    TestFalse(TEXT("a slot just emptied is not due"), IsDue(0.0f, 45.0f));
    TestFalse(TEXT("nor one still inside the delay"), IsDue(44.9f, 45.0f));
    TestTrue(TEXT("a slot exactly at the delay is due"), IsDue(45.0f, 45.0f));
    TestTrue(TEXT("and one past it stays due"), IsDue(600.0f, 45.0f));
    // A zero delay disables repopulation rather than making everything due the
    // instant it dies — the failure a designer wants from a zeroed dial is a
    // quiet world, not every pocket refilling inside one frame.
    TestFalse(TEXT("a zero delay repopulates nothing"), IsDue(600.0f, 0.0f));
    TestFalse(TEXT("and a negative delay is not a licence either"), IsDue(600.0f, -5.0f));

    // WHERE THE PLAYER IS. The gate reads a SQUARED distance because the caller
    // has one and a square root buys nothing.
    TestTrue(TEXT("a distant player does not block a return"), IsClearOfPlayer(4000.0 * 4000.0, 3000.0f));
    TestFalse(TEXT("a near player does"), IsClearOfPlayer(1000.0 * 1000.0, 3000.0f));
    TestTrue(TEXT("the boundary itself is clear"), IsClearOfPlayer(3000.0 * 3000.0, 3000.0f));
    TestTrue(TEXT("a zero minimum gates nothing"), IsClearOfPlayer(0.0, 0.0f));

    // ONE AT A TIME, MOST OVERDUE FIRST.
    {
        const float Empty[] = { 10.0f, 90.0f, 50.0f, 0.0f };
        TestEqual(TEXT("the most overdue slot refills first"),
            NextDueSlot(MakeArrayView(Empty, 4), 45.0f), 1);
    }
    {
        // Nothing due yields nothing: the caller must not be handed slot 0 as a
        // consolation, or a cleared pocket would refill the instant it cleared.
        const float Empty[] = { 1.0f, 2.0f, 3.0f };
        TestEqual(TEXT("no due slot returns none"),
            NextDueSlot(MakeArrayView(Empty, 3), 45.0f), INDEX_NONE);
    }
    {
        // A tie breaks on the lower index so a pocket refills in the order it
        // was authored, and two runs never disagree about the same area.
        const float Empty[] = { 60.0f, 60.0f, 60.0f };
        TestEqual(TEXT("a tie breaks on the authored order"),
            NextDueSlot(MakeArrayView(Empty, 3), 45.0f), 0);
    }
    {
        const TArrayView<const float> None;
        TestEqual(TEXT("an area with no slots refills nothing"),
            NextDueSlot(None, 45.0f), INDEX_NONE);
    }

    // THE SHIPPED CONFIGURATION, against the default-constructed mode. A
    // clearance that shipped inside the enemy's own detection range would let a
    // patrol arrive already hunting the player, which is the arrival complaint
    // this whole block exists to answer — so the relationship is asserted, not
    // just the figure.
    const ABreakerGameMode* Mode = GetDefault<ABreakerGameMode>();
    if (!TestNotNull(TEXT("the game mode class default exists"), Mode)) return false;
    TestTrue(TEXT("the shipped repopulation delay is a real wait"),
        Mode->OutdoorRepopulationDelaySeconds >= 30.0f);
    const float Detection = GetDefault<ABreakerEnemy>()->GetDetectionRange();
    TestTrue(TEXT("a returning patrol cannot arrive already able to see the player"),
        Mode->OutdoorRepopulationClearanceCm > Detection);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
