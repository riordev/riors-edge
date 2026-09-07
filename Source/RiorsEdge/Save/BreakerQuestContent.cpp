#include "Save/BreakerQuestContent.h"

#include "Data/BreakerDataFile.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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
    const FName SurvivorOffered(TEXT("Quest.Survivor.Offered"));
    const FName SurvivorAccepted(TEXT("Quest.Survivor.Accepted"));
    const FName SurvivorMet(TEXT("Quest.Survivor.Met"));
    const FName SurvivorExtracted(TEXT("Quest.Survivor.Extracted"));
    const FName SurvivorReachedAnchor(TEXT("Quest.Survivor.ReachedAnchor"));
    const FName SurvivorTurnedIn(TEXT("Quest.Survivor.TurnedIn"));
}

// ---------------------------------------------------------------------------
// THE LOADER — Data/quests.json into the flag registry and the quest list.
// ---------------------------------------------------------------------------
// The file is the registry. "flags" is the list ValidateQuestContent checks
// every authored reference against, and "quests" is the chain in play order.
// A row that fails any check below fails the WHOLE load: both lists come back
// empty behind an ensure, because a registry that dropped one flag and served
// the rest would turn an authored gate into a silent, permanent no-op — the
// exact failure the registry exists to catch.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    struct FBreakerQuestLoad
    {
        TArray<FName> Flags;
        TArray<FBreakerQuestDefinition> Quests;
        TArray<FString> Errors;
    };

    bool BreakerQuestReadString(const FJsonObject& Row, const TCHAR* Field, const FString& Where, FString& Out, FBreakerDataErrors& Errors)
    {
        if (!Row.TryGetStringField(Field, Out))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not a string"), *Where, Field));
            return false;
        }
        return true;
    }

    // A flag reference: non-empty, and listed in the file's own "flags".
    bool BreakerQuestReadFlag(const FJsonObject& Row, const TCHAR* Field, const FString& Where, const TArray<FName>& Registry, FName& Out, FBreakerDataErrors& Errors)
    {
        FString Name;
        if (!BreakerQuestReadString(Row, Field, Where, Name, Errors))
        {
            return false;
        }
        if (Name.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is empty"), *Where, Field));
            return false;
        }
        Out = FName(*Name);
        if (!Registry.Contains(Out))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" names \"%s\", which is not in \"flags\""), *Where, Field, *Name));
            return false;
        }
        return true;
    }

    bool BreakerQuestReadInt(const FJsonObject& Row, const TCHAR* Field, const FString& Where, int32& Out, FBreakerDataErrors& Errors)
    {
        double Value = 0.0;
        if (!Row.TryGetNumberField(Field, Value) || FMath::TruncToDouble(Value) != Value)
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not an integer"), *Where, Field));
            return false;
        }
        Out = static_cast<int32>(Value);
        return true;
    }

    bool BreakerQuestReadObjective(const FJsonObject& Row, const FString& QuestId, const TArray<FName>& Registry, FBreakerQuestObjective& Out, FBreakerDataErrors& Errors)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: an objective has no \"id\""), *QuestId));
            return false;
        }
        Out.ObjectiveId = FName(*Id);
        const FString Where = QuestId + TEXT(".") + Id;

        bool bOk = BreakerQuestReadString(Row, TEXT("text"), Where, Out.Text, Errors);
        bOk = BreakerQuestReadFlag(Row, TEXT("completionFlag"), Where, Registry, Out.CompletionFlag, Errors) && bOk;

        FString Counter;
        bOk = BreakerQuestReadString(Row, TEXT("progressCounter"), Where, Counter, Errors) && bOk;
        Out.ProgressCounter = Counter.IsEmpty() ? NAME_None : FName(*Counter);

        if (BreakerQuestReadInt(Row, TEXT("requiredCount"), Where, Out.RequiredCount, Errors))
        {
            if (Out.RequiredCount < 0)
            {
                Errors.Add(FString::Printf(TEXT("%s: \"requiredCount\" %d is negative"), *Where, Out.RequiredCount));
                bOk = false;
            }
            // A counted objective advances through its counter; without one the
            // count could never be reached.
            if (Out.RequiredCount > 0 && Out.ProgressCounter.IsNone())
            {
                Errors.Add(FString::Printf(TEXT("%s: \"requiredCount\" is %d but \"progressCounter\" is empty"), *Where, Out.RequiredCount));
                bOk = false;
            }
        }
        else
        {
            bOk = false;
        }

        if (!Row.TryGetBoolField(TEXT("requiresEliteKill"), Out.bRequiresEliteKill))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"requiresEliteKill\" is missing or not a bool"), *Where));
            bOk = false;
        }
        return bOk;
    }

    bool BreakerQuestReadReward(const FJsonObject& Row, const FString& QuestId, FBreakerQuestReward& Out, FBreakerDataErrors& Errors)
    {
        const FString Where = QuestId + TEXT(".reward");
        bool bOk = BreakerQuestReadInt(Row, TEXT("itemCount"), Where, Out.ItemCount, Errors);

        FString Rarity;
        if (!Row.TryGetStringField(TEXT("minimumRarity"), Rarity) || !BreakerDataFile::ParseEnum(Rarity, Out.MinimumRarity))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"minimumRarity\" is \"%s\", not an EBreakerItemRarity"), *Where, *Rarity));
            bOk = false;
        }

        bOk = BreakerQuestReadInt(Row, TEXT("itemLevel"), Where, Out.ItemLevel, Errors) && bOk;
        return bOk;
    }

    // One row into one definition. Returns false with every field-level
    // complaint recorded, not just the first.
    bool BreakerQuestReadRow(const FJsonObject& Row, const TArray<FName>& Registry, FBreakerQuestDefinition& Out, FBreakerDataErrors& Errors)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(TEXT("a quest row has no \"id\""));
            return false;
        }
        Out.QuestId = FName(*Id);

        bool bOk = BreakerQuestReadString(Row, TEXT("title"), Id, Out.Title, Errors);
        bOk = BreakerQuestReadString(Row, TEXT("giver"), Id, Out.Giver, Errors) && bOk;
        bOk = BreakerQuestReadFlag(Row, TEXT("offeredFlag"), Id, Registry, Out.OfferedFlag, Errors) && bOk;
        bOk = BreakerQuestReadFlag(Row, TEXT("acceptedFlag"), Id, Registry, Out.AcceptedFlag, Errors) && bOk;
        bOk = BreakerQuestReadFlag(Row, TEXT("turnedInFlag"), Id, Registry, Out.TurnedInFlag, Errors) && bOk;

        Out.Objectives.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Objectives = nullptr;
        if (!Row.TryGetArrayField(TEXT("objectives"), Objectives))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"objectives\" is missing or not an array"), *Id));
            bOk = false;
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *Objectives)
            {
                const TSharedPtr<FJsonObject>* ObjectiveObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(ObjectiveObject))
                {
                    Errors.Add(FString::Printf(TEXT("%s: an \"objectives\" entry is not an object"), *Id));
                    bOk = false;
                    continue;
                }
                FBreakerQuestObjective Objective;
                if (!BreakerQuestReadObjective(**ObjectiveObject, Id, Registry, Objective, Errors))
                {
                    bOk = false;
                    continue;
                }
                if (Out.Objectives.ContainsByPredicate([&Objective](const FBreakerQuestObjective& Other) { return Other.ObjectiveId == Objective.ObjectiveId; }))
                {
                    Errors.Add(FString::Printf(TEXT("%s: objective \"%s\" appears twice"), *Id, *Objective.ObjectiveId.ToString()));
                    bOk = false;
                    continue;
                }
                Out.Objectives.Add(Objective);
            }
        }

        const TSharedPtr<FJsonObject>* RewardObject = nullptr;
        if (!Row.TryGetObjectField(TEXT("reward"), RewardObject))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"reward\" is missing or not an object"), *Id));
            bOk = false;
        }
        else
        {
            bOk = BreakerQuestReadReward(**RewardObject, Id, Out.Reward, Errors) && bOk;
        }
        return bOk;
    }

    FBreakerQuestLoad BreakerQuestLoadData()
    {
        FBreakerQuestLoad Load;
        FBreakerDataErrors Errors;
        TArray<FName> Flags;
        TArray<FBreakerQuestDefinition> Quests;
        const FString File = UBreakerQuestLibrary::DataRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            // ---- flags -----------------------------------------------------
            const TArray<TSharedPtr<FJsonValue>>* FlagValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("flags"), FlagValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"flags\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *FlagValues)
                {
                    FString Name;
                    if (!Value.IsValid() || !Value->TryGetString(Name) || Name.IsEmpty())
                    {
                        Errors.Add(FString::Printf(TEXT("%s: a \"flags\" entry is not a name"), *File));
                        continue;
                    }
                    const FName Flag(*Name);
                    if (Flags.Contains(Flag))
                    {
                        Errors.Add(FString::Printf(TEXT("flags: \"%s\" appears twice"), *Name));
                        continue;
                    }
                    Flags.Add(Flag);
                }
            }

            // ---- quests ----------------------------------------------------
            const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
            if (!Root->TryGetArrayField(TEXT("quests"), RowValues))
            {
                Errors.Add(FString::Printf(TEXT("%s: no \"quests\" array"), *File));
            }
            else
            {
                for (const TSharedPtr<FJsonValue>& Value : *RowValues)
                {
                    const TSharedPtr<FJsonObject>* RowObject = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: a \"quests\" entry is not an object"), *File));
                        continue;
                    }
                    FBreakerQuestDefinition Quest;
                    if (!BreakerQuestReadRow(**RowObject, Flags, Quest, Errors))
                    {
                        continue;
                    }
                    if (Quests.ContainsByPredicate([&Quest](const FBreakerQuestDefinition& Other) { return Other.QuestId == Quest.QuestId; }))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: id appears twice"), *Quest.QuestId.ToString()));
                        continue;
                    }
                    Quests.Add(Quest);
                }
            }
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; the quest registry is EMPTY.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Flags = MoveTemp(Flags);
        Load.Quests = MoveTemp(Quests);
        return Load;
    }

    const FBreakerQuestLoad& BreakerQuestLoaded()
    {
        static const FBreakerQuestLoad Load = BreakerQuestLoadData();
        return Load;
    }
}

FString UBreakerQuestLibrary::DataRelativePath()
{
    return TEXT("Data/quests.json");
}

const TArray<FString>& UBreakerQuestLibrary::GetDataErrors()
{
    return BreakerQuestLoaded().Errors;
}

const TArray<FName>& UBreakerQuestLibrary::GetRegisteredFlags()
{
    return BreakerQuestLoaded().Flags;
}

bool UBreakerQuestLibrary::IsRegisteredFlag(FName Flag)
{
    return Flag != NAME_None && GetRegisteredFlags().Contains(Flag);
}

const TArray<FBreakerQuestDefinition>& UBreakerQuestLibrary::GetFallbackQuests()
{
    return BreakerQuestLoaded().Quests;
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

    // A file that did not load clean served EMPTY content, and empty content
    // references nothing: every check below would pass. The loaders' own
    // complaints are the validation result in that case.
    TArray<FString> LoadErrors = GetDataErrors();
    LoadErrors.Append(ABreakerNPC::GetDialogueErrors());
    if (!LoadErrors.IsEmpty())
    {
        OutError = FString::Join(LoadErrors, TEXT("\n"));
        return false;
    }

    TArray<FName> Referenced;
    for (const FBreakerQuestDefinition& Quest : GetFallbackQuests())
    {
        Referenced.Add(Quest.OfferedFlag);
        Referenced.Add(Quest.AcceptedFlag);
        Referenced.Add(Quest.TurnedInFlag);
        for (const FBreakerQuestObjective& Objective : Quest.Objectives) Referenced.Add(Objective.CompletionFlag);
    }
    for (const FBreakerDialogueRow& Row : ABreakerNPC::GetDialogueData().Npcs)
    {
        GatherDialogueFlags(Row.Nodes, Referenced);
        GatherEntryFlags(Row.Entries, Referenced);
    }

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
    for (const FBreakerDialogueRow& Row : ABreakerNPC::GetDialogueData().Npcs)
    {
        for (const FBreakerDialogueNode& Node : Row.Nodes)
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
