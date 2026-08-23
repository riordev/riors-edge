#pragma once

#include "CoreMinimal.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Progression/BreakerProgressionTypes.h"

struct FBreakerItemInstance;
class UBreakerAttributeSet;
class UBreakerProgressionComponent;
class UBreakerProgressionNode;

// ---------------------------------------------------------------------------
// "You can't really see the numerical significance of your points."
//
// This is the arithmetic behind the skill screen's two honest numbers:
//
//   1. What the player's spending has ADDED UP TO — the composed totals, read
//      out of the real attribute aggregator, not recomputed alongside it.
//   2. What one more point would BUY — the same totals recomposed with one
//      hypothetical extra rank, so the screen can print "1.13x -> 1.16x"
//      instead of the useless "+3% per rank".
//
// The projection MUST NOT drift from the live numbers, so it does not invent a
// second aggregation: it reuses UBreakerProgressionComponent::AggregateStats
// (the production fold) and composes through a COPY of the character's own
// FBreakerAttributeAggregator, with only the progression contributor swapped.
// Gear, the bases, and the aggregation law itself come from the live object.
// The single duplicated line is the per-spent-point damage baseline, which the
// component adds after AggregateStats returns; BuildOffer mirrors it, and a
// test pins the mirror against a live component.
//
// Everything here is world-free, widget-free and pure, which is why it lives
// beside the screen rather than inside it.
// ---------------------------------------------------------------------------

enum class EBreakerStatFormat : uint8
{
    // 1.13x — a 1.0-based multiplier.
    Multiplier,
    // Stored as a 0..1 fraction, printed 12.0%.
    PercentPoints,
    // 580 — an absolute magnitude.
    Absolute,
};

// One readable row: what it is now, what it would become, and how to print it.
struct RIORSEDGE_API FBreakerStatLine
{
    FString Label;
    float Before = 0.0f;
    float After = 0.0f;
    EBreakerStatFormat Format = EBreakerStatFormat::Multiplier;
    // True when only the tree layer feeds this row, so the UI can say so
    // rather than implying gear is included.
    bool bTreeOnly = false;

    bool Changed() const;
};

// Everything a projection needs, captured once per screen rebuild. No live
// pointer is dereferenced after construction (the node pointers are the
// content library's rooted static assets, which outlive the screen), so a
// snapshot can be handed to a hover handler by value.
struct RIORSEDGE_API FBreakerSkillSnapshot
{
    // Every node the character may hold a rank in, both currencies.
    TArray<const UBreakerProgressionNode*> Nodes;
    // The character's current ranks, class ranks then core ranks — the same
    // concatenation UBreakerProgressionComponent::RecalculateStats builds.
    TArray<FBreakerNodeRank> Ranks;
    // UBreakerProgressionComponent::IncreasedDamagePerSpentPoint.
    float IncreasedDamagePerSpentPoint = 0.0f;
    // A copy of the character's live aggregator: true bases, live gear
    // contribution, live progression contribution.
    FBreakerAttributeAggregator Aggregator;
    // False when the character has no attribute set with captured bases (no
    // ability system, a menu opened outside play). The tree-layer rows still
    // read true; the composed rows fall back to the tree layer alone and are
    // flagged bTreeOnly so nothing lies.
    bool bHasComposedAttributes = false;
};

namespace BreakerSkillProjection
{
    // Reads the live objects ONCE. Survives both being null: the result is an
    // empty snapshot whose totals are all identity, which is exactly what the
    // no-progression / no-class guards want to render.
    RIORSEDGE_API FBreakerSkillSnapshot MakeSnapshot(const UBreakerProgressionComponent* Progression, const UBreakerAttributeSet* Attributes);

    // Copy of Ranks with NodeId's rank moved by Delta (clamped at zero); adds
    // the entry when it is absent and Delta is positive.
    RIORSEDGE_API TArray<FBreakerNodeRank> WithRankDelta(const TArray<FBreakerNodeRank>& Ranks, FName NodeId, int32 Delta);

    // Points committed to nodes: rank x CostPerRank, exactly the arithmetic
    // UBreakerProgressionComponent::GetRefundValue charges and refunds. An
    // unknown node costs 1, matching the component's fallback.
    RIORSEDGE_API int32 CommittedPoints(const TArray<const UBreakerProgressionNode*>& Nodes, const TArray<FBreakerNodeRank>& Ranks);

    // The progression layer's COMPLETE offer for a hypothetical rank set,
    // baseline included. Mirrors RecalculateStats; pinned by automation.
    RIORSEDGE_API FBreakerNodeStats BuildOffer(const FBreakerSkillSnapshot& Snapshot,
        const TArray<FBreakerNodeRank>& Ranks, FBreakerAttributeContribution& OutOffer);

    // How many rows every projection produces. Exported because two tests
    // transcribed it as 10, so adding the ability-damage lane broke them both
    // with an arithmetic complaint rather than a design one — a row count
    // written down beside the row table is the same second source as any other,
    // and this one had already gone stale by the time anybody read it.
    RIORSEDGE_API int32 StatRowCount();

    // Every readable row, valued at Snapshot.Ranks in Before and at AfterRanks
    // in After. Rows are always produced in the same order, so callers may
    // index them.
    RIORSEDGE_API TArray<FBreakerStatLine> Project(const FBreakerSkillSnapshot& Snapshot, const TArray<FBreakerNodeRank>& AfterRanks);

    // Project(Snapshot, Snapshot.Ranks with RankDelta applied to NodeId).
    RIORSEDGE_API TArray<FBreakerStatLine> ProjectPurchase(const FBreakerSkillSnapshot& Snapshot, FName NodeId, int32 RankDelta);

    // The build summary: every row with Before == After.
    RIORSEDGE_API TArray<FBreakerStatLine> CurrentTotals(const FBreakerSkillSnapshot& Snapshot);

    // How much of an Increased bucket each layer is currently supplying, in
    // whole percent. This is what makes "my points did that" visible.
    RIORSEDGE_API float LayerIncreasedPercent(const FBreakerSkillSnapshot& Snapshot,
        EBreakerAttributeContributor Contributor, EBreakerAggregatedAttribute Attribute);

    // ---- WHAT EQUIPPING THIS WOULD DO ------------------------------------
    // The same trick as the skill projection, on the OTHER contributor. The
    // aggregator holds exactly two — Equipment and Progression — and
    // ComposeWithOffer swaps the second; this swaps the first, by re-running
    // the equipment layer's own static AggregateStats over the loadout with one
    // slot replaced. No second aggregation, no recomputed gear maths, and the
    // bases and the fold are still the live character's.
    //
    // IT REPORTS PER LANE, and that is the entire reason it exists rather than
    // a single number. A scalar "item score" cannot be honest across a
    // partitioned damage model: the two delivery lanes measure 0.647x of each
    // other at the cap and 0.388x at endgame, so one figure is wrong for
    // whichever build the player actually has. Weapon and ability are reported
    // separately because O54 partitions them by DELIVERY and no weighting
    // between them is knowable from the item.
    //
    // Defence is honestly ONE number where damage is not, because survivability
    // composes for everybody — but it is effective health AGAINST PHYSICAL, and
    // the row says so. Block and dodge are chance layers (O1) and folding an
    // average of them into a displayed total would be the same lie in a smaller
    // font: a number that is right about nobody's actual hit.
    RIORSEDGE_API TArray<FBreakerStatLine> ProjectEquip(const FBreakerSkillSnapshot& Snapshot,
        const TArray<FBreakerItemInstance>& Equipped, const FBreakerItemInstance& Candidate);

    RIORSEDGE_API FString FormatStat(float Value, EBreakerStatFormat Format);
    // "+3%" / "+2.0%" / "+90". Empty when nothing moved.
    RIORSEDGE_API FString FormatDelta(const FBreakerStatLine& Line);
    // "1.13x -> 1.16x".
    RIORSEDGE_API FString FormatTransition(const FBreakerStatLine& Line);
}
