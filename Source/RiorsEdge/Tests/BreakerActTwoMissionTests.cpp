#include "Misc/AutomationTest.h"
#include "Data/BreakerCensus.h"
#include "Game/BreakerZoneBuilder.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"
#include "Interaction/BreakerNPC.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerActTwoDialogueReachabilityTest, "RiorsEdge.Missions.ActTwo.VisibleOrders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerActTwoDialogueReachabilityTest::RunTest(const FString& Parameters)
{
    const FBreakerDialogueRow* Row = ABreakerNPC::GetDialogueData().Npcs.FindByPredicate(
        [](const FBreakerDialogueRow& Candidate) { return Candidate.Id == TEXT("Quartermaster"); });
    if (!TestNotNull(TEXT("Authored Quartermaster"), Row)) return false;
    ABreakerNPC* NPC = NewObject<ABreakerNPC>();
    NPC->StartNodeId = Row->StartNodeId; NPC->DialogueNodes = Row->Nodes; NPC->EntryOverrides = Row->Entries;
    const TCHAR* Prerequisites[] = { TEXT("Quest.Deeper.TurnedIn"), TEXT("Quest.AlteredContact.TurnedIn") };
    const TCHAR* Leads[] = { TEXT("AlteredContactLead"), TEXT("BreachLead") };
    const TCHAR* Offers[] = { TEXT("Quest.AlteredContact.Offered"), TEXT("Quest.Breach.Offered") };
    const TCHAR* Accepts[] = { TEXT("Quest.AlteredContact.Accepted"), TEXT("Quest.Breach.Accepted") };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FBreakerQuestFlagSet Flags; Flags.Add(Prerequisites[Index]);
        TestEqual(TEXT("Earned prerequisite selects the new orders"), NPC->ResolveStartNodeId(Flags), FName(Leads[Index]));
        FBreakerDialogueNode Lead;
        if (!TestTrue(TEXT("Lead resolves"), NPC->FindDialogueNode(Leads[Index], Lead))) return false;
        TArray<FBreakerDialogueChoice> Visible; NPC->GetVisibleChoices(Lead, Flags, Visible);
        const FBreakerDialogueChoice* Offer = Visible.FindByPredicate([&](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == FName(Offers[Index]); });
        if (!TestNotNull(TEXT("Offer choice visible before it sets Offered"), Offer)) return false;
        FBreakerDialogueNode OfferNode;
        if (!TestTrue(TEXT("Offer destination resolves"), NPC->FindDialogueNode(Offer->NextNodeId, OfferNode))) return false;
        Flags.Add(Offer->SetsQuestFlag);
        NPC->GetVisibleChoices(OfferNode, Flags, Visible);
        TestTrue(TEXT("Acceptance remains visible after offer"), Visible.ContainsByPredicate([&](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == FName(Accepts[Index]); }));
        FBreakerQuestFlagSet Unearned; NPC->GetVisibleChoices(Lead, Unearned, Visible);
        TestFalse(TEXT("Destination still protects the prerequisite turn-in"), Visible.ContainsByPredicate([&](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == FName(Offers[Index]); }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerActTwoMissionFlowTest, "RiorsEdge.Missions.ActTwo.EarnedBenchmark",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerActTwoMissionFlowTest::RunTest(const FString& Parameters)
{
    const auto& Missions = UBreakerMissionLibrary::GetMissions();
    const FBreakerMissionDefinition* ActOne = Missions.FindByPredicate([](const FBreakerMissionDefinition& M) { return M.MissionId == TEXT("Act1.Fernhall"); });
    const FBreakerMissionDefinition* ActTwo = Missions.FindByPredicate([](const FBreakerMissionDefinition& M) { return M.MissionId == TEXT("Act2.Breach"); });
    if (!TestNotNull(TEXT("Original chapter retained"), ActOne) || !TestNotNull(TEXT("Authored Act II chapter"), ActTwo)) return false;
    TestEqual(TEXT("Act II remains Act II, no fictional fourth act"), ActTwo->Act, 2);
    FBreakerQuestFlagSet Flags;
    TestTrue(TEXT("Contact cannot prepay before orders"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("fernhall.altered_contact"), Flags).IsEmpty());
    // Unit setup completes the prior authored chapter. The runtime probe owns
    // actual dialogue/travel/combat validation; this test owns gate/award law.
    for (const FBreakerMissionBeat& Beat : ActOne->Beats)
        for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    TestEqual(TEXT("Act I alone pays two"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 2);
    Flags.Add(TEXT("Quest.AlteredContact.Accepted"));
    TestTrue(TEXT("Acceptance without real arrival cannot finish the contact"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("fernhall.altered_contact"), Flags).IsEmpty());
    for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Fernhall"), Flags)) Flags.Add(Flag);
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    Journal->RestoreFrom(Flags);
    for (int32 Kill = 0; Kill < 20; ++Kill) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true);
    TestFalse(TEXT("Generic elite kills cannot impersonate the dedicated contact"), Journal->HasFlag(TEXT("Quest.AlteredContact.ContactDown")));
    TestTrue(TEXT("Wrong dedicated encounter cannot pay"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("Other.Contact"), Flags).IsEmpty());
    const TArray<FName> Contact = UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("fernhall.altered_contact"), Flags);
    TestEqual(TEXT("Actual dedicated encounter pays its one manual objective"), Contact.Num(), 1);
    for (FName Flag : Contact) Flags.Add(Flag);
    TestTrue(TEXT("Duplicate contact completion is inert"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("fernhall.altered_contact"), Flags).IsEmpty());
    Flags.Add(TEXT("Quest.AlteredContact.TurnedIn"));
    const FBreakerRiftDefinition Breach = UBreakerZoneBuilder::FernhallRiftFor(TEXT("breach"));
    TestEqual(TEXT("Breach has distinct stable encounter identity"), Breach.EncounterId, FName(TEXT("breach.marshalling")));
    TestEqual(TEXT("Breach owns the authored Field Marshal"), UBreakerMissionLibrary::BossForRift(Breach), FName(TEXT("FieldMarshal")));
    TestTrue(TEXT("Breach clear before accepted orders cannot prepay"), UBreakerMissionLibrary::RiftCompletionFlagsFor(Breach, Flags).IsEmpty());
    Flags.Add(TEXT("Quest.Breach.Accepted"));
    TestTrue(TEXT("Existing substation cannot impersonate the Breach"), UBreakerMissionLibrary::RiftCompletionFlagsFor(UBreakerZoneBuilder::FernhallRiftFor(TEXT("substation")), Flags).IsEmpty());
    Journal->RestoreFrom(Flags);
    for (int32 Kill = 0; Kill < 20; ++Kill) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true);
    TestFalse(TEXT("Generic elite kills cannot impersonate the Marshal clear"), Journal->HasFlag(TEXT("Quest.Breach.MarshalDown")));
    const TArray<FName> Completion = UBreakerMissionLibrary::RiftCompletionFlagsFor(Breach, Flags);
    TestEqual(TEXT("Distinct Breach completion finishes one objective"), Completion.Num(), 1);
    for (FName Flag : Completion) Flags.Add(Flag);
    TestEqual(TEXT("Boss clear without return pays no doctrine"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 2);
    Flags.Add(TEXT("Quest.Breach.TurnedIn"));
    TestEqual(TEXT("Earned return raises cumulative doctrine to four"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 4);
    TestTrue(TEXT("Re-clear cannot complete the same mission twice"), UBreakerMissionLibrary::RiftCompletionFlagsFor(Breach, Flags).IsEmpty());
    const FBreakerQuestFlagSet Restored = Flags;
    TestEqual(TEXT("Restored completed flags owe the same four, not six"), UBreakerMissionLibrary::DoctrinePointEntitlement(Restored), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWorldEncounterSchemaTest, "RiorsEdge.Missions.ActTwo.WorldEncounterSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWorldEncounterSchemaTest::RunTest(const FString& Parameters)
{
    const FString Json = BreakerCensus::ExportMissions(UBreakerMissionLibrary::GetRifts(), UBreakerMissionLibrary::GetMissions());
    FBreakerMissionData Data;
    TArray<FString> Errors;
    TestTrue(TEXT("Authored field encounter parses and validates"), UBreakerMissionLibrary::ParseMissionsJson(Json, Data, Errors));
    const FString Unknown = Json.Replace(TEXT("fernhall.altered_contact"), TEXT("unknown.contact"));
    TestFalse(TEXT("Unimplemented field encounter is refused atomically"), UBreakerMissionLibrary::ParseMissionsJson(Unknown, Data, Errors));
    TestTrue(TEXT("Refusal serves no partial missions"), Data.Missions.IsEmpty());
    const FString Both = Json.Replace(TEXT("\"worldEncounter\""), TEXT("\"rift\": \"fernhall.entry\", \"worldEncounter\""));
    TestFalse(TEXT("Encounter cannot name both world and rift sources"), UBreakerMissionLibrary::ParseMissionsJson(Both, Data, Errors));
    TestTrue(TEXT("Ambiguous source serves no partial missions"), Data.Missions.IsEmpty());
    return true;
}
#endif
