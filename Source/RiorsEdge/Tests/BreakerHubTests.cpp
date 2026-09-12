#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Game/BreakerHubBuilder.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

// The registry tests are world-free: they exercise ABreakerTravelPoint's
// static registry and id lookup. The plaza test at the end builds the hub
// into a fixture world for one surface assertion; station clearance and
// vendor routes are BreakerHubStationTests.cpp's. What this suite does NOT
// cover: the NPC dialogue the hub reuses (already covered by
// RiorsEdge.Interaction.DialogueIntegrity in BreakerDialogueTests.cpp), and
// the OnDestinationSelected delegate's actual wiring to gym travel (that
// binding lives in ABreakerGameMode, outside this file's territory, and is
// unwritten as of this test).

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
    // GENERAL destinations include gym, Anchor, Fernhall, earned
    // erased Earths and the packaged regional prototypes. The old "no third destination without checking
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
    TestEqual(TEXT("Eleven general destinations include five packaged regional prototypes"), GeneralCount, 11);
    TestEqual(TEXT("Exactly one door-only destination: the Local Rift"), DoorOnlyCount, 1);

    // A travel point excludes its current destination; other reachable entries
    // appear in the existing multi-card picker.
    FBreakerTravelDestination Found;
    TestTrue(TEXT("The hub is a real destination"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::HubDestinationId, Found));
    TestTrue(TEXT("The hub destination is enabled"), Found.bEnabled);
    TestTrue(TEXT("The gym and the hub are different destinations"),
        ABreakerTravelPoint::HubDestinationId != ABreakerTravelPoint::GymDestinationId);
    TestTrue(TEXT("The earned Earth destination has its actual stable identity"),
        ABreakerTravelPoint::FindDestination(ABreakerTravelPoint::ErasedEarthDestinationId, Found));
    TestTrue(TEXT("Earth is enabled general travel, not a generated Rift door"), Found.bEnabled && !Found.bDoorOnly);
    TestFalse(TEXT("An absent player cannot bypass the earned Earth gate"), ABreakerTravelPoint::CanEnterErasedEarth(nullptr));

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

// THE PLAZA IS GROUND (O279). The hub's floor is a composer-less slab — an
// engine cube scaled flat — and under O279 a slab wears the tiled ground
// material tinted by role rather than the flat shape colour. Found by the
// label the builder gives it, the way the outliner would.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHubPlazaGroundMaterialTest,
    "RiorsEdge.Game.Hub.PlazaWearsGroundMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHubPlazaGroundMaterialTest::RunTest(const FString& Parameters)
{
    UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Breaker/Materials/M_BreakerGround.M_BreakerGround"));
    if (!TestNotNull(TEXT("Shipped ground material M_BreakerGround exists"), Ground)) return false;

    UWorld::InitializationValues Initialization;
    Initialization.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("Hub fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    if (!TestNotNull(TEXT("Hub keeps its travel point"),
        UBreakerHubBuilder::BuildHub(World, FTransform(FVector(100, 200, 40))))) return false;

    const AStaticMeshActor* Plaza = nullptr;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        if (IsValid(*It) && It->GetActorLabel() == TEXT("Runtime_HubPlaza")) { Plaza = *It; break; }
    if (!TestNotNull(TEXT("Built hub has its plaza"), Plaza)) return false;
    const UMaterialInstanceDynamic* Slab = Cast<UMaterialInstanceDynamic>(Plaza->GetStaticMeshComponent()->GetMaterial(0));
    if (!TestNotNull(TEXT("Plaza slot 0 is a dynamic instance"), Slab)) return false;
    TestTrue(TEXT("Plaza is parented to M_BreakerGround"), Slab->Parent == Ground);
    return true;
}

#endif
