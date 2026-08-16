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

    const FName KessSalvageOffered(TEXT("Quest.KessSalvage.Offered"));
    const FName KessSalvageAccepted(TEXT("Quest.KessSalvage.Accepted"));
    const FName KessSalvageFeedstock(TEXT("Quest.KessSalvage.FeedstockTaken"));
    const FName KessSalvageTurnedIn(TEXT("Quest.KessSalvage.TurnedIn"));
    const FName KessSalvageKillCounter(TEXT("Quest.KessSalvage.Kills"));

    const FName PatternOffered(TEXT("Quest.Pattern.Offered"));
    const FName PatternAccepted(TEXT("Quest.Pattern.Accepted"));
    const FName PatternMarkedDown(TEXT("Quest.Pattern.MarkedDown"));
    const FName PatternTurnedIn(TEXT("Quest.Pattern.TurnedIn"));
    const FName PatternEliteCounter(TEXT("Quest.Pattern.EliteKills"));

    const FName DeeperOffered(TEXT("Quest.Deeper.Offered"));
    const FName DeeperAccepted(TEXT("Quest.Deeper.Accepted"));
    const FName DeeperSweepDone(TEXT("Quest.Deeper.SweepDone"));
    const FName DeeperTurnedIn(TEXT("Quest.Deeper.TurnedIn"));
    const FName DeeperEliteCounter(TEXT("Quest.Deeper.EliteKills"));
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
        BreakerQuestFlags::KessSalvageOffered,
        BreakerQuestFlags::KessSalvageAccepted,
        BreakerQuestFlags::KessSalvageFeedstock,
        BreakerQuestFlags::KessSalvageTurnedIn,
        BreakerQuestFlags::PatternOffered,
        BreakerQuestFlags::PatternAccepted,
        BreakerQuestFlags::PatternMarkedDown,
        BreakerQuestFlags::PatternTurnedIn,
        BreakerQuestFlags::DeeperOffered,
        BreakerQuestFlags::DeeperAccepted,
        BreakerQuestFlags::DeeperSweepDone,
        BreakerQuestFlags::DeeperTurnedIn,
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

        // ---- Q2: FEED THE FORGE (Kess, after the first contract closes) ----
        // Kess's first ask. The chain gate lives on her offer choice
        // (requires FirstContractTurnedIn), not here — a definition stays a
        // flat description of the work, and the dialogue decides who may hear
        // about it. Registry order is chain order: the HUD tracker follows the
        // first live quest, so the list must read in the order players play it.
        FBreakerQuestDefinition Salvage;
        Salvage.QuestId = TEXT("Quest.KessSalvage");
        Salvage.Title = TEXT("FEED THE FORGE");
        // "FORGE KEEPER", not the display name "KESS — FORGE KEEPER": the
        // tracker prints "RETURN TO THE <GIVER>" and the em-dash form does not
        // survive that sentence.
        Salvage.Giver = TEXT("FORGE KEEPER");
        Salvage.OfferedFlag = BreakerQuestFlags::KessSalvageOffered;
        Salvage.AcceptedFlag = BreakerQuestFlags::KessSalvageAccepted;
        Salvage.TurnedInFlag = BreakerQuestFlags::KessSalvageTurnedIn;
        {
            FBreakerQuestObjective Feedstock;
            Feedstock.ObjectiveId = TEXT("Feedstock");
            Feedstock.Text = TEXT("Take feedstock from the spill");
            Feedstock.CompletionFlag = BreakerQuestFlags::KessSalvageFeedstock;
            Feedstock.ProgressCounter = BreakerQuestFlags::KessSalvageKillCounter;
            // O2 PLACEHOLDER, checked against content like the first
            // contract's 5: the named encounter fields three melee, two
            // LATTICE and one elite, and every rank feeds an uncounted-rank
            // objective — so 6 is exactly one full pass of the spill.
            Feedstock.RequiredCount = 6;
            Salvage.Objectives.Add(Feedstock);
        }
        Salvage.Reward.ItemCount = 1;                                     // O2 PLACEHOLDER
        Salvage.Reward.MinimumRarity = EBreakerItemRarity::Exceptional;   // O2 PLACEHOLDER
        Salvage.Reward.ItemLevel = 1;                                     // O2 PLACEHOLDER

        // ---- Q3: THE PATTERN (Quartermaster, after Kess's salvage) ---------
        FBreakerQuestDefinition Pattern;
        Pattern.QuestId = TEXT("Quest.Pattern");
        Pattern.Title = TEXT("THE PATTERN");
        Pattern.Giver = TEXT("QUARTERMASTER");
        Pattern.OfferedFlag = BreakerQuestFlags::PatternOffered;
        Pattern.AcceptedFlag = BreakerQuestFlags::PatternAccepted;
        Pattern.TurnedInFlag = BreakerQuestFlags::PatternTurnedIn;
        {
            FBreakerQuestObjective Marked;
            Marked.ObjectiveId = TEXT("Marked");
            Marked.Text = TEXT("Put the marked ones down");
            Marked.CompletionFlag = BreakerQuestFlags::PatternMarkedDown;
            Marked.ProgressCounter = BreakerQuestFlags::PatternEliteCounter;
            // O2 PLACEHOLDER. One elite stands per regroup of the named
            // ground, so 3 is three returns to the same spill — which IS the
            // fiction ("same ground, every time"), not a grind bolted onto it.
            // Wave play pays it too: the director promotes an elite from wave
            // 4 on.
            Marked.RequiredCount = 3;
            Marked.bRequiresEliteKill = true;
            Pattern.Objectives.Add(Marked);
        }
        Pattern.Reward.ItemCount = 1;                                     // O2 PLACEHOLDER
        Pattern.Reward.MinimumRarity = EBreakerItemRarity::Exceptional;   // O2 PLACEHOLDER
        Pattern.Reward.ItemLevel = 1;                                     // O2 PLACEHOLDER

        // ---- Q4: DEEPER (Quartermaster, the Act I capstone) ----------------
        // The brief asked for a boss clear. The kill tracker's only
        // discrimination is elite-or-above (HandleQuestKill collapses rank to
        // one bool), so a boss-only objective cannot be honestly counted
        // today — the honest fallback is a high elite-or-above count, and the
        // Field Marshal PAYS INTO it when killed, since anything above elite
        // counts for an elite objective. When rank reaches the notify API,
        // this objective can tighten without a save migration: the flag names
        // stay.
        FBreakerQuestDefinition Deeper;
        Deeper.QuestId = TEXT("Quest.Deeper");
        Deeper.Title = TEXT("DEEPER");
        Deeper.Giver = TEXT("QUARTERMASTER");
        Deeper.OfferedFlag = BreakerQuestFlags::DeeperOffered;
        Deeper.AcceptedFlag = BreakerQuestFlags::DeeperAccepted;
        Deeper.TurnedInFlag = BreakerQuestFlags::DeeperTurnedIn;
        {
            FBreakerQuestObjective Sweep;
            Sweep.ObjectiveId = TEXT("Sweep");
            Sweep.Text = TEXT("Sweep the source of the spill");
            Sweep.CompletionFlag = BreakerQuestFlags::DeeperSweepDone;
            Sweep.ProgressCounter = BreakerQuestFlags::DeeperEliteCounter;
            // O2 PLACEHOLDER. Five elite-or-above: the field elite, wave
            // promotions, and the Field Marshal all feed it, so the capstone
            // is heavier than Q3 without demanding any one source.
            Sweep.RequiredCount = 5;
            Sweep.bRequiresEliteKill = true;
            Deeper.Objectives.Add(Sweep);
        }
        // The capstone pays best-in-chain by COUNT, not rarity: Aberrant and
        // Anomalous are the endgame chase (the teaching order puts Aberrant
        // limits at A2-10), so an Act I camp contract has no business printing
        // either. Two Exceptionals is heavier than every prior link without
        // spending a rarity the campaign has not introduced.
        Deeper.Reward.ItemCount = 2;                                      // O2 PLACEHOLDER
        Deeper.Reward.MinimumRarity = EBreakerItemRarity::Exceptional;    // O2 PLACEHOLDER
        Deeper.Reward.ItemLevel = 1;                                      // O2 PLACEHOLDER

        return TArray<FBreakerQuestDefinition>{ Contract, Salvage, Pattern, Deeper };
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
