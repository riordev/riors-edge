#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/BreakerNPC.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"

// A gated entry appears only with its flag set, and a blocked one disappears
// once its flag arrives. Before conditions existed the UI iterated every choice
// on a node unconditionally, so a flag could be written and never read.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGatedDialogueTest,
    "RiorsEdge.Interaction.GatedDialogue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGatedDialogueTest::RunTest(const FString& Parameters)
{
    using namespace BreakerQuestFlags;

    ABreakerNPC* Quartermaster = NewObject<ABreakerNPC>();
    Quartermaster->DialogueNodes = ABreakerNPC::MakeQuartermasterDialogue();
    Quartermaster->EntryOverrides = ABreakerNPC::MakeQuartermasterEntries();

    FString Error;
    TestTrue(FString::Printf(TEXT("Quartermaster dialogue validates (%s)"), *Error), Quartermaster->ValidateDialogue(Error));

    FBreakerDialogueNode Start;
    TestTrue(TEXT("Start resolves"), Quartermaster->FindDialogueNode(TEXT("Start"), Start));

    const auto HasChoiceTo = [](const TArray<FBreakerDialogueChoice>& Choices, FName NodeId)
    {
        return Choices.ContainsByPredicate([NodeId](const FBreakerDialogueChoice& Choice) { return Choice.NextNodeId == NodeId; });
    };

    // Fresh character: the job is offered, the follow-up is not.
    FBreakerQuestFlagSet Flags;
    TArray<FBreakerDialogueChoice> Visible;
    Quartermaster->GetVisibleChoices(Start, Flags, Visible);
    TestTrue(TEXT("A fresh player is offered the job"), HasChoiceTo(Visible, TEXT("Job")));
    TestFalse(TEXT("A fresh player has no progress line"), HasChoiceTo(Visible, TEXT("Progress")));

    // Accepted: the offer is blocked, the progress line appears.
    Flags.Add(FirstContractOffered);
    Flags.Add(FirstContractAccepted);
    Quartermaster->GetVisibleChoices(Start, Flags, Visible);
    TestFalse(TEXT("An accepted contract is not offered again"), HasChoiceTo(Visible, TEXT("Job")));
    TestTrue(TEXT("An accepted contract shows the progress line"), HasChoiceTo(Visible, TEXT("Progress")));

    // Kess: the contract acknowledgement is gated on the turn-in.
    ABreakerNPC* Kess = NewObject<ABreakerNPC>();
    Kess->DialogueNodes = ABreakerNPC::MakeForgeKeeperDialogue();
    Kess->EntryOverrides = ABreakerNPC::MakeForgeKeeperEntries();
    TestTrue(FString::Printf(TEXT("Kess dialogue validates (%s)"), *Error), Kess->ValidateDialogue(Error));

    FBreakerDialogueNode KessStart;
    Kess->FindDialogueNode(TEXT("Start"), KessStart);
    Kess->GetVisibleChoices(KessStart, Flags, Visible);
    TestFalse(TEXT("Kess does not mention an unfinished contract"), HasChoiceTo(Visible, TEXT("Contract")));
    Flags.Add(FirstContractTurnedIn);
    Kess->GetVisibleChoices(KessStart, Flags, Visible);
    TestTrue(TEXT("Kess acknowledges a closed contract"), HasChoiceTo(Visible, TEXT("Contract")));

    // Per-NPC entry state: which node the NPC opens on.
    FBreakerQuestFlagSet Fresh;
    TestEqual(TEXT("A fresh player gets the ordinary greeting"), Quartermaster->ResolveStartNodeId(Fresh), FName(TEXT("Start")));
    FBreakerQuestFlagSet Ready;
    Ready.Add(FirstContractOffered); Ready.Add(FirstContractAccepted);
    Ready.Add(FirstContractSpillThinned); Ready.Add(FirstContractEliteDown);
    TestEqual(TEXT("A finished contract opens on the turn-in"), Quartermaster->ResolveStartNodeId(Ready), FName(TEXT("ReadyTurnIn")));
    Ready.Add(FirstContractTurnedIn);
    TestEqual(TEXT("A closed contract opens on the closing line"), Quartermaster->ResolveStartNodeId(Ready), FName(TEXT("Done")));

    // Every node must keep an unconditional way out; a conversation a player
    // cannot leave is the failure mode conditions introduced.
    ABreakerNPC* Stranded = NewObject<ABreakerNPC>();
    FBreakerDialogueNode Trap;
    Trap.NodeId = TEXT("Start");
    Trap.SpeakerLine = TEXT("...");
    FBreakerDialogueChoice Gated;
    Gated.Text = TEXT("[Leave]");
    Gated.RequiredFlags = { MetForgeKeeper };
    Trap.Choices = { Gated };
    Stranded->DialogueNodes = { Trap };
    TestFalse(TEXT("A node with only gated choices fails validation"), Stranded->ValidateDialogue(Error));
    return true;
}

// The contract runs a full loop: offered, accepted, tracked against real kills,
// ready, turned in. Quest state is DERIVED from flags at every step — nothing
// below stores a state value anywhere.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerQuestLoopTest,
    "RiorsEdge.Save.QuestLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQuestLoopTest::RunTest(const FString& Parameters)
{
    using namespace BreakerQuestFlags;

    FBreakerQuestDefinition Contract;
    if (!UBreakerQuestLibrary::FindQuest(TEXT("Quest.FirstContract"), Contract))
    {
        AddError(TEXT("The first contract is not in the fallback quest registry"));
        return false;
    }

    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    const auto State = [&Journal, &Contract]() { return UBreakerQuestLibrary::ComputeQuestState(Contract, Journal->GetState()); };

    TestEqual(TEXT("Untouched"), State(), EBreakerQuestState::NotOffered);

    // Killing things before the contract exists must not pre-complete it.
    for (int32 i = 0; i < 20; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, i % 5 == 0);
    TestEqual(TEXT("Kills before the offer do nothing"), State(), EBreakerQuestState::NotOffered);
    TestEqual(TEXT("No counter was opened"), Journal->GetCounter(FirstContractKillCounter), 0);

    Journal->SetFlag(Contract.OfferedFlag);
    TestEqual(TEXT("Heard but not taken"), State(), EBreakerQuestState::Offered);

    Journal->SetFlag(Contract.AcceptedFlag);
    TestEqual(TEXT("Taken"), State(), EBreakerQuestState::Active);

    // Trash kills close the first objective and only the first.
    for (int32 i = 0; i < 8; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    TestTrue(TEXT("Eight trash kills thin the spill"), Journal->HasFlag(FirstContractSpillThinned));
    TestFalse(TEXT("Trash does not count as the elite"), Journal->HasFlag(FirstContractEliteDown));
    TestEqual(TEXT("Still active with an objective open"), State(), EBreakerQuestState::Active);

    // Over-killing does not push the counter past the threshold once closed.
    UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    TestEqual(TEXT("A closed objective stops counting"), Journal->GetCounter(FirstContractKillCounter), 8);

    UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true);
    TestTrue(TEXT("The elite kill closes the second objective"), Journal->HasFlag(FirstContractEliteDown));
    TestEqual(TEXT("Ready to report"), State(), EBreakerQuestState::ReadyToTurnIn);

    Journal->SetFlag(Contract.TurnedInFlag);
    TestEqual(TEXT("Closed"), State(), EBreakerQuestState::Complete);

    // The whole loop survives a restore, because it is only ever flags.
    UBreakerQuestJournal* Reloaded = NewObject<UBreakerQuestJournal>();
    Reloaded->RestoreFrom(Journal->GetState());
    TestEqual(TEXT("State is derived, so it round-trips for free"),
        UBreakerQuestLibrary::ComputeQuestState(Contract, Reloaded->GetState()), EBreakerQuestState::Complete);
    return true;
}

// A typo in a flag literal is a silent, permanent no-op. This is what turns it
// into a red test.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerQuestContentTest,
    "RiorsEdge.Save.QuestContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQuestContentTest::RunTest(const FString& Parameters)
{
    FString Error;
    TestTrue(FString::Printf(TEXT("Quest and dialogue content only reference registered flags (%s)"), *Error),
        UBreakerQuestLibrary::ValidateQuestContent(Error));

    // The migration's target names must be the ones content actually uses, or
    // an old save would be renamed into a flag nothing reads.
    TestTrue(TEXT("The migrated contract flag is registered"),
        UBreakerQuestLibrary::IsRegisteredFlag(BreakerQuestFlags::FirstContractAccepted));
    TestFalse(TEXT("The retired v1 spelling is not"),
        UBreakerQuestLibrary::IsRegisteredFlag(TEXT("Quest.AcceptedFirstContract")));
    return true;
}

#endif
