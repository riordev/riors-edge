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

    // Trash kills close the first objective and only the first. The count is
    // read from the definition rather than hardcoded, so retuning an O2
    // placeholder does not turn into a failing test.
    const int32 TrashRequired = Contract.Objectives[0].RequiredCount;
    for (int32 i = 0; i < TrashRequired; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    TestTrue(TEXT("Enough trash kills thin the spill"), Journal->HasFlag(FirstContractSpillThinned));
    TestFalse(TEXT("Trash does not count as the elite"), Journal->HasFlag(FirstContractEliteDown));
    TestEqual(TEXT("Still active with an objective open"), State(), EBreakerQuestState::Active);

    // Over-killing does not push the counter past the threshold once closed.
    UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    TestEqual(TEXT("A closed objective stops counting"), Journal->GetCounter(FirstContractKillCounter), TrashRequired);

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

// THE ACT I CHAIN: FirstContract -> Kess's salvage -> the Pattern -> Deeper.
// Chain order is enforced in DIALOGUE (each offer choice requires the previous
// turn-in), so this test drives the actual NPC content, not just the journal:
// an unofferable quest and an unreachable offer node are both failures here.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerQuestChainTest,
    "RiorsEdge.Save.QuestChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQuestChainTest::RunTest(const FString& Parameters)
{
    using namespace BreakerQuestFlags;

    // The registry must hold the whole chain, in play order: the HUD tracker
    // follows the FIRST live quest, so registry order is player-facing.
    const TArray<FBreakerQuestDefinition>& Quests = UBreakerQuestLibrary::GetFallbackQuests();
    const TArray<FName> ChainOrder = { TEXT("Quest.FirstContract"), TEXT("Quest.KessSalvage"), TEXT("Quest.Pattern"), TEXT("Quest.Deeper") };
    if (Quests.Num() < ChainOrder.Num()) { AddError(TEXT("The Act I chain is not fully registered")); return false; }
    for (int32 Index = 0; Index < ChainOrder.Num(); ++Index)
    {
        TestEqual(TEXT("Registry order is chain order"), Quests[Index].QuestId, ChainOrder[Index]);
    }

    FBreakerQuestDefinition Salvage, Pattern, Deeper;
    if (!UBreakerQuestLibrary::FindQuest(TEXT("Quest.KessSalvage"), Salvage)
        || !UBreakerQuestLibrary::FindQuest(TEXT("Quest.Pattern"), Pattern)
        || !UBreakerQuestLibrary::FindQuest(TEXT("Quest.Deeper"), Deeper))
    {
        AddError(TEXT("A chain quest is missing from the registry"));
        return false;
    }

    ABreakerNPC* Kess = NewObject<ABreakerNPC>();
    Kess->DialogueNodes = ABreakerNPC::MakeForgeKeeperDialogue();
    Kess->EntryOverrides = ABreakerNPC::MakeForgeKeeperEntries();
    ABreakerNPC* Quartermaster = NewObject<ABreakerNPC>();
    Quartermaster->DialogueNodes = ABreakerNPC::MakeQuartermasterDialogue();
    Quartermaster->EntryOverrides = ABreakerNPC::MakeQuartermasterEntries();

    FString Error;
    TestTrue(FString::Printf(TEXT("Kess dialogue validates (%s)"), *Error), Kess->ValidateDialogue(Error));
    TestTrue(FString::Printf(TEXT("Quartermaster dialogue validates (%s)"), *Error), Quartermaster->ValidateDialogue(Error));

    // "Visible" means REACHABLE THROUGH VISIBLE CHOICES from wherever the NPC
    // actually greets, not "on the greeting node itself". Mid-chain the entry
    // resolves to a closure node (Done / Warm) whose route-back to Start is
    // pinned below — the player clicks through exactly that hop to reach the
    // next offer, so the helper walks it too. Bounded so a dialogue cycle can
    // never hang the suite.
    const auto OfferVisible = [](ABreakerNPC* NPC, FName OfferNode, const FBreakerQuestFlagSet& Flags)
    {
        TArray<FName> Frontier{ NPC->ResolveStartNodeId(Flags) };
        TSet<FName> Seen(Frontier);
        for (int32 Hop = 0; Hop < 4 && !Frontier.IsEmpty(); ++Hop)
        {
            TArray<FName> Next;
            for (const FName NodeId : Frontier)
            {
                FBreakerDialogueNode Node;
                if (!NPC->FindDialogueNode(NodeId, Node)) continue;
                TArray<FBreakerDialogueChoice> Visible;
                NPC->GetVisibleChoices(Node, Flags, Visible);
                for (const FBreakerDialogueChoice& Choice : Visible)
                {
                    if (Choice.NextNodeId == OfferNode) return true;
                    // Only plain navigation is followed: a choice that SETS a
                    // flag is a commitment the player makes, not a hallway, and
                    // walking through it would let this helper "reach" an offer
                    // by silently accepting a different quest first.
                    if (Choice.SetsQuestFlag.IsNone() && !Choice.NextNodeId.IsNone() && !Seen.Contains(Choice.NextNodeId))
                    {
                        Seen.Add(Choice.NextNodeId);
                        Next.Add(Choice.NextNodeId);
                    }
                }
            }
            Frontier = MoveTemp(Next);
        }
        return false;
    };

    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    const auto StateOf = [&Journal](const FBreakerQuestDefinition& Quest) { return UBreakerQuestLibrary::ComputeQuestState(Quest, Journal->GetState()); };

    // GATING ORDER. A fresh character can be offered nothing but the first
    // contract; each later offer appears only when the previous link closed.
    TestFalse(TEXT("Q2 unofferable before Q1 turned in"), OfferVisible(Kess, TEXT("Salvage"), Journal->GetState()));
    TestFalse(TEXT("Q3 unofferable before Q2 turned in"), OfferVisible(Quartermaster, TEXT("Pattern"), Journal->GetState()));
    TestFalse(TEXT("Q4 unofferable before Q3 turned in"), OfferVisible(Quartermaster, TEXT("Deeper"), Journal->GetState()));

    // Close the first contract the way play does.
    Journal->SetFlag(FirstContractOffered);
    Journal->SetFlag(FirstContractAccepted);
    for (int32 i = 0; i < 5; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true);
    Journal->SetFlag(FirstContractTurnedIn);

    TestTrue(TEXT("Q1 closed opens Q2"), OfferVisible(Kess, TEXT("Salvage"), Journal->GetState()));
    TestFalse(TEXT("Q1 closed does not skip to Q3"), OfferVisible(Quartermaster, TEXT("Pattern"), Journal->GetState()));

    // Q2: FEED THE FORGE. Any rank feeds an uncounted-rank objective.
    Journal->SetFlag(Salvage.OfferedFlag);
    Journal->SetFlag(Salvage.AcceptedFlag);
    const int32 FeedstockRequired = Salvage.Objectives[0].RequiredCount;
    for (int32 i = 0; i < FeedstockRequired - 1; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true); // the spill's own elite counts too
    TestTrue(TEXT("The feedstock is taken"), Journal->HasFlag(KessSalvageFeedstock));
    TestEqual(TEXT("Q2 ready"), StateOf(Salvage), EBreakerQuestState::ReadyToTurnIn);
    TestEqual(TEXT("Kess opens on the salvage turn-in"), Kess->ResolveStartNodeId(Journal->GetState()), FName(TEXT("SalvageTurnIn")));
    Journal->SetFlag(Salvage.TurnedInFlag);
    TestEqual(TEXT("Q2 closed"), StateOf(Salvage), EBreakerQuestState::Complete);
    TestEqual(TEXT("The Forge is lit"), Kess->ResolveStartNodeId(Journal->GetState()), FName(TEXT("Warm")));

    // Mid-chain reachability: the Quartermaster now greets with the closed
    // first contract, and that node MUST route back to Start or the Q3 offer
    // could never be reached from the door she opens.
    TestEqual(TEXT("Quartermaster still opens on the closed contract"), Quartermaster->ResolveStartNodeId(Journal->GetState()), FName(TEXT("Done")));
    {
        FBreakerDialogueNode Done;
        TestTrue(TEXT("Done resolves"), Quartermaster->FindDialogueNode(TEXT("Done"), Done));
        TestTrue(TEXT("Done routes back to Start"), Done.Choices.ContainsByPredicate(
            [](const FBreakerDialogueChoice& Choice) { return Choice.NextNodeId == FName(TEXT("Start")); }));
    }
    TestTrue(TEXT("Q2 closed opens Q3"), OfferVisible(Quartermaster, TEXT("Pattern"), Journal->GetState()));
    TestFalse(TEXT("Q2 closed does not skip to Q4"), OfferVisible(Quartermaster, TEXT("Deeper"), Journal->GetState()));

    // Q3: THE PATTERN. Only the marked count.
    Journal->SetFlag(Pattern.OfferedFlag);
    Journal->SetFlag(Pattern.AcceptedFlag);
    for (int32 i = 0; i < 10; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, false);
    TestEqual(TEXT("Trash does not feed the pattern"), Journal->GetCounter(PatternEliteCounter), 0);
    const int32 MarkedRequired = Pattern.Objectives[0].RequiredCount;
    for (int32 i = 0; i < MarkedRequired; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true);
    TestTrue(TEXT("The marked are down"), Journal->HasFlag(PatternMarkedDown));
    TestEqual(TEXT("Quartermaster opens on the pattern turn-in"), Quartermaster->ResolveStartNodeId(Journal->GetState()), FName(TEXT("PatternTurnIn")));
    Journal->SetFlag(Pattern.TurnedInFlag);
    TestEqual(TEXT("Q3 closed"), StateOf(Pattern), EBreakerQuestState::Complete);

    TestTrue(TEXT("Q3 closed opens Q4"), OfferVisible(Quartermaster, TEXT("Deeper"), Journal->GetState()));

    // Q4: DEEPER. Elite-or-above only; the boss pays into it like any elite.
    Journal->SetFlag(Deeper.OfferedFlag);
    Journal->SetFlag(Deeper.AcceptedFlag);
    const int32 SweepRequired = Deeper.Objectives[0].RequiredCount;
    for (int32 i = 0; i < SweepRequired; ++i) UBreakerQuestLibrary::NotifyEnemyKilled(*Journal, true);
    TestTrue(TEXT("The sweep is done"), Journal->HasFlag(DeeperSweepDone));
    TestEqual(TEXT("Quartermaster opens on the capstone turn-in"), Quartermaster->ResolveStartNodeId(Journal->GetState()), FName(TEXT("DeeperTurnIn")));
    Journal->SetFlag(Deeper.TurnedInFlag);
    TestEqual(TEXT("Q4 closed"), StateOf(Deeper), EBreakerQuestState::Complete);
    TestEqual(TEXT("The sheet is clear"), Quartermaster->ResolveStartNodeId(Journal->GetState()), FName(TEXT("DeeperDone")));

    // The whole chain is flags, so the whole chain round-trips for free.
    UBreakerQuestJournal* Reloaded = NewObject<UBreakerQuestJournal>();
    Reloaded->RestoreFrom(Journal->GetState());
    for (const FName& QuestId : ChainOrder)
    {
        FBreakerQuestDefinition Quest;
        UBreakerQuestLibrary::FindQuest(QuestId, Quest);
        TestEqual(FString::Printf(TEXT("%s survives a restore"), *QuestId.ToString()),
            UBreakerQuestLibrary::ComputeQuestState(Quest, Reloaded->GetState()), EBreakerQuestState::Complete);
    }
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
