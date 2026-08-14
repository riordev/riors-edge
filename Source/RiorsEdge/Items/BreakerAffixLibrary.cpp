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
        float RollWeight = 100.0f,
        EBreakerBuildCondition Condition = EBreakerBuildCondition::Always)
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
        Affix.Condition = Condition;
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

        // --- Critical, both directions ------------------------------------
        // Crit is a genuine third axis in the variance band (Power-Curve §4),
        // which it can only be if BOTH halves have range and enough slots to
        // reach it. Chance is the scarcer half deliberately: it is what turns
        // Critical Damage on, so a build that wants the crit layer has to spend
        // on both rather than stacking whichever line it happens to find.
        const std::initializer_list<EBreakerEquipSlot> CritChanceSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };
        const std::initializer_list<EBreakerEquipSlot> CritDamageSlots =
        {
            EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Necklace,
            EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary
        };
        Pool.Add(MakeAffix(TEXT("Crit.Chance"), TEXT("Critical Chance"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::CriticalChance, EBreakerStatBucket::Flat, CritChanceSlots, 1.0f, 4.0f, 60.0f));   // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Crit.Damage"), TEXT("Critical Damage"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::CriticalDamage, EBreakerStatBucket::Flat, CritDamageSlots, 5.0f, 18.0f, 60.0f));    // O2 PLACEHOLDER

        // --- Unconditional damage, on every slot ---------------------------
        // Was gloves/neck/weapons only, which made helmet, body, boots and
        // waist STRUCTURALLY incapable of raising damage (Power-Curve §"More
        // options in every avenue"). It rolls everywhere now, and its per-line
        // value came down as the pool widened: eight small lines that add up,
        // not four large ones you either find or do not.
        // O2 PLACEHOLDER: 3% (T8) -> 16% (T1); T-1 spikes to 28.8%.
        Pool.Add(MakeAffix(TEXT("Offense.WeaponDamage"), TEXT("Weapon Damage"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WeaponDamage, EBreakerStatBucket::IncreasedPercent, AllSlots, 3.0f, 16.0f, 80.0f));
        // The flat half. Lands before the Increased bucket, so it is worth most
        // to a build that already has a large bucket to multiply it by — the
        // opposite scaling shape to the line above, which is the point.
        // O2 PLACEHOLDER: 1 -> 5 percentage points of base weapon damage.
        Pool.Add(MakeAffix(TEXT("Offense.AddedDamage"), TEXT("Added Damage"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::AddedDamage, EBreakerStatBucket::Flat,
            {EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 1.0f, 5.0f, 70.0f));

        // --- Conditional damage: the movement pillar as a build axis --------
        // Each rolls roughly twice the unconditional line because it is off
        // whenever you are standing still. Slot allocation is per-slot IDENTITY,
        // not a uniform spread: boots are where airborne and slide power lives,
        // the waist is slide/dash/wall, the necklace is Redline and dash, body
        // armour is the grounded-traversal piece. Two players hunting damage on
        // boots and on a necklace are hunting different lines.
        Pool.Add(MakeAffix(TEXT("Offense.AirborneDamage"), TEXT("Damage while Airborne"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirborneDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Primary}, 5.0f, 22.0f, 45.0f, EBreakerBuildCondition::Airborne));   // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.SlidingDamage"), TEXT("Damage while Sliding"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::SlidingDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Secondary}, 5.0f, 22.0f, 45.0f, EBreakerBuildCondition::Sliding));  // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.WallRideDamage"), TEXT("Damage while Wall Riding"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::WallRideDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::BodyArmour}, 6.0f, 26.0f, 40.0f, EBreakerBuildCondition::WallRiding));  // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.RedlineDamage"), TEXT("Damage at Redline"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::RedlineDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Necklace, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::Primary}, 4.5f, 20.0f, 45.0f, EBreakerBuildCondition::Redline));  // O2 PLACEHOLDER
        Pool.Add(MakeAffix(TEXT("Offense.DashDamage"), TEXT("Damage after Dashing"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::RecentlyDashedDamage, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Secondary}, 5.0f, 22.0f, 45.0f, EBreakerBuildCondition::RecentlyDashed)); // O2 PLACEHOLDER
        return Pool;
    }
}

const TArray<FBreakerAffixDefinition>& UBreakerAffixLibrary::GetSliceAffixPool()
{
    static const TArray<FBreakerAffixDefinition> Pool = BuildSliceAffixPool();
    return Pool;
}

bool UBreakerAffixLibrary::IsOffensiveTarget(EBreakerStatTarget Target)
{
    switch (Target)
    {
    case EBreakerStatTarget::WeaponDamage:
    case EBreakerStatTarget::AddedDamage:
    case EBreakerStatTarget::CriticalChance:
    case EBreakerStatTarget::CriticalDamage:
    case EBreakerStatTarget::AirborneDamage:
    case EBreakerStatTarget::SlidingDamage:
    case EBreakerStatTarget::WallRideDamage:
    case EBreakerStatTarget::RedlineDamage:
    case EBreakerStatTarget::RecentlyDashedDamage:
        return true;
    default:
        return false;
    }
}

const FBreakerAffixDefinition* UBreakerAffixLibrary::FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId)
{
    return Pool.FindByPredicate([AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; });
}
