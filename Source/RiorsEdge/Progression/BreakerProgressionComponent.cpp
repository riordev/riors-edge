#include "Progression/BreakerProgressionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#define LOCTEXT_NAMESPACE "BreakerProgression"

UBreakerProgressionComponent::UBreakerProgressionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UBreakerProgressionComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            Attributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
    if (Attributes)
    {
        // Base-value cache, same discipline as the equipment component. Note
        // that equipment caches its own bases; both layers write absolute
        // values derived from their own cache, so whichever recalculates last
        // wins on the shared attributes. Folding the two layers into one
        // application pass is a follow-up, not a slice blocker.
        BaseMaxHealth = Attributes->GetMaxHealth();
        BaseCriticalChance = Attributes->GetCriticalChance();
        BaseCriticalMultiplier = Attributes->GetCriticalMultiplier();
        BaseMoveSpeed = Attributes->GetMoveSpeed();
        BaseDamageOverTimeMultiplier = Attributes->GetDamageOverTimeMultiplier();
    }
    RecalculateStats();
}

void UBreakerProgressionComponent::DevForceClass(EBreakerClassId ClassId)
{
    State.PermanentClass = ClassId;
    if (!ClassDefinition)
    {
        ClassDefinition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
    }
    RecalculateStats();
}

bool UBreakerProgressionComponent::ChoosePermanentClassById(EBreakerClassId ClassId)
{
    if (ClassId == EBreakerClassId::None || State.PermanentClass != EBreakerClassId::None) return false;
    State.PermanentClass = ClassId;
    if (!ClassDefinition)
    {
        ClassDefinition = UBreakerProgressionLibrary::GetFallbackClassDefinition(ClassId);
    }
    RecalculateStats();
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
    RecalculateStats();
    return true;
}

bool UBreakerProgressionComponent::CanPurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason) const
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
    if (GetUnspentPoints(Node->Currency) < Node->CostPerRank)
    {
        OutFailureReason = LOCTEXT("NoPoints", "Not enough points are available.");
        return false;
    }

    OutFailureReason = FText::GetEmpty();
    return true;
}

bool UBreakerProgressionComponent::PurchaseNode(const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason)
{
    if (!CanPurchaseNode(Tree, NodeId, OutFailureReason)) return false;

    const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
    TArray<FBreakerNodeRank>& Ranks = RanksFor(Node->Currency);
    FBreakerNodeRank* Existing = Ranks.FindByPredicate([NodeId](const FBreakerNodeRank& Rank) { return Rank.NodeId == NodeId; });
    if (Existing) ++Existing->Rank;
    else Ranks.Add({NodeId, 1});

    int32& AvailablePoints = Node->Currency == EBreakerPointCurrency::ClassPoints ? State.UnspentClassPoints : State.UnspentCorePoints;
    AvailablePoints -= Node->CostPerRank;

    RecalculateStats();
    OnProgressionChanged.Broadcast();
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
    OnProgressionChanged.Broadcast();
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
    // Cleared ranks must clear their effects, tags and verb grants included.
    RecalculateStats();
    OnProgressionChanged.Broadcast();
    OutFailureReason = FText::GetEmpty();
    return true;
}

int32 UBreakerProgressionComponent::GetNodeRank(FName NodeId, EBreakerPointCurrency Currency) const
{
    const FBreakerNodeRank* Found = RanksFor(Currency).FindByPredicate([NodeId](const FBreakerNodeRank& Rank) { return Rank.NodeId == NodeId; });
    return Found ? Found->Rank : 0;
}

int32 UBreakerProgressionComponent::GetUnspentPoints(EBreakerPointCurrency Currency) const
{
    return Currency == EBreakerPointCurrency::ClassPoints ? State.UnspentClassPoints : State.UnspentCorePoints;
}

void UBreakerProgressionComponent::LoadProgressionState(const FBreakerProgressionState& NewState)
{
    // Permanent class can be loaded but never replaced by ordinary class
    // selection. Save migration/validation will be centralized later.
    State = NewState;
    if (!ClassDefinition && State.PermanentClass != EBreakerClassId::None)
    {
        ClassDefinition = UBreakerProgressionLibrary::GetFallbackClassDefinition(State.PermanentClass);
    }
    RecalculateStats();
    OnProgressionChanged.Broadcast();
}

TArray<UBreakerProgressionTree*> UBreakerProgressionComponent::GetAvailableTrees() const
{
    TArray<UBreakerProgressionTree*> Trees;
    if (ClassDefinition)
    {
        for (const TObjectPtr<UBreakerProgressionTree>& Tree : ClassDefinition->BranchTrees)
        {
            if (Tree) Trees.AddUnique(Tree.Get());
        }
    }
    for (UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetTreesForClass(State.PermanentClass))
    {
        if (Tree) Trees.AddUnique(Tree);
    }
    return Trees;
}

void UBreakerProgressionComponent::GrantPlaytestPoints(int32 ClassPoints, int32 CorePoints)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    State.UnspentClassPoints = FMath::Max(0, State.UnspentClassPoints + ClassPoints);
    State.UnspentCorePoints = FMath::Max(0, State.UnspentCorePoints + CorePoints);
    OnProgressionChanged.Broadcast();
}

void UBreakerProgressionComponent::ApplySliceDefaultsIfFresh()
{
    const bool bFresh = State.ClassNodeRanks.Num() == 0 && State.CoreNodeRanks.Num() == 0
        && State.UnspentClassPoints == 0 && State.UnspentCorePoints == 0;
    if (!bFresh) return;
    if (State.PermanentClass == EBreakerClassId::None) ChoosePermanentClassById(EBreakerClassId::Swift);
    // O2 PLACEHOLDER: 10 Class / 12 Core is the XP-And-Pacing §9 slice budget
    // at cap 10; the shipping numbers come from the curve Data Asset.
    GrantPlaytestPoints(UBreakerProgressionLibrary::SliceClassPointGrant, UBreakerProgressionLibrary::SliceCorePointGrant);
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

void UBreakerProgressionComponent::CollectKnownNodes(TArray<const UBreakerProgressionNode*>& OutNodes, EBreakerPointCurrency Currency) const
{
    auto AddTree = [&OutNodes, Currency](const UBreakerProgressionTree* Tree)
    {
        if (!Tree || Tree->Currency != Currency) return;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node) OutNodes.AddUnique(Node);
        }
    };

    if (ClassDefinition)
    {
        for (const TObjectPtr<UBreakerProgressionTree>& Tree : ClassDefinition->BranchTrees) AddTree(Tree.Get());
    }
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees()) AddTree(Tree);
}

const UBreakerProgressionNode* UBreakerProgressionComponent::FindOwnedNodeDefinition(FName NodeId, EBreakerPointCurrency Currency) const
{
    TArray<const UBreakerProgressionNode*> Nodes;
    CollectKnownNodes(Nodes, Currency);
    for (const UBreakerProgressionNode* Node : Nodes)
    {
        if (Node->NodeId == NodeId) return Node;
    }
    return nullptr;
}

bool UBreakerProgressionComponent::IsAbilityUnlocked(FName AbilityId) const
{
    if (ClassDefinition && (ClassDefinition->BaseUltimateId == AbilityId || ClassDefinition->StartingClassAbilityIds.Contains(AbilityId))) return true;
    for (const EBreakerPointCurrency Currency : {EBreakerPointCurrency::ClassPoints, EBreakerPointCurrency::CorePoints})
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        CollectKnownNodes(Nodes, Currency);
        for (const UBreakerProgressionNode* Node : Nodes)
        {
            if (GetNodeRank(Node->NodeId, Currency) > 0 && Node->GrantedAbilityIds.Contains(AbilityId)) return true;
        }
    }
    return false;
}

FBreakerNodeStats UBreakerProgressionComponent::AggregateStats(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks)
{
    constexpr int32 TargetCount = static_cast<int32>(EBreakerNodeStatTarget::Count);
    float FlatByTarget[TargetCount] = {};
    float IncreasedByTarget[TargetCount] = {};

    FBreakerNodeStats Stats;
    for (const FBreakerNodeRank& Rank : Ranks)
    {
        if (Rank.Rank <= 0) continue;
        const UBreakerProgressionNode* const* Found = Nodes.FindByPredicate(
            [&Rank](const UBreakerProgressionNode* Node) { return Node && Node->NodeId == Rank.NodeId; });
        if (!Found) continue;
        const UBreakerProgressionNode* Node = *Found;

        const int32 EffectiveRank = FMath::Min(Rank.Rank, Node->MaxRank);
        for (const FBreakerNodeEffect& Effect : Node->Effects)
        {
            const int32 Target = static_cast<int32>(Effect.StatTarget);
            if (Target < 0 || Target >= TargetCount) continue;
            const float Value = Effect.ValuePerRank * static_cast<float>(EffectiveRank);
            if (Effect.StatBucket == EBreakerNodeStatBucket::Flat) FlatByTarget[Target] += Value;
            else IncreasedByTarget[Target] += Value;
        }
        Stats.GrantedTags.AppendTags(Node->GrantedTags);
    }

    auto Flat = [&FlatByTarget](EBreakerNodeStatTarget Target) { return FlatByTarget[static_cast<int32>(Target)]; };
    auto Increased = [&IncreasedByTarget](EBreakerNodeStatTarget Target)
    {
        return 1.0f + IncreasedByTarget[static_cast<int32>(Target)] / 100.0f;
    };

    Stats.BonusHealth = Flat(EBreakerNodeStatTarget::Health);
    Stats.CriticalChanceBonus = Flat(EBreakerNodeStatTarget::CriticalChance) / 100.0f;
    Stats.CriticalMultiplierBonus = Flat(EBreakerNodeStatTarget::CriticalDamage) / 100.0f;
    Stats.DodgeChanceBonus = Flat(EBreakerNodeStatTarget::DodgeChance) / 100.0f;
    Stats.BlockChanceBonus = Flat(EBreakerNodeStatTarget::BlockChance) / 100.0f;
    Stats.MoveSpeedMultiplier = Increased(EBreakerNodeStatTarget::MoveSpeed);
    Stats.SlideSpeedMultiplier = Increased(EBreakerNodeStatTarget::SlideSpeed);
    Stats.AirControlMultiplier = Increased(EBreakerNodeStatTarget::AirControl);
    Stats.DamageOverTimeMultiplier = Increased(EBreakerNodeStatTarget::DamageOverTime);
    return Stats;
}

void UBreakerProgressionComponent::RecalculateStats()
{
    TArray<const UBreakerProgressionNode*> Nodes;
    CollectKnownNodes(Nodes, EBreakerPointCurrency::ClassPoints);
    CollectKnownNodes(Nodes, EBreakerPointCurrency::CorePoints);

    TArray<FBreakerNodeRank> Ranks = State.ClassNodeRanks;
    Ranks.Append(State.CoreNodeRanks);

    CachedStats = AggregateStats(Nodes, Ranks);
    ApplyStatsToAttributes();
}

void UBreakerProgressionComponent::ApplyStatsToAttributes()
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority() || BaseMaxHealth < 0.0f) return;

    const float HealthFraction = Attributes->GetMaxHealth() > 0.0f ? Attributes->GetHealth() / Attributes->GetMaxHealth() : 1.0f;
    Attributes->SetMaxHealth(BaseMaxHealth + CachedStats.BonusHealth);
    Attributes->SetHealth(Attributes->GetMaxHealth() * HealthFraction);
    Attributes->SetCriticalChance(BaseCriticalChance + CachedStats.CriticalChanceBonus);
    Attributes->SetCriticalMultiplier(BaseCriticalMultiplier + CachedStats.CriticalMultiplierBonus);
    Attributes->SetMoveSpeed(BaseMoveSpeed * CachedStats.MoveSpeedMultiplier);
    Attributes->SetDamageOverTimeMultiplier(BaseDamageOverTimeMultiplier * CachedStats.DamageOverTimeMultiplier);
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
