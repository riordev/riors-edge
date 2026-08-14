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
    // Ticks only to notice movement-state transitions for conditional node
    // effects. The tick itself is a byte comparison; the recalculation behind
    // it runs on a transition, not on a frame.
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerProgressionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    RefreshBuildConditions();
}

void UBreakerProgressionComponent::RefreshBuildConditions()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const FBreakerBuildConditionState Evaluated = FBreakerBuildConditionState::EvaluateForActor(GetOwner());
    if (Evaluated == ActiveConditions) return;
    ActiveConditions = Evaluated;
    RecalculateStats();
}

void UBreakerProgressionComponent::BeginPlay()
{
    Super::BeginPlay();
    UBreakerAttributeSet* FoundAttributes = nullptr;
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            FoundAttributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
    // Seed the slice budget before any save load runs. Nothing else called
    // this, which is why a gym pawn reached the tree screen with zero points;
    // LoadProgressionState re-runs it after restoring a save.
    ApplySliceDefaultsIfFresh();
    BindAttributes(FoundAttributes);
}

void UBreakerProgressionComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    // No base cache here. The attribute set owns the one true base and
    // capturing is idempotent, so it does not matter whether progression or
    // equipment binds first — neither can snapshot a base that already
    // contains the other's contribution. That was the bug.
    if (Attributes) Attributes->CaptureAttributeBases();
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
    // KNOWN GAP, deliberately not fixed here: a dev swap keeps whatever
    // ClassDefinition is already held, so it can describe the class we just
    // left. Re-fetching unconditionally would stomp an authored Data Asset
    // with the C++ fallback, which is the worse failure. Needs a rule about
    // which source wins; not a dev-tool decision.
    OnProgressionChanged.Broadcast();
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
    // Locking a class is a progression change like any other. Without this the
    // class-resource loops never learn they went live: UBreakerMomentumComponent
    // caches bIsSwift in HandleProgressionChanged, which runs once at BeginPlay
    // and then only on this event, so a character who picks Swift mid-session
    // had dead Momentum until some unrelated purchase happened to broadcast.
    OnProgressionChanged.Broadcast();
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
    // Same reason as ChoosePermanentClassById: this is the Data-Asset-driven
    // twin of that path and the listeners cannot tell them apart.
    OnProgressionChanged.Broadcast();
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
    // A save written before slice seeding existed restores an empty economy.
    // Re-seed here so loading never leaves the tree screen unspendable; the
    // freshness test means a save with any real progress is left untouched.
    ApplySliceDefaultsIfFresh();
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
    // "Fresh" is a statement about the point economy, not about the save file.
    // A character with nothing allocated in either currency and an empty pool
    // has never received the slice budget — whether it is a brand-new gym pawn
    // or an existing save written before this seeding existed. A permanent
    // class already being chosen no longer disqualifies it; only real spending
    // or a non-empty pool does. That is what left owner saves stuck on
    // "CLASS 0 | CORE 0 UNSPENT" with a locked class and nothing to spend.
    const bool bFresh = State.ClassNodeRanks.Num() == 0 && State.CoreNodeRanks.Num() == 0
        && State.UnspentClassPoints == 0 && State.UnspentCorePoints == 0;
    if (!bFresh) return;
    // Only pick a class for a character that has none; a chosen class is kept.
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

FBreakerNodeStats UBreakerProgressionComponent::AggregateStats(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks,
    FBreakerAttributeContribution* OutContribution, const FBreakerBuildConditionState& Conditions)
{
    constexpr int32 TargetCount = static_cast<int32>(EBreakerNodeStatTarget::Count);
    float FlatByTarget[TargetCount] = {};
    float IncreasedByTarget[TargetCount] = {};
    // One entry per owned More source, kept separate until the O3 cap has been
    // applied. Sorting and truncating a list is the only honest way to enforce
    // "a build may hold at most three": composing first and clamping the
    // product afterwards would silently reprice the nodes the player chose.
    TArray<float> DamageMoreMultipliers;
    float ActiveConditionalPercent = 0.0f;
    float PotentialConditionalPercent = 0.0f;

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

            const bool bConditional = Effect.Condition != EBreakerBuildCondition::Always;
            const bool bLive = Conditions.IsActive(Effect.Condition);
            if (bConditional && Effect.StatTarget == EBreakerNodeStatTarget::Damage
                && Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent)
            {
                PotentialConditionalPercent += Value;
                if (bLive) ActiveConditionalPercent += Value;
            }
            // A conditional effect whose state is false contributes nothing at
            // all, in any bucket.
            if (!bLive) continue;

            if (Effect.StatBucket == EBreakerNodeStatBucket::Flat) FlatByTarget[Target] += Value;
            else if (Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent) IncreasedByTarget[Target] += Value;
            else if (Effect.StatTarget == EBreakerNodeStatTarget::Damage)
            {
                // Rank does NOT scale a More multiplier — a rank-2 x1.25 would
                // be x1.5625, which no node table means. Every More node in the
                // content is single-rank; this is the guard, not a limitation.
                DamageMoreMultipliers.Add(1.0f + FMath::Max(0.0f, Effect.ValuePerRank) / 100.0f);
            }
        }
        Stats.GrantedTags.AppendTags(Node->GrantedTags);
    }

    // O3's hard cap of three, and Damage-Pipeline §4's per-multiplier ceiling of
    // 1.30x. The strongest three win, so a fourth purchase is dead weight the
    // player can be told about rather than a quiet nerf to the other three.
    Stats.DamageMoreSourceCount = DamageMoreMultipliers.Num();
    DamageMoreMultipliers.Sort([](float A, float B) { return A > B; });
    float DamageMoreProduct = 1.0f;
    for (int32 Index = 0; Index < FMath::Min(DamageMoreMultipliers.Num(), MaxDamageMoreSources); ++Index)
    {
        DamageMoreProduct *= FMath::Min(DamageMoreMultipliers[Index], SingleMoreCeiling);
    }
    Stats.DamageMoreMultiplier = DamageMoreProduct;
    Stats.ActiveConditionalDamagePercent = ActiveConditionalPercent;
    Stats.PotentialConditionalDamagePercent = PotentialConditionalPercent;

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
    Stats.DamageMultiplier = Increased(EBreakerNodeStatTarget::Damage);

    if (OutContribution)
    {
        // Raw buckets, not the composed multipliers: tree percentages have to
        // join gear percentages in ONE additive bucket per stat. The More
        // product is the exception and always was — it is a different bucket,
        // composed multiplicatively through ComposeMore under the O3 cap
        // enforced above.
        OutContribution->Reset();
        OutContribution->AddFlat(EBreakerAggregatedAttribute::MaxHealth, Stats.BonusHealth);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::CriticalChance, Stats.CriticalChanceBonus);
        OutContribution->AddFlat(EBreakerAggregatedAttribute::CriticalMultiplier, Stats.CriticalMultiplierBonus);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::MoveSpeed)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DamageOverTimeMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::DamageOverTime)]);
        OutContribution->AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier, IncreasedByTarget[static_cast<int32>(EBreakerNodeStatTarget::Damage)]);
        if (!FMath::IsNearlyEqual(DamageMoreProduct, 1.0f))
        {
            OutContribution->ComposeMore(EBreakerAggregatedAttribute::DamageMultiplier, DamageMoreProduct);
        }
    }
    return Stats;
}

float UBreakerProgressionComponent::GetSpentPoints() const
{
    // Points actually committed to nodes, in both wallets. GetRefundValue is
    // rank x CostPerRank, which is exactly what a respec hands back — so a
    // 3-point Convergence node is worth three times a 1-point minor here, and
    // the total can never disagree with what the player was charged.
    return static_cast<float>(GetRefundValue(EBreakerPointCurrency::ClassPoints) + GetRefundValue(EBreakerPointCurrency::CorePoints));
}

float UBreakerProgressionComponent::GetPointSpendDamagePercent() const
{
    return GetSpentPoints() * FMath::Max(0.0f, IncreasedDamagePerSpentPoint);
}

void UBreakerProgressionComponent::RecalculateStats()
{
    TArray<const UBreakerProgressionNode*> Nodes;
    CollectKnownNodes(Nodes, EBreakerPointCurrency::ClassPoints);
    CollectKnownNodes(Nodes, EBreakerPointCurrency::CorePoints);

    TArray<FBreakerNodeRank> Ranks = State.ClassNodeRanks;
    Ranks.Append(State.CoreNodeRanks);

    CachedStats = AggregateStats(Nodes, Ranks, &CachedContribution, ActiveConditions);

    // Every committed point pays a small Increased Damage baseline, into the
    // SAME additive bucket as gear and node damage. Under O27 this is a FLOOR,
    // not a power source: at 0.25%/point both a baseline and an optimized build
    // spend their whole budget, so it lands on both equally and differentiates
    // neither. What separates them is which nodes they bought. It stays a
    // property of SPENDING, not of level, which keeps it clear of the cap-50
    // no-post-cap-power ruling.
    const float SpendPercent = GetPointSpendDamagePercent();
    if (SpendPercent > 0.0f)
    {
        CachedContribution.AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier, SpendPercent);
        CachedStats.DamageMultiplier += SpendPercent / 100.0f;
    }

    ApplyStatsToAttributes();
}

void UBreakerProgressionComponent::ApplyStatsToAttributes()
{
    PublishNodeTagsToAbilitySystem();
    // One submission, no absolute writes. A respec submits an empty
    // contribution, which restores exactly the pre-purchase composition.
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return;
    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, CachedContribution);
}

void UBreakerProgressionComponent::PublishNodeTagsToAbilitySystem()
{
    // A node's GrantedTags used to live only in FBreakerNodeStats, which meant
    // the one system most of them exist to talk to could not read them:
    // UBreakerAbility_Overdrive resolves its keystone variant from
    // ASC->GetOwnedGameplayTags, and Class-Kits' whole branch-keystone pattern
    // is "the keystone rewrites the ultimate". Publishing the aggregate as
    // loose tags is what makes Bloodrhythm a real rewrite instead of a label.
    //
    // Diffed against the last publication rather than cleared and re-added, so
    // a recalculation that changes nothing touches nothing, and a respec
    // removes exactly the tags this component put there and no others.
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Owner);
    UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;

    const FGameplayTagContainer& Desired = CachedStats.GrantedTags;
    for (const FGameplayTag& Tag : PublishedNodeTags)
    {
        if (!Desired.HasTagExact(Tag)) ASC->RemoveLooseGameplayTag(Tag);
    }
    for (const FGameplayTag& Tag : Desired)
    {
        if (!PublishedNodeTags.HasTagExact(Tag)) ASC->AddLooseGameplayTag(Tag);
    }
    PublishedNodeTags = Desired;
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
