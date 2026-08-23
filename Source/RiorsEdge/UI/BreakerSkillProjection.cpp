#include "UI/BreakerSkillProjection.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

bool FBreakerStatLine::Changed() const
{
    return !FMath::IsNearlyEqual(Before, After, 0.0001f);
}

namespace
{
    // The row set, in one place, so Before and After can never disagree about
    // what row 3 is. Order is the reading order on the rail.
    enum class EStatRow : uint8
    {
        // TWO DELIVERY LANES, NOT ONE. O54 partitioned damage into weapon-
        // delivered and ability-delivered pools and the aggregator carries both
        // — DamageMultiplier and AbilityDamageMultiplier — but this table bound
        // a single DAMAGE row to the first of them and never read the second.
        // So the skill screen priced every ability build at its weapon lane:
        // a node that moved abilities alone printed no change, and a node that
        // moved weapons alone printed a gain an ability build would never see.
        // The parity measurement makes the size of that lie concrete — the two
        // lanes sit at 0.647x of each other at the cap and 0.388x at endgame,
        // so one row could not have been within a third of right for anybody.
        WeaponDamage,
        AbilityDamage,
        CriticalChance,
        CriticalDamage,
        MaxHealth,
        DamageOverTime,
        MoveSpeed,
        SlideSpeed,
        AirControl,
        DodgeChance,
        BlockChance,
        Count
    };

    struct FStatRowDescriptor
    {
        const TCHAR* Label;
        EBreakerStatFormat Format;
        // Rows below this index are attribute-backed when the character has a
        // captured attribute set; the rest only ever carry the tree layer,
        // because nothing else writes them.
        bool bAlwaysTreeOnly;
    };

    const FStatRowDescriptor StatRows[] =
    {
        // Named for the LANE, not for the source. "DAMAGE" was accurate when
        // there was one bucket and became a claim about both the moment there
        // were two.
        { TEXT("WEAPON DAMAGE"), EBreakerStatFormat::Multiplier,    false },
        { TEXT("ABILITY DAMAGE"),EBreakerStatFormat::Multiplier,    false },
        { TEXT("CRIT CHANCE"),   EBreakerStatFormat::PercentPoints, false },
        { TEXT("CRIT DAMAGE"),   EBreakerStatFormat::Multiplier,    false },
        { TEXT("MAX HEALTH"),    EBreakerStatFormat::Absolute,      false },
        { TEXT("DOT DAMAGE"),    EBreakerStatFormat::Multiplier,    false },
        { TEXT("MOVE SPEED"),    EBreakerStatFormat::Multiplier,    true  },
        { TEXT("SLIDE SPEED"),   EBreakerStatFormat::Multiplier,    true  },
        { TEXT("AIR CONTROL"),   EBreakerStatFormat::Multiplier,    true  },
        { TEXT("DODGE CHANCE"),  EBreakerStatFormat::PercentPoints, true  },
        { TEXT("BLOCK CHANCE"),  EBreakerStatFormat::PercentPoints, true  },
    };

    static_assert(UE_ARRAY_COUNT(StatRows) == static_cast<int32>(EStatRow::Count),
        "Stat row table and row enum must stay in step.");

    float ComposeWithOffer(const FBreakerSkillSnapshot& Snapshot, const FBreakerAttributeContribution& Offer,
        EBreakerAggregatedAttribute Attribute)
    {
        // A copy, so the character's live aggregator is never touched by a
        // hypothetical. Everything except the progression offer — the true
        // bases, the gear contribution, the fold itself — is the real one.
        FBreakerAttributeAggregator Hypothetical = Snapshot.Aggregator;
        Hypothetical.SetContribution(EBreakerAttributeContributor::Progression, Offer);
        return Hypothetical.Compose(Attribute);
    }

    void CollectRowValues(const FBreakerSkillSnapshot& Snapshot, const FBreakerAttributeContribution& Offer,
        const FBreakerNodeStats& Stats, float (&OutValues)[static_cast<int32>(EStatRow::Count)])
    {
        const bool bComposed = Snapshot.bHasComposedAttributes;

        OutValues[static_cast<int32>(EStatRow::WeaponDamage)] = bComposed
            ? ComposeWithOffer(Snapshot, Offer, EBreakerAggregatedAttribute::DamageMultiplier)
            : Stats.DamageMultiplier;
        // The shared pool needs no row: O54 puts it inside BOTH lanes by
        // construction, so a third row would print the same percentage twice
        // and invite the player to add it to itself.
        OutValues[static_cast<int32>(EStatRow::AbilityDamage)] = bComposed
            ? ComposeWithOffer(Snapshot, Offer, EBreakerAggregatedAttribute::AbilityDamageMultiplier)
            : Stats.AbilityDamageMultiplier;
        OutValues[static_cast<int32>(EStatRow::CriticalChance)] = bComposed
            ? ComposeWithOffer(Snapshot, Offer, EBreakerAggregatedAttribute::CriticalChance)
            : Stats.CriticalChanceBonus;
        OutValues[static_cast<int32>(EStatRow::CriticalDamage)] = bComposed
            ? ComposeWithOffer(Snapshot, Offer, EBreakerAggregatedAttribute::CriticalMultiplier)
            // No captured base to add the bonus to, so the tree half alone is
            // shown against an implied 1.0 and flagged tree-only.
            : 1.0f + Stats.CriticalMultiplierBonus;
        OutValues[static_cast<int32>(EStatRow::MaxHealth)] = bComposed
            ? ComposeWithOffer(Snapshot, Offer, EBreakerAggregatedAttribute::MaxHealth)
            : Stats.BonusHealth;
        OutValues[static_cast<int32>(EStatRow::DamageOverTime)] = bComposed
            ? ComposeWithOffer(Snapshot, Offer, EBreakerAggregatedAttribute::DamageOverTimeMultiplier)
            : Stats.DamageOverTimeMultiplier;

        // These four have no aggregated attribute at all: the movement and
        // combat components read FBreakerNodeStats directly, so the node stats
        // ARE the real numbers here, composed or not.
        OutValues[static_cast<int32>(EStatRow::MoveSpeed)] = Stats.MoveSpeedMultiplier;
        OutValues[static_cast<int32>(EStatRow::SlideSpeed)] = Stats.SlideSpeedMultiplier;
        OutValues[static_cast<int32>(EStatRow::AirControl)] = Stats.AirControlMultiplier;
        OutValues[static_cast<int32>(EStatRow::DodgeChance)] = Stats.DodgeChanceBonus;
        OutValues[static_cast<int32>(EStatRow::BlockChance)] = Stats.BlockChanceBonus;
    }
}

namespace
{
    // One evaluation of a hypothetical loadout: the gear layer's own maths,
    // folded through a copy of the character's live aggregator.
    struct FBreakerEquipEvaluation
    {
        float WeaponDamage = 1.0f;
        float AbilityDamage = 1.0f;
        float CriticalChance = 0.0f;
        float CriticalDamage = 1.0f;
        float EffectiveHealthVsPhysical = 0.0f;
    };

    FBreakerEquipEvaluation BreakerEvaluateLoadout(const FBreakerSkillSnapshot& Snapshot,
        const TArray<FBreakerItemInstance>& Items)
    {
        FBreakerAttributeContribution GearOffer;
        const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats(Items, &GearOffer);

        FBreakerAttributeAggregator Hypothetical = Snapshot.Aggregator;
        Hypothetical.SetContribution(EBreakerAttributeContributor::Equipment, GearOffer);

        FBreakerEquipEvaluation Out;
        Out.WeaponDamage = Hypothetical.Compose(EBreakerAggregatedAttribute::DamageMultiplier);
        Out.AbilityDamage = Hypothetical.Compose(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Out.CriticalChance = FMath::Clamp(Hypothetical.Compose(EBreakerAggregatedAttribute::CriticalChance), 0.0f, 1.0f);
        Out.CriticalDamage = Hypothetical.Compose(EBreakerAggregatedAttribute::CriticalMultiplier);

        // Effective health against physical: the health pool divided by what
        // survives mitigation. The reduction comes back from AggregateStats
        // ALREADY CAPPED, so this cannot report a defence the game would refuse
        // to grant — including the raised cap a rule rewrite can buy.
        const float MaxHealth = Hypothetical.Compose(EBreakerAggregatedAttribute::MaxHealth);
        const float Surviving = FMath::Max(1.0f - Stats.PhysicalDamageReductionPercent / 100.0f, 0.01f);
        Out.EffectiveHealthVsPhysical = MaxHealth / Surviving;
        return Out;
    }

    FBreakerStatLine BreakerEquipRow(const TCHAR* Label, EBreakerStatFormat Format, float Before, float After)
    {
        FBreakerStatLine Line;
        Line.Label = Label;
        Line.Format = Format;
        Line.Before = Before;
        Line.After = After;
        return Line;
    }
}

TArray<FBreakerStatLine> BreakerSkillProjection::ProjectEquip(const FBreakerSkillSnapshot& Snapshot,
    const TArray<FBreakerItemInstance>& Equipped, const FBreakerItemInstance& Candidate)
{
    TArray<FBreakerItemInstance> Hypothetical = Equipped;
    // Equipping REPLACES the piece in that slot, which is what makes this a
    // delta rather than a sum: an item's worth is what it displaces, and a
    // score that ignored the displaced piece would rate a downgrade positively.
    const int32 Existing = Hypothetical.IndexOfByPredicate(
        [&Candidate](const FBreakerItemInstance& Item) { return Item.Slot == Candidate.Slot; });
    if (Hypothetical.IsValidIndex(Existing)) Hypothetical[Existing] = Candidate;
    else Hypothetical.Add(Candidate);

    const FBreakerEquipEvaluation Now = BreakerEvaluateLoadout(Snapshot, Equipped);
    const FBreakerEquipEvaluation Then = BreakerEvaluateLoadout(Snapshot, Hypothetical);

    TArray<FBreakerStatLine> Lines;
    Lines.Add(BreakerEquipRow(TEXT("WEAPON DAMAGE"), EBreakerStatFormat::Multiplier,
        Now.WeaponDamage, Then.WeaponDamage));
    Lines.Add(BreakerEquipRow(TEXT("ABILITY DAMAGE"), EBreakerStatFormat::Multiplier,
        Now.AbilityDamage, Then.AbilityDamage));
    Lines.Add(BreakerEquipRow(TEXT("CRIT CHANCE"), EBreakerStatFormat::PercentPoints,
        Now.CriticalChance, Then.CriticalChance));
    Lines.Add(BreakerEquipRow(TEXT("CRIT DAMAGE"), EBreakerStatFormat::Multiplier,
        Now.CriticalDamage, Then.CriticalDamage));
    // NAMED for what it excludes. Block and dodge are passive chance layers and
    // are deliberately not in this number.
    Lines.Add(BreakerEquipRow(TEXT("EFF. HEALTH vs PHYSICAL"), EBreakerStatFormat::Absolute,
        Now.EffectiveHealthVsPhysical, Then.EffectiveHealthVsPhysical));
    return Lines;
}

int32 BreakerSkillProjection::StatRowCount()
{
    return static_cast<int32>(EStatRow::Count);
}

FBreakerSkillSnapshot BreakerSkillProjection::MakeSnapshot(const UBreakerProgressionComponent* Progression, const UBreakerAttributeSet* Attributes)
{
    FBreakerSkillSnapshot Snapshot;
    if (!Progression) return Snapshot;

    // Same two sources UBreakerProgressionComponent::CollectKnownNodes walks,
    // in the same order: the class definition's branch trees, then every
    // fallback tree. A node reachable by the component must be reachable here
    // or a projection would silently ignore an owned rank.
    auto AddTree = [&Snapshot](const UBreakerProgressionTree* Tree)
    {
        if (!Tree) return;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node) Snapshot.Nodes.AddUnique(Node);
        }
    };
    if (const UBreakerClassDefinition* ClassDef = Progression->ClassDefinition)
    {
        for (const TObjectPtr<UBreakerProgressionTree>& Tree : ClassDef->BranchTrees) AddTree(Tree.Get());
    }
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees()) AddTree(Tree);

    const FBreakerProgressionState& State = Progression->GetProgressionState();
    // EVERY LIVE POOL. This read the retired class array and Core, and never
    // Doctrine -- so once the retired array was permanently empty, every
    // projection on the skill screen (CurrentTotals, Project, ProjectPurchase)
    // was computed from Core ranks alone. A character with doctrine points saw
    // totals that ignored them, and the before/after delta for a doctrine
    // purchase was wrong by however much doctrine they already owned. On the
    // screen the permanent choice is made on.
    //
    // RiorsEdge.Progression.PointPools.EveryPoolIsRouted enumerates the
    // currency and asserts each live one reaches both this snapshot and the
    // aggregator, because this was the SECOND seam with the same omission and
    // the first was only found through eighteen unrelated failures.
    Snapshot.Ranks = State.CoreNodeRanks;
    Snapshot.Ranks.Append(State.DoctrineNodeRanks);
    Snapshot.IncreasedDamagePerSpentPoint = Progression->IncreasedDamagePerSpentPoint;

    if (Attributes && Attributes->HasCapturedAttributeBases())
    {
        Snapshot.Aggregator = Attributes->GetAttributeAggregator();
        Snapshot.bHasComposedAttributes = true;
    }
    return Snapshot;
}

TArray<FBreakerNodeRank> BreakerSkillProjection::WithRankDelta(const TArray<FBreakerNodeRank>& Ranks, FName NodeId, int32 Delta)
{
    TArray<FBreakerNodeRank> Result = Ranks;
    if (NodeId.IsNone() || Delta == 0) return Result;

    for (FBreakerNodeRank& Rank : Result)
    {
        if (Rank.NodeId != NodeId) continue;
        Rank.Rank = FMath::Max(0, Rank.Rank + Delta);
        return Result;
    }

    if (Delta > 0)
    {
        FBreakerNodeRank Added;
        Added.NodeId = NodeId;
        Added.Rank = Delta;
        Result.Add(Added);
    }
    return Result;
}

int32 BreakerSkillProjection::CommittedPoints(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks)
{
    int32 Total = 0;
    for (const FBreakerNodeRank& Rank : Ranks)
    {
        if (Rank.Rank <= 0) continue;
        const UBreakerProgressionNode* const* Found = Nodes.FindByPredicate(
            [&Rank](const UBreakerProgressionNode* Node) { return Node && Node->NodeId == Rank.NodeId; });
        Total += Rank.Rank * (Found ? (*Found)->CostPerRank : 1);
    }
    return Total;
}

FBreakerNodeStats BreakerSkillProjection::BuildOffer(const FBreakerSkillSnapshot& Snapshot,
    const TArray<FBreakerNodeRank>& Ranks, FBreakerAttributeContribution& OutOffer)
{
    // The production fold, not a second implementation of it.
    FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Snapshot.Nodes, Ranks, &OutOffer);

    // The one line RecalculateStats adds afterwards, mirrored here. If that
    // baseline ever moves, RiorsEdge.UI.SkillProjection.MirrorsLiveComponent
    // fails, which is the whole reason the test exists.
    const float SpendPercent = CommittedPoints(Snapshot.Nodes, Ranks)
        * FMath::Max(0.0f, Snapshot.IncreasedDamagePerSpentPoint);
    if (SpendPercent > 0.0f)
    {
        // Shared, exactly as RecalculateStats bids it (O54): the floor is a
        // property of having spent points, not of how damage is delivered.
        OutOffer.AddSharedIncreasedDamage(SpendPercent);
        Stats.DamageMultiplier += SpendPercent / 100.0f;
        Stats.AbilityDamageMultiplier += SpendPercent / 100.0f;
    }
    return Stats;
}

TArray<FBreakerStatLine> BreakerSkillProjection::Project(const FBreakerSkillSnapshot& Snapshot, const TArray<FBreakerNodeRank>& AfterRanks)
{
    constexpr int32 RowCount = static_cast<int32>(EStatRow::Count);

    FBreakerAttributeContribution BeforeOffer;
    const FBreakerNodeStats BeforeStats = BuildOffer(Snapshot, Snapshot.Ranks, BeforeOffer);
    float BeforeValues[RowCount] = {};
    CollectRowValues(Snapshot, BeforeOffer, BeforeStats, BeforeValues);

    FBreakerAttributeContribution AfterOffer;
    const FBreakerNodeStats AfterStats = BuildOffer(Snapshot, AfterRanks, AfterOffer);
    float AfterValues[RowCount] = {};
    CollectRowValues(Snapshot, AfterOffer, AfterStats, AfterValues);

    TArray<FBreakerStatLine> Lines;
    Lines.Reserve(RowCount);
    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        FBreakerStatLine Line;
        Line.Label = StatRows[Row].Label;
        Line.Format = StatRows[Row].Format;
        Line.bTreeOnly = StatRows[Row].bAlwaysTreeOnly || !Snapshot.bHasComposedAttributes;
        Line.Before = BeforeValues[Row];
        Line.After = AfterValues[Row];
        Lines.Add(MoveTemp(Line));
    }
    return Lines;
}

TArray<FBreakerStatLine> BreakerSkillProjection::ProjectPurchase(const FBreakerSkillSnapshot& Snapshot, FName NodeId, int32 RankDelta)
{
    return Project(Snapshot, WithRankDelta(Snapshot.Ranks, NodeId, RankDelta));
}

TArray<FBreakerStatLine> BreakerSkillProjection::CurrentTotals(const FBreakerSkillSnapshot& Snapshot)
{
    return Project(Snapshot, Snapshot.Ranks);
}

float BreakerSkillProjection::LayerIncreasedPercent(const FBreakerSkillSnapshot& Snapshot,
    EBreakerAttributeContributor Contributor, EBreakerAggregatedAttribute Attribute)
{
    if (Contributor == EBreakerAttributeContributor::Count) return 0.0f;
    return Snapshot.Aggregator.GetContribution(Contributor).GetIncreasedPercent(Attribute);
}

FString BreakerSkillProjection::FormatStat(float Value, EBreakerStatFormat Format)
{
    switch (Format)
    {
        case EBreakerStatFormat::PercentPoints: return FString::Printf(TEXT("%.1f%%"), Value * 100.0f);
        case EBreakerStatFormat::Absolute:      return FString::Printf(TEXT("%.0f"), Value);
        default:                                return FString::Printf(TEXT("%.2fx"), Value);
    }
}

FString BreakerSkillProjection::FormatDelta(const FBreakerStatLine& Line)
{
    if (!Line.Changed()) return FString();
    const float Delta = Line.After - Line.Before;
    switch (Line.Format)
    {
        case EBreakerStatFormat::PercentPoints: return FString::Printf(TEXT("%+.1f%%"), Delta * 100.0f);
        case EBreakerStatFormat::Absolute:      return FString::Printf(TEXT("%+.0f"), Delta);
        default:
            // A multiplier's delta reads as percentage points of the 1.0 base,
            // which is how the affix and node tables author it in the first
            // place: 1.13x -> 1.16x is "+3%".
            return FMath::Abs(Delta) < 0.0095f
                ? FString::Printf(TEXT("%+.2f%%"), Delta * 100.0f)
                : FString::Printf(TEXT("%+.0f%%"), Delta * 100.0f);
    }
}

FString BreakerSkillProjection::FormatTransition(const FBreakerStatLine& Line)
{
    return FormatStat(Line.Before, Line.Format) + TEXT(" -> ") + FormatStat(Line.After, Line.Format);
}
