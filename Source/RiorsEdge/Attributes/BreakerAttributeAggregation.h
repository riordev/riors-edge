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
    // Ability cost SCALE. Base 1.0; a Resource Efficiency roll drives it DOWN,
    // so 0.85 is a 15% cheaper cast.
    //
    // Stored as the scale rather than as "percent reduction" for the same
    // reason DashCooldownReduction is stored as its divisor: only one of the
    // two shapes can share a single additive bucket across gear and trees,
    // because two layers each folding a percentage into a cost do not add. The
    // affix authors an Increased percentage OF THE REDUCTION; this is what the
    // ability cost path multiplies by.
    ResourceCostMultiplier,
    // --- O54: the second damage pool ---------------------------------------
    // Increased Ability Damage. DamageMultiplier above is the WEAPON-delivered
    // lane; this is the ability-delivered one. Base 1.0, same shape, and the
    // two are read by different hits — never both by one.
    //
    // THERE IS NO THIRD ATTRIBUTE FOR THE SHARED POOL, and that is deliberate.
    // O54's Increased Damage feeds both lanes, so it is a bid DUPLICATED into
    // both at contribution-build time (FBreakerAttributeContribution
    // ::AddSharedIncreasedDamage) rather than an attribute of its own. A
    // composed "shared multiplier" would be a number no hit ever reads and
    // nothing could sensibly do with — the two lanes are what get composed,
    // and each already contains its share. One authored line, two bids, still
    // ONE additive bucket per lane.
    //
    // O55 decides which lane a hit draws by what DELIVERS the damage, not by
    // what triggers it: a melee ability that swings the equipped weapon is
    // weapon-delivered and reads DamageMultiplier. The discriminator is
    // EBreakerDamageDelivery on the request; see BreakerDamageLibrary.
    //
    // O74: the More ceiling spans BOTH pools. It is one budget selected once
    // in UBreakerProgressionComponent::AggregateStats (strongest three across
    // every lane) and clamped again here per lane, so neither a lane alone nor
    // the two together can pass 1.30^3. A per-lane ceiling would be the second
    // budget the single ceiling exists to delete.
    AbilityDamageMultiplier,
    // Class resource regenerated per second. Base 0; gear's Resource
    // Regeneration affix bids Flat.
    //
    // It is on this path rather than being added by whoever happens to tick,
    // and that is a correction rather than a nicety. Gear regeneration was
    // ticked by the equipment component while the class loop ticked its own
    // PassiveRegenPerSecond, so the two composed by simple addition with no
    // shared bucket -- which was tolerable while regeneration was a trickle
    // beneath an accumulating bank, and stopped being tolerable the moment the
    // owner ruled that Mana starts FULL and DRAINS, making regeneration the
    // Caster's primary recovery path.
    //
    // Two separate additions cannot express an Increased percentage: a future
    // "Increased Resource Regeneration" line, or a Bulwark-style node, would
    // have to pick ONE of the two sources to multiply. One bucket is the same
    // rule damage and movement already follow, and the reason it is the rule
    // is that gear-times-tree composition has now been found and fixed three
    // times in this project.
    //
    // The class loops (Classes/) do not bid yet -- that file belongs to
    // another lane -- so today the composed value equals gear's flat alone and
    // live behaviour is bit-identical. The bucket exists so that when
    // UBreakerManaComponent moves its 6/s in and deletes its own tick, the two
    // are additive from day one instead of after the fourth instance of the
    // bug.
    ClassResourceRegen,
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

enum class EBreakerDamageMoreLane : uint8 { Weapon, Ability, Shared, Dot, Elemental, Void, Reaction, EffectiveHealth };
struct FBreakerDamageMoreSource
{
    FName Key;
    float Multiplier = 1.0f;
    EBreakerDamageMoreLane Lane = EBreakerDamageMoreLane::Weapon;
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
    // O54's shared pool, and the ONLY implementation of it. Increased Damage
    // feeds both delivery lanes, so it bids into both — one authored line, two
    // bids, each landing in its lane's single additive bucket. A hit reads one
    // lane, so nothing is double-counted; what would be double-counted is a
    // source authoring a specific pool AND the shared one for the same
    // percentage, which is the rule the tests hold.
    //
    // Kept as one function rather than two call-site AddIncreasedPercent lines
    // because the equipment and progression contributors both need it and the
    // moment they each write their own pair, one of them gains a lane the
    // other has not.
    void AddSharedIncreasedDamage(float Percent);
    // Each damage call contributes one source; Shared spends one slot for both lanes.
    void ComposeSharedMoreDamage(float Multiplier);
    // Composes into this contributor's More product. Reserved for tree
    // keystones and Anomalous rule rewrites (O3 caps the composed budget).
    void ComposeMore(EBreakerAggregatedAttribute Attribute, float Multiplier);
    void AddDamageMoreSource(FName Key, EBreakerDamageMoreLane Lane, float Multiplier);
    void SetDeadeye(bool bEnabled) { bDeadeye = bEnabled; }
    bool HasDeadeye() const { return bDeadeye; }
    void SetVelocityRules(bool NoGround, bool WeaponConversion, bool AbilityConversion)
    { bNoGround = NoGround; bMoveToWeapon = WeaponConversion; bMoveToAbility = AbilityConversion; }
    bool HasNoGround() const { return bNoGround; }
    bool ConvertsMovementToWeapon() const { return bMoveToWeapon; }
    bool ConvertsMovementToAbility() const { return bMoveToAbility; }
    float GetPositiveMovementIncreased() const { return PositiveMovementIncreased; }
    void SetPositiveMovementIncreased(float Value) { PositiveMovementIncreased = FMath::Max(0.0f, Value); }
    float GetPositiveCriticalFlat() const { return PositiveCriticalFlat; }
    float GetPositiveCriticalIncreased() const { return PositiveCriticalIncreased; }
    float GetPositiveCriticalMore() const { return PositiveCriticalMore; }
    const TArray<FBreakerDamageMoreSource>& GetDamageMoreSources() const { return DamageMoreSources; }
    static TArray<FBreakerDamageMoreSource> SelectDamageMoreSources(const TArray<FBreakerDamageMoreSource>& Sources);
    static float DamageMoreProduct(const TArray<FBreakerDamageMoreSource>& Selected, EBreakerAggregatedAttribute Attribute);

    float GetFlat(EBreakerAggregatedAttribute Attribute) const;
    float GetIncreasedPercent(EBreakerAggregatedAttribute Attribute) const;
    float GetMore(EBreakerAggregatedAttribute Attribute) const;

    // True when this contributor would move nothing at all.
    bool IsIdentity() const;

private:
    bool bDeadeye = false;
    bool bNoGround = false;
    bool bMoveToWeapon = false;
    bool bMoveToAbility = false;
    float PositiveMovementIncreased = 0;
    float PositiveCriticalFlat = 0.0f;
    float PositiveCriticalIncreased = 0.0f;
    float PositiveCriticalMore = 1.0f;
    float Flat[AttributeCount];
    float IncreasedPercent[AttributeCount];
    float MoreMultiplier[AttributeCount];
    TArray<FBreakerDamageMoreSource> DamageMoreSources;
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
    //   Compose == ComposedFlatFactor x (1 + ComposedIncreasedPercent/100) x ComposedMoreProduct
    // Compose is WRITTEN as that product of the three getters, so the four
    // cannot drift: a caller that needs one layer on its own reads the getter
    // and is guaranteed to hold the same number Compose folded.
    float Compose(EBreakerAggregatedAttribute Attribute) const;
    // Base + sum(Flat) across every contributor, the first factor of the fold.
    // A damage request carries this beside the Increased sum so a target-side
    // Increased rider joins the additive bucket UNDER the flat layer — dividing
    // the composed value by the More product alone recovers (1+f)(1+i/100),
    // which is only the Increased factor when nothing bid Flat.
    float ComposedFlatFactor(EBreakerAggregatedAttribute Attribute) const;
    // sum(IncreasedPercent) across every contributor, in whole percent; the
    // ONE additive bucket the rider pass adds to.
    float ComposedIncreasedPercent(EBreakerAggregatedAttribute Attribute) const;

    // ---- The O3 More ceiling, enforced GLOBALLY --------------------------
    // All live gear/tree damage effects enter one strongest-three selection.
    // Source records retain delivery identity until that joint selection; a
    // shared effect consumes one slot and contributes to both delivery lanes.
    // Equal magnitudes preserve tree order, then canonical gear slot/affix order.
    // Temporary windows retain their existing remaining-product-headroom rule.
    //
    // O34: THIS IS THE ONE MORE CEILING. 1.30^3 == 2.197, reached from the two
    // numbers that define it rather than restated as a constant that can drift.
    // The combat component's outgoing-modifier chain (temporary ability windows
    // — Overdrive is the first) used to hold its own hardcoded 2.20 ceiling on
    // top of this one, so the two budgets MULTIPLIED; O34 deleted that constant
    // and the chain now queries ComposedMoreProduct() below and spends whatever
    // headroom the attribute side left. Temporary windows ARE Mores and count
    // within this budget.
    // Delivery, DoT and explicit elemental/reaction/durability scopes share this budget;
    // silently clamping another attribute (say move speed) would be a decision hiding
    // in a safety net.
    static constexpr int32 MaxComposedMoreSources = 3;
    static constexpr float SingleMoreCeiling = 1.30f;
    static float ComposedMoreCeiling();
    // The post-clamp More product across every contributor, WITHOUT the base,
    // flat and Increased terms. O34 needs it queryable at the combat site: the
    // outgoing chain's windows count against the SAME budget, so the chain must
    // know how much of it the attribute side already holds. Recomputed from the
    // live contributions on every call, so it is correct for equipment and
    // progression submissions alike with nothing to cache or invalidate.
    float ComposedMoreProduct(EBreakerAggregatedAttribute Attribute) const;
    // Additional scopes only: delivery Mores are already baked by FillSourcePools.
    float GetScopedMoreProduct(bool bElemental, bool bVoid, bool bReaction, bool bEffectiveHealth) const;
    bool HasDeadeye() const;
    int32 GetDamageMoreSourceCount() const;
    int32 GetSelectedDamageMoreSourceCount() const;
    TArray<FBreakerDamageMoreSource> GetSelectedDamageMoreSources() const;
    // True when Attribute is under the O3 budget. Named so the rule is
    // greppable rather than living inside an `if` in Compose.
    static bool IsMoreCappedAttribute(EBreakerAggregatedAttribute Attribute)
    {
        // O74: ONE ceiling across every damage pool. Both delivery lanes are
        // capped, and they are capped by the SAME number rather than each
        // getting one of its own — the budget is spent once, upstream, where
        // the strongest three sources are selected across all lanes, and this
        // is the belt-and-braces clamp behind that selection. Adding a lane
        // here is not adding headroom; a build holding three Mores has three
        // however they are distributed.
        return Attribute == EBreakerAggregatedAttribute::DamageMultiplier
            || Attribute == EBreakerAggregatedAttribute::AbilityDamageMultiplier
            || Attribute == EBreakerAggregatedAttribute::DamageOverTimeMultiplier;
    }

private:
    float Bases[AttributeCount] = {};
    bool bBasesCaptured = false;
    FBreakerAttributeContribution Contributions[ContributorCount];
};
