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
