#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

FBreakerNodeEffect BreakerCoreRoster::Effect(EBreakerNodeStatTarget Target, EBreakerNodeStatBucket Bucket, float Value)
{
    FBreakerNodeEffect Result;
    Result.StatTarget = Target; Result.StatBucket = Bucket; Result.ValuePerRank = Value;
    return Result;
}

UBreakerProgressionNode* BreakerCoreRoster::Node(UObject* Outer, const TCHAR* Id, const TCHAR* Name,
    const TCHAR* Description, std::initializer_list<FBreakerNodeEffect> Effects, std::initializer_list<const TCHAR*> Tags)
{
    auto* Result = NewObject<UBreakerProgressionNode>(Outer);
    Result->NodeId = FName(Id); Result->DisplayName = FText::FromString(Name); Result->Description = FText::FromString(Description);
    for (const auto& E : Effects) Result->Effects.Add(E);
    for (const TCHAR* Tag : Tags) Result->GrantedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(Tag)));
    return Result;
}

UBreakerProgressionTree* BreakerCoreRoster::BuildCandidate(UObject* Outer, FString& Error)
{
    TArray<FBreakerCoreWedgeDefinition> Wedges;
    AppendWeapon(Outer, Wedges);
    AppendDefence(Outer, Wedges);
    AppendAbility(Outer, Wedges);
    AppendStatus(Outer, Wedges);
    AppendMovement(Outer, Wedges);
    AppendUtility(Outer, Wedges);
    return BreakerCoreTree::Build(Outer, TEXT("Core.Replacement"), FText::FromString(TEXT("Core")), Wedges, Error);
}
