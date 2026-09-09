#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/BreakerMapTravelRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMapTravelOfferTest,
    "RiorsEdge.UI.MapTravel.Offer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMapTravelOfferTest::RunTest(const FString& Parameters)
{
    using namespace BreakerMapTravel;
    const FName Hub(TEXT("Hub"));
    const TArray<FName> Registry{ FName(TEXT("Hub")), FName(TEXT("Fernhall")), FName(TEXT("RedBasin")) };

    // O265, the asymmetry. Outside the hub there is exactly one way out and it
    // leads home — never a second instance, however many the registry lists.
    const TArray<FName> FromInstance = OfferedDestinations(false, Registry, Hub);
    TestEqual(TEXT("an instance offers exactly one destination"), FromInstance.Num(), 1);
    TestEqual(TEXT("and that destination is the hub"), FromInstance.IsEmpty() ? NAME_None : FromInstance[0], Hub);

    // The registry is irrelevant outside the hub: home is offered even when
    // this instance's own list would not have carried it.
    const TArray<FName> NoHubInRegistry = OfferedDestinations(false, { FName(TEXT("Fernhall")) }, Hub);
    TestEqual(TEXT("home is offered whatever the registry says"), NoHubInRegistry.Num(), 1);
    TestEqual(TEXT("and it is still the hub"), NoHubInRegistry.IsEmpty() ? NAME_None : NoHubInRegistry[0], Hub);

    // From the hub: the ordinary registry, minus the hub itself.
    const TArray<FName> FromHub = OfferedDestinations(true, Registry, Hub);
    TestEqual(TEXT("the hub offers the rest of the registry"), FromHub.Num(), 2);
    TestFalse(TEXT("the hub never offers itself"), FromHub.Contains(Hub));
    TestTrue(TEXT("Fernhall survives"), FromHub.Contains(FName(TEXT("Fernhall"))));
    TestTrue(TEXT("Red Basin survives"), FromHub.Contains(FName(TEXT("RedBasin"))));

    // The rule NARROWS by location and can never widen what the registry
    // already refused: an entry the caller filtered out stays filtered out.
    const TArray<FName> Filtered = OfferedDestinations(true, { FName(TEXT("Fernhall")) }, Hub);
    TestEqual(TEXT("a filtered registry stays filtered"), Filtered.Num(), 1);
    TestFalse(TEXT("a dropped destination is not restored"), Filtered.Contains(FName(TEXT("RedBasin"))));

    // Degenerate: no hub authored means no stranding button rather than a
    // button that travels to None.
    TestEqual(TEXT("no hub id offers nothing from an instance"), OfferedDestinations(false, Registry, NAME_None).Num(), 0);

    // The combat gate is both directions, which is what makes it a gate on
    // TRAVEL rather than an escape hatch.
    TestTrue(TEXT("combat refuses travel"), TravelRefusedInCombat(true));
    TestFalse(TEXT("out of combat travels"), TravelRefusedInCombat(false));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
