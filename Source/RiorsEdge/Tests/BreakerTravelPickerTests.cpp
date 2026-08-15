#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/BreakerTravelPoint.h"

// The travel picker (EBreakerMenuScreen::Travel, SBreakerMenu::BuildTravelScreen)
// draws one card per entry in ABreakerTravelPoint::GetAvailableDestinations()
// and commits a click through ABreakerTravelPoint::SelectDestination(). This
// suite covers the half of that which is PURE — the list the screen is handed
// and the answers it gets back — with no UWorld and no Slate.
//
// WHAT IS NOT COVERED HERE, stated plainly rather than implied by absence:
//
//  1. Anything Slate. The card layout, the selected ring, the fixed-height
//     BuildFrame plate, and Escape leaving without travelling all need a live
//     widget tree and a running application; SBreakerMenu is not constructible
//     headless (it reaches for GEngine->GameViewport through MeasureWideScreen
//     on every frame build). Those are LOOK-AT-IT items, not test items.
//  2. The DISABLED-destination refusal. SelectDestination refuses an entry
//     whose bEnabled is false, and the fallback registry contains no such
//     entry — by design: BreakerTravelPoint.h's own header says destinations
//     that do not exist yet are DATA-ABSENT rather than authored-and-disabled,
//     and "do not add fake entries to exercise it today". There is no seam to
//     inject one through (the registry is a file-static built once), so the
//     disabled branch is genuinely unexercised and a test claiming otherwise
//     would be a lie. The UNKNOWN-id branch below shares its refusal path and
//     its no-broadcast guarantee, which is the part the picker depends on.
//  3. What travel DOES. SelectDestination broadcasts and returns; whoever binds
//     OnDestinationSelected owns the rest, and that binding lives in the game
//     mode, outside this pass.
//
// Overlap with BreakerHubTests.cpp is deliberate at exactly one point: that
// suite pins the REGISTRY's shape (how many destinations exist, that their ids
// are stable). This one asks the different question the picker actually cares
// about — what a given POINT offers, and what happens when a card is clicked.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTravelPickerListTest,
    "RiorsEdge.UI.TravelPicker.DestinationList",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTravelPickerListTest::RunTest(const FString& Parameters)
{
    // NewObject, never a world spawn: GetAvailableDestinations reads the static
    // registry and one UPROPERTY on the actor, and nothing on that path needs
    // the actor to be in a level.
    ABreakerTravelPoint* Point = NewObject<ABreakerTravelPoint>();
    const TArray<FBreakerTravelDestination> Available = Point->GetAvailableDestinations();

    // A point with no exclusion set offers everything enabled. If this ever
    // reaches zero the picker draws its "nowhere to go" plate, which is a real
    // state but not the one the hub is supposed to be in.
    TestTrue(TEXT("An unexcluded travel point offers at least one destination"), Available.Num() > 0);

    // THE CARD'S CONTRACT. The screen renders display name, description and a
    // selection keyed on Id. Each of those must be present on every entry, or
    // a card ships blank — the failure mode nobody notices until it is on
    // screen, which is how every other menu in this project shipped wrong.
    TSet<FName> SeenIds;
    for (const FBreakerTravelDestination& Destination : Available)
    {
        TestTrue(TEXT("An offered destination has a real id"), Destination.Id != NAME_None);
        TestFalse(TEXT("An offered destination has a display name"), Destination.DisplayName.IsEmpty());
        TestFalse(TEXT("An offered destination has a description"), Destination.Description.IsEmpty());
        // The filter's whole job. A disabled entry reaching the list would draw
        // a card that SelectDestination then refuses.
        TestTrue(TEXT("An offered destination is enabled"), Destination.bEnabled);

        bool bAlreadySeen = false;
        SeenIds.Add(Destination.Id, &bAlreadySeen);
        TestFalse(TEXT("An offered destination id appears exactly once"), bAlreadySeen);
    }

    // EVERY CARD WORKS. The picker's one guarantee to the player is that a
    // destination it drew can actually be chosen; a list containing a row that
    // refuses on click is worse than a shorter list.
    for (const FBreakerTravelDestination& Destination : Available)
    {
        ABreakerTravelPoint* Fresh = NewObject<ABreakerTravelPoint>();
        FName Broadcast = NAME_None;
        int32 BroadcastCount = 0;
        Fresh->OnDestinationSelected.AddLambda([&Broadcast, &BroadcastCount](FName Id, APawn*)
        {
            Broadcast = Id;
            ++BroadcastCount;
        });

        TestTrue(TEXT("Every offered destination is accepted by SelectDestination"),
            Fresh->SelectDestination(Destination.Id, nullptr));
        TestEqual(TEXT("Acceptance broadcasts exactly once"), BroadcastCount, 1);
        TestEqual(TEXT("The broadcast carries the id that was clicked"), Broadcast, Destination.Id);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTravelPickerExclusionTest,
    "RiorsEdge.UI.TravelPicker.ExcludedDestination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTravelPickerExclusionTest::RunTest(const FString& Parameters)
{
    auto OffersId = [](const TArray<FBreakerTravelDestination>& List, FName Id)
    {
        for (const FBreakerTravelDestination& Destination : List)
        {
            if (Destination.Id == Id) return true;
        }
        return false;
    };

    // The rule the picker exists to respect: a travel point never offers the
    // place it stands in. Without it the Anchor's gate lists "The Anchor" and
    // the player clicks a card that teleports them nowhere.
    ABreakerTravelPoint* AtHub = NewObject<ABreakerTravelPoint>();
    AtHub->ExcludedDestinationId = ABreakerTravelPoint::HubDestinationId;
    const TArray<FBreakerTravelDestination> FromHub = AtHub->GetAvailableDestinations();
    TestFalse(TEXT("A point at the hub does not offer the hub"),
        OffersId(FromHub, ABreakerTravelPoint::HubDestinationId));
    TestTrue(TEXT("A point at the hub still offers the gym"),
        OffersId(FromHub, ABreakerTravelPoint::GymDestinationId));

    // And symmetrically, which is what makes travel two-way rather than a trap.
    ABreakerTravelPoint* AtGym = NewObject<ABreakerTravelPoint>();
    AtGym->ExcludedDestinationId = ABreakerTravelPoint::GymDestinationId;
    const TArray<FBreakerTravelDestination> FromGym = AtGym->GetAvailableDestinations();
    TestFalse(TEXT("A point at the gym does not offer the gym"),
        OffersId(FromGym, ABreakerTravelPoint::GymDestinationId));
    TestTrue(TEXT("A point at the gym still offers the way back to the hub"),
        OffersId(FromGym, ABreakerTravelPoint::HubDestinationId));

    // Excluding a destination shortens both lists by exactly one — it filters,
    // it does not empty.
    ABreakerTravelPoint* Unfiltered = NewObject<ABreakerTravelPoint>();
    const int32 UnfilteredCount = Unfiltered->GetAvailableDestinations().Num();
    TestEqual(TEXT("Exclusion removes exactly one entry"), FromHub.Num(), UnfilteredCount - 1);

    // THE REASON THE SCREEN MUST ITERATE GetAvailableDestinations AND NOT THE
    // REGISTRY. Exclusion is a LIST rule, not a permission: SelectDestination
    // knows nothing about ExcludedDestinationId and will happily accept the id
    // of the place the point is standing in. Pinned here because it is a
    // silent trap for the next person who builds a caller — the filtering has
    // to happen where the choices are offered, because it does not happen
    // where they are honoured.
    bool bBroadcast = false;
    AtHub->OnDestinationSelected.AddLambda([&bBroadcast](FName, APawn*) { bBroadcast = true; });
    TestTrue(TEXT("SelectDestination does not itself enforce the exclusion"),
        AtHub->SelectDestination(ABreakerTravelPoint::HubDestinationId, nullptr));
    TestTrue(TEXT("The excluded id would have broadcast if it were ever offered"), bBroadcast);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTravelPickerRefusalTest,
    "RiorsEdge.UI.TravelPicker.RefusedSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTravelPickerRefusalTest::RunTest(const FString& Parameters)
{
    // The picker's refusal path: a card built before a destination went away is
    // clicked, SelectDestination returns false, and the screen must stay open
    // saying so rather than close on a departure that never happened. What is
    // testable here is the half that decides it — the false return and the
    // silence on the delegate.
    ABreakerTravelPoint* Point = NewObject<ABreakerTravelPoint>();
    int32 BroadcastCount = 0;
    Point->OnDestinationSelected.AddLambda([&BroadcastCount](FName, APawn*) { ++BroadcastCount; });

    TestFalse(TEXT("An id that was never registered is refused"),
        Point->SelectDestination(FName(TEXT("NotARealDestination")), nullptr));
    TestEqual(TEXT("A refused id does not broadcast"), BroadcastCount, 0);

    // NAME_None is the value SelectedTravelDestinationId holds when the screen
    // has nothing marked, so it is the id a bug would most plausibly send.
    TestFalse(TEXT("NAME_None is refused"), Point->SelectDestination(NAME_None, nullptr));
    TestEqual(TEXT("NAME_None does not broadcast"), BroadcastCount, 0);

    // A refusal leaves the point usable: the failed attempt must not have armed
    // or consumed anything, so the very next click on a real card still works.
    TestTrue(TEXT("A real id still works after a refusal"),
        Point->SelectDestination(ABreakerTravelPoint::GymDestinationId, nullptr));
    TestEqual(TEXT("The accepted id broadcasts once"), BroadcastCount, 1);

    return true;
}

#endif
