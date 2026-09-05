#include "Data/BreakerCensus.h"

#include "Dom/JsonValue.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
        TreeValues.Add(MakeShared<FJsonValueObject>(T));
    }
    Census->SetArrayField(TEXT("trees"), TreeValues);
    return Census;
}

FString BreakerCensus::Serialize(const TSharedRef<FJsonObject>& Census)
{
    FString Out;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Census, Writer);
    Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    if (!Out.EndsWith(TEXT("\n"))) { Out += TEXT("\n"); }
    return Out;
}
