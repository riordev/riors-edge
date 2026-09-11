#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Characters/BreakerCharacter.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerRiftDoor.h"
#include "Save/BreakerQuestContent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCampaignMapTest, "RiorsEdge.World.LocalMap.CampaignTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCampaignMapTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init; Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if (!World) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Player = World->SpawnActor<ABreakerCharacter>();
    auto* Gate = World->SpawnActor<ABreakerTravelPoint>();
    auto* HomeGate = World->SpawnActor<ABreakerTravelPoint>();
    auto* Rift = World->SpawnActor<ABreakerRiftDoor>();
    if (!Player || !Gate || !HomeGate || !Rift) return false;
    Gate->SetActorLocation(FVector(12000,0,0)); Gate->ExcludedDestinationId = ABreakerTravelPoint::HubDestinationId;
    // A Fernhall-side gate: it stands in Fernhall, so it offers the hub.
    HomeGate->SetActorLocation(FVector(20000,0,0)); HomeGate->ExcludedDestinationId = ABreakerTravelPoint::FernhallDestinationId;
    Rift->SetActorLocation(FVector(16000,0,0)); Rift->Rift.EncounterId = TEXT("fernhall.entry");
    auto* Map = Player->GetLocalMap(); auto* Journal = Player->GetQuestJournal();
    auto ObjectiveAt = [&](AActor* Actor)
    {
        for (const auto& Marker : Map->GetMarkers())
            if (Marker.Location.Equals(Actor->GetActorLocation())) return Marker.bObjective;
        return false;
    };
    // Fresh flags: the beat is the Quartermaster's dialogue and no Quartermaster
    // stands in this world. The gate that offers the hub is the objective; the
    // gate standing in the hub is not.
    TestTrue(TEXT("Contact beat with no contact here marks the gate home"),ObjectiveAt(HomeGate));
    TestFalse(TEXT("Contact beat with no contact here leaves the hub's own gate unmarked"),ObjectiveAt(Gate));
    TestFalse(TEXT("Contact beat with no contact here leaves the rift door unmarked"),ObjectiveAt(Rift));
    auto* Quartermaster = ABreakerNPC::SpawnQuartermaster(World,FVector(4000,0,0),FRotator::ZeroRotator);
    auto* Kess = ABreakerNPC::SpawnForgeKeeper(World,FVector(8000,0,0),FRotator::ZeroRotator);
    if (!Quartermaster || !Kess) return false;
    TestEqual(TEXT("Native Quartermaster carries authored identity"), Quartermaster->DialogueId,FName(TEXT("Quartermaster")));
    TestTrue(TEXT("First campaign beat identifies actual distant Quartermaster"),ObjectiveAt(Quartermaster));
    TestFalse(TEXT("Contact present in this world takes the objective off the gate home"),ObjectiveAt(HomeGate));
    TestFalse(TEXT("Unrelated undiscovered Kess is not an objective"),ObjectiveAt(Kess));
    for (const auto& Marker : Map->GetMarkers())
    {
        TestFalse(TEXT("Campaign guidance does not fabricate exploration history"),Map->IsDiscovered(Marker.Id));
        TestEqual(TEXT("Only current objective visible before exploring"),Map->IsVisible(Marker),Marker.bObjective);
        if (Marker.bObjective)
        {
            Map->Restore({},Marker.Id);
            FBreakerLocalMapMarker Target;
            TestTrue(TEXT("Current objective tracking resolves without discovery"),Map->GetTrackedMarker(Target));
        }
    }
    Quartermaster->DisplayName = FText::FromString(TEXT("Changed display copy"));
    TestTrue(TEXT("Changing display copy cannot break campaign identity"),ObjectiveAt(Quartermaster));
    Journal->SetFlag(BreakerQuestFlags::FirstContractAccepted);
    TestTrue(TEXT("Acceptance advances objective to real travel gate"),ObjectiveAt(Gate));
    TestFalse(TEXT("Old giver no longer marked"),ObjectiveAt(Quartermaster));
    Journal->SetFlag(TEXT("Mission.Act1.Fernhall.Arrived"));
    TestTrue(TEXT("Arrival points to authored entry Rift"),ObjectiveAt(Rift));
    TestFalse(TEXT("Generic gate no longer marked after arrival"),ObjectiveAt(Gate));
    Journal->SetFlag(BreakerQuestFlags::FirstContractSpillThinned);
    Journal->SetFlag(BreakerQuestFlags::FirstContractEliteDown);
    TestTrue(TEXT("Completed objective returns to actual giver"),ObjectiveAt(Quartermaster));
    Journal->SetFlag(BreakerQuestFlags::FirstContractTurnedIn);
    TestTrue(TEXT("Turn-in advances to Kess's next mission"),ObjectiveAt(Kess));
    TestFalse(TEXT("Previous giver clears"),ObjectiveAt(Quartermaster));
    TestFalse(TEXT("Campaign tracker has meaningful text"),Map->GetCampaignObjective().IsEmpty());
    return true;
}
#endif
