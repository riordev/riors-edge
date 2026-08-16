// SHIPPED-CONFIGURATION TESTS FOR THE BOOT FLOW (O40c), in the
// RiorsEdge.Movement.JumpGrantMatrix mold: every assertion below is about the
// configuration the game actually boots with, not about a rule in the
// abstract. This suite exists because the same failure shape shipped three
// times — JumpGrant, PowerBand, the keystone budget — and then a fourth: the
// three-map flow landed with maps the shipped boot path could not produce a
// pawn in (no PlayerStart), and with a title menu that re-opened, paused, on
// every single map arrival (D1). All of it behind a green suite, because
// nothing asserted the boot path AS CONFIGURED.
//
// What a headless test genuinely cannot cover — actually loading the three
// maps and walking front end → Anchor → gym — is covered by the capture
// harness (-game -BreakerAutoPlay and a plain -game boot); this suite pins
// every decision on that path that is expressible without a world.

#include "Misc/AutomationTest.h"

#include "Characters/BreakerCharacter.h"
#include "Game/BreakerGameInstance.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBootFlowConfigTest,
    "RiorsEdge.Game.BootFlow.ShippedConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBootFlowConfigTest::RunTest(const FString& Parameters)
{
    // ---- The boot map is the front end, and it exists -----------------------
    // GameDefaultMap governs a standalone/packaged launch. If it names a map
    // that is not the front end, the whole front door is bypassed; if it names
    // a map that does not exist, the game boots into nothing.
    FString GameDefaultMap;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"),
        TEXT("GameDefaultMap"), GameDefaultMap, GEngineIni);
    TestTrue(TEXT("GameDefaultMap names the front end"),
        GameDefaultMap.Contains(UBreakerGameInstance::FrontEndMapName()));

    // ---- The three maps the code compares names against are real assets ----
    // Map identity is a string match against the .umap short name
    // (BreakerGameInstance.h): renaming an asset without editing those
    // functions is a silent no-op where the map just becomes "the gym". This
    // pins the other direction — the names the code holds must resolve to
    // packages on disk.
    for (const TCHAR* MapName : { UBreakerGameInstance::FrontEndMapName(),
        UBreakerGameInstance::AnchorMapName(), UBreakerGameInstance::GymMapName() })
    {
        const FString PackagePath = FString::Printf(TEXT("/Game/Breaker/Maps/%s"), MapName);
        TestTrue(*FString::Printf(TEXT("Map package '%s' exists"), *PackagePath),
            FPackageName::DoesPackageExist(PackagePath));
    }

    // ---- The D1 guard matrix ------------------------------------------------
    // The title menu belongs to sessions that have not entered the world yet.
    // The third row is the shipped bug: arriving in the Anchor mid-session
    // re-opened the title screen, paused, on every level transition.
    TestTrue(TEXT("Front end, character chosen (RETURN TO TITLE): menu shows"),
        ABreakerCharacter::ShouldShowInitialMenu(/*bIsFrontEndMap=*/true, /*bSessionHasActiveCharacter=*/true));
    TestTrue(TEXT("Front end, fresh session: menu shows"),
        ABreakerCharacter::ShouldShowInitialMenu(true, false));
    TestFalse(TEXT("Mid-session map arrival: MENU MUST NOT SHOW (D1)"),
        ABreakerCharacter::ShouldShowInitialMenu(false, true));
    TestTrue(TEXT("PIE drop-in with no character: menu shows (the daily workflow)"),
        ABreakerCharacter::ShouldShowInitialMenu(false, false));

    // ---- Travel closes the loop both ways ----------------------------------
    // HandleHubTravelSelected dispatches on exactly these two ids; a registry
    // that drops either one strands the player on one side of the gate.
    FBreakerTravelDestination Destination;
    TestTrue(TEXT("The gym destination is registered"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::GymDestinationId, Destination));
    TestTrue(TEXT("The gym destination is enabled"), Destination.bEnabled);
    TestTrue(TEXT("The hub destination is registered (the way back)"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::HubDestinationId, Destination));
    TestTrue(TEXT("The hub destination is enabled"), Destination.bEnabled);

    // A travel point never offers the place the player already is.
    ABreakerTravelPoint* GymGate = NewObject<ABreakerTravelPoint>();
    GymGate->ExcludedDestinationId = ABreakerTravelPoint::GymDestinationId;
    for (const FBreakerTravelDestination& Offered : GymGate->GetAvailableDestinations())
    {
        TestNotEqual(TEXT("The gym's gate does not offer the gym"),
            Offered.Id, ABreakerTravelPoint::GymDestinationId);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLevelPointEntitlementTest,
    "RiorsEdge.Progression.LevelPointEntitlement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLevelPointEntitlementTest::RunTest(const FString& Parameters)
{
    // XP-And-Pacing §4 as SHIPPED: 1 Class Point per level to 30, 1 Core
    // Point per level to 50, with the slice lump (10/12) as an ADVANCE on the
    // entitlement rather than a bonus beside it. Run at the shipped grants,
    // never at an inflated test budget — granting 30 points in a rig is
    // exactly how the keystone budget contradiction stayed invisible.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    Progression->ApplySliceDefaultsIfFresh();

    TestEqual(TEXT("A fresh character holds exactly the slice budget"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints),
        UBreakerProgressionLibrary::SliceClassPointGrant);

    // Levels 2..10 are pre-paid by the lump: no extra points may arrive.
    const FBreakerExperienceCurve Curve;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, Curve));
    TestEqual(TEXT("Level 10 still holds the slice budget (lump = advance, not bonus)"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints),
        UBreakerProgressionLibrary::SliceClassPointGrant);

    // Level 11 pays the 11th Class Point — and 11 is the first level a branch
    // keystone is affordable through play (investment gate 8 + keystone cost
    // 3), which resolves the A2 budget contradiction by levelling rather than
    // by moving either O2-frozen number.
    Progression->AwardExperience(
        UBreakerExperienceLibrary::TotalXpToReachLevel(11, Curve) - Progression->GetTotalExperience());
    TestEqual(TEXT("Level 11 holds 11 Class Points"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 11);
    TestTrue(TEXT("A keystone (gate 8 + cost 3) is affordable at level 11 in play"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints) >= 8 + 3);

    // The exhaustion beats land exactly where the act structure puts them:
    // Class Points stop at 30, Core Points run to 50.
    Progression->AwardExperience(
        UBreakerExperienceLibrary::TotalXpToReachLevel(30, Curve) - Progression->GetTotalExperience());
    TestEqual(TEXT("Level 30 holds 30 Class Points (exhausted)"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 30);

    Progression->AwardExperience(
        UBreakerExperienceLibrary::TotalXpToReachLevel(50, Curve) - Progression->GetTotalExperience());
    TestEqual(TEXT("Level 50 (cap) still holds 30 Class Points"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 30);
    TestEqual(TEXT("Level 50 holds 50 Core Points"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 50);
    return true;
}
