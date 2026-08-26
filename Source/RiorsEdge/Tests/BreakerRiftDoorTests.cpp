#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Game/BreakerRiftDefinition.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Interaction/BreakerTravelPoint.h"

// THE RIFT DOOR — the first thing that turns FBreakerRiftDefinition from a data
// model into a place. The data model, the deployment briefing and the interior's
// area-level read all shipped before anything could set PendingRift; the door is
// the write, and these are the rules that keep the write from happening in the
// wrong places.
//
// The reachability half matters most here. O40c: a feature merges with its
// in-game path, and the door's path is the travel picker — ABreakerCharacter
// iterates ABreakerTravelPoint and the menu drives both of the functions below
// through a base pointer, so a door being a SUBCLASS is what makes it reachable
// at all. If either of these stops being virtual the door still compiles, still
// spawns, and can never be entered.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftDestinationRegisteredTest,
    "RiorsEdge.Zone.RiftDoor.DestinationRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftDestinationRegisteredTest::RunTest(const FString& Parameters)
{
    FBreakerTravelDestination Rift;
    if (!TestTrue(TEXT("the Local Rift is in the travel registry"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::RiftDestinationId, Rift)))
    {
        return false;
    }
    // Registered so every id validates against ONE list, enabled because the
    // interior is real (the gym, for this landing), and door-only because it is
    // reached by walking to the door rather than chosen from a gate's menu.
    TestTrue(TEXT("the rift destination is enabled"), Rift.bEnabled);
    TestTrue(TEXT("the rift destination is door-only"), Rift.bDoorOnly);

    // Ids are a save-compatibility surface and a delegate payload; they must
    // stay distinct from every other destination.
    TestTrue(TEXT("the rift id is distinct from the gym, hub and Fernhall ids"),
        ABreakerTravelPoint::RiftDestinationId != ABreakerTravelPoint::GymDestinationId
        && ABreakerTravelPoint::RiftDestinationId != ABreakerTravelPoint::HubDestinationId
        && ABreakerTravelPoint::RiftDestinationId != ABreakerTravelPoint::FernhallDestinationId);

    // The rift is the ONLY door-only entry today. If a second one is added,
    // ABreakerRiftDoor's filter starts offering it too, which is a decision
    // rather than an accident — this is where that decision surfaces.
    int32 DoorOnlyCount = 0;
    for (const FBreakerTravelDestination& Destination : ABreakerTravelPoint::GetFallbackRegistry())
    {
        if (Destination.bDoorOnly) ++DoorOnlyCount;
    }
    TestEqual(TEXT("exactly one door-only destination exists"), DoorOnlyCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftDoorOffersOnlyTheRiftTest,
    "RiorsEdge.Zone.RiftDoor.OffersOnlyTheRift",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftDoorOffersOnlyTheRiftTest::RunTest(const FString& Parameters)
{
    // BOTH SIDES OF THE FILTER, because either one alone is satisfied by a
    // broken implementation: a door that offers everything passes the "offers
    // the rift" half, and a gate that offers nothing passes the "does not offer
    // the rift" half.

    // A general gate must not offer it. Without this a player could walk into a
    // rift from the Anchor with no door having authored WHICH rift — PendingRift
    // unset, and the interior silently built at the dev fallback area level.
    ABreakerTravelPoint* Gate = NewObject<ABreakerTravelPoint>();
    Gate->ExcludedDestinationId = ABreakerTravelPoint::HubDestinationId;
    bool bGateOffersRift = false;
    for (const FBreakerTravelDestination& Destination : Gate->GetAvailableDestinations())
    {
        if (Destination.Id == ABreakerTravelPoint::RiftDestinationId) bGateOffersRift = true;
    }
    TestFalse(TEXT("a general travel point never offers the rift"), bGateOffersRift);
    TestTrue(TEXT("and it still offers ordinary destinations"), Gate->GetAvailableDestinations().Num() > 0);

    // The door offers it, and offers nothing else. Walking up to a rift asks one
    // question, and it is not "where would you like to go".
    ABreakerRiftDoor* Door = NewObject<ABreakerRiftDoor>();
    const TArray<FBreakerTravelDestination> Offered = Door->GetAvailableDestinations();
    TestEqual(TEXT("the door offers exactly one destination"), Offered.Num(), 1);
    if (Offered.Num() == 1)
    {
        TestEqual(TEXT("and it is the rift"), Offered[0].Id, ABreakerTravelPoint::RiftDestinationId);
    }

    // VIRTUAL DISPATCH THROUGH A BASE POINTER is the reachability contract: the
    // menu holds ABreakerTravelPoint* and never knows a door is a door. Asserted
    // through a base pointer deliberately — calling it on the derived type would
    // pass even if the function stopped being virtual.
    ABreakerTravelPoint* AsBase = Door;
    TestEqual(TEXT("the door's filter survives a base-pointer call, which is how the menu calls it"),
        AsBase->GetAvailableDestinations().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftDoorEntryTest,
    "RiorsEdge.Zone.RiftDoor.EntryIsFree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftDoorEntryTest::RunTest(const FString& Parameters)
{
    ABreakerRiftDoor* Door = NewObject<ABreakerRiftDoor>();
    Door->Rift.AreaName = FText::FromString(TEXT("Fernhall Substation"));
    Door->Rift.AreaLevel = 5;
    Door->Rift.Tier = EBreakerRiftTier::Campaign;

    int32 Raised = 0;
    FBreakerRiftDefinition Carried;
    Door->OnRiftEntryRequested.AddLambda(
        [&Raised, &Carried](const FBreakerRiftDefinition& Rift, APawn*)
        {
            ++Raised;
            Carried = Rift;
        });

    // O122: a campaign rift is entered FREELY. No key, no cost, no condition —
    // so selection succeeds with nothing else set up, and this assertion is what
    // makes a future entry gate a deliberate change rather than a quiet one.
    TestTrue(TEXT("entering a campaign rift is refused by nothing"),
        Door->SelectDestination(ABreakerTravelPoint::RiftDestinationId, nullptr));
    TestEqual(TEXT("the door raised its entry delegate once"), Raised, 1);

    // THE DEFINITION TRAVELS WITH THE REQUEST, which is the whole reason the
    // door has a delegate of its own: the travel delegate carries an id, and an
    // id cannot say which rift. A door that raised only an id would leave the
    // game mode writing PendingRift from nothing.
    TestEqual(TEXT("the door's own definition is what it hands over"), Carried.AreaLevel, 5);
    TestEqual(TEXT("including its tier"), Carried.Tier, EBreakerRiftTier::Campaign);
    TestTrue(TEXT("and the carried definition is SET, which is what the game mode requires"),
        Carried.IsSet());

    // O123 through the carried tier: campaign prints UNLIMITED. Asserted here
    // rather than only at the readout so the door and the briefing cannot
    // disagree about which rule this rift runs under.
    TestEqual(TEXT("a campaign rift's death allowance reads UNLIMITED"),
        UBreakerRiftLibrary::GetDeathAllowanceReadout(Carried.Tier, 0), FString(TEXT("UNLIMITED")));

    // A door still refuses an id that is not its own, so it remains a legal
    // travel point rather than a thing that says yes to everything.
    TestFalse(TEXT("the door refuses an unknown destination"),
        Door->SelectDestination(FName(TEXT("Nowhere")), nullptr));
    TestEqual(TEXT("and raised nothing for it"), Raised, 1);

    // FAILING CLOSED, not merely hiding. A general point must REFUSE the rift id
    // if it is ever handed one by a caller that is not the picker — hiding a
    // choice and refusing it are different guarantees.
    ABreakerTravelPoint* Gate = NewObject<ABreakerTravelPoint>();
    int32 GateRaised = 0;
    Gate->OnDestinationSelected.AddLambda([&GateRaised](FName, APawn*) { ++GateRaised; });
    TestFalse(TEXT("a general travel point refuses the rift id outright"),
        Gate->SelectDestination(ABreakerTravelPoint::RiftDestinationId, nullptr));
    TestEqual(TEXT("and travels nowhere"), GateRaised, 0);

    // An unauthored door carries no rift, which is the state the game mode
    // refuses travel on rather than sending the player to a nameless interior.
    ABreakerRiftDoor* Blank = NewObject<ABreakerRiftDoor>();
    TestFalse(TEXT("a door with no definition on it is not SET"), Blank->Rift.IsSet());
    return true;
}

#endif
