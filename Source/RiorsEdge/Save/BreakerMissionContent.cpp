#include "Save/BreakerMissionContent.h"

#include "Data/BreakerDataFile.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Game/BreakerZoneBuilder.h"
#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerQuestContent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <initializer_list>

// ---------------------------------------------------------------------------
// THE LOADER — Data/missions.json into the rift list and the mission list.
// ---------------------------------------------------------------------------
// A row that fails any check below fails the WHOLE load: both lists come back
// empty behind an ensure, because a mission list that dropped one beat and
// served the rest would turn an authored gate into a silent skip — the exact
// failure the validator exists to catch.
namespace
{
    using BreakerDataFile::FBreakerDataErrors;

    // ---- field readers -----------------------------------------------------

    bool BreakerMissionReadString(const FJsonObject& Row, const TCHAR* Field, const FString& Where, FString& Out, FBreakerDataErrors& Errors)
    {
        if (!Row.TryGetStringField(Field, Out))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not a string"), *Where, Field));
            return false;
        }
        return true;
    }

    // A name that must be present and non-empty.
    bool BreakerMissionReadName(const FJsonObject& Row, const TCHAR* Field, const FString& Where, FName& Out, FBreakerDataErrors& Errors)
    {
        FString Value;
        if (!BreakerMissionReadString(Row, Field, Where, Value, Errors))
        {
            return false;
        }
        if (Value.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is empty"), *Where, Field));
            return false;
        }
        Out = FName(*Value);
        return true;
    }

    bool BreakerMissionReadInt(const FJsonObject& Row, const TCHAR* Field, const FString& Where, int32& Out, FBreakerDataErrors& Errors)
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

    bool BreakerMissionReadNameArray(const FJsonObject& Row, const TCHAR* Field, const FString& Where, TArray<FName>& Out, FBreakerDataErrors& Errors)
    {
        Out.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Row.TryGetArrayField(Field, Values))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"%s\" is missing or not an array"), *Where, Field));
            return false;
        }
        bool bOk = true;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Name;
            if (!Value.IsValid() || !Value->TryGetString(Name) || Name.IsEmpty())
            {
                Errors.Add(FString::Printf(TEXT("%s: a \"%s\" entry is not a name"), *Where, Field));
                bOk = false;
                continue;
            }
            const FName AsName(*Name);
            if (Out.Contains(AsName))
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" lists \"%s\" twice"), *Where, Field, *Name));
                bOk = false;
                continue;
            }
            Out.Add(AsName);
        }
        return bOk;
    }

    // The exporter writes exactly the fields a kind has; the loader holds the
    // file to the same rule, so a field on the wrong kind is a break rather
    // than an ignored key.
    bool BreakerMissionRefuseExtraFields(const FJsonObject& Row, std::initializer_list<const TCHAR*> Allowed, const FString& Where, FBreakerDataErrors& Errors)
    {
        bool bOk = true;
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Row.Values)
        {
            bool bAllowed = false;
            for (const TCHAR* Field : Allowed)
            {
                if (Pair.Key == Field) { bAllowed = true; break; }
            }
            if (!bAllowed)
            {
                Errors.Add(FString::Printf(TEXT("%s: \"%s\" is not a field of this kind"), *Where, *Pair.Key));
                bOk = false;
            }
        }
        return bOk;
    }

    // ---- one beat ----------------------------------------------------------

    bool BreakerMissionReadBeat(const FJsonObject& Row, const FString& MissionId, FBreakerMissionBeat& Out, FBreakerDataErrors& Errors, TArray<FString>& Warnings)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: a beat has no \"id\""), *MissionId));
            return false;
        }
        Out.BeatId = FName(*Id);
        const FString Where = MissionId + TEXT(".") + Id;

        FString KindName;
        if (!Row.TryGetStringField(TEXT("kind"), KindName) || !BreakerDataFile::ParseEnum(KindName, Out.Kind))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"kind\" is \"%s\", not a beat kind"), *Where, *KindName));
            return false;
        }

        bool bOk = true;
        switch (Out.Kind)
        {
        case EBreakerMissionBeatKind::Dialogue:
        case EBreakerMissionBeatKind::Return:
            bOk = BreakerMissionReadName(Row, TEXT("npc"), Where, Out.Npc, Errors);
            bOk = BreakerMissionReadName(Row, TEXT("node"), Where, Out.Node, Errors) && bOk;
            bOk = BreakerMissionReadName(Row, TEXT("completesOn"), Where, Out.CompletesOn, Errors) && bOk;
            bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("kind"), TEXT("npc"), TEXT("node"), TEXT("completesOn") }, Where, Errors) && bOk;
            break;

        case EBreakerMissionBeatKind::Travel:
            bOk = BreakerMissionReadName(Row, TEXT("destination"), Where, Out.Destination, Errors);
            bOk = BreakerMissionReadName(Row, TEXT("completesOn"), Where, Out.CompletesOn, Errors) && bOk;
            bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("kind"), TEXT("destination"), TEXT("completesOn") }, Where, Errors) && bOk;
            break;

        case EBreakerMissionBeatKind::Encounter:
            if (Row.HasField(TEXT("rift")) == Row.HasField(TEXT("worldEncounter")))
            {
                Errors.Add(Where + TEXT(": an Encounter names exactly one of rift or worldEncounter"));
                bOk = false;
            }
            else if (Row.HasField(TEXT("worldEncounter")))
                bOk = BreakerMissionReadName(Row, TEXT("worldEncounter"), Where, Out.WorldEncounter, Errors);
            else bOk = BreakerMissionReadName(Row, TEXT("rift"), Where, Out.Rift, Errors);
            bOk = BreakerMissionReadName(Row, TEXT("quest"), Where, Out.Quest, Errors) && bOk;
            bOk = BreakerMissionReadNameArray(Row, TEXT("objectives"), Where, Out.Objectives, Errors) && bOk;
            if (bOk && Out.Objectives.IsEmpty())
            {
                Errors.Add(FString::Printf(TEXT("%s: an Encounter counts no objectives"), *Where));
                bOk = false;
            }
            bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("kind"), TEXT("rift"), TEXT("worldEncounter"), TEXT("quest"), TEXT("objectives") }, Where, Errors) && bOk;
            break;

        case EBreakerMissionBeatKind::Boss:
        {
            bOk = BreakerMissionReadName(Row, TEXT("rift"), Where, Out.Rift, Errors);
            // The boss may be "" until the owner names it. That is a recorded
            // gap, not a break: the beat still has a rift and a flag to
            // complete on, so the mission plays; what it cannot do yet is
            // name what stands in the rift.
            FString BossName;
            if (BreakerMissionReadString(Row, TEXT("boss"), Where, BossName, Errors))
            {
                Out.Boss = BossName.IsEmpty() ? NAME_None : FName(*BossName);
                if (Out.Boss.IsNone())
                {
                    Warnings.Add(FString::Printf(TEXT("%s: \"boss\" is not yet named"), *Where));
                }
            }
            else
            {
                bOk = false;
            }
            bOk = BreakerMissionReadName(Row, TEXT("completesOn"), Where, Out.CompletesOn, Errors) && bOk;
            bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("kind"), TEXT("rift"), TEXT("boss"), TEXT("completesOn") }, Where, Errors) && bOk;
            break;
        }

        case EBreakerMissionBeatKind::Reward:
            bOk = BreakerMissionReadName(Row, TEXT("quest"), Where, Out.Quest, Errors);
            bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("kind"), TEXT("quest") }, Where, Errors) && bOk;
            break;

        case EBreakerMissionBeatKind::Unlock:
        {
            int32 Grants = 0;
            if (Row.HasField(TEXT("doctrinePoints")))
            {
                ++Grants;
                if (BreakerMissionReadInt(Row, TEXT("doctrinePoints"), Where, Out.DoctrinePoints, Errors))
                {
                    if (Out.DoctrinePoints <= 0)
                    {
                        Errors.Add(FString::Printf(TEXT("%s: \"doctrinePoints\" %d grants nothing"), *Where, Out.DoctrinePoints));
                        bOk = false;
                    }
                }
                else
                {
                    bOk = false;
                }
            }
            if (Row.HasField(TEXT("corePoint")))
            {
                ++Grants;
                bOk = BreakerMissionReadName(Row, TEXT("corePoint"), Where, Out.CorePoint, Errors) && bOk;
            }
            if (Row.HasField(TEXT("abilityToken")))
            {
                ++Grants;
                bOk = BreakerMissionReadName(Row, TEXT("abilityToken"), Where, Out.AbilityToken, Errors) && bOk;
            }
            if (Grants != 1)
            {
                Errors.Add(FString::Printf(TEXT("%s: an Unlock grants exactly one of \"doctrinePoints\", \"corePoint\", \"abilityToken\" (found %d)"), *Where, Grants));
                bOk = false;
            }
            bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("kind"), TEXT("doctrinePoints"), TEXT("corePoint"), TEXT("abilityToken") }, Where, Errors) && bOk;
            break;
        }
        }
        return bOk;
    }

    // ---- one mission -------------------------------------------------------

    bool BreakerMissionReadRow(const FJsonObject& Row, FBreakerMissionDefinition& Out, FBreakerDataErrors& Errors, TArray<FString>& Warnings)
    {
        FString Id;
        if (!Row.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
        {
            Errors.Add(TEXT("a mission row has no \"id\""));
            return false;
        }
        Out.MissionId = FName(*Id);

        bool bOk = true;
        if (BreakerMissionReadInt(Row, TEXT("act"), Id, Out.Act, Errors))
        {
            if (Out.Act < 1)
            {
                Errors.Add(FString::Printf(TEXT("%s: \"act\" %d is not an act"), *Id, Out.Act));
                bOk = false;
            }
        }
        else
        {
            bOk = false;
        }
        bOk = BreakerMissionReadString(Row, TEXT("title"), Id, Out.Title, Errors) && bOk;
        bOk = BreakerMissionReadNameArray(Row, TEXT("quests"), Id, Out.Quests, Errors) && bOk;

        Out.Beats.Reset();
        const TArray<TSharedPtr<FJsonValue>>* Beats = nullptr;
        if (!Row.TryGetArrayField(TEXT("beats"), Beats))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"beats\" is missing or not an array"), *Id));
            bOk = false;
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *Beats)
            {
                const TSharedPtr<FJsonObject>* BeatObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(BeatObject))
                {
                    Errors.Add(FString::Printf(TEXT("%s: a \"beats\" entry is not an object"), *Id));
                    bOk = false;
                    continue;
                }
                FBreakerMissionBeat Beat;
                if (!BreakerMissionReadBeat(**BeatObject, Id, Beat, Errors, Warnings))
                {
                    bOk = false;
                    continue;
                }
                if (Out.Beats.ContainsByPredicate([&Beat](const FBreakerMissionBeat& Other) { return Other.BeatId == Beat.BeatId; }))
                {
                    Errors.Add(FString::Printf(TEXT("%s: beat \"%s\" appears twice"), *Id, *Beat.BeatId.ToString()));
                    bOk = false;
                    continue;
                }
                Out.Beats.Add(Beat);
            }
        }
        bOk = BreakerMissionRefuseExtraFields(Row, { TEXT("id"), TEXT("act"), TEXT("title"), TEXT("quests"), TEXT("beats") }, Id, Errors) && bOk;
        return bOk;
    }

    // ---- the cross-file checks --------------------------------------------

    bool BreakerMissionFlagIsRegistered(FName Flag, const FBreakerMissionData& Data)
    {
        return UBreakerQuestLibrary::IsRegisteredFlag(Flag) || Data.Flags.Contains(Flag);
    }

    void BreakerMissionCheckFlag(FName Flag, const FString& Where, const FBreakerMissionData& Data, FBreakerDataErrors& Errors)
    {
        if (!BreakerMissionFlagIsRegistered(Flag, Data))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"completesOn\" names \"%s\", which is in neither the quest registry nor the mission flags"), *Where, *Flag.ToString()));
        }
    }

    // (npc, node) resolves, and one of the node's choices sets the flag the
    // beat completes on. A dialogue beat that no choice can close is a gate
    // that never opens.
    void BreakerMissionCheckDialogue(const FBreakerMissionBeat& Beat, const FString& Where, FBreakerDataErrors& Errors)
    {
        const FBreakerDialogueData& Dialogue = ABreakerNPC::GetDialogueData();
        const FBreakerDialogueRow* Row = Dialogue.Npcs.FindByPredicate([&Beat](const FBreakerDialogueRow& Candidate) { return Candidate.Id == Beat.Npc; });
        if (!Row)
        {
            Errors.Add(FString::Printf(TEXT("%s: \"npc\" \"%s\" is not a dialogue row"), *Where, *Beat.Npc.ToString()));
            return;
        }
        const FBreakerDialogueNode* Node = Row->Nodes.FindByPredicate([&Beat](const FBreakerDialogueNode& Candidate) { return Candidate.NodeId == Beat.Node; });
        if (!Node)
        {
            Errors.Add(FString::Printf(TEXT("%s: \"node\" \"%s\" is not a node of \"%s\""), *Where, *Beat.Node.ToString(), *Beat.Npc.ToString()));
            return;
        }
        const bool bSets = Node->Choices.ContainsByPredicate([&Beat](const FBreakerDialogueChoice& Choice) { return Choice.SetsQuestFlag == Beat.CompletesOn; });
        if (!bSets)
        {
            Errors.Add(FString::Printf(TEXT("%s: no choice on \"%s\".\"%s\" sets \"%s\""), *Where, *Beat.Npc.ToString(), *Beat.Node.ToString(), *Beat.CompletesOn.ToString()));
        }
    }

    void BreakerMissionCheckRiftId(FName Rift, const FString& Where, const FBreakerMissionData& Data, FBreakerDataErrors& Errors)
    {
        if (!Data.Rifts.ContainsByPredicate([Rift](const FBreakerMissionRift& Candidate) { return Candidate.RiftId == Rift; }))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"rift\" \"%s\" is not in \"rifts\""), *Where, *Rift.ToString()));
        }
    }

    // The quest resolves in the registry and is one of this mission's quests.
    const FBreakerQuestDefinition* BreakerMissionCheckQuest(FName QuestId, const FString& Where, const FBreakerMissionDefinition& Mission, const TArray<FBreakerQuestDefinition>& Registry, FBreakerDataErrors& Errors)
    {
        const FBreakerQuestDefinition* Quest = Registry.FindByPredicate([QuestId](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == QuestId; });
        if (!Quest)
        {
            Errors.Add(FString::Printf(TEXT("%s: \"quest\" \"%s\" is not a quest"), *Where, *QuestId.ToString()));
            return nullptr;
        }
        if (!Mission.Quests.Contains(QuestId))
        {
            Errors.Add(FString::Printf(TEXT("%s: \"quest\" \"%s\" is not in the mission's \"quests\""), *Where, *QuestId.ToString()));
        }
        return Quest;
    }

    // The quest a Return closes: the one whose turn-in flag it completes on.
    const FBreakerQuestDefinition* BreakerMissionQuestForTurnIn(FName Flag, const TArray<FBreakerQuestDefinition>& Registry)
    {
        return Registry.FindByPredicate([Flag](const FBreakerQuestDefinition& Candidate) { return Candidate.TurnedInFlag == Flag; });
    }

    bool BreakerMissionBossCloses(const FBreakerMissionBeat& Boss, const FBreakerQuestDefinition& Quest)
    {
        return Quest.Objectives.ContainsByPredicate([&Boss](const FBreakerQuestObjective& Objective) { return Objective.CompletionFlag == Boss.CompletesOn; });
    }

    void BreakerMissionValidate(FBreakerMissionData& Data, FBreakerDataErrors& Errors)
    {
        const TArray<FBreakerQuestDefinition>& Registry = UBreakerQuestLibrary::GetFallbackQuests();

        // ---- rifts: ids unique, every yard resolves ------------------------
        // FernhallRiftFor answers the entry yard for any name it does not
        // know, so "resolves" is: the entry yard, or a definition that
        // differs from the entry yard's.
        const FName EntryId = UBreakerZoneBuilder::FernhallRiftFor(NAME_None).EncounterId;
        for (int32 Index = 0; Index < Data.Rifts.Num(); ++Index)
        {
            const FBreakerMissionRift& Rift = Data.Rifts[Index];
            for (int32 Earlier = 0; Earlier < Index; ++Earlier)
            {
                if (Data.Rifts[Earlier].RiftId == Rift.RiftId)
                {
                    Errors.Add(FString::Printf(TEXT("rifts: \"%s\" appears twice"), *Rift.RiftId.ToString()));
                    break;
                }
            }
            if (!Rift.Yard.IsNone() && UBreakerZoneBuilder::FernhallRiftFor(Rift.Yard).EncounterId == EntryId)
            {
                Errors.Add(FString::Printf(TEXT("rifts.%s: yard \"%s\" has no rift definition"), *Rift.RiftId.ToString(), *Rift.Yard.ToString()));
            }
        }

        // ---- missions ------------------------------------------------------
        // Doctrine points are paid two per main-story benchmark, one benchmark
        // per act, and the file's total is held to the whole grant. The file
        // carries the acts that are authored, so the rule is stated per act
        // present and the total is capped: a file with one act grants one
        // benchmark, and the missing acts are the campaign's gap, counted by
        // DoctrinePointGrant minus the sum, not faked by an act-one grant of
        // eight.
        TMap<int32, int32> DoctrineByAct;
        int32 DoctrineTotal = 0;
        TArray<FName> CorePointsGranted;

        for (const FBreakerMissionDefinition& Mission : Data.Missions)
        {
            const FString MissionId = Mission.MissionId.ToString();
            for (const FName& QuestId : Mission.Quests)
            {
                if (!Registry.ContainsByPredicate([QuestId](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == QuestId; }))
                {
                    Errors.Add(FString::Printf(TEXT("%s: \"quests\" names \"%s\", which is not a quest"), *MissionId, *QuestId.ToString()));
                }
            }

            for (int32 Index = 0; Index < Mission.Beats.Num(); ++Index)
            {
                const FBreakerMissionBeat& Beat = Mission.Beats[Index];
                const FString Where = MissionId + TEXT(".") + Beat.BeatId.ToString();
                switch (Beat.Kind)
                {
                case EBreakerMissionBeatKind::Dialogue:
                    BreakerMissionCheckFlag(Beat.CompletesOn, Where, Data, Errors);
                    BreakerMissionCheckDialogue(Beat, Where, Errors);
                    break;

                case EBreakerMissionBeatKind::Travel:
                {
                    BreakerMissionCheckFlag(Beat.CompletesOn, Where, Data, Errors);
                    FBreakerTravelDestination Destination;
                    if (!ABreakerTravelPoint::FindDestination(Beat.Destination, Destination))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: \"destination\" \"%s\" is not in the travel registry"), *Where, *Beat.Destination.ToString()));
                    }
                    break;
                }

                case EBreakerMissionBeatKind::Encounter:
                {
                    if (Beat.WorldEncounter.IsNone()) BreakerMissionCheckRiftId(Beat.Rift, Where, Data, Errors);
                    else if (Beat.WorldEncounter != FName(TEXT("fernhall.altered_contact")))
                        Errors.Add(Where + TEXT(": worldEncounter has no authored field encounter"));
                    if (const FBreakerQuestDefinition* Quest = BreakerMissionCheckQuest(Beat.Quest, Where, Mission, Registry, Errors))
                    {
                        for (const FName& ObjectiveId : Beat.Objectives)
                        {
                            if (!Quest->Objectives.ContainsByPredicate([ObjectiveId](const FBreakerQuestObjective& Objective) { return Objective.ObjectiveId == ObjectiveId; }))
                            {
                                Errors.Add(FString::Printf(TEXT("%s: objective \"%s\" is not an objective of \"%s\""), *Where, *ObjectiveId.ToString(), *Beat.Quest.ToString()));
                            }
                            if (!Beat.WorldEncounter.IsNone())
                                for (const FBreakerQuestObjective& Objective : Quest->Objectives)
                                    if (Objective.ObjectiveId == ObjectiveId && (Objective.RequiredCount != 0 || !Objective.ProgressCounter.IsNone()))
                                        Errors.Add(Where + TEXT(": a dedicated world encounter uses manual objectives, never generic kill counters"));
                        }
                    }
                    break;
                }

                case EBreakerMissionBeatKind::Boss:
                    BreakerMissionCheckRiftId(Beat.Rift, Where, Data, Errors);
                    BreakerMissionCheckFlag(Beat.CompletesOn, Where, Data, Errors);
                    break;

                case EBreakerMissionBeatKind::Return:
                {
                    BreakerMissionCheckFlag(Beat.CompletesOn, Where, Data, Errors);
                    BreakerMissionCheckDialogue(Beat, Where, Errors);
                    const FBreakerQuestDefinition* Quest = BreakerMissionQuestForTurnIn(Beat.CompletesOn, Registry);
                    if (!Quest)
                    {
                        Errors.Add(FString::Printf(TEXT("%s: \"completesOn\" \"%s\" is not a quest's turn-in flag"), *Where, *Beat.CompletesOn.ToString()));
                        break;
                    }
                    if (!Mission.Quests.Contains(Quest->QuestId))
                    {
                        Errors.Add(FString::Printf(TEXT("%s: turns in \"%s\", which is not in the mission's \"quests\""), *Where, *Quest->QuestId.ToString()));
                    }
                    // No Return before the work it closes: an Encounter for
                    // the quest, or a Boss whose completion is one of the
                    // quest's objectives.
                    bool bWorkBefore = false;
                    for (int32 Earlier = 0; Earlier < Index && !bWorkBefore; ++Earlier)
                    {
                        const FBreakerMissionBeat& Other = Mission.Beats[Earlier];
                        bWorkBefore = (Other.Kind == EBreakerMissionBeatKind::Encounter && Other.Quest == Quest->QuestId)
                            || (Other.Kind == EBreakerMissionBeatKind::Boss && BreakerMissionBossCloses(Other, *Quest));
                    }
                    if (!bWorkBefore)
                    {
                        Errors.Add(FString::Printf(TEXT("%s: Return for \"%s\" precedes its Encounter"), *Where, *Quest->QuestId.ToString()));
                    }
                    break;
                }

                case EBreakerMissionBeatKind::Reward:
                {
                    if (const FBreakerQuestDefinition* Quest = BreakerMissionCheckQuest(Beat.Quest, Where, Mission, Registry, Errors))
                    {
                        bool bReturnBefore = false;
                        for (int32 Earlier = 0; Earlier < Index && !bReturnBefore; ++Earlier)
                        {
                            const FBreakerMissionBeat& Other = Mission.Beats[Earlier];
                            bReturnBefore = Other.Kind == EBreakerMissionBeatKind::Return && Other.CompletesOn == Quest->TurnedInFlag;
                        }
                        if (!bReturnBefore)
                        {
                            Errors.Add(FString::Printf(TEXT("%s: Reward for \"%s\" precedes its Return"), *Where, *Quest->QuestId.ToString()));
                        }
                    }
                    break;
                }

                case EBreakerMissionBeatKind::Unlock:
                    if (Beat.DoctrinePoints > 0)
                    {
                        DoctrineByAct.FindOrAdd(Mission.Act) += Beat.DoctrinePoints;
                        DoctrineTotal += Beat.DoctrinePoints;
                    }
                    if (!Beat.CorePoint.IsNone())
                    {
                        const FBreakerWorldPointSource* Source = UBreakerWorldPointLibrary::GetSources().FindByPredicate(
                            [&Beat](const FBreakerWorldPointSource& Candidate) { return Candidate.SourceId == Beat.CorePoint; });
                        if (!Source)
                        {
                            Errors.Add(FString::Printf(TEXT("%s: \"corePoint\" \"%s\" is not a world point source"), *Where, *Beat.CorePoint.ToString()));
                        }
                        else if (Source->Act != Mission.Act)
                        {
                            Errors.Add(FString::Printf(TEXT("%s: \"corePoint\" \"%s\" is an act %d source in an act %d mission"), *Where, *Beat.CorePoint.ToString(), Source->Act, Mission.Act));
                        }
                        if (CorePointsGranted.Contains(Beat.CorePoint))
                        {
                            Errors.Add(FString::Printf(TEXT("%s: \"corePoint\" \"%s\" is granted twice"), *Where, *Beat.CorePoint.ToString()));
                        }
                        CorePointsGranted.Add(Beat.CorePoint);
                    }
                    // An ability token has no registry to resolve against;
                    // the name is held non-empty and nothing more.
                    break;
                }
            }
        }

        // ---- the point budgets ---------------------------------------------
        for (const TPair<int32, int32>& Pair : DoctrineByAct)
        {
            if (Pair.Value != UBreakerProgressionLibrary::DoctrinePointsPerBenchmark)
            {
                Errors.Add(FString::Printf(TEXT("act %d grants %d doctrine points; a benchmark pays %d"), Pair.Key, Pair.Value, UBreakerProgressionLibrary::DoctrinePointsPerBenchmark));
            }
        }
        for (const FBreakerMissionDefinition& Mission : Data.Missions)
        {
            if (!DoctrineByAct.Contains(Mission.Act))
            {
                Errors.Add(FString::Printf(TEXT("act %d grants no doctrine points; a benchmark pays %d"), Mission.Act, UBreakerProgressionLibrary::DoctrinePointsPerBenchmark));
                DoctrineByAct.Add(Mission.Act, 0);
            }
        }
        if (DoctrineTotal > UBreakerProgressionLibrary::DoctrinePointGrant)
        {
            Errors.Add(FString::Printf(TEXT("the file grants %d doctrine points; the whole grant is %d"), DoctrineTotal, UBreakerProgressionLibrary::DoctrinePointGrant));
        }
        if (CorePointsGranted.Num() > UBreakerProgressionLibrary::CoreWorldPointGrant)
        {
            Errors.Add(FString::Printf(TEXT("the file grants %d Core points; the world grant is %d"), CorePointsGranted.Num(), UBreakerProgressionLibrary::CoreWorldPointGrant));
        }
    }

    // ---- the root ----------------------------------------------------------

    void BreakerMissionRead(const FJsonObject& Root, const FString& Where, FBreakerMissionData& Out, FBreakerDataErrors& Errors)
    {
        int32 Version = 0;
        if (BreakerMissionReadInt(Root, TEXT("version"), Where, Version, Errors) && Version != 1)
        {
            Errors.Add(FString::Printf(TEXT("%s: \"version\" %d is not 1"), *Where, Version));
        }

        const TArray<TSharedPtr<FJsonValue>>* RiftValues = nullptr;
        if (!Root.TryGetArrayField(TEXT("rifts"), RiftValues))
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"rifts\" array"), *Where));
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *RiftValues)
            {
                const TSharedPtr<FJsonObject>* RiftObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(RiftObject))
                {
                    Errors.Add(FString::Printf(TEXT("%s: a \"rifts\" entry is not an object"), *Where));
                    continue;
                }
                FBreakerMissionRift Rift;
                if (!BreakerMissionReadName(**RiftObject, TEXT("id"), TEXT("rifts"), Rift.RiftId, Errors))
                {
                    continue;
                }
                FString Yard;
                if (!BreakerMissionReadString(**RiftObject, TEXT("yard"), FString(TEXT("rifts.")) + Rift.RiftId.ToString(), Yard, Errors))
                {
                    continue;
                }
                Rift.Yard = Yard.IsEmpty() ? NAME_None : FName(*Yard);
                BreakerMissionRefuseExtraFields(**RiftObject, { TEXT("id"), TEXT("yard") }, FString(TEXT("rifts.")) + Rift.RiftId.ToString(), Errors);
                Out.Rifts.Add(Rift);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* RowValues = nullptr;
        if (!Root.TryGetArrayField(TEXT("missions"), RowValues))
        {
            Errors.Add(FString::Printf(TEXT("%s: no \"missions\" array"), *Where));
        }
        else
        {
            for (const TSharedPtr<FJsonValue>& Value : *RowValues)
            {
                const TSharedPtr<FJsonObject>* RowObject = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(RowObject))
                {
                    Errors.Add(FString::Printf(TEXT("%s: a \"missions\" entry is not an object"), *Where));
                    continue;
                }
                FBreakerMissionDefinition Mission;
                if (!BreakerMissionReadRow(**RowObject, Mission, Errors, Out.Warnings))
                {
                    continue;
                }
                if (Out.Missions.ContainsByPredicate([&Mission](const FBreakerMissionDefinition& Other) { return Other.MissionId == Mission.MissionId; }))
                {
                    Errors.Add(FString::Printf(TEXT("%s: id appears twice"), *Mission.MissionId.ToString()));
                    continue;
                }
                Out.Missions.Add(Mission);
                Out.Flags.Add(UBreakerMissionLibrary::ArrivedFlagFor(Mission.MissionId));
            }
        }
        BreakerMissionRefuseExtraFields(Root, { TEXT("version"), TEXT("rifts"), TEXT("missions") }, Where, Errors);

        BreakerMissionValidate(Out, Errors);
    }

    struct FBreakerMissionLoad
    {
        FBreakerMissionData Data;
        TArray<FString> Errors;
    };

    FBreakerMissionLoad BreakerMissionLoadData()
    {
        FBreakerMissionLoad Load;
        FBreakerDataErrors Errors;
        FBreakerMissionData Data;
        const FString File = UBreakerMissionLibrary::DataRelativePath();

        const TSharedPtr<FJsonObject> Root = BreakerDataFile::Load(File, Errors);
        if (Root.IsValid())
        {
            BreakerMissionRead(*Root, File, Data, Errors);
        }

        if (!Errors.IsClean())
        {
            Load.Errors = Errors.Messages;
            ensureMsgf(false, TEXT("%s failed to load; the mission list is EMPTY.\n%s"), *File, *Errors.Join());
            return Load;
        }
        Load.Data = MoveTemp(Data);
        return Load;
    }

    const FBreakerMissionLoad& BreakerMissionLoaded()
    {
        static const FBreakerMissionLoad Load = BreakerMissionLoadData();
        return Load;
    }
}

FString UBreakerMissionLibrary::DataRelativePath()
{
    return TEXT("Data/missions.json");
}

const TArray<FString>& UBreakerMissionLibrary::GetDataErrors()
{
    return BreakerMissionLoaded().Errors;
}

const TArray<FString>& UBreakerMissionLibrary::GetDataWarnings()
{
    return BreakerMissionLoaded().Data.Warnings;
}

const TArray<FBreakerMissionDefinition>& UBreakerMissionLibrary::GetMissions()
{
    return BreakerMissionLoaded().Data.Missions;
}

const TArray<FBreakerMissionRift>& UBreakerMissionLibrary::GetRifts()
{
    return BreakerMissionLoaded().Data.Rifts;
}

const TArray<FName>& UBreakerMissionLibrary::GetRegisteredMissionFlags()
{
    return BreakerMissionLoaded().Data.Flags;
}

bool UBreakerMissionLibrary::IsRegisteredMissionFlag(FName Flag)
{
    return Flag != NAME_None && GetRegisteredMissionFlags().Contains(Flag);
}

FName UBreakerMissionLibrary::ArrivedFlagFor(FName MissionId)
{
    if (MissionId.IsNone()) return NAME_None;
    return FName(*FString::Printf(TEXT("Mission.%s.Arrived"), *MissionId.ToString()));
}

bool UBreakerMissionLibrary::ParseMissionsJson(const FString& Json, FBreakerMissionData& Out, TArray<FString>& OutErrors)
{
    Out = FBreakerMissionData();
    OutErrors.Reset();
    FBreakerDataErrors Errors;

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Errors.Add(FString::Printf(TEXT("missions: not a JSON object (%s)"), *Reader->GetErrorMessage()));
    }
    else
    {
        BreakerMissionRead(*Root, TEXT("missions"), Out, Errors);
    }

    OutErrors = Errors.Messages;
    if (!Errors.IsClean())
    {
        Out = FBreakerMissionData();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// PROGRESS. Every function below is a pure read of a flag set.
// ---------------------------------------------------------------------------
namespace
{
    const FBreakerQuestDefinition* BreakerMissionFindQuest(FName QuestId)
    {
        return UBreakerQuestLibrary::GetFallbackQuests().FindByPredicate(
            [QuestId](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == QuestId; });
    }

    const FBreakerMissionRift* BreakerMissionFindRift(FName RiftId)
    {
        return UBreakerMissionLibrary::GetRifts().FindByPredicate(
            [RiftId](const FBreakerMissionRift& Candidate) { return Candidate.RiftId == RiftId; });
    }

    // The quest a flag belongs to, by the field the beat kind completes on.
    // The registry holds every flag unique across its rows, so "the quest
    // whose AcceptedFlag is this" is one row or none.
    const FBreakerQuestDefinition* BreakerMissionQuestByAccepted(FName Flag)
    {
        return UBreakerQuestLibrary::GetFallbackQuests().FindByPredicate(
            [Flag](const FBreakerQuestDefinition& Candidate) { return Candidate.AcceptedFlag == Flag; });
    }

    const FBreakerQuestDefinition* BreakerMissionQuestByTurnedIn(FName Flag)
    {
        return UBreakerQuestLibrary::GetFallbackQuests().FindByPredicate(
            [Flag](const FBreakerQuestDefinition& Candidate) { return Candidate.TurnedInFlag == Flag; });
    }

    // The objective a Boss beat completes on, across the registry: the same
    // match BreakerMissionBossCloses makes per quest, here over every row
    // because a beat alone does not name its mission.
    const FBreakerQuestObjective* BreakerMissionObjectiveByCompletion(FName Flag)
    {
        for (const FBreakerQuestDefinition& Quest : UBreakerQuestLibrary::GetFallbackQuests())
        {
            const FBreakerQuestObjective* Objective = Quest.Objectives.FindByPredicate(
                [Flag](const FBreakerQuestObjective& Candidate) { return Candidate.CompletionFlag == Flag; });
            if (Objective) return Objective;
        }
        return nullptr;
    }

    // An objective's line: its text, and its counter over its count when it
    // is counted. The counter is the one NotifyEnemyKilled raises
    // (Objective.ProgressCounter), clamped so a save that overshot reads as
    // done, not as more than done.
    FString BreakerMissionObjectiveLine(const FBreakerQuestObjective& Objective, const FBreakerQuestFlagSet& Flags)
    {
        FString Line = Objective.Text.ToUpper();
        if (Objective.RequiredCount > 0)
        {
            const int32 Count = FMath::Clamp(Flags.GetCounter(Objective.ProgressCounter), 0, Objective.RequiredCount);
            Line += FString::Printf(TEXT("  %d/%d"), Count, Objective.RequiredCount);
        }
        return Line;
    }

    // The gap branch for a Dialogue or Return whose flag is no quest's: the
    // NPC's name, unverbed. No shipped beat reaches it -- every act-one
    // Dialogue accepts a quest and every Return turns one in -- and the
    // tracker asks a line to name the story, so a beat that names none is
    // left to say who, and nothing more, until the owner writes what.
    FString BreakerMissionNpcName(FName Npc)
    {
        const FBreakerDialogueRow* Row = ABreakerNPC::GetDialogueData().Npcs.FindByPredicate(
            [Npc](const FBreakerDialogueRow& Candidate) { return Candidate.Id == Npc; });
        return Row ? Row->DisplayName.ToUpper() : FString();
    }
}

TArray<FName> UBreakerMissionLibrary::BeatCompletionFlags(const FBreakerMissionBeat& Beat)
{
    TArray<FName> Out;
    switch (Beat.Kind)
    {
    case EBreakerMissionBeatKind::Dialogue:
    case EBreakerMissionBeatKind::Travel:
    case EBreakerMissionBeatKind::Boss:
    case EBreakerMissionBeatKind::Return:
        Out.Add(Beat.CompletesOn);
        break;

    case EBreakerMissionBeatKind::Encounter:
        if (const FBreakerQuestDefinition* Quest = BreakerMissionFindQuest(Beat.Quest))
        {
            for (const FName& ObjectiveId : Beat.Objectives)
            {
                const FBreakerQuestObjective* Objective = Quest->Objectives.FindByPredicate(
                    [ObjectiveId](const FBreakerQuestObjective& Candidate) { return Candidate.ObjectiveId == ObjectiveId; });
                if (Objective) Out.Add(Objective->CompletionFlag);
            }
        }
        break;

    case EBreakerMissionBeatKind::Reward:
        if (const FBreakerQuestDefinition* Quest = BreakerMissionFindQuest(Beat.Quest))
        {
            Out.Add(Quest->TurnedInFlag);
        }
        break;

    case EBreakerMissionBeatKind::Unlock:
        // Complete when reached: the grant IS the beat.
        break;
    }
    return Out;
}

int32 UBreakerMissionLibrary::BeatsCompleted(const FBreakerMissionDefinition& Mission, const FBreakerQuestFlagSet& Flags)
{
    int32 Completed = 0;
    for (const FBreakerMissionBeat& Beat : Mission.Beats)
    {
        if (!Flags.HasAll(BeatCompletionFlags(Beat))) break;
        ++Completed;
    }
    return Completed;
}

const FBreakerMissionBeat* UBreakerMissionLibrary::CurrentBeat(const FBreakerMissionDefinition& Mission, const FBreakerQuestFlagSet& Flags)
{
    const int32 Completed = BeatsCompleted(Mission, Flags);
    return Mission.Beats.IsValidIndex(Completed) ? &Mission.Beats[Completed] : nullptr;
}

FString UBreakerMissionLibrary::TrackerLine(const FBreakerMissionBeat& Beat, const FBreakerQuestFlagSet& Flags)
{
    switch (Beat.Kind)
    {
    case EBreakerMissionBeatKind::Dialogue:
        if (const FBreakerQuestDefinition* Quest = BreakerMissionQuestByAccepted(Beat.CompletesOn))
        {
            return FString::Printf(SpeakToVerb, *Quest->Giver.ToUpper());
        }
        return BreakerMissionNpcName(Beat.Npc);

    case EBreakerMissionBeatKind::Return:
        if (const FBreakerQuestDefinition* Quest = BreakerMissionQuestByTurnedIn(Beat.CompletesOn))
        {
            return FString::Printf(ReturnToVerb, *Quest->Giver.ToUpper());
        }
        return BreakerMissionNpcName(Beat.Npc);

    case EBreakerMissionBeatKind::Encounter:
        if (const FBreakerQuestDefinition* Quest = BreakerMissionFindQuest(Beat.Quest))
        {
            for (const FName& ObjectiveId : Beat.Objectives)
            {
                const FBreakerQuestObjective* Objective = Quest->Objectives.FindByPredicate(
                    [ObjectiveId](const FBreakerQuestObjective& Candidate) { return Candidate.ObjectiveId == ObjectiveId; });
                if (!Objective || Flags.Has(Objective->CompletionFlag)) continue;
                return BreakerMissionObjectiveLine(*Objective, Flags);
            }
        }
        // Every named objective held: the beat is complete and never current,
        // so nothing is asked.
        return FString();

    case EBreakerMissionBeatKind::Boss:
    {
        if (const FBreakerQuestObjective* Objective = BreakerMissionObjectiveByCompletion(Beat.CompletesOn))
        {
            return BreakerMissionObjectiveLine(*Objective, Flags);
        }
        // A Boss whose flag is no objective's names where, not what: the
        // beat has no boss name yet (the loader's warning) and no line of
        // its own, so the rift's area is the truthful ask.
        const FBreakerMissionRift* Rift = BreakerMissionFindRift(Beat.Rift);
        return Rift ? UBreakerZoneBuilder::FernhallRiftFor(Rift->Yard).AreaName.ToString().ToUpper() : FString();
    }

    case EBreakerMissionBeatKind::Travel:
    {
        FBreakerTravelDestination Destination;
        if (ABreakerTravelPoint::FindDestination(Beat.Destination, Destination))
        {
            return Destination.DisplayName.ToString().ToUpper();
        }
        // The loader refuses a destination outside the registry, so this is
        // reachable only through a beat built by hand.
        return FString();
    }

    case EBreakerMissionBeatKind::Reward:
    case EBreakerMissionBeatKind::Unlock:
        // Paid, not asked: the corner is empty by rule.
        return FString();
    }
    return FString();
}

int32 UBreakerMissionLibrary::DoctrinePointEntitlement(const FBreakerQuestFlagSet& Flags)
{
    int32 Entitled = 0;
    for (const FBreakerMissionDefinition& Mission : GetMissions())
    {
        // An Unlock is complete when reached, so "reached" and "inside the
        // completed run" are the same test.
        const int32 Completed = BeatsCompleted(Mission, Flags);
        for (int32 Index = 0; Index < Completed; ++Index)
        {
            const FBreakerMissionBeat& Beat = Mission.Beats[Index];
            if (Beat.Kind == EBreakerMissionBeatKind::Unlock) Entitled += Beat.DoctrinePoints;
        }
    }
    return Entitled;
}

TArray<FName> UBreakerMissionLibrary::ArrivalFlagsFor(FName DestinationId, const FBreakerQuestFlagSet& Flags)
{
    TArray<FName> Out;
    if (DestinationId.IsNone()) return Out;
    for (const FBreakerMissionDefinition& Mission : GetMissions())
    {
        // Current only. Standing in Fernhall before the Quartermaster has
        // given the job does not count as having gone there for it; the
        // beat completes on the next arrival, once it is the beat in play.
        const FBreakerMissionBeat* Beat = CurrentBeat(Mission, Flags);
        if (Beat && Beat->Kind == EBreakerMissionBeatKind::Travel && Beat->Destination == DestinationId)
        {
            Out.Add(Beat->CompletesOn);
        }
    }
    return Out;
}

TArray<FName> UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(FName EncounterId, const FBreakerQuestFlagSet& Flags)
{
    TArray<FName> Result;
    if (EncounterId.IsNone()) return Result;
    for (const FBreakerMissionDefinition& Mission : GetMissions())
    {
        const FBreakerMissionBeat* Beat = CurrentBeat(Mission, Flags);
        if (!Beat || Beat->Kind != EBreakerMissionBeatKind::Encounter || Beat->WorldEncounter != EncounterId) continue;
        if (const FBreakerQuestDefinition* Quest = BreakerMissionFindQuest(Beat->Quest))
        {
            if (!Flags.Has(Quest->AcceptedFlag) || Flags.Has(Quest->TurnedInFlag)) continue;
            for (const FBreakerQuestObjective& Objective : Quest->Objectives)
                if (Beat->Objectives.Contains(Objective.ObjectiveId) && Objective.RequiredCount == 0
                    && Objective.ProgressCounter.IsNone() && !Flags.Has(Objective.CompletionFlag))
                    Result.AddUnique(Objective.CompletionFlag);
        }
    }
    return Result;
}

FName UBreakerMissionLibrary::BossForRift(const FBreakerRiftDefinition& Rift)
{
    if (!Rift.IsSet() || Rift.EncounterId.IsNone()) return NAME_None;
    for (const FBreakerMissionDefinition& Mission : GetMissions())
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
        {
            if (Beat.Kind != EBreakerMissionBeatKind::Boss || Beat.Boss.IsNone()) continue;
            const FBreakerMissionRift* Authored = BreakerMissionFindRift(Beat.Rift);
            if (Authored && UBreakerZoneBuilder::FernhallRiftFor(Authored->Yard).EncounterId == Rift.EncounterId)
                return Beat.Boss;
        }
    return NAME_None;
}

TArray<FName> UBreakerMissionLibrary::RiftCompletionFlagsFor(const FBreakerRiftDefinition& Rift, const FBreakerQuestFlagSet& Flags)
{
    TArray<FName> Out;
    if (!Rift.IsSet() || Rift.EncounterId.IsNone()) return Out;
    for (const FBreakerMissionDefinition& Mission : GetMissions())
    {
        // Current only, as above: clearing the substation before Deeper is
        // accepted is a run, not the sweep the story asked for.
        const FBreakerMissionBeat* Beat = CurrentBeat(Mission, Flags);
        if (!Beat || Beat->Kind != EBreakerMissionBeatKind::Boss) continue;
        const FBreakerMissionRift* BeatRift = BreakerMissionFindRift(Beat->Rift);
        if (!BeatRift) continue;
        if (UBreakerZoneBuilder::FernhallRiftFor(BeatRift->Yard).EncounterId == Rift.EncounterId)
        {
            Out.Add(Beat->CompletesOn);
        }
    }
    return Out;
}

bool UBreakerMissionLibrary::ValidateMissionContent(FString& OutError)
{
    OutError.Reset();
    // The loaders' own complaints are the validation result: a file that did
    // not load clean served EMPTY content, and empty content references
    // nothing.
    TArray<FString> LoadErrors = UBreakerQuestLibrary::GetDataErrors();
    LoadErrors.Append(ABreakerNPC::GetDialogueErrors());
    LoadErrors.Append(GetDataErrors());
    if (!LoadErrors.IsEmpty())
    {
        OutError = FString::Join(LoadErrors, TEXT("\n"));
        return false;
    }
    return true;
}
