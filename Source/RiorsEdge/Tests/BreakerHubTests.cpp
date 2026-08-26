#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/BreakerTravelPoint.h"

// World-free: everything here exercises ABreakerTravelPoint's static
// registry and id lookup, none of it needs a UWorld. What this suite does
// NOT cover: BuildHub's actual spawning (UBreakerHubBuilder::BuildHub,
// BuildPlazaAndBoundary, BuildVendors, BuildTravelPoint all require a
// UWorld to spawn actors into and are untested here), the NPC dialogue the
// hub reuses (already covered by RiorsEdge.Interaction.DialogueIntegrity in
// BreakerDialogueTests.cpp), and the OnDestinationSelected delegate's actual
// wiring to gym travel (that binding lives in ABreakerGameMode, outside this
// file's territory, and is unwritten as of this test).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHubTravelRegistryTest,
    "RiorsEdge.Game.Hub.TravelRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHubTravelRegistryTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerTravelDestination>& Registry = ABreakerTravelPoint::GetFallbackRegistry();

    // THE PIN IS NOW TWO COUNTS, because the registry holds two KINDS of
    // destination and one number could not tell them apart. Splitting it keeps
    // the original claim exactly as strong and adds the new one, rather than
    // loosening a 3 into a 4 and losing what the 3 was asserting.
    //
    // GENERAL destinations — three: the gym, the way back (the Anchor), and
    // the Fernhall approach. The old "no third destination without checking
    // the selection UI exists" reason is discharged: SBreakerMenu's travel
    // screen is a real multi-card picker over GetAvailableDestinations.
    //
    // DOOR-ONLY destinations — one: the Local Rift, offered by
    // ABreakerRiftDoor alone and by no general gate.
    //
    // Both counts stay pinned exactly rather than loosened to "at least one"
    // because every entry here is a REACHABILITY claim: a destination with no
    // dispatch behind it is a card that travels to a refusal log line. THE TWO
    // KINDS ARE DISPATCHED DIFFERENTLY and that is why they are counted apart
    // — a general destination is an id, routed by HandleHubTravelSelected to a
    // map; a door-only destination carries DATA (which rift), so it is routed
    // by the door's own OnRiftEntryRequested, which writes PendingRift before
    // travelling. Adding either kind means adding its dispatch, and these pins
    // are what make forgetting that a red instead of a shrug.
    int32 GeneralCount = 0;
    int32 DoorOnlyCount = 0;
    for (const FBreakerTravelDestination& Destination : Registry)
    {
        if (!Destination.bEnabled) continue;
        if (Destination.bDoorOnly) ++DoorOnlyCount; else ++GeneralCount;
    }
    TestEqual(TEXT("Exactly three general destinations: the gym, the way back, and Fernhall"), GeneralCount, 3);
    TestEqual(TEXT("Exactly one door-only destination: the Local Rift"), DoorOnlyCount, 1);

    // A travel point never offers the place it stands in, which is what keeps
    // each point at exactly one option and therefore inside the no-picker-yet
    // rule above.
    FBreakerTravelDestination Found;
    TestTrue(TEXT("The hub is a real destination"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::HubDestinationId, Found));
    TestTrue(TEXT("The hub destination is enabled"), Found.bEnabled);
    TestTrue(TEXT("The gym and the hub are different destinations"),
        ABreakerTravelPoint::HubDestinationId != ABreakerTravelPoint::GymDestinationId);

    // Ids are unique. Nothing in the registry today would break this, but a
    // second entry added later without checking this test would be a silent
    // ambiguity (FindDestination returns first match), so lock it down now.
    TSet<FName> SeenIds;
    bool bAllUnique = true;
    for (const FBreakerTravelDestination& Destination : Registry)
    {
        bool bAlreadySet = false;
        SeenIds.Add(Destination.Id, &bAlreadySet);
        if (bAlreadySet) bAllUnique = false;
    }
    TestTrue(TEXT("Destination ids are unique"), bAllUnique);

    // No destination carries NAME_None as its id — an unnamed entry could
    // never be selected by id and would be dead content.
    for (const FBreakerTravelDestination& Destination : Registry)
    {
        TestTrue(TEXT("Destination id is not NAME_None"), Destination.Id != NAME_None);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHubGymDestinationTest,
    "RiorsEdge.Game.Hub.GymDestinationStable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHubGymDestinationTest::RunTest(const FString& Parameters)
{
    // The gym destination exists under a stable id and is enabled.
    FBreakerTravelDestination Gym;
    const bool bFound = ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::GymDestinationId, Gym);
    TestTrue(TEXT("Gym destination is found by its stable id"), bFound);
    if (bFound)
    {
        TestTrue(TEXT("Gym destination is enabled"), Gym.bEnabled);
        TestEqual(TEXT("Gym destination id matches the stable constant"), Gym.Id, ABreakerTravelPoint::GymDestinationId);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHubUnknownDestinationRefusedTest,
    "RiorsEdge.Game.Hub.UnknownDestinationRefused",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHubUnknownDestinationRefusedTest::RunTest(const FString& Parameters)
{
    // An id that was never registered must be refused, not silently
    // accepted or defaulted to the first entry.
    FBreakerTravelDestination Unused;
    TestFalse(TEXT("Unknown id is not found"),
        ABreakerTravelPoint::FindDestination(FName(TEXT("NotARealDestination")), Unused));

    // SelectDestination on a fresh actor refuses the same unknown id and does
    // not broadcast. NewObject rather than a world spawn — the actor is
    // never placed in a level, only constructed, which is enough to exercise
    // SelectDestination's pure id-lookup path.
    ABreakerTravelPoint* TravelPoint = NewObject<ABreakerTravelPoint>();
    bool bBroadcast = false;
    TravelPoint->OnDestinationSelected.AddLambda([&bBroadcast](FName, APawn*) { bBroadcast = true; });
    const bool bSelected = TravelPoint->SelectDestination(FName(TEXT("NotARealDestination")), nullptr);
    TestFalse(TEXT("SelectDestination refuses an unknown id"), bSelected);
    TestFalse(TEXT("SelectDestination does not broadcast on refusal"), bBroadcast);

    // A known, enabled id (the gym) is accepted and does broadcast.
    const bool bSelectedGym = TravelPoint->SelectDestination(ABreakerTravelPoint::GymDestinationId, nullptr);
    TestTrue(TEXT("SelectDestination accepts the gym id"), bSelectedGym);
    TestTrue(TEXT("SelectDestination broadcasts on acceptance"), bBroadcast);

    return true;
}

#endif
