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
        // Slide Speed and Dash Cooldown now roll on the two WEAPON slots as
        // well as their armour homes. That is not breadth for its own sake:
        // the owner's sidearm lean is "sidearm slide speed", and a lean toward
        // a line that cannot roll on the slot at all is a comment, not a
        // feature. It also gives the Secondary slot a movement identity, which
        // pairs with the sidearm's fast swap.
        Pool.Add(MakeAffix(TEXT("Move.SlideSpeed"), TEXT("Slide Speed"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::SlideSpeed, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 5.0f, 20.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Move.AirControl"), TEXT("Air Control"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::AirControl, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace}, 5.0f, 22.0f, 60.0f));
        Pool.Add(MakeAffix(TEXT("Move.DashCooldown"), TEXT("Dash Cooldown Reduction"), EBreakerAffixCategory::Prefix, EBreakerStatTarget::DashCooldownReduction, EBreakerStatBucket::IncreasedPercent, {EBreakerEquipSlot::Boots, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 4.0f, 18.0f, 60.0f));

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

        // Cadence. Rolls only on the two WEAPON slots, deliberately: fire rate
        // is a property of the gun, and putting it on boots would make the
        // per-slot identity table meaningless. This is the line the SMG leans
        // toward (owner: "smg fire rate").
        // O2 PLACEHOLDER: 2% (T8) -> 11% (T1). Smaller than Weapon Damage
        // because it multiplies sustained output the same way while ALSO
        // shortening time-to-empty, so it is not a strictly cheaper peer.
        Pool.Add(MakeAffix(TEXT("Weapon.FireRate"), TEXT("Fire Rate"), EBreakerAffixCategory::Suffix, EBreakerStatTarget::FireRate, EBreakerStatBucket::IncreasedPercent,
            {EBreakerEquipSlot::Primary, EBreakerEquipSlot::Secondary}, 2.0f, 11.0f, 60.0f));

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
    // Fire rate raises sustained damage output, so the breadth test counts it
    // as offence even though it lands on a different attribute.
    case EBreakerStatTarget::FireRate:
        return true;
    default:
        return false;
    }
}

const FBreakerAffixDefinition* UBreakerAffixLibrary::FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId)
{
    return Pool.FindByPredicate([AffixId](const FBreakerAffixDefinition& Affix) { return Affix.AffixId == AffixId; });
}

// ---------------------------------------------------------------------------
// PER-ARCHETYPE AFFIX LEANS
// ---------------------------------------------------------------------------
// Each archetype leans toward the stats that express what it already IS
// mechanically, so the lean reinforces a niche the weapon table already
// authored rather than inventing a second, contradictory identity for it.
// A machinegun is the sustained-fire gun, so it leans damage; a sidearm is the
// tempo gun with the fastest swap in the game, so it leans movement.
//
// Multipliers are deliberately modest. At 2.5x a leaned line is roughly two
// and a half times as likely as it would otherwise be, which is enough to feel
// like a pattern across a session of drops and nowhere near enough to make the
// off-lean roll rare enough to be frustrating. Anything much above 4x starts
// to read as a filter, which is exactly what the owner said this must not be.
//
// Every value here is an O2 PLACEHOLDER.
namespace BreakerArchetypeLeans
{
    struct FLean
    {
        const TCHAR* AffixId;
        float Multiplier;
    };

    // Leans are additive in intent but multiplicative in effect: one affix can
    // appear once per archetype row, so a line is never double-counted.
    static const FLean SMGLeans[] = {
        // The owner named this one directly. An SMG's identity IS its cadence.
        { TEXT("Weapon.FireRate"),          3.0f },
        { TEXT("Crit.Chance"),              1.8f },
        { TEXT("Offense.AddedDamage"),      1.5f },  // flat pays a fast gun most
    };

    static const FLean MachinegunLeans[] = {
        // Owner: "lmg damage". The sustained gun wants the damage lines.
        { TEXT("Offense.WeaponDamage"),     3.0f },
        { TEXT("Offense.AddedDamage"),      2.0f },
        { TEXT("Core.Health"),              1.6f },  // it is the planted gun
    };

    static const FLean SidearmLeans[] = {
        // Owner: "sidearm slide speed". The tempo gun leans movement, which
        // also makes it the natural Secondary for a movement build.
        { TEXT("Move.SlideSpeed"),          3.0f },
        { TEXT("Core.MoveSpeed"),           2.2f },
        { TEXT("Move.DashCooldown"),        1.8f },
        { TEXT("Offense.SlidingDamage"),    1.8f },
    };

    static const FLean SniperLeans[] = {
        { TEXT("Crit.Damage"),              3.0f },
        { TEXT("Crit.Chance"),              1.6f },
        { TEXT("Offense.WeaponDamage"),     1.5f },
    };

    static const FLean ShotgunLeans[] = {
        { TEXT("Offense.AddedDamage"),      2.6f },  // flat pays per pellet
        { TEXT("Core.Health"),              1.8f },  // it is the close-range gun
        { TEXT("Offense.SlidingDamage"),    1.8f },
    };

    static const FLean RocketLeans[] = {
        { TEXT("Offense.WeaponDamage"),     2.6f },
        { TEXT("Offense.AirborneDamage"),   2.0f },
    };

    static const FLean BurstRifleLeans[] = {
        { TEXT("Crit.Chance"),              2.2f },
        { TEXT("Crit.Damage"),              2.0f },
        { TEXT("Weapon.FireRate"),          1.6f },
    };

    static const FLean RifleLeans[] = {
        // The generalist leans generalist. Deliberately the flattest row in
        // the table: the rifle's identity is that it has no sharp edge, and a
        // strong lean here would take that away.
        { TEXT("Offense.WeaponDamage"),     1.6f },
        { TEXT("Crit.Chance"),              1.4f },
    };

    static void Rows(EBreakerWeaponArchetype Archetype, const FLean*& OutRows, int32& OutCount)
    {
        #define BREAKER_LEAN_ROW(Table) OutRows = Table; OutCount = UE_ARRAY_COUNT(Table); return;
        switch (Archetype)
        {
            case EBreakerWeaponArchetype::SMG:        BREAKER_LEAN_ROW(SMGLeans);
            case EBreakerWeaponArchetype::Machinegun: BREAKER_LEAN_ROW(MachinegunLeans);
            case EBreakerWeaponArchetype::Sidearm:    BREAKER_LEAN_ROW(SidearmLeans);
            case EBreakerWeaponArchetype::Sniper:     BREAKER_LEAN_ROW(SniperLeans);
            case EBreakerWeaponArchetype::Shotgun:    BREAKER_LEAN_ROW(ShotgunLeans);
            case EBreakerWeaponArchetype::Rocket:     BREAKER_LEAN_ROW(RocketLeans);
            case EBreakerWeaponArchetype::BurstRifle: BREAKER_LEAN_ROW(BurstRifleLeans);
            default:                                  BREAKER_LEAN_ROW(RifleLeans);
        }
        #undef BREAKER_LEAN_ROW
    }
}

float UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype Archetype, FName AffixId)
{
    const BreakerArchetypeLeans::FLean* Rows = nullptr;
    int32 Count = 0;
    BreakerArchetypeLeans::Rows(Archetype, Rows, Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (AffixId == FName(Rows[Index].AffixId))
        {
            // Clamped at 1.0 from below: a "lean" may only ever make a line
            // MORE likely. Making one less likely is a different feature with
            // a different failure mode (a stat that quietly cannot be found),
            // and nobody has asked for it.
            return FMath::Max(1.0f, Rows[Index].Multiplier);
        }
    }
    return 1.0f;
}
