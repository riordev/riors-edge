#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BreakerItemTypes.generated.h"

UENUM(BlueprintType)
enum class EBreakerEquipSlot : uint8
{
    Helmet,
    BodyArmour,
    Gloves,
    Boots,
    Necklace,
    Waist,
    Primary,
    Secondary,
    Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EBreakerItemRarity : uint8
{
    Standard,
    Uncommon,
    Exceptional,
    Aberrant,
    Anomalous
};

UENUM(BlueprintType)
enum class EBreakerAffixCategory : uint8
{
    Prefix,
    Suffix
};

// How a rolled value combines during stat aggregation. Flat values sum first,
// then all Increased percentages sum into a single additive bucket applied
// once. More multipliers are reserved for tree/Anomalous rule rewrites and
// multiply individually; affixes must not use them.
UENUM(BlueprintType)
enum class EBreakerStatBucket : uint8
{
    Flat,
    IncreasedPercent,
    MorePercent
};

UENUM(BlueprintType)
enum class EBreakerStatTarget : uint8
{
    Health,
    ResourceRegen,
    MaxResource,
    MoveSpeed,
    DropChance,
    PhysicalDamageReduction,
    ElementalDamageReduction,
    CriticalChance,
    CriticalDamage,
    SlideSpeed,
    AirControl,
    DashCooldownReduction
};

// Tiers run T8 (worst) to T1 linearly, then spike: T0 = 1.4x T1, T-1 = 1.8x
// T1. Stored as the printed number, so Tier ranges 8..-1.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAffixDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AffixId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerAffixCategory Category = EBreakerAffixCategory::Prefix;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerStatTarget StatTarget = EBreakerStatTarget::Health;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerStatBucket StatBucket = EBreakerStatBucket::Flat;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EBreakerEquipSlot> AllowedSlots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ValueAtT8 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ValueAtT1 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float RollWeight = 100.0f;

    bool AllowsSlot(EBreakerEquipSlot Slot) const { return AllowedSlots.Contains(Slot); }
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerRolledAffix
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AffixId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-1", ClampMax="8")) int32 Tier = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Value = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerAffixCategory Category = EBreakerAffixCategory::Prefix;
};

// A generated item. References a definition by stable id and carries rolled
// affixes separately — definitions are immutable content, instances are save
// data (same rule as progression state: ids and numbers, never pointers).
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerItemInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerEquipSlot Slot = EBreakerEquipSlot::Primary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 ItemLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FBreakerRolledAffix> Affixes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 SaveVersion = 1;

    bool IsValid() const { return ItemId.IsValid(); }
};

// Aggregated result of everything equipped. Flat and Increased buckets are
// already combined; consumers read final values.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerEquipmentStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float BonusHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ResourceRegenPerSecond = 0.0f;
    UPROPERTY(BlueprintReadOnly) float BonusMaxResource = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MoveSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float DropChancePercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float PhysicalDamageReductionPercent = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalChanceBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CriticalMultiplierBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float SlideSpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float AirControlMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float DashCooldownMultiplier = 1.0f;
};
