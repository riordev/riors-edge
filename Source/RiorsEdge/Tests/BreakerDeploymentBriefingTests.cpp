#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerRiftDefinition.h"
#include "UI/BreakerLoadingScreen.h"

// ---------------------------------------------------------------------------
// THE DEPLOYMENT BRIEFING — the composition, exercised with no widget, no
// world and no Slate application.
//
// The pane's whole claim is that every number is DERIVED through the game's
// own functions, never transcribed. That claim is only worth anything held:
// these tests recompose a briefing and check each field against the library
// call it says it came from, so a drift between the pane and what the
// destination actually spawns fails here instead of shipping as a plate that
// lies. O123's ruling — the death-allowance field is ALWAYS present, only
// the value moves — is asserted for both tiers.
//
// NOT COVERED, stated plainly: the widget's layout and its animations (the
// lattice, the crawl, the blink) are Slate and clock; the harness photographs
// them via -BreakerCaptureDeployBeat, and the timings are owner-tuned CVars.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDeploymentBriefingDerivationTest,
    "RiorsEdge.UI.DeploymentBriefing.DerivesFromTheLibraries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDeploymentBriefingDerivationTest::RunTest(const FString& Parameters)
{
    FBreakerRiftDefinition Rift;
    Rift.AreaName = FText::FromString(TEXT("Fernhall Substation"));
    Rift.AreaLine = FText::FromString(TEXT("A relay yard the rift took first."));
    Rift.AreaLevel = 42;
    Rift.Tier = EBreakerRiftTier::Campaign;

    const int32 EliteBonus = 5;
    const FBreakerDeploymentBriefing Briefing =
        SBreakerLoadingScreen::MakeBriefing(Rift, EliteBonus, UBreakerRiftLibrary::SoloEndgameDeathBudget);

    // The authored fields pass through untouched.
    TestEqual(TEXT("The area name passes through"), Briefing.AreaName.ToString(), Rift.AreaName.ToString());
    TestEqual(TEXT("The area line passes through"), Briefing.AreaLine.ToString(), Rift.AreaLine.ToString());
    TestEqual(TEXT("The area level is the clamped effective level"), Briefing.AreaLevel, Rift.EffectiveAreaLevel());

    // The derived fields match the libraries they claim to come from.
    int32 ExpectedMin = 0, ExpectedMax = 0;
    UBreakerRiftLibrary::GetDropItemLevelRange(Rift.EffectiveAreaLevel(), EliteBonus, ExpectedMin, ExpectedMax);
    TestEqual(TEXT("Item level floor is the pipeline's own"), Briefing.ItemLevelMin, ExpectedMin);
    TestEqual(TEXT("Item level ceiling is the pipeline's own"), Briefing.ItemLevelMax, ExpectedMax);

    const FBreakerMonsterChassisParams Params;
    TestEqual(TEXT("Health multiplier is the curve's own"), Briefing.HealthMultiplier,
        UBreakerRiftLibrary::GetMonsterHealthMultiplier(Rift.EffectiveAreaLevel(), Params));
    TestEqual(TEXT("Damage multiplier is the curve's own"), Briefing.DamageMultiplier,
        UBreakerRiftLibrary::GetMonsterDamageMultiplier(Rift.EffectiveAreaLevel(), Params));

    // Sanity on the shape the derivations promise: the ceiling never sits
    // under the floor, and an area 41 levels past the baseline is not x1.0.
    TestTrue(TEXT("The item range is a range"), Briefing.ItemLevelMax >= Briefing.ItemLevelMin);
    TestTrue(TEXT("Level 42 monsters are more than level 1 monsters"), Briefing.HealthMultiplier > 1.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDeploymentBriefingDeathFieldTest,
    "RiorsEdge.UI.DeploymentBriefing.DeathFieldAlwaysPresent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDeploymentBriefingDeathFieldTest::RunTest(const FString& Parameters)
{
    // O123: the field is always present and reads its mode — campaign prints
    // UNLIMITED, endgame prints the count remaining. Only the value moves.
    FBreakerRiftDefinition Campaign;
    Campaign.AreaLevel = 10;
    Campaign.Tier = EBreakerRiftTier::Campaign;
    const FBreakerDeploymentBriefing CampaignBriefing =
        SBreakerLoadingScreen::MakeBriefing(Campaign, 5, UBreakerRiftLibrary::SoloEndgameDeathBudget);
    TestEqual(TEXT("Campaign reads UNLIMITED"), CampaignBriefing.DeathAllowance, FString(TEXT("UNLIMITED")));

    FBreakerRiftDefinition Endgame = Campaign;
    Endgame.Tier = EBreakerRiftTier::Endgame;
    const FBreakerDeploymentBriefing EndgameBriefing =
        SBreakerLoadingScreen::MakeBriefing(Endgame, 5, UBreakerRiftLibrary::SoloEndgameDeathBudget);
    TestEqual(TEXT("Endgame reads the solo budget"), EndgameBriefing.DeathAllowance,
        FString::Printf(TEXT("%d REMAINING"), UBreakerRiftLibrary::SoloEndgameDeathBudget));

    // Both tiers carry the field. An empty readout is a hidden field, which
    // is the thing the ruling forbids.
    TestFalse(TEXT("Campaign's field is present"), CampaignBriefing.DeathAllowance.IsEmpty());
    TestFalse(TEXT("Endgame's field is present"), EndgameBriefing.DeathAllowance.IsEmpty());

    return true;
}

#endif
