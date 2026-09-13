#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerDeathBudgetMath.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerRiftDefinition.h"
#include "UI/BreakerTypeRoles.h"

// THE DEATH BUDGET (O82). Pure maths, no world: campaign never spends and
// always retries; an endgame instance grants a solo character two deaths and
// terminates in exactly two spends; a live boss spends nothing at any tier;
// the death screen's model carries the terminal, boss and campaign variants.
// Shipped configuration is asserted against the defaults the game actually
// constructs.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDeathBudgetTest,
    "RiorsEdge.Game.DeathBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDeathBudgetTest::RunTest(const FString& Parameters)
{
    using namespace BreakerDeathBudget;
    constexpr int32 Budget = UBreakerRiftLibrary::SoloEndgameDeathBudget;

    // (a) Which tier is budgeted.
    TestFalse(TEXT("Campaign is not budgeted"), IsBudgeted(EBreakerRiftTier::Campaign));
    TestTrue(TEXT("Endgame is budgeted"), IsBudgeted(EBreakerRiftTier::Endgame));

    // (b) Campaign never spends and always retries, from any counter.
    for (const int32 Remaining : {Budget, 1, 0})
    {
        TestEqual(FString::Printf(TEXT("Campaign death at %d spends nothing"), Remaining),
            SpendDeath(EBreakerRiftTier::Campaign, Remaining, false), Remaining);
        TestTrue(FString::Printf(TEXT("Campaign at %d always retries"), Remaining),
            CanRetryRift(EBreakerRiftTier::Campaign, Remaining));
    }

    // (c) Endgame from the solo budget terminates in exactly two spends:
    // 2 -> 1 -> 0, retry true, true, false, and the counter never goes
    // negative however many more deaths land.
    {
        int32 Remaining = Budget;
        TestTrue(TEXT("Endgame at the full budget retries"), CanRetryRift(EBreakerRiftTier::Endgame, Remaining));
        Remaining = SpendDeath(EBreakerRiftTier::Endgame, Remaining, false);
        TestEqual(TEXT("First endgame death: 2 -> 1"), Remaining, Budget - 1);
        TestTrue(TEXT("Endgame at one remaining retries"), CanRetryRift(EBreakerRiftTier::Endgame, Remaining));
        Remaining = SpendDeath(EBreakerRiftTier::Endgame, Remaining, false);
        TestEqual(TEXT("Second endgame death: 1 -> 0"), Remaining, 0);
        TestFalse(TEXT("Endgame at zero does not retry"), CanRetryRift(EBreakerRiftTier::Endgame, Remaining));
        Remaining = SpendDeath(EBreakerRiftTier::Endgame, Remaining, false);
        TestEqual(TEXT("A third death never goes negative"), Remaining, 0);
        TestFalse(TEXT("Endgame stays terminal"), CanRetryRift(EBreakerRiftTier::Endgame, Remaining));
    }

    // (d) A live boss spends nothing at any tier: the encounter resets instead.
    for (const EBreakerRiftTier Tier : {EBreakerRiftTier::Campaign, EBreakerRiftTier::Endgame})
    {
        for (const int32 Remaining : {Budget, 1})
        {
            TestEqual(FString::Printf(TEXT("Boss death at tier %d, %d remaining, spends nothing"),
                    static_cast<int32>(Tier), Remaining),
                SpendDeath(Tier, Remaining, true), Remaining);
        }
    }

    // (e) The model: terminal, boss and campaign variants.
    {
        const FBreakerDeathScreenModel Terminal = Model(EBreakerRiftTier::Endgame, 0, Budget, false);
        TestFalse(TEXT("Terminal model offers no retry"), Terminal.bRetry);
        TestTrue(TEXT("Terminal model shows the tally"), Terminal.bShowTally);
        TestEqual(TEXT("Terminal model: 0 remaining"), Terminal.DeathsRemaining, 0);
        TestEqual(TEXT("Terminal model: the solo budget"), Terminal.DeathBudget, Budget);
        TestTrue(TEXT("Terminal line 2 names the loss"), Terminal.Line2.Contains(RiftLostLine()));
        TestTrue(TEXT("Terminal line 2 carries the count"), Terminal.Line2.Contains(TEXT("0 of 2 deaths remain")));

        const FBreakerDeathScreenModel Live = Model(EBreakerRiftTier::Endgame, 1, Budget, false);
        TestTrue(TEXT("Endgame with budget offers retry"), Live.bRetry);
        TestTrue(TEXT("Endgame line 2 carries the count"), Live.Line2.Contains(TEXT("1 of 2 deaths remain")));
        TestTrue(TEXT("Endgame line 2 ends on the gear"), Live.Line2.EndsWith(GearKeptLine()));

        for (const EBreakerRiftTier Tier : {EBreakerRiftTier::Campaign, EBreakerRiftTier::Endgame})
        {
            const FBreakerDeathScreenModel Boss = Model(Tier, Budget, Budget, true);
            TestTrue(FString::Printf(TEXT("Boss model at tier %d ends on the reset line"), static_cast<int32>(Tier)),
                Boss.Line2.EndsWith(BossLine()));
            TestTrue(FString::Printf(TEXT("Boss model at tier %d offers retry"), static_cast<int32>(Tier)), Boss.bRetry);
        }

        const FBreakerDeathScreenModel Campaign = Model(EBreakerRiftTier::Campaign, Budget, Budget, false);
        TestFalse(TEXT("Campaign model shows no tally"), Campaign.bShowTally);
        TestTrue(TEXT("Campaign model offers retry"), Campaign.bRetry);
        TestFalse(TEXT("Campaign line 2 carries no count"), Campaign.Line2.Contains(TEXT("deaths remain")));
        TestTrue(TEXT("Campaign line 2 ends on the gear"), Campaign.Line2.EndsWith(GearKeptLine()));

        // The site's pieces: a wave lands first; an unset wave is omitted.
        FBreakerDeathScreenSite Site;
        Site.AreaName = TEXT("Fernhall Substation");
        Site.Wave = 2;
        Site.WaveTotal = 3;
        const FBreakerDeathScreenModel Sited = Model(EBreakerRiftTier::Campaign, Budget, Budget, false, Site);
        TestTrue(TEXT("Sited line 2 opens on the wave"), Sited.Line2.StartsWith(TEXT("Wave 2 of 3")));
        TestTrue(TEXT("Sited headline names the place"), Sited.Headline.Contains(TEXT("FERNHALL SUBSTATION")));
        TestFalse(TEXT("Unsited line 2 has no wave"), Campaign.Line2.Contains(TEXT("Wave")));
        // The terminal headline changes verb.
        const FBreakerDeathScreenModel SitedTerminal = Model(EBreakerRiftTier::Endgame, 0, Budget, false, Site);
        TestTrue(TEXT("Terminal headline: the place closes"), SitedTerminal.Headline.EndsWith(TEXT("CLOSES")));
        TestFalse(TEXT("Terminal headline is not the erased verb"), SitedTerminal.Headline.Contains(TEXT("ERASED")));
    }

    // (f) Shipped configuration.
    TestEqual(TEXT("Shipped SoloEndgameDeathBudget == 2"), Budget, 2);
    TestEqual(TEXT("A default rift is Campaign"),
        static_cast<int32>(FBreakerRiftDefinition().Tier), static_cast<int32>(EBreakerRiftTier::Campaign));
    TestEqual(TEXT("The session's counter defaults to the solo budget"),
        GetDefault<UBreakerGameInstance>()->EndgameDeathsRemaining, Budget);

    return true;
}

// THE HEADLINE FITS ITS COLUMN. The owner died at Breach Marshalling Yard
// and the death screen's headline — "ERASED AT BREACH MARSHALLING YARD" at
// the display role's 40 px — overran the 720 px column and clipped. The
// fit (BreakerFitDisplaySize) steps the size down until the line fits, no
// lower than the floor, and leaves a short title at the full size. Pinned
// on the headless width estimate the fit uses when no font is measured, so
// a suite with no content still proves the rule; a wrapped headline would
// be the auto-wrap rule broken.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDeathScreenHeadlineFitsColumnTest,
    "RiorsEdge.UI.DeathScreen.HeadlineFitsColumn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDeathScreenHeadlineFitsColumnTest::RunTest(const FString& Parameters)
{
    constexpr int32 ColumnPx = 720;   // O2 PLACEHOLDER — the death screen's headline column
    constexpr int32 HeadlinePx = 40;  // O2 PLACEHOLDER — the display role's headline size
    constexpr int32 FloorPx = 24;     // O2 PLACEHOLDER — the smallest a headline may shrink to

    const auto Long = BreakerFitDisplaySize(TEXT("ERASED AT BREACH MARSHALLING YARD"), ColumnPx, HeadlinePx, FloorPx);
    TestTrue(FString::Printf(TEXT("the long headline steps down from %d (was %d)"), HeadlinePx, static_cast<int32>(Long)),
        Long < HeadlinePx);
    TestTrue(FString::Printf(TEXT("the long headline never drops under the %d floor (was %d)"), FloorPx, static_cast<int32>(Long)),
        Long >= FloorPx);

    const auto Short = BreakerFitDisplaySize(TEXT("ERASED AT DEPOT"), ColumnPx, HeadlinePx, FloorPx);
    TestEqual(TEXT("a short headline keeps the full size"), static_cast<int32>(Short), HeadlinePx);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
