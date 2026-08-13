#include "Progression/BreakerProgressionComponent.h"

#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#define LOCTEXT_NAMESPACE "BreakerProgression"

UBreakerProgressionComponent::UBreakerProgressionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UBreakerProgressionComponent::ChoosePermanentClassById(EBreakerClassId ClassId)
{
    if (ClassId == EBreakerClassId::None || State.PermanentClass != EBreakerClassId::None) return false;
    State.PermanentClass = ClassId;
    return true;
}

bool UBreakerProgressionComponent::ChoosePermanentClass(const UBreakerClassDefinition* NewClassDefinition)
{
    if (!NewClassDefinition || NewClassDefinition->ClassId == EBreakerClassId::None || State.PermanentClass != EBreakerClassId::None)
    {
        return false;
    }

    ClassDefinition = const_cast<UBreakerClassDefinition*>(NewClassDefinition);
    State.PermanentClass = NewClassDefinition->ClassId;
    if (NewClassDefinition->StartingClassAbilityIds.Num() > 0) State.AbilityLoadout.ClassAbilityOne = NewClassDefinition->StartingClassAbilityIds[0];
    if (NewClassDefinition->StartingClassAbilityIds.Num() > 1) State.AbilityLoadout.ClassAbilityTwo = NewClassDefinition->StartingClassAbilityIds[1];
    State.AbilityLoadout.Ultimate = NewClassDefinition->BaseUltimateId;
    return true;
}

bool UBreakerProgressionComponent::PurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason)
{
    if (!Tree)
    {
        OutFailureReason = LOCTEXT("MissingTree", "Progression tree is missing.");
        return false;
    }
    const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
    if (!Node || Node->Currency != Tree->Currency)
    {
        OutFailureReason = LOCTEXT("InvalidNode", "That node does not belong to this tree.");
        return false;
    }
    if (Node->RequiredClass != EBreakerClassId::None && Node->RequiredClass != State.PermanentClass)
    {
        OutFailureReason = LOCTEXT("WrongClass", "This node belongs to another class.");
        return false;
    }
    const int32 CurrentRank = GetNodeRank(NodeId, Node->Currency);
    if (CurrentRank >= Node->MaxRank)
    {
        OutFailureReason = LOCTEXT("MaxRank", "This node is already at maximum rank.");
        return false;
    }
    for (const FBreakerNodePrerequisite& Prerequisite : Node->Prerequisites)
    {
        if (GetNodeRank(Prerequisite.NodeId, Node->Currency) < Prerequisite.RequiredRank)
        {
            OutFailureReason = LOCTEXT("Prerequisite", "A prerequisite node has not been met.");
            return false;
        }
    }
    for (const FName ExcludedId : Node->MutuallyExclusiveNodeIds)
    {
        if (GetNodeRank(ExcludedId, Node->Currency) > 0)
        {
            OutFailureReason = LOCTEXT("Exclusive", "This node conflicts with an existing choice.");
            return false;
        }
    }
    const int32 Gate = FMath::Max(Node->RequiredTreeInvestment, Node->bCornerstone ? Tree->CornerstoneInvestmentGate : 0);
    if (GetTreeInvestment(Tree) < Gate)
    {
        OutFailureReason = LOCTEXT("InvestmentGate", "More points must be invested in this tree first.");
        return false;
    }

    int32& AvailablePoints = Node->Currency == EBreakerPointCurrency::ClassPoints ? State.UnspentClassPoints : State.UnspentCorePoints;
    if (AvailablePoints < Node->CostPerRank)
    {
        OutFailureReason = LOCTEXT("NoPoints", "Not enough points are available.");
        return false;
    }

    TArray<FBreakerNodeRank>& Ranks = RanksFor(Node->Currency);
    FBreakerNodeRank* Existing = Ranks.FindByPredicate([NodeId](const FBreakerNodeRank& Rank) { return Rank.NodeId == NodeId; });
    if (Existing) ++Existing->Rank;
    else Ranks.Add({NodeId, 1});
    AvailablePoints -= Node->CostPerRank;
    OutFailureReason = FText::GetEmpty();
    return true;
}

bool UBreakerProgressionComponent::EquipAbility(EBreakerAbilitySlot Slot, FName AbilityId, FText& OutFailureReason)
{
    if (AbilityId.IsNone() || !IsAbilityUnlocked(AbilityId))
    {
        OutFailureReason = LOCTEXT("AbilityLocked", "That ability has not been unlocked.");
        return false;
    }
    if (State.AbilityLoadout.Contains(AbilityId))
    {
        OutFailureReason = LOCTEXT("AbilityDuplicate", "That ability is already equipped.");
        return false;
    }

    if (Slot == EBreakerAbilitySlot::Ultimate)
    {
        if (!ClassDefinition || AbilityId != ClassDefinition->BaseUltimateId)
        {
            OutFailureReason = LOCTEXT("NotUltimate", "Only an unlocked class ultimate can use the ultimate slot.");
            return false;
        }
        State.AbilityLoadout.Ultimate = AbilityId;
    }
    else if (Slot == EBreakerAbilitySlot::ClassAbilityOne) State.AbilityLoadout.ClassAbilityOne = AbilityId;
    else State.AbilityLoadout.ClassAbilityTwo = AbilityId;

    OutFailureReason = FText::GetEmpty();
    return true;
}

bool UBreakerProgressionComponent::RespecAtForge(EBreakerPointCurrency Currency, bool bIsAtForge, FText& OutFailureReason)
{
    if (!bIsAtForge)
    {
        OutFailureReason = LOCTEXT("ForgeRequired", "Respecs are only available at a Forge.");
        return false;
    }

    TArray<FBreakerNodeRank>& Ranks = RanksFor(Currency);
    const int32 Refunded = GetRefundValue(Currency);
    Ranks.Reset();
    if (Currency == EBreakerPointCurrency::ClassPoints) State.UnspentClassPoints += Refunded;
    else State.UnspentCorePoints += Refunded;
    if (Currency == EBreakerPointCurrency::ClassPoints)
    {
        State.AbilityLoadout.ClassAbilityOne = ClassDefinition && ClassDefinition->StartingClassAbilityIds.Num() > 0
            ? ClassDefinition->StartingClassAbilityIds[0] : NAME_None;
        State.AbilityLoadout.ClassAbilityTwo = ClassDefinition && ClassDefinition->StartingClassAbilityIds.Num() > 1
            ? ClassDefinition->StartingClassAbilityIds[1] : NAME_None;
        State.AbilityLoadout.Ultimate = ClassDefinition ? ClassDefinition->BaseUltimateId : NAME_None;
    }
    OutFailureReason = FText::GetEmpty();
    return true;
}

int32 UBreakerProgressionComponent::GetNodeRank(FName NodeId, EBreakerPointCurrency Currency) const
{
    const FBreakerNodeRank* Found = RanksFor(Currency).FindByPredicate([NodeId](const FBreakerNodeRank& Rank) { return Rank.NodeId == NodeId; });
    return Found ? Found->Rank : 0;
}

void UBreakerProgressionComponent::LoadProgressionState(const FBreakerProgressionState& NewState)
{
    // Permanent class can be loaded but never replaced by ordinary class
    // selection. Save migration/validation will be centralized later.
    State = NewState;
}

int32 UBreakerProgressionComponent::GetTreeInvestment(const UBreakerProgressionTree* Tree) const
{
    int32 Total = 0;
    if (!Tree) return Total;
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
    {
        if (Node) Total += GetNodeRank(Node->NodeId, Tree->Currency) * Node->CostPerRank;
    }
    return Total;
}

int32 UBreakerProgressionComponent::GetRefundValue(EBreakerPointCurrency Currency) const
{
    int32 Total = 0;
    for (const FBreakerNodeRank& Rank : RanksFor(Currency))
    {
        const UBreakerProgressionNode* Definition = FindOwnedNodeDefinition(Rank.NodeId, Currency);
        Total += Rank.Rank * (Definition ? Definition->CostPerRank : 1);
    }
    return Total;
}

const UBreakerProgressionNode* UBreakerProgressionComponent::FindOwnedNodeDefinition(FName NodeId, EBreakerPointCurrency Currency) const
{
    if (!ClassDefinition) return nullptr;
    for (const UBreakerProgressionTree* Tree : ClassDefinition->BranchTrees)
    {
        if (Tree && Tree->Currency == Currency)
        {
            if (const UBreakerProgressionNode* Node = Tree->FindNode(NodeId)) return Node;
        }
    }
    return nullptr;
}

bool UBreakerProgressionComponent::IsAbilityUnlocked(FName AbilityId) const
{
    if (!ClassDefinition) return false;
    if (ClassDefinition->BaseUltimateId == AbilityId || ClassDefinition->StartingClassAbilityIds.Contains(AbilityId)) return true;
    for (const UBreakerProgressionTree* Tree : ClassDefinition->BranchTrees)
    {
        if (!Tree) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node && GetNodeRank(Node->NodeId, Tree->Currency) > 0 && Node->GrantedAbilityIds.Contains(AbilityId)) return true;
        }
    }
    return false;
}

TArray<FBreakerNodeRank>& UBreakerProgressionComponent::RanksFor(EBreakerPointCurrency Currency)
{
    return Currency == EBreakerPointCurrency::ClassPoints ? State.ClassNodeRanks : State.CoreNodeRanks;
}

const TArray<FBreakerNodeRank>& UBreakerProgressionComponent::RanksFor(EBreakerPointCurrency Currency) const
{
    return Currency == EBreakerPointCurrency::ClassPoints ? State.ClassNodeRanks : State.CoreNodeRanks;
}

#undef LOCTEXT_NAMESPACE
