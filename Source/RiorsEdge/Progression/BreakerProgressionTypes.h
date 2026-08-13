#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
    Count UMETA(Hidden)
};

// Same aggregation law as equipment: flat values sum, then one additive
// Increased bucket per stat. More multipliers stay reserved for keystones
// and Convergence nodes (O3) and are not expressible here yet.
UENUM(BlueprintType)
enum class EBreakerNodeStatBucket : uint8
{
    Flat,
    IncreasedPercent
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
