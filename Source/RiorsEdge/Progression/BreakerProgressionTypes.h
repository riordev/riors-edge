#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Progression/BreakerBuildConditions.h"
#include "BreakerProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EBreakerClassId : uint8
{
    None,
    Caster,
    Swift,
    Gunsmith,
    Tank,
    Support
};

UENUM(BlueprintType)
enum class EBreakerPointCurrency : uint8
{
    ClassPoints,
    CorePoints
};

UENUM(BlueprintType)
enum class EBreakerAbilitySlot : uint8
{
    ClassAbilityOne,
    ClassAbilityTwo,
    Ultimate
};

// Stats a skill node can move. Mirrors the equipment layer's stat-target
// idea, but stays a separate enum on purpose: nodes and affixes are
// different layers and must not share a value table (CONTEXT.md).
UENUM(BlueprintType)
enum class EBreakerNodeStatTarget : uint8
{
    CriticalChance,
    CriticalDamage,
    MoveSpeed,
    SlideSpeed,
    AirControl,
    DodgeChance,
    BlockChance,
    Health,
    DamageOverTime,
    // Increased damage dealt. Appended rather than inserted: node effects are
    // authored content and saves store node ids, but Data Assets serialize this
    // enum by value, so existing rows must keep their numbers.
    //
    // Until this entry existed a skill node was STRUCTURALLY incapable of
    // raising weapon damage — the reason spending points never felt like
    // anything. It shares the DamageMultiplier attribute's additive Increased
    // bucket with gear's Weapon Damage affix.
    Damage,

    // -----------------------------------------------------------------------
    // O30 widening pass. Everything below here was added at once, and the
    // reason is measurable: 53 of the 97 authored tree nodes shipped with no
    // stat effect, and this enum is named as the blocker in more of their
    // WAITING ON comments than any other cause. Each entry below is traceable
    // to at least one node that could not be authored without it; the mapping
    // is in Docs/Design/Hook-And-Condition-Vocabulary.md §4.
    //
    // APPEND ONLY, and this is the rule the original comment above already
    // states: these are serialized by value into Data Assets. Insert nothing.
    //
    // ADDING AN ENTRY HERE DOES NOT MAKE IT PAY. A stat target needs an
    // aggregation lane in UBreakerProgressionComponent::AggregateStats before a
    // node authored against it does anything at all. HasAggregationLane below
    // is the honest register of which ones have one, and it is what makes an
    // unconsumed target loud instead of silent.
    // -----------------------------------------------------------------------

    // ---- Ability scaling (O30's ABILITIES axis; owner ruling 3) -----------
    // "trees modify abilities i want that to be an avenue players can scale not
    // just weapons or character (think caster for instance should be heavily
    // spell oriented over weapons)". These five are that ruling.
    //
    // AbilityDamage and WeaponDamage PARTITION Damage rather than replacing it:
    // Damage stays the everything line, and these two are the narrower, more
    // characterful lines a Caster or a Gunsmith tree wants. A node authors one
    // of the three, never Damage plus a partition — that would double-dip the
    // same additive bucket from one node.
    AbilityDamage,
    // Resource cost of abilities. The aggregated attribute ALREADY EXISTS
    // (EBreakerAggregatedAttribute::ResourceCostMultiplier) and gear already
    // bids into it; only the tree-side enum entry was missing. Authored as an
    // Increased percentage OF THE REDUCTION, matching the affix, because that
    // is the only shape two layers can share one additive bucket in.
    // Named by Swift.Frenzy.NoSafety, Swift.Kinetic.SpendToLive,
    // Swift.Marksman.Ledger and the whole Caster cost side.
    AbilityCost,
    // Cooldown reduction on abilities, stored as the divisor exactly as
    // DashCooldownReduction is. NO attribute and NO hook exists:
    // UBreakerGameplayAbility::ApplyCooldown reads Definition->CooldownSeconds
    // raw. Named by Swift.Kinetic.Redirect.
    AbilityCooldown,
    // Radius / arc / range of abilities. Named by Caster.Spellblade.Edge
    // ("Cleave's arc widens"), Swift.Marksman.CalledShot (Lead's range gate)
    // and Rot's zone. The expensive one: radius has no shared representation
    // at all — it is a differently-named UPROPERTY on each ability subclass
    // (Cleave::RangeCm, Cleave::ArcDegrees, Rot::RadiusCm), so this entry needs
    // a base-class accessor before it can land anywhere.
    AbilityArea,
    // Duration of ability windows and zones. Named by
    // Caster.VoidWhisperer.LongDark and Caster.VoidWhisperer.Lingering. Same
    // shape problem as AbilityArea: WindowDuration is on the definition, zone
    // duration is on the subclass.
    AbilityDuration,
    // The delivery-method partition's other half. WeaponDamage is O30's GUNS
    // axis; MeleeDamage is what Caster.Spellblade.Edgework's reserved More is
    // priced against ("melee-only is the tax"). Discriminated by the SourceTags
    // already on every damage request — Damage_Melee exists today in
    // Abilities/BreakerAbilityTags.h — which is why these are stat targets and
    // not conditions: the discriminator is a property of the hit, not of live
    // state. See the vocabulary document §2.1.
    WeaponDamage,
    MeleeDamage,

    // ---- Defence ---------------------------------------------------------
    // Incoming damage reduction. The node layer has never been able to author
    // defence beyond flat Health, which is why Swift.Kinetic.MomentumShield
    // could not be written. Stored as a REDUCTION percentage joining one
    // additive bucket, never as a multiplier per source — stacking multiplier
    // sources is how a build reaches immunity by accident.
    IncomingDamageReduction,
    // Flat mitigation. The aggregated attribute EXISTS and its comment invites
    // this entry by name: "the moment a second layer does (a Bulwark node) the
    // two are additive from day one instead of repeating the gear-x-tree
    // multiplication bug."
    Armor,
    // Health leech. Named by Caster.Spellblade.Bloodprice.
    Lifesteal,

    // ---- Class resource --------------------------------------------------
    // Maximum resource. Class-Kits flags Caster.Multispell.Reservoir as "the
    // one intentional stat node in the Caster kit, and the enum has no Maximum
    // Resource target to carry it" — the single clearest case in the content of
    // a design that wanted a number and could not write it. Attribute exists
    // (MaxClassResource).
    MaxClassResource,
    // Resource generated per second. Attribute exists (ClassResourceRegen);
    // gear bids, the class loops do not yet. Named by
    // Caster.VoidWhisperer.Patience, .Seep and .StandingWater.
    ClassResourceRegen,
    // Rate at which the resource DECAYS. No attribute, and it cannot simply get
    // one: decay is a per-loop rule computed inside each class component
    // (UBreakerMomentumComponent::DecayRateForSpeed and its four siblings), not
    // a shared stat. The five components already have the seam that consumes it
    // — PushLoopOverride — so this entry is plumbing to an existing valve, not
    // a new system. Named by Swift.Kinetic.NoGround, Swift.Marksman.Reserve and
    // Swift.Frenzy.NoSafety, the three tier-4 nodes with real downsides.
    ClassResourceDecay,

    // ---- Weapon handling (O30's GUNS axis) -------------------------------
    // Attribute exists (FireRateMultiplier); O34 names fire rate a "named,
    // watched, currently-uncapped lane", so a tree line here is watched too.
    FireRate,
    // Attribute exists (DashCooldownReduction). AggregateStats already carries
    // a comment asking for exactly this: "There is deliberately NO
    // dash-cooldown line here: EBreakerNodeStatTarget has no dash entry, so no
    // node can author one ... a tree dash node only needs the enum entry and
    // one line here, and it will be additive from the first day it exists."
    DashCooldown,
    // Recoil recovery rate. Core.Volley.TriggerDiscipline is a tier-1, cost-1
    // GATEWAY node — the cheapest and most-purchased node of its constellation
    // — and it does literally nothing today for want of this entry.
    RecoilRecovery,
    // Spread / accuracy. Named by Swift.Marksman.Steady.
    WeaponSpread,
    // Projectile count. Named by Core.Volley.LastRound and the Salvo/Barrage
    // pair. Whole projectiles, so it is a Flat-bucket stat and rounds down; a
    // "+50% projectiles" line is meaningless on a single-shot weapon and the
    // aggregation lane must say so rather than silently firing 1.5 bullets.
    ProjectileCount,
    // Pierce count / falloff. Named by Swift.Marksman.PierceDiscipline,
    // .Overpenetration and .Sightline.
    Pierce,

    // ---- Status ----------------------------------------------------------
    // Duration of statuses this character applies. Named by
    // Caster.Multispell.Resonance ("halves their remaining duration instead").
    StatusDuration,
    // Chance to apply a status on hit. The magnitude twin of
    // Core.Affliction.OpenWound, whose own rewrite (guaranteed application on
    // weak point) stays a tag because bypassing a roll is not a percentage.
    StatusChance,

    Count UMETA(Hidden)
};

// Which stat targets AggregateStats actually composes today.
//
// This exists because the widening pass above added twenty-one entries and the
// aggregator consumes ten of them. Without this register, authoring a node
// against AbilityCooldown would compile, purchase, display on the board and pay
// nothing — the precise failure the project has now shipped four times (the
// third jump, the phantom ability grants, the phantom keystones, the MorePercent
// silently dropped on every stat target but Damage). O40(c) makes reachability
// part of definition-of-done; for an enum entry, "reachable" means "has an
// aggregation lane", and this is where that is stated rather than assumed.
//
// The register is maintained BY HAND on purpose. It cannot be derived — the
// aggregator's lanes are individually written lines — and a hand-maintained list
// that a test pins is honest, whereas a derived one would agree with the code by
// construction and prove nothing. When the coordinator adds a lane, this returns
// true for that entry in the same commit, and the audit test that reports the
// count is what makes the progress visible.
inline bool BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget Target)
{
    switch (Target)
    {
    case EBreakerNodeStatTarget::CriticalChance:
    case EBreakerNodeStatTarget::CriticalDamage:
    case EBreakerNodeStatTarget::MoveSpeed:
    case EBreakerNodeStatTarget::SlideSpeed:
    case EBreakerNodeStatTarget::AirControl:
    case EBreakerNodeStatTarget::DodgeChance:
    case EBreakerNodeStatTarget::BlockChance:
    case EBreakerNodeStatTarget::Health:
    case EBreakerNodeStatTarget::DamageOverTime:
    case EBreakerNodeStatTarget::Damage:
    // ---- Wired 2026-08-15, the first five of the O30 widening ------------
    // Each of these had an aggregated attribute the whole time and no way for
    // a NODE to reach it. One line each in AggregateStats, all joining the
    // same additive Increased bucket gear already bids into.
    //
    // DashCooldown is deliberately NOT here despite the roadmap listing it:
    // EBreakerAggregatedAttribute has no dash entry at all — dash cooldown
    // lives on FBreakerEquipmentStats and the movement component reads it
    // directly, so there is nowhere for a node to bid. See the comment at the
    // call site.
    case EBreakerNodeStatTarget::AbilityCost:
    case EBreakerNodeStatTarget::MaxClassResource:
    case EBreakerNodeStatTarget::ClassResourceRegen:
    case EBreakerNodeStatTarget::FireRate:
    case EBreakerNodeStatTarget::Armor:
        return true;
    default:
        // Every O30 widening entry. They become true one at a time as the
        // coordinator wires each lane; see the vocabulary document §6.
        return false;
    }
}

// Same aggregation law as equipment: flat values sum, then one additive
// Increased bucket per stat, then More multipliers compose as a product.
UENUM(BlueprintType)
enum class EBreakerNodeStatBucket : uint8
{
    Flat,
    IncreasedPercent,
    // O3: "More multipliers multiply as an unordered product; a build may hold
    // 2-3 Mores total (hard cap 3). Trees may author them only on branch
    // keystones and constellation Convergence/Keystone nodes." Appended, so
    // authored rows keep their serialized numbers.
    //
    // Authored in whole percent ABOVE 1.0 — 25.0 means x1.25 — to match how
    // every other percentage in the node tables is written. The aggregator
    // enforces the cap; see UBreakerProgressionComponent::AggregateStats.
    MorePercent
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodeEffect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerNodeStatTarget StatTarget = EBreakerNodeStatTarget::CriticalChance;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerNodeStatBucket StatBucket = EBreakerNodeStatBucket::Flat;
    // Multiplied by the owned rank. Percent stats are authored in whole
    // percent (5.0 == 5%), matching the affix tables.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float ValuePerRank = 0.0f;
    // A conditional effect pays out only while its condition holds, and pays
    // nothing otherwise. This is where a node stops being "+3% to everything"
    // and starts being a build decision (O27, Power-Curve §"Choices over
    // accumulation"). Default Always, so authored rows that predate the field
    // are unchanged.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerBuildCondition Condition = EBreakerBuildCondition::Always;
    // Owner ruling, verbatim: "conditions do compose yes". An effect may name
    // further conditions here and pays only while Condition AND every entry in
    // this array hold at once. That is what lets a node say "while airborne and
    // against a bleeding enemy" — two axes at once — which is where the texture
    // the owner asked for actually comes from.
    //
    // A separate array rather than a widened Condition field, for two reasons.
    // Serialization: Condition keeps its meaning and its value, so every
    // authored row and every affix row that predates composition is untouched
    // and the append-only rule is not strained. And readability: an empty array
    // is the overwhelmingly common case and costs nothing, while a bitmask
    // UPROPERTY would make the ordinary single-condition line harder to author
    // and to read in a diff than it is today.
    //
    // AND only — no OR, no NOT. The reasoning is on
    // FBreakerBuildConditionState::SatisfiesAll.
    //
    // A CAUTION THE CONTENT PASS MUST HEAR, and it is the owner's fourth
    // ruling: "conditionals should be flavour that add spice overall". Each
    // condition added to a line makes it pay less often, so the temptation is
    // to price a two-condition line dramatically higher. Do not. The backbone
    // is unconditional and ability scaling; conditions are texture. A build
    // whose only route to power is satisfying three predicates at once is the
    // failure mode, not the goal. See the vocabulary document §5.3.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<EBreakerBuildCondition> AlsoRequires;

    // True when this effect pays less than always. Both halves matter: a line
    // with Always plus a non-empty AlsoRequires is still conditional.
    bool IsConditional() const
    {
        return Condition != EBreakerBuildCondition::Always || AlsoRequires.Num() > 0;
    }

    // Whether satisfying this effect needs to know about the actor being hit.
    // The damage pipeline uses it to decide whether a node's contribution can
    // be composed once on the source (the cheap, existing path) or has to be
    // deferred to the target side (the expensive new one) — see the vocabulary
    // document §3.2.
    bool RequiresTargetState() const
    {
        if (FBreakerBuildConditionState::IsTargetCondition(Condition)) return true;
        for (const EBreakerBuildCondition Required : AlsoRequires)
        {
            if (FBreakerBuildConditionState::IsTargetCondition(Required)) return true;
        }
        return false;
    }
};

// Aggregated output of every owned node rank. Multipliers are 1.0-based,
// bonuses are absolute (crit/dodge/block are 0..1 fractions).
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodeStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float BonusHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalMultiplierBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float DodgeChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BlockChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MoveSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float SlideSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float AirControlMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float DamageOverTimeMultiplier = 1.0f;
    // Composed from node Damage effects PLUS the per-spent-point baseline, so
    // this is the whole tree layer's contribution to the shared additive
    // Increased bucket on the DamageMultiplier attribute.
    UPROPERTY(BlueprintReadOnly) float DamageMultiplier = 1.0f;
    // The composed More product for outgoing damage, AFTER the O3 cap has
    // been applied. 1.0 when the build holds no More node. This is a separate
    // field from DamageMultiplier on purpose: they are different buckets and
    // merging them would be the exact bug the aggregation rule exists to stop.
    UPROPERTY(BlueprintReadOnly) float DamageMoreMultiplier = 1.0f;
    // How many More sources the build actually holds, before the cap. The skill
    // screen prints "3 / 3 MORE" from this so a fourth purchase visibly does
    // nothing rather than silently doing nothing.
    UPROPERTY(BlueprintReadOnly) int32 DamageMoreSourceCount = 0;
    // Increased damage from conditional effects that are live right now, and
    // what the same effects would be worth with every condition satisfied.
    // Whole percent, display only — the live half is already in DamageMultiplier.
    UPROPERTY(BlueprintReadOnly) float ActiveConditionalDamagePercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float PotentialConditionalDamagePercent = 0.0f;
    // Rule-rewrite and verb-grant nodes cannot be expressed as stats; they
    // publish a tag here and the owning system reads it.
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer GrantedTags;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerNodeRank
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAbilityLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ClassAbilityOne = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ClassAbilityTwo = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Ultimate = NAME_None;

    bool Contains(FName AbilityId) const
    {
        return ClassAbilityOne == AbilityId || ClassAbilityTwo == AbilityId || Ultimate == AbilityId;
    }
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerProgressionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerClassId PermanentClass = EBreakerClassId::None;
    // DERIVED from TotalExperience, never authoritative. It is kept in the
    // struct because a great deal of code already reads it, but the XP total
    // is the stored truth: re-deriving means a retuned curve moves every
    // existing character's level instead of leaving saved levels the new curve
    // can no longer afford. UBreakerProgressionComponent::RefreshLevelFromXp
    // is the only thing that should write it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="50")) int32 CharacterLevel = 1;
    // The XP loop O40(b) was waiting for. Cumulative and monotonic — never
    // "XP into the current level", because that representation cannot survive
    // a curve retune without stranding characters mid-level.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 TotalExperience = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentClassPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentCorePoints = 0;
    // How many level-entitled points have already been paid into the pools
    // above (XP-And-Pacing §4: 1 Class Point per level to 30, 1 Core Point
    // per level to 50). Cumulative like TotalExperience and for the same
    // reason: the entitlement is min(Level, cap) and the component pays only
    // the positive difference, so a curve retune that moves levels can never
    // double-pay, and a retune that LOWERS a level claws nothing back. The
    // slice lump seeds these to its own values — it is an advance, not a
    // bonus (see UBreakerProgressionLibrary::SliceClassPointGrant). Defaulted
    // so an existing save loads as "nothing paid yet"; LoadProgressionState
    // repairs pre-entitlement saves that already received the lump.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 LevelClassPointsGranted = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 LevelCorePointsGranted = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> ClassNodeRanks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> CoreNodeRanks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerAbilityLoadout AbilityLoadout;
    // O37: subclass commitment. None until UBreakerProgressionComponent::
    // CommitToBranch is called; that call is one-way (refuses once this is
    // already set) and only RespecAtForge(ClassPoints, ...) clears it back to
    // None. Committing EMPOWERS rather than excludes — it unlocks the named
    // branch's keystone/cornerstone tier; ordinary nodes of every branch stay
    // freely purchasable regardless (O15 intact). A defaulted UPROPERTY, so
    // an existing save loads with no commitment and stays save-compatible.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CommittedBranch = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 SaveVersion = 1;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDamageSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SourcePower = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CriticalMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageOverTimeMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer SourceTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRolledCritical = false;
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerStatusApplicationSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag StatusTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseDamagePerTick = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TickInterval = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 InitialStacks = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProcCoefficient = 1.0f;
    // DoTs snapshot this structure at application. Later source-stat changes
    // do not rewrite an already-running effect.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerDamageSnapshot Snapshot;
};
