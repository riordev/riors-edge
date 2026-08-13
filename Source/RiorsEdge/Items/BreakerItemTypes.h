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
    // RESERVED, NOT LIVE. There is no elemental resistance model yet (O5 rules
    // the elements Rift/Entropy/Void and puts resistances after armour, before
    // shields), so this target has no aggregated field and no consumer. It is
    // deliberately absent from the slice affix pool — an affix that rolls it
    // would be a line of text that does nothing. Wire it with the resistance
    // model, not before.
    ElementalDamageReduction,
    CriticalChance,
    CriticalDamage,
    SlideSpeed,
    AirControl,
    DashCooldownReduction,
    WeaponDamage,
    Count UMETA(Hidden)
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

// Which way one affix line moves the player relative to the piece it would
// replace. Every stat target in the slice pool is "higher is better" — Dash
// Cooldown Reduction included, because it is authored as an increase to the
// reduction — so one polarity rule covers the whole pool. A target where lower
// is better would need a polarity flag on FBreakerAffixDefinition first.
UENUM(BlueprintType)
enum class EBreakerAffixDelta : uint8
{
    Better,
    Worse,
    Parity
};

// One affix line of a candidate item, already compared against the piece it
// would replace. The UI renders the glyph; the rule lives here.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerAffixComparison
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName AffixId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 Tier = 8;
    // The candidate's rolled value.
    UPROPERTY(BlueprintReadOnly) float Value = 0.0f;
    // What the equipped piece contributes to the SAME stat target in the SAME
    // bucket, summed. Zero when the equipped piece does not touch that stat.
    UPROPERTY(BlueprintReadOnly) float ComparedValue = 0.0f;
    UPROPERTY(BlueprintReadOnly) EBreakerAffixDelta Delta = EBreakerAffixDelta::Better;

    float GetDifference() const { return Value - ComparedValue; }
};

// The complete consequence of equipping one item, answered before the click.
// Two displacements can happen at once and they are deliberately separate:
// the ORDINARY one is the piece in the same slot, which every equip swaps out;
// the LIMIT one is a second piece ejected because the rarity cap (O11 /
// master sheet 4.1: Aberrant 3, Anomalous 1) is already met. The cap is never
// a refusal — it is disclosed, and the named piece is the piece that actually
// leaves.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerEquipPreview
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bSlotOccupied = false;
    UPROPERTY(BlueprintReadOnly) FBreakerItemInstance SlotDisplaced;
    // True when equipping would put the player over the cap for the
    // candidate's rarity, counting the slot swap's own departure first.
    UPROPERTY(BlueprintReadOnly) bool bExceedsRarityLimit = false;
    UPROPERTY(BlueprintReadOnly) FBreakerItemInstance LimitDisplaced;
    // Equipped count of the candidate's rarity right now, and the cap for it.
    // A limit of INDEX_NONE means the rarity is uncapped.
    UPROPERTY(BlueprintReadOnly) int32 RarityCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 RarityLimit = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) TArray<FBreakerAffixComparison> AffixDeltas;
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
    // DISPLAY ONLY. Gear-granted increased weapon damage, on its own. Combat
    // does NOT read this: the same raw percentage is submitted to the
    // DamageMultiplier attribute's shared additive Increased bucket, where it
    // sums with skill-tree damage instead of multiplying against it. Reading
    // this at a damage site would double-count gear.
    UPROPERTY(BlueprintReadOnly) float WeaponDamageMultiplier = 1.0f;
};
