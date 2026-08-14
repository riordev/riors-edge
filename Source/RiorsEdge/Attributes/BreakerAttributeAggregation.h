#pragma once

#include "CoreMinimal.h"

// One application path for every attribute more than one layer writes.
//
// The bug this replaces: equipment and progression each snapshotted their own
// "base" attribute value and then wrote an absolute result, so whichever
// recalculated last erased the other's contribution — gear affixes and skill
// nodes did not stack. Here the base is owned in exactly one place (the
// attribute set, captured once, before any contributor has written), each
// layer submits its complete offer, and every attribute is re-derived from
// bases + all offers on every change. Order of operations cannot matter, and
// removal is exact because nothing is ever written incrementally.
//
// Aggregation is the LOCKED rule (Docs/Item-Foundation.md):
//   value = (Base + sum(Flat)) * (1 + sum(IncreasedPercent) / 100) * prod(More)
// Flat sums first, all Increased percentages form ONE additive bucket per
// stat across every contributor, and More multipliers compose multiplicatively
// (reserved for trees and Anomalous items, capped by ruling O3).

// The attributes the unified pass owns. An attribute a single system owns
// outright (Shield, Armor, ...) deliberately stays out.
enum class EBreakerAggregatedAttribute : uint8
{
    MaxHealth,
    MaxClassResource,
    CriticalChance,
    CriticalMultiplier,
    MoveSpeed,
    DamageOverTimeMultiplier,
    // Outgoing damage scaling. THE single place damage Increased percentages
    // land: gear's Weapon Damage affix and every skill node that raises damage
    // both bid here, so they form one additive bucket instead of composing
    // multiplicatively at the weapon. Base 1.0, so a 20% gear roll and a 15%
    // tree allocation read 1.35 — never 1.20 * 1.15.
    DamageMultiplier,
    // --- Movement composition ---------------------------------------------
    // Three multiplier-shaped attributes, base 1.0, exactly like
    // DamageMultiplier. They exist for the same reason it does: before them
    // UBreakerCharacterMovementComponent read gear and tree movement
    // multipliers separately and MULTIPLIED them, so +20% boots and +20% of
    // tree read x1.44 against a locked rule that says x1.40. Routing both
    // layers through one aggregated attribute makes the additive bucket
    // structural rather than a convention the movement layer has to remember.
    //
    // DashCooldownReduction is a REDUCTION, not a cooldown: the composed value
    // is the divisor (x1.20 == a 20% shorter dash cooldown). Storing it that
    // way is what lets it share the additive bucket at all — an attribute
    // holding the cooldown in seconds would have to fold percentages into a
    // duration, and two layers doing that could not be additive.
    SlideSpeedMultiplier,
    AirControlMultiplier,
    DashCooldownReduction,
    Count
};

// The closed set of layers allowed to contribute. It is an enum rather than a
// name map on purpose: the fold then runs in a fixed order, so the composed
// result is bit-for-bit identical no matter which layer recalculated last.
enum class EBreakerAttributeContributor : uint8
{
    Equipment,
    Progression,
    Count
};

// One layer's complete, re-derivable offer. A contributor rebuilds this from
// scratch whenever anything it owns changes; it never mutates an attribute.
struct RIORSEDGE_API FBreakerAttributeContribution
{
    static constexpr int32 AttributeCount = static_cast<int32>(EBreakerAggregatedAttribute::Count);

    FBreakerAttributeContribution() { Reset(); }

    // Back to contributing nothing: flat 0, increased 0, more x1.
    void Reset();

    void AddFlat(EBreakerAggregatedAttribute Attribute, float Value);
    // Whole percent, matching how affixes and node effects are authored
    // (5.0 == 5%). Every source lands in the same additive bucket.
    void AddIncreasedPercent(EBreakerAggregatedAttribute Attribute, float Percent);
    // Composes into this contributor's More product. Reserved for tree
    // keystones and Anomalous rule rewrites (O3 caps the composed budget).
    void ComposeMore(EBreakerAggregatedAttribute Attribute, float Multiplier);

    float GetFlat(EBreakerAggregatedAttribute Attribute) const;
    float GetIncreasedPercent(EBreakerAggregatedAttribute Attribute) const;
    float GetMore(EBreakerAggregatedAttribute Attribute) const;

    // True when this contributor would move nothing at all.
    bool IsIdentity() const;

private:
    float Flat[AttributeCount];
    float IncreasedPercent[AttributeCount];
    float MoreMultiplier[AttributeCount];
};

// Owns the true base values and folds every contribution over them. Held by
// UBreakerAttributeSet; usable standalone so the math is testable with no
// actor, no world, and no ability system.
struct RIORSEDGE_API FBreakerAttributeAggregator
{
    static constexpr int32 AttributeCount = FBreakerAttributeContribution::AttributeCount;
    static constexpr int32 ContributorCount = static_cast<int32>(EBreakerAttributeContributor::Count);

    // Idempotent by design: the FIRST caller wins and every later call is
    // ignored. That is what guarantees no contributor can ever snapshot a base
    // that already contains another contributor's work. Returns true if this
    // call is the one that captured.
    bool CaptureBases(const float (&Values)[AttributeCount]);
    bool HasCapturedBases() const { return bBasesCaptured; }

    void SetBase(EBreakerAggregatedAttribute Attribute, float Value);
    float GetBase(EBreakerAggregatedAttribute Attribute) const;

    // Replaces this contributor's offer wholesale. Removal is just an identity
    // contribution, so an apply/remove cycle returns to exactly the base.
    void SetContribution(EBreakerAttributeContributor Contributor, const FBreakerAttributeContribution& Contribution);
    void ClearContribution(EBreakerAttributeContributor Contributor);
    const FBreakerAttributeContribution& GetContribution(EBreakerAttributeContributor Contributor) const;

    // The locked fold. Deterministic in the contributor enum's order.
    float Compose(EBreakerAggregatedAttribute Attribute) const;

private:
    float Bases[AttributeCount] = {};
    bool bBasesCaptured = false;
    FBreakerAttributeContribution Contributions[ContributorCount];
};
