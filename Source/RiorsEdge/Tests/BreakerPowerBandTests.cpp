#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/BreakerBaselineLoadout.h"
#include "Tests/BreakerStatusEmit.h"
#include "Tests/BreakerPowerBandFixture.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRules.h"
#include "Items/BreakerLootLibrary.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// THE BUILD VARIANCE BAND (Power-Curve.md Â§4, authority O27; split into two
// bands by O36)
// ---------------------------------------------------------------------------
// "The ratio between a baseline build and an optimized one at the SAME area
// level. This is the number O27 is really about, and it needs to be authored
// explicitly rather than emerging by accident." Originally targeted at a
// single "roughly 8-10x"; O29's item-level-120 gear depth moved where the top
// of the band lives, and O36 rules that the band is now authored at TWO
// points instead of retuning content to force one number: AT-CAP (level 50,
// tiers a level-50 drop can produce, 8-10x) and ENDGAME (ilvl 120, producible
// tiers, seed rails 12-20x). FBreakerPowerBandAtCapTest and
// FBreakerPowerBandEndgameTest below are that split, pinned separately.
//
// Both tests build two characters out of the REAL affix pool and the REAL
// fallback trees, folds them through the REAL aggregator
// (FBreakerAttributeAggregator, the same object UBreakerAttributeSet owns),
// and asserts the composed ratio lands in the relevant band. Nothing here
// re-implements the arithmetic; if the fold changes, this moves with it,
// which is the entire point.
//
// Both builds are measured in the SAME movement state â€” airborne, recently
// dashed, at Redline. That is the fair comparison the doc asks for: same
// content, same instant, different build. The baseline is not a character
// standing still; it is a character who found one conditional line and did not
// organise anything around it.
//
// Both builds also spend their whole point budget, which is why the per-point
// accumulation baseline cancels out of the ratio entirely. Under O27 that is
// the desired property, not an accident: accumulation must not be what
// separates two characters. The point budget does not change between the two
// O36 bands either, because character level (and so points earned) is capped
// at 50 regardless of item level â€” only gear grows past the cap (O29).
// ---------------------------------------------------------------------------

namespace BreakerPowerBandTest
{
    // Actual allocation: 65 Core plus the same legal eight-point Kinetic
    // doctrine on every build. Gear depth never changes this budget.
    constexpr int32 PowerBandFullPointBudget = 73;

    // ---------------------------------------------------------------------
    // O36 â€” TWO BANDS, pinned separately.
    // ---------------------------------------------------------------------
    // "The build variance band is authored at two points: AT-CAP (level 50,
    // tiers a level-50 drop can produce): 8-10x stands. ENDGAME (ilvl 120,
    // producible tiers): seed rails 12-20x (O2 PLACEHOLDER; the back-loaded
    // ladder currently measures ~15x, accepted pending playtest)."
    constexpr float AtCapBandMinimum = 8.0f;
    constexpr float AtCapBandMaximum = 10.0f;
    constexpr float EndgameBandMinimum = 12.0f;   // O2 PLACEHOLDER seed (O36)
    constexpr float EndgameBandMaximum = 20.0f;   // O2 PLACEHOLDER seed (O36)

    // O99: THE PARITY BAND. An ability-geared build and a weapon-geared build
    // land within roughly 15% of each other at the same gear depth. This is the
    // first time "neither lane trivializes the other" is a number rather than a
    // sentiment, and it is a TARGET: the measurement is 0.647x and pinning
    // there would enshrine abilities as a second-class lane, which is the same
    // mistake as pinning the defence inversion at its current 3.76.
    //
    // Ruled at the CAP. Endgame parity is measured and reported beside it but
    // deliberately unpinned â€” whether the figure holds at item level 120 is a
    // different question, because the endgame band is far more crit-driven and
    // crit is currently a weapon-lane story. Divergence between the two is a
    // finding in its own right, not a second edge of this one.
    constexpr float AbilityParityBandMinimum = 0.85f;
    constexpr float AbilityParityBandMaximum = 1.15f;

    // The two measurement points. AT-CAP is the character cap; ENDGAME is the
    // top of the item-level ladder. Same character, same choices, same point
    // budget â€” only the gear differs, which is the whole thesis.
    constexpr int32 AtCapItemLevel = 50;
    constexpr int32 EndgameItemLevel = 120;

    // ---------------------------------------------------------------------
    // WHAT A BASELINE IS â€” one definition, applied identically at both points.
    // ---------------------------------------------------------------------
    // The two fixtures used to disagree about this, and the disagreement made
    // both numbers uninterpretable rather than only one.
    //
    // At cap the baseline was WorstTier: every one of twenty-four affix lines
    // landing at the absolute floor of the ladder. That is not "hitting 50 is
    // satisfying with decent power" â€” it is a character that will never exist,
    // and a band measured against an impossible build measures nothing.
    // Endgame, meanwhile, used a hardcoded T3 against T1: a realistically
    // decent roll against a perfect one, which is the right idea.
    //
    // Worse, the two were not merely different, they leaned opposite ways, so
    // comparing 8.08x against 15.40x compared nothing. The endgame figure
    // looking comfortable inside its rails is precisely why nobody checked it.
    //
    // The definition, owner-ruled: a baseline is a REALISTICALLY DECENT roll,
    // derived from item level exactly as the optimized tier is, and offset by
    // the same number of tiers at both points. The offset is 2 because that is
    // what the endgame pair already encoded (T3 against T1) â€” so the endgame
    // number is unchanged by construction and only the broken half moves.
    //
    // The offset is the knob. Widening it makes the baseline worse and the
    // band wider; it is not a free parameter and moving it moves both bands.
    constexpr int32 BaselineTierOffset = 2;

    // Tier numbers count DOWN as they improve, so +offset is worse gear.
    int32 OptimizedTierFor(int32 ItemLevel)
    {
        return UBreakerAffixLibrary::BestTierForItemLevel(ItemLevel);
    }

    int32 BaselineTierFor(int32 ItemLevel)
    {
        return FMath::Min(UBreakerAffixLibrary::WorstTier,
                          OptimizedTierFor(ItemLevel) + BaselineTierOffset);
    }

    // One equipped piece, built from the real pool so a value can never drift
    // away from what the game would actually roll. Tier is the printed tier.
    struct FPiece
    {
        EBreakerEquipSlot Slot;
        int32 Tier;
        TArray<FName> AffixIds;
    };

    FBreakerItemInstance MakeItem(const FPiece& Piece, int32 ItemLevel)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("PowerBand");
        Item.Slot = Piece.Slot;
        Item.Rarity = EBreakerItemRarity::Unwritten;
        // Which band this piece belongs to (O36): AtCapItemLevel or
        // EndgameItemLevel, passed by the caller rather than assumed here.
        Item.ItemLevel = ItemLevel;
        for (const FName AffixId : Piece.AffixIds)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
            if (!Definition) continue;
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = AffixId;
            Rolled.Tier = Piece.Tier;
            Rolled.Category = Definition->Category;
            // The tier's exact value, no in-band lerp: a band test must not
            // depend on a random stream.
            Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Definition, Piece.Tier);
            Item.Affixes.Add(Rolled);
        }
        return Item;
    }

    TArray<FBreakerItemInstance> MakeLoadout(const TArray<FPiece>& Pieces, int32 ItemLevel)
    {
        TArray<FBreakerItemInstance> Items;
        for (const FPiece& Piece : Pieces) Items.Add(MakeItem(Piece, ItemLevel));
        return Items;
    }

    TArray<const UBreakerProgressionNode*> AllNodes()
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        {
            for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.Add(Node);
        }
        return Nodes;
    }

    int32 AllocatedCost(const TArray<FBreakerNodeRank>& Ranks, EBreakerPointCurrency Currency)
    {
        int32 Cost = 0;
        for (const auto* Node : AllNodes())
            if (Node && Node->Currency == Currency)
                for (const auto& Rank : Ranks)
                    if (Rank.NodeId == Node->NodeId) Cost += Rank.Rank * Node->CostPerRank;
        return Cost;
    }

    // Pure allocation witness, not a privileged character grant. This walks the
    // actual tree metadata rank-by-rank under prerequisites, adjacency, local and
    // tree investment gates, neighbor entry, exclusivity and the two real budgets.
    // Live entitlement acquisition is tested separately by progression tests.
    bool AllocationWitness(const TArray<FBreakerNodeRank>& Ranks, FString& Failure)
    {
        TMap<FName, const UBreakerProgressionNode*> Nodes;
        TMap<FName, const UBreakerProgressionTree*> Trees;
        for (const auto* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
            if (Tree) for (const UBreakerProgressionNode* Node : Tree->Nodes)
                if (Node) { Nodes.Add(Node->NodeId, Node); Trees.Add(Node->NodeId, Tree); }
        TMap<FName, int32> Owned, Wanted;
        for (const auto& Rank : Ranks)
        {
            const auto* const* Found = Nodes.Find(Rank.NodeId);
            if (!Found || Wanted.Contains(Rank.NodeId) || Rank.Rank < 1 || Rank.Rank > (*Found)->MaxRank)
            { Failure = TEXT("Missing, duplicate or invalid rank: ") + Rank.NodeId.ToString(); return false; }
            if ((*Found)->Currency == EBreakerPointCurrency::DoctrinePoints
                && Trees[Rank.NodeId]->TreeId != FName(TEXT("Doctrine.Swift.Kinetic")))
            { Failure = TEXT("Fixture mixes doctrine boards"); return false; }
            Wanted.Add(Rank.NodeId, Rank.Rank);
        }
        const int32 CoreCost = AllocatedCost(Ranks, EBreakerPointCurrency::CorePoints);
        const int32 DoctrineCost = AllocatedCost(Ranks, EBreakerPointCurrency::DoctrinePoints);
        if (CoreCost != 65 || DoctrineCost != 8)
        { Failure = FString::Printf(TEXT("Expected 65 Core + 8 doctrine; got %d + %d"), CoreCost, DoctrineCost); return false; }
        int32 Pending = 0;
        for (const auto& Rank : Ranks) Pending += Rank.Rank;
        while (Pending > 0)
        {
            bool bProgress = false;
            for (const auto& Rank : Ranks)
            {
                if (Owned.FindRef(Rank.NodeId) >= Rank.Rank) continue;
                const auto* Node = Nodes[Rank.NodeId]; const auto* Tree = Trees[Rank.NodeId];
                int32 TreeSpent = 0, LocalSpent = 0;
                for (const auto& Pair : Owned)
                    if (Trees[Pair.Key] == Tree)
                    {
                        const int32 Cost = Nodes[Pair.Key]->CostPerRank * Pair.Value;
                        TreeSpent += Cost;
                        if (Nodes[Pair.Key]->Constellation == Node->Constellation) LocalSpent += Cost;
                    }
                bool bAllowed = TreeSpent >= FMath::Max(Node->RequiredTreeInvestment,
                    Node->bCornerstone ? Tree->CornerstoneInvestmentGate : 0)
                    && LocalSpent >= Node->RequiredConstellationInvestment;
                for (const auto& Requirement : Node->Prerequisites)
                    bAllowed &= Owned.FindRef(Requirement.NodeId) >= Requirement.RequiredRank;
                for (const auto& Group : Node->PrerequisiteGroups)
                {
                    TSet<FName> Satisfied;
                    for (const auto& Candidate : Group.Candidates)
                        if (Owned.FindRef(Candidate.NodeId) >= Candidate.RequiredRank) Satisfied.Add(Candidate.NodeId);
                    bAllowed &= Satisfied.Num() >= Group.MinimumSatisfied;
                }
                for (FName Excluded : Node->MutuallyExclusiveNodeIds) bAllowed &= Owned.FindRef(Excluded) == 0;
                if (Tree->EntryNodeIds.Contains(Node->NodeId))
                {
                    if (Tree->bRestrictEntryToOwnedNeighbor && TreeSpent > 0 && Owned.FindRef(Node->NodeId) == 0)
                    {
                        const int32 Index = Tree->CoreWedgeOrder.IndexOfByKey(Node->Constellation);
                        bool bNeighbor = false;
                        if (Index != INDEX_NONE)
                        {
                            const int32 Count = Tree->CoreWedgeOrder.Num();
                            for (const auto& Pair : Owned)
                                if (Trees[Pair.Key] == Tree)
                                    bNeighbor |= Nodes[Pair.Key]->Constellation == Tree->CoreWedgeOrder[(Index + Count - 1) % Count]
                                        || Nodes[Pair.Key]->Constellation == Tree->CoreWedgeOrder[(Index + 1) % Count];
                        }
                        bAllowed &= bNeighbor;
                    }
                }
                else if (!Tree->AdjacencyEdges.IsEmpty())
                {
                    bool bAdjacent = false;
                    for (const auto& Edge : Tree->AdjacencyEdges)
                        bAdjacent |= (Edge.A == Node->NodeId && Owned.FindRef(Edge.B) > 0)
                            || (Edge.B == Node->NodeId && Owned.FindRef(Edge.A) > 0);
                    bAllowed &= bAdjacent;
                }
                if (!bAllowed) continue;
                ++Owned.FindOrAdd(Node->NodeId); --Pending; bProgress = true;
            }
            if (!bProgress)
            {
                TArray<FString> Blocked;
                for (const auto& Rank : Ranks) if (Owned.FindRef(Rank.NodeId) < Rank.Rank) Blocked.Add(Rank.NodeId.ToString());
                Failure = TEXT("No legal next purchase: ") + FString::Join(Blocked, TEXT(", "));
                return false;
            }
        }
        Failure.Reset(); return true;
    }

    // Everything one character is, folded once. The field names are the layer
    // names in the Power-Curve Â§4 table so the report and the doc can be read
    // against each other line by line.
    struct FComposedBuild
    {
        float FlatLayer = 1.0f;        // (Base 1.0 + Added Damage), the multiplicand
        float IncreasedLayer = 1.0f;   // 1 + sum(Increased) / 100, ONE bucket
        float MoreLayer = 1.0f;        // product of at most three Mores (O3)
        float EffectiveCrit = 1.0f;    // Expected crit factor including critical-only weapon More
        float CriticalChance = 0.0f;
        float CriticalMultiplier = 1.5f;
        float ComposedDamageMultiplier = 1.0f; // FlatLayer * IncreasedLayer * MoreLayer
        float Total = 1.0f;            // ComposedDamageMultiplier * EffectiveCrit
        // Ability crit excludes weapon-only Fixate. These are normalized
        // attribute factors; literal hit bases, tempo and multiplicity are excluded.
        float ComposedAbilityMultiplier = 1.0f;
        float AbilityEffectiveCrit = 1.0f;
        float LaterTargetTotal = 1.0f;
        float ExcludedCoreAddedWeaponDamage = 0.0f;
        float ExcludedCoreAddedAbilityPower = 0.0f;
        float ExcludedCoreCastRate = 1.0f;
        float ExcludedFireRate = 1.0f;
        float AbilityFlatLayer = 1.0f;
        float AbilityIncreasedLayer = 1.0f;
        float AbilityMoreLayer = 1.0f;
        float AbilityTotal = 1.0f;
    };

    FComposedBuild Compose(const TArray<FBreakerItemInstance>& Items, const TArray<FBreakerNodeRank>& Ranks,
        const FBreakerBuildConditionState& Conditions)
    {
        FBreakerAttributeContribution EquipmentOffer;
        UBreakerEquipmentComponent::AggregateStats(Items, &EquipmentOffer, Conditions);

        FBreakerAttributeContribution ProgressionOffer;
        const FBreakerNodeStats NodeStats = UBreakerProgressionComponent::AggregateStats(AllNodes(), Ranks, &ProgressionOffer, Conditions);
        // The one line UBreakerProgressionComponent::RecalculateStats adds after
        // the fold: the per-point accumulation floor, into the same additive
        // bucket. Both builds spend the same budget, so this is identical on
        // both sides and cannot be what separates them â€” which is exactly what
        // O27 asked for.
        // Shared, matching RecalculateStats: the floor lands in both lanes.
        ProgressionOffer.AddSharedIncreasedDamage(
            (AllocatedCost(Ranks, EBreakerPointCurrency::CorePoints)
                + AllocatedCost(Ranks, EBreakerPointCurrency::DoctrinePoints)) * GetDefault<UBreakerProgressionComponent>()->IncreasedDamagePerSpentPoint); // the shipped dial, zero under O27

        // The real aggregator, seeded with UBreakerAttributeSet's authored bases.
        FBreakerAttributeAggregator Aggregator;
        const auto* AuthoredBases = GetDefault<UBreakerAttributeSet>();
        float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::MoveSpeed)] = AuthoredBases->GetMoveSpeed();
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::FireRateMultiplier)] = AuthoredBases->GetFireRateMultiplier();
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalChance)] = AuthoredBases->GetCriticalChance();
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::CriticalMultiplier)] = AuthoredBases->GetCriticalMultiplier();
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = AuthoredBases->GetDamageMultiplier();
        Bases[static_cast<int32>(EBreakerAggregatedAttribute::AbilityDamageMultiplier)] = AuthoredBases->GetAbilityDamageMultiplier();
        Aggregator.CaptureBases(Bases);
        Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, EquipmentOffer);
        Aggregator.SetContribution(EBreakerAttributeContributor::Progression, ProgressionOffer);

        FComposedBuild Build;
        Build.FlatLayer = Aggregator.ComposedFlatFactor(EBreakerAggregatedAttribute::DamageMultiplier);
        Build.IncreasedLayer = 1.0f + Aggregator.ComposedIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier) / 100.0f;
        Build.MoreLayer = Aggregator.ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier);
        Build.ComposedDamageMultiplier = Aggregator.Compose(EBreakerAggregatedAttribute::DamageMultiplier);
        Build.CriticalChance = FMath::Clamp(Aggregator.Compose(EBreakerAggregatedAttribute::CriticalChance), 0.0f, 1.0f);
        Build.CriticalMultiplier = FMath::Max(1.0f, Aggregator.Compose(EBreakerAggregatedAttribute::CriticalMultiplier));
        Build.AbilityEffectiveCrit = 1.0f + Build.CriticalChance * (Build.CriticalMultiplier - 1.0f);
        // Normalized attribute multiplier, NOT a fabricated weapon/ability base hit.
        // Resolve each critical outcome natively: Fixate is selected in the joint
        // budget, but pays only on critical weapon hits. Splinter needs a later target.
        auto ExpectedWeapon = [&](bool bBeyondFirst)
        {
            float Expected = 0.0f;
            for (bool bCritical : {false, true})
            {
                FBreakerDamageRequest Request;
                Request.BaseDamage = 1.0f;
                Request.SourceDamageMultiplier = Build.ComposedDamageMultiplier;
                Request.SourceFlatFactor = Build.FlatLayer;
                Request.SourceIncreasedPercent = (Build.IncreasedLayer - 1.0f) * 100.0f;
                Request.SourceMoreProduct = Build.MoreLayer;
                Request.bHasSourceSplit = true;
                Request.WeaponCriticalMoreProduct = Aggregator.GetScopedMoreProduct(false, false, false, false, true, false);
                Request.WeaponBeyondFirstMoreProduct = Aggregator.GetScopedMoreProduct(false, false, false, false, false, true);
                Request.bWeaponBeyondFirstTarget = bBeyondFirst;
                Request.bUseSnapshotCritical = true;
                Request.bSnapshotCriticalResult = bCritical;
                Request.CriticalChance = Build.CriticalChance;
                Request.CriticalMultiplier = Build.CriticalMultiplier;
                const float Weight = bCritical ? Build.CriticalChance : 1.0f - Build.CriticalChance;
                Expected += Weight * UBreakerDamageLibrary::ResolveDamage(Request, FBreakerDefenseState()).RawDamage;
            }
            return Expected;
        };
        Build.Total = ExpectedWeapon(false);
        Build.LaterTargetTotal = ExpectedWeapon(true);
        Build.EffectiveCrit = Build.Total / FMath::Max(KINDA_SMALL_NUMBER, Build.ComposedDamageMultiplier);
        Build.AbilityIncreasedLayer = 1.0f + Aggregator.ComposedIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier) / 100.0f;
        Build.AbilityMoreLayer = Aggregator.ComposedMoreProduct(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Build.AbilityFlatLayer = Aggregator.ComposedFlatFactor(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Build.ComposedAbilityMultiplier = Aggregator.Compose(EBreakerAggregatedAttribute::AbilityDamageMultiplier);
        Build.AbilityTotal = Build.ComposedAbilityMultiplier * Build.AbilityEffectiveCrit;
        Build.ExcludedCoreAddedWeaponDamage = NodeStats.AddedWeaponDamage;
        Build.ExcludedCoreAddedAbilityPower = NodeStats.AddedAbilityPower;
        Build.ExcludedCoreCastRate = NodeStats.AbilityCastRateMultiplier;
        Build.ExcludedFireRate = Aggregator.Compose(EBreakerAggregatedAttribute::FireRateMultiplier);
        return Build;
    }

    // ---- The two characters ------------------------------------------------

    // BASELINE: a full set of gear, every point spent, no direction. Mid-band
    // rolls, Weapon Damage wherever it happened to land, a little crit, one
    // conditional line it did not build around, and no Convergence node at
    // all â€” so no More multiplier. O27's "hitting 50 must be satisfying with
    // decent power" is what this build is. ItemLevel/Tier are the caller's:
    // O36 measures this same character at two different gear depths.
    // Built from the ONE authored baseline in Tests/BreakerBaselineLoadout.h,
    // which BreakerPromotedFindingTests reads for the same character. The list
    // used to live here, and time-to-die was measured against a different
    // character in the other file â€” eight Health lines against this four â€” so
    // the two disagreed by 1.79x on whether the cap met O18.
    TArray<FBreakerItemInstance> BaselineLoadout(int32 ItemLevel, int32 Tier)
    {
        TArray<FPiece> Pieces;
        for (const BreakerBaselineLoadout::FSlotAffixes& Slot : BreakerBaselineLoadout::BreakerBaselineSlots())
        {
            Pieces.Add({Slot.Slot, Tier, Slot.AffixIds});
        }
        return MakeLoadout(Pieces, ItemLevel);
    }

    TArray<FBreakerNodeRank> BaselineRanks()
    {
        // Broad defence/resource allocation; no convergence or keystone.
        // Exactly 65 Core + 8 Kinetic points; fixture witness checks routing and costs.
        return {
            {TEXT("Core.Precision.Sightline"), 1},
            {TEXT("Core.Vector.Line"), 1},
            {TEXT("Core.Ballistics.WeightOfIt"), 1},
            {TEXT("Core.Loadout.Sling"), 1},
            {TEXT("Core.Aegis.Footing"), 1},
            {TEXT("Core.Bulwark.Read"), 1},
            {TEXT("Core.Constitution.Frame"), 1},
            {TEXT("Core.Ward.Resist"), 1},
            {TEXT("Core.Recovery.Mend"), 1},
            {TEXT("Core.Arc.Prime"), 1},
            {TEXT("Core.Tempo.Metronome"), 1},
            {TEXT("Core.Reservoir.Capacity"), 1},
            {TEXT("Core.Duration.Hold"), 1},
            {TEXT("Core.Aegis.IronFrame"), 3},
            {TEXT("Core.Aegis.Brace"), 1},
            {TEXT("Core.Aegis.Plate"), 3},
            {TEXT("Core.Aegis.Bulk"), 1},
            {TEXT("Core.Aegis.SecondSkin"), 3},
            {TEXT("Core.Aegis.AnsweringFire"), 1},
            {TEXT("Core.Aegis.CleanHands"), 1},
            {TEXT("Core.Aegis.SetStance"), 1},
            {TEXT("Core.Bulwark.Guard"), 3},
            {TEXT("Core.Bulwark.Parry"), 1},
            {TEXT("Core.Bulwark.Evade"), 3},
            {TEXT("Core.Bulwark.Counterweight"), 1},
            {TEXT("Core.Bulwark.Interpose"), 3},
            {TEXT("Core.Bulwark.Riposte"), 1},
            {TEXT("Core.Bulwark.Anticipate"), 1},
            {TEXT("Core.Bulwark.Footwork"), 1},
            {TEXT("Core.Constitution.Mass"), 3},
            {TEXT("Core.Constitution.DeepReserve"), 1},
            {TEXT("Core.Constitution.Layered"), 3},
            {TEXT("Core.Constitution.ThirdLayer"), 1},
            {TEXT("Core.Reservoir.Draw"), 3},
            {TEXT("Core.Reservoir.DeepPockets"), 1},
            {TEXT("Core.Reservoir.Tithe"), 2},
            {TEXT("Core.Ward.Tolerance"), 1},
            // O272: four single-rank Kinetic pairs at cost 1 each. Only
            // Downforce (22 Airborne) touches damage.
            {TEXT("Swift.Kinetic.ReadTheRoom"), 1},
            {TEXT("Swift.Kinetic.NoGround"), 1},
            {TEXT("Swift.Kinetic.Downforce"), 1},
            {TEXT("Swift.Kinetic.MomentumShield"), 1},
            {TEXT("Swift.Kinetic.Landing"), 1},
            {TEXT("Swift.Kinetic.AirWork"), 1},
            {TEXT("Swift.Kinetic.Redirect"), 1},
            {TEXT("Swift.Kinetic.SpendToLive"), 1},
        };
    }

    // OPTIMIZED: the airborne Swift build the Velocity constellation exists
    // for. Top-band rolls on every slot, conditional damage lines chosen to
    // match the states it actually holds, and three More sources â€” all of
    // which O3 lets count. This is "optimized 50 feels great" (at cap) / "gear
    // depth is real" (at endgame) depending which ItemLevel/Tier is passed.
    TArray<FBreakerItemInstance> OptimizedLoadout(int32 ItemLevel, int32 Tier)
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::BodyArmour, Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.RedlineDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Gloves,     Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage"), TEXT("Offense.DashDamage")}},
            {EBreakerEquipSlot::Boots,      Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Move.AirControl"), TEXT("Move.DashCooldown")}},
            {EBreakerEquipSlot::Necklace,   Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage")}},
            {EBreakerEquipSlot::Waist,      Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.DashDamage"), TEXT("Offense.AddedDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    Tier, {TEXT("Offense.WeaponDamage"), TEXT("Offense.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.RedlineDamage")}},
            {EBreakerEquipSlot::Secondary,  Tier, {TEXT("Offense.WeaponDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Offense.AddedDamage"), TEXT("Offense.DashDamage")}},
        }, ItemLevel);
    }

    TArray<FBreakerNodeRank> OptimizedRanks()
    {
        // Precision and Ballistics weapon output plus Velocity; no keystone.
        // Exactly 65 Core + 8 Kinetic points; fixture witness checks routing and costs.
        return {
            {TEXT("Core.Precision.Sightline"), 1},
            {TEXT("Core.Precision.Angle"), 3},
            {TEXT("Core.Precision.CalledShot"), 1},
            {TEXT("Core.Precision.Cadence"), 3},
            {TEXT("Core.Precision.TriggerDiscipline"), 1},
            {TEXT("Core.Precision.Ledger"), 3},
            {TEXT("Core.Precision.LongLens"), 1},
            {TEXT("Core.Precision.Steady"), 1},
            {TEXT("Core.Precision.ColdBarrel"), 1},
            {TEXT("Core.Precision.Fixate"), 1},
            {TEXT("Core.Vector.Line"), 1},
            {TEXT("Core.Threat.Presence"), 1},
            {TEXT("Core.Control.Concussive"), 1},
            {TEXT("Core.Kinesis.LightFooting"), 1},
            {TEXT("Core.Velocity.Grind"), 1},
            {TEXT("Core.Velocity.Stride"), 3},
            {TEXT("Core.Velocity.Momentum"), 1},
            {TEXT("Core.Velocity.Slide"), 3},
            {TEXT("Core.Velocity.Carry"), 1},
            {TEXT("Core.Velocity.Sprint"), 3},
            {TEXT("Core.Velocity.Downforce"), 1},
            {TEXT("Core.Velocity.Traction"), 1},
            {TEXT("Core.Velocity.Afterburn"), 1},
            {TEXT("Core.Velocity.TerminalVelocity"), 1},
            {TEXT("Core.Ballistics.WeightOfIt"), 1},
            {TEXT("Core.Ballistics.ShapedCharge"), 3},
            {TEXT("Core.Ballistics.Siege"), 1},
            {TEXT("Core.Ballistics.Range"), 3},
            {TEXT("Core.Ballistics.FlatTrajectory"), 1},
            {TEXT("Core.Ballistics.Concussion"), 1},
            {TEXT("Core.Ballistics.Break"), 1},
            {TEXT("Core.Ballistics.Cull"), 1},
            {TEXT("Core.Ballistics.Loud"), 1},
            {TEXT("Core.Ballistics.Collapse"), 1},
            // O272: four single-rank Kinetic pairs at cost 1 each. Only
            // Downforce (22 Airborne) touches damage.
            {TEXT("Swift.Kinetic.ReadTheRoom"), 1},
            {TEXT("Swift.Kinetic.NoGround"), 1},
            {TEXT("Swift.Kinetic.Downforce"), 1},
            {TEXT("Swift.Kinetic.MomentumShield"), 1},
            {TEXT("Swift.Kinetic.Landing"), 1},
            {TEXT("Swift.Kinetic.AirWork"), 1},
            {TEXT("Swift.Kinetic.Redirect"), 1},
            {TEXT("Swift.Kinetic.SpendToLive"), 1},
        };
    }

    TArray<FBreakerNodeRank> AbilityOptimizedRanks()
    {
        // Arc, Reservoir and Velocity; ability output with resource investment.
        // Exactly 65 Core + 8 Kinetic points; fixture witness checks routing and costs.
        return {
            {TEXT("Core.Arc.Prime"), 1},
            {TEXT("Core.Arc.Channel"), 3},
            {TEXT("Core.Arc.Widen"), 1},
            {TEXT("Core.Arc.Vent"), 3},
            {TEXT("Core.Arc.Reach"), 1},
            {TEXT("Core.Arc.Anchor"), 3},
            {TEXT("Core.Arc.Persistence"), 1},
            {TEXT("Core.Arc.Recycle"), 1},
            {TEXT("Core.Arc.Spillover"), 1},
            {TEXT("Core.Arc.Overflow"), 1},
            {TEXT("Core.Tempo.Metronome"), 1},
            {TEXT("Core.Tempo.Quicken"), 1},
            {TEXT("Core.Tempo.Reset"), 1},
            {TEXT("Core.Reservoir.Capacity"), 1},
            {TEXT("Core.Reservoir.Draw"), 3},
            {TEXT("Core.Reservoir.DeepPockets"), 1},
            {TEXT("Core.Reservoir.Tithe"), 3},
            {TEXT("Core.Reservoir.Wellspring"), 1},
            {TEXT("Core.Reservoir.SecondShift"), 1},
            {TEXT("Core.Duration.Hold"), 1},
            {TEXT("Core.Affliction.OpenWound"), 1},
            {TEXT("Core.Entropy.Attunement"), 1},
            {TEXT("Core.Reaction.Catalysis"), 1},
            {TEXT("Core.Rift.Displace"), 1},
            {TEXT("Core.Void.Erase"), 1},
            {TEXT("Core.Velocity.Grind"), 1},
            {TEXT("Core.Velocity.Stride"), 3},
            {TEXT("Core.Velocity.Momentum"), 1},
            {TEXT("Core.Velocity.Slide"), 3},
            {TEXT("Core.Velocity.Carry"), 1},
            {TEXT("Core.Velocity.Sprint"), 3},
            {TEXT("Core.Velocity.Downforce"), 1},
            {TEXT("Core.Velocity.Traction"), 1},
            {TEXT("Core.Velocity.Afterburn"), 1},
            {TEXT("Core.Velocity.TerminalVelocity"), 1},
            // O272: four single-rank Kinetic pairs at cost 1 each. Only
            // Downforce (22 Airborne) touches damage.
            {TEXT("Swift.Kinetic.ReadTheRoom"), 1},
            {TEXT("Swift.Kinetic.NoGround"), 1},
            {TEXT("Swift.Kinetic.Downforce"), 1},
            {TEXT("Swift.Kinetic.MomentumShield"), 1},
            {TEXT("Swift.Kinetic.Landing"), 1},
            {TEXT("Swift.Kinetic.AirWork"), 1},
            {TEXT("Swift.Kinetic.Redirect"), 1},
            {TEXT("Swift.Kinetic.SpendToLive"), 1},
        };
    }

    TArray<FBreakerNodeRank> AbilityCritVariantRanks()
    {
        // Unpinned crit variant: Arc/Velocity with Precision and longer gateway travel.
        // Exactly 65 Core + 8 Kinetic points; fixture witness checks routing and costs.
        return {
            {TEXT("Core.Arc.Prime"), 1},
            {TEXT("Core.Arc.Channel"), 3},
            {TEXT("Core.Arc.Widen"), 1},
            {TEXT("Core.Arc.Vent"), 3},
            {TEXT("Core.Arc.Reach"), 1},
            {TEXT("Core.Arc.Anchor"), 3},
            {TEXT("Core.Arc.Persistence"), 1},
            {TEXT("Core.Arc.Recycle"), 1},
            {TEXT("Core.Arc.Spillover"), 1},
            {TEXT("Core.Arc.Overflow"), 1},
            {TEXT("Core.Recovery.Mend"), 1},
            {TEXT("Core.Ward.Resist"), 1},
            {TEXT("Core.Constitution.Frame"), 1},
            {TEXT("Core.Bulwark.Read"), 1},
            {TEXT("Core.Aegis.Footing"), 1},
            {TEXT("Core.Loadout.Sling"), 1},
            {TEXT("Core.Ballistics.WeightOfIt"), 1},
            {TEXT("Core.Vector.Line"), 1},
            {TEXT("Core.Threat.Presence"), 1},
            {TEXT("Core.Control.Concussive"), 1},
            {TEXT("Core.Kinesis.LightFooting"), 1},
            {TEXT("Core.Precision.Sightline"), 1},
            {TEXT("Core.Precision.Angle"), 3},
            {TEXT("Core.Precision.CalledShot"), 1},
            {TEXT("Core.Precision.Cadence"), 3},
            {TEXT("Core.Precision.TriggerDiscipline"), 1},
            {TEXT("Core.Velocity.Grind"), 1},
            {TEXT("Core.Velocity.Stride"), 3},
            {TEXT("Core.Velocity.Momentum"), 1},
            {TEXT("Core.Velocity.Slide"), 3},
            {TEXT("Core.Velocity.Carry"), 1},
            {TEXT("Core.Velocity.Sprint"), 3},
            {TEXT("Core.Velocity.Downforce"), 1},
            {TEXT("Core.Velocity.Traction"), 1},
            {TEXT("Core.Velocity.Afterburn"), 1},
            {TEXT("Core.Velocity.TerminalVelocity"), 1},
            {TEXT("Core.Tempo.Metronome"), 1},
            // O272: four single-rank Kinetic pairs at cost 1 each. Only
            // Downforce (22 Airborne) touches damage.
            {TEXT("Swift.Kinetic.ReadTheRoom"), 1},
            {TEXT("Swift.Kinetic.NoGround"), 1},
            {TEXT("Swift.Kinetic.Downforce"), 1},
            {TEXT("Swift.Kinetic.MomentumShield"), 1},
            {TEXT("Swift.Kinetic.Landing"), 1},
            {TEXT("Swift.Kinetic.AirWork"), 1},
            {TEXT("Swift.Kinetic.Redirect"), 1},
            {TEXT("Swift.Kinetic.SpendToLive"), 1},
        };
    }

    // Airborne, recently dashed, at Redline: the rotation the optimized build is
    // organised around, and the state both builds are measured in.
    FBreakerBuildConditionState MeasurementState()
    {
        FBreakerBuildConditionState State;
        State.Set(EBreakerBuildCondition::Airborne, true);
        State.Set(EBreakerBuildCondition::RecentlyDashed, true);
        State.Set(EBreakerBuildCondition::Redline, true);
        // RecentlyRepositioned is TRUE BY CONSTRUCTION wherever RecentlyDashed
        // is: the window is "dashed or completed a ledge traversal inside the
        // last three seconds", and this state already posits the dash. Setting
        // it is describing the same rotation instant more completely, not
        // granting the build anything.
        //
        // IT HAS TO BE SET EXPLICITLY, and that is the trap this state carries:
        // it is a hand-written bitmask, not an evaluation, so a condition it
        // does not name reads FALSE and every line gated on that condition goes
        // silently dark in the measurement. When the eight Swift-locked dash
        // lines were generalised onto this bit, the band fell 12.89x to 10.89x
        // and it looked exactly like a balance consequence — it was this line
        // missing. Scaling those eight affixes to half magnitude moved the band
        // not one thousandth, which is what proved it.
        State.Set(EBreakerBuildCondition::RecentlyRepositioned, true);
        return State;
    }

    // -----------------------------------------------------------------------
    // O200: THE OPTIMIZED SIDE OF THE AT-CAP BAND IS ROLLED, NOT TYPED.
    // -----------------------------------------------------------------------
    // OptimizedLoadout above is a hand-written piece list at one hand-picked
    // tier. It stays as the fixture for RuleBandImpact and ConditionalDamage,
    // where the question is "what does one rewrite do to a KNOWN build". For
    // the band itself it is the wrong instrument: it can only ever hold the
    // lines somebody thought of, at a tier somebody chose, so widening the
    // pool cannot move it and the band cannot read the content.
    //
    // This helper builds the optimized side the way the game builds one: for
    // each of the eight slots it rolls candidates through the real loot
    // pipeline (UBreakerLootLibrary::RollItem, deterministic under a seed) at
    // the band's item level, and keeps per slot the candidate that composes
    // highest by the band's own metric â€” Compose({Candidate}, OptimizedRanks(),
    // MeasurementState()).Total, one piece against the same tree and the same
    // rotation state the band is measured in. Nothing here hand-sets a tier or
    // a value; a legendary or a rolled rule that comes out of the pipeline is
    // let through because the game grants it at that rarity.
    //
    // THE EQUIP CAPS ARE HONOURED, not approximated: at most one Unwritten and
    // three Aberrant pieces are worn at once (EquipLimitForRarity), the rest
    // are Exceptional. Every slot is rolled at all three rarities, and the
    // capped rarities are handed to the slots where they buy the most over
    // that slot's best Exceptional â€” greedy, one slot at a time, per-slot
    // totals rather than a joint optimum, which is stated so nobody reads
    // "best in slot" as "best loadout". Only ONE slot rolls Unwritten at all,
    // so a legendary (its own equip axis, O37) cannot stack a second Unwritten
    // beside it here. Cadence occupies both hands (the slot rule EquipItem
    // enforces), so a Cadence Primary ejects the Secondary from this loadout
    // exactly as it would from a worn one.
    //
    // THE DENOMINATOR DOES NOT MOVE: O200 leaves the baseline as the band's
    // existing definition (BaselineLoadout at BaselineTierFor, the realistically
    // decent roll two tiers under item level). Rolling both sides would make
    // the band a ratio of two lotteries; rolling the optimized side alone makes
    // it "what the pipeline can produce over what a decent drop is".
    //
    // The seed and the candidate count are the band's fixture inputs. Same
    // seed, same content, same answer; a different seed is a different
    // measurement and is reported as one. 64 candidates per slot per rarity is
    // enough that every slot sees most of its pool at every rarity; the whole
    // fold is a few thousand composes and runs inside the suite's budget.
    constexpr int32 RolledBestInSlotSeed = 200;           // O2 PLACEHOLDER (O200)
    constexpr int32 RolledBestInSlotCandidates = 64;      // O2 PLACEHOLDER (O200)

    TArray<FBreakerItemInstance> BreakerPowerBandRolledBestInSlot(int32 ItemLevel, int32 Seed, int32 CandidatesPerSlot,
        bool bAbilityLane = false)
    {
        const FBreakerBuildConditionState State = MeasurementState();
        const TArray<FBreakerNodeRank> Ranks = bAbilityLane ? AbilityOptimizedRanks() : OptimizedRanks();

        constexpr int32 SlotCount = static_cast<int32>(EBreakerEquipSlot::Count);
        // Index 0 is the uncapped rarity every slot falls back to; the capped
        // rarities follow in the order they are handed out (rarest first).
        const EBreakerItemRarity Rarities[] = {
            EBreakerItemRarity::Exceptional,
            EBreakerItemRarity::Unwritten,
            EBreakerItemRarity::Aberrant,
        };
        constexpr int32 RarityCount = UE_ARRAY_COUNT(Rarities);

        struct FBest
        {
            FBreakerItemInstance Item;
            float Total = -1.0f;
        };
        FBest Best[SlotCount][RarityCount];

        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
            for (int32 RarityIndex = 0; RarityIndex < RarityCount; ++RarityIndex)
            {
                for (int32 Candidate = 0; Candidate < CandidatesPerSlot; ++Candidate)
                {
                    // One seed per (slot, rarity, candidate), derived rather than
                    // sequential so a change to CandidatesPerSlot re-seeds only
                    // the candidates it adds.
                    const uint32 Salt = HashCombine(
                        HashCombine(static_cast<uint32>(Seed), static_cast<uint32>(SlotIndex)),
                        static_cast<uint32>(RarityIndex * CandidatesPerSlot + Candidate));
                    FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(
                        TEXT("PowerBand"), Slot, Rarities[RarityIndex], ItemLevel, static_cast<int32>(Salt));
                    const FComposedBuild CandidateBuild = Compose({Item}, Ranks, State);
                    const float Total = bAbilityLane ? CandidateBuild.AbilityTotal : CandidateBuild.Total;
                    if (Total > Best[SlotIndex][RarityIndex].Total)
                    {
                        Best[SlotIndex][RarityIndex].Item = MoveTemp(Item);
                        Best[SlotIndex][RarityIndex].Total = Total;
                    }
                }
            }
        }

        // The caps: hand each capped rarity to the slots where it buys the most
        // over that slot's Exceptional, up to the shipped limit, never to a slot
        // where it buys nothing.
        int32 Choice[SlotCount] = {};
        auto Promote = [&](int32 RarityIndex)
        {
            const int32 Limit = UBreakerEquipmentComponent::EquipLimitForRarity(Rarities[RarityIndex]);
            for (int32 Picked = 0; Picked < Limit; ++Picked)
            {
                int32 BestSlot = INDEX_NONE;
                float BestGain = 0.0f;
                for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
                {
                    if (Choice[SlotIndex] != 0) continue;
                    const float Gain = Best[SlotIndex][RarityIndex].Total - Best[SlotIndex][0].Total;
                    if (Gain > BestGain)
                    {
                        BestGain = Gain;
                        BestSlot = SlotIndex;
                    }
                }
                if (BestSlot == INDEX_NONE) return;
                Choice[BestSlot] = RarityIndex;
            }
        };
        for (int32 RarityIndex = 1; RarityIndex < RarityCount; ++RarityIndex) Promote(RarityIndex);

        TArray<FBreakerItemInstance> Loadout;
        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            Loadout.Add(Best[SlotIndex][Choice[SlotIndex]].Item);
        }

        // The one slot rule the pipeline can hand back: Cadence occupies both
        // hands, so the Secondary leaves, as EquipItem would make it.
        const bool bCadence = Loadout.ContainsByPredicate(
            [](const FBreakerItemInstance& Item) { return Item.Rule == EBreakerItemRule::Cadence; });
        if (bCadence)
        {
            Loadout.RemoveAll([](const FBreakerItemInstance& Item) { return Item.Slot == EBreakerEquipSlot::Secondary; });
        }
        return Loadout;
    }

    // Separate diagnostic: score each replacement in the entire worn loadout.
    // Same candidate stream and ranks as the original per-piece measurement;
    // this is bounded coordinate search, not a claim of a global optimum.
    // The pinned fixtures and their denominator remain independent of it.
    TArray<FBreakerItemInstance> BreakerPowerBandWholeLoadout(int32 ItemLevel, bool bAbilityLane)
    {
        auto Loadout = BreakerPowerBandRolledBestInSlot(ItemLevel, RolledBestInSlotSeed,
            RolledBestInSlotCandidates, bAbilityLane);
        const auto Ranks = bAbilityLane ? AbilityOptimizedRanks() : OptimizedRanks();
        const auto State = MeasurementState();
        const auto Score = [&](const TArray<FBreakerItemInstance>& Items)
        {
            const auto Build = Compose(Items, Ranks, State);
            return bAbilityLane ? Build.AbilityTotal : Build.Total;
        };
        float BestScore = Score(Loadout);
        const EBreakerItemRarity Rarities[] = { EBreakerItemRarity::Exceptional,
            EBreakerItemRarity::Unwritten, EBreakerItemRarity::Aberrant };
        for (int32 Pass = 0; Pass < 3; ++Pass) // O2 PLACEHOLDER: bounded diagnostic search.
        {
            bool bImproved = false;
            for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
                for (int32 RarityIndex = 0; RarityIndex < UE_ARRAY_COUNT(Rarities); ++RarityIndex)
                    for (int32 Candidate = 0; Candidate < RolledBestInSlotCandidates; ++Candidate)
                    {
                        const auto Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
                        const uint32 Salt = HashCombine(HashCombine(static_cast<uint32>(RolledBestInSlotSeed),
                            static_cast<uint32>(SlotIndex)), static_cast<uint32>(RarityIndex * RolledBestInSlotCandidates + Candidate));
                        const auto Item = UBreakerLootLibrary::RollItem(TEXT("PowerBand"), Slot,
                            Rarities[RarityIndex], ItemLevel, static_cast<int32>(Salt));
                        auto Trial = Loadout;
                        Trial.RemoveAll([&](const auto& Existing) { return Existing.Slot == Slot; });
                        Trial.Add(Item);
                        if (Item.Rule == EBreakerItemRule::Cadence)
                            Trial.RemoveAll([](const auto& Existing) { return Existing.Slot == EBreakerEquipSlot::Secondary; });
                        else if (Slot == EBreakerEquipSlot::Secondary && Trial.ContainsByPredicate(
                            [](const auto& Existing) { return Existing.Rule == EBreakerItemRule::Cadence; })) continue;
                        bool bLegal = true;
                        for (const auto Rarity : Rarities)
                        {
                            const int32 Limit = UBreakerEquipmentComponent::EquipLimitForRarity(Rarity);
                            const int32 Count = Trial.FilterByPredicate([&](const auto& Existing) { return Existing.Rarity == Rarity; }).Num();
                            if (Limit > 0 && Count > Limit) bLegal = false;
                        }
                        if (!bLegal) continue;
                        const float TrialScore = Score(Trial);
                        if (TrialScore > BestScore + UE_SMALL_NUMBER)
                        {
                            BestScore = TrialScore;
                            Loadout = MoveTemp(Trial);
                            bImproved = true;
                        }
                    }
            if (!bImproved) break;
        }
        return Loadout;
    }

    // THE BAND ITSELF, so that nothing has to transcribe it. Declared in
    // BreakerPowerBandFixture.h and used by two tests: PowerBand.AtCap emits
    // what this returns, and Combat.PowerCurve.BossOptimized divides by what
    // this returns. Neither keeps a copy, which is the whole point -- the copy
    // that used to live in BossOptimized went stale the first time the band
    // moved and claimed in a comment that it could not.
    //
    // O200: the numerator is the rolled best-in-slot loadout above, under the
    // equip caps, off the live affix table. The denominator is unchanged --
    // O200 leaves it as the band's existing definition.
    float AtCapBand()
    {
        const FBreakerBuildConditionState State = MeasurementState();
        const FComposedBuild Baseline = Compose(
            BaselineLoadout(AtCapItemLevel, BaselineTierFor(AtCapItemLevel)), BaselineRanks(), State);
        const FComposedBuild Optimized = Compose(
            BreakerPowerBandRolledBestInSlot(AtCapItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates),
            OptimizedRanks(), State);
        return Optimized.Total / Baseline.Total;
    }

    // -----------------------------------------------------------------------
    // THE REWRITE CEILINGS, all of them in one place, consumed by the
    // RuleBandImpact tests below.
    // -----------------------------------------------------------------------
    // O2 PLACEHOLDER, and the reason it is stated here rather than felt later:
    // one Unwritten rewrite is the top of the rarity ladder, so it has to be a
    // real step. It must NOT be so large that finding the right Unwritten is
    // worth more than the whole optimized loadout, which is what "choices beat
    // accumulation" (O27) would look like inverted.
    constexpr float MaximumRuleStep = 1.35f;
    // O36's re-anchor: PROLIFIC gets its OWN, HIGHER ceiling at the endgame
    // fixture. Its whole value IS the size of the T1->T0 tier step (it
    // resolves an affix one tier better), and O29 re-sited that spike from
    // x1.4 to x2.2 -- PROLIFIC got materially stronger without anybody editing
    // it, which is a real and expected consequence of the wider ladder, not a
    // balance regression. Re-using the generic 1.35x ceiling here would fail
    // on content working exactly as designed (measured ~1.462x against the
    // old 1.35x ceiling). Every OTHER rollable rewrite stays at 1.35x.
    constexpr float MaximumProlificRuleStep = 1.5f;   // O36, O2 PLACEHOLDER seed
    // MEASURED, NOT CHANGED: 16.0 is the ARITHMETIC mean of 12-20, in a file
    // that treats the band multiplicatively everywhere else -- the at-cap
    // derivation immediately below is Loge(AtCapBandMid) / Loge(EndgameBandMid),
    // i.e. log-space. The geometric mean of 12-20 is 15.4919.
    //
    // The measured no-rewrite build is 15.4720: 0.13% under the geometric mean
    // and 3.30% under this one. Whether that closeness is structural was asked
    // and answered NO in mechanism: nothing in BaselineLoadout, OptimizedLoadout,
    // BaselineRanks, OptimizedRanks or Compose reads a band edge, so no code
    // path pulls the measurement toward either centre -- it is the product of
    // four independently authored layers (1.30 x 2.71 x 1.93 x 2.27). But it is
    // not surprising either: a ratio of two multiplicatively composed builds,
    // tuned by feel to sit mid-band, lands near the GEOMETRIC centre, because
    // that is what "the middle" means for a product. Coincidence in mechanism,
    // a real tendency in kind.
    //
    // WHAT IT WOULD CHANGE, so the owner can rule on it with the numbers in
    // hand: a geometric mid puts the rewrite layer ceiling at 1.2910 rather
    // than 1.2500. Prolific breaches by 13.4% instead of 17.1%, and the
    // authored major-plus-stack of 1.2384 still fits. The red stands either
    // way, which is why this is a measurement and not an edit -- a band edge or
    // its centre is the owner's number.
    constexpr float EndgameBandMid = 16.0f;   // O36 authored 12-20x
    // AND THE SAME INCONSISTENCY IS LIVE HERE, one line down. 9.0 is the
    // arithmetic mean of 8-10; the geometric mean is 8.9443. Recording it
    // beside the note above rather than only there, because a correction
    // applied to one instance of a repeated shape while its neighbour keeps the
    // shape is how this file got two tautologies and four stale justifications.
    //
    // BOTH mids feed the same expression -- Loge(AtCapBandMid) /
    // Loge(EndgameBandMid) -- so switching one and not the other is not a
    // smaller change, it is a wrong one. Consistently geometric:
    // BandShare 0.79248 -> 0.79955 (+0.89%), and MaximumRuleStepAtCap
    // 1.2685 -> 1.2712. No verdict moves: at-cap measures 6.54 against 8-10
    // either way. So this is a note and not an edit, on the same standing rule
    // as the endgame mid -- a band's centre is the owner's number.
    constexpr float AtCapBandMid = 9.0f;      // O36 authored 8-10x

    // A ceiling is a share of the band it sits in, and the two bands are not
    // the same size, so an endgame ceiling carried down to level 50 would
    // assert nothing there. The share is taken in LOG space because a step is
    // multiplicative and so is a band. O2 PLACEHOLDER, AND SO IS THIS LAW
    // ITSELF, which is the part that wants a ruling rather than a number; the
    // arithmetic-share reading (1 + 0.35 * 9/16 = 1.197) is the other
    // candidate and is stated so the choice is visible rather than implied.
    // Anchored to the AUTHORED bands at their midpoints â€” never to the
    // measurements, because the at-cap measurement is itself out of band and
    // expected-red, and anchoring a ceiling to a number that is already wrong
    // bakes the error in twice.
    inline float AtCapCeilingFor(float EndgameCeiling)
    {
        return FMath::Pow(EndgameCeiling, FMath::Loge(AtCapBandMid) / FMath::Loge(EndgameBandMid));
    }

    // ---- O96: the two rewrite-impact ceilings, derived before authoring ----
    // The restructure (O63/O68) makes the worst-case rewrite layer THREE
    // minors plus ONE major, and O96 orders both ceilings derived before any
    // rewrite is authored against them. The derivation:
    //
    // The LAYER: identity has four independently expandable avenues (O33 â€”
    // class, Core axes, gear affixes, rule rewrites) and no avenue may be the
    // trunk, so the rewrite avenue takes an equal LOG share of the authored
    // endgame band midpoint: 16^(1/4) = 2.0. Everything below is arithmetic;
    // this equal-share law is the one seed that wants a ruling (O2).
    //
    // The PARTITION: the major slot inherits the ruled top single step. 1.5 is
    // what O36 already allows at the top of the ladder, the legendary pair
    // already lives under it, and it is 97% spent (the 1.46 the
    // `rewrite-impact` pin tracks) â€” deriving a different major ceiling would
    // re-price shipped content as a side effect. The three-minor stack gets
    // what is left: 2.0 / 1.5 = 4/3. A full stack of three minors is worth
    // less than one major, which is O65's distinction priced â€” a minor changes
    // the terms of a rule, a major changes the shape of what happens on
    // screen.
    //
    // What this prices TODAY: the stack ceiling implies (4/3)^(1/3) = 1.101
    // per minor. When O63 reclassifies the four rolled rewrites as Aberrant's
    // minor pool, any of them worth more than ~1.10 on an optimized build
    // must come down or stay major-slot content â€” that is the breach O96
    // predicted, now a number instead of a surprise.
    //
    // MinorStack deliberately has NO measuring test: no minor classification
    // exists and the equip caps admit one rule today, so a three-minor stack
    // cannot be composed, and a derivation-only test filed under
    // Progression.RuleBandImpact.MinorStack would retire that invariant
    // without measuring a stack â€” the exact partial-test-under-full-name
    // failure the naming comment on the Step test records. The ceiling
    // precedes the content; the measurement arrives with the content.
    // ---- RE-DERIVED, AND THE FIRST DERIVATION WAS WRONG TWICE ------------
    //
    // IT WAS: layer = EndgameBandMid^(1/4) = 2.0, on the reading that O33's four
    // avenues take equal log shares of the endgame band; major inherited
    // Prolific's 1.5 and the stack took the remainder. Both halves failed.
    //
    // WRONG BASIS. The endgame band is measured on a loadout carrying NO
    // rewrite -- the endgame test asserts exactly that, piece by piece ("A
    // power-band piece carries no rewrite despite being Unwritten"), and then
    // asserts the measured band is untouched by the rarity pass. So 16x is what
    // the OTHER avenues produce with the rewrite layer absent, and taking a
    // quarter-share of it for rewrites shares a band that does not contain the
    // thing being shared. A rewrite multiplies that band rather than living
    // inside it.
    //
    // WHAT THE BAND ACTUALLY AFFORDS is the headroom between where the other
    // avenues land and the top of the authored band: 20 / 16 = 1.25. Derived
    // from the two authored edges, not from an invented log split, and stricter
    // than the measured basis (20 / 15.47 = 1.29) because pinning against a
    // measurement would let the ceiling drift with content.
    //
    // THE CONSEQUENCE IS ALREADY SHIPPED, and the first derivation hid it: at
    // 15.47x measured, one Prolific at 1.4634 composes to 22.6x, outside the
    // authored 12-20 band on its own. O96 predicted the restructure would cause
    // a breach; the breach predates it. Progression.RuleBandImpact.LayerFit is
    // the enumerated red that says so.
    // DERIVED, AND WRITTEN AS A DERIVATION so the report can read it without
    // transcribing it. status.py resolves this form; a literal 1.25 here would
    // be the second copy of a number whose whole value is having one.
    constexpr float RewriteLayerCeilingValue = EndgameBandMaximum / EndgameBandMid;
    inline float RewriteLayerCeiling() { return RewriteLayerCeilingValue; }

    // THE SPLIT IS AUTHORED, NOT DERIVED, AND THAT IS THE POINT.
    //
    // The first version defined the stack AS layer/major and then asserted
    // major x stack == layer, which reduces to x * (k/x) == k and is true for
    // every x. It could not fail. That is the defect this report has now found
    // three times in other people's work and once in its own: a value fixed by
    // construction cannot report a problem.
    //
    // All three numbers are independent now. The layer is derived from the band
    // edges; the major and the per-minor are authored placeholders; and the
    // assertion is that what they compose to FITS, which breaks the moment
    // either is raised. O2 PLACEHOLDER on both magnitudes -- the ratio between
    // a major and three minors is the owner's call, and what is not negotiable
    // is that their product stays under the layer.
    constexpr float MaximumMajorStep = 1.15f;   // O2 PLACEHOLDER
    constexpr float MaximumMinorStep = 1.025f;  // O2 PLACEHOLDER, per minor
    inline float MaximumMinorStackStep() { return FMath::Pow(MaximumMinorStep, 3.0f); }
}

// ---------------------------------------------------------------------------
// EVERY FIXTURE NODE ID RESOLVES TO A NODE THAT EXISTS.
//
// THIS IS THE GUARD PHASE 4 NEEDS AND THE PLAN SCHEDULED FOR PHASE 5, which is
// one phase too late: Phase 4 replaces Core's thirty nodes with an atlas of a
// hundred and sixty-eight, and every id in these fixtures changes with it.
//
// AggregateStats drops an unknown id in SILENCE -- BreakerProgressionComponent
// .cpp:941 is `if (!Found) continue;`. So a renamed Core node does not fail
// here, it contributes nothing, and both fixtures quietly compose down toward
// gear-plus-floor with band drift as the only symptom. Thirty-four Core ids are
// named across the two rank lists; a rename lands them all at once, and the
// bands are already out of band, so the drift would arrive looking like the
// thing everyone is expecting to move anyway.
//
// It asserts the ids specifically rather than a rank count, because a count
// would be satisfied by thirty-four ids that all resolve to nothing.
//
// AND EVERY REQUESTED RANK IS ONE THE NODE CAN HOLD â€” the larger half. One
// line below the silent `if (!Found) continue;` sits an equally silent clamp,
// BreakerProgressionComponent.cpp:944:
//     const int32 EffectiveRank = FMath::Min(Rank.Rank, Node->MaxRank);
// The atlas is ranks-free (MaxRank = 1 everywhere in Core), so a fixture row
// asking rank 3 of a KEEP id resolves correctly, passes the id half above,
// and quietly buys one rank. Thirteen rows across the two fixtures do exactly
// that the moment the atlas lands â€” Baseline loses 10 ranks across 6 rows,
// Optimized 12 across 7 â€” and both compose downward into bands already out of
// band, so the symptom is drift shaped exactly like the drift everyone
// expects. This assertion is landed BEFORE any atlas node so those thirteen
// rows fail loudly and are re-pointed deliberately, with the arithmetic
// stated, instead of silently underbuying.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandFixtureIdsResolveTest,
    "RiorsEdge.Progression.PowerBand.FixtureIdsResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandFixtureIdsResolveTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    TMap<FName, int32> KnownMaxRank;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node) KnownMaxRank.Add(Node->NodeId, Node->MaxRank);
        }
    }
    TestTrue(TEXT("The fallback trees carry nodes to resolve against"), KnownMaxRank.Num() > 100);

    int32 Checked = 0;
    TArray<FString> Missing;
    TArray<FString> Overdrawn;
    for (const TCHAR* Label : {TEXT("baseline"), TEXT("optimized"), TEXT("ability-optimized"), TEXT("ability-crit-variant")})
    {
        const TArray<FBreakerNodeRank> Ranks =
            FString(Label) == TEXT("baseline") ? BaselineRanks()
            : FString(Label) == TEXT("optimized") ? OptimizedRanks()
            : FString(Label) == TEXT("ability-optimized") ? AbilityOptimizedRanks() : AbilityCritVariantRanks();
        FString WitnessFailure;
        const bool bWitness = AllocationWitness(Ranks, WitnessFailure);
        TestTrue(*FString::Printf(TEXT("%s has a legal 65+8 allocation witness: %s"), Label, *WitnessFailure), bWitness);
        const FComposedBuild Diagnostic = Compose({}, Ranks, MeasurementState());
        AddInfo(FString::Printf(TEXT("%s: Core=%d doctrine=%d; normalized first-target weapon=%.4f later-target=%.4f ability=%.4f; excluded Core literal AddedWeapon=%.2f AddedAbility=%.2f cast-rate=%.3f; combined fire-rate=%.3f. No hit-base, tempo, multiplicity or full-DPS parity claim."),
            Label, AllocatedCost(Ranks, EBreakerPointCurrency::CorePoints), AllocatedCost(Ranks, EBreakerPointCurrency::DoctrinePoints),
            Diagnostic.Total, Diagnostic.LaterTargetTotal, Diagnostic.AbilityTotal,
            Diagnostic.ExcludedCoreAddedWeaponDamage, Diagnostic.ExcludedCoreAddedAbilityPower, Diagnostic.ExcludedCoreCastRate, Diagnostic.ExcludedFireRate));
        for (const FBreakerNodeRank& Rank : Ranks)
        {
            ++Checked;
            const int32* MaxRank = KnownMaxRank.Find(Rank.NodeId);
            if (!MaxRank)
            {
                Missing.Add(FString::Printf(TEXT("%s: %s"), Label, *Rank.NodeId.ToString()));
            }
            else if (Rank.Rank > *MaxRank)
            {
                Overdrawn.Add(FString::Printf(TEXT("%s: %s asks rank %d of a MaxRank-%d node"),
                    Label, *Rank.NodeId.ToString(), Rank.Rank, *MaxRank));
            }
        }
    }

    auto Disconnected = OptimizedRanks();
    for (auto& Rank : Disconnected)
        if (Rank.NodeId == FName(TEXT("Core.Vector.Line"))) Rank.NodeId = TEXT("Core.Entropy.Attunement");
    FString DisconnectedFailure;
    TestEqual(TEXT("Disconnected witness retains the same 65-point price"),
        AllocatedCost(Disconnected, EBreakerPointCurrency::CorePoints), 65);
    TestFalse(TEXT("Witness refuses equal-cost disconnected wedges"), AllocationWitness(Disconnected, DisconnectedFailure));

    TestTrue(TEXT("Both fixtures name ranks at all"), Checked > 30);
    TestEqual(*FString::Printf(TEXT("Every fixture id resolves to a real node (%d checked): %s"),
        Checked, Missing.Num() ? *FString::Join(Missing, TEXT("; ")) : TEXT("all resolve")),
        Missing.Num(), 0);
    TestEqual(*FString::Printf(TEXT("Every fixture rank is one its node can hold (%d checked): %s"),
        Checked, Overdrawn.Num() ? *FString::Join(Overdrawn, TEXT("; ")) : TEXT("none clamped")),
        Overdrawn.Num(), 0);
    return true;
}

// ---------------------------------------------------------------------------
// O36 split this single test into two, each pinned to its own fixture and its
// own band. NAMING IS LOAD-BEARING: UE's automation tree cannot hold a leaf
// test at a node that is ALSO a parent. Before this split,
// "RiorsEdge.Progression.PowerBand.RuleImpact" did exactly that to
// "RiorsEdge.Progression.PowerBand" â€” the parent path silently swallowed the
// leaf test of the same name, so the 8-10x band assertion was never
// enumerated for as long as that name collision existed (see
// FBreakerRuleBandImpactTest below, which carries the historical fix). The
// guard this pass adds: AtCap and Endgame are SIBLINGS under the
// "RiorsEdge.Progression.PowerBand" node, and no test anywhere in this suite
// may ever be registered at that bare path â€” the moment one is, it silently
// swallows whichever sibling the tree happens to enumerate alongside it, the
// exact failure mode this whole comment documents. If a third PowerBand
// fixture is ever added, give it a sibling name here too, never the bare one.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandAtCapTest,
    "RiorsEdge.Progression.PowerBand.AtCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandAtCapTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    // O36: "AT-CAP (level 50, tiers a level-50 drop can produce): 8-10x
    // stands." WorstTier is always producible (the floor of every roll);
    // BestTierForItemLevel(AtCapItemLevel) is the best item level alone can
    // reach at the character cap (T6) â€” this IS "tiers a level-50 drop can
    // produce", read as the widest legal spread rather than a fixed pair, so
    // the fixture tracks the tier curve instead of hardcoding a value that
    // could silently stop being reachable under a future retune.
    const int32 BaselineTier = BaselineTierFor(AtCapItemLevel);
    const int32 OptimizedTier = OptimizedTierFor(AtCapItemLevel);

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(AtCapItemLevel, BaselineTier), BaselineRanks(), State);
    // O200: the SAME rolled loadout AtCapBand() composes, so the layer report
    // below describes the build whose ratio is emitted and not a different one.
    const FComposedBuild Optimized = Compose(
        BreakerPowerBandRolledBestInSlot(AtCapItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates),
        OptimizedRanks(), State);

    // The layer-by-layer report. Logged rather than only asserted, because the
    // arithmetic is the deliverable: a future tuning pass needs to see WHICH
    // layer moved, not just that the band broke.
    AddInfo(FString::Printf(TEXT("AT-CAP BASELINE  (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        AtCapItemLevel, BaselineTier, Baseline.FlatLayer, Baseline.IncreasedLayer, Baseline.MoreLayer, Baseline.EffectiveCrit,
        Baseline.CriticalChance * 100.0f, Baseline.CriticalMultiplier, Baseline.Total));
    AddInfo(FString::Printf(TEXT("AT-CAP OPTIMIZED (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        AtCapItemLevel, OptimizedTier, Optimized.FlatLayer, Optimized.IncreasedLayer, Optimized.MoreLayer, Optimized.EffectiveCrit,
        Optimized.CriticalChance * 100.0f, Optimized.CriticalMultiplier, Optimized.Total));

    // THE SAME FUNCTION BossOptimized READS. Composed identically from the
    // same fixtures a few lines above -- the pair above stays because the
    // layer-by-layer report needs both halves, but the NUMBER this test emits
    // comes from the one place that number is defined.
    const float Ratio = AtCapBand();
    AddInfo(FString::Printf(TEXT("AT-CAP BAND      flat %.2fx | increased %.2fx | more %.2fx | crit %.2fx => COMPOSED %.2fx (O36 target %.0f-%.0fx)"),
        Optimized.FlatLayer / Baseline.FlatLayer,
        Optimized.IncreasedLayer / Baseline.IncreasedLayer,
        Optimized.MoreLayer / Baseline.MoreLayer,
        Optimized.EffectiveCrit / Baseline.EffectiveCrit,
        Ratio, AtCapBandMinimum, AtCapBandMaximum));
    BreakerStatus::Emit(TEXT("power-band-atcap"), Ratio);

    // THE SPREAD IS DELIBERATELY THE WIDEST ONE, AND THAT IS A FINDING, NOT A
    // CHOICE OF CONVENIENCE. The back-loaded ladder (O29) concentrates almost
    // all of its multiplicative growth between T6 and T1; the shallow low end
    // (T12..T6) that a level-50 drop is confined to has comparatively little
    // gear-tier spread on its own. Narrower baseline/optimized pairings within
    // [T6,T12] were measured against the exact aggregation formula before this
    // fixture was authored and land well under O36's 8x floor â€” the O3 More
    // budget and the node choices (identical in both O36 bands, because
    // character level does not move with item level) carry most of the band
    // here, and gear supplies the rest only at its full available spread.
    // Reported to CONTEXT.md rather than silently absorbed into the fixture.
    TestTrue(*FString::Printf(TEXT("AT-CAP band %.2fx is at least %.1fx"), Ratio, AtCapBandMinimum), Ratio >= AtCapBandMinimum);
    TestTrue(*FString::Printf(TEXT("AT-CAP band %.2fx is at most %.1fx"), Ratio, AtCapBandMaximum), Ratio <= AtCapBandMaximum);

    // Structural properties of the band, each of which the doc states and each
    // of which a tuning pass could break without moving the ratio.

    // O3 is not broken to reach it: at most three Mores, each at or under 1.30.
    TestTrue(TEXT("Optimized More product respects the O3 cap of three at 1.30x each"),
        Optimized.MoreLayer <= FMath::Pow(UBreakerProgressionComponent::SingleMoreCeiling,
            static_cast<float>(UBreakerProgressionComponent::MaxDamageMoreSources)) + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The optimized build actually holds More multipliers"), Optimized.MoreLayer > 1.5f);
    TestEqual(TEXT("The baseline build holds none"), Baseline.MoreLayer, 1.0f, 0.0001f);

    // The band is earned across all three layers, so no single one is the build.
    TestTrue(TEXT("Increased carries part of the band"), Optimized.IncreasedLayer / Baseline.IncreasedLayer > 1.8f);
    TestTrue(TEXT("Crit carries part of the band"), Optimized.EffectiveCrit / Baseline.EffectiveCrit > 1.4f);
    TestTrue(TEXT("No single layer is the whole band"),
        FMath::Max3(Optimized.IncreasedLayer / Baseline.IncreasedLayer, Optimized.MoreLayer / Baseline.MoreLayer,
            Optimized.EffectiveCrit / Baseline.EffectiveCrit) < Ratio * 0.5f);

    // O27: choices beat accumulation. Both builds spend the same budget, so the
    // accumulation term is identical on both sides; strip it from both and the
    // band must barely move. If someone raises IncreasedDamagePerSpentPoint back
    // toward 1.0 this is the assertion that notices.
    const float AccumulationPercent = PowerBandFullPointBudget * GetDefault<UBreakerProgressionComponent>()->IncreasedDamagePerSpentPoint;
    const float BaselineWithout = Baseline.Total * (Baseline.IncreasedLayer - AccumulationPercent / 100.0f) / Baseline.IncreasedLayer;
    const float OptimizedWithout = Optimized.Total * (Optimized.IncreasedLayer - AccumulationPercent / 100.0f) / Optimized.IncreasedLayer;
    const float RatioWithoutAccumulation = OptimizedWithout / BaselineWithout;
    AddInfo(FString::Printf(TEXT("AT-CAP BAND without the per-point accumulation floor: %.2fx"), RatioWithoutAccumulation));
    TestTrue(TEXT("Accumulation is a floor, not the band: removing it widens the band, never narrows it"),
        RatioWithoutAccumulation >= Ratio);

    return true;
}

// ---------------------------------------------------------------------------
// O200: THE ROLLED OPTIMIZED SIDE IS A LOADOUT THE GAME WOULD LET YOU WEAR.
// ---------------------------------------------------------------------------
// The band's numerator now comes out of the loot pipeline, so the thing to
// prove is not a number but LEGALITY: that nothing in the helper grants what
// the game does not. Every piece obeys the equip caps, the per-item category
// caps, and the tier floor item level sets; every line resolves against a
// real pool; and the same seed reproduces the same loadout to the bit, which
// is what makes the emitted band a measurement rather than a draw. The band
// itself is printed here and asserted only in PowerBand.AtCap, whose pin
// carries it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandRolledBestInSlotTest,
    "RiorsEdge.Progression.PowerBand.RolledBestInSlotIsGameLegal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandRolledBestInSlotTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const TArray<FBreakerItemInstance> Loadout =
        BreakerPowerBandRolledBestInSlot(AtCapItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates);
    constexpr int32 SlotCount = static_cast<int32>(EBreakerEquipSlot::Count);

    // Eight pieces, one per slot -- or seven, when and only when a Cadence
    // Primary has ejected the Secondary as the slot rule says it must.
    const bool bCadence = Loadout.ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Rule == EBreakerItemRule::Cadence; });
    const bool bHasSecondary = Loadout.ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Slot == EBreakerEquipSlot::Secondary; });
    TestEqual(TEXT("One piece per slot (Cadence ejects the Secondary)"),
        Loadout.Num(), bCadence ? SlotCount - 1 : SlotCount);
    TestTrue(TEXT("A Cadence Primary is never worn beside a Secondary"), !(bCadence && bHasSecondary));
    TSet<EBreakerEquipSlot> SlotsSeen;
    for (const FBreakerItemInstance& Item : Loadout)
    {
        TestTrue(TEXT("Every piece is a valid item"), Item.IsValid());
        TestFalse(TEXT("No slot is worn twice"), SlotsSeen.Contains(Item.Slot));
        SlotsSeen.Add(Item.Slot);
        TestEqual(TEXT("Every piece is at the band's item level"), Item.ItemLevel, AtCapItemLevel);
    }

    // The shipped equip caps, read from the component rather than restated.
    // Legendaries are counted INTO the Unwritten tally here on purpose: the
    // helper rolls exactly one slot Unwritten, so the stricter reading holds
    // and a legendary can never ride in as a second Unwritten piece.
    int32 UnwrittenCount = 0;
    int32 AberrantCount = 0;
    for (const FBreakerItemInstance& Item : Loadout)
    {
        if (Item.Rarity == EBreakerItemRarity::Unwritten) ++UnwrittenCount;
        if (Item.Rarity == EBreakerItemRarity::Aberrant) ++AberrantCount;
    }
    TestTrue(*FString::Printf(TEXT("At most %d Unwritten piece(s) worn (found %d)"),
        UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Unwritten), UnwrittenCount),
        UnwrittenCount <= UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Unwritten));
    TestTrue(*FString::Printf(TEXT("At most %d Aberrant pieces worn (found %d)"),
        UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant), AberrantCount),
        AberrantCount <= UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant));

    // Per-item shape and tier floor. The floor is what a level-50 drop can
    // produce: BestTierForItemLevel, with the one exception the pipeline
    // itself grants -- an Aberrant's FIRST line is Focused, one tier better,
    // still under the rarity cap. Anything past that is a tier nobody rolled.
    const int32 BestTier = UBreakerAffixLibrary::BestTierForItemLevel(AtCapItemLevel);
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    for (const FBreakerItemInstance& Item : Loadout)
    {
        const FString Context = UEnum::GetValueAsString(Item.Slot);
        TestTrue(*(Context + TEXT(" holds at most four prefixes")),
            UBreakerLootLibrary::CountAffixesOfCategory(Item, EBreakerAffixCategory::Prefix) <= 4);
        TestTrue(*(Context + TEXT(" holds at most four suffixes")),
            UBreakerLootLibrary::CountAffixesOfCategory(Item, EBreakerAffixCategory::Suffix) <= 4);
        TestTrue(*(Context + TEXT(" carries at least one line")), Item.Affixes.Num() > 0);
        for (int32 Index = 0; Index < Item.Affixes.Num(); ++Index)
        {
            const FBreakerRolledAffix& Rolled = Item.Affixes[Index];
            const bool bFocusedLine = Item.Rarity == EBreakerItemRarity::Aberrant && Index == 0;
            const int32 Floor = bFocusedLine
                ? FMath::Max(BestTier - 1, UBreakerAffixLibrary::TierCapForRarity(Item.Rarity))
                : BestTier;
            TestTrue(*FString::Printf(TEXT("%s: %s at T%d is a tier a level-%d drop can produce (floor T%d)"),
                *Context, *Rolled.AffixId.ToString(), Rolled.Tier, AtCapItemLevel, Floor), Rolled.Tier >= Floor);
            TestNotNull(*FString::Printf(TEXT("%s: %s resolves against a real pool"), *Context, *Rolled.AffixId.ToString()),
                UBreakerAffixLibrary::FindAffix(Pool, Rolled.AffixId));
        }
    }

    // The same seed reproduces the same loadout, line for line, and so the
    // same Total. The GUID is the only field the roll does not derive from
    // the seed and nothing in the band reads it.
    const TArray<FBreakerItemInstance> Again =
        BreakerPowerBandRolledBestInSlot(AtCapItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates);
    TestEqual(TEXT("The seed reproduces the piece count"), Again.Num(), Loadout.Num());
    for (int32 PieceIndex = 0; PieceIndex < FMath::Min(Again.Num(), Loadout.Num()); ++PieceIndex)
    {
        const FBreakerItemInstance& A = Loadout[PieceIndex];
        const FBreakerItemInstance& B = Again[PieceIndex];
        TestEqual(TEXT("The seed reproduces the slot"), static_cast<int32>(A.Slot), static_cast<int32>(B.Slot));
        TestEqual(TEXT("The seed reproduces the rarity"), static_cast<int32>(A.Rarity), static_cast<int32>(B.Rarity));
        TestEqual(TEXT("The seed reproduces the rule"), static_cast<int32>(A.Rule), static_cast<int32>(B.Rule));
        TestEqual(TEXT("The seed reproduces the legendary"), A.LegendaryId, B.LegendaryId);
        TestEqual(TEXT("The seed reproduces the line count"), A.Affixes.Num(), B.Affixes.Num());
        for (int32 Index = 0; Index < FMath::Min(A.Affixes.Num(), B.Affixes.Num()); ++Index)
        {
            TestEqual(TEXT("The seed reproduces the line"), A.Affixes[Index].AffixId, B.Affixes[Index].AffixId);
            TestEqual(TEXT("The seed reproduces the tier"), A.Affixes[Index].Tier, B.Affixes[Index].Tier);
            TestEqual(TEXT("The seed reproduces the value"), A.Affixes[Index].Value, B.Affixes[Index].Value, 0.0f);
        }
    }
    const FBreakerBuildConditionState State = MeasurementState();
    const float TotalOnce = Compose(Loadout, OptimizedRanks(), State).Total;
    const float TotalAgain = Compose(Again, OptimizedRanks(), State).Total;
    TestEqual(TEXT("The seed reproduces the composed Total"), TotalOnce, TotalAgain, 0.0f);

    // What was picked, piece by piece, and the band it composes to -- printed
    // so the suite log says which slot bought what; the number is asserted by
    // PowerBand.AtCap, never here.
    for (const FBreakerItemInstance& Item : Loadout)
    {
        TArray<FString> Lines;
        for (const FBreakerRolledAffix& Rolled : Item.Affixes)
        {
            Lines.Add(FString::Printf(TEXT("%s T%d %.1f"), *Rolled.AffixId.ToString(), Rolled.Tier, Rolled.Value));
        }
        AddInfo(FString::Printf(TEXT("ROLLED BIS  %-10s %-11s %s%s| %s"),
            *UEnum::GetValueAsString(Item.Slot).Replace(TEXT("EBreakerEquipSlot::"), TEXT("")),
            *UEnum::GetValueAsString(Item.Rarity).Replace(TEXT("EBreakerItemRarity::"), TEXT("")),
            Item.IsLegendary() ? *FString::Printf(TEXT("%s "), *Item.LegendaryId.ToString()) : TEXT(""),
            Item.HasRule() ? *FString::Printf(TEXT("rule %s "), *UEnum::GetValueAsString(Item.Rule).Replace(TEXT("EBreakerItemRule::"), TEXT(""))) : TEXT(""),
            *FString::Join(Lines, TEXT(", "))));
    }
    AddInfo(FString::Printf(TEXT("ROLLED BIS  seed %d, %d candidates per slot per rarity, ilvl %d => AT-CAP BAND %.2fx (O36 target %.0f-%.0fx)"),
        RolledBestInSlotSeed, RolledBestInSlotCandidates, AtCapItemLevel, AtCapBand(), AtCapBandMinimum, AtCapBandMaximum));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandEndgameTest,
    "RiorsEdge.Progression.PowerBand.Endgame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandEndgameTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();
    const FComposedBuild Baseline = Compose(BaselineLoadout(EndgameItemLevel, BaselineTierFor(EndgameItemLevel)), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel)), OptimizedRanks(), State);

    // The layer-by-layer report. Logged rather than only asserted, because the
    // arithmetic is the deliverable: a future tuning pass needs to see WHICH
    // layer moved, not just that the band broke.
    AddInfo(FString::Printf(TEXT("ENDGAME BASELINE  (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        EndgameItemLevel, BaselineTierFor(EndgameItemLevel), Baseline.FlatLayer, Baseline.IncreasedLayer, Baseline.MoreLayer, Baseline.EffectiveCrit,
        Baseline.CriticalChance * 100.0f, Baseline.CriticalMultiplier, Baseline.Total));
    AddInfo(FString::Printf(TEXT("ENDGAME OPTIMIZED (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f | crit x%.3f (%.0f%% @ x%.2f) => x%.2f"),
        EndgameItemLevel, OptimizedTierFor(EndgameItemLevel), Optimized.FlatLayer, Optimized.IncreasedLayer, Optimized.MoreLayer, Optimized.EffectiveCrit,
        Optimized.CriticalChance * 100.0f, Optimized.CriticalMultiplier, Optimized.Total));

    const float Ratio = Optimized.Total / Baseline.Total;
    AddInfo(FString::Printf(TEXT("ENDGAME BAND      flat %.2fx | increased %.2fx | more %.2fx | crit %.2fx => COMPOSED %.2fx (O36 seed rails %.0f-%.0fx, O2 PLACEHOLDER)"),
        Optimized.FlatLayer / Baseline.FlatLayer,
        Optimized.IncreasedLayer / Baseline.IncreasedLayer,
        Optimized.MoreLayer / Baseline.MoreLayer,
        Optimized.EffectiveCrit / Baseline.EffectiveCrit,
        Ratio, EndgameBandMinimum, EndgameBandMaximum));
    BreakerStatus::Emit(TEXT("power-band-endgame"), Ratio);

    // O36 (O2 PLACEHOLDER SEED): "seed rails 12-20x... the back-loaded ladder
    // currently measures ~15x, accepted pending playtest." This is the ruling
    // that resolves the fixture this test inherited from the pre-split single
    // PowerBand test: O29 widened the affix ladder and raised every ceiling
    // anchor ~2.2x, the 8-10x band was authored against the pre-O29 ladder,
    // and the honest reading was never "the band broke" â€” it is that O29 MOVED
    // WHERE THE TOP OF THE BAND LIVES, past the character cap, into gear
    // depth, which is exactly O29's own thesis ("all endgame character power
    // comes from gear"). See FBreakerPowerBandAtCapTest above for the other
    // half of the split: the SAME character, SAME choices, measured at the
    // character cap instead, stays inside the original 8-10x band. Do not
    // "fix" a future measurement outside this range by widening it again
    // without a new O-ruling â€” that repeats the mistake this split exists to
    // correct.
    TestTrue(*FString::Printf(TEXT("ENDGAME band %.2fx is at least %.1fx"), Ratio, EndgameBandMinimum), Ratio >= EndgameBandMinimum);
    TestTrue(*FString::Printf(TEXT("ENDGAME band %.2fx is at most %.1fx"), Ratio, EndgameBandMaximum), Ratio <= EndgameBandMaximum);

    // Structural properties of the band, each of which the doc states and each
    // of which a tuning pass could break without moving the ratio.

    // O3 is not broken to reach it: at most three Mores, each at or under 1.30.
    TestTrue(TEXT("Optimized More product respects the O3 cap of three at 1.30x each"),
        Optimized.MoreLayer <= FMath::Pow(UBreakerProgressionComponent::SingleMoreCeiling,
            static_cast<float>(UBreakerProgressionComponent::MaxDamageMoreSources)) + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The optimized build actually holds More multipliers"), Optimized.MoreLayer > 1.5f);
    TestEqual(TEXT("The baseline build holds none"), Baseline.MoreLayer, 1.0f, 0.0001f);

    // The band is earned across all three layers, so no single one is the build.
    TestTrue(TEXT("Increased carries part of the band"), Optimized.IncreasedLayer / Baseline.IncreasedLayer > 1.8f);
    TestTrue(TEXT("Crit carries part of the band"), Optimized.EffectiveCrit / Baseline.EffectiveCrit > 1.4f);
    TestTrue(TEXT("No single layer is the whole band"),
        FMath::Max3(Optimized.IncreasedLayer / Baseline.IncreasedLayer, Optimized.MoreLayer / Baseline.MoreLayer,
            Optimized.EffectiveCrit / Baseline.EffectiveCrit) < Ratio * 0.5f);

    // O27: choices beat accumulation. Both builds spend the same budget, so the
    // accumulation term is identical on both sides; strip it from both and the
    // band must barely move. If someone raises IncreasedDamagePerSpentPoint back
    // toward 1.0 this is the assertion that notices.
    const float AccumulationPercent = PowerBandFullPointBudget * GetDefault<UBreakerProgressionComponent>()->IncreasedDamagePerSpentPoint;
    const float BaselineWithout = Baseline.Total * (Baseline.IncreasedLayer - AccumulationPercent / 100.0f) / Baseline.IncreasedLayer;
    const float OptimizedWithout = Optimized.Total * (Optimized.IncreasedLayer - AccumulationPercent / 100.0f) / Optimized.IncreasedLayer;
    const float RatioWithoutAccumulation = OptimizedWithout / BaselineWithout;
    AddInfo(FString::Printf(TEXT("ENDGAME BAND without the per-point accumulation floor: %.2fx"), RatioWithoutAccumulation));
    TestTrue(TEXT("Accumulation is a floor, not the band: removing it widens the band, never narrows it"),
        RatioWithoutAccumulation >= Ratio);

    return true;
}

// ---------------------------------------------------------------------------
// WHAT A RULE REWRITE IS WORTH, measured against the band it has to live in.
// ---------------------------------------------------------------------------
// The band above is unchanged by the rarity pass, and that is by construction:
// FBreakerItemInstance::Rule defaults to None and the two loadouts are authored
// affix by affix, so the power-band characters carry no rewrite even though
// every piece is built at Unwritten to lift the tier cap. Which means the band
// test on its own would say NOTHING about whether the rewrites are balanced.
//
// This is that measurement, run at the ENDGAME fixture (ilvl 120): a rewrite
// is available to a baseline and an optimized character alike, so the number
// that matters is not the 12-20x band but the STEP: what one Unwritten piece
// is worth on top of a build that has already done everything else right.
// Logged in full, because the value of the rewrites is the deliverable and a
// future tuning pass needs to see which one moved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRuleBandImpactTest,
    // SIBLING of PowerBand.AtCap / PowerBand.Endgame above, not a child of
    // either â€” see their shared naming comment for why UE's automation tree
    // makes that load-bearing. This test itself was RENAMED off
    // "RiorsEdge.Progression.PowerBand.RuleImpact" for the identical reason,
    // historically: that path silently swallowed RiorsEdge.Progression
    // .PowerBand itself, so the (then single) band assertion was not
    // enumerated for the whole time the collision existed. Found while
    // measuring O29's effect on the band.
    //
    // AND THE OTHER HALF OF NAMING, which costs more than the collision did:
    // A TEST COVERING PART OF AN INVARIANT IS NAMED FOR THE PART.
    // `make status` credits an asserted invariant when a test of that NAME
    // exists. Nothing checks that the test's SCOPE matches the invariant's, so
    // a partial test filed under the full name retires the whole invariant and
    // the ceiling falls for free. Twice in two days: UI.Teal.ObjectLaw asserts
    // teal on no interface element ANYWHERE and was nearly claimed by a test of
    // one widget pair (it is now UI.Teal.SealedCluster), and this test claimed
    // "rewrite impact stays under its PER-BAND ceiling" while measuring one
    // band. Two bands are asserted, so two bands are measured below.
    //
    // RENAMED AGAIN â€” ".Step" â€” for the FIRST reason: O96's ceilings brought a
    // sibling (RuleBandImpact.Major below, MinorStack to follow when a stack
    // exists), and a test named for the bare prefix swallows its own children
    // in UE's automation tree exactly as PowerBand.RuleImpact once swallowed
    // this one. The name now says what it measures: the STEP of one rollable
    // rewrite on one piece, per band.
    "RiorsEdge.Progression.RuleBandImpact.Step",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRuleBandImpactTest::RunTest(const FString& Parameters)
{
    // Worst step any single rewrite is worth on an optimized build â€” the one
    // figure `make status` tracks for this section. It stays the ENDGAME
    // figure: `rewrite-impact` is pinned against a ceiling derived there, and
    // emitting a max across two bands would move a pinned number sideways
    // without anyone ruling it. The at-cap band is asserted here and reported
    // in the log; whether it earns its own status row is an open question.
    float WorstRuleStep = 0.0f;
    using namespace BreakerPowerBandTest;

    const FBreakerBuildConditionState State = MeasurementState();

    // The ceilings live in the fixture namespace above, beside the O96 pair
    // they now share a derivation block with; the at-cap variants come from
    // AtCapCeilingFor, the log-space band-share law recorded there.
    const float MaximumRuleStepAtCap = AtCapCeilingFor(MaximumRuleStep);
    const float MaximumProlificRuleStepAtCap = AtCapCeilingFor(MaximumProlificRuleStep);
    const float BandShare = FMath::Loge(AtCapBandMid) / FMath::Loge(EndgameBandMid);

    struct FRuleBandFixture
    {
        const TCHAR* Label;
        int32 ItemLevel;
        float Ceiling;
        float ProlificCeiling;
        bool bEmitStatus;
    };
    const FRuleBandFixture Fixtures[] = {
        { TEXT("ENDGAME"), EndgameItemLevel, MaximumRuleStep, MaximumProlificRuleStep, true },
        { TEXT("AT CAP"), AtCapItemLevel, MaximumRuleStepAtCap, MaximumProlificRuleStepAtCap, false },
    };

    AddInfo(FString::Printf(TEXT("CEILINGS  endgame x%.3f / prolific x%.3f  |  at cap x%.3f / prolific x%.3f (band share %.4f)"),
        MaximumRuleStep, MaximumProlificRuleStep, MaximumRuleStepAtCap, MaximumProlificRuleStepAtCap, BandShare));

    for (const FRuleBandFixture& Fixture : Fixtures)
    {
    const int32 BandItemLevel = Fixture.ItemLevel;
    const FComposedBuild Baseline = Compose(BaselineLoadout(BandItemLevel, BaselineTierFor(BandItemLevel)), BaselineRanks(), State);
    const FComposedBuild Optimized = Compose(OptimizedLoadout(BandItemLevel, OptimizedTierFor(BandItemLevel)), OptimizedRanks(), State);
    const float PlainBand = Optimized.Total / Baseline.Total;

    for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
    {
        if (!Definition.bRollable) continue;   // legendaries have their own tests

        // The rewrite lands on ONE piece, because the equip cap is one
        // Unwritten. Helmet: it carries damage, crit and a conditional line, so
        // every rollable rewrite has something on it to bite on.
        TArray<FBreakerItemInstance> WithRule = OptimizedLoadout(BandItemLevel, OptimizedTierFor(BandItemLevel));
        WithRule[0].Rule = Definition.Rule;
        const FComposedBuild Ruled = Compose(WithRule, OptimizedRanks(), State);

        const float Step = Ruled.Total / Optimized.Total;
        const float RuledBand = Ruled.Total / Baseline.Total;
        if (Definition.Rule == EBreakerItemRule::Prolific)
            AddInfo(FString::Printf(TEXT("PROLIFIC [%s] normalized step decomposition: flat %.5fx, Increased %.5fx, standing More %.5fx, expected crit including critical-only More %.5fx. One helmet uplift; authored aggregation fixture, not a full-DPS or equip-valid loadout claim."),
                Fixture.Label, Ruled.FlatLayer / Optimized.FlatLayer, Ruled.IncreasedLayer / Optimized.IncreasedLayer,
                Ruled.MoreLayer / Optimized.MoreLayer, Ruled.EffectiveCrit / Optimized.EffectiveCrit));

        // The SAME step measured while STANDING STILL, and it is not a footnote:
        // the band above is measured airborne, recently dashed and at Redline,
        // which is the one state in which UNBOUND is worth exactly nothing. A
        // rewrite whose whole job is to free conditional lines has to be
        // measured somewhere its conditions are false, or the report says it is
        // worthless when it is the largest rewrite in the table.
        const FBreakerBuildConditionState Grounded;
        const FComposedBuild GroundedPlain = Compose(OptimizedLoadout(BandItemLevel, OptimizedTierFor(BandItemLevel)), OptimizedRanks(), Grounded);
        const FComposedBuild GroundedRuled = Compose(WithRule, OptimizedRanks(), Grounded);
        const float GroundedStep = GroundedRuled.Total / GroundedPlain.Total;

        const float StepCeiling = Definition.Rule == EBreakerItemRule::Prolific ? Fixture.ProlificCeiling : Fixture.Ceiling;
        if (Fixture.bEmitStatus)
        {
            WorstRuleStep = FMath::Max(WorstRuleStep, Step);
        }

        AddInfo(FString::Printf(TEXT("[%-7s] RULE %-12s step x%.3f in rotation | x%.3f standing still | band %.2fx (plain %.2fx) | ceiling x%.3f"),
            Fixture.Label, *Definition.DisplayName.ToString(), Step, GroundedStep, RuledBand, PlainBand, StepCeiling));
        TestTrue(*FString::Printf(TEXT("[%s] %s never lowers a grounded build either"),
            Fixture.Label, *Definition.DisplayName.ToString()), GroundedStep >= 1.0f - UE_KINDA_SMALL_NUMBER);

        TestTrue(*FString::Printf(TEXT("[%s] %s never LOWERS an optimized build's damage"),
            Fixture.Label, *Definition.DisplayName.ToString()), Step >= 1.0f - UE_KINDA_SMALL_NUMBER);
        TestTrue(*FString::Printf(TEXT("[%s] %s is worth at most x%.3f on top of an optimized build (measured x%.3f)"),
            Fixture.Label, *Definition.DisplayName.ToString(), StepCeiling, Step), Step <= StepCeiling);
        // ...and it must not be the whole build. A rewrite that outweighs the
        // endgame band would make every other decision a rounding error.
        TestTrue(*FString::Printf(TEXT("[%s] %s is smaller than the band it lives in"),
            Fixture.Label, *Definition.DisplayName.ToString()), Step < PlainBand);
    }
    }

    // The pass's own claim, asserted: an item with no rewrite composes exactly
    // as it did before rules existed. If this ever fails, a rewrite has leaked
    // out of its item and become a property of rarity.
    const FComposedBuild EndgameBaseline = Compose(BaselineLoadout(EndgameItemLevel, BaselineTierFor(EndgameItemLevel)), BaselineRanks(), State);
    const FComposedBuild EndgameOptimized = Compose(OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel)), OptimizedRanks(), State);
    TArray<FBreakerItemInstance> Untouched = OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel));
    for (const FBreakerItemInstance& Item : Untouched)
    {
        TestEqual(TEXT("A power-band piece carries no rewrite despite being Unwritten"),
            static_cast<int32>(Item.Rule), static_cast<int32>(EBreakerItemRule::None));
    }
    TestEqual(TEXT("The measured band is untouched by the rarity pass"),
        Compose(Untouched, OptimizedRanks(), State).Total / EndgameBaseline.Total,
        EndgameOptimized.Total / EndgameBaseline.Total, 0.0001f);
    // ---- THE LAYER-FIT PAIR IS RETIRED BY RULING (O136) -------------------
    // This foot used to assert two things: the band with the worst rewrite
    // stays inside the authored endgame maximum, and the worst single step
    // fits a layer ceiling derived from the band's edges. Both fired on
    // Prolific (22.64x composed against 12-20x, step 1.4634 against 1.2500)
    // and the owner ruled the breach is the INTENDED FEEL: the band is where
    // most builds land, its upper edge is a target and not a ceiling, and a
    // rewrite stacking past it is the point of the rewrite. An assertion that
    // fires on the intended outcome is an assertion pointed the wrong way, so
    // the pair is gone rather than widened â€” what bounds a rollable rewrite
    // now is its AUTHORED per-step ceiling alone (asserted per rule above:
    // 1.35, Prolific's own 1.5), and `rewrite-impact` pins against
    // MaximumProlificRuleStep. The composed figure stays LOGGED so the report
    // keeps saying where a Prolific build actually lands.
    //
    // RewriteLayerCeilingValue itself survives for O96's OTHER use: the
    // major/minor-stack partition in RuleBandImpact.Major is an authored
    // budget ruling 6 did not touch.
    const float BandWithWorstRewrite = (EndgameOptimized.Total / EndgameBaseline.Total) * WorstRuleStep;
    AddInfo(FString::Printf(
        TEXT("ENDGAME BAND with the worst rewrite: %.2fx x %.4f = %.2fx (authored %.0f-%.0fx is where most builds land, O136 â€” a rewrite past the edge is intended)"),
        EndgameOptimized.Total / EndgameBaseline.Total, WorstRuleStep, BandWithWorstRewrite,
        EndgameBandMinimum, EndgameBandMaximum));

    BreakerStatus::Emit(TEXT("rewrite-impact"), WorstRuleStep);
    return true;
}

// ---------------------------------------------------------------------------
// O96/O68 â€” the MAJOR ceiling, asserted against its full current population.
// No rolled major exists (O63's minor/major classification is unbuilt), and
// O68 rules that a legendary's authored pair OCCUPIES the major slot rather
// than sitting beside it â€” so today the legendaries ARE the majors, and a
// test that waited for rolled majors would leave the ceiling asserted by
// nothing while three legendaries ship against it. The step method is the
// Step test's: the rule lands on the optimized helmet and the measurement is
// what it adds on top of a build that already did everything else right. The
// PAIR's other half â€” a legendary's generic affixes (O87) â€” is the power
// band's own subject, not this ceiling's: the ceiling governs the rewrite.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRuleBandImpactMajorTest,
    "RiorsEdge.Progression.RuleBandImpact.Major",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRuleBandImpactMajorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    // The O96 derivation, asserted where it is consumed: the layer is the
    // O33 four-avenue log share of the authored band, and major times
    // minor-stack spans it exactly. If either equality breaks, somebody
    // edited one constant without re-deriving the pair â€” which is the
    // authoring-before-deriving failure O96 exists to forbid.
    // THERE IS NO ASSERTION HERE THAT THE LAYER CEILING EQUALS THE BAND
    // HEADROOM, AND THAT IS DELIBERATE. There was one, and it was the same
    // tautology as the partition it replaced: RewriteLayerCeilingValue IS
    // EndgameBandMaximum / EndgameBandMid, so asserting the two are equal is
    // A/B == A/B -- true for every value of both edges, with a comment claiming
    // it would fail if an edge moved. Both sides move together.
    //
    // It also could not catch the case it was written for. Replace the
    // declaration with a literal 1.25f and it still passes, because 1.25 really
    // does equal 20/16 -- and a transcribed constant is only wrong LATER, when
    // an edge moves and the copy does not. No runtime assertion can distinguish
    // a derivation from a correct transcription of its result, because at
    // runtime they are the same float.
    //
    // So the guard for that lives where the two forms ARE distinguishable: in
    // the source. Scripts/status.py's parse_band_edges refuses the report if
    // this constant is not declared as a quotient of two named edges. What is
    // asserted here instead is the thing a runtime check can actually see --
    // that what the layer permits FITS, below.

    // THIS ONE CAN FAIL, WHICH THE VERSION BEFORE IT COULD NOT. It asserted
    // major x stack == layer while the stack was DEFINED as layer / major --
    // x * (k/x) == k, true for every x, a partition asserting itself. Now all
    // three are independent and the claim is that they FIT: raise either
    // authored magnitude and this breaks.
    TestTrue(*FString::Printf(
        TEXT("a major plus a three-minor stack fits the rewrite layer (%.4f x %.4f = %.4f, layer %.4f)"),
        MaximumMajorStep, MaximumMinorStackStep(),
        MaximumMajorStep * MaximumMinorStackStep(), RewriteLayerCeiling()),
        MaximumMajorStep * MaximumMinorStackStep() <= RewriteLayerCeiling() + 0.0001f);

    const FBreakerBuildConditionState State = MeasurementState();

    struct FMajorFixture
    {
        const TCHAR* Label;
        int32 ItemLevel;
        float Ceiling;
    };
    const FMajorFixture Fixtures[] = {
        { TEXT("ENDGAME"), EndgameItemLevel, MaximumMajorStep },
        { TEXT("AT CAP"), AtCapItemLevel, AtCapCeilingFor(MaximumMajorStep) },
    };

    // COVERAGE IS THE WHOLE RULE TABLE, split two ways with no remainder:
    // every rollable definition is the Step test's, every non-rollable one is
    // measured here. A future rule kind cannot fall between the two loops â€”
    // a new enum entry needs a definition, and a definition is one or the
    // other.
    int32 MajorCount = 0;
    for (const FMajorFixture& Fixture : Fixtures)
    {
        const FComposedBuild Baseline = Compose(BaselineLoadout(Fixture.ItemLevel, BaselineTierFor(Fixture.ItemLevel)), BaselineRanks(), State);
        const FComposedBuild Optimized = Compose(OptimizedLoadout(Fixture.ItemLevel, OptimizedTierFor(Fixture.ItemLevel)), OptimizedRanks(), State);
        const float PlainBand = Optimized.Total / Baseline.Total;

        for (const FBreakerItemRuleDefinition& Definition : UBreakerItemRuleLibrary::GetRuleDefinitions())
        {
            if (Definition.bRollable) continue;   // the Step test's population

            TArray<FBreakerItemInstance> WithRule = OptimizedLoadout(Fixture.ItemLevel, OptimizedTierFor(Fixture.ItemLevel));
            WithRule[0].Rule = Definition.Rule;
            const FComposedBuild Ruled = Compose(WithRule, OptimizedRanks(), State);
            const float Step = Ruled.Total / Optimized.Total;

            // Grounded as well, for the same reason the Step test measures it:
            // the rotation state is the one state a condition-bending rule
            // (Deadfall) is worth nothing in, and a ceiling only ever checked
            // where the subject is inert asserts nothing.
            const FBreakerBuildConditionState Grounded;
            const FComposedBuild GroundedPlain = Compose(OptimizedLoadout(Fixture.ItemLevel, OptimizedTierFor(Fixture.ItemLevel)), OptimizedRanks(), Grounded);
            const FComposedBuild GroundedRuled = Compose(WithRule, OptimizedRanks(), Grounded);
            const float GroundedStep = GroundedRuled.Total / GroundedPlain.Total;

            AddInfo(FString::Printf(TEXT("[%-7s] MAJOR %-10s step x%.3f in rotation | x%.3f standing still | ceiling x%.3f"),
                Fixture.Label, *Definition.DisplayName.ToString(), Step, GroundedStep, Fixture.Ceiling));

            // Today every authored forfeit is non-damage (air control, the
            // slot, regen), so a legendary rule never lowers the damage
            // total. A major whose FORFEIT is damage would legitimately break
            // this pair of floors â€” re-scope them when one is authored, do
            // not delete the ceiling above.
            TestTrue(*FString::Printf(TEXT("[%s] %s never lowers an optimized build"),
                Fixture.Label, *Definition.DisplayName.ToString()), Step >= 1.0f - UE_KINDA_SMALL_NUMBER);
            TestTrue(*FString::Printf(TEXT("[%s] %s never lowers a grounded build"),
                Fixture.Label, *Definition.DisplayName.ToString()), GroundedStep >= 1.0f - UE_KINDA_SMALL_NUMBER);
            TestTrue(*FString::Printf(TEXT("[%s] %s lands inside the major ceiling x%.3f (measured x%.3f)"),
                Fixture.Label, *Definition.DisplayName.ToString(), Fixture.Ceiling, Step),
                Step <= Fixture.Ceiling);
            TestTrue(*FString::Printf(TEXT("[%s] %s grounded step also lands inside the major ceiling"),
                Fixture.Label, *Definition.DisplayName.ToString()), GroundedStep <= Fixture.Ceiling);
            ++MajorCount;
        }
    }
    // Four legendaries, two fixtures. Each authored legendary joins the loop by
    // existing; a shrink here means a definition vanished from the table.
    TestEqual(TEXT("the major population is every non-rollable rule, both bands"), MajorCount, 8);
    return true;
}

// Conditional lines are the movement pillar's offensive expression. This test
// pins the property that makes them a CHOICE rather than a free bonus: they are
// worth their full value while the state holds and exactly nothing otherwise.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerConditionalDamageTest,
    "RiorsEdge.Progression.ConditionalDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerConditionalDamageTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const TArray<FBreakerItemInstance> Loadout = OptimizedLoadout(EndgameItemLevel, OptimizedTierFor(EndgameItemLevel));
    const TArray<FBreakerNodeRank> Ranks = OptimizedRanks();

    FBreakerBuildConditionState Grounded;
    FBreakerBuildConditionState Airborne;
    Airborne.Set(EBreakerBuildCondition::Airborne, true);

    const FComposedBuild Standing = Compose(Loadout, Ranks, Grounded);
    const FComposedBuild InAir = Compose(Loadout, Ranks, Airborne);

    TestTrue(TEXT("Going airborne raises the additive bucket"), InAir.IncreasedLayer > Standing.IncreasedLayer + 0.5f);
    TestEqual(TEXT("Terminal Velocity is unconditional; airborne changes Increased, not its More"), InAir.MoreLayer, Standing.MoreLayer);
    // The replacement Core's Velocity More is unconditional. Preserve the
    // conditional gear/doctrine question without inheriting the old Core's
    // arbitrary 1.5 total ratio: the live conditional lines must pay exactly
    // their authored Increased amount, once, with all other factors unchanged.
    FBreakerAttributeContribution GroundedEquipment, AirborneEquipment, GroundedNodes, AirborneNodes;
    UBreakerEquipmentComponent::AggregateStats(Loadout, &GroundedEquipment, Grounded);
    UBreakerEquipmentComponent::AggregateStats(Loadout, &AirborneEquipment, Airborne);
    UBreakerProgressionComponent::AggregateStats(AllNodes(), Ranks, &GroundedNodes, Grounded);
    UBreakerProgressionComponent::AggregateStats(AllNodes(), Ranks, &AirborneNodes, Airborne);
    const auto WeaponLane = EBreakerAggregatedAttribute::DamageMultiplier;
    const float GearUplift = AirborneEquipment.GetIncreasedPercent(WeaponLane) - GroundedEquipment.GetIncreasedPercent(WeaponLane);
    const float DoctrineUplift = AirborneNodes.GetIncreasedPercent(WeaponLane) - GroundedNodes.GetIncreasedPercent(WeaponLane);
    TestTrue(TEXT("Airborne gear pays positive conditional Increased"), GearUplift > 0.0f);
    TestEqual(TEXT("The single purchased Kinetic Downforce rank pays its authored 22 percent"), DoctrineUplift, 22.0f, .0001f);
    TestEqual(TEXT("Conditional gear and doctrine enter one additive bucket exactly once"),
        (InAir.IncreasedLayer - Standing.IncreasedLayer) * 100.0f, GearUplift + DoctrineUplift, .001f);
    TestEqual(TEXT("Airborne does not change the flat factor"), InAir.FlatLayer, Standing.FlatLayer, .0001f);
    TestEqual(TEXT("Airborne does not change expected crit or its scoped More"), InAir.EffectiveCrit, Standing.EffectiveCrit, .0001f);
    const float ExpectedConditionalRatio = 1.0f + (GearUplift + DoctrineUplift) / (100.0f * Standing.IncreasedLayer);
    TestEqual(TEXT("Measured conditional benefit equals the authored additive uplift"),
        InAir.Total / Standing.Total, ExpectedConditionalRatio, .0001f);
    TArray<FBreakerNodeRank> CoreOnly;
    for (const auto& Rank : Ranks)
        if (Rank.NodeId.ToString().StartsWith(TEXT("Core."))) CoreOnly.Add(Rank);
    TestEqual(TEXT("This replacement Core allocation does not invent an airborne damage condition"),
        Compose({}, CoreOnly, Airborne).Total, Compose({}, CoreOnly, Grounded).Total, .0001f);
    AddInfo(FString::Printf(TEXT("Airborne normalized damage benefit %.4fx: gear +%.2f%% and doctrine +%.2f%%; Core's standing More remains %.4fx. This excludes literal added hit bases, tempo and multiplicity."),
        InAir.Total / Standing.Total, GearUplift, DoctrineUplift, Standing.MoreLayer));

    // The empty state is the neutral one: nothing conditional pays, and nothing
    // unconditional is lost. This is what keeps every pre-existing call site
    // (the skill screen's projection included) behaving exactly as before.
    TestTrue(TEXT("Unconditional power survives with no condition active"), Standing.IncreasedLayer > 1.5f);
    TestTrue(TEXT("Unconditional More survives with no condition active"), Standing.MoreLayer > 1.0f);

    // Display figures: the tooltip must be able to say what a line is worth
    // before the player is in the state that turns it on.
    const FBreakerEquipmentStats Grounded_Stats = UBreakerEquipmentComponent::AggregateStats(Loadout, nullptr, Grounded);
    const FBreakerEquipmentStats Air_Stats = UBreakerEquipmentComponent::AggregateStats(Loadout, nullptr, Airborne);
    TestEqual(TEXT("Nothing conditional is live while grounded"), Grounded_Stats.ActiveConditionalDamagePercent, 0.0f, 0.0001f);
    TestTrue(TEXT("The potential figure is stated even while grounded"), Grounded_Stats.PotentialConditionalDamagePercent > 0.0f);
    TestTrue(TEXT("Airborne turns part of the potential into live power"),
        Air_Stats.ActiveConditionalDamagePercent > 0.0f
        && Air_Stats.ActiveConditionalDamagePercent < Air_Stats.PotentialConditionalDamagePercent);
    TestEqual(TEXT("Potential does not depend on the state"),
        Air_Stats.PotentialConditionalDamagePercent, Grounded_Stats.PotentialConditionalDamagePercent, 0.0001f);
    return true;
}

// Every slot must be able to raise damage. This is the structural failure
// Power-Curve Â§"More options in every avenue" names outright: "helmet, body,
// boots and waist are structurally incapable of increasing damage".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixBreadthTest,
    "RiorsEdge.Items.Affixes.Breadth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixBreadthTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();

    int32 OffensiveCount = 0;
    int32 ConditionalCount = 0;
    for (const FBreakerAffixDefinition& Affix : Pool)
    {
        if (UBreakerAffixLibrary::IsOffensiveTarget(Affix.StatTarget)) ++OffensiveCount;
        if (Affix.IsConditional()) ++ConditionalCount;
        TestTrue(*(Affix.AffixId.ToString() + TEXT(" rolls on at least one slot")), Affix.AllowedSlots.Num() > 0);
        // No affix may author a More multiplier; those are reserved for trees
        // and Unwritten rule rewrites (O3, Item-Foundation's locked rule).
        TestTrue(*(Affix.AffixId.ToString() + TEXT(" does not author a More multiplier")),
            Affix.StatBucket != EBreakerStatBucket::MorePercent);
    }

    TestTrue(TEXT("The pool is materially wider than the twelve-line slice"), Pool.Num() >= 18);
    TestTrue(TEXT("Offence is a family, not a single line"), OffensiveCount >= 8);
    TestTrue(TEXT("Conditional damage exists at all"), ConditionalCount >= 5);

    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(SlotIndex);
        int32 OffensiveOnSlot = 0;
        int32 ConditionalOnSlot = 0;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            if (!Affix.AllowsSlot(Slot)) continue;
            if (!UBreakerAffixLibrary::IsOffensiveTarget(Affix.StatTarget)) continue;
            ++OffensiveOnSlot;
            if (Affix.IsConditional()) ++ConditionalOnSlot;
        }
        const FString Context = UEnum::GetValueAsString(Slot);
        // The rule O27 exposed: NO slot may be structurally incapable of
        // raising damage.
        TestTrue(*(Context + TEXT(" can raise damage at all")), OffensiveOnSlot >= 2);
        // O54's half of the same invariant, and the half that was written in
        // the spec and never checked: every slot can raise weapon damage AND
        // every slot can raise ability damage. Before the pool split the second
        // clause was not merely unchecked, it was unsatisfiable â€” there was no
        // ability line to roll.
        int32 WeaponLinesOnSlot = 0;
        int32 AbilityLinesOnSlot = 0;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            if (!Affix.AllowsSlot(Slot)) continue;
            const EBreakerStatTarget Target = Affix.StatTarget;
            if (Target == EBreakerStatTarget::WeaponDamage || Target == EBreakerStatTarget::SharedDamage) ++WeaponLinesOnSlot;
            if (Target == EBreakerStatTarget::AbilityDamage || Target == EBreakerStatTarget::SharedDamage) ++AbilityLinesOnSlot;
        }
        TestTrue(*(Context + TEXT(" can raise weapon damage")), WeaponLinesOnSlot >= 1);
        TestTrue(*(Context + TEXT(" can raise ability damage")), AbilityLinesOnSlot >= 1);
        // Per-slot identity: gearing is a set of decisions, so every slot has
        // at least one conditional line of its own to chase.
        TestTrue(*(Context + TEXT(" has a conditional line of its own")), ConditionalOnSlot >= 1);
    }
    return true;
}


// ---------------------------------------------------------------------------
// O54: THE ABILITY LANE, MEASURED FOR THE FIRST TIME
// ---------------------------------------------------------------------------
// Ability throughput was measured at roughly 4% of rifle throughput, and the
// ruled fix was giving abilities a pool of their own to scale in. Until the
// three-pool split there was ONE damage bucket, so an ability build could only
// grow by growing weapon damage - every ability build was a weapon build with
// extra steps, and no number anywhere said so.
//
// This is that number. It reports two things and asserts only the one that is
// derivable today:
//
//   PARITY   - what an ability-geared build's ability lane composes to against
//              a weapon-geared build's weapon lane, at the character cap. The
//              spec asserts this "sits within the parity band"; the band itself
//              is UNAUTHORED, so the figure is emitted and left unpinned and
//              the report prints it without judging it. Measuring before
//              pinning is the correct order, and the one O2 asks for.
//
//   RESPONSE - whether the ability lane responds to being built for at all.
//              That IS derivable, and it is what was actually broken: before
//              the split an ability build's composed ability multiplier was
//              identical to a baseline's, because nothing it could equip or
//              purchase reached a lane that did not exist.
// ---------------------------------------------------------------------------

namespace BreakerPowerBandTest
{
    // THE OPTIMIZED LOADOUT, MIRRORED LINE FOR LINE INTO THE ABILITY POOL.
    // Every offensive line below is the exact twin of the one OptimizedLoadout
    // holds in the same slot: same condition, same tier anchors, same roll
    // weight, differing only in which pool it feeds. Nothing here is granted
    // that the shipped data does not already grant on that slot.
    //
    // IT DID NOT USED TO BE A MIRROR, and the comment that stood here said it
    // was. The weapon build bought 17 increased lines; this bought 11, with no
    // conditional line at all, while byte-identical Ability.Airborne/Redline/
    // Dash twins shipped unbought. It also bought Offense.AddedDamage on two
    // slots — a line that routes ONLY to DamageMultiplier and is therefore
    // dead in the ability lane, which is why the flat layer emitted exactly
    // x1.000. So the pinned parity figure was measuring a fixture's selections
    // and not the two pools, and the code five hundred lines up says as much:
    // this measure "can only ever hold the lines somebody thought of".
    // Corrected rather than pinned around — a wrong instrument gets fixed.
    TArray<FBreakerItemInstance> AbilityOptimizedLoadout(int32 ItemLevel, int32 Tier)
    {
        return MakeLoadout({
            {EBreakerEquipSlot::Helmet,     Tier, {TEXT("Offense.AbilityDamage"), TEXT("Ability.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Ability.AddedPower")}},
            {EBreakerEquipSlot::BodyArmour, Tier, {TEXT("Offense.AbilityDamage"), TEXT("Ability.RedlineDamage"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Gloves,     Tier, {TEXT("Offense.AbilityDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Ability.AddedPower"), TEXT("Ability.DashDamage")}},
            {EBreakerEquipSlot::Boots,      Tier, {TEXT("Offense.AbilityDamage"), TEXT("Ability.AirborneDamage"), TEXT("Move.AirControl"), TEXT("Move.DashCooldown")}},
            {EBreakerEquipSlot::Necklace,   Tier, {TEXT("Offense.AbilityDamage"), TEXT("Ability.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Ability.AddedPower")}},
            {EBreakerEquipSlot::Waist,      Tier, {TEXT("Offense.AbilityDamage"), TEXT("Ability.DashDamage"), TEXT("Ability.AddedPower"), TEXT("Core.Health")}},
            {EBreakerEquipSlot::Primary,    Tier, {TEXT("Offense.AbilityDamage"), TEXT("Ability.AirborneDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Ability.RedlineDamage")}},
            {EBreakerEquipSlot::Secondary,  Tier, {TEXT("Offense.AbilityDamage"), TEXT("Crit.Chance"), TEXT("Crit.Damage"), TEXT("Ability.AddedPower"), TEXT("Ability.DashDamage")}},
        }, ItemLevel);
    }
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPowerBandAbilityLaneTest,
    "RiorsEdge.Progression.PowerBand.AbilityLane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPowerBandAbilityLaneTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPowerBandTest;

    const int32 Tier = OptimizedTierFor(AtCapItemLevel);
    const FBreakerBuildConditionState State = MeasurementState();

    const FComposedBuild WeaponBuild = Compose(OptimizedLoadout(AtCapItemLevel, Tier), OptimizedRanks(), State);
    // Part One-M: the ability build owns an ability-built TREE as well as its
    // gear. Until this, it rode the weapon build's ranks and the measurement
    // asked what an ability loadout gets from a weapon tree.
    const FComposedBuild AbilityBuild = Compose(AbilityOptimizedLoadout(AtCapItemLevel, Tier), AbilityOptimizedRanks(), State);
    const FComposedBuild Baseline = Compose(BaselineLoadout(AtCapItemLevel, BaselineTierFor(AtCapItemLevel)), BaselineRanks(), State);

    AddInfo(FString::Printf(TEXT("ABILITY LANE  weapon build, weapon lane   (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
        AtCapItemLevel, Tier, WeaponBuild.FlatLayer, WeaponBuild.IncreasedLayer, WeaponBuild.MoreLayer, WeaponBuild.Total));
    AddInfo(FString::Printf(TEXT("ABILITY LANE  ability build, ability lane (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
        AtCapItemLevel, Tier, AbilityBuild.AbilityFlatLayer, AbilityBuild.AbilityIncreasedLayer, AbilityBuild.AbilityMoreLayer, AbilityBuild.AbilityTotal));

    // Fixed-build damage-multiplier parity against O99's ruled band. This
    // compares the selected gear/Core contributions and expected crit only.
    // It does not model an ability's base damage, cast cadence, Mana/Overcast,
    // zone retention, or a weapon's ammunition and reload cycle, so it is not
    // a Fracture/Rot DPS measurement.
    //
    // The fixed loadouts retain their original selections. Ordinary ability
    // flat and conditional affixes now exist; the separate rolled and whole-
    // loadout diagnostics below measure that wider pool without replacing
    // this fixture's inputs or denominator. Actual delivery is measured by
    // the Caster burst/sustain and live encounter fixtures.
    const float Parity = AbilityBuild.AbilityTotal / WeaponBuild.Total;
    AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (cap) %.3fx against O99's %.2f-%.2fx"),
        Parity, AbilityParityBandMinimum, AbilityParityBandMaximum));
    BreakerStatus::Emit(TEXT("power-band-ability"), Parity);
    TestTrue(*FString::Printf(TEXT("PARITY %.3fx is at least %.2fx (O99)"), Parity, AbilityParityBandMinimum),
        Parity >= AbilityParityBandMinimum);
    TestTrue(*FString::Printf(TEXT("PARITY %.3fx is at most %.2fx (O99)"), Parity, AbilityParityBandMaximum),
        Parity <= AbilityParityBandMaximum);
    // The decomposition, because a single ratio does not say what to author.
    // ALL FOUR RATIOS PRINT, AND THEIR PRODUCT IS ASSERTED EQUAL TO PARITY.
    // This line used to print two ratios with the parenthetical "crit and
    // More cancel exactly" â€” true when written (pre-atlas, both lanes held
    // one More product) and FALSE from 983b925 on (the weapon lane holds
    // Fixate x Barrage x TV = 1.9349 while the ability lane holds TV alone =
    // 1.30), so the printed decomposition multiplied to 0.40 while the
    // reported parity was 0.27 and the missing x0.672 hid inside a stale
    // claim. Part One-J caught it. A decomposition that asserts its own
    // product cannot make that claim wrongly again.
    const float CapFlatRatio = AbilityBuild.AbilityFlatLayer / WeaponBuild.FlatLayer;
    const float CapIncreasedRatio = AbilityBuild.AbilityIncreasedLayer / WeaponBuild.IncreasedLayer;
    const float CapMoreRatio = AbilityBuild.AbilityMoreLayer / WeaponBuild.MoreLayer;
    const float CapCritRatio = AbilityBuild.AbilityEffectiveCrit / WeaponBuild.EffectiveCrit;
    AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (cap) decomposes: flat %.3fx x increased %.3fx x more %.3fx x crit %.3fx"),
        CapFlatRatio, CapIncreasedRatio, CapMoreRatio, CapCritRatio));
    TestEqual(TEXT("the printed decomposition multiplies to the reported parity"),
        CapFlatRatio * CapIncreasedRatio * CapMoreRatio * CapCritRatio, Parity, 0.001f);

    // PARITY AT ENDGAME, measured and reported, deliberately UNPINNED. Whether
    // the cap figure holds at item level 120 is a different question: the
    // endgame band is far more crit-driven and crit is currently a weapon-lane
    // story, so the two are free to diverge â€” and if they do, THAT is the
    // finding, not a second edge of O99. Asserting it against the cap's band
    // would answer a question nobody has asked yet.
    {
        const int32 EndgameTier = OptimizedTierFor(EndgameItemLevel);
        const FComposedBuild EndgameWeapon = Compose(OptimizedLoadout(EndgameItemLevel, EndgameTier), OptimizedRanks(), State);
        const FComposedBuild EndgameAbility = Compose(AbilityOptimizedLoadout(EndgameItemLevel, EndgameTier), AbilityOptimizedRanks(), State);
        const float EndgameParity = EndgameAbility.AbilityTotal / EndgameWeapon.Total;
        AddInfo(FString::Printf(TEXT("ABILITY LANE  weapon build, weapon lane   (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
            EndgameItemLevel, EndgameTier, EndgameWeapon.FlatLayer, EndgameWeapon.IncreasedLayer, EndgameWeapon.MoreLayer, EndgameWeapon.Total));
        AddInfo(FString::Printf(TEXT("ABILITY LANE  ability build, ability lane (ilvl %d, T%d) flat x%.3f | increased x%.3f | more x%.3f => x%.2f"),
            EndgameItemLevel, EndgameTier, EndgameAbility.AbilityFlatLayer, EndgameAbility.AbilityIncreasedLayer, EndgameAbility.AbilityMoreLayer, EndgameAbility.AbilityTotal));
        AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (endgame) %.3fx â€” UNPINNED; divergence from the cap figure is its own finding"),
            EndgameParity));
        // The same four-ratio form and the same product assertion as the cap
        // decomposition above, for the same Part One-J reason.
        const float EndFlatRatio = EndgameAbility.AbilityFlatLayer / EndgameWeapon.FlatLayer;
        const float EndIncreasedRatio = EndgameAbility.AbilityIncreasedLayer / EndgameWeapon.IncreasedLayer;
        const float EndMoreRatio = EndgameAbility.AbilityMoreLayer / EndgameWeapon.MoreLayer;
        const float EndCritRatio = EndgameAbility.AbilityEffectiveCrit / EndgameWeapon.EffectiveCrit;
        AddInfo(FString::Printf(TEXT("ABILITY LANE  PARITY (endgame) decomposes: flat %.3fx x increased %.3fx x more %.3fx x crit %.3fx"),
            EndFlatRatio, EndIncreasedRatio, EndMoreRatio, EndCritRatio));
        TestEqual(TEXT("the endgame decomposition multiplies to the reported parity"),
            EndFlatRatio * EndIncreasedRatio * EndMoreRatio * EndCritRatio, EndgameParity, 0.001f);
        // BOTH halves widen with gear depth, and the reason is the same in each:
        // the back-loaded ladder multiplies what a line is worth, so a lane with
        // more lines compounds harder as tiers deepen. The breadth deficit is
        // not a constant offset that deep gear dilutes â€” deep gear WIDENS it.
        // That is why parity has to be measured at two points and not one.
        AddInfo(TEXT("ABILITY LANE  the deficit widens with gear depth: a lane with more lines compounds harder up a back-loaded ladder"));
        BreakerStatus::Emit(TEXT("power-band-ability-endgame"), EndgameParity);
    }

    // PART ONE-N'S REPORT, printed and deliberately unpinned: the same
    // gear, the crit-buying tree. If crit moves and increased/More fall, the
    // fixture's 0.863 was a spend choice; if crit moves and nothing falls,
    // the ability wheels have slack the weapon wheels do not.
    {
        const FComposedBuild CritVariant = Compose(AbilityOptimizedLoadout(AtCapItemLevel, Tier), AbilityCritVariantRanks(), State);
        const float VariantParity = CritVariant.AbilityTotal / WeaponBuild.Total;
        AddInfo(FString::Printf(TEXT("ABILITY LANE  CRIT VARIANT (cap, unpinned) parity %.3fx: flat %.3fx x increased %.3fx x more %.3fx x crit %.3fx"),
            VariantParity,
            CritVariant.AbilityFlatLayer / WeaponBuild.FlatLayer,
            CritVariant.AbilityIncreasedLayer / WeaponBuild.IncreasedLayer,
            CritVariant.AbilityMoreLayer / WeaponBuild.MoreLayer,
            CritVariant.AbilityEffectiveCrit / WeaponBuild.EffectiveCrit));
    }

    // The fixed fixture above predates conditional ability affixes. Report a
    // second comparison selected from the actual loot pipeline so new content
    // can be measured, without replacing the existing parity assertion. Both
    // lanes see the same seeds, candidates, rarity caps and condition state;
    // only their tree and the score used to choose a piece differ.
    for (const int32 ItemLevel : { AtCapItemLevel, EndgameItemLevel })
    {
        const TArray<FBreakerItemInstance> RolledWeapons = BreakerPowerBandRolledBestInSlot(
            ItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates);
        const TArray<FBreakerItemInstance> RolledAbilities = BreakerPowerBandRolledBestInSlot(
            ItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates, true);
        const FComposedBuild Weapon = Compose(RolledWeapons, OptimizedRanks(), State);
        const FComposedBuild Ability = Compose(RolledAbilities, AbilityOptimizedRanks(), State);
        TestTrue(TEXT("Rolled weapon comparison has positive power"), Weapon.Total > 0.0f);
        TestTrue(TEXT("Rolled ability comparison has positive power"), Ability.AbilityTotal > 0.0f);
        AddInfo(FString::Printf(TEXT("ABILITY LANE  ROLLED (ilvl %d, seed %d, %d candidates/slot/rarity) weapon x%.3f ability x%.3f parity %.3fx â€” measured, not pinned"),
            ItemLevel, RolledBestInSlotSeed, RolledBestInSlotCandidates,
            Weapon.Total, Ability.AbilityTotal, Ability.AbilityTotal / FMath::Max(Weapon.Total, UE_SMALL_NUMBER)));
        AddInfo(FString::Printf(TEXT("ABILITY LANE  ROLLED decomposition: flat %.3fx x increased %.3fx x more %.3fx x crit %.3fx"),
            Ability.AbilityFlatLayer / Weapon.FlatLayer, Ability.AbilityIncreasedLayer / Weapon.IncreasedLayer,
            Ability.AbilityMoreLayer / Weapon.MoreLayer, Ability.AbilityEffectiveCrit / Weapon.EffectiveCrit));
        const auto WholeWeapons = BreakerPowerBandWholeLoadout(ItemLevel, false);
        const auto WholeAbilities = BreakerPowerBandWholeLoadout(ItemLevel, true);
        for (const auto* Selected : { &WholeWeapons, &WholeAbilities })
        {
            auto* Owner = NewObject<AActor>(GetTransientPackage());
            auto* Equipment = NewObject<UBreakerEquipmentComponent>(Owner);
            for (const auto& Item : *Selected)
                TestTrue(TEXT("whole-loadout candidate passes actual equipment rules"), Equipment->EquipItem(Item));
            TestEqual(TEXT("whole-loadout candidate keeps every selected piece equipped"), Equipment->GetEquipped().Num(), Selected->Num());
        }
        const auto WholeWeapon = Compose(WholeWeapons, OptimizedRanks(), State);
        const auto WholeAbility = Compose(WholeAbilities, AbilityOptimizedRanks(), State);
        TestTrue(TEXT("whole-loadout weapon search cannot worsen its starting loadout"), WholeWeapon.Total >= Weapon.Total);
        TestTrue(TEXT("whole-loadout ability search cannot worsen its starting loadout"), WholeAbility.AbilityTotal >= Ability.AbilityTotal);
        AddInfo(FString::Printf(TEXT("ABILITY LANE  WHOLE LOADOUT (ilvl %d, same seed/pool/ranks) weapon x%.3f ability x%.3f parity %.3fx; bounded search, not pinned"),
            ItemLevel, WholeWeapon.Total, WholeAbility.AbilityTotal, WholeAbility.AbilityTotal / WholeWeapon.Total));
    }

    // RESPONSE - the assertion the pools were built to make possible.
    const float Response = AbilityBuild.AbilityTotal / Baseline.AbilityTotal;
    AddInfo(FString::Printf(TEXT("ABILITY LANE  RESPONSE %.2fx (an ability build's lane against a baseline's)"), Response));
    TestTrue(*FString::Printf(TEXT("The ability lane responds to being built for (%.2fx)"), Response), Response > 2.0f);

    // The shared pool reaches BOTH lanes - the one structural claim of the
    // three-pool model that no single ratio can show. An ability build still
    // carries the shared line and the per-point floor in its weapon lane.
    TestTrue(TEXT("A shared line reaches the weapon lane of an ability build"),
        AbilityBuild.IncreasedLayer > 1.0f);
    TestTrue(TEXT("An ability build's ability lane outgrows its own weapon lane"),
        AbilityBuild.AbilityIncreasedLayer > AbilityBuild.IncreasedLayer);
    // And the mirror: a weapon build's ability lane carries the shared pool and
    // nothing narrow, so it sits below its weapon lane. Neither lane is a copy
    // of the other, which is the whole point of partitioning them.
    TestTrue(TEXT("A weapon build's ability lane sits below its weapon lane"),
        WeaponBuild.AbilityIncreasedLayer < WeaponBuild.IncreasedLayer);

    // O74: ONE More ceiling spanning the pools. Neither lane alone may pass
    // 1.30^3; the strongest-three selection upstream is what stops a build
    // holding three in each, and this is the belt-and-braces reading of it.
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
    TestTrue(TEXT("The ability lane's More product respects the one ceiling"),
        AbilityBuild.AbilityMoreLayer <= Ceiling + UE_KINDA_SMALL_NUMBER);
    TestTrue(TEXT("The weapon lane's More product respects the one ceiling"),
        AbilityBuild.MoreLayer <= Ceiling + UE_KINDA_SMALL_NUMBER);

    return true;
}

#endif
