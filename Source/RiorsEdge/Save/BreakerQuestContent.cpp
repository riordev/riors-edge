#include "Save/BreakerQuestContent.h"

#include "Interaction/BreakerNPC.h"

namespace BreakerQuestFlags
{
    const FName MetForgeKeeper(TEXT("Quest.MetForgeKeeper"));
    const FName AskedKessAboutRior(TEXT("Quest.AskedKessAboutRior"));
    const FName CheckedVendor(TEXT("Quest.CheckedVendor"));

    const FName FirstContractOffered(TEXT("Quest.FirstContract.Offered"));
    const FName FirstContractAccepted(TEXT("Quest.FirstContract.Accepted"));
    const FName FirstContractSpillThinned(TEXT("Quest.FirstContract.SpillThinned"));
    const FName FirstContractEliteDown(TEXT("Quest.FirstContract.EliteDown"));
    const FName FirstContractTurnedIn(TEXT("Quest.FirstContract.TurnedIn"));

    const FName FirstContractKillCounter(TEXT("Quest.FirstContract.Kills"));
    const FName FirstContractEliteCounter(TEXT("Quest.FirstContract.EliteKills"));
}

const TArray<FName>& UBreakerQuestLibrary::GetRegisteredFlags()
{
    static const TArray<FName> Registered =
    {
        BreakerQuestFlags::MetForgeKeeper,
        BreakerQuestFlags::AskedKessAboutRior,
        BreakerQuestFlags::CheckedVendor,
        BreakerQuestFlags::FirstContractOffered,
        BreakerQuestFlags::FirstContractAccepted,
        BreakerQuestFlags::FirstContractSpillThinned,
        BreakerQuestFlags::FirstContractEliteDown,
        BreakerQuestFlags::FirstContractTurnedIn,
    };
    return Registered;
}

bool UBreakerQuestLibrary::IsRegisteredFlag(FName Flag)
{
    return Flag != NAME_None && GetRegisteredFlags().Contains(Flag);
}

const TArray<FBreakerQuestDefinition>& UBreakerQuestLibrary::GetFallbackQuests()
{
    static const TArray<FBreakerQuestDefinition> Quests = []
    {
        FBreakerQuestDefinition Contract;
        Contract.QuestId = TEXT("Quest.FirstContract");
        Contract.Title = TEXT("THIN THE SPILL");
        Contract.Giver = TEXT("QUARTERMASTER");
        Contract.OfferedFlag = BreakerQuestFlags::FirstContractOffered;
        Contract.AcceptedFlag = BreakerQuestFlags::FirstContractAccepted;
        Contract.TurnedInFlag = BreakerQuestFlags::FirstContractTurnedIn;

        FBreakerQuestObjective Thin;
        Thin.ObjectiveId = TEXT("Thin");
        Thin.Text = TEXT("Thin the spill past the pad");
        Thin.CompletionFlag = BreakerQuestFlags::FirstContractSpillThinned;
        Thin.ProgressCounter = BreakerQuestFlags::FirstContractKillCounter;
        // O2 PLACEHOLDER, but not an arbitrary one: the encounter the contract
        // NAMES ("the spill out past the pad") spawns exactly three melee and
        // two LATTICE ranged alongside its elite. An objective the named
        // encounter cannot satisfy in one pass would send the player to wave
        // mode to finish a story beat.
        Thin.RequiredCount = 5;
        Contract.Objectives.Add(Thin);

        FBreakerQuestObjective Elite;
        Elite.ObjectiveId = TEXT("Elite");
        Elite.Text = TEXT("Put the elite down");
        Elite.CompletionFlag = BreakerQuestFlags::FirstContractEliteDown;
        Elite.ProgressCounter = BreakerQuestFlags::FirstContractEliteCounter;
        Elite.RequiredCount = 1; // O2 PLACEHOLDER — the encounter spawns exactly one elite.
        Elite.bRequiresEliteKill = true;
        Contract.Objectives.Add(Elite);

        Contract.Reward.ItemCount = 1;                                    // O2 PLACEHOLDER
        Contract.Reward.MinimumRarity = EBreakerItemRarity::Exceptional;  // O2 PLACEHOLDER
        Contract.Reward.ItemLevel = 1;                                    // O2 PLACEHOLDER

        return TArray<FBreakerQuestDefinition>{ Contract };
    }();
    return Quests;
}

bool UBreakerQuestLibrary::FindQuest(FName QuestId, FBreakerQuestDefinition& OutQuest)
{
    for (const FBreakerQuestDefinition& Quest : GetFallbackQuests())
    {
        if (Quest.QuestId == QuestId) { OutQuest = Quest; return true; }
    }
    return false;
}

bool UBreakerQuestLibrary::AreAllObjectivesComplete(const FBreakerQuestDefinition& Quest, const FBreakerQuestFlagSet& Flags)
{
    // A quest with no objectives is complete the moment it is accepted — that
    // is the "go and talk to X" shape, not a bug.
    for (const FBreakerQuestObjective& Objective : Quest.Objectives)
    {
        if (!Flags.Has(Objective.CompletionFlag)) return false;
    }
    return true;
}

EBreakerQuestState UBreakerQuestLibrary::ComputeQuestState(const FBreakerQuestDefinition& Quest, const FBreakerQuestFlagSet& Flags)
{
    // Ordered most-progressed first so a save that somehow carries a later flag
    // without an earlier one still reports the truthful state rather than
    // rewinding the player. The migration backfills the prerequisites; this
    // ordering is the belt to that pair of braces.
    if (Flags.Has(Quest.TurnedInFlag)) return EBreakerQuestState::Complete;
    if (Flags.Has(Quest.AcceptedFlag))
    {
        return AreAllObjectivesComplete(Quest, Flags) ? EBreakerQuestState::ReadyToTurnIn : EBreakerQuestState::Active;
    }
    if (Flags.Has(Quest.OfferedFlag)) return EBreakerQuestState::Offered;
    return EBreakerQuestState::NotOffered;
}

bool UBreakerQuestLibrary::PassesFlagConditions(const TArray<FName>& RequiredFlags, const TArray<FName>& BlockedByFlags, const FBreakerQuestFlagSet& Flags)
{
    if (!Flags.HasAll(RequiredFlags)) return false;
    if (Flags.HasAny(BlockedByFlags)) return false;
    return true;
}

int32 UBreakerQuestLibrary::NotifyEnemyKilled(UBreakerQuestJournal& Journal, bool bEliteOrAbove)
{
    int32 Completed = 0;
    for (const FBreakerQuestDefinition& Quest : GetFallbackQuests())
    {
        // Only an ACTIVE quest counts. Killing things before accepting a
        // contract must not pre-complete it — the objective is the work the
        // player agreed to do, and a pre-filled counter reads as a bug.
        if (ComputeQuestState(Quest, Journal.GetState()) != EBreakerQuestState::Active) continue;
        for (const FBreakerQuestObjective& Objective : Quest.Objectives)
        {
            if (Objective.RequiredCount <= 0 || Objective.ProgressCounter == NAME_None) continue;
            if (Objective.bRequiresEliteKill && !bEliteOrAbove) continue;
            if (Journal.HasFlag(Objective.CompletionFlag)) continue;
            if (Journal.AddProgress(Objective.ProgressCounter, 1, Objective.RequiredCount, Objective.CompletionFlag)) ++Completed;
        }
    }
    return Completed;
}

namespace
{
    // Collects every flag a piece of authored content references, so validation
    // can compare the whole set against the registry in one pass.
    void GatherDialogueFlags(const TArray<FBreakerDialogueNode>& Nodes, TArray<FName>& Out)
    {
        for (const FBreakerDialogueNode& Node : Nodes)
        {
            Out.Append(Node.RequiredFlags);
            Out.Append(Node.BlockedByFlags);
            for (const FBreakerDialogueChoice& Choice : Node.Choices)
            {
                if (Choice.SetsQuestFlag != NAME_None) Out.Add(Choice.SetsQuestFlag);
                Out.Append(Choice.RequiredFlags);
                Out.Append(Choice.BlockedByFlags);
            }
        }
    }

    void GatherEntryFlags(const TArray<FBreakerDialogueEntry>& Entries, TArray<FName>& Out)
    {
        for (const FBreakerDialogueEntry& Entry : Entries)
        {
            Out.Append(Entry.RequiredFlags);
            Out.Append(Entry.BlockedByFlags);
        }
    }
}

bool UBreakerQuestLibrary::ValidateQuestContent(FString& OutError)
{
    OutError.Reset();

    TArray<FName> Referenced;
    for (const FBreakerQuestDefinition& Quest : GetFallbackQuests())
    {
        Referenced.Add(Quest.OfferedFlag);
        Referenced.Add(Quest.AcceptedFlag);
        Referenced.Add(Quest.TurnedInFlag);
        for (const FBreakerQuestObjective& Objective : Quest.Objectives) Referenced.Add(Objective.CompletionFlag);
    }
    GatherDialogueFlags(ABreakerNPC::MakeForgeKeeperDialogue(), Referenced);
    GatherDialogueFlags(ABreakerNPC::MakeQuartermasterDialogue(), Referenced);
    GatherEntryFlags(ABreakerNPC::MakeForgeKeeperEntries(), Referenced);
    GatherEntryFlags(ABreakerNPC::MakeQuartermasterEntries(), Referenced);

    for (const FName& Flag : Referenced)
    {
        if (!IsRegisteredFlag(Flag))
        {
            OutError = FString::Printf(TEXT("Flag '%s' is referenced by content but is not in the registry (typo, or a missing registration)"), *Flag.ToString());
            return false;
        }
    }

    // The other half of the typo problem: a registered flag nothing ever sets
    // is a gate that can never open. Objective and turn-in flags are set by
    // combat and by dialogue respectively, so only the OFFER side is checked
    // here — every quest must be reachable from a conversation.
    TArray<FName> DialogueSets;
    for (const FBreakerDialogueNode& Node : ABreakerNPC::MakeQuartermasterDialogue())
    {
        for (const FBreakerDialogueChoice& Choice : Node.Choices) DialogueSets.Add(Choice.SetsQuestFlag);
    }
    for (const FBreakerDialogueNode& Node : ABreakerNPC::MakeForgeKeeperDialogue())
    {
        for (const FBreakerDialogueChoice& Choice : Node.Choices) DialogueSets.Add(Choice.SetsQuestFlag);
    }
    for (const FBreakerQuestDefinition& Quest : GetFallbackQuests())
    {
        if (!DialogueSets.Contains(Quest.OfferedFlag))
        {
            OutError = FString::Printf(TEXT("Quest '%s' can never be offered: no dialogue choice sets '%s'"), *Quest.QuestId.ToString(), *Quest.OfferedFlag.ToString());
            return false;
        }
        if (!DialogueSets.Contains(Quest.AcceptedFlag))
        {
            OutError = FString::Printf(TEXT("Quest '%s' can never be accepted: no dialogue choice sets '%s'"), *Quest.QuestId.ToString(), *Quest.AcceptedFlag.ToString());
            return false;
        }
        if (!DialogueSets.Contains(Quest.TurnedInFlag))
        {
            OutError = FString::Printf(TEXT("Quest '%s' can never be turned in: no dialogue choice sets '%s'"), *Quest.QuestId.ToString(), *Quest.TurnedInFlag.ToString());
            return false;
        }
    }
    return true;
}
