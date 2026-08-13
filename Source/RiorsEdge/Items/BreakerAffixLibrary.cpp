#include "Items/BreakerAffixLibrary.h"

float UBreakerAffixLibrary::ValueForTier(const FBreakerAffixDefinition& Affix, int32 Tier)
{
    const int32 ClampedTier = FMath::Clamp(Tier, -1, 8);
    if (ClampedTier == 0) return Affix.ValueAtT1 * 1.4f;
    if (ClampedTier == -1) return Affix.ValueAtT1 * 1.8f;
    // T8..T1 interpolates linearly; Alpha 0 at T8, 1 at T1.
    const float Alpha = (8.0f - static_cast<float>(ClampedTier)) / 7.0f;
    return FMath::Lerp(Affix.ValueAtT8, Affix.ValueAtT1, Alpha);
}

int32 UBreakerAffixLibrary::BestTierForItemLevel(int32 ItemLevel)
{
    // Levels 1-50 map onto T8..T1: one tier unlocked roughly every 7 levels.
    const int32 Clamped = FMath::Clamp(ItemLevel, 1, 50);
    return FMath::Clamp(8 - (Clamped - 1) / 7, 1, 8);
}

int32 UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity Rarity)
{
    switch (Rarity)
    {
    case EBreakerItemRarity::Standard: return 3;
    case EBreakerItemRarity::Uncommon: return 1;
    default: return -1;
    }
}

void UBreakerAffixLibrary::AffixCountRangeForRarity(EBreakerItemRarity Rarity, int32& OutMinimum, int32& OutMaximum)
{
    switch (Rarity)
    {
    case EBreakerItemRarity::Standard:    OutMinimum = 1; OutMaximum = 2; break;
    case EBreakerItemRarity::Uncommon:    OutMinimum = 2; OutMaximum = 3; break;
    case EBreakerItemRarity::Exceptional: OutMinimum = 3; OutMaximum = 5; break;
    case EBreakerItemRarity::Aberrant:    OutMinimum = 4; OutMaximum = 6; break;
    case EBreakerItemRarity::Anomalous:   OutMinimum = 5; OutMaximum = 6; break;
    default:                              OutMinimum = 1; OutMaximum = 1; break;
    }
}

namespace
{
    FBreakerAffixDefinition MakeAffix(
        FName AffixId,
        const TCHAR* DisplayName,
        EBreakerAffixCategory Category,
        EBreakerStatTarget StatTarget,
        EBreakerStatBucket StatBucket,
        std::initializer_list<EBreakerEquipSlot> Slots,
        float ValueAtT8,
        float ValueAtT1,
        float RollWeight = 100.0f)
    {
        FBreakerAffixDefinition Affix;
        Affix.AffixId = AffixId;
        Affix.DisplayName = FText::FromString(DisplayName);
        Affix.Category = Category;
        Affix.StatTarget = StatTarget;
        Affix.StatBucket = StatBucket;
        Affix.AllowedSlots = Slots;
        Affix.ValueAtT8 = ValueAtT8;
        Affix.ValueAtT1 = ValueAtT1;
        Affix.RollWeight = RollWeight;
        return Affix;
    }

    TArray<FBreakerAffixDefinition> BuildSliceAffixPool()
    {
        const std::initializer_list<EBreakerEquipSlot> AllSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Gloves,
            EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };

        TArray<FBreakerAffixDefinition> Pool;
        Pool.Add(MakeAffix(TEXT("Core.Health"), TEXT("Health"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::Health, EBreakerStatBucket::Flat, AllSlots, 25.0f, 180.0f));
        Pool.Add(MakeAffix(TEXT("Core.ResourceRegen"), TEXT("Resource Regeneration"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::ResourceRegen, EBreakerStatBucket::Flat, AllSlots, 0.5f, 3.0f));
        Pool.Add(MakeAffix(TEXT("Core.MaxResource"), TEXT("Maximum Resource"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::MaxResource, EBreakerStatBucket::Flat, AllSlots, 8.0f, 45.0f));
        Pool.Add(MakeAffix(TEXT("Core.MoveSpeed"), TEXT("Movement Speed"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::MoveSpeed, EBreakerStatBucket::IncreasedPercent, AllSlots, 2.0f, 8.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Core.DropChance"), TEXT("Drop Chance"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::DropChance, EBreakerStatBucket::IncreasedPercent, AllSlots, 3.0f, 14.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Core.PhysicalDR"), TEXT("Physical Damage Reduction"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::PhysicalDamageReduction, EBreakerStatBucket::IncreasedPercent, AllSlots, 2.0f, 8.0f));
        Pool.Add(MakeAffix(TEXT("Move.SlideSpeed"), TEXT("Slide Speed"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::SlideSpeed, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist}, 5.0f, 20.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Move.AirControl"), TEXT("Air Control"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirControl, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace}, 5.0f, 22.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Move.DashCooldown"), TEXT("Dash Cooldown Reduction"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::DashCooldownReduction, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Gloves}, 4.0f, 18.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Crit.Chance"), TEXT("Critical Chance"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::CriticalChance, EBreakerStatBucket::Flat, {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 2.0f, 10.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Crit.Damage"), TEXT("Critical Damage"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::CriticalDamage, EBreakerStatBucket::Flat, {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 8.0f, 35.0f, 60.0f));
        // O2 PLACEHOLDER: master-sheet Weapon Damage % line, 4% (T8) -> 22% (T1).
        Pool.Add(MakeAffix(TEXT("Offense.WeaponDamage"), TEXT("Weapon Damage"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 4.0f, 22.0f, 80.0f));
        return Pool;
    }
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSliceAffixPool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BuildSliceAffixPool();
    return Pool;
}

const FBreakerAffixDefinition* UBreakerAffixLibrary::FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId)
{
    return Pool.FindByPredicate([AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; });
}
