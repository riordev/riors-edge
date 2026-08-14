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
    Count UMETA(Hidden)
};

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
    // A conditional effect pays out only while its movement state holds, and
    // pays nothing otherwise. This is where a node stops being "+3% to
    // everything" and starts being a build decision (O27, Power-Curve
    // §"Choices over accumulation"). Default Always, so authored rows that
    // predate the field are unchanged.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBreakerBuildCondition Condition = EBreakerBuildCondition::Always;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="50")) int32 CharacterLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentClassPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 UnspentCorePoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> ClassNodeRanks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerNodeRank> CoreNodeRanks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBreakerAbilityLoadout AbilityLoadout;
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
