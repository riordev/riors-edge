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
    // Rate of fire, as a multiplier on rounds per minute. Base 1.0, same shape
    // as DamageMultiplier and for the same reason: the owner asked for an SMG
    // that leans toward fire rate, and a cadence stat that composed
    // multiplicatively across gear and trees would break the one-additive-
    // bucket rule the moment a tree node authored one.
    //
    // A MULTIPLIER on RPM rather than an interval, because an interval cannot
    // share an additive bucket -- two layers each folding a percentage into a
    // duration do not add. Consumed by UBreakerWeaponComponent's fire timing.
    FireRateMultiplier,
    // Flat mitigation. The comment above says attributes a single system owns
    // outright stay off this path, and Armor USED to be one: nothing wrote it
    // on the player at all (base 0), so the most ordinary defensive line in the
    // genre could not exist. It joins the fold because gear now bids on it, and
    // the moment a second layer does (a Bulwark node) the two are additive from
    // day one instead of repeating the gear-x-tree multiplication bug.
    //
    // Enemies and target dummies are untouched: they carry neither an equipment
    // nor a progression component, so nothing captures their bases and
    // RecomputeAggregatedAttributes never runs on them. ABreakerTargetDummy
    // keeps writing Armor directly and keeps meaning it.
    Armor,
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

    // ---- The O3 More ceiling, enforced GLOBALLY --------------------------
    // Docs/Item-Foundation.md recorded this as an open hole in so many words:
    // "The O3 More cap is enforced per LAYER, not globally. Three tree Mores is
    // the whole budget today because nothing else authors one. When Anomalous
    // items gain Mores, the clamp has to move to a single shared pass over the
    // composed contribution or a build can hold three tree Mores plus an
    // item's."
    //
    // This is that shared pass. UBreakerProgressionComponent still picks its
    // own strongest three and clamps each at 1.30x — that selection needs to
    // know about individual SOURCES and only that layer has them — but the
    // product every layer's selection composes to is clamped here, once, over
    // the whole build. A contributor cannot buy its way past O3 by arriving
    // second.
    //
    // 1.30^3 == 2.197, i.e. Damage-Pipeline §4's 2.20 composed ceiling and
    // UBreakerCombatComponent::ComposedMoreCeiling, reached from the two numbers
    // that define it rather than restated as a third constant that can drift.
    // Damage only: no other aggregated attribute has an authored More budget,
    // and silently clamping (say) move speed would be a balance decision hiding
    // in a safety net.
    static constexpr int32 MaxComposedMoreSources = 3;
    static constexpr float SingleMoreCeiling = 1.30f;
    static float ComposedMoreCeiling();
    // True when Attribute is under the O3 budget. Named so the rule is
    // greppable rather than living inside an `if` in Compose.
    static bool IsMoreCappedAttribute(EBreakerAggregatedAttribute Attribute)
    {
        return Attribute == EBreakerAggregatedAttribute::DamageMultiplier;
    }

private:
    float Bases[AttributeCount] = {};
    bool bBasesCaptured = false;
    FBreakerAttributeContribution Contributions[ContributorCount];
};
