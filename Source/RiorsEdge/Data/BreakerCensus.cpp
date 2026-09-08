#include "Data/BreakerCensus.h"

#include "Abilities/BreakerAbilityData.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Dom/JsonValue.h"
#include "Interaction/BreakerNPC.h"
#include "Items/BreakerAffixLibrary.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename TEnum>
    FString BreakerCensusEnumName(TEnum Value)
    {
        return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
    }

    TSharedRef<FJsonValueString> BreakerCensusString(const FString& Value)
    {
        return MakeShared<FJsonValueString>(Value);
    }

    TArray<TSharedPtr<FJsonValue>> BreakerCensusNames(const TArray<FName>& Names)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FName& Name : Names)
        {
            Out.Add(BreakerCensusString(Name.ToString()));
        }
        return Out;
    }

    TSharedRef<FJsonObject> BreakerCensusEffect(const FBreakerNodeEffect& Effect)
    {
        TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("target"), BreakerCensusEnumName(Effect.StatTarget));
        Out->SetStringField(TEXT("bucket"), BreakerCensusEnumName(Effect.StatBucket));
        Out->SetNumberField(TEXT("valuePerRank"), Effect.ValuePerRank);
        Out->SetStringField(TEXT("condition"), BreakerCensusEnumName(Effect.Condition));
        TArray<TSharedPtr<FJsonValue>> Also;
        for (EBreakerBuildCondition Required : Effect.AlsoRequires)
        {
            Also.Add(BreakerCensusString(BreakerCensusEnumName(Required)));
        }
        Out->SetArrayField(TEXT("alsoRequires"), Also);
        return Out;
    }

    TSharedRef<FJsonObject> BreakerCensusNode(const UBreakerProgressionNode& Node)
    {
        TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("id"), Node.NodeId.ToString());
        Out->SetNumberField(TEXT("tier"), Node.Tier);
        Out->SetNumberField(TEXT("ranks"), Node.MaxRank);
        Out->SetNumberField(TEXT("cost"), Node.CostPerRank);
        Out->SetBoolField(TEXT("cornerstone"), Node.bCornerstone);
        Out->SetStringField(TEXT("constellation"), Node.Constellation.ToString());

        TArray<TSharedPtr<FJsonValue>> Effects;
        for (const FBreakerNodeEffect& Effect : Node.Effects)
        {
            Effects.Add(MakeShared<FJsonValueObject>(BreakerCensusEffect(Effect)));
        }
        Out->SetArrayField(TEXT("effects"), Effects);

        // Both containers, deduplicated, in authoring order. The consumer
        // census asks "does anything read this tag?" and does not care which
        // container the author put it in.
        TArray<FName> Tags;
        for (const FGameplayTag& Tag : Node.NodeTags) { Tags.AddUnique(Tag.GetTagName()); }
        for (const FGameplayTag& Tag : Node.GrantedTags) { Tags.AddUnique(Tag.GetTagName()); }
        Out->SetArrayField(TEXT("tags"), BreakerCensusNames(Tags));

        Out->SetArrayField(TEXT("exclusive"), BreakerCensusNames(Node.MutuallyExclusiveNodeIds));

        TArray<TSharedPtr<FJsonValue>> Prerequisites;
        for (const FBreakerNodePrerequisite& Prerequisite : Node.Prerequisites)
        {
            TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
            P->SetStringField(TEXT("id"), Prerequisite.NodeId.ToString());
            P->SetNumberField(TEXT("rank"), Prerequisite.RequiredRank);
            Prerequisites.Add(MakeShared<FJsonValueObject>(P));
        }
        Out->SetArrayField(TEXT("prerequisites"), Prerequisites);
        return Out;
    }
}

FString BreakerCensus::RelativePath()
{
    return TEXT("Data/progression.json");
}

TSharedRef<FJsonObject> BreakerCensus::Export(const TArray<UBreakerProgressionTree*>& Trees)
{
    TSharedRef<FJsonObject> Census = MakeShared<FJsonObject>();

    // Budgets, read from the same constexprs the game grants from, so a
    // ruling that moves a budget moves the census in the same build.
    TSharedRef<FJsonObject> Budgets = MakeShared<FJsonObject>();
    Budgets->SetNumberField(TEXT("core"), UBreakerProgressionLibrary::CorePointBudget);
    Budgets->SetNumberField(TEXT("doctrine"), UBreakerProgressionLibrary::DoctrinePointGrant);
    Census->SetObjectField(TEXT("budgets"), Budgets);

    // The lane register: every stat target and which of the three header
    // registers claims it. Reflection over the enum, the header's own
    // predicates for the flags. No second list to rot.
    TArray<TSharedPtr<FJsonValue>> Targets;
    for (int32 Value = 0; Value < static_cast<int32>(EBreakerNodeStatTarget::Count); ++Value)
    {
        const EBreakerNodeStatTarget Target = static_cast<EBreakerNodeStatTarget>(Value);
        TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
        T->SetStringField(TEXT("name"), BreakerCensusEnumName(Target));
        T->SetBoolField(TEXT("lane"), BreakerStatTargetHasAggregationLane(Target));
        T->SetBoolField(TEXT("rider"), BreakerStatTargetIsRiderDelivered(Target));
        T->SetBoolField(TEXT("affixOwned"), BreakerStatTargetIsAffixOwned(Target));
        Targets.Add(MakeShared<FJsonValueObject>(T));
    }
    Census->SetArrayField(TEXT("statTargets"), Targets);

    TArray<TSharedPtr<FJsonValue>> Conditions;
    for (int32 Value = 0; Value < static_cast<int32>(EBreakerBuildCondition::Count); ++Value)
    {
        Conditions.Add(BreakerCensusString(BreakerCensusEnumName(static_cast<EBreakerBuildCondition>(Value))));
    }
    Census->SetArrayField(TEXT("conditions"), Conditions);

    TArray<TSharedPtr<FJsonValue>> TreeValues;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        if (!Tree) { continue; }
        TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
        T->SetStringField(TEXT("id"), Tree->TreeId.ToString());
        T->SetStringField(TEXT("currency"), BreakerCensusEnumName(Tree->Currency));
        T->SetStringField(TEXT("requiredClass"), BreakerCensusEnumName(Tree->RequiredClass));
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) { continue; }
            Nodes.Add(MakeShared<FJsonValueObject>(BreakerCensusNode(*Node)));
        }
        T->SetArrayField(TEXT("nodes"), Nodes);
        // O212: the tree's constellations and the sector each is a wedge of,
        // in authoring order, read from the node table rather than listed.
        // Doctrine trees carry no constellation and export an empty array; a
        // constellation the sector map does not know exports "None", which
        // the wheel test refuses rather than the census inventing a sector.
        TArray<FName> SeenConstellations;
        TArray<TSharedPtr<FJsonValue>> Constellations;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node || Node->Constellation.IsNone() || SeenConstellations.Contains(Node->Constellation)) { continue; }
            SeenConstellations.Add(Node->Constellation);
            TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
            C->SetStringField(TEXT("name"), Node->Constellation.ToString());
            C->SetStringField(TEXT("sector"), BreakerCoreSectorOf(Node->Constellation).ToString());
            Constellations.Add(MakeShared<FJsonValueObject>(C));
        }
        T->SetArrayField(TEXT("constellations"), Constellations);
        TreeValues.Add(MakeShared<FJsonValueObject>(T));
    }
    Census->SetArrayField(TEXT("trees"), TreeValues);
    return Census;
}

namespace
{
    // The committed-file rule shared by both exports: "\n" whatever the
    // platform's LINE_TERMINATOR, one trailing newline.
    void BreakerCensusFinish(FString& Out)
    {
        Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
        if (!Out.EndsWith(TEXT("\n"))) { Out += TEXT("\n"); }
    }

    using FBreakerCensusWriter = TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;

    void BreakerCensusAffixRow(FBreakerCensusWriter& Writer, const FBreakerAffixDefinition& Affix, const TCHAR* Pool)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("id"), Affix.AffixId.ToString());
        Writer.WriteValue(TEXT("pool"), FString(Pool));
        Writer.WriteValue(TEXT("displayName"), Affix.DisplayName.ToString());
        Writer.WriteValue(TEXT("category"), BreakerCensusEnumName(Affix.Category));
        Writer.WriteValue(TEXT("target"), BreakerCensusEnumName(Affix.StatTarget));
        Writer.WriteValue(TEXT("bucket"), BreakerCensusEnumName(Affix.StatBucket));
        Writer.WriteArrayStart(TEXT("slots"));
        for (EBreakerEquipSlot Slot : Affix.AllowedSlots)
        {
            const FString Name = BreakerCensusEnumName(Slot);
            Writer.WriteValue(Name);
        }
        Writer.WriteArrayEnd();
        Writer.WriteValue(TEXT("valueAtT12"), Affix.ValueAtT12);
        Writer.WriteValue(TEXT("valueAtT1"), Affix.ValueAtT1);
        Writer.WriteValue(TEXT("rollWeight"), Affix.RollWeight);
        Writer.WriteValue(TEXT("condition"), BreakerCensusEnumName(Affix.Condition));
        Writer.WriteValue(TEXT("minimumRarity"), BreakerCensusEnumName(Affix.MinimumRarity));
        Writer.WriteValue(TEXT("pairedAffixId"), Affix.PairedAffixId.IsNone() ? FString() : Affix.PairedAffixId.ToString());
        Writer.WriteObjectEnd();
    }

    // NAME_None is "" in every data file; the loaders read "" back as None.
    FString BreakerCensusNameOrEmpty(FName Name)
    {
        return Name.IsNone() ? FString() : Name.ToString();
    }

    void BreakerCensusNameArray(FBreakerCensusWriter& Writer, const TCHAR* Field, const TArray<FName>& Names)
    {
        Writer.WriteArrayStart(Field);
        for (const FName& Name : Names)
        {
            // An lvalue on purpose: the const FString& overload is the one
            // that puts every array element on its own line.
            const FString Value = Name.ToString();
            Writer.WriteValue(Value);
        }
        Writer.WriteArrayEnd();
    }

    void BreakerCensusQuestRow(FBreakerCensusWriter& Writer, const FBreakerQuestDefinition& Quest)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("id"), Quest.QuestId.ToString());
        Writer.WriteValue(TEXT("title"), Quest.Title);
        Writer.WriteValue(TEXT("giver"), Quest.Giver);
        Writer.WriteValue(TEXT("offeredFlag"), BreakerCensusNameOrEmpty(Quest.OfferedFlag));
        Writer.WriteValue(TEXT("acceptedFlag"), BreakerCensusNameOrEmpty(Quest.AcceptedFlag));
        Writer.WriteValue(TEXT("turnedInFlag"), BreakerCensusNameOrEmpty(Quest.TurnedInFlag));
        Writer.WriteArrayStart(TEXT("objectives"));
        for (const FBreakerQuestObjective& Objective : Quest.Objectives)
        {
            Writer.WriteObjectStart();
            Writer.WriteValue(TEXT("id"), Objective.ObjectiveId.ToString());
            Writer.WriteValue(TEXT("text"), Objective.Text);
            Writer.WriteValue(TEXT("completionFlag"), BreakerCensusNameOrEmpty(Objective.CompletionFlag));
            Writer.WriteValue(TEXT("progressCounter"), BreakerCensusNameOrEmpty(Objective.ProgressCounter));
            Writer.WriteValue(TEXT("requiredCount"), Objective.RequiredCount);
            Writer.WriteValue(TEXT("requiresEliteKill"), Objective.bRequiresEliteKill);
            if (Objective.ProgressSource != EBreakerQuestProgressSource::Kill)
                Writer.WriteValue(TEXT("progressSource"), BreakerCensusEnumName(Objective.ProgressSource));
            Writer.WriteObjectEnd();
        }
        Writer.WriteArrayEnd();
        Writer.WriteObjectStart(TEXT("reward"));
        Writer.WriteValue(TEXT("itemCount"), Quest.Reward.ItemCount);
        Writer.WriteValue(TEXT("minimumRarity"), BreakerCensusEnumName(Quest.Reward.MinimumRarity));
        Writer.WriteValue(TEXT("itemLevel"), Quest.Reward.ItemLevel);
        if (Quest.Reward.Experience > 0) Writer.WriteValue(TEXT("experience"), Quest.Reward.Experience);
        Writer.WriteObjectEnd();
        Writer.WriteObjectEnd();
    }

    void BreakerCensusDialogueChoice(FBreakerCensusWriter& Writer, const FBreakerDialogueChoice& Choice)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("text"), Choice.Text);
        Writer.WriteValue(TEXT("nextNodeId"), BreakerCensusNameOrEmpty(Choice.NextNodeId));
        Writer.WriteValue(TEXT("setsQuestFlag"), BreakerCensusNameOrEmpty(Choice.SetsQuestFlag));
        BreakerCensusNameArray(Writer, TEXT("requiredFlags"), Choice.RequiredFlags);
        BreakerCensusNameArray(Writer, TEXT("blockedByFlags"), Choice.BlockedByFlags);
        Writer.WriteValue(TEXT("action"), BreakerCensusEnumName(Choice.Action));
        Writer.WriteObjectEnd();
    }

    void BreakerCensusDialogueNode(FBreakerCensusWriter& Writer, const FBreakerDialogueNode& Node)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("nodeId"), Node.NodeId.ToString());
        Writer.WriteValue(TEXT("speakerLine"), Node.SpeakerLine);
        BreakerCensusNameArray(Writer, TEXT("requiredFlags"), Node.RequiredFlags);
        BreakerCensusNameArray(Writer, TEXT("blockedByFlags"), Node.BlockedByFlags);
        Writer.WriteArrayStart(TEXT("choices"));
        for (const FBreakerDialogueChoice& Choice : Node.Choices)
        {
            BreakerCensusDialogueChoice(Writer, Choice);
        }
        Writer.WriteArrayEnd();
        Writer.WriteObjectEnd();
    }

    void BreakerCensusDialogueRow(FBreakerCensusWriter& Writer, const FBreakerDialogueRow& Row)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("id"), Row.Id.ToString());
        Writer.WriteValue(TEXT("displayName"), Row.DisplayName);
        Writer.WriteValue(TEXT("startNodeId"), BreakerCensusNameOrEmpty(Row.StartNodeId));
        Writer.WriteArrayStart(TEXT("nodes"));
        for (const FBreakerDialogueNode& Node : Row.Nodes)
        {
            BreakerCensusDialogueNode(Writer, Node);
        }
        Writer.WriteArrayEnd();
        Writer.WriteArrayStart(TEXT("entries"));
        for (const FBreakerDialogueEntry& Entry : Row.Entries)
        {
            Writer.WriteObjectStart();
            Writer.WriteValue(TEXT("startNodeId"), BreakerCensusNameOrEmpty(Entry.StartNodeId));
            BreakerCensusNameArray(Writer, TEXT("requiredFlags"), Entry.RequiredFlags);
            BreakerCensusNameArray(Writer, TEXT("blockedByFlags"), Entry.BlockedByFlags);
            Writer.WriteObjectEnd();
        }
        Writer.WriteArrayEnd();
        Writer.WriteObjectEnd();
    }

    // Exactly the fields the kind has, in the schema's order, after "id" and
    // "kind". The loader refuses a field on the wrong kind, so this and the
    // reader are the same list twice by construction.
    void BreakerCensusMissionBeat(FBreakerCensusWriter& Writer, const FBreakerMissionBeat& Beat)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("id"), Beat.BeatId.ToString());
        Writer.WriteValue(TEXT("kind"), BreakerCensusEnumName(Beat.Kind));
        switch (Beat.Kind)
        {
        case EBreakerMissionBeatKind::Dialogue:
        case EBreakerMissionBeatKind::Return:
            Writer.WriteValue(TEXT("npc"), BreakerCensusNameOrEmpty(Beat.Npc));
            Writer.WriteValue(TEXT("node"), BreakerCensusNameOrEmpty(Beat.Node));
            Writer.WriteValue(TEXT("completesOn"), BreakerCensusNameOrEmpty(Beat.CompletesOn));
            break;
        case EBreakerMissionBeatKind::Travel:
            Writer.WriteValue(TEXT("destination"), BreakerCensusNameOrEmpty(Beat.Destination));
            Writer.WriteValue(TEXT("completesOn"), BreakerCensusNameOrEmpty(Beat.CompletesOn));
            break;
        case EBreakerMissionBeatKind::Encounter:
            if (!Beat.WorldEncounter.IsNone()) Writer.WriteValue(TEXT("worldEncounter"), Beat.WorldEncounter.ToString());
            else Writer.WriteValue(TEXT("rift"), BreakerCensusNameOrEmpty(Beat.Rift));
            Writer.WriteValue(TEXT("quest"), BreakerCensusNameOrEmpty(Beat.Quest));
            BreakerCensusNameArray(Writer, TEXT("objectives"), Beat.Objectives);
            break;
        case EBreakerMissionBeatKind::Boss:
            Writer.WriteValue(TEXT("rift"), BreakerCensusNameOrEmpty(Beat.Rift));
            Writer.WriteValue(TEXT("boss"), BreakerCensusNameOrEmpty(Beat.Boss));
            Writer.WriteValue(TEXT("completesOn"), BreakerCensusNameOrEmpty(Beat.CompletesOn));
            break;
        case EBreakerMissionBeatKind::Reward:
            Writer.WriteValue(TEXT("quest"), BreakerCensusNameOrEmpty(Beat.Quest));
            break;
        case EBreakerMissionBeatKind::Unlock:
            if (Beat.DoctrinePoints > 0)
            {
                Writer.WriteValue(TEXT("doctrinePoints"), Beat.DoctrinePoints);
                Writer.WriteValue(TEXT("benchmark"), Beat.Benchmark.ToString());
            }
            if (!Beat.CorePoint.IsNone())
            {
                Writer.WriteValue(TEXT("corePoint"), Beat.CorePoint.ToString());
            }
            if (!Beat.AbilityToken.IsNone())
            {
                Writer.WriteValue(TEXT("abilityToken"), Beat.AbilityToken.ToString());
            }
            break;
        }
        Writer.WriteObjectEnd();
    }

    void BreakerCensusMissionRow(FBreakerCensusWriter& Writer, const FBreakerMissionDefinition& Mission)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("id"), Mission.MissionId.ToString());
        Writer.WriteValue(TEXT("act"), Mission.Act);
        Writer.WriteValue(TEXT("title"), Mission.Title);
        BreakerCensusNameArray(Writer, TEXT("quests"), Mission.Quests);
        Writer.WriteArrayStart(TEXT("beats"));
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
        {
            BreakerCensusMissionBeat(Writer, Beat);
        }
        Writer.WriteArrayEnd();
        Writer.WriteObjectEnd();
    }

    // The base row's keystone is "", a keystone row's is the tag's full
    // name: the spelling the registry's loader matches variants by. The
    // numbers are written in the class's declaration order, super first,
    // through the same walk the loader validates against; a TMap's own
    // order would reshuffle the file on every export. A key the definition
    // does not carry writes the class default object's live value, which is
    // the compiled initialiser when nothing was applied.
    float BreakerCensusAbilityNumber(const UBreakerAbilityDefinition& Definition, const FNumericProperty& Property)
    {
        if (const float* Authored = Definition.Numbers.Find(Property.GetFName()))
        {
            return *Authored;
        }
        const UObject* Defaults = Definition.AbilityClass.Get() ? Definition.AbilityClass.Get()->GetDefaultObject() : nullptr;
        if (!Defaults)
        {
            return 0.0f;
        }
        const void* Value = Property.ContainerPtrToValuePtr<void>(Defaults);
        return Property.IsFloatingPoint()
            ? static_cast<float>(Property.GetFloatingPointPropertyValue(Value))
            : static_cast<float>(Property.GetSignedIntPropertyValue(Value));
    }

    void BreakerCensusAbilityRow(FBreakerCensusWriter& Writer, const UBreakerAbilityDefinition& Definition)
    {
        Writer.WriteObjectStart();
        Writer.WriteValue(TEXT("id"), Definition.AbilityId.ToString());
        Writer.WriteValue(TEXT("resourceCost"), Definition.ResourceCost);
        Writer.WriteValue(TEXT("cooldownSeconds"), Definition.CooldownSeconds);
        Writer.WriteValue(TEXT("windowDuration"), Definition.WindowDuration);
        Writer.WriteObjectStart(TEXT("numbers"));
        for (const FNumericProperty* Property : BreakerAbilityData::NumberProperties(Definition.AbilityClass.Get()))
        {
            const float Value = BreakerCensusAbilityNumber(Definition, *Property);
            Writer.WriteValue(Property->GetName(), Value);
        }
        Writer.WriteObjectEnd();
        Writer.WriteArrayStart(TEXT("variants"));
        for (const FBreakerAbilityVariant& Variant : Definition.Variants)
        {
            Writer.WriteObjectStart();
            Writer.WriteValue(TEXT("keystone"), Variant.KeystoneTag.IsValid() ? Variant.KeystoneTag.GetTagName().ToString() : FString());
            Writer.WriteValue(TEXT("windowDuration"), Variant.WindowDuration);
            Writer.WriteValue(TEXT("speedMultiplier"), Variant.SpeedMultiplier);
            Writer.WriteValue(TEXT("hitTimeoutSeconds"), Variant.HitTimeoutSeconds);
            Writer.WriteValue(TEXT("abilityCostMultiplier"), Variant.AbilityCostMultiplier);
            Writer.WriteObjectEnd();
        }
        Writer.WriteArrayEnd();
        Writer.WriteObjectEnd();
    }
}

FString BreakerCensus::Serialize(const TSharedRef<FJsonObject>& Census)
{
    FString Out;
    TSharedRef<FBreakerCensusWriter> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Census, Writer);
    BreakerCensusFinish(Out);
    return Out;
}

FString BreakerCensus::AffixesRelativePath()
{
    return UBreakerAffixLibrary::DataRelativePath();
}

FString BreakerCensus::ExportAffixes(const FBreakerAffixLibraryData& Data)
{
    FString Out;
    TSharedRef<FBreakerCensusWriter> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);

    Writer->WriteObjectStart();

    Writer->WriteArrayStart(TEXT("affixes"));
    for (const FBreakerAffixDefinition& Affix : Data.Slice) { BreakerCensusAffixRow(*Writer, Affix, TEXT("slice")); }
    for (const FBreakerAffixDefinition& Affix : Data.Aberrant) { BreakerCensusAffixRow(*Writer, Affix, TEXT("aberrant")); }
    for (const FBreakerAffixDefinition& Affix : Data.Anomalous) { BreakerCensusAffixRow(*Writer, Affix, TEXT("anomalous")); }
    for (const FBreakerAffixDefinition& Affix : Data.Downsides) { BreakerCensusAffixRow(*Writer, Affix, TEXT("downside")); }
    if (!Data.Elemental.AffixId.IsNone()) { BreakerCensusAffixRow(*Writer, Data.Elemental, TEXT("elemental")); }
    Writer->WriteArrayEnd();

    Writer->WriteObjectStart(TEXT("leans"));
    for (const FBreakerArchetypeLeans& Table : Data.Leans)
    {
        Writer->WriteObjectStart(BreakerCensusEnumName(Table.Archetype));
        for (const FBreakerAffixLean& Lean : Table.Rows)
        {
            Writer->WriteValue(Lean.AffixId.ToString(), Lean.Multiplier);
        }
        Writer->WriteObjectEnd();
    }
    Writer->WriteObjectEnd();

    Writer->WriteObjectStart(TEXT("caps"));
    for (const FBreakerStatCap& Cap : Data.Caps)
    {
        Writer->WriteValue(BreakerCensusEnumName(Cap.Target), Cap.Cap);
    }
    Writer->WriteObjectEnd();

    Writer->WriteObjectEnd();
    Writer->Close();
    BreakerCensusFinish(Out);
    return Out;
}

FString BreakerCensus::QuestsRelativePath()
{
    return UBreakerQuestLibrary::DataRelativePath();
}

FString BreakerCensus::ExportQuests(const TArray<FBreakerQuestDefinition>& Quests, const TArray<FName>& Flags)
{
    FString Out;
    TSharedRef<FBreakerCensusWriter> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);

    Writer->WriteObjectStart();
    BreakerCensusNameArray(*Writer, TEXT("flags"), Flags);
    Writer->WriteArrayStart(TEXT("quests"));
    for (const FBreakerQuestDefinition& Quest : Quests)
    {
        BreakerCensusQuestRow(*Writer, Quest);
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();
    BreakerCensusFinish(Out);
    return Out;
}

FString BreakerCensus::DialogueRelativePath()
{
    return ABreakerNPC::DialogueRelativePath();
}

FString BreakerCensus::ExportDialogue(const FBreakerDialogueData& Data)
{
    FString Out;
    TSharedRef<FBreakerCensusWriter> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);

    Writer->WriteObjectStart();
    Writer->WriteArrayStart(TEXT("npcs"));
    for (const FBreakerDialogueRow& Row : Data.Npcs)
    {
        BreakerCensusDialogueRow(*Writer, Row);
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();
    BreakerCensusFinish(Out);
    return Out;
}

FString BreakerCensus::MissionsRelativePath()
{
    return UBreakerMissionLibrary::DataRelativePath();
}

FString BreakerCensus::ExportMissions(const TArray<FBreakerMissionRift>& Rifts, const TArray<FBreakerMissionDefinition>& Missions)
{
    FString Out;
    TSharedRef<FBreakerCensusWriter> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);

    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("version"), 1);
    Writer->WriteArrayStart(TEXT("rifts"));
    for (const FBreakerMissionRift& Rift : Rifts)
    {
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("id"), Rift.RiftId.ToString());
        Writer->WriteValue(TEXT("yard"), BreakerCensusNameOrEmpty(Rift.Yard));
        Writer->WriteObjectEnd();
    }
    Writer->WriteArrayEnd();
    Writer->WriteArrayStart(TEXT("missions"));
    for (const FBreakerMissionDefinition& Mission : Missions)
    {
        BreakerCensusMissionRow(*Writer, Mission);
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();
    BreakerCensusFinish(Out);
    return Out;
}

FString BreakerCensus::AbilitiesRelativePath()
{
    return BreakerAbilityData::DataRelativePath();
}

FString BreakerCensus::ExportAbilities(const TArray<UBreakerAbilityDefinition*>& Definitions)
{
    FString Out;
    TSharedRef<FBreakerCensusWriter> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);

    Writer->WriteObjectStart();
    Writer->WriteArrayStart(TEXT("abilities"));
    for (const UBreakerAbilityDefinition* Definition : Definitions)
    {
        if (!Definition) { continue; }
        BreakerCensusAbilityRow(*Writer, *Definition);
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();
    BreakerCensusFinish(Out);
    return Out;
}
